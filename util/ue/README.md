2026-10-05 B466：本 fork 的 shader tools 已内置 app 并自动注册，View 提供 AIR/MSL/HLSL/GLSL，Edit/Compiler 接入 Apple AIR/MSL 编译；release 打包检查固定源码、hash、架构与许可证，无用户路径配置/运行时 Homebrew 或 Rust 依赖，仅 Apple 工具依赖 Xcode。高层重建仅预览，Edit 严格限无资源/无特化的简单 FS；当前 UE3928 fptoui、3612 fptosi 尚不能高层反编译，AIR 可用。定向工具/搬迁/超时/打包 PASS；实际 UI AIR Apply/Remove 与三种源码 View PASS（公开 Qt 选择，非鼠标下拉验收）；同一 UE 两 shader 在06ae及随后匹配当前 API 的固定48afd5d3上均两轮写资源字节一致/恢复/fatal0。新头旧库混用追加 probe 失败不计 PASS；另一任务已更新 backend，本轮未重跑全量，06ae全量仅属于B464。详情见 [B466](../../docs/metal-replay/BATCH466_BUNDLED_SHADER_TOOLS.md)，未提交/推送。

2026-10-04 最新 B464：接入公共 Shader Edit/Apply/Remove 与 Shader Processors，补齐 MSL、嵌入 debug MSL、可重编译 AIR/MetalLib，保留 function constants、specializedName/原入口及所有依赖 native PSO 的原 descriptor。AIR 编译保持捕获 Metal/AIR 版本和部署目标，映射发布前等待 GPU，typed release 释放临时 function/PSO。View 可选择内置 AIR、Captured MSL 和匹配的外部 processors。最终06ae2318库与bundle一致：定向 source/AIR/debug/alias/frame-born PASS；首次候选 Tess/Task/Mesh 替换通过；最终固定全量308/7786/3080 PASS、growth1343488B/hash一致/exit0；全量后同一UE3928 FS/3612 CS两轮编辑/恢复、3612/3928/4036 Usage及整帧两轮reset通过，写目标/GBufferA/呈现图保持基线。实际UI通过微型源码Edit/红色Apply/错误展示/Remove恢复、外部View目标切换，同一UE3928 external AIR与3612内置AIR的Edit/Apply/Remove；停在3928 FS原shader。直接键盘源码输入自动化未验收，Save面板未重试；readonly VisBuffer64在无编辑control reset也变化，原因未定位。AIR→MSL候选metal2vulkan→SPIRV-Cross尚未构建或验证；linked function tables编辑明确拒绝。详见 [B464](../../docs/metal-replay/BATCH464_SHADER_EDIT_AND_PROCESSORS.md)，操作见 [Shader Tools](../shader_tools/README.md)。保留旧改动，未提交/推送。

2026-10-04 最新 B463：Metal Pipeline State 使用公共 flow chart/资源树/shader与CB viewer，对齐传统/Tessellator→TES/TS→MS路径、独立CS、灰/黑/红边框；shader头部与Resources→UAVs→Samplers→Constant Buffers顺序，IA布局/真实vertex流分类、Rasterizer矩阵及并列viewport/scissor、OM附件/混合/深度/模板布局补齐。新增inspection动态状态与细分数据，不改GPU setter/提交/等待或capture chunk。当前885fa783库与bundle一致：定向PASS；一次固定全量308/7786/3080 PASS、growth5996544B/hash一致/exit0；全量后同一UE3612/3928/4036及整帧两轮reset通过，GBufferA/呈现图保持基线。助手实际UI确认IA/RS/OM/FS/CS及native Tess/Task/Mesh页面、公共View/CB/资源跳转；Edit因backend尚无replacement明确禁用。Save原生面板自动化timeout（采样AppKit getxattr），未计UI PASS；UE flow坐标点击工具noWindowsAvailable也未计PASS，公开stage导航确认实际页面。重启后同一UE正常打开，留在3928 FS。详见 [B463](../../docs/metal-replay/BATCH463_PIPELINE_UI_PARITY.md)。未提交/推送。

