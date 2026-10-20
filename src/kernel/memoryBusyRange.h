#pragma once

#include <cstdint>

namespace Libs::LibKernel::Memory {

enum class BusyRangeOperation { Lookup, Protection, Mutation };

// Match the upstream switch: enabled by default; a leading '0' opts out.
constexpr bool ProtectSerializationEnabled(const char* value) {
    return value == nullptr || *value != '0';
}

// Windows page-table coverage for a busy half-open address interval. Include
// the following table page when the interval ends exactly on its boundary.
// Divide first to avoid overflow while extending the upper boundary.
constexpr bool SharesBusyPageTable(uint64_t busy_begin, uint64_t busy_end,
                                   uint64_t address, uint64_t size,
                                   bool changing_range = false) {
    constexpr uint64_t TableSpan = uint64_t{2} << 20u;
    if (busy_end <= busy_begin || size == 0) { return false; }
    if (size - 1u > UINT64_MAX - address) { return true; }
    // Incoming map/unmap/trim operations also touch the boundary table page.
    // Protection-only queries retain their ordinary half-open interval.
    if (changing_range && size > UINT64_MAX - address) { return true; }
    const auto first = busy_begin / TableSpan;
    const auto last = busy_end / TableSpan;
    return address / TableSpan <= last &&
           (address + size - (changing_range ? 0u : 1u)) / TableSpan >= first;
}

// Guest views share a backing section even when their Windows page-table pages
// differ. Protection changes therefore wait for every active map/unmap/trim.
// Lookups and incoming mutations keep their existing range-local exclusions.
constexpr bool BusyRangeConflicts(uint64_t busy_begin, uint64_t busy_end,
                                  uint64_t address, uint64_t size,
                                  BusyRangeOperation operation, bool serialize_protection) {
    if (busy_end <= busy_begin || size == 0) { return false; }
    if (operation == BusyRangeOperation::Protection && serialize_protection) { return true; }
    return SharesBusyPageTable(busy_begin, busy_end, address, size,
                               operation == BusyRangeOperation::Mutation);
}

} // namespace Libs::LibKernel::Memory
