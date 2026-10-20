#include "writeback-verify.h"

namespace {
constexpr uint64_t Base = 0x200000000ull;
std::array<uint8_t, 2 * WritebackVerify::PageSize> backing {};
void Expect(bool pass, const char* message) {
	if (!pass) {
		std::fprintf(stderr, "WritebackVerifyTests: %s\n", message);
		std::exit(1);
	}
}
}

namespace Libs::LibKernel::Memory {
const uint8_t* TryGetBackingPointer(uint64_t address, uint64_t size) {
	if (address < Base || address - Base > backing.size() || size > backing.size() - (address - Base)) return nullptr;
	return backing.data() + (address - Base);
}
}

int main() {
	using namespace WritebackVerify;
	auto& state = GetState();
	std::array<uint8_t, 32> result {};
	Owned(Base, 1);
	Check(Base + 32, result.data(), result.size(), "test unchanged");
	Expect(state.checked == 1 && state.mismatches == 0, "unchanged page reported corruption");
	backing[40] = 0x5a; // A store that bypassed the tracking boundary.
	Check(Base + 32, result.data(), result.size(), "test changed");
	Expect(state.checked == 2 && state.mismatches == 1, "silent guest store was not detected");
	Rehash(Base + 32, result.size());
	Check(Base + 32, result.data(), result.size(), "test rehashed");
	Expect(state.mismatches == 1 && state.rehashed == 1, "accepted write-back was reported again");
	Released(Base, 1);
	backing[40] = 0x6b;
	Check(Base + 32, result.data(), result.size(), "test CPU-owned");
	Expect(state.checked == 3 && state.mismatches == 1, "released page was still monitored");
	Owned(Base, 2);
	const auto before = state.checked.load();
	Check(Base + PageSize - 16, result.data(), result.size(), "test crossing");
	Expect(state.checked == before + 2 && state.mismatches == 1, "cross-page write-back was not fully checked");
	Released(Base, 2);
	Owned(Base + backing.size(), 1);
	Expect(state.unbacked == 1, "unmapped backing was dereferenced");
	Check(Base, result.data(), 0, "test empty");
	std::puts("WritebackVerifyTests: change detection, rehash, release and page boundaries passed");
}
