# Metal Replay 历史交接证据

此文件保存此前 HANDOFF 的阶段级详细验收记录，仅在追查历史问题时读取。
当前状态与最近检查点以 STATUS.md 为准。

2026-09-23 最新 T16：`/tmp/t16-final-regression.log` 记录 T00-T16 全量 native/capture/XML/
output/data/state、逐份 CLI replay、17×10 lifecycle 全部通过，resident growth 114688 bytes。
正式 `captures/metal-smoke/t16_capture.rdc` SHA-256 为
`2f1b95128946d180d7f114ca9047bb2b90f28d43915e82ad9eca2925ff788ec5`；最新 qrenderdoc
完全重启后载入字节相同的 `/tmp/t16-ui-capture.rdc`，核对 EID 2 Event/API、VS Texture 17/
Sampler 18、Point/Clamp、Mesh VS Input、标准 Buffer Viewer 384-byte 数据、Texture/Resource
跳转、`VS - Texture` usage、四象限输出及状态栏 `No problems detected`。UI DDS 和自动 DDS
SHA-256 同为 `95597aa5bcb91a27f057df2f4f36cbeb49423ac7e5275d5ed17ca29b03ca21cf`；
`t16_pipeline_state_standard.html` 包含 VS Texture/Sampler。T03/T12 定向通过，slot 128 与空
texture/sampler 的四份派生 RDC 均在对应 chunk 拒绝。T16 已关闭，不重跑 L3/L4；下一项 P18.1。

2026-09-23 最新 T15：`/tmp/t15-final-regression.log` 记录 T00-T15 全量 native/capture/XML/
output/data/state、逐份 CLI replay、16×10 lifecycle 全部通过，resident growth 737280 bytes。
正式 `captures/metal-smoke/t15_capture.rdc` SHA-256 为
`208b794cc2ab49cf88f970cdda787bfb016f3483284b389130e05a39bae1a614`。T15 自动 smoke
验证 Point `(1,1)`、Line `(3,2)`、Line Strip `(6,3)` action/topology、clear/draw/回退、
264-byte 顶点 buffer、三个 Mesh preview、Vertex Buffer usage、DDS；非法 primitive 99 与零
vertexCount 派生 capture 均被拒绝。最新 qrenderdoc L4 在完全重启 app 后看到 EID 2/3/4、
API 的 Line Strip/6/3、IA Point List/Line List/Line Strip、三种 Mesh VS Input、标准 Buffer Viewer
全部顶点值、Resource Inspector `Vertex Buffer`、Texture Viewer 红点绿线蓝折线；
UI `captures/metal-smoke/t15_output_ui.dds` 与自动 DDS 同为 480128 bytes 且 SHA-256 一致，
`t15_pipeline_state_standard.html` 含 Line Strip 与 Buffer 16，状态栏 `No problems detected`。
T15 已关闭，不重跑 L3/L4；下一项 P17.1。

2026-09-23 最新 T10：初次完整回归通过；通用 UI 修正后最终
`/tmp/t10-final-regression-post-ui.log` 记录 T00-T10 复验通过、十一份 capture 各 10 次
lifecycle resident growth 1,277,952 字节；`/tmp/t10-final-cli-replay-post-ui.log`
记录 T00-T10 逐份 CLI replay 成功。qrenderdoc 对正式 T10 capture 的 SHA-256 相同 `/tmp` 副本
完成实机验收：Event Browser 的 `Copy/Clear Pass #1` 含 EID 1-6，API Inspector 可跳到 Buffer 18
与 Texture 20；Buffer Viewer 的 EID 2/3 为复制橙色字节/`0x60` fill，Texture 20 四象限、
Texture 21 mip3 `(134,132,88,255)` 和 EID 8 四条色带正确。Resource Inspector usage 为
`Copy - Dest`、`Clear`、`Generate Mips`；UI 保存的全 mip `t10_mips_ui_final.dds` 与自动产物
各 468 字节且 `cmp` 相同，状态栏 `No problems detected`。T10 已关闭，T11 不需重做。
`build_metal_dev_macos.sh` 现在会把最新 `librenderdoc.dylib` 同步进 qrenderdoc app 包，避免
只改 replay 代码时 UI 加载旧库；若手工增量构建后直接启动 UI，应先比较 app 包内库与 build 库。

2026-09-23 最新 T11：`/tmp/t11-final-regression.log` 记录 T00-T11 全量回归、逐份 CLI replay、
12×10 lifecycle 通过，resident growth 999,424 字节。正式 capture
`captures/metal-smoke/t11_capture.rdc` 与 UI 副本 `/tmp/t11-ui-capture-final.rdc` 的 SHA-256
同为 `72950dd1639faff10234f247224495a89c81493a6d1783bd7ab310e6b551a733`。qrenderdoc
EID 2 CS 页显示 `filter_main`、Texture 19 只读、Texture 20 读写；EID 1→2 的 Texture 20
由全黑变为 swizzle，首像素 `(16,32,24,255)`，EID 5 final draw 正确。Resource Inspector usage、
资源跳转、`/tmp/t11_pipeline_state_standard.html`、UI/自动 384-byte DDS `cmp` 和状态栏
`No problems detected` 均通过。T11 已关闭，T12 不需重做。