2026-10-04 最新 B462：修复 GBufferA 4036 假 CS_RW 和遗漏读取。移除 residency/lifetime 资源闭包的 Usage 误报；uniform bindless 按各 command buffer 的 submission CPU snapshot 解析，再统一 bake EID，GPU 提交顺序不变。原 UE 打开即有九个 Usage 事件：3415 Clear、3509 FB Color、3596/3612 CS_RW、3739/3824/3928/4096/4482 FS Resource；七个 shader 事件逐项跳转资源一致、4036 排除且访问前后列表稳定。补齐 compute/render 显式 resource Barrier Usage；不为 scope barrier 虚构资源。4036 的64³光照体积全体为常量SH系数，filter输入/输出与两轮reset一致，UI dispatch深度名称已修正。Windows差异有实际session日志证明：本次低负载capture关闭Lumen GI/Reflections、VSM、ShadowQuality等，不能作为全功能帧平齐验收。候选库与bundle a881aab3：late-uniform/CS/Task/Mesh/Barrier定向通过；固定全量308/7786/3080通过、growth0B/hash一致/exit0；全量后同一UE九Usage、整帧三份呈现原基线均通过。实际UI通过4036体积/3928法线、ResourceInspector八分组九事件与3739跳转；缩略图右键及外点击关闭因CUA窗口定位错误未验收。详见 [B462](../../docs/metal-replay/BATCH462_SUBMISSION_BINDLESS_USAGE.md)。未提交/推送。

以下为历史记录，B460–B461 的4036 Usage及3928需选中才补入的解释已由B462纠正。

2026-10-04 最新 [B460–B461](../../docs/metal-replay/BATCH460_461_BINDLESS_SHADER_STAGES.md)：同一 UE 的3596/3612 Nanite compute 写入已出现在 GBufferA Usage 与3612 Outputs 中；扩展 CS/VS/FS/Object/Task/Mesh 的 uniform bindless、Task/Mesh Pipeline State 与 compute ray AS 输入。当前6362ccc0定向、固定库全量和全量后同一 UE通过；人工UI分项与未完成项见报告。阶段定向入口（关闭 UE/qrenderdoc，使用已构建库，GPU 串行）：

```sh
bash util/buildscripts/scripts/test_metal_bindless_stages_macos.sh
# 同时检查当前 UE 捕获的固定 EID/GBufferA
bash util/buildscripts/scripts/test_metal_bindless_stages_macos.sh \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc
```

下面为此前验收记录；当前仍不代表完整动态 bindless feedback 已与 DX12/Vulkan 平齐。未提交/推送。

2026-10-04 最新 B458–B459：参照 DX12/Vulkan 将 Metal 公共 EID 按提交映射，同一 UE GBufferA 已为3415 Clear→3509 FB Color→3928 FS Resource；修正 ICB API event去重、submit扁平scope、encoder结束后的signal/wait归属和AS Encoder归属。DontCare新增 MSAA逐样本、untracked standalone/heap、memoryless Private backing、3D/cube/rate map；修正部分回放 StoreDontCare丢失所选draw，以及resolve-only源Discard usage。当前库671c90b4/GUIc3a0e3b6（内嵌库同步）：18单样本+15MSAA定向、额外深度/模板独立Store和204周期通过（Store stress growth3325952B）；固定全量308/7786/3080通过（growth0B/hash一致/exit0）。全量后同一UE3928/3509、六light overlays/None恢复、Clear Before、正常打开和两次整帧重置均通过；SceneColor/GBuffer/深度与三份呈现图保持基线，原capture hash不变。实际UI通过3928/3509/54 Begin Blit跳转和场景法线显示，留在3928 GBufferA/None；右键自动操作未唤出菜单，菜单与外点击关闭未验收。本机2x/4x可验证，8x/D24S8不支持，不能记PASS；MSAA直接Texture Viewer读回为既有独立缺口，当前逐样本验证使用真实GPU consumer。参见 [B458](../../docs/metal-replay/BATCH458_SUBMISSION_EVENTS.md)、[B459](../../docs/metal-replay/BATCH459_EXTENDED_DISCARD.md)。未提交/推送。

