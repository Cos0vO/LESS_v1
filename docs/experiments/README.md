# LESS Experiments

This folder is the index for LESS audit and constant-time RREF experiments.

## Current RREF Work

- `rref-pivot-search-mask-benchmark.md`: before/after timing for the first
  mask-based pivot-search change in `generator_RREF()`.
- `rref-pivot-search-mask-benchmark.csv`: machine-readable copy of the same
  table.
- `rref-masked-row-swap-benchmark.md`: timing for the next step, replacing the
  direct pivot-row swap with a fixed-scan masked row swap.
- `rref-masked-row-swap-benchmark.csv`: machine-readable copy of the row-swap
  timing table.
- `rref-constant-time-inverse-benchmark.md`: timing for replacing
  `fq_inv_table[x]` with a fixed multiplication-chain inverse.
- `rref-constant-time-inverse-benchmark.csv`: machine-readable copy of the
  inverse timing table.
- `rref-fixed-normalization-benchmark.md`: timing for changing pivot-row
  normalization to scan the full row with a mask.
- `rref-fixed-normalization-benchmark.csv`: machine-readable copy of the
  fixed-normalization timing table.
- `rref-masked-elimination-benchmark.md`: timing for replacing the
  `row_idx != pivot_row` elimination branch with masked row updates.
- `rref-masked-elimination-benchmark.csv`: machine-readable copy of the masked
  elimination timing table.
- `rref-no-early-return-benchmark.md`: timing for deferring
  `generator_RREF()` failure return until after all rounds.
- `rref-no-early-return-benchmark.csv`: machine-readable copy of the no early
  return timing table.
- `rref-fixed-pivot-access-benchmark.md`: timing for replacing direct
  `pivot_column` matrix reads with fixed scans and masked selection.
- `rref-fixed-pivot-access-benchmark.csv`: machine-readable copy of the fixed
  pivot-column access benchmark.
- `rref-level-a-fast-pivot-access-benchmark.md`: timing for the Level-A fast
  variant that restores direct `pivot_column` access while preserving the
  fixed/ masked schedule for secret-derived pivot-row behavior.
- `rref-simd-row-fma-optimization.md`: Chinese running log for Row FMA
  optimization, including the helper extraction step and the opt-in NEON
  experiment.

## Previous Experiment Data

The historical audit phases are exposed here as links to the original data
under `audit/` so large raw trace files are not duplicated.

- `phase1`: baseline RREF/verify-path audit report and structured results.
- `phase2`: trace export, reuse/no-reuse comparison, and trace inspection data.
- `phase3`: Bob timing-oracle harness, summaries, timing NDJSON, and raw trace
  JSONL files.
- `rref_reuse_harness.c`: malformed/precondition test harness for pivot reuse.
- `less_verify_malformed_pk_harness.c`: malformed public-key verify harness.

The original `audit/` paths are intentionally preserved because existing
reports and scripts refer to them.
