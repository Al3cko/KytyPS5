#include "libs/errno.h"
#include "loader/symbolDatabase.h"
#include "graphics/guest_gpu/superResolution.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Libs::LibPsml { void InitPsml_1(Loader::SymbolDatabase*); }
static void Check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "PSML regression: %s\n", what); std::exit(1); }
}

int main() {
    Loader::SymbolDatabase symbols;
    Libs::LibPsml::InitPsml_1(&symbols);
    const auto find = [&](const char* nid) {
        const auto* record = symbols.FindByNid(nid, Loader::SymbolType::Func);
        Check(record != nullptr, nid);
        return record->vaddr;
    };
    struct Requirements { uint64_t size, alignment, count; };
    struct Parameters { uint32_t type = 0, reserved = 0; };
    struct Block { uint64_t address = 0, size = 0; };
    struct Shared {
        uint32_t type = 0, reserved = 0;
        const Block* blocks;
        uint64_t count, address = 0;
    };
    struct Context {
        uint32_t type = 0, mode = 0;
        const Block* blocks;
        uint64_t count;
        uint16_t width = 3840, height = 2160;
        uint32_t reserved = 0;
        const uint32_t* size_a = nullptr;
        const uint32_t* size_b = nullptr;
        uint64_t reserved2 = 0, shared = 0;
        const void* extra_a = nullptr;
        const void* extra_b = nullptr;
    };
    static_assert(offsetof(Context, width) == 0x18 && offsetof(Context, shared) == 0x38);
    using Init = int(KYTY_SYSV_ABI*)();
    using RequirementsFn = int(KYTY_SYSV_ABI*)(Requirements*, const Parameters*);
    using SharedFn = int(KYTY_SYSV_ABI*)(uint64_t*, const Shared*);
    using ContextFn = int(KYTY_SYSV_ABI*)(uint64_t*, const Context*);
    using DispatchSizeFn = int(KYTY_SYSV_ABI*)(uint64_t, uint32_t*);
    struct Span { uint32_t* commands; uint32_t dwords; };
    using DispatchFn = int(KYTY_SYSV_ABI*)(uint64_t, const Span*, const uint8_t*);
    Check(reinterpret_cast<Init>(find("3WVD91e12ZQ"))() == OK, "initialize");
    Requirements requirements {};
    Parameters parameters;
    Check(reinterpret_cast<RequirementsFn>(find("+2KpvixvL6E"))(&requirements, &parameters) == OK,
          "shared requirements");
    std::vector<Block> blocks(requirements.count);
    Shared shared_params {0, 0, blocks.data(), blocks.size(), 0};
    uint64_t shared = 0, context = 0;
    Check(reinterpret_cast<SharedFn>(find("eWoKNeB6V-k"))(&shared, &shared_params) == OK && shared != 0,
          "shared resources");
    Context context_params {0, 0, blocks.data(), 1};
    context_params.shared = shared;
    const auto legacy_create = find("gxv3i+MTEzU");
    Check(legacy_create == find("fccGInHrj8A"), "both context creation exports resolve");
    Check(reinterpret_cast<ContextFn>(legacy_create)(&context, &context_params) == OK && context != 0,
          "01.005 context creation");
    uint32_t words = 0;
    Check(reinterpret_cast<DispatchSizeFn>(find("AHalTX9wFZY"))(context, &words) == OK,
          "dispatch size");
    Check(words == Libs::Graphics::SuperResolution::PacketDwords, "packet allocation size");
    std::vector<uint32_t> commands(words + 2, 0xdeadbeef);
    Span span {commands.data(), words};
    std::array<uint8_t, Libs::Graphics::SuperResolution::ParameterBytes> input {};
    Check(reinterpret_cast<DispatchFn>(find("94iBp3KvIuI"))(context, &span, input.data()) == OK,
          "dispatch records packet");
    Libs::Graphics::SuperResolution::Packet packet;
    std::memcpy(&packet, commands.data() + 1, sizeof(packet));
    Check(packet.output_width == 3840 && packet.output_height == 2160, "context output dimensions survive dispatch");
    Check(commands[words] == 0xdeadbeef && commands[words + 1] == 0xdeadbeef, "dispatch stays inside reservation");
    // The older dispatch takes only 0x148 bytes. Sentinel fields distinguish
    // the old offsets from the newer layout; check what the renderer consumes.
    std::array<uint8_t, 0x148> legacy {};
    std::array<std::array<uint32_t, 8>, 7> textures {};
    for (size_t slot = 0; slot < textures.size(); ++slot) {
        for (size_t word = 0; word < 8; ++word) textures[slot][word] = 100 * slot + word + 1;
        const auto* descriptor = textures[slot].data();
        std::memcpy(legacy.data() + slot * 8, &descriptor, sizeof(descriptor));
    }
    for (size_t i = 0x38; i < legacy.size(); ++i) legacy[i] = static_cast<uint8_t>(i);
    legacy[0x114] = 1;
    const auto legacy_dispatch = reinterpret_cast<DispatchFn>(find("RUNLFro+qok"));
    std::fill(commands.begin(), commands.end(), 0xdeadbeef);
    Span short_span {commands.data(), words - 1};
    Check(legacy_dispatch(context, &short_span, legacy.data()) != OK, "short reservation rejected");
    Check(commands.front() == 0xdeadbeef, "failed dispatch writes no command");
    Check(legacy_dispatch(context, &span, nullptr) != OK, "null legacy parameters rejected");
    Check(legacy_dispatch(context, &span, legacy.data()) == OK, "legacy dispatch records packet");
    std::memcpy(&packet, commands.data() + 1, sizeof(packet));
    Check(packet.textures_given == 0x7f && std::memcmp(packet.textures, textures.data(), sizeof(packet.textures)) == 0,
          "legacy texture descriptors survive dispatch");
    const auto* normalized = reinterpret_cast<const uint8_t*>(packet.parameters);
    Check(std::memcmp(normalized + 0x6c, legacy.data() + 0x6c, 64) == 0, "legacy projection");
    Check(std::memcmp(normalized + 0xe8, legacy.data() + 0xe8, 8) == 0, "legacy near and far planes");
    Check(std::memcmp(normalized + 0x100, legacy.data() + 0xf8, 8) == 0, "legacy jitter relocation");
    Check(std::memcmp(normalized + 0x108, legacy.data() + 0x100, 8) == 0, "legacy motion scale relocation");
    Check(normalized[0x11c] == 1 && normalized[0x11d] == 0, "legacy reset relocation and padding");
    Check(packet.output_width == 3840 && packet.output_height == 2160, "legacy output dimensions");
    Check(commands[words] == 0xdeadbeef && commands[words + 1] == 0xdeadbeef, "legacy reservation bounds");
    std::puts("PSML: 01.005 context and legacy dispatch, modern dispatch and packet bounds passed");
}
