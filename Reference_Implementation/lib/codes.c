/**
 *
 * LESS 的参考 ISO-C11 实现。
 *
 * @version 1.2（2025 年 2 月）
 *
 * @author Alessandro Barenghi <alessandro.barenghi@polimi.it>
 * @author Gerardo Pelosi <gerardo.pelosi@polimi.it>
 * @author Floyd Zweydinger <zweydfg8+github@rub.de>
 *
 * 本代码在此置于公有领域。
 *
 * 本软件由作者按“原样”提供，不作任何明示或暗示担保，包括但不限于
 * 适销性和特定用途适用性的暗示担保。无论基于何种责任理论，无论是
 * 合同、严格责任还是侵权（包括疏忽或其他原因），作者或贡献者均不
 * 对因使用本软件而以任何方式产生的任何直接、间接、偶然、特殊、
 * 惩罚性或后果性损害承担责任，包括但不限于替代商品或服务的采购、
 * 使用损失、数据损失、利润损失或业务中断，即使已被告知可能发生
 * 此类损害。
 *
 **/

#include <string.h>

#include "codes.h"
#include "fq_ct.h"
#include "parameters.h"
#include "row_ct.h"
#include "trace_rref.h"

/// 交换 r 和 s 中的 N 个 uint8 元素。
/// \param r[in/out]
/// \param s[in/out]
void swap_rows(FQ_ELEM r[N],
               FQ_ELEM s[N]) {
    FQ_ELEM tmp[N];
    memcpy(tmp, r, sizeof(FQ_ELEM) * N);
    memcpy(r, s, sizeof(FQ_ELEM) * N);
    memcpy(s, tmp, sizeof(FQ_ELEM) * N);
} /* swap_rows 结束 */

/* 计算 pivot 标记数组 */
void generator_get_pivot_flags(const rref_generator_mat_t *const G,
                               uint8_t pivot_flag [N]) {
    for (uint32_t i = 0; i < N; i = i + 1) {
        pivot_flag[i] = 1;
    }

    for (uint32_t i = 0; i < K; i = i + 1) {
        pivot_flag[G->column_pos[i]] = 0;
    }
}

/* 将生成矩阵右乘一个 monomial（单项式矩阵） */
void generator_monomial_mul(generator_mat_t *res,
                            const generator_mat_t *const G,
                            const monomial_t *const monom) {
   for(uint32_t src_col_idx = 0; src_col_idx < N; src_col_idx++) {
      for(uint32_t row_idx = 0; row_idx < K; row_idx++) {
         res->values[row_idx][monom->permutation[src_col_idx]] =
            fq_mul(G->values[row_idx][src_col_idx], monom->coefficients[src_col_idx]);
      }
   }
} /* generator_monomial_mul 结束 */

/// \param G[in/out]: 生成矩阵
/// \param is_pivot_column[out]: N 字节数组；如果某列是 pivot 列，
///                 对应位置设为 1
/// \return 0 表示失败
///         1 表示成功

