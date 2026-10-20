#include "local/scene-tags.h"

#include <cstdlib>

static void Check(bool condition, const char* message) {
	if (!condition) {
		std::fprintf(stderr, "SceneTagsTests: %s\n", message);
		std::exit(1);
	}
}

int main() {
	using namespace SceneTags;
	const auto path = std::filesystem::temp_directory_path() /
	                  ("kyty-scene-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	constexpr uint32_t first = 0x02000000, second = 0x04010000;
	constexpr uint64_t shared = 0x1122334455667788, exclusive = 0xfedcba9876543210;
	Check(Load(path, "test-game-driver"), "missing file should start empty");
	NoteUse(shared);
	Check(Recipes(None).empty(), "menu binding was attributed to a world");
	Enter(first);
	const auto generation = Generation();
	Enter(first);
	Check(Generation() == generation, "repeated reads of one world started another visit");
	NoteUse(shared);
	NoteUse(shared);
	NoteUse(exclusive);
	Check(!Knows(first), "brief visit prematurely restricted warmup");
	Check(Save(), "initial save failed");
	{
		std::lock_guard lock(Get().mutex);
		Get().entered_at -= std::chrono::seconds(KnownSeconds + 1);
	}
	Check(Save() && Knows(first), "qualified visit was not persisted");
	Enter(second);
	NoteUse(shared);
	Check(Generation() == generation + 1, "world change did not advance generation");
	Check(Recipes(second).size() == 1 && !Recipes(second).contains(exclusive), "world recipes overlapped incorrectly");
	Check(Save(), "replacing an existing scene file failed");
	Check(Load(path, "test-game-driver") && Knows(first) && !Knows(second), "visit confidence did not survive reload");
	Check(Recipes(first).contains(exclusive) && Recipes(first).contains(shared), "64-bit recipes did not survive reload");
	Check(!Load(path, "different-driver") && Recipes(first).empty(), "foreign driver recording was accepted");
	Check(Load(path, "test-game-driver"), "valid recording was damaged");
	// A checksum-valid file with extra payload must not leave a partially accepted set of recipes.
	std::ifstream in(path, std::ios::binary | std::ios::ate);
	const auto size = static_cast<size_t>(in.tellg());
	std::string bytes(size, '\0');
	in.seekg(0);
	in.read(bytes.data(), static_cast<std::streamsize>(size));
	in.close();
	const auto header = Get().identity.size();
	bytes.append(4, '\0');
	const uint64_t checksum = XXH3_64bits(bytes.data() + header + 8, bytes.size() - header - 8);
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), static_cast<std::streamsize>(header));
	out.write(reinterpret_cast<const char*>(&checksum), 8);
	out.write(bytes.data() + header + 8, static_cast<std::streamsize>(bytes.size() - header - 8));
	out.close();
	Check(!Load(path, "test-game-driver") && Recipes(first).empty() && !Knows(first), "malformed recording retained partial tags");
	std::filesystem::remove(path);
	std::puts("SceneTagsTests: world changes, visit confidence, persistence and invalid recordings passed");
}
