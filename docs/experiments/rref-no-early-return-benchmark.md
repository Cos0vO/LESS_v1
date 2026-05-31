# RREF No Early Return Benchmark

This benchmark compares the masked-elimination stage against the next stage,
where `generator_RREF()` no longer returns immediately when a pivot is missing.

Scope:

- Implementation: `Reference_Implementation/lib/codes.c`
- Operation: standalone `generator_RREF()` wall-clock timing
- Unit: milliseconds
- Repetitions: 15 per parameter set
- Build style: temporary Release-style harness, `clang -O3 -DNDEBUG`

| Parameter set | Masked elimination RREF | No early return RREF | Incremental slowdown |
|---|---:|---:|---:|
| 252_192 | 2.635 | 2.778 | 1.05x |
| 252_68 | 2.556 | 3.017 | 1.18x |
| 252_45 | 2.560 | 2.596 | 1.01x |
| 400_220 | 9.255 | 9.308 | 1.01x |
| 400_102 | 9.144 | 9.196 | 1.01x |
| 548_345 | 25.106 | 25.664 | 1.02x |
| 548_137 | 24.483 | 25.721 | 1.05x |

Notes:

- `generator_RREF()` now accumulates `rref_success` and returns after all `K`
  rounds.
- When a pivot is missing, `found_mask` disables pivot-flag writes, row swap,
  normalization, and elimination for that round.
- This leaves the plain `generator_RREF()` path much closer to the Wave-style
  fixed-work pattern. The main remaining work is the separate
  `generator_RREF_pivot_reuse()` path and downstream pivot extraction.

Raw table:

- `docs/experiments/rref-no-early-return-benchmark.csv`
