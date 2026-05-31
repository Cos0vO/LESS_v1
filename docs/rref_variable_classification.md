# RREF 变量分类

| 变量 | 分类 | 是否允许控制 branch | 是否允许作为数组下标 | 当前处理方式 |
|---|---|---:|---:|---|
| `row_to_reduce` | public schedule | 是 | 是 | 固定 `0..K-1` loop。 |
| `row` / `col` loop counters | public schedule | 是 | 是 | 作为 loop counter 使用。 |
| `pivot_row` | signing/keygen 中 secret-derived | 否 | 否 | 普通 `generator_RREF()` 使用 masked scan/swap；pivot row 被移动到 public `row_to_reduce` 后再处理。 |
| `found_pivot` | secret-derived | 否 | 否 | 转换成 mask；失败状态累计到所有固定 round 结束后再返回。 |
| `pivot_column` | Level A 下允许公开 | Level A 下允许 | Level A 下允许 | 仍有 pivot-column 相关访问；只在 Level A 目标下可接受。 |
| `scaling_factor` | 由 pivot value 派生，secret-derived | 否 | 否 | `fq_inv()` 使用固定乘法链。 |
| `multiplier` / `factor` | secret-derived | 否 | 否 | 在 Level-A 候选实现中用于 masked row update。 |
| `was_pivot_column[col]` | 取决于路径，可能 public，也可能 secret-derived | signing 中不允许 | signing 中不允许 | fast reuse path 会基于它 branch；signing 默认不再使用该路径。 |
| `pvt_reuse_cnt` | signing 中 secret-derived | 否 | 否 | fast reuse path 仍是 variable-time；QCT wrapper 会忽略 reuse count。 |

重要边界：`generator_RREF_pivot_reuse()` 不属于当前 Level-A CT claim。除非后续被 bounded / masked reuse 实现替换，否则它只是 verify / benchmark 的 fast path。
