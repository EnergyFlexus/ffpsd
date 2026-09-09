#ifndef FFPSD_FFPSD_HPP_
#define FFPSD_FFPSD_HPP_

#include <ffpsd/adjustments.hpp>
#include <ffpsd/document.hpp>
#include <ffpsd/export.h>
#include <ffpsd/image.hpp>
#include <ffpsd/image_resources.hpp>
#include <ffpsd/layer.hpp>
#include <ffpsd/tagged_block.hpp>
#include <ffpsd/types.hpp>

#if defined(FFPSD_HAS_PNG)
#include <ffpsd/png.hpp>
#endif

namespace ffpsd
{
    // Library version as "MAJOR.MINOR.PATCH".
    FFPSD_EXPORT const char* Version() noexcept;
} // namespace ffpsd

#endif // FFPSD_FFPSD_HPP_