2026-10-03 最新 B457：新 UE GBufferA 的3300 FS/3808 Clear/4057 FB Color是CPU编码EID：producer14143于6790提交，consumer14146于6791提交（同queue11），实际先写后读；Metal尚未使用DX12/Vulkan的提交时baked EID模型，usage菜单已注明编码顺序。新增公共 LOAD/STORE DONT CARE图案（单样本tracked颜色/深度/模板、并行store、mip/slice；Fastest跳过），上传staging创建引用已平衡，隔离回放增长从58.9MB降至0.74MB。最终库 `1437b5cb`、GUI `459b09eb`：定向及untracked边界通过；固定库全量308/7786/3080通过（growth999424B/hash一致/exit0）；全量后同一UE3300/4057、六light overlays、Clear Before及None恢复通过，正常打开+两次整帧呈现与GBuffer/深度均保持基线。解锁后实际UI正常打开同一UE并选择3300，GBufferA显示场景法线，停在RGB/None；右键自动操作未唤出菜单，标题及外点击关闭仍未验收，不能记为UI通过。EID架构仍未平齐DX12/Vulkan。详见 [B457](../../docs/metal-replay/BATCH457_USAGE_ORDER_AND_DISCARD.md)。未提交/推送。

2026-10-03 最新 B455–B456：参照 Vulkan 补齐 Quad Overdraw 与 Triangle Size Draw/Pass，当前库 `8e332844`、GUI `48b2791c`。两项各十一种 Native 定向、同一 UE3300 两周期及 GBuffer Range/Histogram 复核通过；原资源字节不变。固定库合并验收全量308/7786/3080通过（growth5816320B/hash一致/exit0），全量后真实 UE全部覆盖层/统计复核及正常打开+两次整帧重置通过，GBuffer/深度及三份呈现图保持基线；bb793d8d 全量保留为历史。实际 UI 已正常打开并显示3300原图，但自动下拉框输入在独立标准 Qt 窗口同样无法切换，Quad/Triangle 渲染 UI 待确认，最新窗口停在3300 SceneColor/None。详见 [B455](../../docs/metal-replay/BATCH455_VULKAN_QUAD_OVERDRAW.md)、[B456](../../docs/metal-replay/BATCH456_VULKAN_TRIANGLE_SIZE.md)。持续推进，未提交/推送。

2026-10-03 最新 B454：参照 Vulkan/D3D12 完成原片元深度导出的 stencil-mask Depth Test，并修复 Depth/Clear Before 源 encoder context。库 `bb793d8d`：七种深度导出与八种 Clear Before Native 定向、同一 UE3112/3300两周期、固定库全量308/7786/3080（growth9502720B/hash一致/exit0）及全量后真实 UE复核通过。整帧正常打开和两次EID0恢复图都保持fc5f3afe基线。助手实际UI通过3112 Depth→None、3300 Clear Draw→Pass→None；未代替用户人工验收。参见 [B454](../../docs/metal-replay/BATCH454_ORIGINAL_FRAGMENT_DEPTH_MASK.md)。Quad四桶计数 Native组件通过，正在构建正式回放候选，本库全量不归于下一候选。持续推进，未提交/推送。

