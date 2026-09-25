# BATCH31-32 一次性 qrenderdoc GUI 验收单

状态：T30 原功能及公共事件树最短复验均已由用户确认；T31 自动与 GUI L4
已通过，GUI 导出入口由用户本轮免复验。PHASE31、PHASE32 和 BATCH31-32
已关闭。本单以下步骤保留为验收记录。

## 打开位置

先用 ⌘Q 完全退出旧 qrenderdoc。在 Finder 按 ⌘⇧G，粘贴
`/Users/crossous/Developer/renderdoc-metal/build-macos-debug/bin/`，双击
`qrenderdoc.app`。在程序内按 ⌘O；文件窗口按 ⌘⇧G，粘贴
`/Users/crossous/Developer/renderdoc-metal/captures/metal-smoke/`，先打开
`t30_capture.rdc` 完成最短事件树复验，然后在**同一程序进程**按 ⌘O 打开
`t31_capture.rdc`。

| 项目 | 完整路径 |
| --- | --- |
| qrenderdoc | `/Users/crossous/Developer/renderdoc-metal/build-macos-debug/bin/qrenderdoc.app` |
| T30 | `/Users/crossous/Developer/renderdoc-metal/captures/metal-smoke/t30_capture.rdc` |
| T31 | `/Users/crossous/Developer/renderdoc-metal/captures/metal-smoke/t31_capture.rdc` |

构建程序 SHA-256 `d80297e329c9…`，内嵌 replay 库 `7a494e202d1b…`；
T30/T31 capture SHA-256 分别为 `8bfe33501fad…`、`f5ad1c86660b…`。
已完成 native、capture/XML、Replay API 的像素/原始字节/状态/描述符/usage/seek、
异常捕获拒绝、联合旧场景定向；本次公共事件层级和 render pass 变更在最终库上
又通过 17 份 Replay API 与逐份 CLI replay、18 份 capture × 10 轮 lifecycle。
L3 条件未触发，故未执行全量回归。

## T30：compute sampler 直接绑定

无筛选的 Event Browser 会显示状态调用；输入 `$action()` 后状态行会隐藏，
清空过滤框会恢复。按以下 EID 依次看，均在 `t30_capture.rdc` 中：

T30 原功能已验收。本次公共事件树改动仅需最短复验：打开 T30，清空筛选框，
确认 EID 4 `Begin Metal Compute Pass` 和 EID 13 `End Metal Compute Pass`
在顶层而非 `Copy/Clear Pass` 子项；输入 `$action()` 后两行仍显示，
EID 8/11 `setSamplerState` 隐藏。通过后不必重做下面的完整 T30 步骤。

1. **EID 4 `Begin Metal Compute Pass`**：此时右侧 Output 为空是正确的。
   手动在 Texture Viewer 主画面选择 **Texture 20** 或 **Texture 21**；
   两者应为黑色/零值，这是 dispatch 前的资源初值，并非 EID 4 的输出绑定。
2. **EID 8 `setSamplerState` → EID 9 `dispatchThreadgroups`**：在 API Inspector
   应看到 `setSamplerState` 的 slot **0**；在 **Pipeline State → CS → Samplers**
   应看到 slot **0、Sampler 22、Point/Point、Clamp Edge**。同页 Read-Only
   Textures 是 **Texture 19**，Read-Write Textures 是 **Texture 20**。
   双击 sampler 应进入 Resource Inspector；双击纹理应进入 Texture Viewer。
   Texture 20 主画面应是 8×8 渐变，放大后各格清晰；左上像素约
   **RGBA (16,32,24,255)**。Texture Viewer 右侧 Input/Output 小图均应有内容。
3. **EID 11 `setSamplerState` → EID 12 `dispatchThreadgroups`**：API Inspector
   的 slot 仍为 **0**；CS Samplers 换为 **Sampler 23、Linear/Linear、Clamp Edge**，
   Read-Write Textures 换为 **Texture 21**。Texture 21 的颜色仍为渐变，但和
   Texture 20 不同；左上像素约 **RGBA (20,38,30,255)**。Texture Viewer
   右侧 Input/Output 小图均应有内容。
4. **EID 19 `drawPrimitives(4)`**：最终画面应为铺满窗口的平滑、由暗到亮的
   渐变。Texture Viewer 右侧 Output **Texture 32** 的小图应显示同样内容。
   状态栏应显示 `No problems detected`。

在 EID 12 的 Pipeline State 页面尝试 **Export to HTML**；在 Texture Viewer
选择 **Texture 21** 后使用 **Save Texture / DDS**，建议保存到
`captures/metal-smoke/t30-linear-ui.dds`。只需确认入口可用并记录保存位置；
自动参考 `t30_linear.dds` 已核对为 384 字节，无需手算像素。

## T31：compute 批量资源绑定

在同一 qrenderdoc 进程中打开 `t31_capture.rdc`，按 EID 顺序检查：