2026-09-23 最新 T12：`/tmp/t12-final-regression.log` 记录 T00-T12 全量回归、逐份 CLI replay、
13×10 lifecycle 通过，resident growth 671,744 字节。正式 capture
`captures/metal-smoke/t12_capture.rdc` SHA-256 为
`d13c1ef93e76340e7c530d5a753bb5f74fd0a2b967be4699dc2cff1f0726150f`。自动 smoke 已验证
argument buffer 0、Texture 17 `id(0)`、Sampler 18 `id(1)`、通用 descriptor/reflection、
`PS_Constants/PS_Resource` usage、clear/draw seek、四象限、64-byte readback 和 192-byte DDS。
最新 qrenderdoc L4 已在完全重启 app 后通过：Event Browser EID 2、FS `Buffer 20` / `Texture 17` /
`Sampler 18`、标准 Buffer/Texture/Resource 跳转、四象限、UI DDS 与 Pipeline HTML export 均正确，
状态栏为 `No problems detected`。`/tmp/t12_argument_texture_ui.dds` 与自动 DDS 均为 192 bytes 且
SHA-256 相同；HTML 位于 `captures/metal-smoke/t12_pipeline_state_standard.html`。首次打开使用的是
构建前已启动、持有旧 replay dylib 的 qrenderdoc 进程，完全退出并启动最新 app 后即正常；这不是
capture/replay 缺陷。T12 已关闭，不重跑 L3/L4。

2026-09-23 最新 T13：`/tmp/t13-final-regression.log` 记录 T00-T13 全量回归、逐份 CLI replay、
14×10 lifecycle 通过，resident growth 1,294,336 bytes。正式 capture
`captures/metal-smoke/t13_capture.rdc` 的 Buffer 18 在 offset 16 保存 `3/2/1/1`；自动 smoke
验证 indirect action/usage、clear/draw seek、左右像素和 16-byte raw export。由 XML+ZIP 派生的
offset 17（未对齐）和 36（越界）capture 均被 replay 明确拒绝。最新 qrenderdoc L4 在 EID 2
看到 `drawPrimitives(indirect, 3 vertices, 2 instances)`；API/IA Pipeline/Resource Inspector 与
标准 Buffer Viewer 均指向 Buffer 18，Viewer 范围为 16/16、四列值为 `3/2/1/1`。UI 导出的
`captures/metal-smoke/t13_arguments_ui.bin` 与自动 `.bin` 完全一致；
`t13_pipeline_state_standard.html` 包含 Indirect Buffer，状态栏为 `No problems detected`。
T13 已关闭，不重跑 L3/L4；下一项 P15.1。

2026-09-23 最新 T14：`/tmp/t14-final-regression.log` 记录 T00-T14 全量回归、逐份 CLI replay、
15×10 lifecycle 通过，resident growth 1,015,808 bytes。正式
`captures/metal-smoke/t14_capture.rdc` 使用 UInt16 index buffer offset 4、`baseVertex=1`、
`instanceCount=2`、`baseInstance=1`，原生和 replay 均为左红右蓝、中心背景。自动 smoke 验证
index/vertex/instance 数据与 usage、clear/draw/rewind、两个 Mesh instance、4/6 byte 标准
Buffer Viewer 区间及 6-byte raw export；派生 offset 3（未对齐）与 10（越界）RDC 均被拒绝。
最新 qrenderdoc L4 在 EID 2 的 Event/API、IA Buffer 16/17/18、Resource Inspector
`Index Buffer`、标准 Buffer Viewer `0/1/2`、Mesh instance 0/1、Texture Viewer 左右输出均一致。
UI 导出 `captures/metal-smoke/t14_indices_ui.csv` 为 `0/1/2`；
`t14_pipeline_state_standard.html` 包含 Triangle List 与 Buffer 16/17/18。状态栏
`No problems detected`。T14 已关闭，不重跑 L3/L4；下一项 P16.1。

