#!/usr/bin/env python3

import argparse
import json
import math
from collections import defaultdict
from pathlib import Path


RREF_EVENT_STAGE_IDS = {"generator_rref_pivot_reuse_step", "generator_rref_step"}


def load_ndjson(path: Path):
    rows = []
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        rows.append(json.loads(line))
    return rows


def percentile(values, pct):
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    rank = math.ceil((pct / 100.0) * len(ordered))
    rank = max(1, min(rank, len(ordered)))
    return ordered[rank - 1]


def summarize_numeric(values):
    if not values:
        return {
            "count": 0,
            "min": None,
            "max": None,
            "mean": None,
            "stddev": None,
            "p05": None,
            "p50": None,
            "p95": None,
        }
    mean_value = sum(values) / len(values)
    if len(values) > 1:
        variance = sum((value - mean_value) ** 2 for value in values) / (len(values) - 1)
        stddev = math.sqrt(variance)
    else:
        stddev = 0.0
    return {
        "count": len(values),
        "min": min(values),
        "max": max(values),
        "mean": mean_value,
        "stddev": stddev,
        "p05": percentile(values, 5),
        "p50": percentile(values, 50),
        "p95": percentile(values, 95),
    }


def group_trace_records(trace_rows):
    grouped = defaultdict(list)
    for row in trace_rows:
        grouped[row["verify_sequence"]].append(row)
    return grouped


def split_trace_lines_by_sequence(trace_path: Path):
    split = defaultdict(list)
    for raw_line in trace_path.read_text().splitlines():
        raw_line = raw_line.strip()
        if not raw_line:
            continue
        row = json.loads(raw_line)
        split[row["verify_sequence"]].append(raw_line)
    return split


def extract_trace_metrics(records):
    stage_duration_lists = defaultdict(list)
    total_rref_event_ns = 0
    event_count = 0
    reuse_hits = 0
    row_swaps = 0

    for record in records:
        if record["record_type"] in {"snapshot", "control"}:
            stage_duration_lists[record["stage_id"]].append(record["duration_ns"])
        if record["record_type"] == "event" and record["stage_id"] in RREF_EVENT_STAGE_IDS:
            total_rref_event_ns += record["duration_ns"]
            event_count += 1
            event = record["events"][0]
            reuse_hits += int(event["reuse_hit"])
            row_swaps += int(event["did_row_swap"])

    metrics = {}
    for stage_id, durations in stage_duration_lists.items():
        if len(durations) == 1:
            metrics[f"{stage_id}_ns"] = durations[0]
        else:
            metrics[f"{stage_id}_ns"] = sum(durations)

    metrics["rref_event_total_ns"] = total_rref_event_ns
    metrics["rref_event_mean_ns"] = (total_rref_event_ns / event_count) if event_count else None
    metrics["rref_event_step_count"] = event_count
    metrics["reuse_hit_count"] = reuse_hits
    metrics["row_swap_count"] = row_swaps
    return metrics


def choose_dominant_stage(trace_stats):
    candidate_keys = [
        "verify_end_ns",
        "g0_expand_to_rref_exit_ns",
        "g_prime_apply_cf_action_with_pivots_exit_ns",
        "g_prime_rref_exit_ns",
        "rref_event_total_ns",
    ]
    deltas = {}
    for key in candidate_keys:
        wrong_mean = trace_stats["wrong"].get(key, {}).get("mean")
        correct_mean = trace_stats["correct"].get(key, {}).get("mean")
        if wrong_mean is None or correct_mean is None:
            continue
        deltas[key] = abs(wrong_mean - correct_mean)
    if not deltas:
        return None
    return max(deltas, key=deltas.get)


