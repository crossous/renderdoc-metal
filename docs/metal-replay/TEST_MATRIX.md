# Metal Replay 测试矩阵

## 优先级

- P0：首条可用 replay 链路必须覆盖。
- P1：首版“常见图形接口”承诺的一部分。
- P2：首版后扩展；不能阻塞 P0/P1 交付。
- Out：当前阶段明确不做。

## 功能覆盖矩阵

| 类别 | P0 | P1 | P2 / Out |
| --- | --- | --- | --- |
| Device/Queue | default device、command queue、command buffer、commit/wait | 多 command buffer、debug label/group | 多 GPU、任意应用注入 |
| Buffer | length/bytes 创建、shared/private、初始内容、vertex/index/uniform | offset 更新、blit copy/fill、多 buffer | sparse/IO buffer |
| Texture | 2D、mip、sampled、render target、RGBA/BGRA/depth | array、cube、compressed、MSAA resolve、views | sparse texture（P2） |
| Render pass | clear/load/store、单 color、depth | MRT、stencil、resolve | tile/imageblock 高级路径（P2） |
| Pipeline | vertex/fragment、vertex descriptor、blend/depth/raster | function constants、多个 attachment | dynamic library/function stitching（P2） |
| Binding | vertex/fragment buffer、texture、sampler | 批量绑定、argument buffer 只读解析 | function table/ray resources（Out） |
| Draw | primitives、indexed | instanced、base vertex/base instance、常见 primitive | indirect/ICB（P2） |
| Compute | 不阻塞第一条三角形链路 | pipeline、buffer/texture binding、dispatch | advanced synchronization（P2） |
| Blit | texture/buffer copy | fill、mipmap、常见同步 | sparse mapping（Out） |
| UI | Event、Texture、Buffer、Pipeline、Shader、Mesh | resource links、markers、错误提示 | shader debug、pixel history（Out） |

## 必备测试场景

每个场景应当有源程序、固定输入、capture 生成说明、预期事件树和验证数据。测试程序优先保持小而
独立，不依赖大型引擎。

每个场景分别跟踪五个状态：Native、Capture、RDC inspect、Replay、UI。禁止仅因为生成了文件就把
场景标记为完成。