int generator_RREF(generator_mat_t *G,
                   uint8_t is_pivot_column[N]) {                           // 高斯消元
   unsigned rref_success = 1;

   for (unsigned row_to_reduce = 0; row_to_reduce < K; row_to_reduce++) {
      const uint64_t trace_row_start_ns = less_trace_now_ns();
      unsigned pivot_row = row_to_reduce;
      unsigned pivot_column = row_to_reduce;
      ct_mask_t found_mask = CT_FALSE;

      /* 固定扫描 pivot 搜索：用 mask 保留第一个非零候选项。 */
      for (unsigned col = row_to_reduce; col < N; col++) {
         for (unsigned row = row_to_reduce; row < K; row++) {
            const ct_mask_t take = fq_isnonzero_ct(G->values[row][col]) & ~found_mask;

            pivot_row = ct_select_u32(take, row, pivot_row);
            pivot_column = ct_select_u32(take, col, pivot_column);
            found_mask |= take;
         }
      }
      const unsigned found_pivot = found_mask & 1u;
      rref_success &= found_pivot;

      for (unsigned col = 0; col < N; col++) {
         const ct_mask_t set_mask = found_mask & ct_eq_u32(col, pivot_column);
         is_pivot_column[col] = ct_select_u8(set_mask, 1, is_pivot_column[col]);
      }

      /* 如果找到的 pivot 位于索引大于当前待化简行的行上，
       * 就需要交换行 */
      const int did_row_swap =
         (int)((found_mask & ct_is_nonzero_u32(row_to_reduce ^ pivot_row)) & 1u);
      for (unsigned row = row_to_reduce; row < K; row++) {
         const ct_mask_t swap_mask = found_mask & ct_eq_u32(row, pivot_row);

         for (unsigned col_idx = 0; col_idx < N; col_idx++) {
            FQ_ELEM tmp = (FQ_ELEM)(swap_mask &
                                    (G->values[row_to_reduce][col_idx] ^
                                     G->values[row][col_idx]));
            G->values[row_to_reduce][col_idx] ^= tmp;
            G->values[row][col_idx] ^= tmp;
         }
      }
      pivot_row = row_to_reduce; /* 含有 pivot 的行现在已经就位 */

      /* 在不直接索引秘密 pivot 列的情况下计算缩放因子。 */
      FQ_ELEM pivot_value = 0;
      for (unsigned col_idx = 0; col_idx < N; col_idx++) {
         const ct_mask_t select_mask = found_mask & ct_eq_u32(col_idx, pivot_column);
         pivot_value = fq_select_ct(select_mask, G->values[pivot_row][col_idx], pivot_value);
      }
      const FQ_ELEM scaling_factor = fq_inv_ct_safe(pivot_value, found_mask);

      /* 重新缩放 pivot 行，使 pivot = 1。pivot 左侧的值
       * 已在之前的迭代中被置为 0 */
      for (unsigned i = 0; i < N; i++) {
         const ct_mask_t mask = found_mask & ct_ge_u32(i, pivot_column);
         const FQ_ELEM scaled = fq_mul_ct(scaling_factor, G->values[pivot_row][i]);

         G->values[pivot_row][i] = fq_select_ct(mask, scaled, G->values[pivot_row][i]);
      }
      /* pivot 行就位并缩放后，用它从其他行中消去对应分量 */
      for (unsigned row_idx = 0; row_idx < K; row_idx++) {
         const ct_mask_t mask = found_mask & ~ct_eq_u32(row_idx, pivot_row);
         FQ_ELEM multiplier = 0;
         for (unsigned multiplier_col = 0; multiplier_col < N; multiplier_col++) {
            const ct_mask_t select_mask = found_mask & ct_eq_u32(multiplier_col, pivot_column);

            multiplier = fq_select_ct(select_mask,
                                      G->values[row_idx][multiplier_col],
                                      multiplier);
         }

         /* pivot 行中 pivot 之前的元素全为 0，不需要从其他行中减去它们。 */
         for (unsigned col_idx = 0; col_idx < N; col_idx++) {
            const FQ_ELEM tmp = fq_mul_ct(multiplier, G->values[pivot_row][col_idx]);
            const FQ_ELEM reduced = fq_sub_ct(G->values[row_idx][col_idx], tmp);

            G->values[row_idx][col_idx] =
               fq_select_ct(mask, reduced, G->values[row_idx][col_idx]);
         }
      }

      less_trace_rref_event("generator_rref_step",
                            "generator_RREF",
                            "data-dependent execution",
                            row_to_reduce,
                            pivot_row,
                            pivot_column,
                            found_pivot,
                            did_row_swap,
                            0,
                            less_trace_now_ns() - trace_row_start_ns);
   }

   return (int) rref_success;
} /* generator_RREF 结束 */

int generator_RREF_ct(generator_mat_t *G,
                      uint8_t is_pivot_column[N_pad]) {
   return generator_RREF(G, is_pivot_column);
}