2026-10-03 最新 B453：参照 Vulkan 补齐 Clear Before Draw/Pass，当前库 `a086f3e5`。八种 Native 双 MRT/原 shader/混合/discard/深度导出/stencil 定向及已有六覆盖层通过；同一 UE EID3300 清空/恢复两周期与全量后复核通过，完整 UE 三份呈现图保持基线。固定库全量308/7786/3080通过（growth6127616B/hash一致）。当前实际 UI 待手动解锁：CUA 明确报告 Mac 锁定，已请求解锁一次；未把终端结果记作 UI。下一项 original-FS depth stencil-mask 的64x32 Native组件已通过，正式回放集成仍待。详情 [B453](../../docs/metal-replay/BATCH453_VULKAN_CLEAR_BEFORE_OVERLAYS.md)。目标 active，未提交/推送。

2026-10-03 最新纹理查看器对齐见 [B452](../../docs/metal-replay/BATCH452_VULKAN_PASS_AND_VIEWPORT_OVERLAY.md)：库 `0776deb7` 的 pass 归属、Viewport/Scissor 及其余五绘制覆盖层通过定向与同一 UE，两周期后输入/输出字节不变，助手 UI 的 Viewport/Scissor→Depth→None 切换和原图恢复通过，界面停在 EID3300 SceneColor。本库没有重跑全量，上一批 ac2a9748 全量作为历史记录。GPU 串行，不提交/推送，持续目标 active。

2026-10-03 Texture Viewer 当前工作见 [B449](../../docs/metal-replay/BATCH449_TEXTURE_VIEW_INSPECTION.md)、[B450–451](../../docs/metal-replay/BATCH450_451_NATIVE_DRAW_OVERLAYS.md)。库 `ac2a9748` 对同一 UE EID3300 的 Depth/Stencil/Drawcall/BackfaceCull/Wireframe 两周期检查通过，SceneColor 与 GBuffer/深度原始字节不变；固定库全量308/7786/3080通过（growth8716288B/hash一致），全量后同一 UE 五覆盖层及 GBuffer Range/Histogram 两周期通过，实际助手 UI 的五绘制覆盖层、Range/自动范围/Histogram/数值覆盖层/Pixel Context 均通过，完整 UE 两次 reset 保持基线，界面停在3300 SceneColor，未提交/推送。可运行 `bash util/buildscripts/scripts/test_metal_texture_view_macos.sh` 或 `bash util/buildscripts/scripts/test_metal_test_overlay_macos.sh` 做定向；后者可追加当前验收截帧路径与 EID3300。下方 ff444e68 是前批历史验收。 持续对齐目标保持 active；五项通过不代表所有 Overlay 已与 Vulkan 平齐，后续优先 pass 归属和其余 N/A 项。

2026-10-03 最终助手 UI 验收已完成：当前库 `ff444e68` 的定向、全量、全量后真实 UE 和实际 UI 均通过。新副本 `build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc` 已正常打开，界面停在光照 EID3300 的 Fragment Shader 输入页；GBuffer 命名/场景、usage 关闭与跳转、AIR/MSL 已核对。用户画面反馈尚待，未提交/推送。详见 [B448](../../docs/metal-replay/BATCH448_SUBMISSION_DESCRIPTOR_SCOPE.md)。下面记录保留为历史。

2026-10-03 最新本地候选 `ff444e68` 已通过定向、固定库全量及全量后的新旧UE验证；新UE整帧与原始缩略图精确一致，光照3300的GBufferA/C与Nanite后4349逐字节一致。新副本路径为 `build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`。最终UI待再次解锁；详情见 [B446–B447](../../docs/metal-replay/BATCH446_447_FRAME_FLOAT_AND_VISIBILITY.md)、[B448](../../docs/metal-replay/BATCH448_SUBMISSION_DESCRIPTOR_SCOPE.md)。下方旧候选记录保留作历史。

资源名称、usage 和 UI 对齐的当前进展见 [B444–B445](../../docs/metal-replay/BATCH444_445_RESOURCE_USAGE_AND_UI.md)。当前dafb37a9定向、全量及全量后同一UE3371/3551/188和整帧复核已通过；最终菜单/UI复核仍待解锁，B443验收保留作历史。

