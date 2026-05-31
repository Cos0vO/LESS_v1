# RREF Constant-Time Inverse Benchmark

This benchmark compares the masked row-swap stage against the next stage, where
`fq_inv()` no longer performs a secret-indexed table lookup.

Scope:

- Implementation: `Reference_Implementation/include/fq_arith.h`
- Operation: standalone `generator_RREF()` wall-clock timing
- Unit: milliseconds
- Repetitions: 15 per parameter set
- Build style: temporary Release-style harness, `clang -O3 -DNDEBUG`

| Parameter set | Masked row swap RREF | Constant-time inverse RREF | Incremental slowdown |
|---|---:|---:|---:|
| 252_192 | 3.725 | 3.114 | 0.84x |
| 252_68 | 2.458 | 2.805 | 1.14x |
| 252_45 | 4.236 | 3.463 | 0.82x |
| 400_220 | 9.685 | 9.751 | 1.01x |
| 400_102 | 10.425 | 9.636 | 0.92x |
| 548_345 | 25.614 | 24.745 | 0.97x |
| 548_137 | 25.562 | 24.329 | 0.95x |

Notes:

- The new `fq_inv()` computes `x^125 mod 127` with a fixed multiplication
  chain. For nonzero `x`, this is `x^{-1}` by Fermat's little theorem; `x = 0`
  still maps to `0`.
- The table shows noisy wall-clock measurements. The important security change
  is removing `fq_inv_table[x]`, not the small timing variation in this run.
- Later steps still need treatment: normalization range, elimination branch,
  failure return, and pivot reuse.

Raw table:

- `docs/experiments/rref-constant-time-inverse-benchmark.csv`