| ID | 场景 | 主要覆盖 | 优先级 | 状态 |
| --- | --- | --- | --- | --- |
| T00 | 空帧 + present | 文件加载、device/queue、输出窗口 | P0 | Native/Capture/RDC inspect/CLI Replay/output 像素/lifecycle/UI 重开验证通过 |
| T01 | 彩色三角形 | render pass、pipeline、MSL、非索引 draw | P0 | Native/Capture/RDC inspect/CLI Replay/Texture UI/resize/event seek/capture texture readback/像素拾取/DDS 保存/Shader UI/最小 Pipeline State UI/lifecycle/UI 重开验证通过 |
| T02 | 索引立方体 | vertex/index buffer、depth、Mesh Viewer | P0 | Native/Capture/RDC inspect/CLI Replay/output 像素/index/vertex 数据/lifecycle/Event + Texture + Pipeline + Buffer + Mesh UI 全部通过 |
| T03 | 纹理四边形 | texture upload、sampler、fragment binding | P0 | Native/Capture/RDC inspect/CLI Replay/output 像素/texture data+pick/generic binding/shader reflection/used-unused 过滤/lifecycle/Pipeline+Texture+Resource UI 验证通过 |
| T04 | 动态 uniform | buffer offset/更新、多 draw | P1 | Native/Capture/RDC inspect/CLI Replay/event seek/buffer data/constant-block reflection+descriptor/Pipeline+Buffer UI/lifecycle 验证通过 |
| T05 | 实例化网格 | instance layout、base instance | P1 | Native/Capture/RDC inspect/CLI Replay/action+buffer+VS input+mesh+pixel 自动断言/lifecycle/Pipeline+Mesh+Buffer UI 验证通过 |
| T06 | MRT + blending | 多 attachment、blend state | P1 | Native/Capture/RDC inspect/CLI Replay/双 attachment readback+event seek/action outputs/blend state/lifecycle/Texture+Pipeline OM+export UI 验证通过 |
| T07 | depth/stencil | depth/stencil state 和 attachment | P1 | Native/Capture/RDC inspect/CLI Replay/五 draw event seek/depth output/front-back stencil state/lifecycle/Texture+Pipeline OM+export UI 验证通过 |
| T08 | MSAA resolve | multisample texture、resolve | P1 | Native/Capture/RDC inspect/CLI Replay/三 draw resolve event seek/sample+resolve state/lifecycle/Texture+Pipeline OM+export UI 验证通过 |
| T09 | mip/cube/array | 子资源枚举和查看 | P1 | Native/Capture/RDC inspect/CLI Replay/12 子资源 readback+display/pick/DDS 保存/lifecycle/Pipeline+Texture UI 验证通过 |
| T10 | buffer/texture blit | copy/fill/mipmap | P1 | Native/Capture/RDC inspect/CLI Replay/buffer+texture+mip readback/event seek+usage/DDS 保存/lifecycle/Event+Resource+Buffer+Texture UI 验证通过 |
| T11 | compute texture filter | compute pipeline、dispatch、读写纹理 | P1 | Native/Capture/RDC inspect/CLI Replay/dispatch 前后 readback+event seek+read-write descriptor/DDS 保存/lifecycle/最新 Event+CS Pipeline+Resource+Texture UI 验证通过 |
| T12 | argument buffer | 资源引用解析 | P2 | Native/Capture/RDC inspect/CLI Replay/argument resource+descriptor+reflection/event seek/readback/DDS/lifecycle/最新 Event+FS Pipeline+Buffer+Texture+Resource UI 验证通过 |
| T13 | indirect draw | 单次间接 draw 参数 | P2 | Native/Capture/RDC inspect/CLI Replay/Indirect action+usage/16-byte Buffer Viewer/seek/raw save/14×10 lifecycle/最新 Event+API+IA Pipeline+Resource UI 通过；ICB 留待独立 fixture |
| T14 | indexed instancing | 非零 index offset/base vertex/base instance | P1 | Native/Capture/RDC inspect/CLI Replay/action+usage/clear-draw seek/6-byte index range+save/Mesh instance 0/1/15×10 lifecycle/最新 Event+API+IA Pipeline+Resource+Buffer+Mesh UI 通过 |
| T15 | point/line topology | Point/Line/Line Strip 与 VS Input | P1 | Native/Capture/RDC inspect/CLI Replay/action+topology/vertex offset/seek/Mesh/Buffer/Resource/DDS/16×10 lifecycle/最新 qrenderdoc L4 均通过 |
| T16 | vertex texture/sampler binding | vertex stage texture/sampler 与 VS Pipeline/descriptor | P1 | Native/Capture/RDC inspect/CLI Replay/VS reflection+descriptor/usage/seek/Texture+Mesh+Buffer+Resource/DDS/HTML/17×10 lifecycle/最新 qrenderdoc L4 均通过 |
| T17 | texture/sampler batch binding | vertex/fragment range、空槽与 used/unused | P1 | Native/Capture/XML/Replay、4 份异常 RDC、descriptor/usage/seek、DDS/HTML、18×10 lifecycle 与最新 qrenderdoc L4 通过 |
| T18 | fragment storage buffer | 非零 slot、结构化只读 buffer、PS Resource | P1 | Native/Capture/XML/Replay、descriptor/usage/seek、raw/CSV/HTML、19×10 lifecycle 与最新 qrenderdoc L4 通过 |
| T19 | vertex storage buffer | 非零 slot、只读 buffer、VS Resource | P1 | Native/Capture/XML/Replay、descriptor/usage/seek、raw/CSV/HTML、本场景 lifecycle 与最新 qrenderdoc L4 通过；T18/T16/T02/T05 定向通过，L3 未触发 |
| T20 | indirect command buffer | 单命令 ICB、execute range、资源与 action | P2 | Native/Capture/XML/Replay/readback/action/state、6 类异常 RDC、联合 T01/T02/T05/T13/T14 定向、CLI/8×10 lifecycle 与最新 qrenderdoc Event/API/IA/Mesh/Buffer/Resource/HTML/CSV L4 通过；L3 未触发 |
| T21 | indexed indirect draw | 间接五字段、非零 index/参数 offset、IA/Mesh | P2 | Native/Capture/XML/Replay/readback/action/state、9 类异常 RDC、联合清单/CLI/8×10 lifecycle 与最新 qrenderdoc Event/API/IA/Mesh/Buffer/Resource/五字段 CSV/HTML L4 通过；T13 四字段 UI 复验通过，L3 未触发 |
| T22 | 多命令 ICB | 非零 execute range、多个展开 draw 与逐命令 state | P2 | Native/Capture/XML/Replay API/CLI、三包原始字节、逐事件 seek、11 类异常拒绝、联合 9×10 lifecycle 与最新 qrenderdoc Event/API/IA/Mesh/Buffer/Resource/HTML/CSV L4 通过；L3 未触发 |
| T23 | indexed ICB | ICB indexed command、index offset/base vertex/instance | P2 | Native/Capture/XML/Replay API/CLI、6-byte index raw、两实例 Mesh、18 类异常拒绝、联合 9×10 lifecycle 与最新 qrenderdoc Event/API/IA/Mesh/Buffer/Resource/HTML/CSV/DDS L4 通过；L3 未触发 |
| T24 | ICB reset 后重编码 | `resetWithRange` 失效旧命令、替换与邻居保留 | P2 | Native/Capture/XML/Replay API/CLI、4 包原始字节、逐事件 seek、旧资源无 draw usage、8 类异常拒绝、联合 11×10 lifecycle 与最新 qrenderdoc Event/API/IA/Mesh/Buffer/Resource/HTML/CSV L4 通过；L3 未触发 |
| T25 | 混合命令 ICB | 同一 descriptor/range 的非索引与 indexed draw | P2 | Native/Capture/XML/Replay API/CLI、252-byte 原始资源、逐事件 seek、15 类异常拒绝、联合清单/lifecycle 与最新 qrenderdoc 两类 action、IA/Mesh/Buffer/Resource、HTML/CSV/DDS L4 通过；L3 未触发 |
| T26 | ICB inherit pipeline | execute 时继承 render encoder pipeline | P2 | Native/Capture/XML/Replay API/CLI、24-byte 原始顶点、逐事件 state/seek、异常拒绝、联合 11×10 lifecycle 与同轮最新 qrenderdoc Pipeline 17/18、IA/Mesh/Buffer/Resource、HTML/CSV/DDS L4 通过；L3 未触发 |
| T27 | ICB inherit buffers | execute 时继承 vertex buffer 与动态 offset | P2 | Native/Capture/XML/Replay API/CLI、2×104-byte 原始 packet、逐事件 state/seek、异常拒绝、联合 11×10 lifecycle 与同轮最新 qrenderdoc Buffer 16/17 offset 16、IA/Mesh/Resource、HTML/CSV/DDS L4 通过；L3 未触发 |
| T28 | compute dispatchThreads | thread-level grid、非整除边界与 action | P2 | Native/Capture/XML/Replay API action/state/usage/readback/seek、10×7 局部更新/透明边界、DDS、异常拒绝、联合 CLI/lifecycle 与 GUI L4 通过；右侧缩略图已验 |
| T29 | compute buffer binding | 非零 offset 的 compute buffer 输入/输出 | P2 | Native/Capture/XML/Replay API buffer slot 2/4、offset 32/64、272-byte descriptor、原始字节/像素/seek、异常拒绝、联合 CLI/lifecycle 与 GUI L4 通过；`$action()` 和右侧缩略图已验 |
| T30 | compute sampler 直接绑定 | CS sampler slot、采样过滤/寻址、状态与输出 | P2 | 自动与 GUI L4 通过；公共事件树 EID 4/13 顶层及 `$action()` 最短复验已确认；阶段已关闭 |
| T31 | compute 批量资源绑定 | 非零 range、空槽/覆盖、texture/sampler/buffer 批量入口 | P2 | 自动与 GUI L4 通过；EID 7 分组、空槽、筛选、状态栏已验，GUI 导出入口由用户本轮免复验且自动 DDS/raw 已核对；阶段已关闭 |
| T32 | CPU 参数 compute indirect dispatch | 非零 offset、间接 threadgroup 数量与资源 usage | P2 | Native/Capture/XML/Replay API、5 类异常拒绝、联合 CLI/lifecycle 与 GUI L4 通过；新版实际执行数量摘要已验，阶段已关闭 |
| T33 | GPU 生成参数 compute indirect dispatch | compute 写入参数、跨 encoder 可见性与间接 dispatch | P2 | Native/Capture/XML/Replay API、6 类异常拒绝、写入前后/dispatch 后 seek、联合 CLI/lifecycle 与 GUI L4 通过；新版实际执行数量摘要已验，阶段已关闭 |