1. **EID 7 `Begin Metal Compute Pass`**：它应是事件树顶层一行，位于此前的
   blit 事件之后；**EID 23 `End Metal Compute Pass`** 也应是顶层一行。
   本 capture 中不再出现 `Copy/Clear Pass #1` 是正常的：自动分组会避开真实
   pass 边界，而前面的 blit 操作不足以独立形成虚拟分组。EID 7 尚未绑定 compute 输出，右侧
   **Output 为空是正确的**。要检查预先清零的资源，请在 Texture Viewer 主画面
   手动选择 **Texture 20** 或 **Texture 21**，应为黑色/零值；Buffer Viewer
   手动选择 **Buffer 24**，原始字节应为 `A5` 哨兵。这些是资源初值，
   不代表 EID 7 有 Output attachment。
2. **EID 12 `setTextures`、EID 13 `setSamplerStates`、EID 14 `setBuffers`**：
   无筛选的 Event Browser 应有三行和这些 EID；API Inspector 的 `range`
   结构依次是 **location=1, length=3**（slot 1/2/3）、
   **location=2, length=2**（slot 2/3）、**location=4, length=3**
   （slot 4/5/6）。这些是起始位置与数量，界面显示 `1, 3` 等是正确的。
   对应数组中间一项为 null。把 Event Browser 的筛选框改为 `$action()`，
   三行应隐藏、dispatch 保留；清空筛选框恢复三行。
3. **EID 18 `dispatchThreadgroups`**：CS 页 Read-Only Textures 应有
   **slot 1 / Texture 19**，Read-Write Textures 是 **slot 3 / Texture 20**；
   Samplers 是 **slot 2 / Sampler 26 / Point**；Read-Only Buffers 是
   **slot 4 / Buffer 23 / offset 32**，Read-Write Buffers 是
   **slot 6 / Buffer 24 / offset 64**。启用 **Show Unused Items** 后还能看到
   未使用的 **Texture 22 / slot 5、Sampler 28 / slot 5、Buffer 25 / slot 8**；
   关闭后它们隐藏。双击 Texture、Sampler、Buffer 行分别应进入标准
   Texture Viewer、Resource Inspector、Buffer Viewer。Texture 20 主画面
   是有色渐变；右侧 Input/Output 小图都应有内容。Buffer 24 从 offset 64
   起已写入，前 64 字节仍是 `A5`。**空槽也在这个 EID 一次检查**：
   从菜单 **Window → Pipeline State** 打开独立窗口，选 **CS / Compute Shader**，
   点击窗口顶栏 **Show Empty Items**，应分别看到 texture slot **2**、
   sampler slot **3**、buffer slot **5** 为 Empty。这三项证明批量 null 清除了
   先前绑定；它们不在 API Inspector 的参数视图里。
4. **EID 19 `setTexture`、EID 20 `setSamplerState`、EID 21 `setBuffer` →
   EID 22 `dispatchThreadgroups`**：CS slot 3 改成 **Texture 21**，sampler
   slot 2 改为 **Sampler 27 / Linear**，Buffer 24 的 slot 6 offset 改为
   **80**。Texture 21 主画面为不同于 EID 18 的渐变；左上约
   **RGBA (19,38,30,255)**。Buffer 24 的后段写入仍可见，前 64 字节保持
   `A5`；右侧 Input/Output 小图应有内容。
5. **EID 29 `drawPrimitives(4)`**：最终画面为铺满窗口的渐变，右侧 Output
   **Texture 39** 小图有内容，状态栏为 `No problems detected`。

**T31 已完成的最短复验记录**：完全退出并重新打开上面的 app，再打开 T31。清空 Event Browser
筛选框，确认 EID 7 与 EID 23 都在顶层，且没有虚拟 `Copy/Clear Pass #1`；
选 EID 7 确认右侧 Output 为空、手动选 Texture 20 可见黑色；筛选框输入
`$action()`，Begin/End 两行应仍显示，而 EID 12/13/14 状态调用隐藏。
清空筛选框，选 EID 18，在 **Window → Pipeline State → CS** 启用
**Show Empty Items**，确认 texture 2、sampler 3、buffer 5 为 Empty。
Render pass EID 24/30 现在
分别标为 `C0=Clear` / `C0=Store`；compute pass 没有 attachment load/store 标注。

以下导出步骤已由用户明确免除本轮 GUI 复验，保留供以后按需检查：在 EID 22
的 CS Pipeline 页面尝试 **Export to HTML**。Buffer Viewer 选择
**Buffer 24**，确认格式化表格的 **Export to CSV** 入口，并找到
**Export to Bytes / Save Bytes**，建议保存
`captures/metal-smoke/t31-buffer-ui.bin`；Texture Viewer 选择 **Texture 21**
保存 DDS 到 `captures/metal-smoke/t31-linear-ui.dds`。自动参考
`t31_linear.dds` 为 384 字节，`t31_linear.dds.bin` 为 336 字节；自动数据
已核对，GUI 导出入口本轮未实测，且不再列为待验项。

## 回复格式

用户已反馈 T30 公共事件树符合预期，本批无待验项。后续若发现差异，请写
`T 编号 + EID + 实际看到的内容`，必要时附截图；我会先分析并给最短复验步骤。
