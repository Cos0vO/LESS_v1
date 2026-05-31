# RREF Fixed Normalization Benchmark

This benchmark compares the constant-time inverse stage against the next stage,
where pivot-row normalization scans the full row instead of starting at the
data-dependent pivot column.

Scope:

- Implementation: `Reference_Implementation/lib/codes.c`
- Operation: standalone `generator_RREF()` wall-clock timing
- Unit: milliseconds
- Repetitions: 15 per parameter set
- Build style: temporary Release-style harness, `clang -O3 -DNDEBUG`

| Parameter set | Constant-time inverse RREF | Fixed normalization RREF | Incremental slowdown |
|---|---:|---:|---:|
| 252_192 | 3.114 | 3.514 | 1.13x |
| 252_68 | 2.805 | 3.283 | 1.17x |
| 252_45 | 3.463 | 2.998 | 0.87x |
| 400_220 | 9.751 | 10.365 | 1.06x |
| 400_102 | 9.636 | 8.980 | 0.93x |
| 548_345 | 24.745 | 24.181 | 0.98x |
| 548_137 | 24.329 | 24.862 | 1.02x |

Notes:

- The normalization loop now scans `0..N-1` and uses a mask to apply the scaled
  value only when `col >= pivot_column`.
- This preserves the old result while removing the pivot-column-dependent loop
  bound.
- Later steps still need treatment: elimination branch, failure return, and
  pivot reuse.

Raw table:

- `docs/experiments/rref-fixed-normalization-benchmark.csv`