## 外部样例候选

外部样例用于补充验证，不直接替代仓库内的最小确定性测试。引入前必须记录固定 commit、许可证、
所需 SDK 和实际使用的 target。

1. Apple 官方 Metal Sample Code：
   `https://developer.apple.com/metal/sample-code/`
   - 优先场景：Using a Render Pipeline to Render Primitives、Creating and Sampling Textures、
     Customizing Render Pass Setup、Processing a Texture in a Compute Function、MSAA、argument buffer。
   - `LearnMetalCPP.zip` 适合验证 metal-cpp、基础绘制、buffer、texture 和 animation。
2. `metal-by-example/learn-metal-cpp-ios`
   - 地址：`https://github.com/metal-by-example/learn-metal-cpp-ios`
   - 初查 commit：`d966e516631f42bac8febdb44d955a545fac4661`
   - 许可证：Apache-2.0。
   - 主要用于 API 调用序列参考；它以 iOS 为目标，需筛选可移植到 macOS 的核心 renderer。
3. `LeeTeng2001/metal-cpp-cmake`
   - 地址：`https://github.com/LeeTeng2001/metal-cpp-cmake`
   - 初查 commit：`e22adb2d0e5fd8d34a96c9c7c6cb3ef6ec943285`
   - 许可证：Apache-2.0。
   - CMake 结构适合快速构建多个 metal-cpp 基础用例。
