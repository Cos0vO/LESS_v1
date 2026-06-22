# RREF Level-A Fast Pivot-Column Access Benchmark

This benchmark compares the stricter CT RREF candidate against a Level-A fast
variant. The Level-A fast variant keeps fixed pivot search, masked row movement,
deferred failure, and masked elimination, but allows direct memory access through
the selected `pivot_column`.

Scope:

- Implementation: `Reference_Implementation/lib/codes.c`
- Operation: standalone RREF wall-clock timing
- Unit: milliseconds
- Repetitions: 24 per parameter set
- Build: `Reference_Implementation/build-ct-local`

| Parameter set | Strict CT RREF | Level-A fast RREF | Speedup |
|---|---:|---:|---:|
| 252_192 | 6.808 | 2.450 | 2.778x |
| 252_68 | 6.794 | 2.440 | 2.784x |
| 252_45 | 5.891 | 2.176 | 2.707x |
| 400_220 | 23.610 | 8.426 | 2.802x |
| 400_102 | 23.980 | 8.443 | 2.840x |
| 548_345 | 60.444 | 22.774 | 2.654x |
| 548_137 | 60.422 | 22.775 | 2.653x |

Notes:

- The benchmark binary checks that strict and Level-A fast outputs match for
  each sampled matrix.
- This is a Level-A optimization only. It intentionally allows the
  `pivot_column` sequence to influence memory access.
- The variant does not restore variable-time pivot row selection, row-swap
  indexing, early return, multiplier-zero skipping, or pivot reuse.

Command:

```bash
for t in 252_192 252_68 252_45 400_220 400_102 548_345 548_137; do Reference_Implementation/build-ct-local/LESS_rref_wall_benchmark_cat_${t}; done
```
