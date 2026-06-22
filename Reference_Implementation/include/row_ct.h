/**
 * Row-level constant-time helpers for leakage-aware RREF experiments.
 */

#pragma once

#include "fq_ct.h"

#if defined(LESS_RREF_ENABLE_NEON_ROW_FMA) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
#include <arm_neon.h>
#endif

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

static inline void ct_cond_fma_row_level_a(FQ_ELEM *dst,
                                           const FQ_ELEM *src,
                                           const FQ_ELEM factor,
                                           const ct_mask_t do_reduce) {
#if defined(LESS_RREF_ENABLE_NEON_ROW_FMA) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
    const uint8x16_t factor_v = vdupq_n_u8(factor);
    const uint16x8_t q_v = vdupq_n_u16(Q);
    const uint16x8_t q_mask_v = vdupq_n_u16(Q);
    const uint8x16_t reduce_mask_v = vdupq_n_u8((uint8_t)do_reduce);
    uint32_t c = 0;

    for (; c + 16 <= N; c += 16) {
        const uint8x16_t src_v = vld1q_u8(src + c);
        const uint8x16_t dst_v = vld1q_u8(dst + c);

        const uint16x8_t prod_lo =
            vmull_u8(vget_low_u8(src_v), vget_low_u8(factor_v));
        const uint16x8_t prod_hi =
            vmull_u8(vget_high_u8(src_v), vget_high_u8(factor_v));

        uint16x8_t red_lo = vaddq_u16(vshrq_n_u16(prod_lo, 7),
                                      vandq_u16(prod_lo, q_mask_v));
        uint16x8_t red_hi = vaddq_u16(vshrq_n_u16(prod_hi, 7),
                                      vandq_u16(prod_hi, q_mask_v));

        red_lo = vbslq_u16(vcgeq_u16(red_lo, q_v), vsubq_u16(red_lo, q_v), red_lo);
        red_hi = vbslq_u16(vcgeq_u16(red_hi, q_v), vsubq_u16(red_hi, q_v), red_hi);

        const uint8x16_t product_v = vcombine_u8(vmovn_u16(red_lo),
                                                 vmovn_u16(red_hi));

        uint16x8_t sub_lo =
            vsubq_u16(vaddq_u16(vmovl_u8(vget_low_u8(dst_v)), q_v),
                      vmovl_u8(vget_low_u8(product_v)));
        uint16x8_t sub_hi =
            vsubq_u16(vaddq_u16(vmovl_u8(vget_high_u8(dst_v)), q_v),
                      vmovl_u8(vget_high_u8(product_v)));

        sub_lo = vbslq_u16(vcgeq_u16(sub_lo, q_v), vsubq_u16(sub_lo, q_v), sub_lo);
        sub_hi = vbslq_u16(vcgeq_u16(sub_hi, q_v), vsubq_u16(sub_hi, q_v), sub_hi);

        const uint8x16_t reduced_v = vcombine_u8(vmovn_u16(sub_lo),
                                                 vmovn_u16(sub_hi));
        const uint8x16_t result_v = vbslq_u8(reduce_mask_v, reduced_v, dst_v);
        vst1q_u8(dst + c, result_v);
    }

    for (; c < N; c++) {
        const FQ_ELEM product = fq_mul_ct(factor, src[c]);
        const FQ_ELEM reduced = fq_sub_ct(dst[c], product);
        dst[c] = fq_select_ct(do_reduce, reduced, dst[c]);
    }
#else
    for (uint32_t c = 0; c < N; c++) {
        const FQ_ELEM product = fq_mul_ct(factor, src[c]);
        const FQ_ELEM reduced = fq_sub_ct(dst[c], product);
        dst[c] = fq_select_ct(do_reduce, reduced, dst[c]);
    }
#endif
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
