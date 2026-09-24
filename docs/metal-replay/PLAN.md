# Metal Replay 总体计划

## 原则

1. 先打通纵向链路，再增加 API 宽度：UI 启动、样例原生运行、截帧、加载事件、replay 一个三角形。
2. Replay 是产品目标，capture 是不可省略的输入前提；先支持受控测试程序，不把任意应用注入列入首版。
3. 每个功能必须有最小样例、`.rdc` 夹具或自动测试，以及 UI/数据验证方法。
4. 每条切片完成自动验证时更新 `STATUS.md`；批次关闭前补齐最终证据，并写好下一批的细化任务。
5. 不用空实现伪装支持。未支持能力必须返回明确结果或在 UI 中禁用，而不是崩溃或给出错误数据。

## Agent 推进节奏（省额度稳定模式）

后续默认以已写明边界的相邻 2–3 条 `PHASEx.md` 切片作为一次工作批次，而不是每完成一个 Txx
就等待用户再次发送“继续”。当前计划批次固定为 `BATCH31-32.md` 中的 T30/T31，不自动扩成 10 个
phase。每条切片按 fixture/native -> capture/structured data -> replay/readback/state -> 自动断言
推进；前一条自动链路通过并记录批末 UI 待验后继续后一条。两条切片经最终联合验证和同一轮
用户 qrenderdoc L4 验收后一起关闭。T28/T29 的最终联合自动与用户同轮 L4
均已通过，`QA_PENDING.md` 当前无待验项；
从 BATCH29-30 起按 [QA_GUIDE.md](QA_GUIDE.md) 将 GUI 可见交互交给用户验收。

为减少随 T 场景数量增长的重复消耗，每条切片开发时只构建受影响目标，并立即运行当前 fixture
必要的 native/capture/replay 自动验证及受影响旧路径的定向断言。批末在最终构建上执行各 T 的
CLI/lifecycle 与旧 T 清单的并集，重复 T 编号只跑一次；可复用最终代码上未受后续修改影响的
已通过结果。命令行可判定的 QA 由 agent 完成；最终正式 captures 准备好后，
agent 给出合并批内检查项的一次性 GUI 验收单，由用户在同一轮最新 qrenderdoc
中依次打开并反馈。每个 T 的人工验收状态跨批次登记在 [QA_PENDING.md](QA_PENDING.md)；
用户未回复、漏看或只反馈部分步骤时，未验功能持续“等待用户 L4”，后续功能可继续
开发，但对应阶段/批次不关闭。每次结果和交接须列出全部仍待人工 QA 的 T。
完整 `test_metal_capture_macos.sh` 与全部 capture 的 CLI replay/lifecycle 仅在较大里程碑、
发布/合并前，或发现无法由定向测试覆盖的具体跨场景风险时
运行，不是每个 Txx 阶段的关闭门槛。公共 replay、资源生命周期或 UI 代码变更先按受影响路径
选旧 fixture；只有范围无法合理界定时升级全量。修复后重跑受影响项。UI 布局和操作语义继续向
RenderDoc 其他图形 API 的标准页面收敛。

每份阶段文档预先列明当前 Txx、必跑旧 T、条件触发旧 T、批末 UI 验收项，并明确全量回归条件
及完整 T 范围；批次文档另列去重后的联合清单。实现改变影响范围时先更新这两处清单和原因，
批末只记录实际执行结果；具体格式见 `HANDOFF.md`。功能必要就开发并验证，不能以减少 QA
为由缩小实现；确实不必要的功能须说明依据并更新范围。

批次关闭后可以在当前对话继续，也可以根据上下文状态新建任务；两者都从 `STATUS.md` 第一项
未完成工作接手。接手时读取入口、当前状态、批次与阶段清单及交接检查点；历史阶段文档按需追查。
批次中途只有在实现状态已经写入 `STATUS.md` 的安全检查点才建议 compact；compact 后仍从
检查点继续，不重新跑已验证基线。
精确的交接输出与下一任务提示见 `HANDOFF.md`。

## 首版完成定义

首版（MVP）同时满足以下条件才算完成：

- qrenderdoc 在目标 macOS/Apple Silicon 机器上稳定启动。
- qrenderdoc 能识别并打开由受控测试程序生成的 Metal `.rdc`。
- Event Browser 能显示 render pass、draw、dispatch/blit（若测试中存在）及 debug marker。
- 能正确 replay 基础非索引、索引和实例化绘制。
- Buffer Viewer、Texture Viewer、Pipeline State、Mesh Viewer 和 Shader Viewer 对首版矩阵中的
  用例给出正确内容。
- 对支持矩阵中的每个 P0/P1 项都有可重复测试；重开 capture 和切换 event 不产生明显资源泄漏或崩溃。
- 已知限制被写入用户文档，不声称支持未覆盖的高级 Metal 特性。

## 阶段 0：仓库与可重复构建基线

目标：建立不依赖个人 shell 状态的 macOS 构建和启动路径。

任务：