4. `dehesa/sample-metal`
   - 地址：`https://github.com/dehesa/sample-metal`
   - 初查 commit：`0003824a52516052f2d28503f576907e03425dd3`
   - 许可证：MIT。
   - 覆盖 Swift/macOS/iOS 的更广 Metal 示例，用于第二轮兼容验证。

外部样例应放在独立的 `tests/metal/external` 或工作区外缓存中，不能无说明地复制进 RenderDoc
源码。Apple 下载样例的再分发条款需要在 vendor 前单独确认。

## 验证方式

- 数据验证：已知 buffer 字节模式、已知纹理颜色/梯度、CPU 参考结果。
- 状态验证：测试程序写出 descriptor 摘要，与 qrenderdoc Pipeline State 对比。
- 图像验证：保存参考输出，允许明确的颜色空间/浮点误差阈值。
- 事件验证：对 action 名称、层级、draw 参数和 event ID 做结构化断言。
- 稳定性验证：重复打开 capture、切换事件和资源、关闭 capture，检查崩溃与资源增长。

当前自动回归还会把 Metal replay output 读回为 640x480 RGB PPM：T00 校验中心 clear 色，T01
校验背景色和中心非背景像素，以防 output window 或 texture display 退化为“只是不崩溃”。T01
还在同一 controller 中连续 10 轮依次选择 clear、draw、clear action，断言中心像素
`#14141a -> 非背景 -> #14141a`，覆盖前进与回退的 event-range replay。

T01 同时直接读取 capture backbuffer 的 400x300 BGRA 原始字节，在 clear EID 校验中心
`1a1414ff`，在 draw EID 校验中心发生变化，并通过 `PickPixel()` 校验背景 RGBA 约为
`(0.08, 0.08, 0.10, 1.0)`。最后经 `SaveTexture()` 写出 400x300 ARGB8888 DDS，覆盖 qrenderdoc
纹理保存所依赖的数据路径。qrenderdoc 实机在 EID 2 对 `(202, 128)` 的右键拾取返回
`(0.61176, 0.34118, 0.32157, 1.00)`，并成功从保存对话框写出同规格 DDS。

T01 的 replay smoke 还断言 shader 资源恰好包含 `vs_main`/Vertex 与 `fs_main`/Fragment，reflection
encoding 为 MSL，`captured.metal` 同时包含真实 vertex/fragment 入口。qrenderdoc 实机从 Resource
Inspector 的 Function 13/14 分别进入 Shader Viewer，两者均显示该 MSL，状态栏为
`No problems detected`。

T01 的 Pipeline State 自动回归会分别选择 clear 与 draw EID，确认 Metal capture 类型和 color
target；在 draw EID 进一步断言 render pipeline 为真实 `PipelineState` resource、vertex/fragment
shader 入口及 reflection、`TriangleList`、slot 0 的 96-byte vertex buffer 和 swapbuffer color target。
qrenderdoc 实机在 EID 2 的 Metal Pipeline State 页面显示 `Pipeline State 15`、`Function 13/14`、
`vs_main/fs_main`、`Triangle List`、`Buffer 16`（offset 0、size 96）和 `Texture 23`（mip/slice 0），
状态栏为 `No problems detected`。

T02 自动回归会核对 stride 28 的 interleaved Float3/Float4 vertex descriptor、`Depth32Float` attachment、
less/write depth state、CCW/back-face raster state、两组 viewport/scissor，以及 36 个 UInt16 和 36 个
UInt32 index draw。Replay smoke 逐字节比较 72/144-byte index buffer，确认两个 action 都带
`Drawcall|Indexed`，逐 draw 断言 vertex attributes/layout、index binding、depth/raster 与左右
viewport/scissor，并检查 clear -> UInt16 draw -> UInt32 draw -> UInt16 draw 回退的像素结果。
同一 smoke 会断言通用 `GetVertexInputs()` 返回 `attr0` Float3/offset 0 与 `attr1` Float4/offset 12，
创建 Mesh replay output，实际执行 Float3 VS Input 的 indexed wireframe 绘制并检查非背景像素。
qrenderdoc 实机选择 EID 3 后，API Inspector 显示
`drawIndexedPrimitives(36, MTLIndexTypeUInt32, Buffer 20)`，Texture Viewer 的 Outputs 同时列出
`FB0 Texture 27` 和 `DS Texture 17`，左右两个彩色立方体正确。Pipeline State 在 EID 2/3 分别显示
`Buffer 19 / 72 / UInt16` 与 `Buffer 20 / 144 / UInt32`、左/右 viewport/scissor，并共同显示
Float3/Float4、stride 28、less/write 和 back/CCW；状态栏为 `No problems detected`。
从 vertex attribute 行激活可进入标准 Mesh Viewer：EID 2 的 VS Input 表格按 UInt16 index 展开
`attr0/attr1`，预览显示立方体线框，VS Output 表明确显示 Metal post-VS unsupported。从 Pipeline
State 激活 Buffer 18 会打开 224-byte、stride 28 的自动格式 Buffer Viewer；Buffer 19/20 分别以
`ushort index`/`uint index` 打开 72/144-byte 子范围并显示相同索引序列。

