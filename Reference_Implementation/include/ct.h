/**
 * Constant-time scalar helpers for leakage-aware RREF experiments.
 *
 * These helpers use all-ones/all-zeroes masks. They are intentionally small and
 * header-only so the existing Reference_Implementation build can use them
 * without changing the build graph.
 */

#pragma once

#include <stdint.h>

typedef uint32_t ct_mask_t;

#define CT_TRUE  ((ct_mask_t)0xffffffffu)
#define CT_FALSE ((ct_mask_t)0x00000000u)

static inline ct_mask_t ct_mask_from_bit_u32(const uint32_t bit) {
    return (ct_mask_t)(0u - (bit & 1u));
}

static inline ct_mask_t ct_is_zero_u32(const uint32_t x) {
    return ct_mask_from_bit_u32(((x | (0u - x)) >> 31) ^ 1u);
}

static inline ct_mask_t ct_is_nonzero_u32(const uint32_t x) {
    return ct_mask_from_bit_u32((x | (0u - x)) >> 31);
}

static inline ct_mask_t ct_eq_u32(const uint32_t a, const uint32_t b) {
    return ct_is_zero_u32(a ^ b);
}

static inline ct_mask_t ct_lt_u32(const uint32_t a, const uint32_t b) {
    return ct_mask_from_bit_u32(a < b);
}

static inline ct_mask_t ct_ge_u32(const uint32_t a, const uint32_t b) {
    return (ct_mask_t)~ct_lt_u32(a, b);
}

static inline uint32_t ct_select_u32(const ct_mask_t m,
                                     const uint32_t a,
                                     const uint32_t b) {
    return (m & a) | (~m & b);
}

static inline uint8_t ct_select_u8(const ct_mask_t m,
                                   const uint8_t a,
                                   const uint8_t b) {
    return (uint8_t)((m & a) | (~m & b));
}
