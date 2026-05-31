# RREF CT 设计说明

## 新增接口

- `ct.h`：统一 all-ones mask 约定，并提供 scalar select / equality helper。
- `fq_ct.h`：finite-field operation wrapper，以及 `fq_inv_ct_safe()`。
- `row_ct.h`：后续 row-level refactor 可复用的 masked row helper。
- `generator_RREF_ct()`：当前 Level-A 候选实现的命名入口。
- `generator_RREF_qct_pivot_reuse()`：signing-safe 的 QCT 入口；它忽略 reuse hint，并 fallback 到 `generator_RREF_ct()`。
- `generator_RREF_mode()`：FAST、CT、QCT 实验模式的 dispatch 层。

## 当前实现形态

当前 Level-A 候选实现是 `Reference_Implementation/lib/codes.c` 中已有的
`generator_RREF()`。它已经包含 `docs/experiments/` 中记录过的渐进式加固：

- fixed-scan masked pivot search；
- masked row movement；
- fixed-row normalization；
- masked elimination；
- deferred failure return；
- fixed pivot-value 和 multiplier selection scan。

新增 wrapper 的目的，是把这个候选实现显式命名为 CT/QCT 实验入口，同时不破坏仍然直接调用 `generator_RREF()` 的旧调用方。

## Signing 接入

`LESS_sign()` 现在默认使用 `generator_RREF_ct()`。旧的 signing pivot reuse
路径只有在 CMake option `LESS_SIGN_FAST_PIVOT_REUSE=ON` 显式定义
`LESS_SIGN_FAST_PIVOT_REUSE` 时才会启用。这样保留了 A/B benchmark 用的
opt-in fast path，同时让默认 signing path 走 leakage-aware 实现。

Verification 在启用 `LESS_REUSE_PIVOTS_VY` 时仍然使用
`generator_RREF_pivot_reuse()`，因为 verify-side 输入是 public 或
attacker-provided，而且这条路径是 fast baseline。

## 尚未实现

- Strict Level-B pivot-column hiding。
- 真正的 bounded / masked pivot reuse。
- 专门的 dudect harness。
- 完整的 assembly / perf-counter report。
