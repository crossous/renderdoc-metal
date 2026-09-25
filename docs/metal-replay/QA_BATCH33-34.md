# BATCH33-34 一次性 qrenderdoc GUI 验收单

状态：用户已确认 T32/T33 原单的参数、资源、画面、CS 间接参数栏，
并确认 Event Browser 显示实际执行的 threadgroup 数量 `<2, 2, 1>`。
两阶段及批次已关闭。以下保留当时的验收步骤作为历史记录。

## 已通过的摘要最短复验

当时完全退出旧 app，按下方路径重新打开最终 app，在同一进程依次打开 T32/T33。
Event Browser 的筛选设置中启用 **Show custom action names**，分别选 T32
**EID 12** 与 T33 **EID 18**。两行均显示
`dispatchThreadgroups(indirect, <2, 2, 1>)`。尖括号内是参数 buffer 在
该次执行中的实际 threadgroup 数量；API Inspector 仍显示 buffer、offset
和 `{4,4,1}` 每组线程数。用户已回复确认；CS 栏位、Buffer Viewer 和
画面也已验收。

## 打开位置

先用 ⌘Q 完全退出旧 qrenderdoc。在 Finder 按 ⌘⇧G，粘贴
`/Users/crossous/Developer/renderdoc-metal/build-macos-debug/bin/`，双击
`qrenderdoc.app`。在程序内按 ⌘O；文件窗口按 ⌘⇧G，粘贴
`/Users/crossous/Developer/renderdoc-metal/captures/metal-smoke/`，先打开
`t32_capture.rdc`，随后在**同一程序进程**按 ⌘O 打开 `t33_capture.rdc`。

| 项目 | 完整路径 |
| --- | --- |
| qrenderdoc | `/Users/crossous/Developer/renderdoc-metal/build-macos-debug/bin/qrenderdoc.app` |
| T32 | `/Users/crossous/Developer/renderdoc-metal/captures/metal-smoke/t32_capture.rdc` |
| T33 | `/Users/crossous/Developer/renderdoc-metal/captures/metal-smoke/t33_capture.rdc` |

GUI 程序 SHA-256 `f1ebea2ddb86…`，内嵌 replay 库 `aa21c9a6983d…`；
T32/T33 capture SHA-256 分别为 `9aa1432c8d9a…`、`4478197f9994…`。
最终库上 16 份 Replay API 和逐份 CLI replay 通过，含 T00 的 17 份
capture × 10 轮 lifecycle 通过。T32/T33 的 5/6 类异常捕获均被拒绝。
L3 条件未触发：间接 compute 与跨 encoder 参数依赖已由批内和定向旧场景
覆盖，本批未进入发布或合并门槛。自动保存的两份 8×8 DDS 完全相同；
参数 raw 文件各 44 字节。无需手工导出或逐字节计算。

## T32：CPU 写入间接参数

在 `t32_capture.rdc` 中清空 Event Browser 筛选框，按 EID 顺序检查：

1. **EID 6 `Begin Metal Compute Pass`**：在事件树顶层。它不是写入动作，
   右侧 Output 为空是正常的。手动在 Texture Viewer 主画面选择
   **Texture 23**，此时应为黑色/零值。
2. **EID 12 `dispatchThreadgroups(indirect, <2, 2, 1>)`**：Event Browser 有一条间接
   dispatch，API Inspector 显示 `indirectBuffer` 为 **Buffer 21**、
   `indirectBufferOffset` 为 **16**、`threadsPerGroup` 为 **4,4,1**。
   打开 **Window → Pipeline State → CS**，`Indirect Dispatch` 一行应为
   **Buffer 21 / offset 16 / size 12 / Dispatch Threadgroups**。双击该行
   应进入 Buffer Viewer，格式化的三个 `uint` 应为 **2、2、1**。
   这三个数是 threadgroup 数量，与 `threadsPerGroup` 相乘得到 8×8 工作区。
3. **仍在 EID 12**：切到 **Pipeline State → CS**，Compute Shader 是
   `filter_main`；Read-Only Textures 是 **Texture 22 / slot 0**，
   Read-Write Textures 是 **Texture 23 / slot 1**，Read-Only Buffers 是
   **Buffer 19 / slot 2 / offset 32**，Read-Write Buffers 是
   **Buffer 20 / slot 4 / offset 64**。双击纹理、buffer 行可进入标准
   Viewer。Texture 23 主画面应是 8×8 有色渐变，左上像素约
   **RGBA (19,32,24,255)**；右侧 Input/Output 小图均应有内容。
4. **EID 19 `drawPrimitives(4)`**：最终画面应为铺满窗口的渐变；状态栏为
   `No problems detected`。如需核对前后切换，可点回 EID 6 再回 EID 12：
   Texture 23 应从黑色恢复为渐变。

## T33：GPU 先写参数，再间接 dispatch

在同一 qrenderdoc 进程打开 `t33_capture.rdc`，清空筛选框，按 EID 顺序检查：

1. **EID 7 `Begin Metal Compute Pass`**：这是参数写入 encoder 的起点。
   在 Buffer Viewer 手动选择 **Buffer 23**，byte offset **16–27**
   的三个 `uint` 应全为 **0**。此时右侧 Output 为空是正常的。
2. **EID 10 `dispatchThreadgroups`**：这是 GPU 参数 writer，不是最终滤镜。
   **Pipeline State → CS** 的 Compute Shader 为 `write_groups`；
   Read-Write Buffers 显示 **Buffer 23 / slot 0 / offset 16**。
   Buffer 23 的 offset 16 起三个 `uint` 此时应变为 **2、2、1**。
3. **EID 12 `Begin Metal Compute Pass`**：第二个 compute encoder 已开始，
   Buffer 23 的 **2、2、1** 仍可见；手动选择 **Texture 25** 时尚为黑色。
   Begin 本身没有 Output attachment，右侧 Output 为空是正常的。
4. **EID 18 `dispatchThreadgroups(indirect, <2, 2, 1>)`**：API Inspector 显示
   `indirectBuffer` 为 **Buffer 23**、offset **16**、`threadsPerGroup`
   为 **4,4,1**。**Pipeline State → CS → Indirect Dispatch** 应为
   **Buffer 23 / offset 16 / size 12 / Dispatch Threadgroups**；双击后
   格式化值仍是 **2、2、1**。**CS** 页应显示 `filter_main`，
   Texture 24/25 分别位于只读 slot 0 / 读写 slot 1，Buffer 21/22
   分别位于只读 slot 2 offset 32 / 读写 slot 4 offset 64。
   Texture 25 主画面应从黑色变成与 T32 相同的 8×8 渐变，左上约
   **RGBA (19,32,24,255)**；右侧 Input/Output 小图有内容。
5. **EID 25 `drawPrimitives(4)`**：最终画面是铺满窗口的渐变，状态栏为
   `No problems detected`。

无需保存 DDS、raw 或 HTML：本批未新增导出入口，自动数据已核对。
如想验证 Event Browser 筛选，输入 `$action()` 会隐藏 `setBuffer` 等
状态调用，间接 dispatch 仍在；清空筛选会恢复原来的 EID 列表。

## 回复格式

当时若两份均符合，回复 `T32/T33 全部符合` 即可。若某项不同，回复
`T32 或 T33 + EID + 实际看到的内容`；有差异时可附截图。我会先对照
自动证据分析或修复，再给最短复验步骤。未明确确认的 T 会持续留在
`QA_PENDING.md`，不会提前关闭阶段或批次。T32/T33 现均已确认，
该流程作为验收记录保留。
