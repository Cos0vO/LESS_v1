# Phase 1：Verify 路径基线与矩阵生命周期

日期：2026-05-27
刷新：2026-05-28

这一阶段**不修改 LESS 算法本身**。它的目标是把 reference verifier 里的 `G`
说清楚：`G` 是什么、在内存里怎么摆放、哪些地方会被原地改写，以及当前有哪些明显
的 non-constant-time 行为。

## 1. 基线结论

- 当前源码树 **已经可以重新编译通过**。
  - 新的复查构建目录：
    [`Reference_Implementation/build-phase1-recheck`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/build-phase1-recheck)
  - 复跑命令：`cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && make -j2`
  - 结果：完整重建成功。
- 基于这次新构建目录，7 个 `LESS_test_cat_*` 全部返回 `exit=0`：
  - `LESS_test_cat_252_45`
  - `LESS_test_cat_252_68`
  - `LESS_test_cat_252_192`
  - `LESS_test_cat_400_102`
  - `LESS_test_cat_400_220`
  - `LESS_test_cat_548_137`
  - `LESS_test_cat_548_345`
- 对应日志已保存到 `/tmp/LESS_test_cat_*.phase1.recheck.log`。

历史注记：

1. 在更早一版 Phase 1 里，当前工作树确实曾因为
   [`Reference_Implementation/lib/codes.c:119`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/codes.c:119)
   的异常字符 `【‘` 而无法编译；
2. 这个编译阻塞点现在已经被修复；
3. 更早建立的 ASan 基线
   [`Reference_Implementation/build-audit-asan`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/build-audit-asan)
   仍然和这次刷新后的结果保持一致，但它不再是“当前源码状态”的主要证据。

## 2. `G` 在内存里是什么样子

最关键的类型定义在
[`Reference_Implementation/include/codes.h:34-36`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/include/codes.h:34)：

```c
typedef struct {
   FQ_ELEM values[K][N_pad] __attribute__((aligned(32)));
} generator_mat_t;
```

再结合：

- [`Reference_Implementation/include/parameters.h:195-199`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/include/parameters.h:195)：
  reference build 下 `N_pad = N`，`K_pad = K`
- [`Reference_Implementation/include/parameters.h:35-36`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/include/parameters.h:35)：
  `FQ_ELEM = uint8_t`

可以得到 reference 实现里的矩阵布局结论：

- 是连续内存
- 是 row-major
- 每个域元素占 1 字节
- `values` 前面没有隐藏 padding

因此对 reference 实现来说：

- `base_address == &G.values[0][0]`
- `byte_size == K * N`
- `row_stride_bytes == N`
- 某个元素地址可写成：`base_address + row * N + col`

运行时探针结果如下：

| Category/Target | `K` | `N` | `sizeof(generator_mat_t)` | 每行字节数 |
| --- | ---: | ---: | ---: | ---: |
| `252/45` | 126 | 252 | 31776 | 252 |
| `252/68` | 126 | 252 | 31776 | 252 |
| `252/192` | 126 | 252 | 31776 | 252 |
| `400/102` | 200 | 400 | 80000 | 400 |
| `400/220` | 200 | 400 | 80000 | 400 |
| `548/137` | 274 | 548 | 150176 | 548 |
| `548/345` | 274 | 548 | 150176 | 548 |

## 3. Verify 侧 `G` 的生命周期

主入口在
[`Reference_Implementation/lib/LESS.c:267-389`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/LESS.c:267)。

把路径写成文字图如下：

```text
PK->G_0_seed
  -> generator_sample(&G0_rref, PK->G_0_seed)
  -> generator_get_pivot_flags(&G0_rref, g0_initial_pivot_flags)
  -> generator_rref_expand(&G0_full, &G0_rref)

for each round i:
  if fixed_weight_string[i] == 0:
    round seed -> monomial_sample_salt(&mu_tilde, ...)
    G0_full -> generator_monomial_mul(&G_prime, &G0_full, &mu_tilde)
    可选：把旧 pivot flags 做一次置换
    G_prime -> generator_RREF_pivot_reuse(...) 或 generator_RREF(...)
  else:
    PK->SF_G[j] -> expand_to_rref(&G0, PK->SF_G[j], gi_initial_pivot_flags)
    sig->cf_monom_actions[k] -> apply_cf_action_to_G_with_pivots(&G_prime, ...)
    G_prime -> generator_RREF_pivot_reuse(...) 或 generator_RREF(...)

  G_prime + is_pivot_column -> 把非 pivot 列拷贝到 Ai
  Ai -> CF(&Ai)
```

这些矩阵放在哪里：

- `G0_full`、`G0`、`G_prime` 都是
  [`Reference_Implementation/lib/LESS.c:303-304`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/LESS.c:303)
  里的局部栈对象
- `generator_RREF()` 和 `generator_RREF_pivot_reuse()` 都会**原地修改**
  `G_prime`
- `Ai` 是从 RREF 之后的 `G_prime` 中，把所有非 pivot 列复制出来得到的

## 4. 依赖类型与风险表