int generator_RREF_ct_level_a_fast(generator_mat_t *G,
                                   uint8_t is_pivot_column[N_pad]) {
   unsigned rref_success = 1;

   for (unsigned row_to_reduce = 0; row_to_reduce < K; row_to_reduce++) {
      const uint64_t trace_row_start_ns = less_trace_now_ns();
      unsigned pivot_row = row_to_reduce;
      unsigned pivot_column = row_to_reduce;
      ct_mask_t found_mask = CT_FALSE;

      for (unsigned col = row_to_reduce; col < N; col++) {
         for (unsigned row = row_to_reduce; row < K; row++) {
            const ct_mask_t take = fq_isnonzero_ct(G->values[row][col]) & ~found_mask;

            pivot_row = ct_select_u32(take, row, pivot_row);
            pivot_column = ct_select_u32(take, col, pivot_column);
            found_mask |= take;
         }
      }
      const unsigned found_pivot = found_mask & 1u;
      rref_success &= found_pivot;

      for (unsigned col = 0; col < N; col++) {
         const ct_mask_t set_mask = found_mask & ct_eq_u32(col, pivot_column);
         is_pivot_column[col] = ct_select_u8(set_mask, 1, is_pivot_column[col]);
      }

      const int did_row_swap =
         (int)((found_mask & ct_is_nonzero_u32(row_to_reduce ^ pivot_row)) & 1u);
      for (unsigned row = row_to_reduce; row < K; row++) {
         const ct_mask_t swap_mask = found_mask & ct_eq_u32(row, pivot_row);

         for (unsigned col_idx = 0; col_idx < N; col_idx++) {
            FQ_ELEM tmp = (FQ_ELEM)(swap_mask &
                                    (G->values[row_to_reduce][col_idx] ^
                                     G->values[row][col_idx]));
            G->values[row_to_reduce][col_idx] ^= tmp;
            G->values[row][col_idx] ^= tmp;
         }
      }
      pivot_row = row_to_reduce;

      const FQ_ELEM pivot_value =
         fq_select_ct(found_mask, G->values[pivot_row][pivot_column], 0);
      const FQ_ELEM scaling_factor = fq_inv_ct_safe(pivot_value, found_mask);

      for (unsigned i = 0; i < N; i++) {
         const ct_mask_t mask = found_mask & ct_ge_u32(i, pivot_column);
         const FQ_ELEM scaled = fq_mul_ct(scaling_factor, G->values[pivot_row][i]);

         G->values[pivot_row][i] = fq_select_ct(mask, scaled, G->values[pivot_row][i]);
      }

      for (unsigned row_idx = 0; row_idx < K; row_idx++) {
         const ct_mask_t mask = found_mask & ~ct_eq_u32(row_idx, pivot_row);
         const FQ_ELEM multiplier =
            fq_select_ct(found_mask, G->values[row_idx][pivot_column], 0);

         ct_cond_fma_row_level_a(G->values[row_idx],
                                 G->values[pivot_row],
                                 multiplier,
                                 mask);
      }

      less_trace_rref_event("generator_rref_step",
                            "generator_RREF_ct_level_a_fast",
                            "level-a direct pivot-column access",
                            row_to_reduce,
                            pivot_row,
                            pivot_column,
                            found_pivot,
                            did_row_swap,
                            0,
                            less_trace_now_ns() - trace_row_start_ns);
   }

   return (int) rref_success;
}

int generator_RREF_qct_pivot_reuse(generator_mat_t *G,
                                   uint8_t is_pivot_column[N],
                                   uint8_t was_pivot_column[N],
                                   const int pvt_reuse_limit) {
   (void) was_pivot_column;
   (void) pvt_reuse_limit;
   return generator_RREF_ct_level_a_fast(G, is_pivot_column);
}

