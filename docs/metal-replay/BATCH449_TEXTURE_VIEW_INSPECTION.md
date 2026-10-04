# B449 — Metal Texture Viewer inspection

2026-10-03。沿用用户要求的节奏：局部定向检查后立即回到同一 UE；分开记录定向、全量、真实 UE 和助手 UI。不提交/推送。

## 当前检查点

- 集中回归固定库 SHA256：`74edb8eeaa9c60300a072c04930e2693332a98df7355b01cd09acf5b5a35d553`。
- 继续对照发现 mip 显示范围应保留 mip0 尺寸（公共缩放/平移/像素上下文的约定）；补丁库 `f4396853ee89e1f629be15a915afc7fc8da496720832f6f1af18020cd77906c4` 在 `metal-texture-view.godqeg` 中完成图案 mip、7×5 SInt mip 和紧随的同一 UE GBufferA 检查，均通过。不把 74ed 的回归当成 f439 的全量。
- 新原生纹理检查用例、两次 replay/reset 及紧随的同一 UE 检查通过：`build-macos-debug/metal-texture-view.dFjy83/`。
- `74edb8ee` 固定库的 308 原始捕获与 7786 畸形输入阶段完成，生命周期在第 0 轮失败：旧测试的 `ValidateUnsupportedInterfaces` 要求 Histogram 返回空数组。现已将该检查改为验证 256 桶、全像素四通道计数及 Native 原图不变；后续需重验生命周期。此轮总体不记录为全量通过。日志 `build-macos-debug/frozen-validation-74edb8ee/full-regression.log`；此前 `ff444e68` 的全量仅是历史证据。
- 助手 UI 待验：CUA 在打开应用后报告 Mac 锁定，自动解锁失败。终端 GPU 检查和 UI 不并行；已关闭本次打开的空 qrenderdoc。
- 本批数值 Overlay 结果不能代替绘制覆盖层。随后 B450–451 已补齐 Depth/Stencil/Drawcall/Wireframe/Backface Cull 的真实 Native draw 回放，分别记录于 `BATCH450_451_NATIVE_DRAW_OVERLAYS.md`；其他覆盖层仍待实现。

## 已补齐的路径

原 `GetMinMax`/`GetHistogram` 是失败占位；显示 shader 忽略数值 Overlay、gamma、RGBM/YUV，Cube 只采样面中心，背景是单色占位，像素放大框为空实现。

本次复用既有有界 Native 纹理/Buffer view readback 和公共 `formatpacking` 解码：

- 四通道 MinMax，UInt/SInt 保留 `PixelValue` union 数值；按所选 mip、array/face/volume slice 统计。
- Histogram 的 256 桶、所选通道合并、包含最大端点、排除范围外和非有限值；无通道选中返回全零。
- NaN 红、Inf 绿、负值蓝；Histogram Clipping 低端红/高端绿，其他值用共同亮度系数灰显。关闭通道先置零，避免 `NaN * 0` 污染。
- Range、单通道灰显、alpha、flipY、RGBM、YUV、sRGB/gamma 的处理顺序对照公共 HLSL/GLSL。
- UInt/SInt、Depth/Stencil、Texture Buffer 和类型转换通过共享解码生成临时 RGBA32Float 显示纹理，原资源不改写。
- 图案 Cube/Cube Array 各面正确采样；Array/3D/mip 选择；单采样的 ResolveSamples 等价 sample0。
- 64px 棋盘背景、纯色背景，以及像素上下文白色内框/黑色外框实现。
- 仅 Texture Viewer 使用 owned output 的 sRGB view，实现与 D3D12/Vulkan 一致的线性 alpha 混合；保存输出字节及 raw presentation 路径保留 UNorm。临时 view 在 GPU 完成后释放。
- 补充 mip0 显示范围约定，采样仍读取所选 mip；整数/深度临时浮点图像也携带原尺寸，避免非二次幂的两轴缩放偏差。
- UI 保留公共 Overlay 排序/枚举；Metal 未实现的绘制类项目暂时标记 N/A 并禁用，NaN/INF/负值与 Histogram Clipping 可用。恢复其他 API 时重新启用。不是绘制类 Overlay 的完成替代。

统计目前在有界 Native readback 后做 CPU reduction。常规未压缩单采样纹理支持；压缩格式统计、MSAA 样本/resolve 统计仍待补齐，不宣称所有 Metal 格式支持。Native readback 仍受既有 128MiB/8192px/256 depth 上限限制，显示临时浮点图像也限制为 128MiB。

## 对照的仓库源码

- `renderdoc/data/hlsl/texdisplay.hlsl`、GLSL counterpart：通道、NaN/Inf/负值、clipping、RGBM/YUV、灰显及 gamma 的顺序与颜色。
- `renderdoc/data/hlsl/histogram.hlsl`：256 桶、合并所选通道、包含最大端点。
- `renderdoc/driver/d3d12/d3d12_rendertexture.cpp`：inverse range、非有限逆范围处理、sRGB 输出与输入 gamma 解释。
- `renderdoc/driver/d3d12/d3d12_replay.cpp` 和 `renderdoc/driver/vulkan/vk_replay.cpp`：checkerboard、HighlightBox、MinMax/Histogram、GetPassEvents。
- `renderdoc/driver/d3d12/d3d12_overlay.cpp` 和 `renderdoc/driver/vulkan/vk_overlay.cpp`：重绘真实 draw、替换 fragment shader、恢复状态。Viewport/Scissor 也包含真实 draw 测试，不能只画一块矩形冒充。