- [x] M0.1 检出 RenderDoc `v1.46` 并记录精确提交。
- [x] M0.2 从 detached tag 创建开发分支 `metal-replay-v1.46`。
- [x] M0.3 盘点本机 Xcode、SDK、CMake、Ninja、Qt 和构建依赖。
- [x] M0.4 安装/修复 Qt 5.15、autoconf、automake、pcre 和必要的 bison 工具。
- [x] M0.5 先以 `ENABLE_METAL=OFF` 完成最小 qrenderdoc Debug 构建并启动 UI。
- [x] M0.6 以 `ENABLE_METAL=ON` 完成构建并启动 UI，记录与非 Metal 基线的差异。
- [x] M0.7 增加仓库内的 macOS 配置/构建/启动脚本或 CMake preset，避免手工命令漂移。
- [x] M0.8 记录产物路径、启动命令、日志位置和清理方式。

建议的最小配置从关闭无关驱动开始，以缩短反馈时间：

```sh
cmake -S . -B build-macos-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DQMAKE_QT5_COMMAND=<qt5-qmake-absolute-path> \
  -DENABLE_METAL=ON \
  -DENABLE_GL=OFF \
  -DENABLE_GLES=OFF \
  -DENABLE_EGL=OFF \
  -DENABLE_VULKAN=OFF
cmake --build build-macos-debug --target qrenderdoc
```

准确 target 名称和 app 路径以首次成功配置结果为准，随后写入脚本和 `STATUS.md`。

阶段验收：

- 一条文档化命令能从干净 build 目录构建 qrenderdoc。
- RenderDoc UI 可启动、显示主窗口并正常退出。
- `ENABLE_METAL=ON` 构建无未解释的链接或 Objective-C runtime 错误。

## 阶段 1：Metal 样例与最小 capture 闭环

目标：先让一个确定性的 Metal 样例原生运行，再通过 RenderDoc 生成结构正确的 `.rdc`。

任务：

- [x] M1.1 固定第一批样例来源，先选 T00 清屏与 T01 彩色三角形。
- [x] M1.2 将最小 macOS Metal 测试程序接入仓库构建，验证未注入时稳定运行。
- [x] M1.3 验证并修复测试程序加载 RenderDoc、hook `MTLCreateSystemDefaultDevice` 和对象包装路径。
- [x] M1.4 打通 frame trigger、`CAMetalLayer/nextDrawable`、present 和 `.rdc` 写盘。
- [x] M1.5 补齐 T00/T01 所需的 buffer、library/function、pipeline、render pass、draw 的 capture 序列化。
- [x] M1.6 用 structured export/chunk inspection 检查 `.rdc` 包含预期资源、命令和初始内容。
- [x] M1.7 固化“一键构建样例、一键截帧、输出 capture 路径”的测试脚本。

阶段验收：T00/T01 原生输出正确；RenderDoc 能稳定生成非空 Metal `.rdc`；文件 header、driver、
frame section 和核心 chunks 可检查。此时尚不要求 qrenderdoc 成功 replay。

状态：2026-09-20 已完成。自动验证入口为
`./util/buildscripts/scripts/test_metal_capture_macos.sh`。

## 阶段 2：Replay 后端骨架与基础图形 replay

目标：让 qrenderdoc 打开阶段 1 生成的 `.rdc`，重放一个 render pass 中的普通三角形，并形成
正确事件树。

任务：

- [x] M2.1 让 `MetalReplay` 实现 `IReplayDriver` 并注册 Metal replay provider。
- [x] M2.2 打通 `RDCFile -> ReadLogInitialisation -> ProcessChunk -> replay device`。
- [x] M2.3 实现 `AddEvent()`、`AddAction()`、event/action ID 分配和基础 action。
- [x] M2.4 补齐 T00/T01 的 command queue、command buffer、render encoder replay 生命周期。
- [x] M2.5 支持 T00/T01 render pass attachment 的 load/store/clear 基础语义。
- [ ] M2.6 支持 viewport、scissor、cull、front face、fill、depth bias 等常用固定状态。（T02
  所需 viewport/scissor/cull/front face 已完成 capture 与 GPU replay；fill/depth bias 等仍待 fixture。）
- [x] M2.7 支持基础 `drawPrimitives`，并让输出窗口通过现有 `CAMetalLayer` 显示结果。
- [x] M2.8 实现选定 event 的 full/without-draw/only-draw replay 区间语义。
- [x] M2.9 为当前未支持接口实现稳定降级，避免 UI 调用空指针或得到伪数据。

阶段验收：T00/T01 capture 能打开；Event Browser 的 pass/draw 次序正确；选择 draw 后输出与
原程序基准图一致。该 T00/T01 纵向切片已于 2026-09-21 完成，并通过同进程循环打开/关闭和
qrenderdoc 重开验证。M2.6 已由 T02/T07 补入 raster、depth/stencil 子集，fill/depth bias 等完整固定
状态仍随后续 fixture 扩展；索引绘制在阶段 5 的 Mesh Viewer 纵向切片中完成。

## 后续 feature 推进方式

阶段 3 以后不采用“先把所有样例都截成 `.rdc`，最后统一做 replay”的瀑布方式。每个测试场景按
以下顺序闭环后，才进入下一个场景：

1. 样例未注入时原生运行正确并保存参考输出/参考数据。
2. RenderDoc 能截取该样例，且 structured chunks 与调用参数一致。
3. replay 能执行到该 feature，不出现未处理 chunk 或资源缺失。
4. 对应 Buffer/Texture/Pipeline/Shader/Mesh UI 数据正确。
5. 把样例和 capture/replay 检查加入回归测试。

