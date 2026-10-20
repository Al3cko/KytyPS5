#include "loader/demonsSoulsCopy.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
#if defined(__x86_64__) || defined(_M_X64)
#ifndef XBYAK_NO_EXCEPTION
#define XBYAK_NO_EXCEPTION
#endif
#include <xbyak/xbyak.h>
#endif
namespace {
using namespace Loader::DemonsSoulsCopy;
uint64_t calls = 0, prepared_address = 0, prepared_size = 0;
bool     accept = true;
bool     Prepare(uint64_t address, uint64_t size) {
    ++calls;
    prepared_address = address;
    prepared_size    = size;
    return accept;
}
void Check(bool ok) {
	if (!ok) std::abort();
}
void* KYTY_SYSV_ABI Bridge(void* to, const void* from, size_t size) {
	return Move(to, from, size, Prepare);
}
uint64_t flush_calls = 0;
bool flush_accepts = true;
bool Flush(uint64_t, uint64_t) {
	++flush_calls;
	return flush_accepts;
}
void CheckHookLifetime() {
	CopyHook hook;
	int first = 0, second = 0;
	const auto* owner = reinterpret_cast<const Loader::Program*>(&first);
	const auto* other = reinterpret_cast<const Loader::Program*>(&second);
	std::array<uint8_t, 13> code {};
	code.fill(0x90);
	const auto original = code;
	const auto patch = Tailcall(reinterpret_cast<uint64_t>(&Bridge));
	const auto address = reinterpret_cast<uint64_t>(code.data());
	Check(!hook.Install(nullptr, address, patch, Flush));
	Check(!hook.Install(owner, 0, patch, Flush));
	Check(hook.Install(owner, address, patch, Flush) && code == patch && hook.Active());
	Check(!hook.Install(other, address, patch, Flush));
	hook.Clear(Flush, other);
	Check(code == patch && hook.Active() && flush_calls == 1);
	hook.Clear(Flush, owner);
	Check(code == original && !hook.Active() && flush_calls == 2);
	// Reuse the released address with bytes identical to the old patch. A stale
	// global hook would overwrite these unrelated bytes with its saved original.
	code = patch;
	hook.Clear(Flush);
	Check(code == patch && flush_calls == 2);
	code = original;
	Check(hook.Install(other, address, patch, Flush));
	hook.Clear(Flush, owner);
	Check(hook.Active() && code == patch);
	hook.Clear(Flush);
	Check(!hook.Active() && code == original && flush_calls == 4);
	Check(hook.Install(owner, address, patch, Flush));
	code.fill(0xcc); // Another patch superseded ours; preserve it on unload.
	const auto replacement = code;
	hook.Clear(Flush, owner);
	Check(!hook.Active() && code == replacement && flush_calls == 5);
	flush_accepts = false;
	Check(!hook.Install(owner, address, patch, Flush));
	Check(!hook.Active() && code == replacement && flush_calls == 7);
	hook.Clear(Flush);
	Check(code == replacement && flush_calls == 7);
	flush_accepts = true;
}
} // namespace
int main() {
	using namespace Loader::DemonsSoulsCopy;
	Check(!Enabled(nullptr));
	for (const char* value: {"", "0", "false", "true", "2", "01", "1 ", " 1"}) Check(!Enabled(value));
	Check(Enabled("1"));
	CheckHookLifetime();
	for (const size_t size:
	     {size_t {0}, size_t {17}, size_t {262143}, size_t {262144}, size_t {300000}})
		for (const size_t displacement: {size_t {0}, size_t {1}, size_t {31}, size_t {400000}})
			for (const bool reverse: {false, true})
				for (const bool prepare_accepts: {false, true}) {
					std::vector<uint8_t> expected(size + displacement + 64), actual;
					for (size_t i = 0; i < expected.size(); ++i)
						expected[i] = static_cast<uint8_t>((i * 37) ^ (i >> 8));
					actual                 = expected;
					const auto source      = reverse ? displacement + 16 : 16;
					const auto destination = reverse ? 16 : displacement + 16;
					std::memmove(expected.data() + destination, expected.data() + source, size);
					calls  = 0;
					accept = prepare_accepts;
					Check(Bridge(actual.data() + destination, actual.data() + source, size) ==
					      actual.data() + destination);
					Check(actual == expected);
					Check(calls == uint64_t(size >= 262144 && displacement != 0));
					if (calls)
						Check(prepared_address ==
						          reinterpret_cast<uint64_t>(actual.data() + destination) &&
						      prepared_size == size);
				}
#if defined(__x86_64__) || defined(_M_X64)
	Xbyak::CodeGenerator code(128);
	code.push(Xbyak::util::rbp);
	code.mov(Xbyak::util::rbp, Xbyak::util::rsp);
	const auto tail = Tailcall(reinterpret_cast<uint64_t>(&Bridge));
	code.db(tail.data(), tail.size());
	code.ready();
	using Function = void*(KYTY_SYSV_ABI*)(void*, const void*, size_t);
	char output[32] {}, input[32] = "memmove ABI";
	Check(code.getCode<Function>()(output, input, sizeof(input)) == output);
	Check(std::memcmp(output, input, sizeof(input)) == 0);
#endif
	std::puts("DemonsSoulsCopyTests: all cases passed");
}