int generator_RREF_mode(generator_mat_t *G,
                        uint8_t is_pivot_column[N_pad],
                        uint8_t was_pivot_column[N_pad],
                        const int pvt_reuse_limit,
                        const less_rref_mode_t mode) {
   switch (mode) {
      case LESS_RREF_MODE_FAST:
         if (was_pivot_column != NULL) {
            return generator_RREF_pivot_reuse(G,
                                              is_pivot_column,
                                              was_pivot_column,
                                              pvt_reuse_limit);
         }
         return generator_RREF(G, is_pivot_column);
      case LESS_RREF_MODE_QCT:
         return generator_RREF_qct_pivot_reuse(G,
                                               is_pivot_column,
                                               was_pivot_column,
                                               pvt_reuse_limit);
      case LESS_RREF_MODE_CT:
      default:
         return generator_RREF_ct(G, is_pivot_column);
   }
}

/// \param G[in/out]: K \times N 生成矩阵
/// \param is_pivot_column[out]: N 字节数组；如果某列是 pivot 列，
///                 对应位置设为 1
/// \param was_pivot_column[out]: N 字节数组；如果某列曾是 pivot 列，
///                 对应位置设为 1
/// \param pvt_reuse_limit:[in]: pivot 复用上限
/// \return 0 表示失败
///         1 表示成功
int generator_RREF_pivot_reuse(generator_mat_t *G,
                               uint8_t is_pivot_column[N],
                               uint8_t was_pivot_column[N],
                               const int pvt_reuse_limit) {
   int pvt_reuse_cnt = 0;

    // 行交换预处理：把之前的 pivot 元素交换到对应行，以降低被破坏的可能性
    if (pvt_reuse_limit != 0) {
        for (int preproc_col = K - 1; preproc_col >= 0; preproc_col--) {
            if (was_pivot_column[preproc_col] == 1) {
                // 查找 pivot 所在行
                uint32_t pivot_el_row = -1;                             // 问题：UINT32_MAX
                for (uint32_t row = 0; row < K; row = row + 1) {
                    if (G->values[row][preproc_col] != 0) {
                        pivot_el_row = row;
                    }
                }
                swap_rows(G->values[preproc_col], G->values[pivot_el_row]);
            }
        }
    }

	    for (uint32_t row_to_reduce = 0; row_to_reduce < K; row_to_reduce++) {
	        const uint64_t trace_row_start_ns = less_trace_now_ns();
	        uint32_t pivot_row = row_to_reduce;
        /* 先从列号等于行号的位置开始搜索 pivot */
        uint32_t pivot_column = row_to_reduce;
        while ((pivot_column < N) && (G->values[pivot_row][pivot_column] == 0)) {
            while ((pivot_row < K) && (G->values[pivot_row][pivot_column] == 0)) {
                pivot_row++;
            }
            if (pivot_row >= K) { /* 已经扫完整个列尾部 */
                pivot_column++; /* 移动到下一列 */
                pivot_row = row_to_reduce; /* 从待化简行重新开始 */
            }
        }
	        if (pivot_column >= N) {
	            less_trace_rref_event("generator_rref_pivot_reuse_step",
	                                  "generator_RREF_pivot_reuse",
	                                  "data-dependent execution",
	                                  row_to_reduce,
	                                  pivot_row,
	                                  pivot_column,
	                                  0,
	                                  0,
	                                  0,
	                                  less_trace_now_ns() - trace_row_start_ns);
		            return 0; /* 没有剩余 pivot 候选项，报告失败 */
	        }
		        is_pivot_column[pivot_column] = 1; /* 找到 pivot，标记该列 */

		        /* 如果找到的 pivot 位于索引大于当前待化简行的行上，
		         * 就需要交换行 */
	        const int did_row_swap = (row_to_reduce != pivot_row);
	        if (row_to_reduce != pivot_row) {
		            was_pivot_column[pivot_row] = 0; // pivot 不再可复用：它会在行化简过程中被破坏
	            swap_rows(G->values[row_to_reduce], G->values[pivot_row]);
        }
	        pivot_row = row_to_reduce; /* 含有 pivot 的行现在已经就位 */

	        /// 注意：这里需要解释。我们可以跳过 pivot 行的化简，因为这对
	        /// CF 来说并不重要。CF 只关心 0 的数量，而化简一个复用的
	        /// pivot 行不会改变这个数量。
	        if (((was_pivot_column[pivot_column] == 1) && (pvt_reuse_cnt < pvt_reuse_limit) && (pivot_column < K))) {
	            pvt_reuse_cnt++;
	            less_trace_rref_event("generator_rref_pivot_reuse_step",
	                                  "generator_RREF_pivot_reuse",
	                                  "data-dependent execution",
	                                  row_to_reduce,
	                                  pivot_row,
	                                  pivot_column,
	                                  1,
	                                  did_row_swap,
	                                  1,
	                                  less_trace_now_ns() - trace_row_start_ns);
	            continue;
	        }

        /* 计算缩放因子 */
        const FQ_ELEM scaling_factor = fq_inv(G->values[pivot_row][pivot_column]);

        /* 重新缩放 pivot 行，使 pivot = 1。pivot 左侧的值
         * 已在之前的迭代中被置为 0 */
        for (uint32_t i = pivot_column; i < N; i++) {
            G->values[pivot_row][i] = fq_mul(scaling_factor, G->values[pivot_row][i]);
        }

        /* pivot 行就位并缩放后，用它从其他行中消去对应分量 */
        for (uint32_t row_idx = 0; row_idx < K; row_idx++) {
            if (row_idx != pivot_row) {
                FQ_ELEM multiplier = G->values[row_idx][pivot_column];
                /* pivot 行中 pivot 之前的元素全为 0，不需要从其他行中减去它们。 */
                for (int col_idx = 0; col_idx < N; col_idx++) {
                    FQ_ELEM tmp = fq_mul(multiplier, G->values[pivot_row][col_idx]);
	                    G->values[row_idx][col_idx] = fq_sub(G->values[row_idx][col_idx], tmp);
	                }
	            }
	        }

	        less_trace_rref_event("generator_rref_pivot_reuse_step",
	                              "generator_RREF_pivot_reuse",
	                              "data-dependent execution",
	                              row_to_reduce,
	                              pivot_row,
	                              pivot_column,
	                              1,
	                              did_row_swap,
	                              0,
	                              less_trace_now_ns() - trace_row_start_ns);
	    }

    return 1;
} /* generator_RREF_pivot_reuse 结束 */