## 阶段 3：Buffer、Texture 与初始内容

目标：资源能被正确创建、恢复、枚举和读取。

当前进度：T00/T01 所需的 shared vertex buffer 原始数据，以及单采样 2D
RGBA8/BGRA8 render target 的枚举、原始读取、像素拾取和 DDS 保存已经闭环；完整 storage mode、
texture 类型/格式和 blit 覆盖仍按本阶段后续任务推进。T02 已新增 shared vertex/index buffer 与
private `Depth32Float` attachment 的创建/恢复，并自动逐字节验证 16/32-bit index 数据。T03 已覆盖
shared 4x4 RGBA8 sampled texture 的 descriptor、`replaceRegion` 初始内容、GPU replay、逐字节读取与
四个已知 texel 的 `PickPixel()`；T09 已进一步完成 RGBA8 2D mip、2D array 和 cube 的确定性
子资源上传、读取与显示；T10 已验证 shared buffer copy/fill、RGBA8 2D texture copy 和 mipmap
generation 的 GPU replay 与数据读取。Texture view 和其他 storage mode 仍未完成。

任务：

- [ ] M3.1 完成 buffer 创建路径、storage mode、初始内容和生命周期映射。（shared vertex/index
  buffer 已覆盖；private/managed buffer 和更新路径待完成。）
- [ ] M3.2 实现 `GetBuffers()`、`GetBufferData()`、名称和派生关系。（T01 所需枚举和 shared
  buffer 原始字节已完成；名称、派生关系和更多 storage mode 待完成。）
- [ ] M3.3 完成 texture descriptor、子资源、mip、array/cube、初始内容和 view 映射。（T03 的单层
  RGBA8 2D 和 T09 的 2D mip/array/cube descriptor 与 slice-aware `replaceRegion` 已完成；cube array、
  texture view、更多格式/storage mode 待扩展。）
- [x] M3.4a 实现 T00/T01 单采样 2D RGBA8/BGRA8 的 `GetTextures()`、`GetTextureData()`、
  `PickPixel()` 和保存路径。
- [ ] M3.4b 扩展常见整数/浮点/depth/stencil/压缩格式及 cube array/3D/MSAA 子资源展示。
  （T09 的单采样 RGBA8 2D mip、2D array 和 cube 已完成。）
- [x] M3.5 支持 blit copy/fill/mipmap generation 中 P0/P1 用例。（T10 的 buffer→buffer、
  RGBA8 2D texture→texture、buffer fill 与 2D mipmap generation 已通过数据/UI 验证；其他
  overload、格式和跨 queue 场景仍按后续 fixture 扩展。）
- [ ] M3.6 验证 render target、depth/stencil、MSAA resolve 和 sampled texture。（T00/T01 render
  target、T02 depth target、T03 sampled texture、T07 combined depth/stencil attachment 与 T08
  4x MSAA 显式 resolve 已验证；更多格式和 attachment 组合待扩展。）
- [ ] M3.7 增加大资源、零长度读取、越界范围和已销毁资源的错误测试。

阶段验收：Buffer Viewer 和 Texture Viewer 能通过 `TEST_MATRIX.md` 中所有 P0 资源用例，且内容
与测试程序写入的已知模式逐字节/逐像素匹配。

## 阶段 4：Pipeline State、Shader 与绑定反射

目标：在 UI 中准确解释选定 draw 的 Metal 管线和资源绑定。

