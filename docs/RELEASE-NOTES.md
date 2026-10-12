## New in this build

- **Fewer crashes, above all when travelling between worlds**:
  - The emulator could close without any message, most often in the Nexus after visiting a few worlds: the game
    unloads a large texture streaming area in one go, and the emulator's handling of that used up the stack of one
    of the game's job threads. Fixed. Should a thread's stack ever run out again, the log now says where instead of
    the window just closing.
  - A crash after visiting an area again ("memory tracker re-entered from upload callback", seen at the Tower of
    Latria).
  - A crash when the pause menu opened in some places ("unsupported render-target format combination").
  - On GPUs with less video memory: a crash when a very large texture had to be converted (the emulator's scratch
    memory for it now goes to system memory instead).
  - A freeze of several seconds in some areas, where one 1.6 GB texture pool was created again and again.
  - The debug warp (F8-F10) into the Nexus could crash while the area loaded.
- **Travelling between worlds no longer slows the game down**: the emulator's bookkeeping of the game's memory grew
  with every area loaded and unloaded, so the game paused for 0.3-0.8 s now and then and ran slower each time an
  area was visited again. On the test PC, six worlds (1-1, 2-1, 3-1, 4-1, 5-1 and the Nexus) visited three times in
  one session, "Up to 120 fps" on, average over the six worlds:

  | Round | 1 | 2 | 3 |
  | --- | ---: | ---: | ---: |
  | Walking, before | 69.4 | 61.4 | 58.3 |
  | Walking, now | 75.3 | 71.1 | 69.5 |
  | Turning the camera, before | 68.2 | 61.3 | 55.8 |
  | Turning the camera, now | 72.8 | 69.9 | 68.5 |
  | Slowest frame, before | 570 ms | 649 ms | 786 ms |
  | Slowest frame, now | 49 ms | 53 ms | 52 ms |

  ("Before": a build from right after the previous release.)
- **Long sessions tested**: 36 area loads in a row without restarting (the six worlds six times each, about 40
  minutes) ran without a crash or freeze, and so did a round through twelve areas (the first area of each world
  and 1-2, 1-3, 1-4, 2-2, 2-3, 3-2, 3-3). RAM and video memory grow over the first two or three rounds and only
  slowly after that. With the emulator limited to 8 GB of video memory, three rounds of the six worlds also ran
  without a crash.
- **Faster area loading**: the game's level data is copied into memory several times faster (a 141 MB data file of
  the Tower of Latria: under 0.1 s instead of about 0.3 s).
- **Less video memory**: buffers over memory the game has mostly given back are trimmed to the part still in use.

## 本版本的变化

- **崩溃更少，尤其是在世界之间来回传送时**：
  - 模拟器有时会不留任何提示直接关闭，多发生在去过几个世界之后回到 Nexus 时：游戏会一次性卸载一大块纹理流式区域，模拟器处理这一步时耗尽了游戏某个任务线程的栈。已修复。以后如果再出现线程栈耗尽，日志里会写明位置，而不是窗口直接消失。
  - 再次进入某个区域后的崩溃（"memory tracker re-entered from upload callback"，出现在拉特利亚之塔）。
  - 在某些地方打开暂停菜单时的崩溃（"unsupported render-target format combination"）。
  - 显存较小的显卡：转换超大纹理时的崩溃（这部分临时内存现在会改放到系统内存）。
  - 部分区域会卡住好几秒：同一个 1.6 GB 的纹理池被反复重建。
  - 使用调试传送（F8-F10）进入 Nexus 时，加载过程中可能崩溃。
- **在世界之间来回传送不再越来越慢**：此前模拟器对游戏内存的记录会随着每次加载、卸载区域不断变大，所以游戏会不时停顿 0.3-0.8 秒，而且每重访一次区域就更慢一些。测试机上在一次游玩中把六个世界（1-1、2-1、3-1、4-1、5-1 和 Nexus）各进入三次，开启 "Up to 120 fps"，六个世界的平均值：

  | 轮次 | 1 | 2 | 3 |
  | --- | ---: | ---: | ---: |
  | 走动，之前 | 69.4 | 61.4 | 58.3 |
  | 走动，现在 | 75.3 | 71.1 | 69.5 |
  | 转动镜头，之前 | 68.2 | 61.3 | 55.8 |
  | 转动镜头，现在 | 72.8 | 69.9 | 68.5 |
  | 最慢的一帧，之前 | 570 ms | 649 ms | 786 ms |
  | 最慢的一帧，现在 | 49 ms | 53 ms | 52 ms |

  （"之前"为上一个版本发布后不久的构建。）
- **长时间游玩测试**：不重启连续加载 36 次区域（六个世界各六次，约 40 分钟）没有崩溃或卡死，一轮走遍 12 个区域（各世界的第一个区域，以及 1-2、1-3、1-4、2-2、2-3、3-2、3-3）也没有；内存和显存在前两三轮增长较快，之后只缓慢增长。把模拟器可用显存限制在 8 GB 时，六个世界跑三轮也没有崩溃。
- **区域加载更快**：游戏的关卡数据写入内存的速度提高了数倍（拉特利亚之塔一个 141 MB 的数据文件：从约 0.3 秒降到 0.1 秒以内）。
- **显存占用更少**：游戏已大部分归还的内存上的缓冲区，会裁剪到仍在使用的部分。
