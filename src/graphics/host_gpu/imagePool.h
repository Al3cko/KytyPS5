#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Libs::Graphics::ImagePool {

// Stop retaining retired allocations at the existing GC budget. Resume only
// when the whole normal pool fits in the headroom, avoiding immediate refill
// after a trim. A missing budget preserves the previous state.
class BudgetGate {
public:
	bool Update(uint64_t usage, uint64_t budget, uint64_t capacity) {
		if (budget != 0) {
			if (usage >= budget) suspended_ = true;
			else if (budget - usage >= capacity) suspended_ = false;
		}
		return !suspended_;
	}
	bool AllowsRetention() const { return !suspended_; }
private:
	bool suspended_ = false;
};

// All entries are already retired and available for reuse. Checked-out images
// are absent from this vector and must never be destroyed by a pool trim.
template <typename Image, typename Destroy>
void Trim(std::vector<Image>& pool, uint64_t& pool_bytes, uint64_t limit, Destroy&& destroy) {
	std::size_t evicted = 0;
	while (pool_bytes > limit && evicted < pool.size()) {
		const auto& oldest = pool[evicted++];
		destroy(oldest);
		pool_bytes -= oldest.bytes;
	}
	pool.erase(pool.begin(), pool.begin() + evicted);
}

// Caller holds the pool lock. An image that cannot fit must leave reusable images
// alone; inserting it and then trimming would discard the entire pool for nothing.
template <typename Image, typename Destroy>
bool Retain(std::vector<Image>& pool, uint64_t& pool_bytes, uint64_t limit, Image image, Destroy&& destroy) {
	if (image.bytes > limit) return false;
	pool.push_back(image);
	std::size_t evicted = 0;
	// Subtract before adding so even a limit near UINT64_MAX cannot wrap accounting.
	while (pool_bytes > limit - image.bytes) {
		const auto& oldest = pool[evicted++];
		destroy(oldest);
		pool_bytes -= oldest.bytes;
	}
	pool.erase(pool.begin(), pool.begin() + evicted);
	pool_bytes += image.bytes;
	return true;
}

} // namespace Libs::Graphics::ImagePool