T03 自动回归会核对 4x4 RGBA8 descriptor、64-byte `replaceRegion` 内容、nearest/clamp sampler、
fragment texture/sampler slot 0、Float2 position/Float2 UV 与 `TriangleStrip` draw。Replay output 在
四个象限分别断言 `ff2010`、`10e030`、`1840ff`、`f0d020`，并通过 `GetTextureData()` 与
`PickPixel()` 精确读取同一组 texel；通用 `PipeState::GetReadOnlyResources(Fragment)` 和
`GetSamplers(Fragment)` 同时核对 Texture 17 与 Sampler 18。qrenderdoc 实机在 EID 2 显示正确四象限，
Pipeline 双击纹理进入标准 Texture Viewer，双击 sampler 进入 Resource Inspector，状态栏无错误。

P4.4 的首个 UI 收敛切片复用标准 `PipelineFlowChart`，把 Metal 状态分到 IA/VS/RS/FS/OM 五个阶段页。
T03 实机验证 IA 的 Float2/Float2 与空 index 提示、FS 的 Function 14/Texture 17/Sampler 18、shader
直接跳转和 OM 空 depth 提示；T02 实机验证 IA 的 UInt16 Buffer 19、RS 的左 viewport/scissor 与
Back/CCW、OM 的 Texture 27/Depth Texture 17/Less/Write。`Show Empty Items` 使用标准红色空槽；
`Show Unused Items` 在 shader binding reflection 到位前保持禁用并显示原因。

P4.4 第二个 UI 收敛切片将所有表切换为标准 `RDTreeWidget`/`RDHeaderView`，并将 Metal ResourceId
接入通用资源上下文、usage、thumbnail/preview 分发。T03 EID 2 的 Texture 17 双击仍打开标准
Texture Viewer；标准 Export 控件实际写出 `/tmp/metal-pipeline-t03.html`，内容核对 IA/VS/RS/FS/OM、
Triangle Strip、Texture 17 和 Sampler 18。macOS CUA 的 secondary-click 对 Event Browser 和资源表
均不产生 Qt context-menu 事件，因此右键菜单本轮以公共接线、构建和资源激活回归为证据，不把自动化
限制误记为功能失败。

P4.4 第三个 UI 收敛切片在创建 render pipeline 时请求 Metal argument reflection。T03 自动回归断言
fragment shader 的 `colourTexture`/`colourSampler` 均为 slot 0、直接 Image/Sampler binding、只读且
active；fixture 同时把 Texture 17/Sampler 18 绑定到 shader 未声明的 slot 1，断言通用 descriptor
查询将 slot 0 映射到 reflection index 0，将 slot 1 标记为 `NoShaderBinding + staticallyUnused`，且
`onlyUsed=true` 只返回 slot 0。最终 qrenderdoc 在 T03 EID 2 的 FS 页默认仅显示 slot 0；勾选
`Show Unused Items` 后 texture/sampler 表各增加真实物理 slot 1，状态栏保持 `No problems detected`。

T04 使用一个 512-byte uniform buffer，在 offset 0/256 保存两组固定 float4 颜色；两条 draw 通过
`setFragmentBufferOffset` 与左右 viewport 形成可独立验证的事件。XML 断言初始字节、slot、offset
和两条 draw；output smoke 断言 buffer 原始数据、`uniforms`/slot 0/16-byte active constant-block
reflection、通用 descriptor、EID 2/3 的 0/256 offset，以及 clear -> 左红 -> 左红右绿 -> 左红图像
往返。最终 qrenderdoc 的 FS Constant Buffers 表分别显示 `Buffer 16 / 0 / 512` 与
`Buffer 16 / 256 / 256`；双击 EID 3 行进入标准 Buffer Viewer 的 offset 256、length 256 子范围。

T05 使用独立的 24-byte position buffer 和 96-byte instance offset/colour buffer，vertex descriptor
分别设置 stride 8/per-vertex 与 stride 24/per-instance。第 0 个 instance 是不会被绘制的 sentinel，
`drawPrimitives(..., instanceCount=3, baseInstance=1)` 应只输出红、绿、蓝三个实例。XML 与 replay
smoke 断言两个 buffer 初始字节、slot/stride/step、`Drawcall|Instanced`、instance count/base instance、
通用 `attr0/attr1/attr2` 映射、clear/draw 往返和三处精确像素。qrenderdoc EID 2 的 IA 显示
`Vertex / 1` 与 `Instance / 1`；Mesh Viewer instance 0/1 分别读取 record 1/2，两个 Pipeline buffer
行都能进入标准自动格式 Buffer Viewer。

