// Included inside VulkanHarness; exercises real mappings, page watchers and upload jobs.
void CheckFreshMemoryUploads() {
  constexpr const char* name = "FreshMemoryUploads";
  namespace Memory = Libs::LibKernel::Memory;
  constexpr uint64_t base = 0x203a00000ull, alias = base + 0x100000;
  constexpr uint64_t g = FreshMemory::Granule, size = 5 * g;
  EnsureRuntimeContext();
  int64_t physical = -1;
  Require(name, "allocate", Memory::KernelAllocateDirectMemory(0, Memory::KernelGetDirectMemorySize(),
      size, g, 0, &physical) == 0, "allocation failed");
  for (auto address : {base, alias}) {
    void* target = reinterpret_cast<void*>(address);
    Require(name, "map", Memory::KernelMapDirectMemory(&target, size, 0x33, 0x10, physical, g) == 0,
            "mapping failed");
  }
  {
    RenderContext context(m_runtime_context);
    auto& scheduler = context.GetCommandScheduler();
    HW::Context registers{}; HW::UserConfig config{}; HW::Shader shaders{};
    scheduler.Begin(registers, config, shaders);
    auto& resources = context.GetGpuResources();
    resources.MapMemory(base, size);
    resources.MapMemory(alias, size);
    auto& cache = resources.GetBufferCache();
    const auto old_async = kyty_local_async_upload_mode.exchange(2);
    std::vector<uint8_t> staging(size, 0xcd);
    const auto stage = [&] {
      Require(name, "queue staging", BufferCacheTestAccess::StageImage(cache, staging.data(), base, size), "staging unavailable");
      // The production caller wakes the upload worker after submitting its batch.
      AsyncUpload::Get().Kick();
      AsyncUpload::Drain();
    };
    stage();
    Require(name, "fresh zeros", std::all_of(staging.begin(), staging.end(), [](auto b) { return b == 0; }), "fresh upload was not zero");
    const auto* backing = Memory::TryGetBackingPointer(base, size);
    Require(name, "backing", backing != nullptr, "missing backing");
    const auto marker = uint32_t{0xabcdef12};
    if (FreshMemory::Enabled()) {
      auto& state = *FreshMemory::Get();
      Require(name, "avoided source reads", state.zero_upload_bytes.load() >= size, "fresh staging did not avoid reads");
      Require(name, "alias first write", resources.HandleFault(PageFaultAccess::Write, alias + 7), "first write not handled");
      Require(name, "shared written bit", FreshMemory::IsWritten(state, FreshMemory::GranuleOf(state, reinterpret_cast<uint64_t>(backing))), "alias not marked");
    }
    // Upload tracking can still watch the page after the fresh watch is gone.
    resources.InvalidateMemoryForWrite(alias, sizeof(marker));
    std::memcpy(reinterpret_cast<void*>(alias), &marker, sizeof(marker));
    Require(name, "host backing write", Memory::TryWriteBacking(base + g + 3, &marker, sizeof(marker)), "backing write failed");
    Require(name, "host CPU write", Memory::TryWriteCpuBacking(base + 2 * g + 5, &marker, sizeof(marker)), "CPU backing write failed");
    stage();
    for (const auto offset : {uint64_t{0}, g + 3, 2 * g + 5})
      Require(name, "written bytes retained", std::memcmp(staging.data() + offset, &marker, sizeof(marker)) == 0, "write became zeros");
    Require(name, "untouched tail", std::all_of(staging.begin() + 3 * g, staging.end(), [](auto b) { return b == 0; }), "tail changed");
    resources.UnmapMemory(alias + 3 * g + 0x4000, 0x4000, true);
    if (FreshMemory::Enabled()) {
      auto& state = *FreshMemory::Get();
      Require(name, "partial unmap", FreshMemory::IsWritten(state, FreshMemory::GranuleOf(state,
          reinterpret_cast<uint64_t>(backing) + 3 * g)), "partial unmap left unwatched bytes fresh");
    }
    resources.UnmapMemory(alias, size, true);
    if (FreshMemory::Enabled()) {
      auto& state = *FreshMemory::Get();
      Require(name, "surviving alias", FreshMemory::IsWritten(state, FreshMemory::GranuleOf(state,
          reinterpret_cast<uint64_t>(backing) + 4 * g)), "full alias unmap left the surviving unwatched alias fresh");
    }
    resources.UnmapMemory(base, size, true);
    scheduler.Finish();
    kyty_local_async_upload_mode.store(old_async);
  }
  for (auto address : {base, alias}) Require(name, "unmap", Memory::KernelMunmap(address, size) == 0, "unmap failed");
  Require(name, "release", Memory::KernelReleaseDirectMemory(physical, size) == 0, "release failed");
  std::printf("[gpu]     %-32s ok\n", name);
}

void CheckBufferStreamGaps() {
  constexpr const char* name = "BufferStreamGaps";
  namespace Memory = Libs::LibKernel::Memory;
  constexpr uint64_t base = 0x204000000ull, page = 0x4000, size = 0x100000;
  EnsureRuntimeContext();
  int64_t physical = -1;
  Require(name, "allocate", Memory::KernelAllocateDirectMemory(0, Memory::KernelGetDirectMemorySize(), size,
      0x10000, 0, &physical) == 0, "allocation failed");
  void* target = reinterpret_cast<void*>(base);
  Require(name, "map", Memory::KernelMapDirectMemory(&target, size, 3, 0x10, physical, 0x10000) == 0, "map failed");
  {
    RenderContext context(m_runtime_context);
    auto& scheduler = context.GetCommandScheduler();
    HW::Context registers{}; HW::UserConfig config{}; HW::Shader shaders{};
    scheduler.Begin(registers, config, shaders);
    auto& resources = context.GetGpuResources();
    auto& cache = resources.GetBufferCache();
    // Two tracked islands with a 128 KiB hole between them.
    resources.MapMemory(base, 4 * page);
    resources.MapMemory(base + 12 * page, 4 * page);
    auto left = cache.FindBuffer(base, 2 * page);
    cache.GetBuffer(left).IncreaseStreamScore(17);
    auto span = BufferCacheTestAccess::ResolveSpan(cache, base + page, 2 * page);
    Require(name, "right leap", span.first == base && span.second == base + 4 * page, "right leap crossed a hole");
    auto right = cache.FindBuffer(base + 14 * page, 2 * page);
    cache.GetBuffer(right).IncreaseStreamScore(17);
    span = BufferCacheTestAccess::ResolveSpan(cache, base + 13 * page, 2 * page);
    Require(name, "left leap", span.first == base + 12 * page && span.second == base + 16 * page, "left leap crossed a hole");
    // A finite descriptor may span holes. Once unmapped, separate the surviving islands.
    cache.FindBuffer(base, 16 * page);
    cache.SplitUnmappedBuffers(base + 4 * page, 8 * page);
    Require(name, "split hole", !BufferCacheTestAccess::PageOwner(cache, base + 5 * page) &&
        BufferCacheTestAccess::PageOwner(cache, base) != BufferCacheTestAccess::PageOwner(cache, base + 12 * page),
        "128 KiB hole retained one oversized allocation");
    Require(name, "accounting", BufferCacheTestAccess::AccountingMatches(cache), "split accounting drifted");
    resources.UnmapMemory(base, size, true);
    scheduler.Finish();
  }
  Require(name, "unmap", Memory::KernelMunmap(base, size) == 0, "unmap failed");
  Require(name, "release", Memory::KernelReleaseDirectMemory(physical, size) == 0, "release failed");
  std::printf("[gpu]     %-32s ok\n", name);
}
