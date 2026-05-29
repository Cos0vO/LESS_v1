#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "LESS.h"
#include "codes.h"
#include "rng.h"
#include "trace_rref.h"
#include "utils.h"

#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x)

static const char k_parameter_tag[] = STRINGIFY(CATEGORY) "_" STRINGIFY(TARGET);

#ifdef LESS_TRACE_RREF
static const char k_sample_kind[] = "trace";
#else
static const char k_sample_kind[] = "wall";
#endif

typedef struct {
    uint64_t state;
} splitmix64_t;

typedef struct {
    uint32_t target_round;
    uint32_t sf_g_index;
    uint32_t source_col;
    uint32_t target_output_col;
} target_info_t;

typedef struct {
    int bases;
    int reps;
    uint64_t seed;
    uint64_t message_len;
    const char *format;
} cli_config_t;

typedef struct {
    pubkey_t legal;
    pubkey_t wrong;
    pubkey_t correct;
} pk_variants_t;

static uint64_t phase3_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t) ts.tv_sec * 1000000000ULL) + (uint64_t) ts.tv_nsec;
}

static uint64_t splitmix64_next(splitmix64_t *rng) {
    uint64_t z;
    rng->state += 0x9e3779b97f4a7c15ULL;
    z = rng->state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static void fill_seed_material(uint64_t root_seed, unsigned char out[48]) {
    splitmix64_t rng = {.state = root_seed ^ 0x4f5241434c45534fULL};
    for (size_t idx = 0; idx < 48; idx += sizeof(uint64_t)) {
        const uint64_t value = splitmix64_next(&rng);
        memcpy(out + idx, &value, sizeof(uint64_t));
    }
}

static void fill_message(uint64_t root_seed, unsigned char *message, uint64_t message_len) {
    splitmix64_t rng = {.state = root_seed ^ 0x4d45535341474531ULL};
    uint64_t offset = 0;
    while (offset < message_len) {
        const uint64_t value = splitmix64_next(&rng);
        const uint64_t chunk = ((message_len - offset) < sizeof(uint64_t))
                               ? (message_len - offset)
                               : (uint64_t) sizeof(uint64_t);
        memcpy(message + offset, &value, (size_t) chunk);
        offset += chunk;
    }
}

static uint8_t draw_fq(splitmix64_t *rng) {
    return (uint8_t) (splitmix64_next(rng) % Q);
}

static splitmix64_t make_base_rng(uint64_t root_seed, uint32_t base_index) {
    splitmix64_t rng = {
        .state = root_seed ^ 0x42415345434f4c31ULL ^ (((uint64_t) base_index) * 0x100000001b3ULL)
    };
    (void) splitmix64_next(&rng);
    return rng;
}

static splitmix64_t make_shuffle_rng(uint64_t root_seed, uint32_t base_index, uint32_t rep_index) {
    splitmix64_t rng = {
        .state = root_seed ^ 0x53485546464c4531ULL ^
                 (((uint64_t) base_index) << 32) ^ (uint64_t) rep_index
    };
    (void) splitmix64_next(&rng);
    return rng;
}

static void shuffle_variants(splitmix64_t *rng, int order[3]) {
    order[0] = 0;
    order[1] = 1;
    order[2] = 2;
    for (int idx = 2; idx > 0; idx--) {
        const int swap_idx = (int) (splitmix64_next(rng) % (uint64_t) (idx + 1));
        const int tmp = order[idx];
        order[idx] = order[swap_idx];
        order[swap_idx] = tmp;
    }
}

static int parse_cli(int argc, char **argv, cli_config_t *cfg) {
    *cfg = (cli_config_t) {
        .bases = 20,
        .reps = 10,
        .seed = 0x5eed20260528ULL,
        .message_len = 80,
        .format = "ndjson"
    };

    for (int idx = 1; idx < argc; idx++) {
        if ((strcmp(argv[idx], "--bases") == 0) && (idx + 1 < argc)) {
            cfg->bases = atoi(argv[++idx]);
        } else if ((strcmp(argv[idx], "--reps") == 0) && (idx + 1 < argc)) {
            cfg->reps = atoi(argv[++idx]);
        } else if ((strcmp(argv[idx], "--seed") == 0) && (idx + 1 < argc)) {
            cfg->seed = strtoull(argv[++idx], NULL, 0);
        } else if ((strcmp(argv[idx], "--message-len") == 0) && (idx + 1 < argc)) {
            cfg->message_len = strtoull(argv[++idx], NULL, 0);
        } else if ((strcmp(argv[idx], "--format") == 0) && (idx + 1 < argc)) {
            cfg->format = argv[++idx];
        } else {
            fprintf(stderr,
                    "usage: %s [--bases N] [--reps N] [--seed U64] [--message-len N] [--format ndjson]\n",
                    argv[0]);
            return 0;
        }
    }

    if ((cfg->bases <= 0) || (cfg->reps <= 0) || (cfg->message_len == 0) ||
        (strcmp(cfg->format, "ndjson") != 0)) {
        fprintf(stderr, "invalid Phase 3 arguments\n");
        return 0;
    }

    return 1;
}

static int find_target_info(const pubkey_t *pk,
                            const sign_t *sig,
                            target_info_t *target,
                            generator_mat_t *decoded_matrix,
                            uint8_t pivot_flags[N]) {
    uint8_t fixed_weight_string[T] = {0};
    SampleChallenge(fixed_weight_string, sig->digest);

    target->target_round = UINT32_MAX;
    for (uint32_t round = 0; round < T; round++) {
        if (fixed_weight_string[round] != 0) {
            target->target_round = round;
            target->sf_g_index = (uint32_t) fixed_weight_string[round] - 1u;
            break;
        }
    }

    if (target->target_round == UINT32_MAX) {
        return 0;
    }

    expand_to_rref(decoded_matrix, pk->SF_G[target->sf_g_index], pivot_flags);

    uint32_t left_index = 0;
    uint32_t right_index = 0;
    for (uint32_t src_col = 0; src_col < N; src_col++) {
        const uint8_t bit = (sig->cf_monom_actions[0][src_col / 8u] >> (src_col % 8u)) & 1u;
        uint32_t dst_col;
        if (bit) {
            dst_col = left_index;
            left_index++;
        } else {
            dst_col = K + right_index;
            right_index++;
        }

        if (bit && (pivot_flags[src_col] == 0u)) {
            target->source_col = src_col;
            target->target_output_col = dst_col;
            return 1;
        }
    }

    return 0;
}

static void randomize_nonpivot_columns(generator_mat_t *dst,
                                       const generator_mat_t *legal_matrix,
                                       const uint8_t pivot_flags[N],
                                       splitmix64_t *rng) {
    memcpy(dst, legal_matrix, sizeof(*dst));
    for (uint32_t col = 0; col < N; col++) {
        if (pivot_flags[col] != 0u) {
            continue;
        }
        for (uint32_t row = 0; row < K; row++) {
            dst->values[row][col] = draw_fq(rng);
        }
    }
}

static int columns_equal(const generator_mat_t *lhs,
                         const generator_mat_t *rhs,
                         uint32_t column_index) {
    for (uint32_t row = 0; row < K; row++) {
        if (lhs->values[row][column_index] != rhs->values[row][column_index]) {
            return 0;
        }
    }
    return 1;
}

static void copy_column(generator_mat_t *dst,
                        const generator_mat_t *src,
                        uint32_t column_index) {
    for (uint32_t row = 0; row < K; row++) {
        dst->values[row][column_index] = src->values[row][column_index];
    }
}

static void ensure_wrong_column(generator_mat_t *wrong_matrix,
                                const generator_mat_t *legal_matrix,
                                uint32_t column_index) {
    if (!columns_equal(wrong_matrix, legal_matrix, column_index)) {
        return;
    }

    wrong_matrix->values[0][column_index] =
            (uint8_t) ((legal_matrix->values[0][column_index] + 1u) % Q);
}

static void build_variant_public_keys(const pubkey_t *legal_pk,
                                      const generator_mat_t *legal_matrix,
                                      const uint8_t pivot_flags[N],
                                      const target_info_t *target,
                                      uint64_t root_seed,
                                      uint32_t base_index,
                                      pk_variants_t *variants) {
    generator_mat_t randomized_matrix;
    generator_mat_t wrong_matrix;
    generator_mat_t correct_matrix;
    splitmix64_t rng = make_base_rng(root_seed, base_index);

    variants->legal = *legal_pk;
    variants->wrong = *legal_pk;
    variants->correct = *legal_pk;

    randomize_nonpivot_columns(&randomized_matrix, legal_matrix, pivot_flags, &rng);
    wrong_matrix = randomized_matrix;
    correct_matrix = randomized_matrix;

    ensure_wrong_column(&wrong_matrix, legal_matrix, target->source_col);
    copy_column(&correct_matrix, legal_matrix, target->source_col);

    compress_rref(variants->wrong.SF_G[target->sf_g_index], &wrong_matrix, pivot_flags);
    compress_rref(variants->correct.SF_G[target->sf_g_index], &correct_matrix, pivot_flags);
}

static int run_verify_sample(const char *variant_name,
                             const pubkey_t *pk,
                             const unsigned char *message,
                             uint64_t message_len,
                             const sign_t *sig,
                             const target_info_t *target,
                             uint32_t base_index,
                             uint32_t rep_index,
                             uint32_t trace_sequence_index) {
    const uint64_t start_ns = phase3_now_ns();
    const int verify_result = LESS_verify(pk, (const char *) message, message_len, sig);
    const uint64_t duration_ns = phase3_now_ns() - start_ns;

    printf("{\"parameter_tag\":\"%s\",\"sample_kind\":\"%s\",\"variant\":\"%s\","
           "\"base_index\":%u,\"rep_index\":%u,\"verify_result\":%d,"
           "\"duration_ns\":%" PRIu64 ",\"target_round\":%u,\"sf_g_index\":%u,"
           "\"source_col\":%u,\"target_output_col\":%u,\"trace_sequence_index\":%u}\n",
           k_parameter_tag,
           k_sample_kind,
           variant_name,
           base_index,
           rep_index,
           verify_result,
           duration_ns,
           target->target_round,
           target->sf_g_index,
           target->source_col,
           target->target_output_col,
           trace_sequence_index);
    fflush(stdout);
    return verify_result;
}

static void warm_up_variants(const pk_variants_t *variants,
                             const unsigned char *message,
                             uint64_t message_len,
                             const sign_t *sig) {
    const pubkey_t *warmup_order[3] = {
        &variants->legal,
        &variants->wrong,
        &variants->correct
    };

    for (int iter = 0; iter < 20; iter++) {
        (void) LESS_verify(warmup_order[iter % 3], (const char *) message, message_len, sig);
    }
}

#ifdef LESS_TRACE_RREF
static void configure_trace_runtime(int enable_trace, const char *trace_file, uint32_t target_round) {
    if (enable_trace && (trace_file != NULL) && (trace_file[0] != '\0')) {
        char round_buf[32];
        snprintf(round_buf, sizeof(round_buf), "%u", target_round);
        setenv("LESS_TRACE_RREF_FILE", trace_file, 1);
        setenv("LESS_TRACE_RREF_ROUND_ONLY", round_buf, 1);
    } else {
        unsetenv("LESS_TRACE_RREF_FILE");
        unsetenv("LESS_TRACE_RREF_ROUND_ONLY");
    }
    less_trace_reset_runtime_config();
}
#endif

int main(int argc, char **argv) {
    cli_config_t cfg;
    if (!parse_cli(argc, argv, &cfg)) {
        return 1;
    }

    unsigned char seed_material[48];
    fill_seed_material(cfg.seed, seed_material);
    initialize_csprng(&platform_csprng_state, seed_material, sizeof(seed_material));

    unsigned char *message = (unsigned char *) calloc((size_t) cfg.message_len, sizeof(unsigned char));
    if (message == NULL) {
        fprintf(stderr, "failed to allocate message buffer\n");
        return 1;
    }
    fill_message(cfg.seed, message, cfg.message_len);

    prikey_t sk;
    pubkey_t legal_pk;
    sign_t sig;
    generator_mat_t target_sf_matrix;
    uint8_t target_pivot_flags[N] = {0};
    target_info_t target;
    memset(&sk, 0, sizeof(sk));
    memset(&legal_pk, 0, sizeof(legal_pk));
    memset(&sig, 0, sizeof(sig));

#ifdef LESS_TRACE_RREF
    const char *trace_file = getenv("LESS_TRACE_RREF_FILE");
    char trace_file_copy[4096] = {0};
    if ((trace_file != NULL) && (trace_file[0] != '\0')) {
        snprintf(trace_file_copy, sizeof(trace_file_copy), "%s", trace_file);
    }
    configure_trace_runtime(0, NULL, 0);
#endif

    LESS_keygen(&sk, &legal_pk);
    (void) LESS_sign(&sk, (const char *) message, cfg.message_len, &sig);

    if (LESS_verify(&legal_pk, (const char *) message, cfg.message_len, &sig) != 1) {
        fprintf(stderr, "baseline legal verify failed for %s\n", k_parameter_tag);
        free(message);
        return 1;
    }

    if (!find_target_info(&legal_pk, &sig, &target, &target_sf_matrix, target_pivot_flags)) {
        fprintf(stderr, "failed to select Phase 3 target column for %s\n", k_parameter_tag);
        free(message);
        return 1;
    }

    pk_variants_t warmup_variants;
    build_variant_public_keys(&legal_pk,
                              &target_sf_matrix,
                              target_pivot_flags,
                              &target,
                              cfg.seed,
                              0,
                              &warmup_variants);

#ifdef LESS_TRACE_RREF
    warm_up_variants(&warmup_variants, message, cfg.message_len, &sig);
    configure_trace_runtime(trace_file_copy[0] != '\0', trace_file_copy, target.target_round);
#else
    warm_up_variants(&warmup_variants, message, cfg.message_len, &sig);
#endif

#ifdef LESS_TRACE_RREF
    uint32_t trace_sequence_index = 0;
#endif
    for (int base_idx = 0; base_idx < cfg.bases; base_idx++) {
        pk_variants_t variants;
        build_variant_public_keys(&legal_pk,
                                  &target_sf_matrix,
                                  target_pivot_flags,
                                  &target,
                                  cfg.seed,
                                  (uint32_t) base_idx,
                                  &variants);

        for (int rep_idx = 0; rep_idx < cfg.reps; rep_idx++) {
            int order[3];
            splitmix64_t shuffle_rng = make_shuffle_rng(cfg.seed, (uint32_t) base_idx, (uint32_t) rep_idx);
            shuffle_variants(&shuffle_rng, order);

            for (int slot = 0; slot < 3; slot++) {
                const int variant_id = order[slot];
                const char *variant_name = NULL;
                const pubkey_t *variant_pk = NULL;

                if (variant_id == 0) {
                    variant_name = "legal";
                    variant_pk = &variants.legal;
                } else if (variant_id == 1) {
                    variant_name = "wrong";
                    variant_pk = &variants.wrong;
                } else {
                    variant_name = "correct";
                    variant_pk = &variants.correct;
                }

                uint32_t sample_trace_sequence = 0;
#ifdef LESS_TRACE_RREF
                if ((getenv("LESS_TRACE_RREF_FILE") != NULL) && (getenv("LESS_TRACE_RREF_FILE")[0] != '\0')) {
                    trace_sequence_index += 1;
                    sample_trace_sequence = trace_sequence_index;
                }
#endif
                (void) run_verify_sample(variant_name,
                                         variant_pk,
                                         message,
                                         cfg.message_len,
                                         &sig,
                                         &target,
                                         (uint32_t) base_idx,
                                         (uint32_t) rep_idx,
                                         sample_trace_sequence);
            }
        }
    }

    free(message);
    return 0;
}
