# RREF 验证报告

## 当前状态

Reference implementation 现在已经有命名的 CT/QCT interface layer，并且默认
signing integration 会避开 fast pivot reuse。

## 功能验证

运行：

```bash
ctest --test-dir Reference_Implementation/build-ct-local -R '^LESS_test_' --output-on-failure
```

当前 tree 的预期状态：7 个 `LESS_test_*` 参数集测试全部通过。

## 结构验证

当前 Level-A 候选实现已经从源码层面检查了以下性质：

- main `K` 个 RREF round 内没有 early return；
- pivot search 会固定扫描剩余 candidate rectangle；
- 使用 masked row movement，而不是直接 selected-row swap；
- pivot row normalization 使用 fixed full-row loop；
- elimination 会扫描每一行，并用 mask 决定是否更新；
- pivot value 和 row multiplier 都通过 fixed scan 选择。

fast pivot-reuse 实现被有意排除在这个 claim 之外。它仍然包含
data-dependent branch 和 early exit。

## 剩余验证工作

- 增加独立 RREF equivalence test，对比 masked candidate 和 reference oracle。
- 增加 dudect 分类测试：pivot-row position、pivot-reuse count、field value。
- 从 non-bitcode build 生成 IR / assembly inspection artifacts。
- 为 FAST、CT、QCT 模式增加 cache / perf counter variance 表。
