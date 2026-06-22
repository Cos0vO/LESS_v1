#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "codes.h"
#include "monomial_mat.h"
#include "rng.h"

#include "test_helpers.c"

#ifndef RREF_WALL_ITERS
#define RREF_WALL_ITERS 24u
#endif

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ull) + (uint64_t)ts.tv_nsec;
}

static void generator_copy(generator_mat_t *dst, const generator_mat_t *src) {
    memcpy(dst, src, sizeof(*dst));
}

static void generator_rnd_fullrank(generator_mat_t *G, uint8_t is_pivot_column[N]) {
    do {
        generator_rnd(G);
        memset(is_pivot_column, 0, N);
    } while (generator_RREF(G, is_pivot_column) == 0);
}

int main(void) {
    generator_mat_t base = {0};
    generator_mat_t input = {0};
    generator_mat_t strict = {0};
    generator_mat_t fast = {0};
    uint8_t scratch_pivots[N] = {0};
    uint8_t strict_pivots[N] = {0};
    uint8_t fast_pivots[N] = {0};
    monomial_t q;
    uint64_t strict_ns = 0;
    uint64_t fast_ns = 0;
    uint64_t strict_acc = 0;
    uint64_t fast_acc = 0;

    init_randombytes((const unsigned char *)"rref_wall", 9);
    generator_rnd_fullrank(&base, scratch_pivots);

    for (uint32_t i = 0; i < RREF_WALL_ITERS; i++) {
        monomial_mat_rnd(&q);
        generator_monomial_mul(&input, &base, &q);

        generator_copy(&strict, &input);
        generator_copy(&fast, &input);
        memset(strict_pivots, 0, sizeof(strict_pivots));
        memset(fast_pivots, 0, sizeof(fast_pivots));

        uint64_t start = now_ns();
        const int strict_ret = generator_RREF_ct(&strict, strict_pivots);
        strict_ns += now_ns() - start;

        start = now_ns();
        const int fast_ret = generator_RREF_ct_level_a_fast(&fast, fast_pivots);
        fast_ns += now_ns() - start;

        strict_acc += (uint64_t)strict_ret + strict.values[0][0];
        fast_acc += (uint64_t)fast_ret + fast.values[0][0];

        if (strict_ret != fast_ret ||
            memcmp(&strict, &fast, sizeof(generator_mat_t)) != 0 ||
            memcmp(strict_pivots, fast_pivots, N) != 0) {
            fprintf(stderr, "RREF wall benchmark equivalence failed at iteration %" PRIu32 "\n", i);
            return 1;
        }
    }

    const double strict_ms = (double)strict_ns / (1000000.0 * (double)RREF_WALL_ITERS);
    const double fast_ms = (double)fast_ns / (1000000.0 * (double)RREF_WALL_ITERS);
    printf("rref wall benchmark category=%d target=%d iterations=%u\n", CATEGORY, TARGET, RREF_WALL_ITERS);
    printf("strict-ct:     %.3f ms\n", strict_ms);
    printf("level-a-fast:  %.3f ms\n", fast_ms);
    printf("fast/strict:   %.3f\n", fast_ms / strict_ms);
    printf("speedup:       %.3fx\n", strict_ms / fast_ms);
    printf("accumulators:  strict=%" PRIu64 " fast=%" PRIu64 "\n", strict_acc, fast_acc);

    return 0;
}
