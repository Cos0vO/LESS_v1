# Leakage-Aware RREF 威胁模型

本文档记录 LESS RREF 加固工作的当前工程目标。

## 需要保护的路径

| 路径 | 当前策略 | 原因 |
|---|---|---|
| KeyGen | 通过 `generator_RREF()` 使用 CT Level-A 候选实现 | 公钥压缩前会处理由 private monomial 派生出的矩阵。 |
| Sign | 默认通过 `generator_RREF_ct_level_a_fast()` 使用 Level-A CT/QCT 候选实现 | 签名会处理 secret 和 ephemeral monomial 派生出的矩阵，因此除非显式启用 `LESS_SIGN_FAST_PIVOT_REUSE`，否则不使用 pivot reuse。 |
| Verify | 默认保留 fast pivot reuse | 验证路径处理 public / attacker-provided 数据，保留为性能基线。 |
| Benchmark | FAST 和 CT/QCT 模式都可以使用 | benchmark 必须明确说明所选模式。 |

## Level A 目标

第一阶段工程目标是和规格对齐的 Level A：

- `pivot_row` 视为 secret-derived。
- 在 hardened path 中，`pivot_row` 的选择不能直接控制 branch、loop bound、early return，或者 selected-row matrix indexing。
- 在 Level A 目标下，`pivot_column` 允许影响 memory access。

`Reference_Implementation/lib/codes.c` 中的 `generator_RREF()` 是当前
Level-A 候选实现。它已经使用 fixed pivot scan、masked row movement、fixed
normalization、masked elimination，以及 deferred failure return。

`generator_RREF_ct_level_a_fast()` 是默认 signing path 使用的优化入口。它
保持上述 pivot-row 保护，但允许 `pivot_column` 直接影响矩阵读取，用来移除
strict 候选中全列 masked selection 的主要开销。

## Level B 目标

Strict Level B 目前还没有实现。Level B 还需要隐藏 pivot-column pattern 和
rank evolution：不能直接用 `pivot_column` 选择内存访问，并且需要执行固定的
all-column schedule。

## 当前 Pivot-Reuse 策略

`generator_RREF_pivot_reuse()` 仍然是 fast variable-time 实现。它保留给
verify-side 和 benchmark 使用。

`generator_RREF_qct_pivot_reuse()` 是 signing-safe 的第一步：它忽略 reuse
hint，直接 fallback 到 Level-A fast 候选实现。这样可以先移除 signing 中由
reuse count 决定的工作量差异，但它还不是 bounded / masked pivot-reuse
实现。
