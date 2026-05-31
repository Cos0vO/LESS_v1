# RREF Masked Row Swap Benchmark

This benchmark compares the first constant-time RREF step, fixed-scan masked
pivot search, against the next step, fixed-scan masked row swap.

Scope:

- Implementation: `Reference_Implementation/lib/codes.c`
- Operation: standalone `generator_RREF()` wall-clock timing
- Unit: milliseconds
- Repetitions: 15 per parameter set
- Build style: temporary Release-style harness, `clang -O3 -DNDEBUG`

| Parameter set | Pivot search mask RREF | + masked row swap RREF | Incremental slowdown |
|---|---:|---:|---:|
| 252_192 | 2.083 | 3.725 | 1.79x |
| 252_68 | 2.069 | 2.458 | 1.19x |
| 252_45 | 2.076 | 4.236 | 2.04x |
| 400_220 | 8.417 | 9.685 | 1.15x |
| 400_102 | 8.321 | 10.425 | 1.25x |
| 548_345 | 22.742 | 25.614 | 1.13x |
| 548_137 | 22.755 | 25.562 | 1.12x |

Notes:

- The row-swap change scans every candidate row and applies an XOR swap only
  when the row index matches the selected pivot row.
- This removes the explicit `if (row_to_reduce != pivot_row)` branch and avoids
  directly indexing only the selected pivot row for swapping.
- Later steps still need treatment: `fq_inv`, normalization range, elimination
  branch, failure return, and pivot reuse.

Raw table:

- `docs/experiments/rref-masked-row-swap-benchmark.csv`
