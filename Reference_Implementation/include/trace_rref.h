#pragma once

#include <stdint.h>

#include "codes.h"

#ifdef LESS_TRACE_RREF

uint64_t less_trace_now_ns(void);
void less_trace_reset_runtime_config(void);

void less_trace_verify_begin(void);
void less_trace_verify_end(int verify_result, uint64_t duration_ns);

void less_trace_set_round_context(int round_index,
                                  uint8_t fixed_weight_value,
                                  const char *branch_label,
                                  const char *dependency_class);

void less_trace_snapshot_generator(const char *stage_id,
                                   const char *callsite,
                                   const char *matrix_name,
                                   const generator_mat_t *G,
                                   const uint8_t *pivot_flags,
                                   const char *dependency_class,
                                   uint64_t duration_ns);

void less_trace_rref_event(const char *stage_id,
                           const char *callsite,
                           const char *dependency_class,
                           uint32_t row_to_reduce,
                           uint32_t pivot_row,
                           uint32_t pivot_column,
                           int found_pivot,
                           int did_row_swap,
                           int reuse_hit,
                           uint64_t duration_ns);

#else

static inline uint64_t less_trace_now_ns(void) {
    return 0;
}

static inline void less_trace_reset_runtime_config(void) {
}

static inline void less_trace_verify_begin(void) {
}

static inline void less_trace_verify_end(int verify_result, uint64_t duration_ns) {
    (void)verify_result;
    (void)duration_ns;
}

static inline void less_trace_set_round_context(int round_index,
                                                uint8_t fixed_weight_value,
                                                const char *branch_label,
                                                const char *dependency_class) {
    (void)round_index;
    (void)fixed_weight_value;
    (void)branch_label;
    (void)dependency_class;
}

static inline void less_trace_snapshot_generator(const char *stage_id,
                                                 const char *callsite,
                                                 const char *matrix_name,
                                                 const generator_mat_t *G,
                                                 const uint8_t *pivot_flags,
                                                 const char *dependency_class,
                                                 uint64_t duration_ns) {
    (void)stage_id;
    (void)callsite;
    (void)matrix_name;
    (void)G;
    (void)pivot_flags;
    (void)dependency_class;
    (void)duration_ns;
}

static inline void less_trace_rref_event(const char *stage_id,
                                         const char *callsite,
                                         const char *dependency_class,
                                         uint32_t row_to_reduce,
                                         uint32_t pivot_row,
                                         uint32_t pivot_column,
                                         int found_pivot,
                                         int did_row_swap,
                                         int reuse_hit,
                                         uint64_t duration_ns) {
    (void)stage_id;
    (void)callsite;
    (void)dependency_class;
    (void)row_to_reduce;
    (void)pivot_row;
    (void)pivot_column;
    (void)found_pivot;
    (void)did_row_swap;
    (void)reuse_hit;
    (void)duration_ns;
}

#endif
