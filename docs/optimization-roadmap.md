# LESS_v1 Leakage-Aware RREF 路线图

## Reference Implementation 已完成内容

- 增加显式 CT helper headers：`ct.h`、`fq_ct.h`、`row_ct.h`。
- 增加 FAST / CT / QCT 实验用的命名 RREF 入口。
- 默认 signing path 改为使用 Level-A CT 候选实现，而不是 fast pivot reuse。
- 增加 `LESS_SIGN_FAST_PIVOT_REUSE`，作为 opt-in CMake benchmark switch。
- verify-side pivot reuse 保留为 public-input fast baseline。

## 当前 Checklist

- [x] 写明 Level-A threat model 和 variable classification。
- [x] 将当前 Level-A `generator_RREF()` 候选实现命名为 `generator_RREF_ct()`。
- [x] 增加 signing-safe 的 QCT pivot-reuse 入口；当前版本 fallback 到 CT candidate。
- [x] 保留 fast `generator_RREF_pivot_reuse()`，用于 verify 和 benchmark 对比。
- [ ] 实现真正的 bounded / masked pivot reuse。
- [ ] 增加独立 RREF equivalence tests。
- [ ] 增加 dudect harnesses。
- [ ] 生成 assembly / perf-counter validation artifacts。
- [ ] 将 optimized SIMD implementations 移植到 CT，或者明确隔离为 fast-only。

## 下一步工程目标

下一个具体目标是真正的 bounded / masked pivot reuse：

1. 用 fixed scan 替换 `was_pivot_column` pre-pass branch。
2. 移除由 reuse count 决定的 `continue`。
3. 无论 pivot 是否 reused，都保持相同的外层 work schedule。
4. 增加 benchmark，对比 fast reuse、QCT fallback、bounded reuse。
