# Phase 2 验签侧 No-Reuse 实验

更新日期：2026-05-28

## 目标

在不改变默认构建行为的前提下，运行一组和 Phase 2 相同的 verify 侧 trace 实验，但关闭 verify 中的 pivot reuse。

## 使用的代码路径

[Reference_Implementation/include/parameters.h](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/include/parameters.h) 里现在对 verify reuse 宏做了条件控制，所以默认构建仍然会定义 `LESS_REUSE_PIVOTS_VY`，但专门的实验构建可以通过 `LESS_DISABLE_VERIFY_PIVOT_REUSE` 把它关掉。

构建系统里的支持加在了 [Reference_Implementation/CMakeLists.txt](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/CMakeLists.txt)。

另外，no-reuse 的 verify 分支也会在 `apply_cf_action_to_G()` 之后导出自己的 trace 快照，这部分逻辑在 [Reference_Implementation/lib/LESS.c](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/LESS.c) 里。

## 构建与运行

no-reuse 的 trace 构建目录：

- [Reference_Implementation/build-phase2-trace-noreuse](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/build-phase2-trace-noreuse)

配置命令：

```bash
cd /Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/build-phase2-trace-noreuse
cmake .. \
  -DLESS_TRACE_RREF=ON \
  -DLESS_DISABLE_VERIFY_PIVOT_REUSE=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build . --target LESS_test_cat_252_45 -j4
```

样例运行方式：

```bash
LESS_TRACE_RREF_FILE=/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/sample_trace_252_45_round1_noreuse.jsonl \
LESS_TRACE_RREF_ROUND_LIMIT=1 \
./LESS_test_cat_252_45
```

## 产物

- no-reuse 样本 trace：
  [audit/phase2/sample_trace_252_45_round1_noreuse.jsonl](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/sample_trace_252_45_round1_noreuse.jsonl)
- no-reuse 结构化结果：
  [audit/phase2/phase2_results_noreuse.json](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/phase2_results_noreuse.json)
- reuse 与 no-reuse 的对照结果：
  [audit/phase2/phase2_noreuse_comparison.json](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/phase2_noreuse_comparison.json)
- no-reuse 分支扫描结果：
  [audit/phase2/branch_scan_results_noreuse.json](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/branch_scan_results_noreuse.json)

运行日志：

- `/tmp/LESS_test_cat_252_45.phase2.noreuse.notrace.log`
- `/tmp/LESS_test_cat_252_45.phase2.noreuse.trace.log`

这两个日志最终都以 `all good` 结束。

## 主要观察

这个单轮样本仍然包含 `133` 条记录，并且仍然保持了 `G_prime` 的稳定原地修改特征：

- `record_count = 133`
- `all_snapshot_lengths_valid = true`
- `all_rref_entry_exit_base_addresses_stable = true`

最主要的差异出现在 RREF 的行为上：

- reuse 构建：
  - 事件阶段：`generator_rref_pivot_reuse_step`
  - `reuse_hit_count = 69`
  - `row_swap_count = 0`
  - `g_prime_rref_exit.duration_ns = 3655000`
- no-reuse 构建：
  - 事件阶段：`generator_rref_step`
  - `reuse_hit_count = 0`
  - `row_swap_count = 69`
  - `g_prime_rref_exit.duration_ns = 8157000`

所以，对这个确定性的 `252_45` 样本来说，关闭 verify reuse 之后，同一个第一轮结构会从“69 次 reuse 命中、0 次 swap”变成“0 次 reuse 命中、69 次 swap”。

## 记录样本中的时间差异

下面这些数字来自 [phase2_noreuse_comparison.json](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/phase2_noreuse_comparison.json)：

- 每一步 RREF 事件的平均时间：
  - reuse：`22309 ns`
  - no-reuse：`56690 ns`
- 第一轮完整的 `g_prime_rref_exit` 持续时间：
  - reuse：`3655000 ns`
  - no-reuse：`8157000 ns`
  - 变慢比例：`2.232x`
- 整个样本 verify 的持续时间：
  - reuse：`206233000 ns`
  - no-reuse：`380784000 ns`
  - 变慢比例：`1.846x`

这还不是完整的统计学 timing 研究。它本质上还是一个受控的单样本 trace 对照实验，只是两边都使用同一个确定性的 KAT 风格运行。

## 分支覆盖说明

和 reuse 构建一样，no-reuse 构建也额外做了一次 8 轮的临时扫描检查：

- `has_monomial_branch = true`
- `monomial_count = 3`
- `apply_cf_count = 5`

结果保存在 [branch_scan_results_noreuse.json](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/branch_scan_results_noreuse.json)。

## 结论

现在已经可以在编译期比较干净地关闭 verify 侧的 reuse。

对于这次采样到的 `252_45` verify trace，关闭 reuse 会：

- 保持合法测试样例上的正确性不变，
- 保持矩阵快照/导出机制继续正常工作，
- 让 RREF 事件流里的 reuse hit 消失，
- 同时让观测到的第一轮 RREF 工作明显变慢。
