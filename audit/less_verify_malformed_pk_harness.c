#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "LESS.h"
#include "parameters.h"
#include "rng.h"

static void corrupt_public_key_pivot_flags(pubkey_t *pk, const uint8_t value) {
    const uint32_t pivot_flag_bytes = (N + 7u) / 8u;

    for (uint32_t key_idx = 0; key_idx < NUM_KEYPAIRS - 1; key_idx++) {
        memset(pk->SF_G[key_idx], value, pivot_flag_bytes);
    }
}

int main(int argc, char **argv) {
    static const unsigned char seed[48] = {
        0x83, 0xC6, 0x53, 0x70, 0x8F, 0xAF, 0x3E, 0x5F,
        0x6F, 0xBC, 0x9D, 0xFB, 0xE6, 0xFB, 0x5E, 0x83,
        0xE5, 0x72, 0xA7, 0x68, 0x86, 0x45, 0xD7, 0x5D,
        0x2C, 0x48, 0x35, 0xB2, 0x86, 0x95, 0xDE, 0xA4,
        0xBD, 0x70, 0x93, 0x74, 0x0D, 0x0F, 0xF4, 0x32,
        0x37, 0x35, 0x4E, 0xAD, 0x1C, 0x97, 0x8B, 0xC2,
    };
    static const char msg[] = "LESS malformed public key RREF reuse audit";

    prikey_t sk;
    pubkey_t pk;
    sign_t sig;

    initialize_csprng(&platform_csprng_state, seed, sizeof(seed));
    memset(&sk, 0, sizeof(sk));
    memset(&pk, 0, sizeof(pk));
    memset(&sig, 0, sizeof(sig));

    LESS_keygen(&sk, &pk);
    const size_t opened = LESS_sign(&sk, msg, sizeof(msg) - 1u, &sig);
    printf("valid signature opened_seeds=%zu\n", opened);

    const int valid = LESS_verify(&pk, msg, sizeof(msg) - 1u, &sig);
    printf("valid LESS_verify returned %d\n", valid);
    if (valid != 1) {
        return 2;
    }

    const uint8_t corrupt_value = (argc > 1 && strcmp(argv[1], "zero") == 0) ? 0x00 : 0xff;
    corrupt_public_key_pivot_flags(&pk, corrupt_value);
    printf("corrupted public key pivot flags with 0x%02x; calling LESS_verify\n", corrupt_value);

    const int malformed = LESS_verify(&pk, msg, sizeof(msg) - 1u, &sig);
    printf("malformed LESS_verify returned %d\n", malformed);
    return 0;
}
