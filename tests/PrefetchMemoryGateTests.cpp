#include "graphics/host_gpu/renderer/pipeline/prefetchMemoryGate.h"

#include <cstdio>
#include <cstdlib>
#include <deque>

static void Require(bool condition) {
    if (!condition) std::abort();
}

int main() {
    using namespace Kyty::PrefetchMemory;
    PressureGate gate(2048, 512);
    Require(gate.Allow(0)); // no initial OS reading: preserve normal operation
    Require(gate.Allow(2048));
    Require(!gate.Allow(2047));
    Require(!gate.Allow(0)); // a failed query cannot undo observed pressure
    Require(!gate.Allow(2048));
    Require(!gate.Allow(2559));
    Require(gate.Allow(2560));
    Require(gate.Allow(2200)); // no oscillation between pause and resume thresholds
    Require(!gate.Allow(1000));
    Require(!gate.Allow(2400));
    Require(gate.Allow(3000));
    PressureGate disabled(0, 512);
    Require(disabled.Allow(1) && disabled.Allow(0));
    PressureGate saturated(UINT64_MAX - 1, 512);
    Require(!saturated.Allow(1));
    Require(!saturated.Allow(UINT64_MAX - 1));
    Require(saturated.Allow(UINT64_MAX));

    // Enough physical RAM but little system commit room must still pause work.
    SystemPressureGate system(2048, 512);
    Require(system.Allow(4096, true, 4000, 8000, true));
    Require(!system.Allow(4096, true, 6500, 8000, true));
    Require(!system.Allow(8192, true, 0, 0, false)); // failed commit query preserves pause
    Require(!system.Allow(8192, true, 5600, 8000, true)); // recovery margin
    Require(system.Allow(8192, true, 5440, 8000, true));
    Require(!system.Allow(0, true, 4000, 8000, true)); // known zero RAM
    Require(!system.Allow(0, false, 4000, 8000, true));
    Require(!system.Allow(4096, true, 8000, 8000, true)); // exactly exhausted commit
    Require(!system.Allow(4096, true, UINT64_MAX, 8000, true)); // no underflow
    Require(system.Allow(4096, true, 4000, 8000, true));
    SystemPressureGate both(2048, 512);
    Require(!both.Allow(100, true, 7900, 8000, true));
    Require(!both.Allow(4096, true, 0, 0, false)); // commit updated even while RAM blocked
    Require(both.Allow(4096, true, 4000, 8000, true));
    SystemPressureGate physical_only(2048, 512);
    Require(physical_only.Allow(4096, true, 0, 0, false)); // platform lacks commit query
    Require(!physical_only.Allow(1024, true, 0, 0, false));
    SystemPressureGate no_gate(0, 512);
    Require(no_gate.Allow(0, true, 8000, 8000, true));

    // While prediction is blocked the render thread can claim pending work.
    // The worker must not dequeue or lose its neighbor before memory recovers.
    std::deque<int> requests {1, 2};
    bool stopped = false;
    uint64_t available = 1024;
    unsigned waits = 0, queries = 0;
    PressureGate waiting_gate(2048, 512);
    auto stop = [&] { return stopped; };
    auto ready = [&] { ++queries; return waiting_gate.Allow(available); };
    Require(WaitForRoom(stop, ready, [&] {
        ++waits;
        if (waits == 1) {
            Require(requests.front() == 1);
            requests.pop_front(); // actual render demand remains possible
            available = 2048; // still below recovery threshold
        } else {
            Require(waits == 2 && requests.front() == 2);
            available = 2560;
        }
    }));
    Require(waits == 2 && requests.front() == 2);
    requests.pop_front();
    Require(requests.empty());

    // Cancellation while waiting neither consumes another item nor queries again.
    available = 512;
    Require(!WaitForRoom(stop, ready, [&] { stopped = true; }));
    const auto before = queries;
    Require(!WaitForRoom(stop, ready, [] { std::abort(); }));
    Require(queries == before);
    stopped = false;
    Require(!WaitForRoom(stop, [&] { stopped = true; return true; }, [] { std::abort(); }));

    // Bound the whole speculative lifecycle, not just one submitted batch.
    PlanPreloadBudget budget(256);
    Require(budget.Reserve(300) == 256 && budget.Room() == 0);
    for (unsigned i = 0; i < 256; ++i) budget.Complete(true);
    for (unsigned i = 0; i < 20; ++i) Require(budget.Reserve(256) == 0);
    budget.UsedOrEvicted();
    Require(budget.Reserve(4) == 1);
    budget.Complete(false); // canceled/failed/stale result releases its slot
    Require(budget.Room() == 1);
    Require(budget.Reserve(1) == 1);
    budget.Complete(false); // synchronous demand consumes it without speculative residency
    for (unsigned i = 0; i < 255; ++i) budget.UsedOrEvicted();
    Require(budget.Room() == 256);
    // A canceled epoch and a replacement can finish independently, even if
    // the allocator reused an entry's address: each owns a separate slot.
    Require(budget.Reserve(2) == 2);
    budget.Complete(false);
    budget.Complete(true);
    Require(budget.Room() == 255);
    budget.UsedOrEvicted();
    Require(budget.Room() == 256);
    PlanPreloadBudget empty(0);
    Require(empty.Reserve(1) == 0);
    std::puts("Shader pressure policy: hysteresis, unknown readings, cancellation, demand bypass and bounded preload lifecycle passed");
}
