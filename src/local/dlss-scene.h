#pragma once

#include "loader/demonsSoulsRenderSize.h"

#include <cstdint>

namespace Libs::Graphics::FrameGen {

// Only the known Performance-mode scene sizes, including the render-height
// patch. Cinematic, output-sized UI, arbitrary aspect ratios and volume images
// must not be mistaken for the post-TAA scene.
inline bool IsSupportedSrScene(uint32_t width, uint32_t height, uint32_t depth) {
	const auto expected = Loader::DemonsSoulsRenderSize::RenderSizeFor(height);
	return depth == 1 && height <= 1440 && expected.height != 0 && width == expected.width;
}

// Query the installed SDK rather than assuming its accepted dynamic-resolution
// range. A malformed/empty range cannot authorize an evaluation.
constexpr bool SrRenderFits(uint32_t width, uint32_t height, uint32_t min_width, uint32_t min_height,
                            uint32_t max_width, uint32_t max_height) {
	return min_width != 0 && min_height != 0 && min_width <= max_width && min_height <= max_height &&
	       width >= min_width && width <= max_width && height >= min_height && height <= max_height;
}

} // namespace Libs::Graphics::FrameGen
