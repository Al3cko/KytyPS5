## New in this build

- **No more "the longer you play, the slower it gets"**: until now the emulator kept every buffer and texture the
  game had ever used, in video memory and in its write tracking, so frame rates sank and RAM / video memory
  kept growing the longer you played. It now lets go of what the game has not used for a while. On the test PC,
  three rounds of the same route through 1-1 (about 12 minutes, walking and turning the camera, "Up to 120 fps" on):

  | Round | 1 | 2 | 3 |
  | --- | ---: | ---: | ---: |
  | Turning the camera, before | 60.5 | 52.7 | 48.0 |
  | Turning the camera, now | 65.2 | 60.8 | 57.7 |
  | Walking, before | 57.8 | 51.4 | 46.6 |
  | Walking, now | 62.8 | 58.9 | 55.2 |

  Video memory in use after 12 minutes: about 13 GB instead of 16 GB.
- **Fewer crashes**: the crashes in the middle of play at Boletarian Palace 1-1 (the game's job threads stopping on
  an empty scene entry) are caught in eight more places of the game; the test route that crashed within two
  rounds before now runs many rounds without a crash.
- **Higher frame rates**, mostly where many objects are drawn: the per-draw work of the renderer is lighter
  (resource tables, image sets, uploads, GPU submission batching), see the table below.
- **No more short pauses from audio streams**: the ambience and music streams are read ahead in the background,
  so the game no longer waits for the disk at each piece (pauses of up to a third of a second, mostly right
  after the PC wrote a lot to the disk).
- **F6 records your position** to `_Build\waypoints\route-<time>.txt` (for testing; the window title counts the
  points).

## Frame rates on the test PC

i9-14900K + RTX 5090, 2560×1440, "Up to 120 fps" on: average fps over 10 seconds (1% low in brackets) standing
still, walking around and turning the camera at one spot of each area, the previous release's numbers in
brackets on the second line. Runs of the same build vary by a few fps, and the 1% lows more.

| World | Standing | Moving | Turning the camera |
| --- | ---: | ---: | ---: |
| 1-1 Boletarian Palace | 63 (56) | 82 (51) | 85 (53) |
| previous release | 60 (54) | 80 (50) | 83 (53) |
| 1-2 Boletarian Palace | 72 (61) | 78 (54) | 80 (57) |
| previous release | 62 (55) | 74 (48) | 76 (28) |
| 1-3 Boletarian Palace | 69 (60) | 74 (40) | 81 (60) |
| previous release | 69 (59) | 67 (36) | 76 (57) |
| 3-1 Tower of Latria | 67 (58) | 74 (52) | 78 (56) |
| previous release | 60 (52) | 58 (37) | 64 (44) |
| 4-1 Shrine of Storms | 75 (65) | 89 (64) | 87 (43) |
| previous release | 74 (62) | 84 (58) | 85 (48) |
| 5-1 Valley of Defilement | 84 (70) | 91 (46) | 96 (43) |
| previous release | 80 (67) | 97 (60) | 102 (67) |

5-1: walking and turning there vary the most between runs (three runs of today's builds: 91 to 103 moving, 96 to 109
turning, 1% lows 43 to 70).

## 本版本的变化

- **解决"玩得越久越慢"的问题**：此前模拟器会一直保留游戏用过的所有缓冲区和纹理（占着显存，也一直在做写入跟踪），所以玩得越久帧率越低，内存和显存也一直增长。现在会释放游戏一段时间没再用到的部分。测试机上沿同一条 1-1 路线连续跑 3 圈（约 12 分钟，边走边转镜头，开启 "Up to 120 fps"）：

  | 圈数 | 1 | 2 | 3 |
  | --- | ---: | ---: | ---: |
  | 转动镜头，之前 | 60.5 | 52.7 | 48.0 |
  | 转动镜头，现在 | 65.2 | 60.8 | 57.7 |
  | 走动，之前 | 57.8 | 51.4 | 46.6 |
  | 走动，现在 | 62.8 | 58.9 | 55.2 |

  12 分钟后的显存占用约 13 GB（之前约 16 GB）。
- **崩溃更少**：在波雷塔利亚王城 1-1 游玩中途的崩溃（游戏的任务线程遇到空的场景节点）又在 8 处被拦住；之前两圈内必崩的测试路线，现在连续跑很多圈都不再崩溃。
- **帧率提升**，主要在物体多的地方：渲染器每个绘制调用的开销更低（资源表、图像集、上传、GPU 提交的合批），见下表。
- **不再因为音频流出现短暂停顿**：环境音和音乐流会在后台预读，游戏不再每读一段就等一次磁盘（最长约 0.3 秒的停顿，多见于电脑刚往磁盘写过大量数据之后）。
- **F6 记录当前位置**到 `_Build\waypoints\route-<时间>.txt`（测试用，窗口标题会显示已记录的点数）。

## 测试机上的帧率

i9-14900K + RTX 5090，2560×1440，开启 "Up to 120 fps"：每个区域的一个位置上站立、走动、转动镜头各 10 秒的平均帧率（括号内为 1% low），每个世界的第二行是上一个版本的数值。同一版本多次运行会有几帧的差异，1% low 的差异更大。

| 世界 | 站立 | 移动 | 转动镜头 |
| --- | ---: | ---: | ---: |
| 1-1 Boletarian Palace | 63 (56) | 82 (51) | 85 (53) |
| 上一个版本 | 60 (54) | 80 (50) | 83 (53) |
| 1-2 Boletarian Palace | 72 (61) | 78 (54) | 80 (57) |
| 上一个版本 | 62 (55) | 74 (48) | 76 (28) |
| 1-3 Boletarian Palace | 69 (60) | 74 (40) | 81 (60) |
| 上一个版本 | 69 (59) | 67 (36) | 76 (57) |
| 3-1 Tower of Latria | 67 (58) | 74 (52) | 78 (56) |
| 上一个版本 | 60 (52) | 58 (37) | 64 (44) |
| 4-1 Shrine of Storms | 75 (65) | 89 (64) | 87 (43) |
| 上一个版本 | 74 (62) | 84 (58) | 85 (48) |
| 5-1 Valley of Defilement | 84 (70) | 91 (46) | 96 (43) |
| 上一个版本 | 80 (67) | 97 (60) | 102 (67) |

5-1：那里的走动和转动镜头在多次运行之间差异最大（今天三次运行的结果：走动 91 到 103，转动镜头 96 到 109，1% low 43 到 70）。
