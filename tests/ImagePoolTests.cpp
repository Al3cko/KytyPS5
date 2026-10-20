#include "graphics/host_gpu/imagePool.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
struct Image {
	uint32_t id;
	uint64_t bytes;
};

void Check(bool condition, const char* message) {
	if (!condition) {
		std::fprintf(stderr, "ImagePoolTests: %s\n", message);
		std::exit(1);
	}
}
} // namespace

int main() {
	std::vector<Image> pool;
	std::vector<uint32_t> destroyed;
	uint64_t bytes = 0;
	const auto destroy = [&](const Image& image) { destroyed.push_back(image.id); };
	const auto retain = [&](Image image, uint64_t limit = 100) {
		return Libs::Graphics::ImagePool::Retain(pool, bytes, limit, image, destroy);
	};
	Check(retain({1, 40}) && retain({2, 24}), "small transient images were not retained");
	// A streamed texture larger than the pool must not evict the targets the next
	// frame could reuse. Repeat to model retirement of successive large textures.
	for (uint32_t id = 3; id < 103; ++id) {
		Check(!retain({id, 101}), "oversized image entered the pool");
		Check(pool.size() == 2 && pool[0].id == 1 && pool[1].id == 2 && bytes == 64 && destroyed.empty(),
		      "oversized retirement discarded reusable transient images");
	}
	Check(retain({103, 60}), "fitting image was rejected");
	Check(pool.size() == 2 && pool[0].id == 2 && pool[1].id == 103 && bytes == 84 &&
	          destroyed == std::vector<uint32_t> {1},
	      "admission did not evict only the oldest necessary image");
	Check(retain({104, 100}), "image exactly matching the limit was rejected");
	Check(pool.size() == 1 && pool[0].id == 104 && bytes == 100 &&
	          destroyed == std::vector<uint32_t> {1, 2, 103},
	      "full-sized admission lost FIFO order or retained excess bytes");
	// Simulate lookup removing a retained allocation: accounting must allow the
	// next retirement to use that room without disposing the checked-out image.
	bytes -= pool.front().bytes;
	pool.erase(pool.begin());
	Check(retain({105, 20}) && bytes == 20 && destroyed.size() == 3,
	      "checkout left stale pool accounting");
	pool.clear();
	bytes = 0;
	destroyed.clear();
	Check(!retain({106, 1}, 0) && pool.empty() && bytes == 0 && destroyed.empty(),
	      "disabled capacity retained or destroyed an image owned by the caller");
	constexpr uint64_t max = std::numeric_limits<uint64_t>::max();
	Check(retain({107, max - 1}, max) && retain({108, 2}, max), "large accounting fixture was rejected");
	Check(pool.size() == 1 && pool[0].id == 108 && bytes == 2 && destroyed == std::vector<uint32_t> {107},
	      "admission overflowed byte accounting");
	using Libs::Graphics::ImagePool::BudgetGate;
	using Libs::Graphics::ImagePool::Trim;
	BudgetGate gate;
	Check(gate.Update(900, 1000, 100), "normal headroom disabled reuse");
	Check(!gate.Update(1000, 1000, 100), "budget exhaustion did not stop retention");
	Check(!gate.Update(0, 0, 100), "unknown budget released pressure gate");
	Check(!gate.Update(950, 1000, 100), "pool refilled before sufficient recovery");
	Check(gate.Update(900, 1000, 100), "recovered headroom failed to restore reuse");
	Check(!gate.Update(max, 1000, 100), "over-budget usage underflowed");
	Check(gate.Update(0, max, max), "maximum budget overflowed recovery calculation");
	Trim(pool, bytes, 0, destroy);
	Check(pool.empty() && bytes == 0 && destroyed.back() == 108, "pressure trim failed to release pool");
	destroyed.clear();
	Check(retain({201, 40}) && retain({202, 24}) && retain({203, 20}), "trim fixture failed");
	const auto checked_out = pool[1];
	bytes -= checked_out.bytes;
	pool.erase(pool.begin() + 1);
	Trim(pool, bytes, 25, destroy);
	Check(bytes == 20 && pool.size() == 1 && pool[0].id == 203 && destroyed == std::vector<uint32_t>{201},
	      "partial trim lost FIFO order or destroyed checked-out image");
	Trim(pool, bytes, 0, destroy);
	Trim(pool, bytes, 0, destroy);
	Check(bytes == 0 && pool.empty() && destroyed == std::vector<uint32_t>{201, 203},
	      "repeat trim double-destroyed or freed a checked-out image");
	std::puts("ImagePoolTests: oversized retirement, FIFO eviction, checkout and byte bounds passed");
}
