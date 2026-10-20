#ifndef EMULATOR_CACHE_GC_POLICY_H_
#define EMULATOR_CACHE_GC_POLICY_H_

#include <algorithm>
#include <cstdint>

namespace Libs::Graphics::CacheGcPolicy {

struct BufferLimits {
	uint64_t trigger;
	uint64_t critical;
};

// Budget already includes GraphicContext's driver headroom. Preserve the startup
// collection policy, lowering its limits if the OS reduces the available budget.
constexpr BufferLimits ForBudget(BufferLimits startup, uint64_t budget) {
	if (budget == 0) return startup; // unavailable reading, not a zero-sized GPU

	constexpr uint64_t MiB = 1024ull * 1024;
	const auto live_trigger = budget > 2560 * MiB ? budget - 1536 * MiB : std::min(budget, 1024 * MiB);
	const auto live_critical = budget > 1536 * MiB ? budget - 512 * MiB : std::min(budget, 1024 * MiB);
	const auto trigger = std::min(startup.trigger, live_trigger);
	const auto separation = std::min(budget - trigger, 256 * MiB);
	return {trigger, std::min(budget, std::max(std::min(startup.critical, live_critical), trigger + separation))};
}

} // namespace Libs::Graphics::CacheGcPolicy

#endif
