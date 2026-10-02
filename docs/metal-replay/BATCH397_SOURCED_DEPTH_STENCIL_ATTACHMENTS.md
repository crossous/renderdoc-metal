# B397：sourced depth/stencil pass（定向验证中）

实际 UE12230757 CPU 清点：89 render passes，其中22有depth/stencil，89均有counter
sample attachments。当前切片仅增加带color的D16/D32/D32S8 depth/stencil闭包；
counter、depth-only与deferred unknown store仍须分别适配，不宣称完整实际UE通过。

复用RenderDoc已有Metal initial-content和pipeline/attachment模型（与D3D12 OM targets、
Vulkan framebuffer/pipeline attachment compatibility同一原则）。新增保存PSO的depth/stencil
format并核对pass，attachment要求2D/sample1/单mip、PrivateTracked、RT用途、无resolve、
level/slice/depthPlane0、Store/无store options。上限512×512。
clearDepth必须finite且0..1，clearStencil0..255，resolve filter保持sample0。

Load必须由完整initial或本submission/此前已提交submission的Clear证明。Clear证明分别
按depth和stencil方面记录，不能以depth clear替代stencil clear。D32S8同一Native texture
用于两方面，仍以原ResourceId/native placement/生命周期/queue ownership重放。
future heap depth descriptor复用已有logical size、Native footprint/range与Live资源检查。

极小用例扩展现有MRT/descriptor producer，D16/D32/D32S8分别有background initialized与
frame heap附件；另D32S8五MRT。first draw ClearDepth1/stencil7，LessEqual写depth0并
increment stencil到8；第二pass Load同一附件，Depth Equal+Stencil Equal8并禁止depth write。
helper逐draw验证pipeline state、全部depth/stencil原始像素、所有MRT像素、跨pass采样、
descriptor relocation、producer与EID0恢复。预期14captures/56seek cycles。

运行 `bash util/buildscripts/scripts/test_metal_descriptor_depth_macos.sh`。
新增depth gate覆盖aspect初始化、missing/format/subresource/resolve/load/store、非法clear
范围/NaN/Inf，并与现有MRT gate联合验证拒绝发生在frame GPU wait之前。

B396精确5473713a全量通过后才开始构建本候选；当前尚未确认候选GPU结果。
无commit/push，人工UI仍未验。持续目标active。

精确b38753a635b48fe3c35de5f343797e62be0915d74d27aeb80a70fffec8604608：metal-depth-mrt.1vxZ4g定向全部通过，14captures/56cycles/
236 API+CLI negative groups。Metal API Validation启用；所有depth/stencil raw pixels、
color MRT和cross-pass pixel结果正确。第一轮lSbkK0被helper错误的default stencil
断言阻止：Native default front/back stencil descriptor配置存在，后按真实capture状态修正
helper，只在D32S8要求stencilEnabled；未改变product状态或Native渲染行为。
当前开始间接与晚出生11copy partial prefix交叉验证；精确全量待后续。

交叉验证同精确b38753a6：metal-indirect-reuse.HNMp6j（4 captures/28reset seeks）
与metal-submission-index.QEDJSq（late staging/11copies/11births，4captures/16cycles
+16first-draw seeks/144 API+CLI negatives）均通过。前两项Native机制仍正确。
本精确候选全量回归正在进行（depth-pass-combined-regression.log），未替换库。

精确b38753a6全量通过：308captures/7786malformed/3080lifecycle，
resident growth11321344B；端点库哈希不变。日志depth-pass-combined-regression.log。
只在其结束后才开始B398链接/定向；完整实际UE与人工UI未验。
