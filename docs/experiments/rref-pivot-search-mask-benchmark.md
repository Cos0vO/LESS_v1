# RREF Pivot Search Mask Benchmark

This benchmark compares `generator_RREF()` before and after replacing the
data-dependent pivot search with a fixed-scan mask-based pivot selection.

Scope:

- Implementation: `Reference_Implementation/lib/codes.c`
- Operation: standalone RREF wall-clock timing
- Unit: milliseconds
- Repetitions: 15 per parameter set
- Build style: temporary Release-style harness, `clang -O3 -DNDEBUG`
- Pivot reuse: verify/sign reuse defaults were left enabled, but this table is
  for standalone `generator_RREF()` timing.

| Parameter set | Before RREF | After RREF | Slowdown |
|---|---:|---:|---:|
| 252_192 | 0.347 | 2.083 | 6.00x |
| 252_68 | 0.350 | 2.069 | 5.91x |
| 252_45 | 0.351 | 2.076 | 5.91x |
| 400_220 | 1.248 | 8.417 | 6.74x |
| 400_102 | 1.231 | 8.321 | 6.76x |
| 548_345 | 4.521 | 22.742 | 5.03x |
| 548_137 | 4.486 | 22.755 | 5.07x |

Interpretation:

- The slowdown is expected for this first step because the pivot search now
  scans the full remaining candidate rectangle instead of stopping at the first
  pivot.
- This is not a complete constant-time RREF yet. Row swap, `fq_inv`, row
  normalization, elimination, failure return, and pivot reuse still need later
  treatment.

Raw table:

- `docs/experiments/rref-pivot-search-mask-benchmark.csv`
