# Phase 3 Report

## Summary

### 252_45

- target_round = 0, sf_g_index = 4, source_col = 126, target_output_col = 62
- legal verify success count = 200 / 200
- wrong p50 = 27545000 ns, correct p50 = 27557000 ns
- median gap = -12000 ns, median ratio = 0.9995645389556193
- correct faster than wrong: False
- stable distinguishable by [p05, p95] separation: False
- dominant internal stage = verify_end_ns

### 252_68

- target_round = 0, sf_g_index = 1, source_col = 128, target_output_col = 66
- legal verify success count = 200 / 200
- wrong p50 = 40317000 ns, correct p50 = 40421000 ns
- median gap = -104000 ns, median ratio = 0.997427079983177
- correct faster than wrong: False
- stable distinguishable by [p05, p95] separation: False
- dominant internal stage = verify_end_ns

### 400_102

- target_round = 0, sf_g_index = 1, source_col = 200, target_output_col = 102
- legal verify success count = 200 / 200
- wrong p50 = 146925000 ns, correct p50 = 146058000 ns
- median gap = 867000 ns, median ratio = 1.0059359980281806
- correct faster than wrong: True
- stable distinguishable by [p05, p95] separation: False
- dominant internal stage = g_prime_rref_exit_ns

## Phase 2 No-Reuse Reference

- 252_45 single-sample verify slowdown from disabling verify reuse: 1.846
- 252_45 single-sample RREF slowdown from disabling verify reuse: 2.232

## Notes

- `correct faster` compares wall-clock medians (`p50`) between the `correct` and `wrong` variants.
- `stable distinguishable` uses a simple non-overlap test on the `[p05, p95]` timing intervals.
- `dominant internal stage` is chosen from trace-stage mean deltas between `wrong` and `correct`.
