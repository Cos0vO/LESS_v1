# Phase 2 报告

更新时间：2026-05-28

## 本阶段改动

Phase 2 在 `LESS_TRACE_RREF` 开关后面增加了 verify 侧的 JSONL trace 导出路径。

代码改动：

- [Reference_Implementation/CMakeLists.txt](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/CMakeLists.txt)
- [Reference_Implementation/include/trace_rref.h](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/include/trace_rref.h)
- [Reference_Implementation/lib/trace_rref.c](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/trace_rref.c)
- [Reference_Implementation/lib/LESS.c](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/LESS.c)
- [Reference_Implementation/lib/codes.c](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/lib/codes.c)

审计辅助文件：

- [audit/phase2/trace_inspect.py](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/trace_inspect.py)
- [audit/phase2/phase2_results.json](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/phase2_results.json)
- [audit/phase2/branch_scan_results.json](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/branch_scan_results.json)
- [audit/phase2/sample_trace_252_45_round1.jsonl](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/sample_trace_252_45_round1.jsonl)

## Trace 格式

这个 trace 是“编译期开关控制，运行时环境变量启用”的：

```bash
cd /Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/build-phase2-trace
cmake .. -DLESS_TRACE_RREF=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build . --target LESS_test_cat_252_45 -j4

LESS_TRACE_RREF_FILE=/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/sample_trace_252_45_round1.jsonl \
LESS_TRACE_RREF_ROUND_LIMIT=1 \
./LESS_test_cat_252_45
```

每条 JSONL 记录目前包含这些字段：

- `record_type`
- `stage_id`
- `callsite`
- `round_index`
- `fixed_weight_value`
- `branch_label`
- `matrix_name`
- `rows`
- `cols`
- `base_address`
- `byte_size`
- `row_stride_bytes`
- `row_major_values`
- `pivot_flags`
- `secret_dependency_class`
- `duration_ns`
- `events`

当前已经接入的矩阵快照点：

- `g0_rref_expand_exit`
- `g0_expand_to_rref_exit`
- `g_prime_monomial_mul_exit`
- `g_prime_apply_cf_action_with_pivots_exit`
- `g_prime_rref_entry`
- `g_prime_rref_exit`

当前已经接入的事件点：

- `generator_rref_pivot_reuse_step`

## 最小阅读说明

可以直接这样看：

```bash
python3 /Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/trace_inspect.py \
  /Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/sample_trace_252_45_round1.jsonl
```

建议优先看这几项：

1. `stage_counts`：确认这次 verify 实际走到了哪条子路径。
2. `row_major_values`：按 `K x N` 展平后的 row-major 矩阵内容。
3. `pivot_flags`：该快照点对应的 pivot 掩码。
4. `rref_entry_exit_base_address_checks`：确认 `G_prime` 是否是在原地被修改。
5. `rref_event_summary`：快速看 reuse 命中次数、row swap 次数以及逐步 timing。

## Phase 2 样本结果

构建与测试状态：

- trace 版构建成功，目录在 [Reference_Implementation/build-phase2-trace](/Users/cosovo/workspace/pqc-signature-security/LESS/Reference_Implementation/build-phase2-trace)
- 不开 trace 的 `LESS_test_cat_252_45`：`/tmp/LESS_test_cat_252_45.phase2.tracebuild.notrace.log` 以 `all good` 结束
- 开 trace 的 `LESS_test_cat_252_45`：`/tmp/LESS_test_cat_252_45.phase2.tracebuild.trace.log` 以 `all good` 结束
- 基线 no-trace 二进制仍然通过：`/tmp/LESS_test_cat_252_45.phase2.notrace.log`

来自 [phase2_results.json](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/phase2_results.json) 的样本检查结果：

- `record_count = 133`
- `record_type_counts = {control: 2, snapshot: 5, event: 126}`
- `verify_sequence_count = 1`
- `all_snapshot_lengths_valid = true`
- `all_rref_entry_exit_base_addresses_stable = true`

样本测得的具体数字：

- `g0_rref_expand_exit.duration_ns = 26000`
- `g0_expand_to_rref_exit.duration_ns = 58000`
- `g_prime_apply_cf_action_with_pivots_exit.duration_ns = 37000`
- `g_prime_rref_exit.duration_ns = 3655000`
- `verify_end.duration_ns = 206233000`
- `generator_rref_pivot_reuse_step.step_count = 126`
- `reuse_hit_count = 69`
- `row_swap_count = 0`

解释：

- 这份 `round_limit=1` 的样本 trace 命中了 `fixed_weight_nonzero` 分支，所以这一轮实际走的是  
  `expand_to_rref -> apply_cf_action_to_G_with_pivots -> generator_RREF_pivot_reuse`
- `G_prime` 在 `g_prime_rref_entry` 和 `g_prime_rref_exit` 之间的地址完全相同，这和 Phase 1 的结论一致：RREF 过程是在同一块连续的 `K x N` 缓冲区上原地修改。
- 每个快照的 `row_major_values` 长度都等于 `rows * cols`，说明我们导出的展平矩阵格式是自洽的。

## 分支覆盖说明

这一份单轮样本没有命中 `generator_monomial_mul`，因为这次签名里的第 0 轮并不是 zero-weight 分支。

为了确认 verify 的另一条分支也确实被 trace 到了，我又额外跑了一次临时的更宽扫描，设置 `LESS_TRACE_RREF_ROUND_LIMIT=8`。结果保存在 [branch_scan_results.json](/Users/cosovo/workspace/pqc-signature-security/LESS/audit/phase2/branch_scan_results.json)：

- `has_monomial_branch = true`
- `monomial_count = 3`
- `apply_cf_count = 5`

所以，Phase 2 的 instrumentation 现在已经覆盖了 verify 的两条主分支。只是为了让主样本更小、更好读，正式保留的样本文件只展示了 nonzero 分支。

## Phase 2 退出条件

状态：已满足。

- trace 模式下，样本合法输入的 verify 结果保持成功。
- 导出的矩阵快照已经能直接展示内容和内存段元数据。
- 校验脚本确认了 row-major 布局一致性，以及 `G_prime` 在 RREF 前后保持原地地址稳定。
