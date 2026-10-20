#pragma once
#include "common/abi.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Loader {
struct Program;
namespace DemonsSoulsCopy {
void Install(Program* program);
void Clear(const Program* program = nullptr);
inline bool Enabled(const char* value) {
	return value && std::strcmp(value, "1") == 0;
}

// The loader clears the owning module's patch before releasing its mapping. A later
// global clear must never inspect an address that has been freed or reused.
class CopyHook {
public:
	using Flush = bool (*)(uint64_t, uint64_t);
	[[nodiscard]] bool Active() const { return m_site != 0; }
	bool Install(const Program* owner, uint64_t address, const std::array<uint8_t, 13>& patch, Flush flush) {
		if (Active() || !owner || !address) return false;
		std::memcpy(m_original.data(), reinterpret_cast<const void*>(address), m_original.size());
		std::memcpy(reinterpret_cast<void*>(address), patch.data(), patch.size());
		if (!flush(address, patch.size())) {
			std::memcpy(reinterpret_cast<void*>(address), m_original.data(), m_original.size());
			(void)flush(address, m_original.size());
			return false;
		}
		m_patch = patch;
		m_owner = owner;
		m_site = address;
		return true;
	}
	void Clear(Flush flush, const Program* owner = nullptr) {
		if (!Active() || (owner && owner != m_owner)) return;
		if (std::memcmp(reinterpret_cast<const void*>(m_site), m_patch.data(), m_patch.size()) == 0) {
			std::memcpy(reinterpret_cast<void*>(m_site), m_original.data(), m_original.size());
			(void)flush(m_site, m_original.size());
		}
		m_site = 0;
		m_owner = nullptr;
	}

private:
	const Program* m_owner = nullptr;
	uint64_t m_site = 0;
	std::array<uint8_t, 13> m_original {}, m_patch {};
};

using PrepareWrite = bool (*)(uint64_t, uint64_t);
inline void* Move(void* destination, const void* source, size_t size, PrepareWrite prepare) {
	if (size == 0 || destination == source) return destination;
	// Preserve the normal mapping/fault path after preparing a complete large write.
	if (size >= 256 * 1024 && prepare) (void)prepare(reinterpret_cast<uint64_t>(destination), size);
	return std::memmove(destination, source, size);
}
inline std::array<uint8_t, 13> Tailcall(uint64_t function) {
	std::array<uint8_t, 13> code {0x5d, 0x48, 0xb8}; // pop rbp; mov rax, function; jmp rax
	std::memcpy(code.data() + 3, &function, sizeof(function));
	code[11] = 0xff;
	code[12] = 0xe0;
	return code;
}
} // namespace DemonsSoulsCopy
} // namespace Loader
