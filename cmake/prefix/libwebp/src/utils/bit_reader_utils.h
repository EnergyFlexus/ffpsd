/* Found before libwebp's own header: that one makes VP8GetValue and VP8GetSignedValue function-like macros,
 * which replace their renames and leave the declarations unprefixed, so both are renamed and declared again. */
#include "../../../../../vendor/libwebp/src/utils/bit_reader_utils.h"

#if !(BITTRACE > 0)
#undef VP8GetValue
#undef VP8GetSignedValue
#define VP8GetValue(BR, N, L) ffpsd_VP8GetValue(BR, N)
#define VP8GetSignedValue(BR, N, L) ffpsd_VP8GetSignedValue(BR, N)
uint32_t ffpsd_VP8GetValue(VP8BitReader* const br, int num_bits);
int32_t ffpsd_VP8GetSignedValue(VP8BitReader* const br, int num_bits);
#endif
