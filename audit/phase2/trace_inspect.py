#!/usr/bin/env python3

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from statistics import mean


def load_records(path: Path):
    return [json.loads(line) for line in path.read_text().splitlines() if line.strip()]


def main():
    parser = argparse.ArgumentParser(description="Inspect LESS Phase 2 JSONL traces.")
    parser.add_argument("trace", type=Path, help="Path to the JSONL trace file")
    args = parser.parse_args()

    records = load_records(args.trace)
    stage_counts = Counter(record["stage_id"] for record in records)
    record_type_counts = Counter(record["record_type"] for record in records)
    verify_sequences = sorted({record["verify_sequence"] for record in records})

    snapshot_length_mismatches = []
    for idx, record in enumerate(records):
        row_major = record.get("row_major_values")
        if record.get("record_type") != "snapshot" or row_major is None:
            continue
        expected = record["rows"] * record["cols"]
        if len(row_major) != expected:
            snapshot_length_mismatches.append(
                {
                    "record_index": idx,
                    "stage_id": record["stage_id"],
                    "expected": expected,
                    "actual": len(row_major),
                }
            )

    entry_records = defaultdict(list)
    exit_records = defaultdict(list)
    for record in records:
        key = (
            record["verify_sequence"],
            record["round_index"],
            record["matrix_name"],
            record["callsite"],
        )
        if record["stage_id"] == "g_prime_rref_entry":
            entry_records[key].append(record)
        elif record["stage_id"] == "g_prime_rref_exit":
            exit_records[key].append(record)

    base_address_checks = []
    for key, entries in entry_records.items():
        exits = exit_records.get(key, [])
        for idx, entry in enumerate(entries):
            if idx >= len(exits):
                base_address_checks.append(
                    {
                        "key": key,
                        "matched": False,
                        "reason": "missing_exit",
                    }
                )
                continue

            exit_record = exits[idx]
            base_address_checks.append(
                {
                    "key": key,
                    "matched": entry["base_address"] == exit_record["base_address"],
                    "entry_base_address": entry["base_address"],
                    "exit_base_address": exit_record["base_address"],
                }
            )

    rref_event_stage_ids = {"generator_rref_pivot_reuse_step", "generator_rref_step"}
    rref_events = [
        record for record in records
        if record["record_type"] == "event" and record["stage_id"] in rref_event_stage_ids
    ]
    rref_event_summary = {}
    if rref_events:
        step_durations = [record["duration_ns"] for record in rref_events]
        rref_event_summary = {
            "event_stage_counts": dict(Counter(record["stage_id"] for record in rref_events)),
            "step_count": len(rref_events),
            "reuse_hit_count": sum(record["events"][0]["reuse_hit"] for record in rref_events),
            "row_swap_count": sum(record["events"][0]["did_row_swap"] for record in rref_events),
            "duration_ns_min": min(step_durations),
            "duration_ns_max": max(step_durations),
            "duration_ns_mean": int(mean(step_durations)),
        }

    snapshot_durations = {}
    for record in records:
        if record["record_type"] != "snapshot":
            continue
        snapshot_durations[record["stage_id"]] = record["duration_ns"]

    output = {
        "trace_path": str(args.trace),
        "record_count": len(records),
        "record_type_counts": dict(record_type_counts),
        "stage_counts": dict(stage_counts),
        "verify_sequences": verify_sequences,
        "verify_sequence_count": len(verify_sequences),
        "snapshot_length_mismatches": snapshot_length_mismatches,
        "all_snapshot_lengths_valid": len(snapshot_length_mismatches) == 0,
        "rref_entry_exit_base_address_checks": base_address_checks,
        "all_rref_entry_exit_base_addresses_stable": all(
            check.get("matched", False) for check in base_address_checks
        ),
        "rref_event_summary": rref_event_summary,
        "snapshot_durations_ns": snapshot_durations,
        "first_stage": records[0]["stage_id"] if records else None,
        "last_stage": records[-1]["stage_id"] if records else None,
    }

    print(json.dumps(output, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
