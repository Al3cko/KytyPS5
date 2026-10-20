#include "graphics/host_gpu/renderer/cache/cacheGcPolicy.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

using Libs::Graphics::CacheGcPolicy::BufferLimits;
using Libs::Graphics::CacheGcPolicy::ForBudget;

void Check(bool condition, const char* message) {
	if (!condition) {
		std::fprintf(stderr, "CacheGcPolicyTests: %s\n", message);
		std::exit(1);
	}
}

int main() {
	constexpr uint64_t MiB = 1024ull * 1024;
	constexpr BufferLimits startup {9000 * MiB, 12000 * MiB};
	const auto initial = ForBudget(startup, 14000 * MiB);
	Check(initial.trigger == startup.trigger && initial.critical == startup.critical,
	      "adequate budget changed the startup policy");
	const auto reduced = ForBudget(startup, 8000 * MiB);
	Check(reduced.trigger == 6464 * MiB && reduced.critical == 7488 * MiB,
	      "a falling budget failed to move collection below the new limit");
	const auto restored = ForBudget(startup, 14000 * MiB);
	Check(restored.trigger == initial.trigger && restored.critical == initial.critical,
	      "a recovered budget retained unnecessarily aggressive limits");
	const auto missing = ForBudget(startup, 0);
	Check(missing.trigger == startup.trigger && missing.critical == startup.critical,
	      "an unavailable budget discarded startup limits");
	for (uint64_t budget_mib = 1; budget_mib <= 32768; ++budget_mib) {
		const auto budget = budget_mib * MiB;
		const auto limits = ForBudget(startup, budget);
		Check(limits.trigger <= limits.critical && limits.critical <= budget,
		      "collection thresholds exceeded the reported budget");
		Check(limits.trigger <= startup.trigger, "adaptive policy delayed initial collection");
	}
	const auto huge = ForBudget({std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::max()},
	                            std::numeric_limits<uint64_t>::max());
	Check(huge.trigger == std::numeric_limits<uint64_t>::max() - 1536 * MiB &&
	          huge.critical == std::numeric_limits<uint64_t>::max() - 512 * MiB,
	      "large budget arithmetic overflowed");
	std::puts("CacheGcPolicyTests: budget drop/recovery and 32768 bounded budgets passed");
}