2026-09-23 T09：一键脚本生成 `captures/metal-smoke/t09_capture.rdc`，完整 T00-T09 回归和
十份 capture 各 10 次 lifecycle 通过。qrenderdoc EID 2 FS 页显示 Texture 17/18/19 分别为
2D/2D Array/Cube，Sampler 20 为 Point/Clamp；Texture Viewer 的三层 mip、三层 array slice、
六个 cube face 可选择，X-/Z- 等固定色已实机核对。macOS 26 的 Qt 5 combo popup 崩溃，子资源框
在 macOS 上改为点击逐项循环、键盘方向键/Home/End 选择。标准 Save Texture 对话框导出全部 cube
faces 到 `captures/metal-smoke/t09_cube_ui.dds`（512 字节），状态栏为 `No problems detected`。
本次 L4 对最新 `.rdc` 使用 `/tmp/t09-ui-capture.rdc` 字节拷贝，因为直接从 macOS `Documents`
路径启动 qrenderdoc 偶发卡在文件 open；CLI replay 对原路径正常。T10 不需重做 T09 验收。

较早的 T02 qrenderdoc 验证直接打开 `captures/metal-smoke/t02_capture.rdc`。Event Browser 显示 EID 2/3 两次
`drawIndexedPrimitives(36)`；EID 3 API Inspector 标明 `MTLIndexTypeUInt32, Buffer 20`，Texture Viewer
列出 `FB0 Texture 27` 与 `DS Texture 17` 并正确显示双立方体，状态栏无错误。Pipeline State 进一步
验证 EID 2 的左 viewport/scissor 与 UInt16 Buffer 19/72、EID 3 的右 viewport/scissor 与 UInt32
Buffer 20/144，并显示 Float3/Float4、stride 28、less/write、back/CCW。P3.4 随后已接通通用
`GetVertexInputs()` 与最小 Metal `RenderMesh()`：EID 2 Mesh Viewer 显示 UInt16 展开的 `attr0/attr1`
和立方体线框；Buffer 18 自动解析 Float3/Float4，Buffer 19/20 分别以 `ushort`/`uint index` 显示
72/144-byte 范围。T03 最新 qrenderdoc 验证已选择 EID 2：Pipeline 显示 `Triangle Strip`、
Float2/Float2、Fragment Texture 17（RGBA8）和 Sampler 18（Point/Point/None、ClampEdge x3），四象限
图像正确。双击 Texture 17 进入标准 Texture Viewer，双击 Sampler 18 进入 Resource Inspector；
所有表已经切换到标准 RDTree，并接入通用 context/usage 与 thumbnail/preview 分发。Export 控件已在
T03 EID 2 实际写出 `/tmp/metal-pipeline-t03.html`，核对包含五阶段、Triangle Strip、Texture 17 和
Sampler 18；该次验证状态栏无错误。

生命周期回归位于 `util/test/metal/metal_replay_lifecycle_smoke.mm`，由一键脚本自动编译执行。它会在
同一进程中打开/关闭 T00-T13 各 10 次，并验证 unsupported 接口与 resident growth。完成 T12 后的
最新完整回归值为 671,744 字节；失败阈值为 64 MiB。新增资源 wrapper
必须沿用 D017 的所有权规则。

当前 texture data 回归位于 `util/test/metal/metal_replay_output_smoke.mm`：它在 T01 clear/draw EID
读取 400x300 BGRA backbuffer，校验背景字节和拾取浮点值，再保存
`captures/metal-smoke/t01_texture.dds`。readback helper 为
`MetalReplay::ReadTextureSubresource()`，基础路径由 D014 固定，mip/array/cube 扩展由 D031 记录；
扩展格式时必须增加对应 fixture 和数据断言。

2026-09-22 的最新 Pipeline UI 验证：最终构建已将 Metal 页面重排为 Controls + 标准
`PipelineFlowChart` + IA/VS/RS/FS/OM 阶段页，并将表格切换到标准 RDTree。T03 EID 2 的 FS 页显示
Function 14、Texture 17、Sampler 18，texture 双击进入 Texture Viewer；Export 实际生成并核对五阶段
HTML。`Show Empty Items` 在 IA/OM 显示红色空 index/depth 槽。T02 EID 2 的 IA 显示 Buffer 19/UInt16，
RS 显示左 viewport/scissor、Back/CCW，OM 显示 Texture 27、Depth Texture 17 和 Less/Write。状态栏
均为 `No problems detected`。第三切片还从 Metal pipeline argument reflection 枚举 T03 的
`colourTexture`/`colourSampler`；同一资源绑定到未声明 slot 1 后，默认表只显示 slot 0，启用
`Show Unused Items` 后 texture/sampler 表均显示真实 slot 1；该次 T03 验证已完成。

2026-09-22 的 T04 qrenderdoc 验证：EID 2/3 的 Texture Viewer 分别显示左红/右背景与左红/右绿；
FS Constant Buffers 分别显示 `Buffer 16 / offset 0 / size 512 / needed 16` 和
`Buffer 16 / offset 256 / size 256 / needed 16`。双击 EID 3 binding 后，标准 Buffer Viewer 的 byte
range 为 256/256，开头四个值为 `.0625/.875/.1875/1.0` 对应的原始 float 字节。状态栏为
`No problems detected`；当前最终构建保持运行在 T04 EID 3 FS 页。

