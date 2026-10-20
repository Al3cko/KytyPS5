#include "local/dlss-scene.h"
#include "loader/guestCode.h"

#include <array>
#include <cstdio>
#include <cstdlib>

static void Check(bool condition, const char* message) {
	if (!condition) {
		std::fprintf(stderr, "DlssSceneTests: %s\n", message);
		std::exit(1);
	}
}

int main() {
	using Libs::Graphics::FrameGen::IsSupportedSrScene;
	using Libs::Graphics::FrameGen::SrRenderFits;
	using Loader::DemonsSoulsRenderSize::RenderSizeFor;
	Check(IsSupportedSrScene(2560, 1440, 1), "Quality scene rejected");
	Check(IsSupportedSrScene(2240, 1260, 1), "Balanced scene rejected");
	Check(IsSupportedSrScene(1920, 1080, 1), "1080p scene rejected");
	Check(IsSupportedSrScene(1280, 720, 1), "minimum patched scene rejected");
	Check(!IsSupportedSrScene(3840, 2160, 1), "Cinematic or 4K UI accepted as scene");
	Check(!IsSupportedSrScene(2560, 1260, 1), "mixed Quality/Balanced dimensions accepted");
	Check(!IsSupportedSrScene(2240, 1440, 1), "mixed Balanced/Quality dimensions accepted");
	Check(!IsSupportedSrScene(3416, 1440, 1), "untested ultrawide composite accepted");
	Check(!IsSupportedSrScene(2240, 1260, 2), "volume image accepted");
	Check(!IsSupportedSrScene(0, 0, 1), "empty scene accepted");
	Check(!IsSupportedSrScene(0, 0xffffffffu, 1), "out-of-range height accepted");
	Check(SrRenderFits(2240, 1260, 1920, 1080, 2560, 1440), "Balanced input rejected inside SDK range");
	Check(SrRenderFits(2560, 1440, 2560, 1440, 2560, 1440), "fixed-size SDK range rejected");
	Check(!SrRenderFits(2240, 1260, 2560, 1440, 2560, 1440), "input outside SDK range accepted");
	Check(!SrRenderFits(2240, 1260, 0, 0, 0, 0), "empty SDK range accepted");
	Check(!SrRenderFits(2240, 1260, 2560, 1440, 1920, 1080), "reversed SDK range accepted");
	Check(!SrRenderFits(2560, 1441, 1920, 1080, 2560, 1440), "out-of-range height accepted by SDK gate");
	const auto balanced = RenderSizeFor(1260);
	Check(balanced.width == 2240 && balanced.height == 1260, "guest patch differs from Balanced SR size");
	// The 1.005 render cases use vmovddup, not vmovsd. Both exact instruction
	// forms must match, without widening the signature to unrelated opcodes.
	const Loader::GuestCode::Pattern scalar(Loader::DemonsSoulsRenderSize::ModeCase);
	const Loader::GuestCode::Pattern duplicate(Loader::DemonsSoulsRenderSize::ModeCaseDuplicated);
	std::array<uint8_t, 17> code {0xc5, 0xfb, 0x12, 0x0d, 1, 2, 3, 4,
	                              0xc5, 0xfb, 0x12, 0x05, 0xfc, 0xfd, 0xfe, 0xff, 0xeb};
	Check(scalar.Valid() && duplicate.Valid() && scalar.Size() == code.size() && duplicate.Size() == code.size(),
	      "malformed render patch instruction pattern");
	Check(duplicate.Matches(code.data()) && !scalar.Matches(code.data()), "1.005 vmovddup case not recognized");
	Check(duplicate.Find(code, 64).size() == 1, "production scanner missed the 1.005 case");
	code[2] = code[10] = 0x10;
	Check(scalar.Matches(code.data()) && !duplicate.Matches(code.data()), "legacy vmovsd case regressed");
	code[2] = code[10] = 0x11;
	Check(!scalar.Matches(code.data()) && !duplicate.Matches(code.data()), "unrelated store opcode accepted");
    const Loader::GuestCode::Pattern trinity(Loader::DemonsSoulsRenderSize::TrinityCase);
    const Loader::GuestCode::Pattern trinity_duplicate(Loader::DemonsSoulsRenderSize::TrinityCaseDuplicated);
    std::array<uint8_t, 21> pro_code {0xc5, 0xfb, 0x12, 0x0d, 1, 2, 3, 4,
        0xc5, 0xfb, 0x12, 0x05, 5, 6, 7, 8, 0xc6, 0x45, 0xe0, 0x01, 0xeb};
    Check(trinity_duplicate.Matches(pro_code.data()) && !trinity.Matches(pro_code.data()),
          "01.005 PS5 Pro render case missed");
    pro_code[20] = 0x48;
    Check(trinity_duplicate.Matches(pro_code.data()), "01.005 fallthrough case requires no short jump");
    pro_code[20] = 0xeb;
    pro_code[2] = pro_code[10] = 0x10;
    Check(trinity.Matches(pro_code.data()) && !trinity_duplicate.Matches(pro_code.data()),
          "newer PS5 Pro render case regressed");
    pro_code[2] = pro_code[10] = 0x11;
    Check(!trinity.Matches(pro_code.data()) && !trinity_duplicate.Matches(pro_code.data()),
          "PS5 Pro pattern accepted a store instruction");
	// Every supported guest height must produce the same extent the SR path accepts.
	for (uint32_t height = 720; height <= 1440; ++height) {
		const auto render = RenderSizeFor(height);
		Check(IsSupportedSrScene(render.width, render.height, 1), "guest/SR extent disagreement");
		Check(!IsSupportedSrScene(render.width + 1, render.height, 1), "wrong scene width accepted");
	}
	std::puts("DLSS scene: Quality/Balanced, 721 guest heights, SDK bounds and render patch signatures passed");
}

