/**
 * Constant-time finite-field wrappers for leakage-aware RREF experiments.
 */

#pragma once

#include "ct.h"
#include "fq_arith.h"

static inline FQ_ELEM fq_add_ct(const FQ_ELEM a, const FQ_ELEM b) {
    return fq_add(a, b);
}

static inline FQ_ELEM fq_sub_ct(const FQ_ELEM a, const FQ_ELEM b) {
    return fq_sub(a, b);
}

static inline FQ_ELEM fq_mul_ct(const FQ_ELEM a, const FQ_ELEM b) {
    return fq_mul(a, b);
}

static inline FQ_ELEM fq_neg_ct(const FQ_ELEM a) {
    return fq_sub(0, a);
}

static inline ct_mask_t fq_iszero_ct(const FQ_ELEM a) {
    return ct_is_zero_u32((uint32_t)a);
}

static inline ct_mask_t fq_isnonzero_ct(const FQ_ELEM a) {
    return ct_is_nonzero_u32((uint32_t)a);
}

static inline FQ_ELEM fq_select_ct(const ct_mask_t m,
                                   const FQ_ELEM a,
                                   const FQ_ELEM b) {
    return ct_select_u8(m, a, b);
}

static inline FQ_ELEM fq_inv_ct_safe(const FQ_ELEM a,
                                     const ct_mask_t nonzero) {
    const FQ_ELEM safe_a = fq_select_ct(nonzero, a, 1);
    const FQ_ELEM inv = fq_inv(safe_a);
    return fq_select_ct(nonzero, inv, 0);
}