2026-09-22 的 T05 qrenderdoc 验证：EID 2 Texture Viewer 显示红、绿、蓝三个实例；IA 显示
`Buffer 16 / 24 / stride 8 / Vertex / 1` 和 `Buffer 17 / 96 / stride 24 / Instance / 1`。Mesh Viewer
instance 0/1 按 base instance 读取 record 1/2，offset 为 `-0.55/0.00`，instance 1 colour 为
`.0625/.875/.1875/1.0`。两个 buffer 均进入标准自动格式 Buffer Viewer，状态栏为
`No problems detected`；最终进程保持运行在 T05 capture。

2026-09-22 的 T06 qrenderdoc 验证：EID 3 Texture Viewer Outputs 列出 FB0 Texture 24 与 FB1
Texture 17，前者显示 alpha blending 后的红褐背景/重叠三角形，后者显示 RGB write-mask 结果的蓝底
黄三角。OM 页显示两个 Color Targets；Blend State slot 0 为
`True / Src Alpha / 1 - Src Alpha / Add / One / Zero / Add / RGBA`，slot 1 为 disabled 且 `RGB_`。
实际导出的 `captures/metal-smoke/t06_pipeline_state_standard.html` 包含同一状态，状态栏为
`No problems detected`；最终进程保持运行在 T06 EID 3 OM 页。

2026-09-22 的 T07 qrenderdoc 验证：EID 6 Texture Viewer 显示左绿右蓝三角形；OM 页显示 color
Texture 27、combined depth/stencil Texture 20、`Less / Enabled` depth state，以及 Front/Back
reference 5、compare mask `000000FF`、write mask `00000000`、Equal、`Inc Sat/Dec Sat`。共享
PipelineFlowChart 的 Home/End 键导航已实机从 IA 往返 OM。UI 实际导出的
`captures/metal-smoke/t07_pipeline_state_standard.html` 包含同一状态，状态栏为
`No problems detected`；最终进程保持运行在 T07 EID 6 OM 页。

2026-09-22 的 T08 qrenderdoc 验证：EID 4 OM 页显示 Multisample State 为
`4 / Enabled / Disabled`，Color Targets 为 `Texture 17 / Texture 2D MS / 4 samples`，Resolve Targets
为 `Texture 24 / Texture 2D / 1 sample`。双击 resolve 行进入标准 Texture Viewer 后显示左红、中绿、
右蓝三角形，中心拾取为 `(0.06275, 0.87451, 0.18824, 1.00)`。实际导出的
`captures/metal-smoke/t08_pipeline_state_standard.html` 包含同一状态，状态栏为
`No problems detected`；最终进程保持运行在 T08 EID 4。

最近一次 qrenderdoc 手工验证：打开 `captures/metal-smoke/t01_capture.rdc`，选择 EID 2，在纹理
坐标 `(202, 128)` 右键拾取得到 `(0.61176, 0.34118, 0.32157, 1.00)`；保存按钮成功写出
`captures/metal-smoke/t01_texture_ui.dds`，状态栏保持 `No problems detected`。

2026-09-21 的 Shader Viewer 验证：新构建 qrenderdoc 加载 T01 后，Resource Inspector 的
Function 13/14 均出现 “View Contents”；分别打开后都显示 `captured.metal` 的真实 MSL，入口为
`vs_main` 与 `fs_main`，状态栏保持 `No problems detected`。若本机布局保存过 D3D11 Pipeline State
子页面，当前代码会在加载 Metal capture 时清空该不匹配页面，避免旧 backend viewer 崩溃。

2026-09-21 的 Pipeline State 验证：完整 smoke 会断言 T01 draw 的真实 pipeline/shader/topology/
vertex buffer/color target。随后已关闭旧 qrenderdoc、启动本次新构建并打开最新 T01；EID 2 的
Metal Pipeline State 页面显示 `Pipeline State 15`、`Triangle List`、Vertex/Pixel 的
`Function 13/14` 与 `vs_main/fs_main`、`Buffer 16`（offset 0、size 96）及 `Texture 23`
（mip/slice 0），状态栏保持 `No problems detected`。

2026-09-21 的阶段关闭 UI 验证：关闭旧进程并启动最新 qrenderdoc，同一进程依次完成
`T01 -> Close -> T00 -> Close -> T01`。重开 T01 后 EID 2 仍显示彩色三角形和上述 Pipeline State；
T00/T01 状态栏均为 `No problems detected`。Texture Viewer 的 History/Debug 按钮分别显示
`Pixel History not supported on this API` 与 `Shader Debugging not supported on this API` 并保持禁用。