当前进度：source-created library 的 MSL、`vs_main`/`fs_main` 入口和 vertex/fragment stage 已经通过
通用 Shader Viewer 展示并自动验证；T01 还已建立最小 Metal pipeline state snapshot 和专用状态页，
能显示 render pipeline、shader、topology、vertex buffer 与 color target。完整 descriptor、其余 stage
bindings、编译选项和 function constants 尚未实现。T02 的 interleaved vertex descriptor、
`Depth32Float` pipeline format、less/write depth state 与 front-face/cull/scissor 已完成 capture/XML
往返、真实 GPU replay、Metal pipeline snapshot 和 qrenderdoc 状态页；T02 vertex/index 输入现已
通过通用 `PipeState` 接入标准 Buffer/Mesh Viewer。Metal 专用 Pipeline 页的视觉布局仍是过渡实现，
后续按 D021 逐步收敛到 RenderDoc 其他后端的分组和操作习惯。T03 已进一步完成 fragment texture/
sampler 的 capture/replay、通用 descriptor 查询、Pipeline 展示和标准 Texture Viewer/Resource
Inspector 跳转。P4.4 的首个 UI 切片已复用标准 `PipelineFlowChart`，按 IA/VS/RS/FS/OM 分页，并
实现真实空槽显示及 shader 直接跳转；第二切片已将资源表迁移到 `RDTreeWidget`/`RDHeaderView`，
接入通用资源菜单、缩略图/预览分发与五阶段 HTML export。第三切片现已在 replay 创建 pipeline 时
请求 Metal argument reflection，枚举 T03 的 `colourTexture`/`colourSampler`，并用同一资源额外绑定到
shader 未声明的 slot 1 验证真实 used/unused 过滤：默认只显示 slot 0，启用 `Show Unused Items` 后
显示 slot 1。T04 又完成 fragment constant buffer、动态 offset、事件级 descriptor/reflection 与标准
Buffer Viewer 跳转；T05 已完成多 vertex buffer、per-instance layout、base instance 和标准
IA/Mesh/Buffer Viewer。T06 已完成两个 color attachment、多输出 action、逐 attachment blend state、
标准 OM Blend State 表、Texture Viewer FB0/FB1 切换与 HTML export。T07 已完成 combined
depth/stencil attachment、front/back stencil state、dynamic reference、五个 draw 的事件结果以及标准
OM Depth/Stencil 表和 HTML export；共享 `PipelineFlowChart` 也补齐 Left/Right/Home/End 键盘导航。
T08 已进一步完成 4x multisample color attachment、显式 resolve、sample state、标准 OM
Multisample/Color/Resolve 分组、Texture Viewer resolve 跳转与 HTML export。T09 已完成 2D mip、
2D array 和 cube 的 fragment binding/descriptor、标准 Texture Viewer 子资源切换与 DDS 保存。
T10 还验证 blit 结果进入 final draw 的 fragment buffer/texture binding。T11 已实现 compute
texture filter 的 pipeline/shader、direct read/write texture binding、dispatch event/usage、CS Pipeline
页及通用 Texture Viewer 跳转；自动 L3 和最新 qrenderdoc L4 均通过，T11 已关闭。下一项按
`PHASE13.md` 已完成单层直接 argument buffer 的 API 语义重建、通用 descriptor/reflection、标准
Buffer/Texture/Resource 跳转、DDS/HTML export；自动 L3 和最新 qrenderdoc L4 均通过，T12 已关闭。
`PHASE14.md` 已完成单次间接 draw 参数：offset 16 的 16-byte 参数区、`Indirect` action/usage、
标准 Buffer Viewer/Resource 跳转、raw bytes/HTML 导出、异常 offset 拒绝、T00-T13 全量 L3 与最新
qrenderdoc L4 均通过。`PHASE15.md` 又完成 UInt16 index offset 4、base vertex 1、base instance 1
的 indexed instancing；标准 IA/Mesh/Buffer Viewer、15×10 lifecycle、T00-T14 L3 与最新 qrenderdoc
L4 均通过。`PHASE16.md` 的 T15 已补 Point/Line/Line Strip：native、capture/XML、
action、event seek、标准 VS Input/Mesh/Buffer/Resource/DDS、T00-T15 L3 与最新 qrenderdoc L4
均已通过。`PHASE17.md` 的 T16 已补 vertex texture/sampler 直接绑定：四象限原生/重放像素、
VS reflection/descriptor/usage、标准 Viewer 与 UI DDS/HTML、T00-T16 L3 和最新 qrenderdoc L4
均已通过。`PHASE18.md` 的 T17 已补 VS/FS texture/sampler 批量绑定、空槽和 used/unused，
T00-T17 L3 与最新 qrenderdoc L4 均通过。`PHASE19.md` 的 T18 已补 fragment storage buffer
slot/offset、reflection/descriptor/usage、FS 标准页面、Buffer/Resource 跳转与 HTML/CSV 导出；
T00-T18 L3 与最新 qrenderdoc L4 均通过。`PHASE20.md` 的 T19 已补 vertex storage buffer、
IA/storage 分类、VS 标准页面和 `VS_Resource`；T19/T18/T16/T02/T05 定向与最新 qrenderdoc L4
通过，未触发全量 L3。`BATCH21-22.md` 又完成 T20 单命令 ICB、T21 indexed indirect、
联合 T01/T02/T05/T13/T14/T20/T21 自动验证及同轮 qrenderdoc L4；T21 的 20-byte 五字段
参数、6-byte 精确 index 范围和标准 IA/Mesh/Buffer/Resource 均通过，通用 Viewer 的 indirect
标签/格式修正后 T13 非索引路径也复验通过。`BATCH23-24.md` 又完成 T22 非零 range 的多命令
ICB 和 T23 indexed ICB：逐命令 action/state/usage、UInt16 index 4/6、两实例 Mesh 与标准
Viewer 均通过；最终联合 T01/T02/T13/T14/T20/T21/T22/T23 自动、9×10 lifecycle 和最新
qrenderdoc 同轮 L4 通过，L3 未触发。`BATCH25-26.md` 又完成 T24 reset 后重编码与 T25
混合非索引/indexed ICB：reset snapshot、组合 commandTypes、逐命令 action/state/usage、
联合 11×10 lifecycle 和最新 qrenderdoc 同轮 L4 通过，L3 未触发。`BATCH27-28.md`
又完成 T26 pipeline inheritance 与 T27 buffers inheritance：两次 execute 的 pipeline
17/18 和 Buffer 16/17 offset 16、逐 draw state/usage、红蓝输出、异常拒绝均通过；联合
11×10 lifecycle 和同轮 qrenderdoc L4 通过，L3 未触发。此后 `BATCH29-30.md`
完成 T28 compute `dispatchThreads` 与 T29 compute buffer binding；当前计划批次
`BATCH31-32.md` 已规划 T30 compute sampler 直接绑定与 T31 compute texture/sampler/
buffer 批量绑定，尚未实施。GPU 生成、compute ICB、heap、blit ICB 管理和多 queue
仍由后续独立场景推进。

