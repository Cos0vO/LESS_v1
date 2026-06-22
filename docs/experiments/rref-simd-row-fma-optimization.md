# RREF SIMD Row FMA 优化记录

## 当前系统情况

当前 Reference Implementation 已经有两条 RREF 候选路径：

- `generator_RREF_ct()`：strict CT 候选。它固定扫描 pivot search，并且隐藏
  `pivot_column` 相关读取，因此会在读取 pivot value 和 multiplier 时做全列
  masked selection。
- `generator_RREF_ct_level_a_fast()`：Level-A fast 候选。它仍然保护
  secret-derived `pivot_row`，保留 fixed-scan pivot search、masked row
  movement、deferred failure 和 masked elimination，但允许 `pivot_column`
  直接影响矩阵读取。

上一轮优化已经证明：在 Level-A 威胁模型下恢复直接 `pivot_column` 读取，可以把
strict CT RREF 的 wall-clock 时间降低到约 35%-38%，也就是约 2.65x-2.84x
加速。

## 可优化点

现在 Level-A fast 路径中的主要重复工作是 elimination 的 row FMA：

```text
dst[col] = select(do_reduce, dst[col] - multiplier * pivot_row[col], dst[col])
```

这段代码的特点是：

- 每一轮会对 `K` 行执行；
- 每一行会扫描 `N` 个 `uint8_t` 域元素；
- 运算模式规整，适合编译器自动向量化，也适合作为后续 NEON / AVX2 intrinsic
  的落点；
- `do_reduce` 和 `multiplier` 由当前 row 决定，但内层 `col` loop 是固定长度，
  不需要 secret-dependent branch。

## 小步骤 1：抽出 Level-A Row FMA kernel

操作：

- 在 `row_ct.h` 中新增专门的 `ct_cond_fma_row_level_a()` helper。
- 在 `generator_RREF_ct_level_a_fast()` 的 elimination 内层调用该 helper。

这样做的逻辑：

- 先把热点从 RREF 主循环中分离出来，形成一个单一、稳定、容易审计的 kernel。
- helper 的语义仍然是逐列执行 `dst -= factor * src`，再用 mask 决定是否写回；
  因此它不改变 RREF 结果。
- 这个小步骤本身不声称已经完成手写 SIMD，但它给后续 SIMD 版本提供了边界：
  只要 SIMD helper 与 scalar helper 在每个元素上等价，就可以局部替换。

安全边界：

- `col` loop 仍为固定 `0..N-1`。
- `do_reduce` 只进入 select mask，不控制 branch 或 loop bound。
- 这个步骤不新增 `pivot_row` 直接索引，也不恢复 early return 或 pivot reuse。

验证：

```bash
cmake --build Reference_Implementation/build-ct-local
ctest --test-dir Reference_Implementation/build-ct-local -R '^LESS_test_' --output-on-failure
```

结果：7 个 `LESS_test_*` 全部通过。

抽样 wall-clock：

| 参数集 | strict CT | Level-A fast | 加速 |
|---|---:|---:|---:|
| 252_45 | 6.877 ms | 2.454 ms | 2.803x |
| 400_102 | 23.726 ms | 8.393 ms | 2.827x |
| 548_137 | 61.036 ms | 22.801 ms | 2.677x |

结论：抽出 helper 后结果保持一致，性能没有明显退化。这个步骤主要是建立 SIMD
落点，而不是单独追求加速。

## 小步骤 2：加入 arm64 NEON Row FMA

操作：

- 在 `ct_cond_fma_row_level_a()` 中为 `__ARM_NEON` / `__ARM_NEON__` 增加
  16-lane `uint8_t` SIMD 路径。
- 每次处理 16 个域元素；不足 16 的尾部元素仍走 scalar loop。

向量化逻辑：

1. 读取 `src[0..15]` 和 `dst[0..15]`。
2. 将 `uint8_t` 扩展为两组 `uint16_t`，因为乘法最大值为
   `126 * 126 = 15876`，8 bit 容不下。
3. 使用 `Q=127` 的折叠归约：

   ```text
   x mod 127 = (x >> 7) + (x & 127)，必要时再减 127
   ```

   对本场景的乘法结果，该范围内一次条件减法足够。
4. 计算 `dst - product`，再做一次条件减 127。
5. 用 `do_reduce` 生成全 0 或全 1 mask，选择写回 reduced 或保留原 `dst`。

安全边界：

- NEON 路径没有根据 secret 数据改变 loop bound。
- `do_reduce` 只生成 vector select mask，不控制分支。
- `factor` 和数据值只参与寄存器内算术，不作为表索引。

实现结果：

- 新增 CMake option：`LESS_RREF_ENABLE_NEON_ROW_FMA`，默认 `OFF`。
- 默认构建继续使用 scalar/autovec helper。
- NEON kernel 作为 opt-in 实验路径保留，便于后续继续打磨。

为什么默认关闭：

手写 NEON 原型功能正确，但当前版本在 Apple arm64 上变慢。主要原因很可能是：

- GF(127) 乘法需要 `uint8_t -> uint16_t` 扩展、乘法、折叠归约、条件减法、再
  pack 回 `uint8_t`；
- 每 16 个元素要做多次 widen/narrow 和 vector select；
- 编译器对 scalar/autovec 版本已经能做不错的优化，手写 NEON 没有明显减少
  指令路径。

默认构建验证：

```bash
cmake -S Reference_Implementation -B Reference_Implementation/build-ct-local
cmake --build Reference_Implementation/build-ct-local
ctest --test-dir Reference_Implementation/build-ct-local -R '^LESS_test_' --output-on-failure
```

结果：7 个 `LESS_test_*` 全部通过。

默认构建 wall-clock：

| 参数集 | strict CT | Level-A fast | 加速 |
|---|---:|---:|---:|
| 252_192 | 7.014 ms | 2.515 ms | 2.789x |
| 252_68 | 6.696 ms | 2.453 ms | 2.730x |
| 252_45 | 6.798 ms | 2.458 ms | 2.765x |
| 400_220 | 23.981 ms | 8.537 ms | 2.809x |
| 400_102 | 23.912 ms | 8.583 ms | 2.786x |
| 548_345 | 61.538 ms | 23.352 ms | 2.635x |
| 548_137 | 61.207 ms | 23.240 ms | 2.634x |

opt-in NEON 抽样验证：

```bash
cmake -S Reference_Implementation -B Reference_Implementation/build-neon-row-fma \
  -DLESS_RREF_ENABLE_NEON_ROW_FMA=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build Reference_Implementation/build-neon-row-fma \
  --target LESS_rref_wall_benchmark_cat_252_45 LESS_test_cat_252_45
Reference_Implementation/build-neon-row-fma/LESS_test_cat_252_45
Reference_Implementation/build-neon-row-fma/LESS_rref_wall_benchmark_cat_252_45
```

结果：

| 参数集 | strict CT | Level-A fast NEON | 加速 |
|---|---:|---:|---:|
| 252_45 | 6.682 ms | 2.638 ms | 2.533x |

结论：当前 NEON 原型正确但不更快，因此不作为默认路径。下一步如果继续推进 SIMD，
应先生成 assembly / instruction-level 对比，确认 scalar/autovec 版本到底生成了
什么，再决定是改写 NEON reduction、增加 batch lanes，还是转向 block/tile 布局。
