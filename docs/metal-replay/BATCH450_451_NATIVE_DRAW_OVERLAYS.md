# B450–451 — Native draw overlays and immutable state lifetime

2026-10-03，继续同一 UE 验证；不提交或推送。用户确认 Overlay 指 Depth Test 等绘制测试，并要求优先参照 Vulkan。

## 实现及对照

主对照 `renderdoc/driver/vulkan/vk_overlay.cpp` 的 Drawcall/Wireframe/BackfaceCull/Depth/Stencil 分支，交叉核对 D3D12 同名分支。沿用真实 draw、固定 fragment 颜色、单独红/绿测试和公共 ReplayOutput 的 WithoutDraw → Overlay → OnlyDraw 顺序。

Metal 为每次测试准备所选 pass 的原始 prefix。深度/模板的初始 Native StoreAction 临时设为 Unknown，在结束 prefix 时设为 Store；捕获元数据和普通回放的设置保持原样。直接修改已知初始 StoreAction 会触发 Metal validation，这是本次极小用例发现并纠正的 API 差异。

保留原 vertex shader 和完整管线描述；复用已有序列化器恢复绑定、inline 指针注解、动态状态、residency，并执行所选真实直接/间接 draw。Owned RGBA16Float target 使用与 Vulkan 相同的覆盖层颜色。替换 Native PSO 时保留原 PSO 元数据用于已有绑定验证。每个红/绿测试重新恢复 prefix，避免 vertex shader 写资源反馈；随后公共 OnlyDraw 走正常 Full 恢复，不续用已结束的 encoder。

Scope 中的 inline 指针注解是一次性消耗项，不能只重放 setVertexBytes。按捕获流顺序恢复对应 layout/binding 注解和 setter，并还原 shadow 状态。已用 UE EID3300 验证该 bindless 路径。

IsRenderOutput 识别当前 color/depth targets，替代仅识别最终呈现图像的占位。UI 启用上述五种已验证绘制覆盖层，以及 B449 的数值覆盖层；其他覆盖层保留公共枚举并暂标 N/A。

目前绘制覆盖层限定普通单采样、非分层 2D、mip0 的直接/间接 draw。不支持 mesh/tessellation/ICB、管线拥有的 function tables、MSAA/layered targets。导出 shader depth 的 Depth 分支需要 Vulkan/D3D12 的 original-fragment + stencil-mask 流程，尚未补齐，不使用 VS 深度冒充。GetPassEvents、Viewport/Scissor、Clear Before、Quad Overdraw 和 Triangle Size 尚待推进。

## 定向

可运行 `bash util/buildscripts/scripts/test_metal_test_overlay_macos.sh [current-ue-capture.rdc EID]`，不自动构建，GPU 串行。可选 UE 参数是当前验收文件，输入检查固定 ResourceId12323/12331/12397。

`5663c7e87a60c97999ff240fe8e1bd7c4a0ca8ab75485db3320324f43c43b0fe`：`build-macos-debug/metal-test-overlay.pC2fAl/`。Native 独立参考、捕获、两个 reset 周期：串行/并行子 encoder 的 Depth/Stencil 各 256 红 + 256 绿；Cull 失败分支 256 红 + 256 绿，与原生 front/back 颜色逐像素一致；Wireframe 有 77 个线像素，435 个透明像素，颜色为 200/255、1、0；原图不变。当前 UE 的五种覆盖层两轮通过。

另发现有效 Native 临时 DepthStencilState 捕获后丢失，旧 replay 对 nil state 触发 validation：`metal-test-overlay-current/cull_capture.rdc` / `replay-cull.log`。B451 补上 DepthStencilState/SamplerState 的 new* 代理 ownership，并用公共 ResourceRecord parent 引用保留 immutable state 的创建记录。Writable GPU 资源不被此路径额外持有。修复库 `7a119ff32c105453da5b79319dc25d1279d025f7b9af3353f5f0500ec76fc4ed` 的重新捕获 `ephemeral_capture.rdc` 两周期/五覆盖层通过：`metal-test-overlay-current/replay-ephemeral.log`。旧失败截帧已缺创建数据，不能靠 replay 补回；未修改用户 UE 原件。

当前候选 `ac2a97488ba8b396bc2e1a85f9c0af59e274ee22baa69c1f052a49ee8593ad88` 的 ephemeral 用例同时实际采样临时 SamplerState，避免以未使用的 sampler 证明 lifetime。Depth-export 用例分别用含空白的 MSL 和真实预编译 metallib 验证：Depth 返回不支持，其他四覆盖层正常，原图不变，均两周期通过。证据 `metal-test-overlay-current/replay-depth-export-source.log`、`replay-depth-export-binary.log`。MSL/AIR 检测目前保守检查整份 library；未实现 original-fragment 深度 mask，不把拒绝不支持记作功能支持。