任务：

- [ ] M4.1 保存并还原 render pipeline descriptor 的常用字段、color attachments 和 vertex descriptor。
  （T02 Float3/Float4 attributes、layout/stride/step 与 depth format 已验证往返并进入 snapshot/UI；
  完整格式和字段待继续覆盖。）
- [ ] M4.2 保存 depth/stencil、blend、raster、sample count 和 attachment 状态。（T02 depth
  compare/write、front face、cull、viewport/scissor，T06 两个 color attachment 与独立 blend，及 T07
  combined depth/stencil、front/back compare/operations/masks/reference，以及 T08 sample count、
  alpha-to-coverage 与 resolve attachment 已进入 capture/replay/snapshot/UI；其余组合等待后续 fixture。）
- [x] M4.3a 建立 T01 所需的 Metal pipeline state snapshot，接入 RenderDoc 通用 `PipeState`，并在
  qrenderdoc 显示 pipeline、shader、topology、vertex buffer 和 color target。
- [ ] M4.3b 扩展完整 render/depth/raster/blend/attachment 状态及后续 fixture 所需字段。（T06 已补
  多 color target 与逐 attachment blend，T07 已补 front/back stencil 与标准 OM Depth/Stencil 表；
  T08 已补 multisample/resolve 与标准 OM 分组；其余组合仍待覆盖。）
- [x] M4.3c 将 Metal Pipeline 页面接入标准 Controls、`PipelineFlowChart` 和 IA/VS/RS/FS/OM 阶段页，
  支持 empty-slot 显示及 shader/mesh/buffer/texture/resource 标准跳转。
- [x] M4.3d 对齐标准 `RDTreeWidget`/`RDHeaderView` 资源样式，复用通用上下文菜单与预览，并补齐
  IA/VS/RS/FS/OM HTML export。
- [x] M4.3e 以 shader resource reflection 启用真实 used/unused 过滤；无反射证据时保持禁用且不伪造
  使用状态。
- [ ] M4.3f 继续核对紧凑布局、键盘操作和更多状态字段，保持向 RenderDoc 标准页面收敛。（共享
  `PipelineFlowChart` 已支持焦点及 Left/Right/Home/End 导航；紧凑布局和后续状态字段继续收敛。）
- [x] M4.4a 枚举 source-created vertex/fragment function 的 entry point 和 stage。
- [x] M4.4b 枚举当前 source-created MSL 的直接 fragment texture/sampler bindings，并将反射数组索引、
  Metal 物理 slot 与静态 active 状态接入通用 descriptor 查询。（fragment buffer、T16 直接
  vertex texture/sampler 与 argument buffer 已在 M4.7 的后续切片分别覆盖。）
- [x] M4.5a 对 `newLibraryWithSource` 保存并在 Shader Viewer 显示真实 MSL 源码。
- [ ] M4.5b 保留 library 编译选项和 function constants 元数据。
- [ ] M4.6 对预编译 `.metallib` 显示可获得的函数/反射信息；没有源码时明确标记，不伪造源码。
- [x] M4.7 支持 buffer/texture/sampler 的 vertex 和 fragment stage 绑定。（T01/T02 vertex buffer、
  T03 fragment texture/sampler、T04 fragment constant buffer/dynamic offset、T16 直接 vertex
  texture/sampler、T17 批量 binding、T18 fragment storage buffer 与 T19 vertex storage buffer
  已完成。）

阶段验收：Pipeline State 页面与测试程序创建参数一致；点击 shader 能看到正确入口和可获得的
MSL；绑定资源可跳转到对应 Buffer/Texture。

## 阶段 5：Mesh Viewer 与常用绘制覆盖

目标：从 Metal vertex descriptor 和 draw 参数重建网格输入。

任务：

- [ ] M5.1 映射 Metal vertex format、buffer layout、step function、stride 和 attribute offset。（T02
  的 Float3/Float4、per-vertex、stride 28、offset 0/12 已进入 pipeline snapshot、通用
  `GetVertexInputs()`、Buffer Viewer 和 Mesh Viewer；完整格式仍待后续 fixture。）
- [x] M5.2 支持 16/32 位 index、base vertex、base instance 和 instance step rate。（T02 直接
  UInt16/UInt32 index、T05 非索引 base instance/instance step rate，以及 T14 非零 index byte offset、
  indexed base vertex/instancing 均完成自动与标准 UI 验证；其他 Metal draw overload 另列边界。）
- [ ] M5.3 为非标准/缺失 vertex descriptor 提供手工格式查看能力和清楚限制。
- [x] M5.4 验证多 vertex buffer、interleaved/deinterleaved、instancing 和 primitive 类型。（T02/T05/T14
  覆盖 interleaved 与分离 per-vertex/per-instance buffer、直接及 indexed instancing、
  TriangleList/Strip；T15 覆盖 PointList/LineList/LineStrip 与非零 vertex start。其他 vertex
  format 与 Metal draw overload 仍按独立 fixture 扩展。）
- [x] M5.5 校验 Mesh Viewer 的 VS input 与 replay 图像一致。（T02 已完成 UInt16/UInt32 indexed
  表格/线框；T05 已完成 base instance 后的 per-instance offset/colour 切换、raw position preview 与
  三实例 GPU 输出自动/实机验证。）

