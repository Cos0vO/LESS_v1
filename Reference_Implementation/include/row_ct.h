/**
 * Row-level constant-time helpers for leakage-aware RREF experiments.
 */

#pragma once

#include "fq_ct.h"

static inline void ct_cond_swap_cell(FQ_ELEM *a,
                                     FQ_ELEM *b,
                                     const ct_mask_t do_swap) {
    const FQ_ELEM x = *a;
    const FQ_ELEM y = *b;
    *a = fq_select_ct(do_swap, y, x);
    *b = fq_select_ct(do_swap, x, y);
}

static inline void ct_cond_fma_row(FQ_ELEM dst[N_pad],
                                   const FQ_ELEM src[N_pad],
                                   const FQ_ELEM factor,
                                   const ct_mask_t do_reduce) {
    for (uint32_t c = 0; c < N; c++) {
        const FQ_ELEM product = fq_mul_ct(factor, src[c]);
        const FQ_ELEM reduced = fq_sub_ct(dst[c], product);
        dst[c] = fq_select_ct(do_reduce, reduced, dst[c]);
    }
}

static inline void ct_row_select_by_mask(FQ_ELEM out[N_pad],
                                         const FQ_ELEM G[K][N_pad],
                                         const ct_mask_t row_mask[K]) {
    for (uint32_t c = 0; c < N; c++) {
        FQ_ELEM acc = 0;
        for (uint32_t r = 0; r < K; r++) {
            acc = fq_select_ct(row_mask[r], G[r][c], acc);
        }
        out[c] = acc;
    }
}

static inline void ct_row_write_by_mask(FQ_ELEM G[K][N_pad],
                                        const FQ_ELEM new_row[N_pad],
                                        const ct_mask_t row_mask[K]) {
    for (uint32_t r = 0; r < K; r++) {
        for (uint32_t c = 0; c < N; c++) {
            G[r][c] = fq_select_ct(row_mask[r], new_row[c], G[r][c]);
        }
    }
}