/// 注意：不是常数时间
/// \param res[out]: G*c，一个 K \times N-K 生成矩阵
/// \param G[in]: 当前 K \times N-K 生成矩阵
/// \param c[in]: 压缩后的 CF 动作
void apply_cf_action_to_G(generator_mat_t* res,
                          const generator_mat_t *G,
                          const uint8_t *const c) {
    uint32_t l = 0, r = 0;
    for (uint32_t i = 0; i < N8; i++) {
        for (uint32_t j = 0; j < 8; j++) {
            if ((i*8 + j) >= N) { goto finish; }

            const uint8_t bit = (c[i] >> j) & 1u;
            uint32_t pos;
            if (bit) {
                pos = l;
                l += 1;
            } else {
                pos = K + r;
                r += 1;
            }

            // 复制该列
            for (uint32_t k = 0; k < K; k++) {
                res->values[k][pos] = G->values[k][i*8 + j];
            }
        }
    }
finish:
    return;
}

/// 注意：不是常数时间
/// \param res[out]: G*c，一个 K \times N-K 生成矩阵
/// \param G[in]: 当前 K \times N-K 生成矩阵
/// \param c[in]: 压缩后的 CF 动作
void apply_cf_action_to_G_with_pivots(generator_mat_t* res,
                                      const generator_mat_t *G,
                                      const uint8_t *const c,
                                      const uint8_t initial_G_col_pivot[N],
                                      uint8_t permuted_G_col_pivot[N]) {
    uint32_t l = 0, r = 0;
    for (uint32_t i = 0; i < N8; i++) {
        for (uint32_t j = 0; j < 8; j++) {
            if ((i*8 + j) >= N) { goto finish; }

            const uint8_t bit = (c[i] >> j) & 1u;
            uint32_t pos;
            if (bit) {
                pos = l;
                l += 1;
            } else {
                pos = K + r;
                r += 1;
            }

            permuted_G_col_pivot[pos] = initial_G_col_pivot[i*8+j];

            // 复制该列
            for (uint32_t k = 0; k < K; k++) {
                res->values[k][pos] = G->values[k][i*8 + j];
            }
        }
    }
finish:
    return;
}

