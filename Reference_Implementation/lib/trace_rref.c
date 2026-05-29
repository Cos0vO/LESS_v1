#include "trace_rref.h"

#ifdef LESS_TRACE_RREF

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    int active;
    int initialized;
    int enabled;
    int verify_sequence;
    int round_index;
    int round_limit;
    int round_only;
    uint8_t fixed_weight_value;
    const char *branch_label;
    const char *dependency_class;
    FILE *fp;
} less_trace_state_t;

static less_trace_state_t g_trace_state = {
    .active = 0,
    .initialized = 0,
    .enabled = 0,
    .verify_sequence = 0,
    .round_index = -1,
    .round_limit = -1,
    .round_only = -1,
    .fixed_weight_value = 0,
    .branch_label = "uninitialized",
    .dependency_class = "unclassified",
    .fp = NULL
};

void less_trace_reset_runtime_config(void) {
    if (g_trace_state.fp != NULL) {
        fclose(g_trace_state.fp);
    }

    g_trace_state.active = 0;
    g_trace_state.initialized = 0;
    g_trace_state.enabled = 0;
    g_trace_state.verify_sequence = 0;
    g_trace_state.round_index = -1;
    g_trace_state.round_limit = -1;
    g_trace_state.round_only = -1;
    g_trace_state.fixed_weight_value = 0;
    g_trace_state.branch_label = "uninitialized";
    g_trace_state.dependency_class = "unclassified";
    g_trace_state.fp = NULL;
}

static void less_trace_init_if_needed(void) {
    if (g_trace_state.initialized) {
        return;
    }
    g_trace_state.initialized = 1;

    const char *trace_path = getenv("LESS_TRACE_RREF_FILE");
    if ((trace_path == NULL) || (trace_path[0] == '\0')) {
        return;
    }

    g_trace_state.fp = fopen(trace_path, "a");
    if (g_trace_state.fp == NULL) {
        return;
    }

    const char *round_limit = getenv("LESS_TRACE_RREF_ROUND_LIMIT");
    if ((round_limit != NULL) && (round_limit[0] != '\0')) {
        g_trace_state.round_limit = atoi(round_limit);
    }

    const char *round_only = getenv("LESS_TRACE_RREF_ROUND_ONLY");
    if ((round_only != NULL) && (round_only[0] != '\0')) {
        g_trace_state.round_only = atoi(round_only);
    }

    g_trace_state.enabled = 1;
}

static int less_trace_should_emit_control(void) {
    if ((!g_trace_state.enabled) || (!g_trace_state.active) || (g_trace_state.fp == NULL)) {
        return 0;
    }

    return 1;
}

static int less_trace_should_emit_data(void) {
    if (!less_trace_should_emit_control()) {
        return 0;
    }

    if ((g_trace_state.round_limit >= 0) && (g_trace_state.round_index >= 0) &&
        (g_trace_state.round_index >= g_trace_state.round_limit)) {
        return 0;
    }

    if ((g_trace_state.round_only >= 0) && (g_trace_state.round_index != g_trace_state.round_only)) {
        return 0;
    }

    return 1;
}

static void less_trace_write_json_string(FILE *fp, const char *value) {
    if (value == NULL) {
        fputs("null", fp);
        return;
    }

    fputc('"', fp);
    for (const unsigned char *p = (const unsigned char *)value; *p != '\0'; p++) {
        switch (*p) {
            case '\\':
            case '"':
                fputc('\\', fp);
                fputc(*p, fp);
                break;
            case '\n':
                fputs("\\n", fp);
                break;
            case '\r':
                fputs("\\r", fp);
                break;
            case '\t':
                fputs("\\t", fp);
                break;
            default:
                if (*p < 0x20) {
                    fprintf(fp, "\\u%04x", *p);
                } else {
                    fputc(*p, fp);
                }
                break;
        }
    }
    fputc('"', fp);
}