阶段验收：测试矩阵中的 triangle、indexed cube、多 buffer 和 instanced mesh 均能正确显示顶点值、
索引和几何形状。

## 阶段 6：Compute、同步与常用扩展面

目标：覆盖常见图形工作负载中与渲染紧密相关的 compute/blit/synchronization。

任务：

- [ ] M6.1 支持 compute pipeline、dispatch threadgroups/threads 和资源绑定。（T11 已完成
  `dispatchThreadgroups` + 直接读写 2D texture；T28 `dispatchThreads` 与 T29 直接 compute
  buffer 绑定的自动验证与用户 L4 均已通过。compute sampler 与更广资源绑定仍未覆盖。）
- [ ] M6.2 支持常用 blit encoder 操作以及 encoder/command buffer 间资源可见性。（T10 已完成
  同一 command buffer 中 blit→render 的 copy/fill/mipgen 与可见性；跨 command buffer/queue、
  更多 blit overload 和 managed-resource 同步待独立 fixture 验证。）
- [ ] M6.3 支持 fence/event/managed-resource 同步中实际测试需要的子集。
- [x] M6.4 支持 argument buffer 的只读查看与常见资源引用解析。（T12 已覆盖 function argument
  encoder、单层 buffer slot 0、`id(0)` RGBA8 texture、`id(1)` sampler、间接资源 capture/replay、
  通用 descriptor/reflection、标准 Buffer/Texture/Resource Viewer 跳转与 DDS 保存；嵌套、数组、
  bindless/heap 和 compute argument buffer 不在本项范围。）
- [x] M6.5 根据样例结果决定是否把 indirect command buffer/heaps 纳入首版扩展。（T13 证明单次
  CPU/shared indirect draw；T20–T27 又完成单/多/混合/indexed ICB、reset、pipeline/buffer
  inheritance 与标准 Viewer。heap、GPU 生成与 compute ICB 不纳入首版 P1 门槛。）

阶段验收：compute texture processing 样例可 replay，dispatch 前后资源值正确；不支持的高级能力
有明确诊断且不会破坏同帧其他事件。

## 阶段 7：稳定性、回归与交付

目标：把实验后端整理为可持续开发的 replay 版本。

任务：

- [ ] M7.1 建立一键运行的样例构建、capture 生成、replay smoke test 和结果比对。
- [ ] M7.2 对 capture 打开/关闭、事件切换、资源查看、窗口 resize 做循环稳定性测试。
- [ ] M7.3 检查 Objective-C retain/release、wrapped/live resource 映射和 GPU 等待点。
- [ ] M7.4 在 Debug/Release、当前 macOS/SDK 和至少一个额外受支持 macOS 环境验证。
- [ ] M7.5 整理已知限制、支持矩阵、构建说明和故障排查。
- [ ] M7.6 进行代码审查、格式化和与 RenderDoc 通用 replay 约定的最终核对。

阶段验收：P0/P1 自动测试通过；手工 UI 验收清单通过；文档能指导新环境从源码构建并重现结果。

当前进度：`test_metal_capture_macos.sh` 已覆盖 T00-T13 的构建、原生运行、capture、structured
inspection、CLI replay、output 像素、event seek、shader reflection、texture readback/pick/save；
T01 的最小 pipeline state 也会自动断言 pipeline/shader/topology/vertex buffer/color target；T02
会断言 draw-time vertex descriptor、depth/raster state、16/32-bit index binding、clear/draw1/draw2/
回退图像、index buffer 原始数据、通用 VS input 映射与 Metal mesh preview 输出；T03 会断言 4x4
RGBA8 upload、nearest/clamp sampler、fragment slots、通用 texture/sampler descriptor、四象限 GPU
输出和精确 texel/`PickPixel()`；T04 会断言 512-byte uniform、0/256 dynamic offset、两条 draw 的
event seek、constant-block reflection/descriptor 与左右输出；T05 会断言 24/96-byte 两个 vertex
buffer、stride/step、`instanceCount=3/baseInstance=1` action、per-instance generic VS input、Mesh preview
和三色 GPU 输出；T06 会断言两个 color attachment、全部 action outputs、独立 blend factor/operation/
write mask、240-byte vertex 数据、两张 texture 的 clear/draw/回退像素与最终输出；T07 会断言
`Depth32Float_Stencil8`、front/back stencil descriptor、single/dual dynamic reference、五个 draw action
的 depth output、通用 depth/stencil state、事件图像回退及最终左绿右蓝输出；T08 会断言 4x MSAA
descriptor、显式 resolve/store action、sample/resolve state、三 draw/回退像素与最终红绿蓝输出；T09
会断言 12 个 RGBA8 mip/array/cube 子资源、逐子资源 readback/display、cube pick、DDS 与 Pipeline
fragment binding；T10 会断言 buffer copy/fill、texture copy、generated mips 的 usage/event/seek、
原始字节、最终四条色带与全 mip DDS；T11 已断言 compute dispatch 前后的全部 8×8 RGBA8
texel、回退、标准只读/读写 binding、最终 render 图像和 DDS；T12 会断言 argument encoder 创建与
编码 chunk、buffer/texture/sampler 身份、间接资源 usage、通用 descriptor/reflection、前进/回退、
四象限图像、64-byte texture readback 与 192-byte DDS；最新 qrenderdoc 还验证 EID 2 的 FS
`Buffer 20` / `Texture 17` / `Sampler 18`、三类标准资源跳转、UI/自动 DDS 一致、Pipeline HTML export
和 `No problems detected`。T13 另断言 `Drawcall|Indirect|Instanced`、四个真实参数、精确
16-byte 子范围、`Indirect` usage、clear/draw seek、左右输出和 raw `.bin`；最新 qrenderdoc 的 EID 2、
Buffer 18、Pipeline/Resource 跳转与 UI/自动 `.bin` 一致。M7.1
保持未完成，直到其余 P0/P1 场景进入同一回归入口。