本轮 Inputs/shader 改动见 [B443](../../docs/metal-replay/BATCH443_UE_SHADER_INSPECTION.md)。
EID3371 支持从 uniform 解析的 bindless 纹理输入；Shader Viewer 可选择 **Metal AIR (Apple toolchain)**，显示捕获的中间代码。编译库没有原始 HLSL/MSL 时不会伪造源码。验收状态以 B443 为准。

# 本地 M2 / Testproj 当前运行入口（2026-10-03）

当前已推送基线为 `95214a2`。后续 DX12/Metal 对比发现的事件归属、blit 边界失败和光照部分回放数据偏差，见
[B440–B442 修复与分项验证](../../docs/metal-replay/BATCH440_442_UE_EVENT_NAVIGATION.md)。前一轮候选 `c7bb6da1` 的全量及随后的同一 UE 复核通过，助手 UI 已验证188/3182/3371/3551。当前 B444–B445 库为 `dafb37a9`，应用已重建并同步。定向及全量通过，最终真实 UE/UI 结果见上方报告；Mac 锁定，尚未完成最终菜单与界面复核。旧验证记录保留在
[2026-10-02 checkpoint](../../docs/metal-replay/CHECKPOINT_2026-10-02_UE_REPLAY.md)。

已安装的 UE5.8.3 Testproj 插件、隔离 MetalRHI provider 与 `build-macos-debug` 配合使用。启动前关闭正在进行回放的 qrenderdoc 与 UE Editor；自动截帧需要已解锁的桌面。隔离 provider 仅用于本次进程，不替换安装引擎。本入口保留当前已有 capture，然后等待45秒自动截一帧并退出 UE，严格 pre-submit 检查通过后生成独立 `replay.rdc`，执行正常 OpenCapture、两次 EID0 重置及原生呈现纹理读回，按官方缩放/jpge90与原始捕获缩略图逐字节比较。不会自动跑全量、提交或推送。

```bash
cd /Users/crossous/Developer/renderdoc-metal
# 只检查工程、插件、库和隔离 provider 的路径与哈希
python3 util/ue/run_testproj_metal_replay_macos.py --check
# 自动新截帧、严格预检、生成副本、真实 GPU 图像验证
python3 util/ue/run_testproj_metal_replay_macos.py --capture
# 对已有原始截帧执行相同流程（保留原件）
python3 util/ue/run_testproj_metal_replay_macos.py --replay '/absolute/path/original.rdc'
# 打开上一步日志给出的绝对 candidate 路径；采用正常打开，无诊断覆盖标志
python3 util/ue/run_testproj_metal_replay_macos.py --ui '/absolute/path/replay.rdc'
```

当前使用同一份副本（原始 faa8540e 全部命令及 binary 保留；本轮修复不需要重新截帧）：

```bash
python3 util/ue/run_testproj_metal_replay_macos.py --ui '/Users/crossous/Developer/renderdoc-metal/build-macos-debug/local-m2-descriptor-replay/testproj-20261002-211846-505594/replay.rdc'
```

此命令在前台等待 qrenderdoc 退出，终端保持占用是正常的。当前调查可先看 EID188（blit 边界）、EID3371（实际批量光照 draw）、EID3551 的 Target3/Texture49933（GBufferC；3551本身是 render-pass begin，不是 draw）；EID3251 为 depth/stencil-only draw，Target4 为 CustomData，不能把这些无颜色/零值输出直接判为场景丢失。

此前 b814fdbb 已验证正常打开、完整900×640图像、BasePass五MRT和体积MRT末层；详细定向、当前库全量、真实UE与UI结果见 [B439](../../docs/metal-replay/BATCH439_RETIRED_PRIVATE_TEXTURE_BACKING.md)。新截帧仍按上方 `--capture` 流程逐份严格审计和验证。