T06 使用 BGRA8 drawable 和 shared RGBA8 第二 color attachment。slot 0 开启
`SourceAlpha/OneMinusSourceAlpha` RGB blending，alpha 使用 `One/Zero`；slot 1 禁用 blending 并只写
RGB。两条 draw 分别覆盖全屏与中央三角形，240-byte vertex buffer 为 Float2 position、Float4 color0、
Float4 color1。XML 断言两个 format、blend factors 和 `RGBA/RGB_` write mask；replay smoke 断言 clear/
draw action 的两个 outputs、通用 blend state、buffer 字节，以及两张 texture 在 clear、draw 1、draw 2、
回退 draw 1 的精确 RGBA 值。qrenderdoc EID 3 的 Texture Viewer Outputs 可切换 FB0/FB1，OM 页显示
两张 Color Targets 与两行 Blend State；实际 HTML export 包含相同数据，状态栏无错误。

T07 使用一个 `Depth32Float_Stencil8` combined attachment 和 BGRA8 color target。mask/test 两个
depth-stencil state 覆盖 Always/Less、depth write、front/back Equal compare、read/write masks 与
Keep/Replace/IncSat/DecSat operations；fixture 先发出一次双 reference，再以单 reference 建立左右 mask。
五条 draw 依次形成 stencil mask、左绿、右蓝和一次 depth fail。XML 断言 format、operations/masks、
single/dual reference 和 draw 数量；replay smoke 断言五个 action 的 depth output、各事件 state 与图像
往返，最终左右像素为 `10df30/1840ff`。qrenderdoc EID 6 的 Texture Viewer 显示左绿右蓝；标准 OM
Depth/Stencil 表与实际 HTML export 显示 Texture 20、Less/Write Enabled、Front/Back reference 5、
`000000FF/00000000` masks、Equal 与 `Inc Sat/Dec Sat`，状态栏无错误。

T08 使用 4x BGRA8 `Texture2DMultisample` color attachment，并通过
`StoreActionMultisampleResolve` 显式 resolve 到单采样 drawable。三条 draw 形成左红、右蓝与中央绿色
叠加三角形。XML 断言 texture/pipeline sample count、resolveTexture、storeAction 和 216-byte vertex
buffer；replay smoke 断言 clear、三条 draw、rewind 的 resolve 像素、`Texture2DMS` color target、
单采样 resolve target、sample/alpha-to-coverage state 与 action output。qrenderdoc EID 4 的标准 OM
Multisample/Color/Resolve 分组显示 `4x -> 1x` 关系；从 resolve 行进入 Texture Viewer 后中心拾取为
`(0.06275, 0.87451, 0.18824, 1.00)`，实际 HTML export 与页面一致，状态栏无错误。

T09 使用 3-mip RGBA8 2D、3-slice 2D array 和六面 cube，每个子资源为不同固定色；12 条屏幕色带
定位采样结果。XML 断言所有 `replaceRegion` 的 mip/slice、3 个 fragment texture binding 与 sampler；
replay smoke 逐子资源检查原始字节、cube face pick、越界拒绝、标准输出显示和 cube DDS 导出。
qrenderdoc EID 2 FS 页显示 Texture 17/18/19（2D/2D Array/Cube）和 Sampler 20；Texture Viewer
可切换 mip、slice 与 face。macOS 26 上 Qt 5 combo 弹窗崩溃，子资源框以点击循环/方向键方式选择。

T10 使用 64-byte source/destination buffer、8x8 四象限 RGBA8 source/destination texture 与
4-mip RGBA8 texture；XML 检查 blit encoder、buffer offset/length、fill range/value、texture
区域和 mipgen。Replay smoke 断言 `CopySrc/CopyDst/Clear/GenMips` usage、EID 1-6 action 顺序与
资源链接、EID 2→3→2 buffer 数据往返、texture copy 四象限、mip1/mip3 原始字节、最终四条
色带和 468-byte 全 mip DDS。最新 qrenderdoc 的 Event Browser 显示 `Copy/Clear Pass #1` 和
逐操作事件；API Inspector 可跳到 Buffer 18/Texture 20，标准 Buffer Viewer、Texture Viewer
与 UI DDS 保存均和自动结果一致，状态栏为 `No problems detected`。