T02 的 Event、Texture 与 Pipeline State 已完成 qrenderdoc 实机验证：EID 2 显示左半屏 viewport/
scissor 和 `Buffer 19 / 72 / UInt16`，EID 3 切换为右半屏和 `Buffer 20 / 144 / UInt32`；两者均显示
Float3/Float4 attributes、stride 28、less/write depth、back cull/CCW、color `Texture 27` 与 depth
`Texture 17`，双立方体图像正确且状态栏无错误。P3.4 进一步验证 VS Input 表格、立方体线框、
interleaved vertex Buffer Viewer，以及 UInt16/UInt32 index Buffer Viewer；Pipeline 资源激活行为已
复用标准 Mesh/Buffer Viewer，但 Pipeline 页面视觉排版仍按 D021 留作后续收敛。

T03 的 Event、Texture 与 Pipeline State 也已完成 qrenderdoc 实机验证：EID 2 显示四象限纹理四边形、
`Triangle Strip`、Float2/Float2 vertex input、fragment Texture 17 和 nearest/clamp Sampler 18；纹理资源
双击进入标准 Texture Viewer，sampler 进入 Resource Inspector，状态栏为 `No problems detected`。

T04 的 Event、Texture、Pipeline State 与 Buffer Viewer 已完成 qrenderdoc 实机验证：EID 2/3 分别显示
左红/右背景和左红/右绿；FS Constant Buffers 分别显示 `Buffer 16 / 0 / 512` 与
`Buffer 16 / 256 / 256`，shader reflection 需要 16 bytes。双击 EID 3 binding 会进入标准 Buffer
Viewer 的 offset 256、length 256 子范围，状态栏为 `No problems detected`。

T05 的 Event、Texture、Pipeline State、Mesh Viewer 与 Buffer Viewer 已完成 qrenderdoc 实机验证：
EID 2 显示红绿蓝三个实例；IA 显示 slot 0 `Buffer 16 / 24 / stride 8 / Vertex / 1` 和 slot 1
`Buffer 17 / 96 / stride 24 / Instance / 1`。Mesh Viewer instance 0/1 按 base instance 读取
`-0.55/0.00` offset 与对应红/绿 colour；两个 buffer 都能进入标准自动格式 Buffer Viewer，状态栏为
`No problems detected`。

T06 的 Texture 与 Pipeline State 已完成 qrenderdoc 实机验证：EID 3 Outputs 列出 FB0/FB1，OM 显示
两张 Color Targets 和两行独立 Blend State；实际 HTML export 与页面一致，状态栏无错误。

T07 的 Event、Texture 与 Pipeline State 已完成 qrenderdoc 实机验证：EID 6 Texture Viewer 显示左绿
右蓝三角形；OM 显示 Texture 20 combined depth/stencil target、`Less / Enabled` depth state，以及
Front/Back reference 5、compare mask `000000FF`、write mask `00000000`、Equal 和
`Inc Sat/Dec Sat` depth-fail operations。阶段流程图经 Home/End 键验证可在 IA/OM 间导航，实际导出的
`captures/metal-smoke/t07_pipeline_state_standard.html` 包含同一状态，状态栏为
`No problems detected`。

T08 的 Event、Texture 与 Pipeline State 已完成 qrenderdoc 实机验证：EID 4 OM 页显示
`4 / Enabled / Disabled` Multisample State、`Texture 17 / Texture 2D MS / 4 samples` Color Target 和
`Texture 24 / Texture 2D / 1 sample` Resolve Target。从 resolve 行进入标准 Texture Viewer 后显示
左红、中绿、右蓝三角形，中心拾取为 `(0.06275, 0.87451, 0.18824, 1.00)`；实际导出的
`captures/metal-smoke/t08_pipeline_state_standard.html` 包含同一状态，状态栏为
`No problems detected`。

T09 的 Event、Texture 与 Pipeline State 已完成 qrenderdoc 实机验证：EID 2 FS 页显示 2D
Texture 17、2D Array Texture 18、Cube Texture 19 和 Point/Clamp Sampler 20。Texture Viewer
的 mip 0/1/2、array slice 0/1/2 与 cube face X+/X-/Z- 切换得到对应固定色；从 Z- face 的标准
保存对话框导出全部 faces，得到 512-byte DDS，状态栏为 `No problems detected`。Qt 5 在 macOS 26
的 combo 弹窗崩溃，两个子资源控件以点击循环和键盘选择保持可用；其余平台不改行为。

