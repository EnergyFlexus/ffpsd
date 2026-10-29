/* Renames to ffpsd_ what zlib-ng's own ZLIB_SYMBOL_PREFIX leaves out; the list is written by update_vendor_prefixes. */
#ifndef FFPSD_PREFIX_ZLIB_NG_H
#define FFPSD_PREFIX_ZLIB_NG_H

#define adler32_avx2 ffpsd_adler32_avx2
#define adler32_avx512 ffpsd_adler32_avx512
#define adler32_avx512_vnni ffpsd_adler32_avx512_vnni
#define adler32_c ffpsd_adler32_c
#define adler32_fold_copy_avx2 ffpsd_adler32_fold_copy_avx2
#define adler32_fold_copy_avx512 ffpsd_adler32_fold_copy_avx512
#define adler32_fold_copy_avx512_vnni ffpsd_adler32_fold_copy_avx512_vnni
#define adler32_fold_copy_c ffpsd_adler32_fold_copy_c
#define adler32_fold_copy_neon ffpsd_adler32_fold_copy_neon
#define adler32_fold_copy_sse42 ffpsd_adler32_fold_copy_sse42
#define adler32_neon ffpsd_adler32_neon
#define adler32_ssse3 ffpsd_adler32_ssse3
#define alloc_deflate ffpsd_alloc_deflate
#define alloc_inflate ffpsd_alloc_inflate
#define arm_check_features ffpsd_arm_check_features
#define base_dist ffpsd_base_dist
#define base_length ffpsd_base_length
#define chorba_small_nondestructive_sse2 ffpsd_chorba_small_nondestructive_sse2
#define chunkmemset_safe_avx2 ffpsd_chunkmemset_safe_avx2
#define chunkmemset_safe_avx512 ffpsd_chunkmemset_safe_avx512
#define chunkmemset_safe_c ffpsd_chunkmemset_safe_c
#define chunkmemset_safe_neon ffpsd_chunkmemset_safe_neon
#define chunkmemset_safe_sse2 ffpsd_chunkmemset_safe_sse2
#define chunkmemset_safe_ssse3 ffpsd_chunkmemset_safe_ssse3
#define compare256_avx2 ffpsd_compare256_avx2
#define compare256_avx512 ffpsd_compare256_avx512
#define compare256_c ffpsd_compare256_c
#define compare256_neon ffpsd_compare256_neon
#define compare256_sse2 ffpsd_compare256_sse2
#define cpu_check_features ffpsd_cpu_check_features
#define crc32_armv8 ffpsd_crc32_armv8
#define crc32_braid ffpsd_crc32_braid
#define crc32_braid_internal ffpsd_crc32_braid_internal
#define crc32_chorba ffpsd_crc32_chorba
#define crc32_chorba_118960_nondestructive ffpsd_crc32_chorba_118960_nondestructive
#define crc32_chorba_32768_nondestructive ffpsd_crc32_chorba_32768_nondestructive
#define crc32_chorba_small_nondestructive ffpsd_crc32_chorba_small_nondestructive
#define crc32_chorba_sse2 ffpsd_crc32_chorba_sse2
#define crc32_chorba_sse41 ffpsd_crc32_chorba_sse41
#define crc32_fold_armv8 ffpsd_crc32_fold_armv8
#define crc32_fold_c ffpsd_crc32_fold_c
#define crc32_fold_copy_armv8 ffpsd_crc32_fold_copy_armv8
#define crc32_fold_copy_c ffpsd_crc32_fold_copy_c
#define crc32_fold_final_c ffpsd_crc32_fold_final_c
#define crc32_fold_pclmulqdq ffpsd_crc32_fold_pclmulqdq
#define crc32_fold_pclmulqdq_copy ffpsd_crc32_fold_pclmulqdq_copy
#define crc32_fold_pclmulqdq_final ffpsd_crc32_fold_pclmulqdq_final
#define crc32_fold_pclmulqdq_reset ffpsd_crc32_fold_pclmulqdq_reset
#define crc32_fold_reset_c ffpsd_crc32_fold_reset_c
#define crc32_fold_vpclmulqdq ffpsd_crc32_fold_vpclmulqdq
#define crc32_fold_vpclmulqdq_copy ffpsd_crc32_fold_vpclmulqdq_copy
#define crc32_fold_vpclmulqdq_final ffpsd_crc32_fold_vpclmulqdq_final
#define crc32_fold_vpclmulqdq_reset ffpsd_crc32_fold_vpclmulqdq_reset
#define crc32_pclmulqdq ffpsd_crc32_pclmulqdq
#define crc32_vpclmulqdq ffpsd_crc32_vpclmulqdq
#define deflate_fast ffpsd_deflate_fast
#define deflate_huff ffpsd_deflate_huff
#define deflate_medium ffpsd_deflate_medium
#define deflate_quick ffpsd_deflate_quick
#define deflate_rle ffpsd_deflate_rle
#define deflate_slow ffpsd_deflate_slow
#define deflate_stored ffpsd_deflate_stored
#define free_inflate ffpsd_free_inflate
#define functable ffpsd_functable
#define gen_codes ffpsd_gen_codes
#define gz_buffer_alloc ffpsd_gz_buffer_alloc
#define gz_buffer_free ffpsd_gz_buffer_free
#define gz_intmax ffpsd_gz_intmax
#define gz_state_free ffpsd_gz_state_free
#define inflate_fast_avx2 ffpsd_inflate_fast_avx2
#define inflate_fast_avx512 ffpsd_inflate_fast_avx512
#define inflate_fast_c ffpsd_inflate_fast_c
#define inflate_fast_neon ffpsd_inflate_fast_neon
#define inflate_fast_sse2 ffpsd_inflate_fast_sse2
#define inflate_fast_ssse3 ffpsd_inflate_fast_ssse3
#define insert_string ffpsd_insert_string
#define insert_string_roll ffpsd_insert_string_roll
#define longest_match_avx2 ffpsd_longest_match_avx2
#define longest_match_avx512 ffpsd_longest_match_avx512
#define longest_match_c ffpsd_longest_match_c
#define longest_match_neon ffpsd_longest_match_neon
#define longest_match_slow_avx2 ffpsd_longest_match_slow_avx2
#define longest_match_slow_avx512 ffpsd_longest_match_slow_avx512
#define longest_match_slow_c ffpsd_longest_match_slow_c
#define longest_match_slow_neon ffpsd_longest_match_slow_neon
#define longest_match_slow_sse2 ffpsd_longest_match_slow_sse2
#define longest_match_sse2 ffpsd_longest_match_sse2
#define quick_insert_string ffpsd_quick_insert_string
#define quick_insert_string_roll ffpsd_quick_insert_string_roll
#define slide_hash_avx2 ffpsd_slide_hash_avx2
#define slide_hash_c ffpsd_slide_hash_c
#define slide_hash_neon ffpsd_slide_hash_neon
#define slide_hash_sse2 ffpsd_slide_hash_sse2
#define static_dtree ffpsd_static_dtree
#define static_ltree ffpsd_static_ltree
#define update_hash ffpsd_update_hash
#define update_hash_roll ffpsd_update_hash_roll
#define x86_check_features ffpsd_x86_check_features
#define z_error ffpsd_z_error
#define z_verbose ffpsd_z_verbose
#define zng_dist_code ffpsd_zng_dist_code
#define zng_inflate_table ffpsd_zng_inflate_table
#define zng_length_code ffpsd_zng_length_code
#define zng_tr_align ffpsd_zng_tr_align
#define zng_tr_flush_bits ffpsd_zng_tr_flush_bits
#define zng_tr_flush_block ffpsd_zng_tr_flush_block
#define zng_tr_init ffpsd_zng_tr_init
#define zng_tr_stored_block ffpsd_zng_tr_stored_block

