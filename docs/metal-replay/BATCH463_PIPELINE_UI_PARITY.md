# B463：Metal Pipeline State 布局与公共控件对齐

2026-10-04。保留工作树已有 B440–B462 改动，不提交或推送。同一 UE 捕获：`build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`，SHA-256 `c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`。证据目录 `build-macos-debug/metal-ui-alignment/`。

## 对照与实现

对照仓库 D3D12/Vulkan Pipeline State 的 `.ui`、viewer 及公共 `PipelineFlowChart`、资源树、shader/constant-buffer viewer；不是使用独立的 Metal 样式近似。

- 传统图形路径保留 IA、VS、Tessellator、Rasterizer、FS、OM；未使用的 Tessellator 灰显。细分 draw 改为 IA → Tessellator → TES → Rasterizer → FS → OM。Mesh 路径显示 TS → MS → Rasterizer → FS → OM。CS 在三种路径都为独立框，路径更新后重新设置 isolated stage，解决 `setStages` 清掉隔离边的问题。颜色、边框、选中和 hover 使用公共 flow chart。
- Metal 不伪装成 DX12 的 HS/DS，也不添加不存在的 GS。[Apple Metal Tessellation](https://developer.apple.com/library/archive/documentation/Miscellaneous/Conceptual/MetalProgrammingGuide/Tessellation/Tessellation.html) 的模型是独立 compute 生成因素、固定 tessellator、post-tessellation vertex function。TES 显示这段真实 shader；factor 生成的 CS 在对应 dispatch 查看。
- Shader 页面统一 `Pipeline & Shader` 标题、资源链接、View/Edit/Save 按钮。Metal 没有 DX12 Root Signature，显示真实 pipeline resource。Shader View 进入公共 Shader Viewer，Save 保存捕获 shader 字节。**Edit 仍不可用并明确禁用**：当前 Metal backend 未提供 target encoding 或 shader replacement，不能只做一个看起来能编辑的按钮。
- VS/FS/CS/TS/MS 统一 Resources → UAVs → Samplers → Constant Buffers。使用公共 PipeState RO/RW/sampler/constant getter，包括 bindless 的已解析资源和 AS。保留原绑定名、真实资源名、mip/slice/range 与 view-format 差异颜色，Go 使用公共箭头。反射常量缓冲区进入公共变量查看器，IA buffer 使用公共顶点格式。资源树保留 Show Unused/Show Empty 与公共 Extensions。
- Shader 标题在资源滚动区之外，与 D3D12/Vulkan 的结构一致；消除全页 QScrollArea 的 height-for-width 使四个资源分区撑出视口的问题。
- IA：按 vertex descriptor 筛选固定功能顶点流，不把 Metal 的 bindless argument heap 槽位当作 IA buffer；上方 Input Layouts；下方 Buffers（合并 index/vertex）、可点击 Mesh View、公用 Primitive Topology 图，独立 indirect 参数按绑定显示。
- Rasterizer：状态格线和 tick/cross；下方并列 Viewports / Scissor Regions，保留所有动态数组元素。
- OM：Render Targets（合并 depth）、按需 Resolve Targets、Target Blends；下方 Blend State / Depth State / Stencil State 矩阵；各资源维度和格式列与公共布局一致；stencil mask/ref 复用 D3D12 的 SetStencilTreeItemValue 两位十六进制及十进制/二进制 tooltip。

## 状态数据补齐

原 backend 已调用 native Metal 动态 API，但 inspection state 缺少 fill/clip/bias/blend factor、多 viewport/scissor 与 tessellation 数据。新增公开 Pipeline State 字段及序列化；从 captured API 和 RDMTL pipeline descriptor 更新 inspection 数据，不增加 native setter、提交、等待或读回，不改 capture chunk 格式。

四种 patch draw 记录实际 control point 数量及 PatchList 拓扑；公开 factors buffer/range/stride、partition/step/winding、max factor、factor scale。顶点 layout 公开真实 step function。

没有显式 setViewport/setScissorRect 时，按 encoder render area 展示 Metal 默认值（显式 renderTargetWidth/Height 与各 attachment mip 尺寸约束）。[Apple setViewport](https://developer.apple.com/documentation/metal/mtlrendercommandencoder/setviewport(_:))、[setScissorRect](https://developer.apple.com/documentation/metal/mtlrendercommandencoder/setscissorrect(_:)) 描述该默认行为。公共 GetViewport/GetScissor 支持全部 Metal 数组索引。

## 分项验证

当前 backend/build/bundle/冻结库 SHA-256 一致：`885fa783a811840ede81e778de9e495ebd834f0297be4384791ff7d9814db414`。最后三个改动仅 Qt UI（sampler Comparison 判断、公共 Metal shader 阶段名/CB 标题、IA 分类/stencil tooltip），不改变此 replay 库。

- **定向终端 PASS**：T36/T66/T67 状态、GPU 图像和重复 reset；T79/T80/T83/T85 公共 Task/Mesh 检查。额外既有 viewport-array/reset 的两个状态数组、公共 GetViewport/GetScissor 索引1及单值重置 PASS。日志 `directed.log`、`inspection-t*.log`、`viewport-array.log`、`viewport-reset.log`。
- **真实 UE PASS（全量前）**：3612/3928/4036 各两次 EID0 reset、GPU readback 相同；GBufferA九Usage稳定，4036不含GBufferA shader访问。GBufferA对应六份数据 SHA-256 保持 `33d636f6cc68eebcce5813669fe0a81f45d60d595c4770087b856706a459d55f`。正常打开及两次整帧 reset 的三份900×640呈现图均保持 `fc5f3afe33b3ec7a523d447d59d0b517337d4c8f8867f6d863f99f0ab5eb4f61`。日志 `ue-events/events.log`、`ue-images/events.log`。
- **全量 PASS**：本候选一次固定库串行验收 308 captures / 7786 malformed / 3080 lifecycle opens，resident growth 5996544B，exit0；起止库哈希一致。日志 `full-regression.log`、`full-exit.log`、`full-build/start-hash.log` / `end-hash.log`。此结果属于885fa783，不使用B462全量代替。全量后同一UE复核通过，见下一项。
- **真实 UE PASS（全量后）**：再次正常打开、3612/3928/4036 各两轮 EID0 reset 与读回通过，GBufferA 九Usage稳定；同一 GBufferA六份数据和三份900×640整帧呈现图 SHA-256 均保持上述基线。日志 `post-full-events/events.log`、`post-full-images/events.log`，capture hash不变。
- **实际 UI（助手操作，分项记录）**：T66 实际点击确认 Tessellator→TES、独立CS、默认400×300 viewport/scissor、OM比例及 shader View；T80 Mesh页、T85 Task页四资源分区、sampler及View确认。最终binary重新打开同一UE：3509 IA仅真实576B index buffer、没有把bindless参数heap列入IA；3509 OM六附件/五blend/front-back stencil矩阵，两位十六进制F6/FF/86与公共tooltip；3612 CS四个写资源含GBufferA/B、4036三输入/三UAV体积且无GBufferA，四分区比例确认；3928 FS的五个命名RO资源与独立CS、header/View/Edit禁用、资源GBufferA双击进入公共TextureViewer、常量buffer双击进入“Fragment CB 1”公共viewer、View进入AIR/CapturedMSL公共ShaderViewer均确认。保存面板取消失败并重启后，同一UE正常打开；3928 Rasterizer的状态格线、tick/cross、并列viewport/scissor及320×240实际值再次确认。最终停在3928 Fragment Shader Pipeline State。
- **Shader Save UI未验收**：AX点击真实Save后，CUA的AX/截图/Escape连续timeout。进程采样显示公共`SaveShaderFile→RDDialog→QFileDialog→NSSavePanel`，主线程停在AppKit文件属性`getxattr`查询；未出现新的Metal completion错误，不能据此判定Shader字节保存正确或错误。采样 `save-dialog-sample.txt`。取消失败后终止本次测试实例并重新打开同一UE，未改捕获、未写shader文件；公共Save路径代码仍保留，原生保存面板实际完成未计PASS。

两项新断言的首次尝试失败已修正：T67会显式设置三个不同 scissor，不能沿用T66全宽预期；公共越界 Viewport/Scissor默认对象的 enabled 为true、width为0，reset后应验证数组长度和width而非enabled。测试编译时还补上现有standalone probe使用的 pipestate.inl 及 uint32 DoStringise。最终各probe重新编译通过，失败尝试不计为PASS。

UE大窗口的CUA坐标操作间歇返回 noWindowsAvailable；AX与截图仍正常。3509 OM通过RenderDoc内置Python REPL调用公开SelectPipelineStage导航后读取实际界面，没有改UI数据；3509 IA在初始IA页通过Find EID观察。T66/T80/T85鼠标阶段切换实际完成。UE阶段鼠标切换不能据此单独记为全面验收，最终重开再次点击OM仍返回同一工具错误；不把该坐标操作记PASS。

定向入口（先退出 qrenderdoc/UE，GPU 串行；构建 -j2）：

```sh
bash util/buildscripts/scripts/test_metal_pipeline_inspection_macos.sh
```

T36 检查 captured bias/clip/blend/scissor；T66/T67 检查四种 patch draw、默认 viewport 与每次实际 scissor；同时检查真实 GPU 图像及 reset。T79/T80/T83/T85 检查公共 Task/Mesh stages。额外 viewport-array/reset 用已有小捕获验证索引 1 和单值 setter 清除旧数组。

仍保留 B462 的 uniform bindless 边界，不宣称任意 per-invocation 动态索引 feedback 或完整光追 helper 访问已全面平齐。独立 stencil attachment、所有 Metal 可选能力和 shader replacement 也未在本批宣称完成。
