# RREF Masked Elimination Benchmark

This benchmark compares the fixed-normalization stage against the next stage,
where row elimination no longer branches on `row_idx != pivot_row`.

Scope:

- Implementation: `Reference_Implementation/lib/codes.c`
- Operation: standalone `generator_RREF()` wall-clock timing
- Unit: milliseconds
- Repetitions: 15 per parameter set
- Build style: temporary Release-style harness, `clang -O3 -DNDEBUG`

| Parameter set | Fixed normalization RREF | Masked elimination RREF | Incremental slowdown |
|---|---:|---:|---:|
| 252_192 | 3.514 | 2.635 | 0.75x |
| 252_68 | 3.283 | 2.556 | 0.78x |
| 252_45 | 2.998 | 2.560 | 0.85x |
| 400_220 | 10.365 | 9.255 | 0.89x |
| 400_102 | 8.980 | 9.144 | 1.02x |
| 548_345 | 24.181 | 25.106 | 1.04x |
| 548_137 | 24.862 | 24.483 | 0.98x |

Notes:

- Elimination now scans every row and uses a mask to preserve the pivot row.
- The table is noisy wall-clock data; some medians improve because the compiler
  sees a simpler straight-line loop.
- Remaining plain-`generator_RREF()` work: remove the data-dependent failure
  return or defer it until after fixed work, and then repeat equivalent changes
  in `generator_RREF_pivot_reuse()`.

Raw table:

- `docs/experiments/rref-masked-elimination-benchmark.csv`
