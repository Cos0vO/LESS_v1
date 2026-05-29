#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "codes.h"
#include "parameters.h"

static int run_malformed_reuse_case(void) {
    generator_mat_t G;
    uint8_t is_pivot_column[N_pad];
    uint8_t was_pivot_column[N_pad];

    memset(&G, 0, sizeof(G));
    memset(is_pivot_column, 0, sizeof(is_pivot_column));
    memset(was_pivot_column, 0, sizeof(was_pivot_column));

    /*
     * Deliberately violates generator_RREF_pivot_reuse's implicit precondition:
     * column 0 is advertised as a reusable pivot column, but the matrix has no
     * nonzero entry in that column. ASan should catch the resulting bad row swap.
     */
    was_pivot_column[0] = 1;

    return generator_RREF_pivot_reuse(&G, is_pivot_column, was_pivot_column, K);
}

int main(void) {
    int ret = run_malformed_reuse_case();
    printf("generator_RREF_pivot_reuse returned %d\n", ret);
    return 0;
}
