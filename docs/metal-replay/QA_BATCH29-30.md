# BATCH29-30 一次性 qrenderdoc GUI 验收单

验收结果：用户已在同一轮 qrenderdoc 确认下列 T28/T29 缩略图与 T29
`$action()` 筛选均正常；此前其余 L4 项与 T28 DDS 也已验收。本单归档。

## 2026-09-24 最终余项（已验收）

T28/T29 已通过的画面、Pipeline、Buffer 20 变化、导出、状态栏和首次 ⌘O
无需重复验收。T28 的 10×7 UI DDS 已由 agent 核对，与自动参考逐字节相同。
T22 父行 EID 5 展开及绘制也已由用户确认。Metal 帧内状态调用现在拥有 EID，
以下编号对应当前正式 captures。缩略图修复后，qrenderdoc 程序路径不变，
但须完全退出并重开，以载入新的内嵌 replay 库。

先按 ⌘Q 完全退出旧程序。在 Finder 按 ⌘⇧G，进入
`/Users/crossous/Developer/renderdoc-metal/build-macos-debug/bin/` 并双击
`qrenderdoc.app`。在 qrenderdoc 按 ⌘O，文件窗口按 ⌘⇧G 进入
`/Users/crossous/Developer/renderdoc-metal/captures/metal-smoke/`，依次打开下表 `.rdc`。

| 用途 | 完整路径 |
| --- | --- |
| T28 | `/Users/crossous/Developer/renderdoc-metal/captures/metal-smoke/t28_capture.rdc` |
| T29 | `/Users/crossous/Developer/renderdoc-metal/captures/metal-smoke/t29_capture.rdc` |

1. **T28，EID 7** `dispatchThreads(7x5x1, 4x3x1)`：在 Texture Viewer 右侧
   **Input/Output** 列表看 **Input Texture 19** 和 **RW Output Texture 20**。
   两个条目内的小图都应有颜色内容；Output 左上 7×5 有渐变，右侧三列和底部
   两行不出现渐变（透明区可能显示棋盘格）。再点 **EID 14** `drawPrimitives(4)`，右侧 **Output Texture 30**
   的小图应有渐变画面。这里只看右侧小图，无需再保存 DDS。
2. **T29，EID 10/11/12**：先在 Event Browser 输入 `$action()`，确认 EID
   **10/11** 的 `setBuffer` 隐藏，dispatch 仍显示 EID **12**；清空过滤框后
   两条 `setBuffer` 重新出现。再选 **EID 12**，Texture Viewer 右侧
   **Input Texture 21** 与 **RW Output Texture 22** 的小图都应有颜色内容。
   选 **EID 19** `drawPrimitives(4)`，右侧 **Output Texture 34** 小图应有
   渐变画面。

状态栏应为 `No problems detected`。全部符合可回复：
`T28 缩略图正常；T29 $action() 正常、缩略图正常`。
若只做了部分，写已完成的 T 编号；其余持续待验。若有差异，写
`T 编号、EID、实际看到的内容`，必要时附截图。