T11 使用独立的 8×8 RGBA8 source/destination texture，`filter_main` 以 `2×2×1` threadgroups、
`4×4×1` threads/group 将 source 的 B/R/G 分量写入 destination，再由 render draw 采样。destination
清零记录在 frame stream 内，保证 seek 到 dispatch 前/后/回退时读到全零、CPU 参考 swizzle、全零。
XML 断言 compute pipeline/encoder、两个 texture binding、dispatch 参数和 end；replay smoke 断言
`CS_Resource/CS_RWResource` usage、compute pipeline/shader/entry、标准只读/读写 descriptor、全部 256
字节 texel、最终绘制边界像素和 384-byte DDS。T03/T09/T10 定向 output smoke、完整 T00-T11 回归、
12×10 lifecycle 和逐份 CLI replay 已通过。最新 qrenderdoc EID 2 CS 页显示 `filter_main`、
Texture 19 只读、Texture 20 读写；标准 Texture Viewer 首像素拾取为 `(16,32,24,255)`，
Resource Inspector 与 EID 5 draw 绑定一致。UI/自动 DDS 逐字节相同，CS HTML export 完整，
状态栏为 `No problems detected`。

T12 使用 fragment function 创建 buffer slot 0 的 `MTLArgumentEncoder`，将 4×4 RGBA8 texture 写入
`id(0)`、Point/Clamp sampler 写入 `id(1)`，并把 16-byte argument buffer 绑定到 fragment buffer 0。
XML 断言 encoder 创建、target buffer、texture/sampler 写入、`setFragmentBuffer` 与 `useResource`；
replay smoke 断言 `MetalPipe::ArgumentBuffer`、`arguments.colourTexture`/
`arguments.colourSampler` reflection、通用 texture/sampler descriptor、`PS_Constants`/
`PS_Resource` usage、64-byte texel、四象限输出、clear→draw seek 和 192-byte DDS。T03/T04/T11 定向、
完整 T00-T12、13×10 lifecycle（resident growth 671,744 bytes）与逐份 CLI replay 已通过。最新
qrenderdoc 在 EID 2 验证 `Buffer 20`、`Texture 17`、`Sampler 18` 及三类标准资源跳转；四象限输出
正确，UI/自动 192-byte DDS 的 SHA-256 相同，Pipeline HTML export 包含 `fs_main` 与三项绑定，状态栏
为 `No problems detected`。

T13 使用 offset 16 的 16-byte `MTLDrawPrimitivesIndirectArguments`，四字段固定为 `3/2/1/1`；
capture/replay/Buffer Viewer/Raw `.bin` 和左右色块一致。未对齐 offset 17 与越界 offset 36 的
派生 RDC 均在 indirect chunk 明确失败。完整 T00-T13 回归、14×10 lifecycle（resident growth
1,294,336 bytes）和最新 qrenderdoc 的 Event/API/IA Pipeline/Resource/标准 Buffer Viewer 验收通过；
UI raw `.bin` 与自动产物逐字节一致，状态栏为 `No problems detected`。

T14 使用 UInt16 index bytes `{4,4,0,1,2,4}`、byte offset 4、baseVertex 1、baseInstance 1、
instanceCount 2；越界/错位哨兵令左红右蓝输出与 Mesh VS Input 可以判别三个 offset。自动验证
`Indexed|Instanced` action、Buffer 18 的精确 4/6 子范围、Buffer 16/17 的 Vertex Buffer usage 与
Buffer 18 的 Index Buffer usage、6-byte `{0,1,2}` raw save、clear/draw/回退、两实例 Mesh preview。
派生 RDC 的 byte offset 3/10 分别因未对齐/越界被拒绝。完整 T00-T14 回归、15×10 lifecycle
（resident growth 1,015,808 bytes）及最新 qrenderdoc Event/API/IA/Buffer/Resource/Mesh/Texture
验收通过；UI CSV 为 0/1/2，Pipeline HTML 与状态栏均正确。

T15 使用同一 264-byte interleaved Float2/Float4 buffer 中三段非零起点，Point `(1,1)`、
Line `(3,2)`、Line Strip `(6,3)` 分别输出红点、绿水平线、蓝折线；视口外哨兵隔离范围。
自动 smoke 验证 XML/action topology、VS Input、Buffer 数据、Vertex Buffer usage、三个 Mesh preview、
clear→draw→回退和 480128-byte DDS。非法 primitive 99 与零 vertexCount 的派生 capture 均在
`drawPrimitives` chunk 拒绝；T01/T02/T05/T14 定向以及 T00-T15 全量 L3、16×10 lifecycle
（resident growth 737280 bytes）均通过。最新 qrenderdoc L4 已看到三类 Event/API、拓扑、标准
Buffer/Resource、三个 Mesh VS Input、UI DDS/HTML export 和状态栏均已通过。

