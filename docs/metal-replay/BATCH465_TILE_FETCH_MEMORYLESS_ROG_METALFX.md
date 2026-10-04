# B465：Tile、Framebuffer Fetch、Memoryless、ROG、MetalFX Spatial

2026-10-05。保留既有未提交修改；没有提交或推送。

## 2026-10-05 UI 整改

- MetalFX 的公开输入/输出现在同时进入 common descriptor inspection API 和 Texture Viewer 的 Inputs/Outputs 缩略图，名称分别为 MetalFX Input / MetalFX Output。资源归属于 opaque operation，未伪造 shader/PSO。Usage 使用新增且追加到枚举末尾的 MetalFXInput / MetalFXOutput，右键/Resource Inspector 显示 MetalFX - Input / Output，timeline 分别按读/写分类。现有 rdc 无需重截，Usage 在打开回放时重建。
- Fetch 的 Usage 从初版 PS_Resource 更正为 InputTarget（UI 为 FB Input），与 Vulkan 的实际实现一致；同一资源的 ColorTarget（FB Color）写入仍保留。Memoryless 是纹理的 storage/lifetime 属性，单独使用不意味着 shader 输入。仅在被 fetch 时报告 FB Input；Clear / FB Color / Discard 按实际事件记录。不存在伪造的 Memoryless usage 事件。
- Metal Shader Features 移至 FS / Tile 页面末尾。默认只在有 fetch、ROG、imageblock 声明时显示；plain shader 隐藏。Show Unused Items 可显示 None / Unavailable 元数据，且在没有普通 resource bindings 的 FS 也可操作。公开 MiniQtHelper 的 checkable widget 操作补齐 QToolButton 支持，以实际 UI 验证该控制。
- Programmable blending 是 FS 中读取已有附件并自行计算的 shader 行为，没有可替代固定功能 BS 的独立 blend-state descriptor。它也可用于 deferred lighting，并不必然是混合计算。OM 保留实际固定功能状态，增加解释 tooltip。新增 combined_capture.rdc 同时启用 fetch / ROG 与固定加法混合：原生验证、捕获、回放均通过；color0 两次 draw RGB=160/255，color1 shader 输出 RGB=96/192，证明两者不互斥。

当前构建 backend / app 内嵌库均为 SHA256 `f0280e362faecd9832e303905abbf74fd1dad2facb4852122ab68bad0a8eb209`。七份定向捕获及像素/Usage/descriptor 名称验证通过；20 份旧帧定向回归通过。真实 GUI 使用公开 UI API 检查了 Inputs/Outputs 的可见缩略图、资源跟随、FS/Tile 的末尾布局、plain 条件隐藏及 Show Unused 切换，均通过；右键菜单鼠标操作及最终外观仍由用户验收。日志：review-validation.log、review-regression.log、ui-review.log；不是全量回归。

整改 UI 重现：

```bash
build-macos-debug/bin/qrenderdoc.app/Contents/MacOS/qrenderdoc \
  captures/metal-features-b465/metalfx-spatial_capture.rdc \
  --ui-script util/test/metal/metal_feature_review_ui.py
```

Vulkan 对比（以下为本 checkout 的实现，并非仅由扩展列表推断）：

| 概念 | Vulkan 相近能力 | RenderDoc 本 checkout 的处理 |
|---|---|---|
| Framebuffer Fetch | subpass input attachment / dynamic rendering local read | FS resources 显示 Input Attachment；OM 显示颜色/深度附件；Usage 为 InputTarget / ColorTarget，非普通 FS texture |
| ROG | fragment shader interlock / rasterization order attachment access，语义非逐项等同 | 两项 EXT 在支持列表和 wrapper 支持路径；没有独立的 shader-features 汇总面板，也没有用它们替换 Blend State；原始声明/flags 保留在 shader 和 API / resource creation 数据 |
| Memoryless | TRANSIENT_ATTACHMENT + LAZILY_ALLOCATED（相近存储策略） | 保留图像资源身份；Usage 按输入、输出、清除、丢弃分类，storage 属性不单独成为 usage。不是 subpass input 的同义词 |

源码依据：VulkanPipelineStateViewer.cpp 的 FS Input Attachment 分类；vk_core.cpp 的 AddUsageForDescriptor 排除 input attachment，并在 AddFramebufferUsage 将它记为 InputTarget；vk_core.cpp 支持列表与 vk_serialise.cpp 的 interlock / rasterization-order feature 序列化；vk_pipestate.h 的 ColorBlendState 保留固定混合字段，没有上述扩展的独立 UI 开关。

