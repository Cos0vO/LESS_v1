# LESS Experiments

This folder is the index for LESS audit and constant-time RREF experiments.

## Current RREF Work

- `rref-pivot-search-mask-benchmark.md`: before/after timing for the first
  mask-based pivot-search change in `generator_RREF()`.
- `rref-pivot-search-mask-benchmark.csv`: machine-readable copy of the same
  table.

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
