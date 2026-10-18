## New in this build

- **Faster shaders: run the precompile once more.** Two changes give many of the game's shaders new pipelines:
  - About 1,200 compute shaders (most of them the lighting at the end of each frame) run faster on NVIDIA, Intel and
    RDNA2 AMD cards: on an RTX 5090 in Cinematic mode, 52 → 58 fps.
  - Shaders that read many entries of the game's resource tables no longer keep all of them in registers from their
    first instruction, which made some of them spill and run very slowly on cards with a small cache.

  Together on an RTX 3080 Laptop (8 GB): Boletaria 1-1's rampart while the dragon burns it, often 8-16 fps before, now
  runs at 27-34 fps (one fire shader took 80 ms a frame, now about 3 ms), and the route there went from 22.7 to 25.4
  fps on average.

  The launcher offers the precompile at the first start; it compiles only what changed (27 minutes on an RTX 3080
  Laptop, and the game can start after 22). Until then the game keeps the shaders it has, as before, without new
  stutter. If your precompile ended with errors since v20261016, it works again now (an old warm-up recording made it
  fail).
- **Safe GPU mode** (new in the launcher, off by default): for PCs whose graphics card hangs ("device lost", the
  screen going black while the graphics driver restarts, a VIDEO_TDR_FAILURE blue screen). It prevents those crashes
  and blue screens, but **the frame rate is about 37% lower**. On a laptop RTX 3080 (driver 566.07), 3 of 4 runs
  through Boletaria hung without it and none of 3 with it. After this version's precompile, the same laptop ran that
  route 4 times without a hang: the shader fix above seems to have removed those hangs there, so try without it first.
  It does not help the MEMORY_MANAGEMENT blue screens of a full RAM. Leave it off when the game runs without such
  crashes.