语义参考：[Apple deferred lighting](https://developer.apple.com/documentation/metal/rendering-a-scene-with-deferred-lighting-in-objective-c)、[Apple ROG](https://developer.apple.com/videos/play/tech-talks/605/)、[Khronos ordered framebuffer fetch](https://docs.vulkan.org/samples/latest/samples/extensions/rasterization_order_attachment_access/README.html)、[Khronos transient attachments](https://docs.vulkan.org/samples/latest/samples/performance/subpasses/README.html)。

## 本轮实现

- Tile 调度在 Pipeline State 中显示独立的 `Tile` 流程和页面。显示 native Tile PSO、kernel、直接 buffer/texture/sampler/inline bytes、实际 tile 大小、threads per tile、descriptor 最大线程数和 matchesTileSize、imageblockSampleLength、render-pass threadgroupMemoryLength、各 tile threadgroup memory binding 的 offset/length、所属 render-pass 附件。
- Tile 的原生执行仍使用 render command encoder；复用公共 Compute kernel/descriptor inspection API 来打开 shader 和绑定，并明确用 `tileDispatch` 区分。没有增加 graphics/compute queue 的虚构状态，也没有加入全局 ShaderStage::Tile。Tile dispatch 归属实际 render pass，并报告附件输出及 Usage。
- Framebuffer Fetch 在 FS Resources 中显示 `Input Attachment: color[n]`，同一个 ResourceId 仍在 OM Render Targets 中显示输出。共享 GetReadOnlyResources API 也报告此输入，Usage 同时报告 InputTarget 和 ColorTarget，不伪造成 texture(n) 绑定。Programmable blending 是 fragment shader 行为；原有固定功能 blend state 保留在 OM。
- FS 的 Metal Shader Features 显示 framebuffer fetch 附件、Raster Order Group 编号、imageblock 和元数据来源。AIR 按当前入口及其 metadata graph 提取，避免输出 color 被算作 fetch、避免相同 library/module 的其他 shader 污染。源码-only libraries 使用当前声明及所引用的 struct，标明 `Captured MSL declaration`；宏、模板或没有可确认信息时显示 Unavailable，不声称已验证关闭。
- OM Attachment Operations 显示捕获的 storage/load/store/storeOptions，动态 storeAction setter 同步更新。Memoryless 仍采用此前 Private replay backing 以支持事件内 inspection；UI 显示原始 Memoryless，DONT_CARE 后的数据保持 undefined/discard 语义。Tile 页显示同样的 pass 附件操作。
- MetalFX Spatial 从公开 scaler API 捕获为 opaque operation，避免依赖系统内置 metallib/私有实现。回放在 native command buffer 调用 MetalFX，保留输入/输出/模式/content size/fence；UI 显示独立 MetalFX 页面、输入/输出尺寸与格式、content size、Perceptual/Linear/HDR 和纹理跳转。内部 shader 不提供编辑或单步调试。原生 command buffer 持有 replay scaler，覆盖未提交 partial replay 与 unretained-reference 生命周期。
- 修复 writable struct/atomic buffer 被误归为 Constant Block 的 inspection 分类。
- 修复切换捕获时 Texture Viewer 的异步缩略图回调访问已销毁 ResourcePreview：在 UI 线程建立 QPointer，使用稳定的应用回调上下文，并在 UI 更新前检查生命周期。

Vulkan 对照位置：`VulkanPipelineStateViewer.cpp:1353` 将 input attachment 作为 shader resource 显示；`vk_replay.cpp:2486` 将 InputAttachment descriptor 转成公共 Image descriptor。本轮使用相同的 FS 输入 / OM 附件输出关系；Metal 的 fetch 没有 descriptor set/binding，因此显示 `color[n]`。ROG 是 shader 声明信息，放在 FS；Memoryless 和 load/store 是 pass 附件信息，放在 OM。没有把 ROG 编号伪造成 Vulkan 的全局 interlock 开关。

## 范围

没有实现 Metal shader 单步调试、逐像素 imageblock 内存调试，也没有宣称支持全部 MetalFX。此次 MetalFX 是 Metal 3 `MTLFXSpatialScaler`；Temporal、Temporal Denoised、Frame Interpolation、Metal 4 scaler 仍未接入。本轮未扩张既有 Tile pipeline 的限制：color0 仅 RGBA8Unorm/BGRA8Unorm，额外颜色附件不支持，sampleCount=1；preloaded library、mutability、call-stack 的既有边界也保留。

源码声明不能替代编译后的 specialization/宏展开结果；缺少 Xcode metal-objdump 的 binary shader 保留回放，但 feature 元数据可能 Unavailable。

## 重现

```bash
cd /Users/crossous/Developer/renderdoc-metal
bash util/buildscripts/scripts/test_metal_tile_features_macos.sh
```

捕获：`captures/metal-features-b465/`。
日志、native MetalFX oracle：`build-macos-debug/metal-tile-features/`。

七份：`tile_capture.rdc`、`tile-source_capture.rdc`、`private_capture.rdc`、`private-source_capture.rdc`、`memoryless_capture.rdc`、`metalfx-spatial_capture.rdc`、`combined_capture.rdc`。

每份定向 probe 做 event 0 reset、正向及反向遍历。Tile counter=4，imageblock event 像素 RGBA=(96,64,64,255)；fetch draw 1 / 2 的 stored MRT 像素 RGBA=(96,96,96,255)/(128,128,128,255)。MetalFX 的 32×32 RGBA16F 输出与原生运行逐字节相同。源码/AIR 同 library 的 plain entry 不继承 fetch/ROG。CLI 每份主 AIR/MetalFX 帧循环回放三次。

16 个 MetalFX malformed capture 干净拒绝；原 T74 28 个 malformed capture 仍干净拒绝。20 份旧帧的 API output probe 和 CLI 回放通过：9 sentinel，加 T34/43/74/75/76/79/85/170/200/244/308。本轮没有重跑全量 308/7786/3080，不把 B464 全量结果当作此轮结果。

初版 GUI 构建完成后，三轮各 9 次真实 GUI 捕获切换通过（27 次）。实际页面检查确认 Tile 独立流程、shader/dispatch/imageblock 参数，Fetch/ROG 的 FS 输入与 OM 输出、Memoryless/DontCare 附件表，以及 MetalFX 参数和输出纹理双击跳转。Tile dispatch summary 已移至绑定表之前，当前窗口停在 Tile EID13。用户对布局的满意度仍待验收。

初版后端与 app 内嵌库 SHA256 均为 `48afd5d3fdfba858f0dc4a145b2746eae8b649774f691ccb8ccd16744806328b`。主要日志：`validation.log`、`regression.log`、`tile-invalid.log`、`metalfx-invalid.log`、`ui-tile.log`、`ui-memoryless.log`、`ui-metalfx.log`。

可选 GUI 切换重现（会关闭并打开当前测试窗口的捕获，请用新窗口）：

```bash
build-macos-debug/bin/qrenderdoc.app/Contents/MacOS/qrenderdoc \
  captures/metal-features-b465/tile_capture.rdc \
  --ui-script util/test/metal/metal_tile_features_ui_smoke.py
```

## 人工 UI 验收

```bash
APP=/Users/crossous/Developer/renderdoc-metal/build-macos-debug/bin/qrenderdoc.app
CAP=/Users/crossous/Developer/renderdoc-metal/captures/metal-features-b465
open -n "$APP" --args "$CAP/tile_capture.rdc"
# 查看完一份后，可替换文件名打开：
open -n "$APP" --args "$CAP/memoryless_capture.rdc"
open -n "$APP" --args "$CAP/metalfx-spatial_capture.rdc"
```

- Tile：EID 13 dispatchThreadsPerTile，流程应只显示 Tile。kernel=tile_image；tile/threads=16×16/16×16×1，max=256，matches=Yes，imageblock=4 bytes/sample，pass memory=32，slot0 memory offset=16/length=16；UAV counter buffer offset=4，inline buffer1=2 bytes。双击附件打开 texture，在(8,8)取样应为(96,64,64,255)。切 EID 6/18 应恢复传统 graphics 流程。
- Fetch/Memoryless：EID 7/10，FS feature fetch=color[0]、ROG=2。FS Resources 的 input attachment 与 OM color0 必须是同一资源。OM color0 原始 storage=Memoryless、store=DontCare；color1=Private、store=Store。color1 (8,8) 两次 draw 分别为96/128，alpha255。EID16 plain shader 的 fetch/ROG应清空。
- Private 帧：与 Memoryless 帧对比，同样的 FS 输入/OM 输出结构，color0 storage=Private/store=Store。
- Source 帧：结果和资源关系一致；metadata source=Captured MSL declaration，View 可以看 MSL。
- MetalFX：EID6，独立 MetalFX 页面，input16×16/output32×32/content16×16、Linear、RGBA16F。Input/Output 可跳转纹理；此操作没有内部 Shader Edit/Debug。

UI 外观和布局是否满意仍由用户验收；自动 inspection/pixel PASS 不等于人工 UI PASS。