T10 的 Event、Resource、Buffer 与 Texture Viewer 已完成 qrenderdoc 实机验证：blit 组含
EID 1-6，Buffer 18 在 EID 2/3 分别显示复制橙色字节和 `0x60` 填充值；Texture 20 为四象限，
Texture 21 的 mip3 拾取为 `(134,132,88,255)`，UI 导出的 468-byte 全 mip DDS 与自动产物
逐字节相同。EID 8 为橙/灰/红/橄榄四条色带，状态栏为 `No problems detected`。

T11 的 Event、CS Pipeline、Resource Inspector 与 Texture Viewer 已完成最新 qrenderdoc 实机
验证：EID 2 为 `dispatchThreadgroups(2x2x1, 4x4x1)`，CS 页显示 Compute Pipeline State 16、
Function 13 `filter_main`、Texture 19 只读与 Texture 20 读写；EID 1→2 的目标纹理由全黑变为
swizzle 图案，首像素为 `(16,32,24,255)`。从 CS 资源行可跳到 Texture Viewer，Resource Inspector
分别显示 `CS - Texture`/`CS - Image/SSBO`；EID 5 的 draw 采样 Texture 20。UI/自动两份
384-byte DDS 逐字节相同，CS Pipeline HTML export 包含 shader 与两个 texture，状态栏为
`No problems detected`。

同一回归入口还会在单进程中依次打开并关闭 T00-T13 各 10 次，检查资源、event、texture readback
和 unsupported 接口。2026-09-21 完整运行的基线后 resident growth 为 491,520 字节；shader debug、
pixel history、histogram、post-VS 和 custom/target shader build 均返回稳定的空结果或明确错误。
完成 T10 后的完整运行覆盖十一份 capture，resident growth 为 1,277,952 字节；T11 全量运行覆盖
十二份 capture，resident growth 为 999,424 字节；T12 全量运行覆盖十三份 capture，增长为
671,744 字节；T13 全量运行覆盖十四份 capture，增长为 1,294,336 字节。这只关闭当前十四份 fixture
的资源生命周期缺口，不代表阶段 7 对全部 P0/P1 场景的稳定性验收已经完成。

## 风险与应对

- Metal capture 输入不足：优先用小型自有测试程序和现有 capture 骨架生成确定性 `.rdc`；不把
  任意应用注入作为前置条件。
- 新 SDK 与 v1.46 兼容性：先固定已验证 Xcode/SDK，针对 SDK 26 的编译差异单独记录补丁。
- Qt 5 在新 macOS 上的兼容性：固定 Qt 5.15 路径；必要时把 UI 基线与 replay library 构建拆开。
- `IReplayDriver` 接口面很大：先使用 RenderDoc 的 dummy driver 组合并显式覆盖必须正确的接口，
  避免一次性复制其他后端的大量无关实现。
- Metal shader 源码并非总能从 `.metallib` 恢复：首版保证 source-created library 的 MSL 展示，
  对 binary library 只展示真实可用信息。
- Apple Silicon unified memory 会掩盖离散 GPU storage 问题：测试矩阵仍区分 shared/private/managed，
  无法在本机验证的模式标记为待外部机器验证。

## 最近批次记录

- 2026-09-24：BATCH21-22 完成 T20 单命令 ICB 与 T21 indexed indirect；联合
  T01/T02/T05/T13/T14/T20/T21 native/capture/XML/Replay API/CLI、异常拒绝、8×10 lifecycle
  及最新 qrenderdoc 同轮 L4 通过。修正 T21 indirect 五字段 Viewer 标签/格式并复验 T13；
  L3 未触发。下一批 BATCH23-24，第一项 P23.1。
- 2026-09-24：BATCH23-24 完成 T22 多命令 ICB 与 T23 indexed ICB；联合
  T01/T02/T13/T14/T20/T21/T22/T23 native/capture/XML/Replay API/CLI、异常拒绝、
  9×10 lifecycle 和最新 qrenderdoc 同轮 L4 通过。L3 未触发。下一批 BATCH25-26，
  第一项 P25.1。
- 2026-09-24：BATCH25-26 完成 T24 ICB reset 后重编码与 T25 混合非索引/indexed ICB；联合
  T01/T02/T13/T14/T20/T21/T22/T23/T24/T25 native/capture/XML/Replay API/CLI、异常拒绝、
  11×10 lifecycle 和最新 qrenderdoc 同轮 L4 通过。L3 未触发。下一批 BATCH27-28，
  第一项 P27.1。
- 2026-09-24：BATCH27-28 完成 T26 ICB pipeline inheritance 与 T27 buffer inheritance；
  联合 T01/T05/T16/T19/T20/T22/T24/T25/T26/T27 自动、异常拒绝、逐份 CLI replay、
  11×10 lifecycle 和最新 qrenderdoc 同轮 L4 通过。L3 未触发。下一批 BATCH29-30，
  第一项 P29.1。

- 2026-09-24：BATCH29-30 的 T28/T29 自动链路、10 类异常拒绝、联合
  T11/T10/T01/T18/T19/T12/T16/T17、11×10 lifecycle 全部通过；L3 未触发。
  此后用户同轮 L4 已确认，T28/T29 与批次关闭；下一批 BATCH31-32 从 P31.1 开始。