- **Fixes:**
  - **Archstone travel previews** (the destination videos in the Nexus) no longer show stripes or a green picture the
    first time (issue #52). The video decoder's first reads of fresh memory got zeros.
  - **Graphics Low and Lowest** no longer make objects vanish or floors turn black, such as the cart and the barricades
    at the Gates of Boletaria and parts of the Nexus floor (issues #40, #49). They no longer force the shadow level of
    detail; Lowest is a little slower for it (27.4 → 25.6 fps at the gates on an RTX 3080 Laptop).
  - **"failed to create image" exits** on 16 GB AMD cards, after some play or when switching to Cinematic mode
    (issues #49, #53):
    - The game's streamed texture pools now share one image per pool. Before, each view of a pool became an image of
      its own, uploaded whole, until 13 GB of video memory were full.
    - A texture or storage image the card has no memory for is left out for a moment while the emulator frees images
      the game stopped using; a render target gets the room of images unused for 16 frames before the emulator gives
      up.
    - Textures that the game and the GPU both write parts of upload only those parts. The whole uploads made frames of
      190-360 ms on an RTX 5090 and 3.2 s on an RX 9070 as textures streamed in.
  - **Controllers** (issue #49): unplugging a device the emulator does not use (a wheel, a virtual pad) no longer ends
    the game.
  - **Blue screens with a full RAM** (MEMORY_MANAGEMENT 0x1A, issues #5 and #50): two changes make them less likely.
    The emulator no longer changes page protections in a page table while it maps or unmaps memory there, and the
    background shader preparation waits while less than 1 GiB of RAM is free. One reporter still had one with the
    first change alone, so please tell us if it happens again.
  - **Windows Defender** reported the precompile as a trojan (issue #47). It no longer copies itself into %TEMP% under
    the emulator's name; it runs from `_PipelineCache\precompile-as-emulator` (a hard link, nothing copied), and the
    old folder in %TEMP% is removed.
  - The launcher, `run.cmd` and `precompile.cmd` start cmd.exe and Windows PowerShell by their full paths: no more
    "How do you want to open 'cmd'?" or "'powershell' is not recognized" (issues #47, #55).
  - A draw or dispatch whose buffer was merged into a new one just before it was recorded wrote to the old buffer, so
    its GPU writes could be lost. Its buffers are looked up again now.
- **Frame generation (DLSS-G, issue #45):**
  - DLSS-G gets NVIDIA Reflex's per-frame sleep, which Streamline requires (without it, other forks measured DLSS-G
    presents blocked for 25-35 ms on RTX 40 cards). It turns itself off on the title, movies and loading screens and
    around a new swapchain, and resets after a frame that was not shown.
  - 64 GiB of address space above the game are kept free for the driver until the Vulkan device is made. If the
    device is still refused, the log says whether DLSS-G is supported here and how to turn Bottom-up ASLR on for
    kyty_emulator.exe, a user's fix (README).
  - A vblank that came late is no longer made up with a burst of flips, which showed the queued frames at once.
- **In the game:** F8 switches the overlay between Off, "Frame rate" and "Detailed", whatever the launcher started it
  with (also with `run.cmd`). The debug keys F1, F6, F9 and F10 are gone (F1 is free for key bindings).
- **Faster:**
  - Fewer GPU submissions: a flush nothing waits on goes with the next one while the GPU still has work queued (117 →
    59 submissions a frame in Cinematic mode, +1%).
  - Sequential writes of the game to memory the GPU reads take one fault per 64 KiB instead of 16 KiB (RTX 5090 at
    1-1: 1% low +3.8%).
  - The optimized build's profile (PGO) was trained again on this version's code.
- **Diagnostics** (for bug reports, all off by default): `KYTY_PRESENT_DIRECT=1` (issue #37),
  `KYTY_TEST_MAX_ARRAY_LAYERS=<n>` (issue #24), `KYTY_FRAMEGEN_VMMAP=1` (issue #45), `KYTY_VULKAN_SYNCVAL=1` (the
  validation layer's synchronization checks), `KYTY_PROG_TRACE=<hash>`, `KYTY_CHAIN_FENCE=<hash>`.
  `KYTY_LANE_LOCAL_COOP=0` and `KYTY_TABLE_SLOTS_AT_USE=0` turn the two shader changes above off (their pipelines are
  then compiled where first needed, with stutter), to compare when something looks wrong. The log now has
  "PC memory" lines: the emulator's working set and private memory and the RAM the system has left, when they change by
  1 GiB or the free RAM drops under 2 GiB (issue #47).
- **README:** a user's NVIDIA settings for tearing or stutter on a 60 Hz TV (issue #30).

## 本版本的变化

- **更快的着色器：请再预编译一次。** 有两项改动让游戏的许多着色器需要新的管线：
  - 约 1200 个计算着色器（大多是每帧末尾的光照）在 NVIDIA、Intel 和 RDNA2 的 AMD 显卡上运行得更快了：RTX 5090 电影模式下 52 → 58 fps。
  - 读取大量游戏资源表项的着色器，不再从第一条指令起就把它们全部放在寄存器里。以前这会让其中一些着色器溢出寄存器，在缓存较小的显卡上非常慢。

  两项合起来，在 RTX 3080 Laptop（8 GB）上：波雷塔利亚 1-1 城墙被龙火焚烧时，以前常常只有 8-16 fps，现在 27-34 fps（其中一个火焰着色器以前每帧要 80 ms，现在约 3 ms），那一段路线的平均帧率从 22.7 提高到 25.4 fps。

  第一次启动时启动器会提示预编译，只编译有变化的部分（RTX 3080 Laptop 上 27 分钟，22 分钟后就可以开始游戏）。在那之前，游戏照旧使用现有的着色器，不会多出卡顿。如果你从 v20261016 起预编译总是以错误结束，现在可以正常完成了（原因是旧的预热录制文件）。
- **安全 GPU 模式**（启动器新选项，默认关闭）：适用于显卡会卡死的电脑（"device lost"、驱动重启时黑屏、VIDEO_TDR_FAILURE 蓝屏）。它能防止这类崩溃和蓝屏，但**帧率会降低约 37%**。在一台 RTX 3080 笔记本（566.07 驱动）上，关闭时波雷塔利亚的 4 次测试里有 3 次卡死，开启后 3 次都没有。用这个版本预编译之后，同一台笔记本在同一路线上跑了 4 次都没有卡死：上面的着色器修正看来也消除了那里的卡死，所以请先不开它试试。它对内存占满时的 MEMORY_MANAGEMENT 蓝屏无效。游戏不出这类崩溃时请保持关闭。
- **修正：**
  - **要石传送预览**（连接之地里各目的地的预览视频）第一次播放时不再出现条纹或整片发绿（issue #52）。原因是视频解码器第一次读取新内存时读到了零。
  - **画质 Low 和 Lowest** 不再让物体消失或地面变黑，比如波雷塔利亚城门处的推车和两侧的路障、连接之地的部分地面（issue #40、#49）。它们不再强制阴影细节等级，Lowest 因此稍慢一点（RTX 3080 Laptop 在城门处 27.4 → 25.6 fps）。
  - **16 GB AMD 显卡上 "failed to create image" 退出**：玩一段时间后，或切换到电影模式时（issue #49、#53）：
    - 游戏流式加载的纹理池现在每个池只用一张图像。之前池的每个视图都单独建一张图像并整张上传，直到 13 GB 显存被占满。
    - 显存不足、建不了的纹理或存储图像会暂时空着，同时模拟器释放游戏不再使用的图像；渲染目标会先拿到 16 帧未用图像的空间，实在不行才退出。
    - 游戏和 GPU 各写了一部分的纹理，现在只上传被写的部分。之前整张上传，纹理加载时在 RTX 5090 上出现 190-360 ms 的帧，在 RX 9070 上是 3.2 秒。
  - **手柄**（issue #49）：拔掉模拟器没在用的设备（方向盘、虚拟手柄）不再导致游戏结束。
  - **内存占满时的蓝屏**（MEMORY_MANAGEMENT 0x1A，issue #5 和 #50）：两处改动降低了发生的可能。模拟器在某个页表里映射或解除映射内存时，不再修改那个页表里的页保护；内存剩余不到 1 GiB 时，后台准备着色器会暂停等待。有一位反馈者在只有第一处改动时仍然蓝屏过，所以如果再次发生请告诉我们。
  - **Windows Defender** 把预编译程序报成木马（issue #47）。它不再以模拟器的名字把自己复制到 %TEMP%，而是从 `_PipelineCache\precompile-as-emulator` 运行（硬链接，不复制文件），%TEMP% 里的旧文件夹会被删除。
  - 启动器、`run.cmd` 和 `precompile.cmd` 按完整路径启动 cmd.exe 和 Windows PowerShell：不会再出现"你要如何打开 'cmd'"或 "'powershell' 不是内部或外部命令"（issue #47、#55）。
  - 绘制或调度的缓冲区如果在录制前刚被合并进新缓冲区，之前会写到旧缓冲区里，GPU 的写入可能丢失。现在会重新查找它的缓冲区。
- **帧生成（DLSS-G，issue #45）：**
  - DLSS-G 每帧都会调用 NVIDIA Reflex 的 sleep，这是 Streamline 的要求（其他分支测到，没有它时 RTX 40 显卡上 DLSS-G 的呈现会被阻塞 25-35 ms）。在标题画面、过场视频和加载画面以及重建交换链前后，DLSS-G 会自动关闭；有一帧没显示出来时会重置。
  - 在 Vulkan 设备创建之前，游戏上方的 64 GiB 地址空间会留给驱动。如果设备仍被拒绝，日志会说明这台电脑是否支持 DLSS-G，以及如何为 kyty_emulator.exe 开启"随机化内存分配（自下而上 ASLR）"，这是一位用户找到的办法（见 README）。
  - 迟到的垂直同步不再用一连串翻页来补，以前那样会把排队的帧一下子全显示出来。
- **游戏中：** F8 在关闭、"帧率"和"详细"之间切换叠加面板，不管启动器用哪个设置启动（用 `run.cmd` 启动也可以）。调试键 F1、F6、F9、F10 已移除（F1 可以用来绑定按键了）。
- **更快：**
  - 更少的 GPU 提交：没人等待的提交，在 GPU 还有排队工作时会和下一次合并（电影模式下每帧 117 → 59 次提交，+1%）。
  - 游戏顺序写入 GPU 会读的内存时，每 64 KiB 触发一次缺页，而不是每 16 KiB 一次（RTX 5090 在 1-1：1% low +3.8%）。
  - 优化构建的配置文件（PGO）按这个版本的代码重新训练了。
- **诊断**（用于报告问题，默认全部关闭）：`KYTY_PRESENT_DIRECT=1`（issue #37）、`KYTY_TEST_MAX_ARRAY_LAYERS=<n>`（issue #24）、`KYTY_FRAMEGEN_VMMAP=1`（issue #45）、`KYTY_VULKAN_SYNCVAL=1`（验证层的同步检查）、`KYTY_PROG_TRACE=<hash>`、`KYTY_CHAIN_FENCE=<hash>`。`KYTY_LANE_LOCAL_COOP=0` 和 `KYTY_TABLE_SLOTS_AT_USE=0` 可以分别关闭上面的两项着色器改动（对应的管线会在第一次用到时才编译，会有卡顿），用于在画面异常时对比。日志里新增"PC memory"行：模拟器的工作集、私有内存和系统剩余内存，在变化 1 GiB 或空闲内存低于 2 GiB 时记录（issue #47）。
- **README：** 一位用户在 60 Hz 电视上解决画面撕裂或卡顿的 NVIDIA 设置（issue #30）。
