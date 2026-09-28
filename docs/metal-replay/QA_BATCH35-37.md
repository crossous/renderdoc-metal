# BATCH35-37 后续 qrenderdoc GUI 验收单

状态：仅自动验证通过；本轮按用户要求没有执行任何 GUI/Computer Use。以下项目留给
后续 chat，由 agent 指导用户一次完成。未收到明确反馈前，T34/T35/T36 均保持待验。

后续 BATCH38 已扩展 T36，并增加 T37/T10 marker；请合并 `QA_BATCH38.md` 验收。
下表已更新为最终库及新版 T36，旧版 EID 不再沿用。

## 固定产物

| 项目 | 路径 / SHA-256 |
| --- | --- |
| qrenderdoc | `/Users/crossous/Developer/renderdoc-metal/build-macos-debug/bin/qrenderdoc.app`；主程序 `f1ebea2ddb86…` |
| replay 库 | `build-macos-debug/lib/librenderdoc.dylib` 与 app 内嵌库均为 `47e0635e8736…` |
| T34 | `captures/metal-smoke/t34_capture.rdc`；`e6ee77deb50a…` |
| T35 | `captures/metal-smoke/t35_capture.rdc`；`7fbed8c95df6…` |
| T36 | `captures/metal-smoke/t36_capture.rdc`；`436b3262fef5…` |

## T34：inline bytes 与 batch buffers

1. 打开 T34，清空 Event Browser 筛选；确认 draw 前可见 `setVertexBuffers`、
   `setVertexBufferOffset`、`setVertexBytes`、`setFragmentBuffers`、`setFragmentBytes`。
2. 在 API Inspector 核对 vertex range `(0,2)`、offsets `16,0`，随后 slot 1 offset
   更新为 256；fragment range `(3,2)`、offsets `0,16`；两项 inline data 分别为
   8 bytes/slot 2 与 16 bytes/slot 2。
3. 选择 draw，在 Pipeline State 核对 VS Storage Buffers slot 0/1 的 offset
   16/256，FS Buffers slot 3/4 的 offset 0/16。inline bytes 当前没有独立 ResourceId，
   slot 2 不应错误显示成先前 buffer；数据以 API Inspector 为准。
4. Texture Viewer 中央像素约为 RGBA `(0.5625,0.5,0.5,1)`，画面铺满；资源行可打开
   Buffer Viewer，状态栏为 `No problems detected`。

## T35：command 创建变体

1. Event Browser 应有两个 Compute Pass、各一条 dispatch，随后一个只清屏的 Render Pass；
   不应出现伪 draw。API Inspector 中第一条 encoder 为 Concurrent，第二条 descriptor
   encoder 为 Serial；第一条 command buffer commit 后应记录 `waitUntilScheduled`。
2. 分别选择两条 dispatch，CS Pipeline 的 output buffer slot 0 offset 应为 0 和 4；
   Buffer Viewer 前两个 `uint` 最终为 `17,17`。
3. 最终 Texture Viewer 是约 RGBA `(0.07,0.16,0.31,1)` 的纯色清屏；切换两个 dispatch
   与 render pass 不崩溃，状态栏为 `No problems detected`。

## T36：render 动态状态

1. draw 前六项状态调用均应出现在 Event Browser/API Inspector；API Inspector 还应能
   看到 command/render debug group 的 push/pop 和 render signpost，字符串分别为
   `T36 command debug group`、`T36 render debug group`、`T36 dynamic state`。核对 viewport 数组首项
   `0,0,400,300,0,1`，scissor `64,48,272,204`，depth clip `Clamp`，depth bias
   `1.25/2.5/3.75`，triangle fill `Fill`，blend constant `0.2/0.4/0.6/0.8`。
2. 选择 draw，在 RS 页面核对 viewport 400×300 和 scissor 64,48,272×204。depth
   clip/bias/fill/blend constant 尚无 Metal 专用 Pipeline State 字段，不要求页面显示；只确认
   API Inspector 参数和输出，不把缺失字段误记为本轮回归失败。
3. Texture Viewer 中央约 RGBA `(0.2,0.4,0.6,0.8)`，左上角为黑色不透明；scissor 外
   保持 clear 色，状态栏为 `No problems detected`。

## 回复格式

若三份均符合，回复 `T34/T35/T36 全部符合`。若不同，回复 `T 编号 + 调用或 draw +
实际看到的值/画面`；可附截图。届时先对照自动证据分析，再给最短复验步骤。