历史诊断和可复用构建已按文件SHA256归档到 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261002-2050`，原路径用软链接保留；使用历史产物时需连接此外置磁盘。34份项目原始截帧均留在本机，当前库、隔离provider和已验收副本留在本机。归档清单、每文件校验及恢复说明见外置目录的 `README.md`、`migration.jsonl` 和 `additional-migration.jsonl`。

结果输出到 `build-macos-debug/local-m2-descriptor-replay/testproj-时间/`：`results.json` 单独记录真实UE、定向、全量、UI状态；`audit/candidate-audit.json` 证明原命令/元数据及全部binary/thumbnail保留；`images/normal-replay.log` 和三次原生 `.bin` 记录实际 GPU 进展。`--verify '/absolute/path/replay.rdc'` 可单独重做已有候选的图像验证。若失败，只运行一次并保留日志，没有自动重试或跳过 GPU 工作。

原始 UE `.rdc` 不声明完整 descriptor coverage：主堆布局本身不够。新候选只增加一个 coverage65 声明；生成前必须经过同一库的严格 provenance/order/lifetime pre-submit 检查，转换后审计每一原始chunk和binary。候选预检通过不等于真实GPU或UI通过，三者分别记录。此流程只覆盖当前有界 UE 捕获内容，不能用于宣称任意UE图形功能已经支持。首次GPU偏差及局部提交前缀修复见 [B434](../../docs/metal-replay/BATCH434_RETIRED_DESCRIPTOR_UNIFORM_REUSE.md)、[B435](../../docs/metal-replay/BATCH435_LATE_SIGNAL_SUBMISSION_PREFIX.md)。下方旧机器/旧库入口保留作历史记录。

# UE 5.8 Metal 首帧接入

**当前入口（2026-09-29 BATCH320）：** 项目按钮已改为受控抓取当前 scene
viewport：渲染线程开始捕获、主动绘制 viewport、等待编辑器 Slate present，
再结束。不要用旧 `TriggerCapture` 版本判断是否截到场景 pass。本机插件已编译，
库和 qrenderdoc 内嵌库 SHA256 均为
`ca90af4c95ab69858145eadf1e3023de16cbbb424d565eff864e3b91c7432872`。
用户旧 `frame1770` 在 qrenderdoc 内容验收失败：只有 Slate pass、黑色 RT；
新按钮尚待用户点一次验证。见 [BATCH320](../../docs/metal-replay/BATCH320.md)。
以下 BATCH315–318 入口与 `TriggerCapture` 说明为历史记录。

**历史入口（2026-09-29 BATCH315–318）：** 新 v0x10 库 SHA256
`c13816cf6485ab945aa08ba476bc173613d5fbd609cb15518a6e65aabee49354`；
使用 `RENDERDOC_METAL_BUILD_DIR=/tmp/rdm-t312-build bash
util/ue/run_ue_metal_capture_macos.sh --run` 启动 UE，在 Empty、非 Nanite
视口点一次截帧。新 `.rdc` 生成后，可用 `RENDERDOC_METAL_BUILD_DIR=/tmp/rdm-t312-build
bash util/ue/replay_ue_metal_once_macos.sh <capture.rdc>` 单次有界打开。
旧 v0xF `UE58_frame5394.rdc` 缺 BC 纹理帧首初始内容，最终库安全拒绝；
需用此库重截，才能定位下一个真实阻塞。详见
[BATCH315–318](../../docs/metal-replay/BATCH315-318.md)。
下方 M5 与更早记录为历史证据，以本段和 STATUS 顶部为准。

本目录提供项目级 Mac Editor 插件和启动脚本，不修改 UE Engine。插件向 Level
Editor 视口工具栏及 **Tools** 菜单添加 **Capture Metal Frame**。按钮调用本仓库
`RENDERDOC_GetAPI` 的 `TriggerCapture()`，只请求下一帧；它不能让已经运行的
进程事后加载 Metal hook。必须由启动脚本从进程创建时注入 dylib。

本机项目 `SocoTestProj` 的 UE `Build.version` 为 **5.8.3**，初始地图是
`/Game/FirstPerson/Lvl_FirstPerson`。这是用于定位首个真实阻塞点的入口，
尚未完成 UE 普通场景与人工 UI 验收。下一次请在编辑器中使用一个简单的非 Nanite
场景；项目现有默认渲染设置包含 ray tracing、VSM 和 Lumen，不代表最小配置。

**当前停点（09-29 M5）：** `UE58_frame833.rdc` 在 Private buffer 初始状态
恢复后仍于 `ResourceId::503251` 等待超时。UE SM6 bindless heap 内有旧进程
GPU VA；旧帧缺捕获地址映射，不能宣称 viewer 已能打开。定向结果见
`docs/metal-replay/UE58_M5_PRIVATE_INITIAL_BINDLESS_2026-09-29.md`。新编译
viewer 备份在 `build-private-initial-viewer/bin/qrenderdoc.app`，未做人工 UI。
原 `build-qrenderdoc/bin/qrenderdoc.app` 也已同步为相同构建。
启动脚本目前默认读取同目录 `lib/librenderdoc.dylib`；`--check` 会打印它的
SHA256，必须等于本批证据里的 `19d491f0...` 才是这一版。旧
`build-ue-debug` 保留作历史构建，仍可用 `RENDERDOC_METAL_BUILD_DIR` 显式选择。

## 准备（终端）

当前这台机器的 `SocoTestProj` 已安装并编译插件；以下安装与构建命令用于
另一个项目或重建，不要在同一项目重复运行安装器（它会拒绝覆盖现有插件）。

```bash
cd "/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312"
python3 util/ue/install_renderdoc_metal_plugin.py \
  "$HOME/Documents/Unreal Projects/SocoTestProj/SocoTestProj.uproject"