B449 Histogram 旧生命周期断言已改为真实 256 桶、全像素计数及 Native 原图不变。5663 库的 2 captures × 10 iterations 定向通过，resident growth 311296 bytes；不当作全量结果。

## 真实 UE

同一文件 `build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`，SHA256 `c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`。EID3300，输出 SceneColor ResourceId12320。

5663 库五覆盖层两周期通过：Depth/Stencil/BackfaceCull 各 35840 绿、40960 透明；Wireframe 12640 个线像素、64160 透明；Drawcall 与原生覆盖一致，背景 alpha .5。固定 fragment 覆盖层不保留原 FS 颜色/discard，因此几何覆盖数不要求等于最终颜色非零数。

每次覆盖层后直接核对公共 OnlyDraw 的恢复结果，未另行强制 seek 掩盖损坏。SceneColor、GBufferA12323、GBufferC12331、SceneDepthZ12397 均逐字节不变。GBuffers hash 保持 `33d636f6cc68eebcce5813669fe0a81f45d60d595c4770087b856706a459d55f`、`bb4bd6b0785f8b5282bdf02d7e13f7f295d2efa46ce192eb36bb0aef4e0ca1b8`；Depth hash `2a174a5c2734422b71c9dbbd06b9d7ccff8fef55cf917be8bb42863373643829`。当前 ac2a9748 候选五覆盖层两周期和上述原资源不变检查已通过：`metal-test-overlay-current/ue-depth-export-guard.log`。

## 全量及助手 UI

当前 ac2a9748 候选使用独立冻结库全量通过：308 captures、7786 malformed cases、3080 lifecycle opens，resident growth 8716288 bytes；start/end hash 一致，退出码 0。证据 `build-macos-debug/frozen-validation-ac2a9748/full-regression.log`。B449 的 74ed 全量因旧 Histogram 占位断言失败，不记为通过；ff444e68 仅是历史全量证据。

全量后重新执行当前源码脚本：`metal-test-overlay.okKgzy/` 的 serial/parallel/cull/ephemeral 微型捕获及同一 UE EID3300 五覆盖层两周期通过，库前后 hash 一致，原 SceneColor/GBuffer/Depth 未变。全量不代替真实 UE 进展。

全量后 B449 定向与同一 UE GBufferA 统计两周期通过：`metal-texture-view.xB5Xmn/`。完整呈现图像正常打开和两次 EID0/reset 通过：`metal-test-overlay-current/post-full-presented/`，三份 Native BGRA900×640 hash 均为 `fc5f3afe33b3ec7a523d447d59d0b517337d4c8f8867f6d863f99f0ab5eb4f61`，保持此前已精确匹配捕获缩略图的基线。

助手实际 UI 已通过（2026-10-03，当前 qrenderdoc executable SHA256 `a88e01654cae2a0808cfaaae42041e2ed4e8b1b2c7f3833a329137a0195b9b3b`，app 内嵌库与 ac2a9748 一致）。同一 UE EID3300 的 Depth、Stencil、BackfaceCull 显示真实绿色几何覆盖，Wireframe 显示实际间接 draw 的黄色分块线框，Drawcall 显示紫色覆盖；逐项切换没有 fatal dialog，None 恢复 SceneColor。

GBufferA 独立 tab、RGB/红单通道、Range 输入、实际自动范围（R max .98729）、Histogram 曲线、范围 .2–.4 的红/绿 clipping、NaN 模式正常值灰显均已操作并观察。右键 Pick (194,182) 读值 (.50244,.50244,1,.33333)，Pixel Context 放大图和中心黑白边框可见；返回 SceneColor 后对应 HDR Pick (2.64258,.74121,.00084,0)，原图正常。菜单中未实现项保留 N/A。最后恢复 RGB、Range[0,1]、Overlay None、Histogram 关闭，界面留在 EID3300 SceneColor，GBufferA tab 保留供用户查看。这是助手 UI 验证，不代替用户画面验收。

本机首次使用系统 Open Capture 对话框时，AX 卡在 macOS ViewBridge，采样见 `/tmp/renderdoc-overlay-ui-sample.txt`。重启本次空测试窗口，通过 Recent Captures 成功加载同一文件；未把这个系统文件对话框故障当作 replay failure。Qt 可访问性切换 Histogram 的 checked 状态不代表执行 clicked 槽，改用屏幕实际按钮点击，确认布局扩展和曲线绘出后才记通过。