static void less_trace_write_common_prefix(FILE *fp,
                                           const char *record_type,
                                           const char *stage_id,
                                           const char *callsite,
                                           const char *matrix_name,
                                           uint32_t rows,
                                           uint32_t cols,
                                           const void *base_address,
                                           size_t byte_size,
                                           const char *dependency_class,
                                           uint64_t duration_ns) {
    fprintf(fp, "{\"record_type\":");
    less_trace_write_json_string(fp, record_type);
    fprintf(fp, ",\"verify_sequence\":%d", g_trace_state.verify_sequence);
    fprintf(fp, ",\"round_index\":%d", g_trace_state.round_index);
    fprintf(fp, ",\"fixed_weight_value\":%u", g_trace_state.fixed_weight_value);
    fprintf(fp, ",\"stage_id\":");
    less_trace_write_json_string(fp, stage_id);
    fprintf(fp, ",\"callsite\":");
    less_trace_write_json_string(fp, callsite);
    fprintf(fp, ",\"branch_label\":");
    less_trace_write_json_string(fp, g_trace_state.branch_label);
    fprintf(fp, ",\"matrix_name\":");
    less_trace_write_json_string(fp, matrix_name);
    fprintf(fp, ",\"rows\":%u", rows);
    fprintf(fp, ",\"cols\":%u", cols);
    fprintf(fp, ",\"base_address\":");
    if (base_address == NULL) {
        fputs("null", fp);
    } else {
        char address_buf[32];
        snprintf(address_buf, sizeof(address_buf), "0x%" PRIxPTR, (uintptr_t)base_address);
        less_trace_write_json_string(fp, address_buf);
    }
    fprintf(fp, ",\"byte_size\":%zu", byte_size);
    fprintf(fp, ",\"row_stride_bytes\":%u", N);
    fprintf(fp, ",\"secret_dependency_class\":");
    less_trace_write_json_string(fp,
                                 (dependency_class != NULL) ? dependency_class :
                                 g_trace_state.dependency_class);
    fprintf(fp, ",\"duration_ns\":%" PRIu64, duration_ns);
}

uint64_t less_trace_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

void less_trace_verify_begin(void) {
    less_trace_init_if_needed();
    if (!g_trace_state.enabled) {
        return;
    }

    g_trace_state.active = 1;
    g_trace_state.verify_sequence += 1;
    g_trace_state.round_index = -1;
    g_trace_state.fixed_weight_value = 0;
    g_trace_state.branch_label = "setup";
    g_trace_state.dependency_class = "public-only";

    if (!less_trace_should_emit_control()) {
        return;
    }

    FILE *fp = g_trace_state.fp;
    less_trace_write_common_prefix(fp,
                                   "control",
                                   "verify_begin",
                                   "LESS_verify",
                                   NULL,
                                   0,
                                   0,
                                   NULL,
                                   0,
                                   "public-only",
                                   0);
    fputs(",\"pivot_flags\":null,\"row_major_values\":null,\"events\":[{\"kind\":\"verify_begin\"}]}\n", fp);
    fflush(fp);
}

void less_trace_verify_end(int verify_result, uint64_t duration_ns) {
    if (!g_trace_state.enabled) {
        return;
    }

    if (g_trace_state.active && (g_trace_state.fp != NULL)) {
        FILE *fp = g_trace_state.fp;
        const int saved_round_index = g_trace_state.round_index;
        const uint8_t saved_fixed_weight = g_trace_state.fixed_weight_value;
        const char *saved_branch_label = g_trace_state.branch_label;
        const char *saved_dependency_class = g_trace_state.dependency_class;
        g_trace_state.round_index = -1;
        g_trace_state.fixed_weight_value = 0;
        g_trace_state.branch_label = "summary";
        g_trace_state.dependency_class = "data-dependent execution";
        less_trace_write_common_prefix(fp,
                                       "control",
                                       "verify_end",
                                       "LESS_verify",
                                       NULL,
                                       0,
                                       0,
                                       NULL,
                                       0,
                                       "data-dependent execution",
                                       duration_ns);
        fprintf(fp,
                ",\"pivot_flags\":null,\"row_major_values\":null,\"events\":[{\"kind\":\"verify_end\",\"verify_result\":%d}]}\n",
                verify_result);
        fflush(fp);
        g_trace_state.round_index = saved_round_index;
        g_trace_state.fixed_weight_value = saved_fixed_weight;
        g_trace_state.branch_label = saved_branch_label;
        g_trace_state.dependency_class = saved_dependency_class;
    }

    g_trace_state.active = 0;
}

