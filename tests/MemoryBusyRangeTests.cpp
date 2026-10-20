#include "kernel/memoryBusyRange.h"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using Libs::LibKernel::Memory::SharesBusyPageTable;
using Libs::LibKernel::Memory::BusyRangeConflicts;
using Libs::LibKernel::Memory::BusyRangeOperation;
using Libs::LibKernel::Memory::ProtectSerializationEnabled;

static void Check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

int main() {
    constexpr uint64_t MiB = 1024u * 1024u;
    constexpr uint64_t Page = 4096u;
    // Separate views still share one Windows page-table page.
    Check(SharesBusyPageTable(4 * MiB + Page, 4 * MiB + 2 * Page, 4 * MiB, Page),
          "neighbour before a busy view was not excluded");
    Check(SharesBusyPageTable(4 * MiB, 4 * MiB + Page, 6 * MiB - Page, Page),
          "neighbour after a busy view was not excluded");
    Check(!SharesBusyPageTable(4 * MiB, 4 * MiB + Page, 6 * MiB, Page),
          "unrelated page-table page was unnecessarily excluded");
    Check(SharesBusyPageTable(4 * MiB, 6 * MiB, 6 * MiB, Page),
          "exact end boundary did not protect the following table page");
    Check(!SharesBusyPageTable(4 * MiB, 6 * MiB, 8 * MiB, Page),
          "end-boundary protection extended past the following page");
    Check(SharesBusyPageTable(4 * MiB, 4 * MiB + Page, 4 * MiB - Page, 2 * Page),
          "query spanning into busy table page was missed");
    Check(!SharesBusyPageTable(4 * MiB, 4 * MiB + Page, 4 * MiB - Page, Page),
          "query ending before the busy page was included");
    Check(!SharesBusyPageTable(4 * MiB, 4 * MiB + Page, 4 * MiB, 0),
          "empty query was included");
    Check(!SharesBusyPageTable(4 * MiB, 4 * MiB, 4 * MiB, Page),
          "empty busy interval was included");
    Check(SharesBusyPageTable(UINT64_MAX - Page, UINT64_MAX, UINT64_MAX - 1, 1),
          "upper address limit wrapped during table expansion");
    Check(!SharesBusyPageTable(UINT64_MAX - Page, UINT64_MAX, 0, Page),
          "upper address limit falsely overlaps address zero");
    Check(SharesBusyPageTable(32 * MiB, 48 * MiB, 16 * MiB, 16 * MiB, true),
          "incoming map ending at active trim start was not excluded");
    Check(SharesBusyPageTable(16 * MiB, 32 * MiB, 32 * MiB, 16 * MiB, true),
          "reverse map/trim ordering was not excluded");
    Check(!SharesBusyPageTable(32 * MiB, 48 * MiB, 16 * MiB, 14 * MiB, true),
          "nonoverlapping mutation footprints were included");
    Check(SharesBusyPageTable(0, Page, UINT64_MAX - Page + 1, Page, true),
          "overflowing mutation interval was not conservatively excluded");
    // A distant view of the same section can be mapped or trimmed while a
    // tracker changes protections. Only protection queries wait globally.
    const auto conflicts = [&](BusyRangeOperation operation, bool serial) {
        return BusyRangeConflicts(32 * MiB, 48 * MiB, 4 * MiB, Page, operation, serial);
    };
    Check(conflicts(BusyRangeOperation::Protection, true),
          "serialized protection did not wait for a distant busy view");
    Check(!conflicts(BusyRangeOperation::Protection, false),
          "opt-out still serialized protection of an unrelated page table");
    for (const bool serial : {false, true}) {
        Check(!conflicts(BusyRangeOperation::Mutation, serial),
              "protection policy serialized unrelated map/unmap/trim operations");
        Check(!conflicts(BusyRangeOperation::Lookup, serial),
              "protection policy serialized unrelated ownership lookups");
        Check(BusyRangeConflicts(32 * MiB, 48 * MiB, 16 * MiB, 16 * MiB,
                                 BusyRangeOperation::Mutation, serial),
              "map/trim boundary exclusion was lost under the protection policy");
        Check(!BusyRangeConflicts(32 * MiB, 48 * MiB, 16 * MiB, 16 * MiB,
                                  BusyRangeOperation::Lookup, serial),
              "lookup inherited the mutation boundary expansion");
        Check(BusyRangeConflicts(4 * MiB, 6 * MiB, 6 * MiB, Page,
                                 BusyRangeOperation::Protection, serial),
              "protection opt-out lost the existing page-table boundary exclusion");
        Check(BusyRangeConflicts(0, Page, UINT64_MAX - Page + 1, Page,
                                 BusyRangeOperation::Mutation, serial),
              "mutation overflow guard changed under the protection policy");
    }
    Check(!BusyRangeConflicts(32 * MiB, 48 * MiB, 4 * MiB, 0,
                              BusyRangeOperation::Protection, true),
          "empty protection query waited globally");
    Check(!BusyRangeConflicts(32 * MiB, 32 * MiB, 4 * MiB, Page,
                              BusyRangeOperation::Protection, true),
          "empty busy interval blocked serialized protection");
    Check(ProtectSerializationEnabled(nullptr) && ProtectSerializationEnabled("") &&
              ProtectSerializationEnabled("1"),
          "protection serialization was not enabled by default");
    Check(!ProtectSerializationEnabled("0") && !ProtectSerializationEnabled("0anything"),
          "protection serialization opt-out differs from the upstream switch");
    std::puts("MemoryBusyRangeTests: boundary and protection-policy cases passed (no OS memory stress)");
}