## 定向证据

脚本：`bash util/buildscripts/scripts/test_metal_texture_view_macos.sh [capture.rdc EID TextureResourceId]`。脚本先检查 qrenderdoc/UE 未运行，不自动构建；日志保存在一个小目录，不复制 UE 截帧。

`metal-texture-view.dFjy83` 包括原生/capture/replay 结果与库前后 hash。两次 replay/reset 检查：

- RGBA32Float 的负值、NaN、Inf 和有限梯度；SRGB、R32UInt、R32SInt、Depth32Float_Stencil8。
- MinMax 的四通道值、整数 union、Histogram 端点与排除项、无通道及非法范围。
- NaN/Inf/负值颜色、禁用 NaN 通道、clipping、单通道、alpha 灰显、Range/gamma、RGBM/YUV、flipY。
- 反向和零宽 Range、平移背景、棋盘两种色块、线性 alpha 合成；半透明 `.5` 覆盖 `.25` 背景得到约 `.39941488`，避免 gamma 空间混合。
- Array 的 3 slices、Cube 的 6 faces、Cube Array 的 12 faces、3D 的 4 slices 和 mip1；每面带不同值和 x/y 梯度，防止中心常量/错误方向漏检。
- 原生独立回读检查有限/特殊值及不同 face/volume 内容；浮点原始字节在检查前后不变。

像素放大框通过源码接通，但实际像素上下文 UI 尚待解锁验证，不能把定向 checkerboard 结果代替 HighlightBox 人工检查。

后续当前 ac2a9748 实际助手 UI 已完成：GBufferA 的 Range/自动范围/Histogram/clipping/NaN 模式、RGB/红单通道及 Pick (194,182) 的 Pixel Context 黑白中心边框均已操作并观察；恢复默认显示后 SceneColor 正常。详细 UI 证据和限制见 [B450–451](BATCH450_451_NATIVE_DRAW_OVERLAYS.md)，上述“尚待”保留作 B449 当时状态。

## 真实 UE 证据

同一文件：`build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`，SHA256 `c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`。

显示候选 `f4396853`，以及绘制覆盖层候选 `5663c7e8`（`metal-texture-view.Vh0TeA`）：EID3300、GBufferA ResourceId12323 两次检查/reset 通过；Histogram 统计覆盖全部 76800 像素，Min R=0、Max R≈.987292；检查前后 Native 字节一致，SHA256 `33d636f6cc68eebcce5813669fe0a81f45d60d595c4770087b856706a459d55f` 与原验收基线相同。

中间候选 `9aa24317`：同一 EID3300 的 SceneDepthZ12397 两次检查/reset 通过，Histogram 76800，Min R=0、Max R≈.0209239，原始字节不变。最新候选的集中回归后仍需重查深度、GBuffers 和完整呈现图像。

B450–451 当前 ac2a9748 全量通过后，`metal-texture-view.xB5Xmn/` 的微型定向及同一 GBufferA 两周期检查通过，统计与 Native hash 保持上述值。`metal-test-overlay-current/post-full-presented/` 正常 OpenCapture、两次 EID0 重置完整回放通过，三份 900×640 BGRA Native 呈现图像均为 `fc5f3afe33b3ec7a523d447d59d0b517337d4c8f8867f6d863f99f0ab5eb4f61`，保持此前与原捕获缩略图精确核对的基线。

这证明资源检查没有改变当前 UE 的真实输入数据；不等于用户已接受最终画面，也不等于绘制类 Overlay 已能重绘 UE。

## 绘制类 Overlay 的后续（下列为 B449 开始时分析）

B450–451 已实现并验证五种真实绘制覆盖层，当前结果见 [B450–451](BATCH450_451_NATIVE_DRAW_OVERLAYS.md)。以下占位状态描述保留为本批起点，不代表当前实现。

已确认 Metal `RenderOverlay` 仍返回空 ResourceId，`GetPassEvents` 仅返回当前 EID，`IsRenderOutput` 仅识别最后呈现图像。

公共 ReplayOutput 调用顺序为 WithoutDraw → RenderOverlay → OnlyDraw。Metal WithoutDraw 保留未提交的 Native encoder，OnlyDraw 依赖这个 encoder。直接在 Overlay 中结束/提交它再继续 OnlyDraw 会破坏生命周期。真正补齐应保留真实 draw 的 vertex function、vertex/argument/texture/sampler bindings、Native indirect 参数和 pass 深度/模板资源，按其他 API 的固定 fragment shader 路径重绘到 owned overlay，再恢复原回放状态。

先用极小 Native 用例证明覆盖层、原资源无写入和重复 seek，再立即检查同一 UE。涉及提交/公共恢复的补丁单独做全量，不能复用本次显示候选的全量结果。不得用颜色近似、CPU 猜测 footprint 或静态矩形冒充真实 GPU 测试。
