## New in this build

- **Voices that were silent now play**: much of the story dialogue, many NPC lines and the cutscene voices were
  silent. The game keeps 1,330 of its sound files as 16-channel "3D audio" (ambisonics) streams, which the emulator
  could not decode. It now does.
- **NPC speech is easier to hear**: the game's 3D audio is now mixed so that a sound straight ahead of you (usually
  the person you are talking to) plays at full level; before, it came out 3 dB below sounds from the sides. If
  speech is still too quiet for your speakers or headphones, the game's own Settings > Audio > Midnight Mode makes
  it about 5 dB louder again.
- **Character creation faces are right**: faces no longer look dirty, and switching presets no longer keeps the
  previous preset's makeup.
- **Render height option** for slower GPUs: the launcher's "Render height" (or `run-windows.ps1 -RenderHeight 1080`)
  renders the game's 3D at a lower height (720 to 1440) while the output and the menus stay 4K. On a laptop RTX 3080
  at 1080: standing 20.7 -> 24.8 fps.
- **Faster start-up**, above all on PCs with fewer CPU cores:
  - The game's start-up file reads no longer wait on a slow system call (laptop: from launch to the game 108-111 s
    -> 63-64 s).
  - The background shader preparation no longer competes with the game's loading for CPU cores, and keeps its
    temporary data compressed (4.5 GB -> 1.3 GB of RAM).
- **Shader precompile**:
  - It no longer compiles the same pipelines several times over; on the test laptop the full precompile took about
    3.5 hours before.
  - On drivers without pipeline binaries (NVIDIA before 570, AMD, Intel), the precompiled shaders are read one at a
    time when needed instead of loaded whole, about 4 GB less RAM in use.
- **Fewer hitches while walking on GPUs with less than 12 GB of video memory**: the shaders this PC has recorded are
  prepared at start-up there too, within 1/16 of the video memory, and pipelines share their layouts (a laptop's
  1-1 walk: no frame of 90 ms or more, before 200-900 ms hitches).
- **Less video memory on GPUs with less than 12 GB**: a texture the game has so far streamed in only from its smaller
  levels now takes only the memory of those levels (an 8 GB laptop at 1-1: textures 4.2 -> 2.6 GB, so the game fits
  in video memory instead of being paged in and out; walking about 8% faster).

## 本版本的变化

- **原本没有声音的语音现在有了**：大部分剧情对话、很多 NPC 台词和过场动画的人声之前都是静音的。游戏里有 1330 个音频文件是 16 声道的"3D 音频"（Ambisonics）格式，模拟器之前无法解码，现在可以了。
- **NPC 说话更容易听清**：重新调整了游戏 3D 音频的混音方式，正前方的声音（通常就是你在对话的人）现在以完整音量播放；之前比两侧的声音低 3 dB。如果在你的音箱或耳机上说话声仍然偏小，可以打开游戏自带的 设置 > 音频 > Midnight Mode（午夜模式），说话声会再大约 5 dB。
- **捏脸时的脸恢复正常**：脸不再显得脏，切换预设时也不会残留上一个预设的妆容。
- **新增渲染高度选项**，适合较慢的显卡：启动器里的"Render height"（或 `run-windows.ps1 -RenderHeight 1080`）可以让游戏的 3D 画面以较低高度渲染（720 到 1440），输出画面和菜单仍保持 4K。笔记本 RTX 3080 设为 1080 时：站立 20.7 -> 24.8 fps。
- **启动更快**，CPU 核心较少的电脑尤其明显：
  - 游戏启动时读取文件不再被一个很慢的系统调用拖住（笔记本：从启动到进入游戏 108-111 秒 -> 63-64 秒）。
  - 后台的着色器准备不再和游戏加载抢 CPU 核心，临时数据也改为压缩存放（内存占用 4.5 GB -> 1.3 GB）。
- **着色器预编译**：
  - 不再把同一批管线重复编译好几遍；测试笔记本上之前完整预编译约需 3.5 小时。
  - 在不支持 pipeline binary 的驱动上（570 之前的 NVIDIA 驱动、AMD、Intel），预编译结果改为用到时逐个读取，不再一次全部载入，约少占 4 GB 内存。
- **显存小于 12 GB 的显卡走动时卡顿更少**：这类显卡现在也会在启动时准备本机已记录的着色器（占用不超过显存的 1/16），管线之间也共享布局（笔记本 1-1 走动：没有超过 90 ms 的帧，之前有 200-900 ms 的卡顿）。
- **显存小于 12 GB 的显卡占用更少显存**：游戏目前只流式载入了较小几级 mipmap 的纹理，现在只占用这几级的显存（8 GB 笔记本在 1-1：纹理 4.2 -> 2.6 GB，游戏能放进显存，不再被换入换出；走动约快 8%）。