T16 的 2×2 RGBA8 texture 在 vertex shader `texture(0)`/`sampler(0)` 采样，24 个顶点构成
红、绿、蓝、黄四象限；未注入 BGRA readback、capture XML、原始 texture/vertex bytes、
VS reflection/descriptor、`VS_Resource` usage、clear→draw→回退与 DDS 通过自动检查。
texture/sampler slot 128 与空资源四份派生 capture 均在相应 chunk 拒绝；T03/T12 定向与
T00-T16 全量 L3、17×10 lifecycle（resident growth 114688 bytes）通过。最新 qrenderdoc L4
已核对 EID 2 Event/API、VS Pipeline、标准 Texture/Buffer/Mesh/Resource 跳转、UI DDS/HTML
及 `No problems detected` 状态栏。

T18 使用 640-byte shared buffer，在 byte offset 256 的 fragment `device const float4 *` 读取
四组确定性颜色，物理 slot 3，未绑定 slot 5。自动 smoke 核对 native BGRA、XML、storage
reflection/descriptor、`PS_Resource`、clear/draw/回退、640-byte raw export、哨兵与异常 offset。
T04/T12/T10 定向、T00-T18 全量 L3、逐份 CLI replay 和 19×10 lifecycle（resident growth
1,556,480 bytes）通过。最新 qrenderdoc L4 核对 EID 2 的 FS Storage Buffers 256+384 范围、
标准 Buffer Viewer、Resource Inspector `FS - Resource`、HTML/CSV 保存和状态栏；UI CSV 前四行
与 raw export 数据一致。

生命周期 smoke 会在同一进程中分别打开/关闭 T00-T18 各 10 次，检查 action、swapbuffer、draw、
texture readback，并首次调用 histogram、pixel history、post-VS、四种 shader debug 和 target/custom
shader build 的 unsupported 路径。完成 P4.4 第三个 UI 切片后的 2026-09-22 完整回归在两轮 warm-up 后
resident growth 为 540,672 字节；完成 T10 后十一份 capture 回归增长为 1,277,952 字节；T11 的
十二份 capture 全量回归增长为 999,424 字节。加入 T02
前的额外 50 轮压力检查增长 1,441,792 字节。qrenderdoc 还实机完成
`T01 -> Close -> T00 -> Close -> T01`，重开后 T01 的 EID 2 图像和 Pipeline State 正确；
History/Debug 按钮明确显示不支持且保持禁用。

## 切片与批次执行顺序

```text
每个 T：Native reference -> Capture -> RDC/chunk inspection -> Replay/readback/state 自动断言
批次末：最终构建 -> agent 完成去重的 L1/L2 + CLI/lifecycle -> 用户按 QA_GUIDE.md
在同一轮 qrenderdoc 完成 L4 并反馈 -> 两阶段关闭
```

T19 使用 768-byte shared buffer，在 byte offset 256 保存 24 个 `float4` 位置；vertex shader 从
slot 4 读取，slot 6 绑定同一 buffer 但静态未使用。自动 smoke 核对四象限、XML、reflection、
descriptor、`VS_Resource`、IA/storage 分类、clear/draw/回退、raw 范围和异常 slot/offset。
T19/T18/T16/T02/T05 定向、CLI replay 与本场景 10 轮 lifecycle 通过；最新 qrenderdoc L4 核对
VS Storage Buffers、used/unused、标准 Buffer/Resource、HTML/CSV/raw 与状态栏。未触发 T00-T19 L3；
最近完整基线仍为 T00-T18。

T00-T29 已完成整条链路；T26/T27 的联合自动与同轮 qrenderdoc 证据集中在
`BATCH27-28.md`。T28/T29 的自动链路、最终联合定向及用户同轮 L4
均已通过，两条及批次关闭；见 `BATCH29-30.md`。每条切片立即验证功能自动链路，
最终构建上由 agent 联合去重测试；用户按 `QA_GUIDE.md` 的一次性验收单，在同一轮
qrenderdoc 验收两份 capture 并反馈后一起关闭。

## T28/T29 自动与验收摘要

T28 用 7×5 `dispatchThreads`、4×3 threadgroup 更新 10×7 RGBA8 texture，右三列与下两行
保持零；T29 用 compute buffer slot 2/4、offset 32/64 读写 64 个 uint，前后哨兵保持
`0xa5`。两项的 action/state/usage/descriptor、逐事件 seek、原始字节、最终像素、DDS/raw
输出均自动断言；10 类异常 RDC 被拒绝。联合 T11/T10/T01/T18/T19/T12/T16/T17
通过，11×10 lifecycle resident growth 1,638,400 bytes。随后缩略图修复追加
T01/T03/T09/T11/T12/T16/T17/T22/T25/T28/T29 Replay API 与 CLI，12×10
lifecycle 通过；GUI L4 见 `QA_BATCH29-30.md`，L3 未触发。