def build_param_summary(param_tag: str,
                        timing_path: Path,
                        trace_path: Path,
                        output_dir: Path):
    timing_rows = load_ndjson(timing_path)
    wall_rows = [row for row in timing_rows if row["sample_kind"] == "wall"]
    trace_rows_meta = [row for row in timing_rows if row["sample_kind"] == "trace"]
    trace_rows = load_ndjson(trace_path) if trace_path.exists() else []

    if not timing_rows:
        raise RuntimeError(f"no timing rows found for {param_tag}")

    variants = ["legal", "wrong", "correct"]
    wall_stats = {}
    verify_result_counts = {}
    target_info = None
    for variant in variants:
        variant_rows = [row for row in wall_rows if row["variant"] == variant]
        durations = [row["duration_ns"] for row in variant_rows]
        wall_stats[variant] = summarize_numeric(durations)
        verify_result_counts[variant] = {
            "success": sum(1 for row in variant_rows if row["verify_result"] == 1),
            "failure": sum(1 for row in variant_rows if row["verify_result"] != 1),
        }
        if target_info is None and variant_rows:
            exemplar = variant_rows[0]
            target_info = {
                "target_round": exemplar["target_round"],
                "sf_g_index": exemplar["sf_g_index"],
                "source_col": exemplar["source_col"],
                "target_output_col": exemplar["target_output_col"],
            }

    trace_sequence_to_variant = {
        row["trace_sequence_index"]: row["variant"]
        for row in trace_rows_meta
        if row.get("trace_sequence_index", 0) > 0
    }
    trace_records_by_sequence = group_trace_records(trace_rows)
    raw_trace_lines_by_sequence = split_trace_lines_by_sequence(trace_path) if trace_path.exists() else {}

    trace_variant_metrics = defaultdict(list)
    per_variant_lines = defaultdict(list)
    for sequence_index, variant in sorted(trace_sequence_to_variant.items()):
        records = trace_records_by_sequence.get(sequence_index, [])
        if not records:
            continue
        trace_variant_metrics[variant].append(extract_trace_metrics(records))
        per_variant_lines[variant].extend(raw_trace_lines_by_sequence.get(sequence_index, []))

    trace_stats = {}
    for variant in variants:
        metric_rows = trace_variant_metrics.get(variant, [])
        metric_names = sorted({metric for row in metric_rows for metric in row.keys()})
        trace_stats[variant] = {}
        for metric_name in metric_names:
            values = [row[metric_name] for row in metric_rows if row.get(metric_name) is not None]
            trace_stats[variant][metric_name] = summarize_numeric(values)

        trace_output_path = output_dir / f"phase3_traces_{param_tag}_{variant}.jsonl"
        trace_output_path.write_text(
            "\n".join(per_variant_lines.get(variant, [])) + ("\n" if per_variant_lines.get(variant) else "")
        )

    wrong_p50 = wall_stats["wrong"]["p50"]
    correct_p50 = wall_stats["correct"]["p50"]
    wrong_p05 = wall_stats["wrong"]["p05"]
    wrong_p95 = wall_stats["wrong"]["p95"]
    correct_p05 = wall_stats["correct"]["p05"]
    correct_p95 = wall_stats["correct"]["p95"]

    comparison = {
        "correct_faster": (correct_p50 is not None and wrong_p50 is not None and correct_p50 < wrong_p50),
        "stable_distinguishable": (
            wrong_p05 is not None and wrong_p95 is not None and
            correct_p05 is not None and correct_p95 is not None and
            ((correct_p95 < wrong_p05) or (wrong_p95 < correct_p05))
        ),
        "median_gap_ns": (wrong_p50 - correct_p50) if (wrong_p50 is not None and correct_p50 is not None) else None,
        "median_ratio": (wrong_p50 / correct_p50) if (wrong_p50 is not None and correct_p50 not in (None, 0)) else None,
        "dominant_internal_stage": choose_dominant_stage(trace_stats),
    }

    param_summary = {
        "parameter_tag": param_tag,
        "target_info": target_info,
        "wall_stats": wall_stats,
        "verify_result_counts": verify_result_counts,
        "trace_stats": trace_stats,
        "comparison": comparison,
        "sample_counts": {
            "wall_rows": len(wall_rows),
            "trace_rows": len(trace_rows_meta),
            "trace_record_count": len(trace_rows),
        },
        "input_files": {
            "timings": str(timing_path),
            "trace_source": str(trace_path),
        },
    }

    (output_dir / f"phase3_results_{param_tag}.json").write_text(
        json.dumps(param_summary, indent=2, sort_keys=True) + "\n"
    )
    return param_summary


def build_report(all_params, output_dir: Path):
    overview = {entry["parameter_tag"]: entry for entry in all_params}
    (output_dir / "phase3_overview.json").write_text(json.dumps(overview, indent=2, sort_keys=True) + "\n")

    noreuse_reference = None
    noreuse_path = output_dir.parent / "phase2" / "phase2_noreuse_comparison.json"
    if noreuse_path.exists():
        noreuse_reference = json.loads(noreuse_path.read_text())

    lines = [
        "# Phase 3 Report",
        "",
        "## Summary",
        "",
    ]

    for entry in all_params:
        comp = entry["comparison"]
        target = entry["target_info"]
        lines.extend([
            f"### {entry['parameter_tag']}",
            "",
            f"- target_round = {target['target_round']}, sf_g_index = {target['sf_g_index']}, source_col = {target['source_col']}, target_output_col = {target['target_output_col']}",
            f"- legal verify success count = {entry['verify_result_counts']['legal']['success']} / {entry['wall_stats']['legal']['count']}",
            f"- wrong p50 = {entry['wall_stats']['wrong']['p50']} ns, correct p50 = {entry['wall_stats']['correct']['p50']} ns",
            f"- median gap = {comp['median_gap_ns']} ns, median ratio = {comp['median_ratio']}",
            f"- correct faster than wrong: {comp['correct_faster']}",
            f"- stable distinguishable by [p05, p95] separation: {comp['stable_distinguishable']}",
            f"- dominant internal stage = {comp['dominant_internal_stage']}",
            "",
        ])

    if noreuse_reference is not None:
        lines.extend([
            "## Phase 2 No-Reuse Reference",
            "",
            f"- 252_45 single-sample verify slowdown from disabling verify reuse: {noreuse_reference.get('verify_end_slowdown_ratio_noreuse_over_reuse')}",
            f"- 252_45 single-sample RREF slowdown from disabling verify reuse: {noreuse_reference.get('rref_exit_slowdown_ratio_noreuse_over_reuse')}",
            "",
        ])

    lines.extend([
        "## Notes",
        "",
        "- `correct faster` compares wall-clock medians (`p50`) between the `correct` and `wrong` variants.",
        "- `stable distinguishable` uses a simple non-overlap test on the `[p05, p95]` timing intervals.",
        "- `dominant internal stage` is chosen from trace-stage mean deltas between `wrong` and `correct`.",
        "",
    ])

    (output_dir / "phase3_report.md").write_text("\n".join(lines))


def main():
    parser = argparse.ArgumentParser(description="Summarize LESS Phase 3 timing-oracle experiments.")
    parser.add_argument(
        "--bundle",
        action="append",
        required=True,
        help="Bundle in the form param_tag:/path/to/timings.ndjson:/path/to/trace_source.jsonl",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        required=True,
        help="Directory where phase3_results/report/overview should be written",
    )
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    all_params = []
    for bundle in args.bundle:
        try:
            param_tag, timing_file, trace_file = bundle.split(":", 2)
        except ValueError as exc:
            raise SystemExit(f"invalid --bundle value: {bundle}") from exc
        all_params.append(
            build_param_summary(param_tag, Path(timing_file), Path(trace_file), args.output_dir)
        )

    build_report(all_params, args.output_dir)


if __name__ == "__main__":
    main()
