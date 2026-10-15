## New in this build

- **Smoother walking**: when the game streams textures in and out, the emulator no longer remaps the rest of the
  game's memory each time (that held the game's loading threads for up to 30 ms). Walking in Boletaria 1-1 on the test
  PC, the longest frames went from about 43 ms to 22 ms and the 1% low from 35 to 49 fps.
- **Faster loading, most on PCs with 16 GB of RAM**: the game opens about 28,000 files to load an area, one after
  another; the emulator now opens the next ones ahead of time, in the order recorded on earlier loads. A recording of
  every world ships with the package, so the first launch gets it too. On a 16 GB laptop, loading Boletaria 1-1 went
  from about 110 s to about 20 s (the prologue from 25-37 s to 7 s); PCs with plenty of RAM load as before.
- **Ultrawide displays**: the launcher's "Aspect ratio" (21:9 or 32:9) renders the 3D wider instead of stretching it;
  the HUD keeps its shape at the sides of the screen, in-game menus are stretched, movies stay 16:9.
- **Frame generation**: the launcher's "Frame generation" (DLSS, 2x to 6x) on NVIDIA RTX 40 or newer (3x and up RTX 50),
  with NVIDIA's Streamline runtime, which you download yourself (README.md says how).
- **Overlay**: the launcher's "Overlay" shows the frame rate with a graph of the frame times, or a detailed panel
  (render thread, graphics card, each processor core, memory).
- **Graphics presets**: the launcher's "Graphics" Low and Lowest draw less (simpler shadows and distant detail), which
  helps where the processor limits the frame rate.
- **Up to 120 fps** keeps the game at its normal speed (issue #22); movies, menus and loading screens stay at 60 fps,
  so the intro's sound stays in sync.
- **DualSense**: vibration, speaker and adaptive trigger effects. The window can be resized and maximized.
- **Shader preparation**: the shipped shader hints now hold tours of every world and an AMD player's recorded play, so
  more of what a first visit needs is ready before you play; the precompile is ready to play sooner, and the
  "Preparing shaders" screens at start-up are gone (that work runs beside the game). After updating, the launcher
  asks once to update the shader cache: only what is new is compiled.
- **Fixes**:
  - Continue loads the character you saved last, as on the PS5. It always loaded the first character slot, and with a
    tutorial character there it could drop the player under the world (issue #26). If you have several characters,
    Continue may now load a different one than before; Load Game picks any of them.
  - Crashes: a rare exit while playing in a world ("unknown format"); rare crashes of the game's own threads while memory
    was being unmapped.
  - On PCs with 16 GB of RAM the graphics driver's pipeline cache no longer starts over every launch or two (which made
    shaders compile again).
  - In long sessions the frame rate drops much less after many trips between worlds.
  - AMD graphics cards: the shader prefetch now covers their compute shaders too (issue #20).
  - Freezes with arrays of textures on some graphics cards (issue #9).
  - The other region's release (PPSA01342) gets the game patches, engine settings and shader hints too.
  - Linux: fixes to run_ds.sh.

## 本版本的变化

- **走动更流畅**：游戏载入和卸载纹理时，模拟器不再每次都把游戏的其余内存重新映射一遍（之前这会让游戏的载入线程卡住最多 30 ms）。在测试电脑上走过波雷塔利亚 1-1，最长的帧从约 43 ms 降到 22 ms，1% low 从 35 提高到 49 fps。
- **载入更快，16 GB 内存的电脑提升最明显**：游戏载入一个区域要依次打开约 28,000 个文件；模拟器现在按以前载入时记录的顺序，提前打开接下来要用的文件。安装包自带所有世界的记录，所以第一次启动也能用上。在 16 GB 内存的笔记本上，载入波雷塔利亚 1-1 从约 110 秒缩短到约 20 秒（序章从 25-37 秒缩短到 7 秒）；内存充足的电脑载入速度和以前一样。
- **带鱼屏（超宽屏）**：启动器的"Aspect ratio"（21:9 或 32:9）会把 3D 画面渲染得更宽，而不是拉伸；HUD 保持原来的形状、靠在屏幕两侧，游戏内菜单会被拉伸，过场动画保持 16:9。
- **帧生成**：启动器的"Frame generation"（DLSS，2x 到 6x），需要 NVIDIA RTX 40 或更新的显卡（3x 及以上需要 RTX 50），并需要自行下载 NVIDIA 的 Streamline 运行库（方法见 README.md）。
- **性能浮层**：启动器的"Overlay"可以显示帧率和帧时间曲线，或详细面板（渲染线程、显卡、每个处理器核心、内存）。
- **画质预设**：启动器的"Graphics"选 Low 或 Lowest 会少画一些东西（更简单的阴影和远景细节），在处理器限制帧率时有帮助。
- **最高 120 fps** 时游戏速度保持正常（issue #22）；过场动画、菜单和载入画面保持 60 fps，开场动画的声音不再错位。
- **DualSense**：支持震动、手柄扬声器和自适应扳机效果。窗口可以调整大小和最大化。
- **着色器准备**：随包附带的着色器提示加入了所有世界的巡游记录，以及一位 AMD 玩家的游玩记录，第一次进入各区域时需要的着色器更多地会提前准备好；预编译能更早达到"可以开始游戏"，启动时的"Preparing shaders"画面也去掉了（这部分工作改为在游戏旁边进行）。更新后，启动器会询问一次是否更新着色器缓存：只编译新增的部分。
- **修正**：
  - Continue 现在加载最近一次保存的角色，和 PS5 上一样。之前它总是加载第一个角色槽；如果那里是教程角色，就可能掉到地图下面（issue #26）。有多个角色的话，Continue 加载的角色可能和以前不同，可以用 Load Game 选择任意角色。
  - 崩溃：在世界中游玩时偶尔退出的问题（"unknown format"）；内存解除映射时，游戏自身线程偶尔崩溃的问题。
  - 16 GB 内存的电脑上，显卡驱动的管线缓存不再每启动一两次就重新开始（之前会导致着色器重新编译）。
  - 长时间游玩时，在世界之间来回多次后帧率的下降小了很多。
  - AMD 显卡：着色器预取现在也覆盖计算着色器（issue #20）。
  - 部分显卡上使用纹理数组时卡死的问题（issue #9）。
  - 另一个地区的版本（PPSA01342）也能用上游戏补丁、引擎设置和着色器提示。
  - Linux：修正了 run_ds.sh 的一些问题。
