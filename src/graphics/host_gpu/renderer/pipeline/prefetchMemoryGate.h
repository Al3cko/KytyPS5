#pragma once

#include <cstdint>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>

namespace Kyty::PrefetchMemory {

// Callers serialize access. Separate pause/resume thresholds avoid starting more
// translations each time a little memory is freed. Unknown OS readings preserve
// the previous state instead of releasing a gate that already observed pressure.
class PressureGate {
public:
    explicit PressureGate(uint64_t minimum, uint64_t recovery_margin)
        : minimum_(minimum), resume_(minimum +
              (recovery_margin > std::numeric_limits<uint64_t>::max() - minimum
                   ? std::numeric_limits<uint64_t>::max() - minimum : recovery_margin)) {}

    bool Allow(uint64_t available) {
        return Allow(available, available != 0);
    }

    bool Allow(uint64_t available, bool known) {
        if (minimum_ == 0) return true;
        if (!known) return !paused_;
        paused_ = available < (paused_ ? resume_ : minimum_);
        return !paused_;
    }

private:
    uint64_t minimum_, resume_;
    bool paused_ = false;
};

// Free physical pages do not imply room for more committed allocations. Track
// both limits independently so recovery or a failed query for one cannot release
// the other's pause. A known zero is exhaustion, not an unavailable reading.
class SystemPressureGate {
public:
    explicit SystemPressureGate(uint64_t minimum, uint64_t recovery_margin)
        : physical_(minimum, recovery_margin), commit_(minimum, recovery_margin) {}

    bool Allow(uint64_t physical_available, bool physical_known,
               uint64_t commit_used, uint64_t commit_limit, bool commit_known) {
        const bool physical_ready = physical_.Allow(physical_available, physical_known);
        const uint64_t commit_room = commit_limit - std::min(commit_limit, commit_used);
        const bool commit_ready = commit_.Allow(commit_room, commit_known);
        return physical_ready && commit_ready;
    }

private:
    PressureGate physical_, commit_;
};

// Render-thread accounting: a speculative slot remains occupied while queued,
// while a worker loads it, and while its unused result is resident. Otherwise a
// succession of 256-plan batches can still reload thousands of unused plans.
class PlanPreloadBudget {
public:
    explicit PlanPreloadBudget(size_t limit) : limit_(limit) {}
    size_t Room() const { return limit_ - pending_ - unused_; }
    size_t Reserve(size_t requested) {
        const auto reserved = std::min(requested, Room());
        pending_ += reserved;
        return reserved;
    }
    void Complete(bool retained_unused) {
        assert(pending_ != 0);
        --pending_;
        unused_ += retained_unused ? 1 : 0;
    }
    void UsedOrEvicted() {
        assert(unused_ != 0);
        --unused_;
    }
private:
    size_t limit_, pending_ = 0, unused_ = 0;
};

// Background work must not dequeue a request until there is room. The render
// thread's actual demand bypasses this wait and can claim a pending program.
template <typename Stop, typename Ready, typename Wait>
bool WaitForRoom(Stop stop, Ready ready, Wait wait) {
    while (!stop()) {
        if (ready()) return !stop();
        wait();
    }
    return false;
}

} // namespace Kyty::PrefetchMemory