/* 压缩 RREF 形式的生成矩阵，只保存 non-pivot 列及其位置 */
void generator_rref_compact(rref_generator_mat_t *compact,
                            const generator_mat_t *const full,
                            const uint8_t is_pivot_column[N]) {
    int dst_col_idx = 0;
    for (uint32_t src_col_idx = 0; src_col_idx < N; src_col_idx++) {
        if (!is_pivot_column[src_col_idx]) {
            for (uint32_t row_idx = 0; row_idx < K; row_idx++) {
                compact->values[row_idx][dst_col_idx] = full->values[row_idx][src_col_idx];
            }
            compact->column_pos[dst_col_idx] = src_col_idx;
            dst_col_idx++;
        }
    }
} /* generator_rref_compact 结束 */

/* 将 RREF 形式的生成矩阵压缩为字节数组 */
void compress_rref(uint8_t *compressed,
                   const generator_mat_t *const full,
                   const uint8_t is_pivot_column[N]) {
    // 压缩 pivot 标记
    for (uint32_t col_byte = 0; col_byte < N / 8; col_byte++) {
        compressed[col_byte] = is_pivot_column[8 * col_byte + 0] |
                               (is_pivot_column[8 * col_byte + 1] << 1) |
                               (is_pivot_column[8 * col_byte + 2] << 2) |
                               (is_pivot_column[8 * col_byte + 3] << 3) |
                               (is_pivot_column[8 * col_byte + 4] << 4) |
                               (is_pivot_column[8 * col_byte + 5] << 5) |
                               (is_pivot_column[8 * col_byte + 6] << 6) |
                               (is_pivot_column[8 * col_byte + 7] << 7);
    }

#if (CATEGORY == 252) || (CATEGORY == 548)
    // 压缩最后几个标记
    compressed[N / 8] = is_pivot_column[N - 4] | (is_pivot_column[N - 3] << 1) |
                        (is_pivot_column[N - 2] << 2) |
                        (is_pivot_column[N - 1] << 3);

    int compress_idx = N / 8 + 1;
#else
    int compress_idx = N / 8;
#endif

    // 按行压缩 non-pivot 列
    int encode_state = 0;
    for (uint32_t row_idx = 0; row_idx < K; row_idx++) {
        for (uint32_t col_idx = 0; col_idx < N; col_idx++) {
            if (!is_pivot_column[col_idx]) {
                switch (encode_state) {
                    case 0:
                        compressed[compress_idx] = full->values[row_idx][col_idx];
                        break;
                    case 1:
                        compressed[compress_idx] =
                                compressed[compress_idx] | (full->values[row_idx][col_idx] << 7);
                        compress_idx++;
                        compressed[compress_idx] = (full->values[row_idx][col_idx] >> 1);
                        break;
                    case 2:
                        compressed[compress_idx] =
                                compressed[compress_idx] | (full->values[row_idx][col_idx] << 6);
                        compress_idx++;
                        compressed[compress_idx] = (full->values[row_idx][col_idx] >> 2);
                        break;
                    case 3:
                        compressed[compress_idx] =
                                compressed[compress_idx] | (full->values[row_idx][col_idx] << 5);
                        compress_idx++;
                        compressed[compress_idx] = (full->values[row_idx][col_idx] >> 3);
                        break;
                    case 4:
                        compressed[compress_idx] =
                                compressed[compress_idx] | (full->values[row_idx][col_idx] << 4);
                        compress_idx++;
                        compressed[compress_idx] = (full->values[row_idx][col_idx] >> 4);
                        break;
                    case 5:
                        compressed[compress_idx] =
                                compressed[compress_idx] | (full->values[row_idx][col_idx] << 3);
                        compress_idx++;
                        compressed[compress_idx] = (full->values[row_idx][col_idx] >> 5);
                        break;
                    case 6:
                        compressed[compress_idx] =
                                compressed[compress_idx] | (full->values[row_idx][col_idx] << 2);
                        compress_idx++;
                        compressed[compress_idx] = (full->values[row_idx][col_idx] >> 6);
                        break;
                    case 7:
                        compressed[compress_idx] =
                                compressed[compress_idx] | (full->values[row_idx][col_idx] << 1);
                        compress_idx++;
                        break;
                }

                if (encode_state != 7) {
                    encode_state++;
                } else {
                    encode_state = 0;
                }
            }
        }
    } /* compress_rref 结束 */
}

