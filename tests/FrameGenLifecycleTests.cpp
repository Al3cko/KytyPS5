#include "local/frame-gen-lifecycle.h"

#include <cstdio>
#include <cstdlib>

static void Require(bool value) {
	if (!value) std::abort();
}

int main() {
	using Libs::Graphics::FrameGen::PresentationHistory;
	using Libs::Graphics::FrameGen::InputAllocationBackoff;
	PresentationHistory history;
	Require(!history.Next(false, 0).generate && history.Next(false, 0).reset);
	// Loading -> gameplay: collect a short valid streak before resuming generation.
	for (uint64_t frame = 1; frame <= 10; ++frame) {
		const auto next = history.Next(true, frame);
		Require(next.generate == (frame == 10));
		Require(next.reset == (frame == 1));
		history.Presented(true, true, frame);
	}
	Require(history.Next(true, 11).generate && !history.Next(true, 11).reset);
	// A dropped or repeated guest frame cannot reuse motion history from a
	// nonadjacent frame, even if both inputs individually look valid.
	Require(history.Next(true, 12).generate && history.Next(true, 12).reset);
	history.Presented(true, true, 12);
	Require(history.Next(true, 12).reset);
	Require(!history.Next(true, 13).reset);
	// Immediately suppress generation on a menu/reused/invalid-input frame.
	Require(!history.Next(false, 0).generate && history.Next(false, 0).reset);
	history.Presented(true, false, 0);
	Require(!history.Next(true, 14).generate && history.Next(true, 14).reset);
	// Failed presents must not make a ten-frame recovery streak out of one frame.
	for (unsigned retry = 0; retry < 20; ++retry) {
		Require(!history.Next(true, 14).generate);
		history.Presented(false, true, 14);
	}
	for (uint64_t frame = 14; frame < 23; ++frame) {
		Require(!history.Next(true, frame).generate);
		history.Presented(true, true, frame);
	}
	Require(history.Next(true, 23).generate);
	// Failure on an otherwise continuous frame forces the retry to reset history.
	Require(!history.Next(true, 23).reset);
	history.Presented(false, true, 23);
	Require(history.Next(true, 23).reset);
	history.Presented(true, true, 23);
	Require(!history.Next(true, 24).reset);
	// Missing inputs turn FG off before token acquisition. A failed token must
	// still break the old completed streak and require ten good presents again.
	history.Presented(false, false, 0);
	for (uint64_t frame = 24; frame <= 33; ++frame) {
		Require(history.Next(true, frame).generate == (frame == 33));
		history.Presented(true, true, frame);
	}
	// Swapchain recreation (including while already off) discards old history.
	history.Reset();
	Require(!history.Next(true, 24).generate && history.Next(true, 24).reset);
	history.Reset();
	Require(!history.Next(false, 0).generate && history.Next(false, 0).reset);
	// Input allocation failure suspends allocation attempts across ring slots;
	// real-frame presentation breaks the old FG streak during that interval.
	InputAllocationBackoff allocation;
	Require(allocation.MayAttempt(100));
	allocation.Failed(100);
	for (uint64_t frame = 100; frame < 220; ++frame) {
		Require(!allocation.MayAttempt(frame));
		Require(!history.Next(false, frame).generate);
		history.Presented(true, false, frame);
	}
	Require(allocation.MayAttempt(220));
	allocation.Failed(220); // a second allocation failure starts a new delay
	Require(!allocation.MayAttempt(339) && allocation.MayAttempt(340));
	allocation.Succeeded();
	for (uint64_t frame = 340; frame < 350; ++frame) {
		Require(allocation.MayAttempt(frame));
		Require(history.Next(true, frame).generate == (frame == 349));
		history.Presented(true, true, frame);
	}
	// Unsigned elapsed-frame arithmetic also handles the counter rolling over.
	allocation.Failed(UINT64_MAX - 59);
	Require(!allocation.MayAttempt(59) && allocation.MayAttempt(60));
	std::puts("Frame-generation lifecycle and allocation backoff/recovery passed");
}
