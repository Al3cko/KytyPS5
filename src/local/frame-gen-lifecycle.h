#pragma once

#include <algorithm>
#include <cstdint>

namespace Libs::Graphics::FrameGen {

// Renderer-thread gate: after an input allocation fails, keep presenting real
// frames without attempting the same allocation on every frame or ring slot.
class InputAllocationBackoff {
public:
	static constexpr uint64_t RetryFrames = 120;
	[[nodiscard]] bool MayAttempt(uint64_t guest_frame) const {
		return !failed || guest_frame - failed_frame >= RetryFrames;
	}
	void Failed(uint64_t guest_frame) { failed = true; failed_frame = guest_frame; }
	void Succeeded() { failed = false; }

private:
	uint64_t failed_frame = 0;
	bool failed = false;
};

// Present-thread state. An attempted present is committed only after it succeeds,
// so a swapchain retry cannot advance the input streak or interpolation history.
class PresentationHistory {
public:
	struct Decision {
		bool generate;
		bool reset;
	};
	static constexpr uint32_t ResumeFrames = 10;

	[[nodiscard]] Decision Next(bool usable, uint64_t guest_frame) const {
		const bool contiguous = last_guest_frame != 0 && guest_frame == last_guest_frame + 1;
		return {usable && valid_frames >= ResumeFrames - 1, !usable || reset_pending || !contiguous};
	}

	void Presented(bool success, bool usable, uint64_t guest_frame) {
		// BeforePresent already turned generation off for missing inputs. Even
		// a failed token/present must break the streak before it can resume.
		if (!usable) {
			Reset();
			return;
		}
		if (!success) {
			reset_pending = true;
			return;
		}
		valid_frames = std::min(valid_frames + 1, ResumeFrames);
		last_guest_frame = guest_frame;
		reset_pending = false;
	}

	void Reset() { *this = {}; }

private:
	uint64_t last_guest_frame = 0;
	uint32_t valid_frames = 0;
	bool reset_pending = true;
};

} // namespace Libs::Graphics::FrameGen
