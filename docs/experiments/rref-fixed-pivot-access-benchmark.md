# RREF Fixed Pivot-Column Access Benchmark

This benchmark compares the no-early-return stage against the next stage, where
`generator_RREF()` no longer indexes matrix rows directly with `pivot_column`
when reading the pivot value or elimination multiplier.

Scope:

- Implementation: `Reference_Implementation/lib/codes.c`
- Operation: standalone `generator_RREF()` wall-clock timing
- Unit: milliseconds
- Repetitions: 15 per parameter set
- Build style: temporary Release-style harness, `clang -O3 -DNDEBUG`

| Parameter set | No early return RREF | Fixed pivot access RREF | Incremental slowdown |
|---|---:|---:|---:|
| 252_192 | 2.778 | 7.570 | 2.72x |
| 252_68 | 3.017 | 6.929 | 2.30x |
| 252_45 | 2.596 | 7.565 | 2.91x |
| 400_220 | 9.308 | 25.058 | 2.69x |
| 400_102 | 9.196 | 24.981 | 2.72x |
| 548_345 | 25.664 | 64.708 | 2.52x |
| 548_137 | 25.721 | 64.440 | 2.51x |

Raw fixed-pivot-access timing:

| Parameter set | min | median | p95 | max |
|---|---:|---:|---:|---:|
| 252_192 | 6.186 | 7.570 | 11.388 | 11.388 |
| 252_68 | 6.160 | 6.929 | 11.209 | 11.209 |
| 252_45 | 6.185 | 7.565 | 11.263 | 11.263 |
| 400_220 | 24.672 | 25.058 | 36.132 | 36.132 |
| 400_102 | 24.547 | 24.981 | 37.049 | 37.049 |
| 548_345 | 64.247 | 64.708 | 89.915 | 89.915 |
| 548_137 | 64.097 | 64.440 | 92.083 | 92.083 |

Notes:

- The pivot value is now selected by scanning all columns of the pivot row and
  masking in the selected column.
- Each row multiplier is also selected by scanning all columns of that row.
- This removes direct `G->values[...][pivot_column]` reads from the plain
  `generator_RREF()` path, at the cost of roughly 2.3x to 2.9x over the previous
  stage in this wall-clock harness.

Raw table:

- `docs/experiments/rref-fixed-pivot-access-benchmark.csv`