# 源码和构建目录带空格时，CMake 在 -force_load 处会拆分路径。用无空格别名：
test -L /tmp/renderdoc-metal-t312 || ln -s "$PWD" /tmp/renderdoc-metal-t312
cmake -S /tmp/renderdoc-metal-t312 -B /tmp/renderdoc-metal-t312/build-ue-debug \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-Wno-nontrivial-memcall \
  -DENABLE_METAL=ON -DENABLE_GL=OFF -DENABLE_GLES=OFF -DENABLE_EGL=OFF \
  -DENABLE_VULKAN=OFF -DENABLE_QRENDERDOC=OFF -DENABLE_PYRENDERDOC=OFF \
  -DENABLE_RENDERDOCCMD=ON
cmake --build /tmp/renderdoc-metal-t312/build-ue-debug --target renderdoc renderdoccmd -j 12

"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
  SocoTestProjEditor Mac Development \
  -Project="$HOME/Documents/Unreal Projects/SocoTestProj/SocoTestProj.uproject"

bash util/ue/run_ue_metal_capture_macos.sh --check
```

上述编译标志仅为本机 Xcode 26 的旧代码警告兼容设置；未调整 Metal 功能守卫。
`--check` 只记录版本、签名和 hash，不启动 UE。

## 当前验证状态

用户 `20260929-064750` 会话已用 SHA256 `1ab4448a93b2…` 的新注入库点击按钮，
成功保存 `Saved/RenderDocMetalCaptures/UE58_frame833.rdc`（SHA256
`472dfa48a56cbaa43b6cc97d5f82d0471263de0c0fdd47049dde166e1df6f163`）。
这次 UE 截帧结束没有重现旧的生命周期崩溃。viewer 位于
`/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312/build-qrenderdoc/bin/qrenderdoc.app`；
CLI 单次回放该帧在 `MTLHeap::newTexture(offset)` 安全拒绝，首个未接通纹理类型
是 3D placement。viewer 可以由用户手动启动，但目前不能据此确认该 UE 帧的
画面回放正确。见 `docs/metal-replay/UE58_M4_QRENDERDOC_3D_PLACEMENT_2026-09-29.md`。

用户 `20260929-024537` 再次点击按钮时，旧库在截帧结束阶段因 command buffer
生命周期错误崩溃，没有新 UE `.rdc`。当前 worktree 已修复该路径并通过短
autorelease pool 的原生、注入、API/CLI 定向测试；后续用户截帧已跨过该崩溃点。
证据见 `docs/metal-replay/UE58_M4_CAPTURE_LIFETIME_2026-09-29.md`。
重试前可用启动脚本输出的 library SHA256 确认当前 dylib 是
`1ab4448a93b2c8f08211087bb40f2afbf72a9231d98c978918ea5425fe3b6ef9`。

注入、编辑器启动和按钮触发已经由终端验证；`20260929-010300` 会话取得
`Saved/RenderDocMetalCaptures/UE58_frame99.rdc`。在当前 worktree 构建的
`librenderdoc.dylib` 下，这一帧已通过 Metal Validation 的单次 CLI 回放，
API 探针对 165 个 draw 进行六次前进与回跳的 pipeline 身份检查也通过。
交错 command buffer 回放修复及命令见
`docs/metal-replay/UE58_M4_INTERLEAVED_REPLAY_2026-09-29.md`。这是定向终端
验证；该项目还不是固定的非 Nanite 最小场景，尚未将原生画面与回放 GPU
输出逐项对照，也没有人工 UI 验收。

本机 ZenServer 未安装，脚本默认使用 `-ddc=InstalledNoZenLocalFallback`，
避免在 Zen 检查处反复等待。可用 `UE_METAL_DDC_MODE` 覆盖。
可用 `UE_METAL_EDITOR_ARGS` 传递额外编辑器参数；脚本将其写入 session manifest，
并在 UE stdout 中记录完整启动命令。当前项目的 `METAL_SM6` 在
`-BindlessOff` 下报告 3157 个全局 shader 编译错误，窗口未创建；
本轮不要以该参数启动。详见
`docs/metal-replay/UE58_M6_BINDLESSOFF_STARTUP_2026-09-29.md`。

## 后续单次验证命令

```bash
bash util/ue/run_ue_metal_capture_macos.sh --run
```

若已用旧版脚本启动编辑器，请先正常关闭该 UE 进程，再运行上面命令。
旧版脚本经 `/usr/bin/arch` 启动，而 macOS 会移除传给这个受保护工具的
`DYLD_*` 变量，导致按钮可见但 `RENDERDOC_GetAPI` 缺席。新版直接启动
UE 的 arm64 可执行文件；不可在已经运行的编辑器中补注入。

确认编辑器日志中有 `RenderDoc API resolved from ...librenderdoc.dylib`，再点
**Capture Metal Frame** 一次。截帧写入项目 `Saved/RenderDocMetalCaptures`。
每次启动的 manifest、UE stdout 和 RenderDoc 日志存于
`Saved/RenderDocMetalSessions/<时间>`。按钮会显示请求已发出或已保存的路径；
若 60 秒内没有文件，会明确提示超时。启动脚本最长运行 1800 秒，可用
`UE_METAL_TIMEOUT_SECONDS` 调整；超时会停止本次编辑器进程组。脚本不自动点击、不循环截帧。

如启动失败，先检查 session 中是否有 `DYLD_INSERT_LIBRARIES` 被忽略、
`Loading into ...UnrealEditor`、`RenderDoc API resolved`，以及最早的
`METAL_NOT_HOOKED`/`METAL_CHUNK_NOT_HANDLED`/`RDCERR`。没有这三类注入证据
时，不能把失败归因于具体 Metal API。仅在生成有效 `.rdc` 后，对该文件单次
运行 `build-ue-debug/bin/renderdoccmd replay --loops 1 <capture.rdc>`。

`qrenderdoc` 的现有目标控制功能可在连接到运行中的目标后请求截帧，但本项目
尚未在 UE 5.8 上验证该路径。本插件是本轮固定的一按钮入口。
