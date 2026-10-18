#include "loader/demonsSoulsRenderSize.h"
#include "loader/demonsSoulsSuperResolution.h"

#include "graphics/host_gpu/hostMemory.h"
#include "graphics/host_gpu/renderer/demonsSouls.h"
#include "loader/elf.h"
#include "loader/guestCode.h"
#include "loader/runtimeLinker.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace Loader::DemonsSoulsRenderSize {
namespace {

bool Readable(uint64_t address, uint64_t size) {
	return Libs::Graphics::HostMemoryRangeIsReadable(address, size);
}

bool HoldsPair(uint64_t address, uint32_t width, uint32_t height) {
	uint32_t pair[2] {};
	if (!Readable(address, sizeof(pair))) return false;
	std::memcpy(pair, reinterpret_cast<const void*>(address), sizeof(pair));
	return pair[0] == width && pair[1] == height;
}

// The one address of a mode's render size the mode cases (`pattern`) load (both cases load the same constant).
std::optional<uint64_t> FindRenderConstant(const Program& program, std::string_view pattern, uint32_t width,
                                           uint32_t height) {
	std::vector<uint64_t> constants;
	for (const auto site: GuestCode::Find(program, GuestCode::Pattern(pattern), 64)) {
		int32_t output_displacement = 0, render_displacement = 0;
		std::memcpy(&output_displacement, reinterpret_cast<const void*>(site + 4), sizeof(int32_t));
		std::memcpy(&render_displacement, reinterpret_cast<const void*>(site + 12), sizeof(int32_t));
		const uint64_t output = site + 8 + static_cast<int64_t>(output_displacement);
		const uint64_t render = site + 16 + static_cast<int64_t>(render_displacement);
		if (HoldsPair(output, OutputWidth, OutputHeight) && HoldsPair(render, width, height) &&
		    std::find(constants.begin(), constants.end(), render) == constants.end())
			constants.push_back(render);
	}
	return constants.size() == 1 ? std::optional(constants[0]) : std::nullopt;
}

// The one place `bytes` is in the program's loaded segments.
std::optional<uint64_t> FindUniqueData(const Program& program, std::span<const uint8_t> bytes) {
	const auto* elf = program.elf.get();
	if (elf == nullptr || elf->GetEhdr() == nullptr || elf->GetPhdr() == nullptr) return std::nullopt;
	std::vector<uint64_t> found;
	const auto*           phdr = elf->GetPhdr();
	for (size_t i = 0; i < elf->GetEhdr()->e_phnum && found.size() < 2; i++) {
		if (phdr[i].p_type != PT_LOAD || phdr[i].p_filesz < bytes.size()) continue;
		const auto address = program.base_vaddr + phdr[i].p_vaddr;
		if (!Readable(address, phdr[i].p_filesz)) continue;
		const auto* begin = reinterpret_cast<const uint8_t*>(address);
		const auto* end   = begin + phdr[i].p_filesz;
		for (const auto* at = std::search(begin, end, bytes.begin(), bytes.end()); at != end && found.size() < 2;
		     at             = std::search(at + 1, end, bytes.begin(), bytes.end()))
			found.push_back(reinterpret_cast<uint64_t>(at));
	}
	return found.size() == 1 ? std::optional(found[0]) : std::nullopt;
}

double g_wide_aspect = 0.0; // the aspect KYTY_RENDER_ASPECT installed (0: 16:9)

} // namespace

double WideAspect() {
	return g_wide_aspect;
}

void Install(Program* program) {
	const char* setting = std::getenv("KYTY_RENDER_HEIGHT");
	const char* wide    = std::getenv("KYTY_RENDER_ASPECT");
	if (((setting == nullptr || *setting == 0) && (wide == nullptr || *wide == 0)) || program == nullptr ||
	    !Libs::Graphics::DemonsSouls::IsSupportedGame() || program->file_name.filename() != "eboot.bin")
		return;
	// (The game's super resolution path: display mode 5 is the performance mode, loader/demonsSoulsSuperResolution.h.)
	const bool     trinity     = DemonsSoulsSuperResolution::Active();
	const uint32_t game_width  = trinity ? TrinityWidth : RenderWidth;
	const uint32_t game_height = trinity ? TrinityHeight : RenderHeight;
	const int      mode        = trinity ? TrinityMode : PerformanceMode;
	auto requested = setting != nullptr && *setting != 0 ? static_cast<uint32_t>(std::strtoul(setting, nullptr, 10))
	                                                     : game_height;
	const auto aspect    = wide != nullptr && *wide != 0 ? ParseAspect(wide) : 0.0;
	if (wide != nullptr && *wide != 0 && aspect == 0.0)
		std::printf("Demon's Souls render size: KYTY_RENDER_ASPECT=%s is not wider than 16:9 and at most 32:9; 16:9 kept\n",
		            wide);
	if (trinity && requested >= 720 && requested < SuperResolutionLeastHeight) {
		std::printf("Demon's Souls render size: KYTY_RENDER_HEIGHT=%u is below the game's super resolution (1080 at "
		            "least); 1080 instead\n",
		            requested);
		requested = SuperResolutionLeastHeight;
	}
	if (requested == game_height && aspect == 0.0) return;
	const auto size = RenderSizeFor(requested, aspect != 0.0 ? aspect : GameAspect);
	if (size.height == 0) {
		std::printf("Demon's Souls render size: KYTY_RENDER_HEIGHT=%s is not 720-2160; the game's %ux%u kept\n", setting,
		            game_width, game_height);
		return;
	}
	const auto constant = FindRenderConstant(*program, trinity ? TrinityCase : ModeCase, game_width, game_height);
	const auto widths   = FindUniqueData(*program, std::span(reinterpret_cast<const uint8_t*>(ModeWidths), sizeof(ModeWidths)));
	const auto heights  = FindUniqueData(*program, std::span(reinterpret_cast<const uint8_t*>(ModeHeights), sizeof(ModeHeights)));
	// (The render setup's 16:9: the one such store in the code.)
	std::optional<uint64_t> aspect_store;
	if (aspect != 0.0) {
		const auto sites = GuestCode::Find(*program, GuestCode::Pattern(AspectStore), 2);
		if (sites.size() == 1) aspect_store = sites[0];
	}
	if (!constant || !widths || !heights || (aspect != 0.0 && !aspect_store)) {
		std::printf("Demon's Souls render size: the video settings of a known build not found; the game's %ux%u kept\n",
		            game_width, game_height);
		return;
	}
	// Installation occurs before module initializers or guest threads run; program memory is writable to the host.
	const uint32_t pair[2] {size.width, size.height};
	std::memcpy(reinterpret_cast<void*>(*constant), pair, sizeof(pair));
	std::memcpy(reinterpret_cast<void*>(*widths + mode * sizeof(uint32_t)), &size.width, sizeof(uint32_t));
	std::memcpy(reinterpret_cast<void*>(*heights + mode * sizeof(uint32_t)), &size.height, sizeof(uint32_t));
	if (aspect_store) {
		// The projection's aspect: the render size's own, as the game's 2560x1440 has 16/9.
		const float value = static_cast<float>(static_cast<double>(size.width) / size.height);
		std::memcpy(reinterpret_cast<void*>(*aspect_store + 3), &value, sizeof(value));
		g_wide_aspect = value;
	}
	std::printf("Demon's Souls render size: performance mode renders %ux%u instead of %ux%u (output %ux%u)%s\n", size.width,
	            size.height, game_width, game_height, OutputWidth, OutputHeight,
	            aspect_store ? ", its projection as wide" : "");
}

} // namespace Loader::DemonsSoulsRenderSize
