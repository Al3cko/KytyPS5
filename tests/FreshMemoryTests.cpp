#include "local/fresh-memory.h"
#include <array>
#include <cstdlib>
#include <thread>
#include <tuple>
#include <vector>

namespace {
constexpr uint64_t Base = 0x100000000ull, Guest = 0x200000000ull;
constexpr uint64_t G = FreshMemory::Granule;
void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FreshMemory: %s\n", message); std::exit(1); }
}
}
// Fake address translation only; the production tracker must never dereference these pointers.
namespace Libs::LibKernel::Memory {
bool BackingView(uint64_t* base, uint64_t* size) { *base = Base; *size = 128 * G; return true; }
uint64_t BackingMappingEnd(uint64_t address) {
    if (address >= Guest && address < Guest + 4 * G) return Guest + 4 * G;
    if (address >= Guest + 8 * G && address < Guest + 12 * G) return Guest + 12 * G;
    return 0;
}
const uint8_t* TryGetBackingPointer(uint64_t address, uint64_t size) {
    const auto end = BackingMappingEnd(address);
    if (end <= address || size > end - address) return nullptr;
    const auto offset = address >= Guest + 8 * G ? address - Guest - 8 * G : address - Guest;
    return reinterpret_cast<const uint8_t*>(Base + offset);
}
}

int main() {
    using Run = std::tuple<uint64_t, uint64_t, bool>;
    std::vector<Run> runs;
    auto append = [&](uint64_t offset, uint64_t bytes, bool fresh) { runs.emplace_back(offset, bytes, fresh); };
    if (!FreshMemory::Enabled()) {
        Check(FreshMemory::Get() == nullptr, "disabled tracker allocated state");
        Check(!FreshMemory::ForEachBackingRun(reinterpret_cast<const uint8_t*>(Base), G, append) && runs.empty(),
              "disabled tracker changed the upload path");
        std::puts("FreshMemory: disabled fallback passed");
        return 0;
    }
    auto& state = *FreshMemory::Get();
    Check(FreshMemory::ForEachBackingRun(reinterpret_cast<const uint8_t*>(Base + 17), 3 * G, append), "enumerate");
    Check(runs == std::vector<Run>{{0, 3 * G, true}}, "untouched runs not coalesced");
    FreshMemory::MarkBacking(Base + G - 1, 2);
    runs.clear();
    FreshMemory::ForEachBackingRun(reinterpret_cast<const uint8_t*>(Base), 3 * G, append);
    Check(runs == std::vector<Run>{{0, 2 * G, false}, {2 * G, G, true}}, "cross-granule write lost bytes");
    FreshMemory::MarkGuestRange(Guest + 8 * G + 2 * G + 9, 1);
    Check(FreshMemory::IsWritten(state, 2), "alias write did not mark shared backing");
    FreshMemory::SetArmed(state, 3, true);
    FreshMemory::MarkArmedBacking(Base + 3 * G, 2 * G);
    Check(FreshMemory::IsWritten(state, 3) && !FreshMemory::IsWritten(state, 4), "remap did not preserve armed state");
    FreshMemory::SetArmed(state, 3, false);
    FreshMemory::MarkGuestRange(Guest + 4 * G, G); // hole
    Check(!FreshMemory::IsWritten(state, 4), "hole marked unrelated backing");
    runs.clear();
    FreshMemory::ForEachRun(Guest + 3 * G, 2 * G, append);
    Check(runs == std::vector<Run>{{0, 2 * G, false}}, "unmapped bytes classified as fresh");
    FreshMemory::MarkBacking(Base + 64 * G - 1, 2);
    Check(FreshMemory::IsWritten(state, 63) && FreshMemory::IsWritten(state, 64) &&
          !FreshMemory::IsWritten(state, 62) && !FreshMemory::IsWritten(state, 65), "bitmap-word boundary");
    std::array<std::thread, 8> workers;
    for (auto& worker : workers) worker = std::thread([&] { for (uint64_t g = 70; g < 100; ++g) FreshMemory::MarkGranule(state, g); });
    for (auto& worker : workers) worker.join();
    for (uint64_t g = 70; g < 100; ++g) Check(FreshMemory::IsWritten(state, g), "concurrent mark lost a bit");
    runs.clear();
    FreshMemory::ForEachBackingRun(reinterpret_cast<const uint8_t*>(Base + 128 * G), G, append);
    Check(runs == std::vector<Run>{{0, G, false}}, "out-of-store bytes classified as zero");
    Check(FreshMemory::IsWritten(state, UINT64_MAX) && !FreshMemory::MarkGranule(state, UINT64_MAX), "invalid granule");
    std::puts("FreshMemory: boundary, alias, remap, hole and concurrent marking cases passed");
}