| 阶段 / 矩阵 | 来源 | 依赖类型 | 是否原地改写 | 本阶段看到的主要风险 |
| --- | --- | --- | --- | --- |
| `G0_rref` | `generator_sample(PK->G_0_seed)` | public-only | 否 | 公钥 seed 决定稠密部分 |
| `g0_initial_pivot_flags` | `generator_get_pivot_flags(G0_rref)` | public-only | 否 | 后续会被拿去驱动 pivot reuse |
| `G0_full` | `generator_rref_expand(G0_rref)` | public-only | 否 | 作为零挑战分支的基线完整矩阵 |
| `G0` | `expand_to_rref(PK->SF_G[j], ...)` | public-key-controlled | 是 | malformed pivot flags 可改变解码形状 |
| `generator_monomial_mul` 产生的 `G_prime` | `G0_full + mu_tilde` | attacker-observable / data-dependent execution | 是 | monomial 置换会改变 pivot 搜索路径 |
| `apply_cf_action_to_G_with_pivots` 产生的 `G_prime` | `G0 + sig->cf_monom_actions[k]` | attacker-controlled | 是 | Bob 可以改列并影响 reuse / pivot 搜索 |
| `is_pivot_column` | RREF 输出辅助缓冲区 | data-dependent execution | 是 | 后续 `while (is_pivot_column[ctr])` 会受其影响 |
| `Ai` | 从 `G_prime` 的非 pivot 列复制得到 | downstream derived | 是 | 它是 `CF` 的输入，但本阶段还未细审 `CF` |

## 5. 已确认的 non-constant-time 证据点

下面这些是 Phase 1 已经确认的、verify 路径里最重要的 timing 面。

1. `generator_RREF()` 的 pivot 搜索是 data-dependent 的。  
   位置：
   [`Reference_Implementation/lib/codes.c:79-99`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/codes.c:79)
   - 两层 `while` 会在找到第一个非零 pivot 候选时停止
   - 所以工作量取决于“第一个合法 pivot 在哪里”

2. `generator_RREF()` 的 row swap 和 inversion 也是 data-dependent 的。  
   位置：
   [`Reference_Implementation/lib/codes.c:105-112`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/codes.c:105)
   和
   [`Reference_Implementation/include/fq_arith.h:159-167`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/include/fq_arith.h:159)
   - row swap 是条件分支
   - `fq_inv()` 是查表
   - `fq_inv()` 自己的注释也明确说输入 “must not be secret”

3. `generator_RREF_pivot_reuse()` 多了一轮 data-dependent 预处理。  
   位置：
   [`Reference_Implementation/lib/codes.c:152-166`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/codes.c:152)
   - 它会扫描 `was_pivot_column`
   - 会根据列内容继续找非零项
   - 还会条件性地交换行

4. `generator_RREF_pivot_reuse()` 有一个 reuse `continue`。  
   位置：
   [`Reference_Implementation/lib/codes.c:194-200`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/codes.c:194)
   - 命中 reuse 时，会跳过该 pivot 行的归一化和消元
   - 所以不同矩阵内容会对应不同工作量

5. verifier 现在默认总是启用 reuse。  
   位置：
   [`Reference_Implementation/include/parameters.h:180`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/include/parameters.h:180)
   与
   [`Reference_Implementation/include/parameters.h:257-258`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/include/parameters.h:257)
   - `VERIFY_PIVOT_REUSE_LIMIT == K`
   - `LESS_REUSE_PIVOTS_VY` 默认开启

6. `apply_cf_action_to_G_with_pivots()` 和 `Ai` 提取在 `CF` 之前也已经是
   data-dependent 的。  
   位置：
   [`Reference_Implementation/lib/codes.c:265-295`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/codes.c:265)
   与
   [`Reference_Implementation/lib/LESS.c:361-372`](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/LESS.c:361)
   - CF action 会按位重排列
   - `Ai` 提取时会一路 `while` 扫到下一个非 pivot 列
   - 它们不是本轮最核心的泄露点，但同样会影响 verify 时间

## 6. 后续阶段的计时点设计

后面做 `LESS_TRACE_RREF` 时，建议把探针固定在这些位置：

| `stage_id` | 位置 | 矩阵 | 建议记录内容 |
| --- | --- | --- | --- |
| `g0_rref_expand_exit` | `generator_rref_expand()` 之后 | `G0_full` | 矩阵快照、pivot flags、耗时 |
| `round_start` | 每轮 verify 开始处 | 无 | 轮次编号、challenge 值 |
| `monomial_mul_exit` | `generator_monomial_mul()` 之后 | `G_prime` | 矩阵快照、标记 `fixed_weight==0` 分支 |
| `expand_to_rref_exit` | `expand_to_rref()` 之后 | `G0` | 矩阵快照、解码出的 pivot flags |
| `cf_action_exit` | `apply_cf_action_to_G_with_pivots()` 之后 | `G_prime` | 矩阵快照、置换后的 pivot flags |
| `rref_entry` | 进入 `generator_RREF[_pivot_reuse]()` 之前 | `G_prime` | 矩阵快照、来源分支 |
| `rref_event` | pivot 搜索内部 | `G_prime` | 命中的 `(row,col)`、是否 row swap、是否 reuse 命中 |
| `rref_exit` | `generator_RREF[_pivot_reuse]()` 之后 | `G_prime` | 矩阵快照、最终 pivot flags、耗时 |
| `ai_copy_exit` | 非 pivot 列提取之后 | `Ai` | 拷贝列数、耗时 |

建议每个快照至少包含这些字段：

- `stage_id`
- `callsite`
- `round_index`
- `matrix_name`
- `rows`
- `cols`
- `base_address`
- `byte_size`
- `row_major_values`
- `pivot_flags`
- `secret_dependency_class`
- `events`
- `duration_ns`

## 7. Phase 1 小结

按照原计划，Phase 1 已经完成了它应该完成的事情：

- verify 侧 `G` 的生命周期已经钉住
- reference 实现里的矩阵内存布局已经钉住
- 主要 timing-sensitive 分支已经钉住
- 当前工作树已经恢复为可重建状态
- 新的 debug 基线已经通过同一组 7 个 reference 测试

而这阶段**还没有做**的内容包括：

- 还没有导出 trace
- 还没有写 timing benchmark harness
- 还没有做 hardened verifier
- 还没有做网页可视化