/* 将压缩的 RREF 生成矩阵展开为完整矩阵 */
void expand_to_rref(generator_mat_t *full,
                    const uint8_t *compressed,
                    uint8_t is_pivot_column[N]) {
    // 解压 pivot 标记
    for (int i = 0; i < N; i++) {
        is_pivot_column[i] = 0;
    }

    for (int col_byte = 0; col_byte < N / 8; col_byte++) {
        is_pivot_column[col_byte * 8 + 0] = compressed[col_byte] & 0x1;
        is_pivot_column[col_byte * 8 + 1] = (compressed[col_byte] >> 1) & 0x1;
        is_pivot_column[col_byte * 8 + 2] = (compressed[col_byte] >> 2) & 0x1;
        is_pivot_column[col_byte * 8 + 3] = (compressed[col_byte] >> 3) & 0x1;
        is_pivot_column[col_byte * 8 + 4] = (compressed[col_byte] >> 4) & 0x1;
        is_pivot_column[col_byte * 8 + 5] = (compressed[col_byte] >> 5) & 0x1;
        is_pivot_column[col_byte * 8 + 6] = (compressed[col_byte] >> 6) & 0x1;
        is_pivot_column[col_byte * 8 + 7] = (compressed[col_byte] >> 7) & 0x1;
    }

#if (CATEGORY == 252) || (CATEGORY == 548)
    // 解压最后几个标记
    is_pivot_column[N - 4] = compressed[N / 8] & 0x1;
    is_pivot_column[N - 3] = (compressed[N / 8] >> 1) & 0x1;
    is_pivot_column[N - 2] = (compressed[N / 8] >> 2) & 0x1;
    is_pivot_column[N - 1] = (compressed[N / 8] >> 3) & 0x1;

    int compress_idx = N / 8 + 1;
#else
    int compress_idx = N / 8;
#endif

    // 按行解压列
    int decode_state = 0;
    for (uint32_t row_idx = 0; row_idx < K; row_idx++) {
        int pivot_idx = 0;
        for (uint32_t col_idx = 0; col_idx < N; col_idx++) {
            if (!is_pivot_column[col_idx]) {
                // 解压 non-pivot 列
                switch (decode_state) {
                    case 0:
                        full->values[row_idx][col_idx] = compressed[compress_idx] & MASK_Q;
                        break;
                    case 1:
                        full->values[row_idx][col_idx] =
                                ((compressed[compress_idx] >> 7) |
                                 (compressed[compress_idx + 1] << 1)) &
                                MASK_Q;
                        compress_idx++;
                        break;
                    case 2:
                        full->values[row_idx][col_idx] =
                                ((compressed[compress_idx] >> 6) |
                                 (compressed[compress_idx + 1] << 2)) &
                                MASK_Q;
                        compress_idx++;
                        break;
                    case 3:
                        full->values[row_idx][col_idx] =
                                ((compressed[compress_idx] >> 5) |
                                 (compressed[compress_idx + 1] << 3)) &
                                MASK_Q;
                        compress_idx++;
                        break;
                    case 4:
                        full->values[row_idx][col_idx] =
                                ((compressed[compress_idx] >> 4) |
                                 (compressed[compress_idx + 1] << 4)) &
                                MASK_Q;
                        compress_idx++;
                        break;
                    case 5:
                        full->values[row_idx][col_idx] =
                                ((compressed[compress_idx] >> 3) |
                                 (compressed[compress_idx + 1] << 5)) &
                                MASK_Q;
                        compress_idx++;
                        break;
                    case 6:
                        full->values[row_idx][col_idx] =
                                ((compressed[compress_idx] >> 2) |
                                 (compressed[compress_idx + 1] << 6)) &
                                MASK_Q;
                        compress_idx++;
                        break;
                    case 7:
                        full->values[row_idx][col_idx] =
                                (compressed[compress_idx] >> 1) & MASK_Q;
                        compress_idx++;
                        break;
                }

                if (decode_state != 7) {
                    decode_state++;
                } else {
                    decode_state = 0;
                }
            } else {
                // 解压 pivot 列
                full->values[row_idx][col_idx] = ((uint32_t)row_idx == (uint32_t)pivot_idx);
                pivot_idx++;
            }
        }
    }

} /* expand_to_rref 结束 */