/* zbuild.h sets _POSIX_C_SOURCE, which on macOS hides u_int and the like from the sys/types.h zlib.h includes;
 * arm_features.c asks for them with _DARWIN_C_SOURCE only after this header. */
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif

/* deflateInit2, inflateInit2 and inflateBackInit are defined through PREFIX after an #undef of their names, and
 * zlib.h makes their prefixed names function-like macros; in parentheses the names stay the functions,
 * which the gz files call too. */
#include "zbuild.h"
#include "zlib.h"
#undef PREFIX
#define PREFIX(x) ffpsd_##x
#undef ffpsd_deflateInit2
#undef ffpsd_inflateInit2
#undef ffpsd_inflateBackInit
int32_t Z_INTERNAL
ffpsd_deflateInit2(z_stream* strm, int32_t level, int32_t method, int32_t windowBits, int32_t memLevel, int32_t strategy);
int32_t Z_INTERNAL ffpsd_inflateInit2(z_stream* strm, int32_t windowBits);
int32_t Z_INTERNAL ffpsd_inflateBackInit(z_stream* strm, int32_t windowBits, uint8_t* window);
#define ffpsd_deflateInit2 (ffpsd_deflateInit2)
#define ffpsd_inflateInit2 (ffpsd_inflateInit2)
#define ffpsd_inflateBackInit (ffpsd_inflateBackInit)

#endif