void less_trace_set_round_context(int round_index,
                                  uint8_t fixed_weight_value,
                                  const char *branch_label,
                                  const char *dependency_class) {
    if (!g_trace_state.enabled) {
        return;
    }

    g_trace_state.round_index = round_index;
    g_trace_state.fixed_weight_value = fixed_weight_value;
    g_trace_state.branch_label = branch_label;
    g_trace_state.dependency_class = dependency_class;
}

void less_trace_snapshot_generator(const char *stage_id,
                                   const char *callsite,
                                   const char *matrix_name,
                                   const generator_mat_t *G,
                                   const uint8_t *pivot_flags,
                                   const char *dependency_class,
                                   uint64_t duration_ns) {
    if ((!less_trace_should_emit_data()) || (G == NULL)) {
        return;
    }

    FILE *fp = g_trace_state.fp;
    const FQ_ELEM *flat = &G->values[0][0];
    less_trace_write_common_prefix(fp,
                                   "snapshot",
                                   stage_id,
                                   callsite,
                                   matrix_name,
                                   K,
                                   N,
                                   flat,
                                   (size_t)K * (size_t)N,
                                   dependency_class,
                                   duration_ns);

    fputs(",\"pivot_flags\":", fp);
    if (pivot_flags == NULL) {
        fputs("null", fp);
    } else {
        fputc('[', fp);
        for (uint32_t col_idx = 0; col_idx < N; col_idx++) {
            if (col_idx != 0) {
                fputc(',', fp);
            }
            fprintf(fp, "%u", pivot_flags[col_idx]);
        }
        fputc(']', fp);
    }

    fputs(",\"row_major_values\":[", fp);
    for (uint32_t row_idx = 0; row_idx < K; row_idx++) {
        for (uint32_t col_idx = 0; col_idx < N; col_idx++) {
            if ((row_idx != 0) || (col_idx != 0)) {
                fputc(',', fp);
            }
            fprintf(fp, "%u", G->values[row_idx][col_idx]);
        }
    }
    fputs("],\"events\":[]}\n", fp);
    fflush(fp);
}

void less_trace_rref_event(const char *stage_id,
                           const char *callsite,
                           const char *dependency_class,
                           uint32_t row_to_reduce,
                           uint32_t pivot_row,
                           uint32_t pivot_column,
                           int found_pivot,
                           int did_row_swap,
                           int reuse_hit,
                           uint64_t duration_ns) {
    if (!less_trace_should_emit_data()) {
        return;
    }

    FILE *fp = g_trace_state.fp;
    less_trace_write_common_prefix(fp,
                                   "event",
                                   stage_id,
                                   callsite,
                                   NULL,
                                   0,
                                   0,
                                   NULL,
                                   0,
                                   dependency_class,
                                   duration_ns);
    fprintf(fp,
            ",\"pivot_flags\":null,\"row_major_values\":null,\"events\":[{\"kind\":\"rref_step\",\"row_to_reduce\":%u,\"pivot_row\":%u,\"pivot_column\":%u,\"found_pivot\":%d,\"did_row_swap\":%d,\"reuse_hit\":%d}]}\n",
            row_to_reduce,
            pivot_row,
            pivot_column,
            found_pivot,
            did_row_swap,
            reuse_hit);
    fflush(fp);
}

#endif