/* 将压缩的 RREF 生成矩阵展开为完整矩阵 */
void generator_rref_expand(generator_mat_t *full,
                           const rref_generator_mat_t *const compact) {
    int placed_dense_cols = 0;
    for (uint32_t col_idx = 0; col_idx < N; col_idx++) {
        if ((placed_dense_cols < N - K) && (col_idx == compact->column_pos[placed_dense_cols])) {
            /* non-pivot 列：恢复一个完整列 */
            for (uint32_t row_idx = 0; row_idx < K; row_idx++) {
                full->values[row_idx][col_idx] = compact->values[row_idx][placed_dense_cols];
            }
            placed_dense_cols++;
        } else {
            /* 重新生成对应的 pivot 列 */
            for (uint32_t row_idx = 0; row_idx < K; row_idx++) {
                full->values[row_idx][col_idx] = (row_idx == col_idx - placed_dense_cols);

            }
        }
    }
} /* generator_rref_expand 结束 */

// V1 = V2
void normalized_copy(normalized_IS_t *V1,
                     const normalized_IS_t *V2) {
    memcpy(V1->values, V2->values, sizeof(normalized_IS_t));
}

/// \param V[in/out]: K \times N-K 矩阵，其中 `row1` 和
///                 `row2` 会被交换
/// \param row1[in]: 第一行
/// \param row2[in]: 第二行
void normalized_row_swap(normalized_IS_t *V,
                         const POSITION_T row1,
                         const POSITION_T row2) {
    if (row1 == row2) { return; }
    for(uint32_t i = 0; i < N-K; i++){
        POSITION_T tmp = V->values[row1][i];
        V->values[row1][i] = V->values[row2][i];
        V->values[row2][i] = tmp;
    }
}

/// \param res[out]: 满秩 K \times N-K 生成矩阵
/// \param seed[int] PRNG 的 seed
void generator_sample(rref_generator_mat_t *res, const unsigned char seed[SEED_LENGTH_BYTES]) {
    SHAKE_STATE_STRUCT csprng_state;
    initialize_csprng(&csprng_state, seed, SEED_LENGTH_BYTES);
    for (uint32_t i = 0; i < K; i++) {
        rand_range_q_state_elements(&csprng_state, res->values[i], N - K);
    }
    for (uint32_t i = 0; i < N - K; i++) {
        res->column_pos[i] = i + K;
    }
} /* generator_seed_expand 结束 */
