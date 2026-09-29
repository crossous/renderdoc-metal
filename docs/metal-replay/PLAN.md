# Metal Replay 总体计划

> **实施门槛：**所有新的真实应用失败先按
> [跨 API 横向排查顺序](CROSS_API_TRIAGE.md)检查 D3D12/Vulkan 的现成方案、
> UE/Unity 是否专门适配，再只对 Metal 特有约束设计实现；不能直接删守卫。

## 当前优先级：黑盒回放、稳定性、真实普通帧

09-29 [BATCH321](BATCH321_UE58_CAPTURE_VIEW_ORDER_AND_WATCHDOG.md)：
`UE58_capture.rdc` 的 buffer view/父 placement buffer 顺序已修于 capture 代码，
但旧帧推进到帧内 purgeable/GPU 完成时触发 Metal Validation 断言；本机随后
WindowServer watchdog 重启。此机暂停 GPU 回放，当前代码只做执行前安全拒绝。
先在别的 Mac 对完成回调与 purgeable 资源回收做原生、注入、API/CLI 和
必要负例闭环，再考虑当前帧重播；不要宣称该帧已正常开启。

09-29 [BATCH320](BATCH320.md)：`frame1770` 人工 UI 已打开但内容失败。
先用受控 viewport 按钮取得一个含 UE 场景 render pass 的最小帧，核对
scope、attachment、GPU 像素；旧帧的 Metal debug group 已在新 viewer
终端事件树接通。黑色 replay 输出与 Shader Converter GPU VA/资源身份
缺口需完整小夹具闭环，不能因为 API/CLI 打开成功就宣称画面正确。

09-29 [BATCH319](BATCH319.md)：用户 v0x10 `frame1770` 的帧内 Shared placement
buffer 重置已修复，同帧有界 API/CLI 打开通过。下一步由用户人工检查当前
`qrenderdoc.app` 的画面、事件树、资源与异常日志，再取得真正 Empty、
非 Nanite 最小场景与原生 UE 输出对照。默认 bindless GPU VA 重定位仍需
独立验证；当前一帧终端打开成功不能证明所有资源数据或 Nanite 正确。

09-29 当前首阻塞见 [BATCH315–318](BATCH315-318.md)：用户 v0xF
`frame5394` 先命中 typed buffer view，随后命中 BC placement 纹理。
两族小夹具定向终端通过；BC 帧首纹理快照要求新 v0x10 capture，旧 UE 帧
明确拒绝。下一步以新库在 Empty、非 Nanite 场景重截一次，随后只做
一次有日志、超时的 API/CLI 回放。默认 bindless GPU VA 重定位仍是
待实测缺口；不要用旧 v0xF 帧推断它是当前首阻塞。

09-29 新帧重截和有界回放的终端入口见
[M7 记录](UE58_M7_RECAPTURE_GATE_2026-09-29.md)。必须先有 v0xF UE 新帧，
再依据实测首阻塞决定是否实施 Shader Converter bindless 族；静态嫌疑不等于
回放失败归因。

09-29 [BATCH313–314](BATCH313-314.md) 已在 v0xF 新截帧中定向验证
placement buffer 的显式 alias 和 UE 风格释放复用，含原生、注入、
API/CLI、两轮事件回跳及畸形拒绝。旧 v0xE `frame4476` 无释放时序仍
安全拒绝；下一次 UE 测试需用新库取得 Empty、非 Nanite 最小帧，
然后只做一次有日志/超时的 API/CLI 打开，定位下一首个真实阻塞。
默认 bindless 的 GPU VA 重定位未解决，不宣称 UE 普通帧已通过。

09-29 新真实帧 `UE58_frame4476.rdc` 的**第一个**回放阻塞已定位到
placement heap 区间复用；见[首个阻塞证据](UE58_M6_FRAME4476_PLACEMENT_LIFETIME_2026-09-29.md)。
横向对照与新格式的定向实现见 BATCH313–314；纹理交叉复用、帧前
无序释放和复杂 GPU 在途 alias 仍需各自原生/注入验证，不得删除重叠守卫。
既有 [T133](BATCH131-132.md) 原生/注入正例和回放拒绝为最小复现起点。
该帧是 `Lvl_FirstPerson` 编辑器视口，后续仍需 Empty、非 Nanite 最小帧。
旧 `frame833` 的 bindless/GPU 等待问题是另一缺口。

09-29 当前停点见[Private 初始状态与 bindless 审计](UE58_M5_PRIVATE_INITIAL_BINDLESS_2026-09-29.md)：
`frame833` 仍卡在 GPU command buffer `503251`；优先以小夹具验证 UE
Shader Converter bindless descriptor 的 GPU 地址/资源 ID 重定位，再取得
新截帧。**当前项目不要再用 `-BindlessOff` 做截帧入口**：本机 UE 5.8.3
在该参数下的 `METAL_SM6` 全局着色器编译失败，未创建编辑器窗口；见
[启动失败证据](UE58_M6_BINDLESSOFF_STARTUP_2026-09-29.md)。先恢复已能启动的
默认配置，在 Empty、非 Nanite 关卡取得普通帧；bindless 需正面实现和新帧验证。

本机 UE 5.8.3 已有一份真帧定向 CLI/API 回放通过，见
[STATUS](STATUS.md) 顶部。下一步固定非 Nanite 最小场景，核对原生 UE
画面/GPU 结果与 RenderDoc 输出，并由用户人工检查 UI 事件树与资源；
不得把这一份截帧的终端通过外推到 Nanite、全量稳定性或 UI 通过。

按[BLACKBOX_GATE](BLACKBOX_GATE.md)执行：近期不以穷举光追参数或
降低防御宏计数为目标。先保留安全捕获/必要状态/正确GPU结果的
黑盒边界，逐级隔离06:26 IOGPU panic；随后优先处理集中UI QA、M4
对照和UE 5.6.1普通帧的首个真实阻塞。下方旧阶段计划仅供历史追溯，
与本节冲突时以本节和STATUS顶部为准。

09-28 [Binary Archive资源链](BINARY_ARCHIVE_GATE.md)已在T301接通文件
导入、捕获快照、compute/render pipeline依赖及GPU回放。捕获期间修改
archive的普通compute/render `add*PipelineFunctions` 已在
[BATCH305–306](BATCH305-306.md)接通；单个 visible 函数添加
见[BATCH307](BATCH307.md)，单节点 stitched library 添加见
[BATCH308](BATCH308.md)；tile 函数添加在[BATCH309](BATCH309.md)接通，
mesh 函数添加在[BATCH310](BATCH310.md)接通；同步 tile/mesh pipeline
archive 依赖在[BATCH311–312](BATCH311-312.md)接通。异步 archive 绑定、
复杂 graph 与其他函数形态仍拒绝。
未来按真实应用需求逐项实现。
09-28 [BATCH303](BATCH303.md)已接通单函数/单节点 stitched library；
异步入口已在[BATCH304](BATCH304.md)接通；复杂 graph 仍需按依赖图和
时序单独推进，不能原生透传。

## 当前批次入口

实时状态只维护在[STATUS.md](STATUS.md)顶部；indexed triangle refit 见
[BATCH266–271](BATCH266-271.md)，refit 几何 buffer 切换见
[BATCH261–265](BATCH261-265.md)，refitted box AS 复制与再次
refit 见[BATCH259–260](BATCH259-260.md)，bounding-box refit 见
[BATCH256–258](BATCH256-258.md)，格式化 triangle refit 见
[BATCH251–255](BATCH251-255.md)，indexed triangle 顶点格式与步长见
[BATCH248–250](BATCH248-250.md)，非 indexed triangle 顶点格式与步长见
[BATCH244–247](BATCH244-247.md)，禁止重复的 triangle refit 见
[BATCH240–243](BATCH240-243.md)，triangle 禁止重复交点调用见
[BATCH237–239](BATCH237-239.md)，box 禁止重复交点调用见
[BATCH234–236](BATCH234-236.md)，triangle 几何表偏移见
[BATCH231–233](BATCH231-233.md)，indexed triangle 的组合
顶点/索引/scratch 偏移见[BATCH227–230](BATCH227-230.md)，UInt32 indexed triangle
索引偏移与 descriptor 分配见[BATCH226](BATCH226.md)，UInt16 直接分配见
[BATCH225](BATCH225.md)，opaque box 几何见
[BATCH223–224](BATCH223-224.md)，box 几何交点函数表偏移见
[BATCH221–222](BATCH221-222.md)，双盒 AS 射线语义和 compute
交点函数表见[BATCH218–220](BATCH218-220.md)，URL 动态库安全重定位见
[BATCH216–217](BATCH216-217.md)，box AS 非默认 stride 见
[BATCH213–215](BATCH213-215.md)，box AS 偏移见
[BATCH209–212](BATCH209-212.md)，refit 偏移和独立目标见
[BATCH204–208](BATCH204-208.md)，refit descriptor 分配见
[BATCH202–203](BATCH202-203.md)，refit 后压缩 ray 见
[BATCH200–201](BATCH200-201.md)，TLAS 复制及压缩后 ray
见[BATCH196–199](BATCH196-199.md)，indexed/多三角形压缩目标
后续 ray 见[BATCH194–195](BATCH194-195.md)，AS 复制/压缩目标后续 ray 与
活动encoder身份修复见[BATCH192–193](BATCH192-193.md)，其余来源逐类验证
门槛见[BATCH189–191](BATCH189-191.md)，TLAS 实例 descriptor 分配见
[BATCH186–188](BATCH186-188.md)，AS 空绑定清空见
[BATCH183–185](BATCH183-185.md)，有界多 indexed triangle 分配见
[BATCH182](BATCH182.md)，有界多非 indexed 三角形分配见
[BATCH181](BATCH181.md)，有界多 box descriptor 分配见
[BATCH180](BATCH180.md)，双 box descriptor 分配见
[BATCH179](BATCH179.md)，单 box 几何 descriptor 分配见
[BATCH178](BATCH178.md)，顶点/scratch 双偏移组合见
[BATCH176–177](BATCH176-177.md)，scratch buffer 偏移见
[BATCH175](BATCH175.md)，非零顶点偏移的 descriptor 分配见
[BATCH174](BATCH174.md)，静态三角形顶点偏移见
[BATCH173](BATCH173.md)，UInt32 indexed 双侧对照见
[BATCH171–172](BATCH171-172.md)，indexed 非 opaque 反向对照见
[BATCH170](BATCH170.md)，indexed opaque descriptor 分配见
[BATCH169](BATCH169.md)，UInt32 indexed opaque 见
[BATCH168](BATCH168.md)，UInt16 indexed opaque 三角形见
[BATCH167](BATCH167.md)，opaque descriptor 分配见
[BATCH166](BATCH166.md)，显式opaque三角形AS见
[BATCH165](BATCH165.md)，带默认descriptor的AS encoder创建见
[BATCH164](BATCH164.md)，函数表显式residency与encoder
身份修复见[BATCH163](BATCH163.md)，IFT嵌套visible table参数见
[BATCH161–162](BATCH161-162.md)，IFT buffer参数见
[BATCH159–160](BATCH159-160.md)，opaque-triangle表快捷更新见
[BATCH156–157](BATCH156-157.md)，T36 Validation兼容修复见
[BATCH158](BATCH158-T36-VALIDATION.md)，render intersection table六种绑定见
[BATCH148–153](BATCH148-153.md)，vertex/tile阶段TLAS见[BATCH146–147](BATCH146-147.md)，
fragment阶段TLAS见[BATCH145](BATCH145.md)，
不同BLAS双实例见[BATCH144](BATCH144.md)，
同BLAS两实例TLAS见[BATCH143](BATCH143.md)，
单实例TLAS见[BATCH142](BATCH142.md)，
GPU可观察refit见[BATCH141](BATCH141.md)，
AS压缩copy见[BATCH140](BATCH140.md)，
等容量copy见[BATCH139](BATCH139.md)，
描述符创建见[BATCH138](BATCH138.md)，
build几何扩展见[BATCH136–137](BATCH136-137.md)，
基础链见[BATCH135](BATCH135.md)，
查询子集见[BATCH134](BATCH134.md)，
上个完整批次见[BATCH131–132](BATCH131-132.md)，
待人工项见[QA_PENDING.md](QA_PENDING.md)。本计划不再复制每波测试数量和版本，以免状态漂移。

当前机器的显式blit/draw counter sampling边界均不受支持，不能在此机完成对应
旧chunk的GPU闭环；IFT buffer及嵌套visible table已接通，下一族优先评估
其他可验证旧chunk或真实应用中的资源声明剩余子路径，
或选择其余旧chunk。T61 GPU执行点ICB range的单独实施门槛仍见下文，未被忽略。
盒型AS offset/stride 已有捕获与GPU尺寸写回；非默认stride的第二盒几何内容
仍需程序化交点函数配合GPU ray-query验证，不能把1280-byte压缩尺寸当作命中证明。
另见STATUS顶部2026-09-28的IOGPUFamily kernel panic：资源析构安全修补后只有
轻量定向回归通过，暂不自动重跑完整GPU压力/畸形批，不能记全量通过。

## BATCH77之后的依赖顺序

当前剩余bridge不再主要是孤立重载。优先评估以下完整资源链，不把单个原生透传
误计为完成：

1. 函数表与ray tracing：BATCH119–130已完成render fragment、vertex、tile与
   compute的visible function链接、handle、table设置，以及单槽/数组/argument buffer
   绑定的GPU执行链；同步和异步tile descriptor均验证。T134已在Apple M2 Pro上
   原生验证triangle AS build、GPU compacted-size写回（1536/1280），并接通静态
   triangle/box的两个尺寸查询。T135已包装AS资源并接通单个静态无索引三角形build
   及GPU压缩尺寸写回；T136–137扩展零偏移UInt16/UInt32 indexed triangle和单个
   默认stride bounding box build；T138接通单静态三角形descriptor形式AS分配。T141已接
   单无索引三角形原位refit与compute shader AS单槽绑定，GPU ray-query确认命中→未命中；
   T142–143已接同一底层AS的一或两个top-level实例并以compute ray-query确认实例
   变换；T144进一步接两个实例分别引用两个不同BLAS并通过GPU射线区别索引。
   T145接通fragment阶段AS绑定，GPU ray-query驱动绿色像素与Shared输出。
   T146–147进一步接通vertex/tile阶段AS绑定，均由GPU ray-query验证。
   T148–153接通render PSO intersection table创建、intersection handle与setFunction、
   fragment/vertex/tile单槽和range绑定，非不透明BLAS上自定义交点函数使GPU命中
   由1变0。T156–157接通fragment表的单槽及range opaque-triangle快捷更新，
   GPU对照空表未命中0、设置后命中1。表内资源参数、其它opaque签名与curve快捷函数、compute/argument-buffer绑定、
   更多实例/间接实例及refit形态仍缺口。
   T139已接等容量AS copy，T140进一步
   以已完成的GPU尺寸写回为容量依据接通compact copy；其它时序仍显式拒绝，不可据此
   打开`supportsRaytracing`。原依赖顺序为先包装function handle、visible/intersection function table和
   acceleration structure，再接设备/PSO创建及vertex/fragment/tile绑定，最后做可执行
   shader、GPU输出、引用和回退。此机原生设备报告ray tracing支持，但当前包装设备
   暂时报告不支持；必须在资源链可捕获回放后才改变该查询。
2. Mesh/object：先证明硬件与最小原生mesh pipeline可用，再建立pipeline、资源绑定、
   三种draw的完整事件与GPU输出。现有约20个bridge入口不对应可安全单独接通的小块。
   2026-09-27原生`Metal_Mesh`夹具已在Apple M2 Pro及Metal API Validation下通过：
   一个mesh pipeline和直接`drawMeshThreadgroups`产生真实三角形，源文件
   `util/test/demos/metal/metal_mesh.cpp`。**这仅是硬件/原生门槛，不是bridge或chunk已接通**。
   BATCH78已接同步mesh pipeline创建与直接draw；BATCH79已接四种mesh buffer/bytes
   绑定并由真实shader验证；BATCH80已接六种mesh texture/sampler绑定。下一步object绑定、
   更多资源族；BATCH81已接直接`drawMeshThreads`，BATCH82已接异步mesh pipeline，
   BATCH83已接同步object+mesh pipeline与一个object buffer入口，下一步补齐对象阶段
   其它资源重载并以payload控制GPU输出。BATCH84已接三种object buffer/bytes
   重载；BATCH85已接object texture/sampler六重载并由真实shader消费。
   BATCH86已接object threadgroup memory并由真实scratch验证；BATCH87已接异步
   object+mesh pipeline。BATCH88–90已在本机验证并接通间接mesh draw，包括GPU
   写入Private参数的执行时消费与object shader分支；事件树网格尺寸保持未知，
   不把编码时CPU内容当成GPU执行值。BATCH91–92进一步修复两个direct draw
   对object threadgroup非1值的过窄校验。BATCH93–94进一步区分object输入网格
   与每个object组输出的mesh网格上限。BATCH95–97接通rasterization rate map创建、
   参数copy和单层pass使用，及双层descriptor重建；BATCH98证明双层map绑定
   array-target、slice0真实绘制并回拷；BATCH99进一步用mesh primitive层索引
   证明slice1半水平速率下的真实光栅、回拷及API像素；BATCH100把同样非默认
   mesh grid上限的异步pipeline快照路径一并验证。BATCH101接通stage-boundary
   counter sample buffer创建、render pass四阶段索引与blit resolve的完整链。
   BATCH102接通reflection `MTLBufferBinding`创建argument encoder的简单
   只读texture2d/sampler布局；BATCH118接通一层只读texture/sampler子argument
   buffer的encoder创建与GPU采样。更深层、数组/可写成员仍单独评估。
   后续优先完整函数表/光追资源链或heap placement/alias生命周期，见下方独立门槛。
3. Heap placement/alias、GPU生成ICB range：BATCH131–132已接通不复用的
   `makeAliasable` buffer/texture调用；重叠资源复用仍按下方时序门槛单独解决，尤其不能从
   编码时CPU值推断GPU执行点范围。
4. Counter sampling、sparse/resource-state、parallel render和跨GPU remote资源：
   先做本机能力/生命周期夹具，无法可靠验证则保持明确缺口。

2026-09-27本机原生探针：Apple M2 Pro只报告`AtStageBoundary=true`，
`AtDrawBoundary/AtDispatchBoundary/AtTileDispatchBoundary/AtBlitBoundary=false`。
BATCH101已通过普通render pass attachment取得四个真实阶段时间戳，再通过
独立提交的blit `resolveCounters`复制到Shared buffer并由API读回；因此
`resolveCounters`已接通，但不能据此声称Blit encoder自身的
`sampleCountersInBuffer`可用。后者以及render/compute encoder主动采样仍保持
明确缺口，需支持相应边界能力的设备或新的可验证路径。
T103已验证非零sample range与目标buffer offset，不能只依据T101零偏移证明
泛化正确。

BATCH104–105已接通`newDynamicLibrary:`、动态MSL编译选项和一/两个依赖的
实际GPU链接执行。回放把installName重映射到私有临时目录并在编译可执行
library前序列化重建的动态库；源路径不存在也能回放。`newDynamicLibraryWithURL:`
仍是独立缺口：原生验证把序列化文件移动后URL导入保留原嵌入installName，
删除原路径会让pipeline出现undefined symbol。不能把原文件路径硬写回放机
来伪装可移植支持。BATCH106–109进一步接通compute、fragment、vertex
pipeline descriptor的preloadedLibraries（含render options/reflection路径），
v7/v8兼容读取旧capture。BATCH110–111又接通异步dynamic source编译与
异步executable source绑定动态依赖。Binary archive、URL导入及非默认复杂
compile options仍未支持。BATCH112–115接通异步render/compute descriptor
的动态库预加载，包括fragment、vertex和options/reflection overload；
离线只重建结果资源，不重演应用回调。

## 待接通：Heap placement 与 automatic alias 生命周期

BATCH71–72支持 Private/automatic/tracked Heap及未重叠的buffer/texture子资源；
BATCH116–117进一步接通Private/placement/tracked Heap上的显式offset buffer/texture，
按设备size/align和堆边界校验，且拒绝所有重叠区间。BATCH131–132已捕获并回放
`MTLBuffer/Texture::makeAliasable` 的非复用路径；真实重叠复用仍不支持。
当前资源创建由resource record在frame前重建，帧内`makeAliasable`与后续资源创建的
原生顺序可能无法保留；[Apple的makeAliasable定义](https://developer.apple.com/documentation/metal/mtlresource/makealiasable%28%29)
明确旧资源一旦alias后再读取是未定义行为。不能仅凭本批的调用chunk
推断已支持复用。后续需先做最小堆容量的真实别名夹具，证明第二次分配依赖释放的空间，
再调整创建/alias chunk时序并验证前后事件seek、资源查看不可访问状态、负例和回退。
在此之前仅把标记后的无重叠生命周期计作已接通，重叠复用单独保持缺口。

## 待接通：GPU 生成的 ICB indirect 执行范围（不可遗忘）

`MTLRenderCommandEncoder::executeCommandsInBuffer:indirectBuffer:indirectBufferOffset:`
的桥接已在 BATCH70 加入**原生执行 + 捕获记录 + 离线明确拒绝**，替代原先截帧时的
`METAL_NOT_HOOKED` 致命中断；**这不算正确回放已接通**。T70 在同一个 command buffer
先由 compute 写入 range `(0,6)`，CPU 初始哨兵值不是执行范围；Metal Validation
原生执行与捕获通过，离线 replay 在该 chunk 明确失败。旧 chunk 分支虽不再标为
`METAL_CHUNK_NOT_HANDLED`，仍是功能缺口。不能因原始标记计数下降而关闭本项。

实施门槛：先确定在同一 GPU 时间线的执行点取得 range 的可行方案（例如安全的分段
回放/同步读回），再按实际 `location/length` 建立 ICB 子事件和资源引用；验证范围边界、
GPU 写后读取、跨 command buffer/queue 依赖和前后 seek。至少一份 GPU 生成 range 的
真实 capture 要在 Metal Validation 下通过 API/CLI 回放，并证明事件数与输出一致。
当前技术阻塞：编码时 CPU 读到的值可能是哨兵或上一帧值，而执行范围由同一 GPU
时间线后续写入；现有事件树在读 chunk 时按 CPU 序列化数据构建，不能从这份数据
恢复实际子命令集合。普通 command buffer 完成后的 CPU 读回也可能看到更晚的写入，
不能证明执行点值。仍需设计执行点 GPU 快照或可验证的分段回放方案，并保持跨 encoder、
跨 command buffer/queue 顺序；此工作未完成。若方案不可行，保持明确拒绝，
不得用编码时 CPU 快照假装支持。

## 2026-09-26 BATCH53历史检查点

最新BATCH53完成ICB单命令reset与GPU reset/copy/optimize，共1 bridge、3旧chunk；
追加1270 reset、1271初值不可用诊断，Max1272。54 captures/1330类异常/540次lifecycle
通过，剩余148/87；详情BATCH53/PHASE53。CPU初始化Shared render ICB范围明确，
GPU生成/帧前未知初值不声称支持；空命令与epoch初值恢复已有自动证据。
用户现在要求连续终端开发、重置后集中QA；`QA_CONSOLIDATED.md`
合并21份待验capture，T34–T53和T10 marker均不因后续开发自动关闭。下段为历史检查点。

BATCH35–37 的 T34/T35/T36，以及 BATCH38 的 T36 扩展、T37 blit transfer 和 T10
marker 增量已完成终端自动验证。当前 38 captures API/CLI/lifecycle、71 类异常拒绝
通过。按用户要求不运行 qrenderdoc 或 Computer Use；GUI 项保留在 `QA_BATCH35-37.md`
和 `QA_BATCH38.md`，阶段与批次保持开放。bridge/chunk 未接通计数从 216/165 降为
186/128。下一轮仍按资源依赖推进，不以删标记代替 fixture 与 replay 验证。

BATCH31-32 的 T30/T31 与 BATCH33-34 的 T32/T33 均已通过最终联合自动验证
和用户 GUI L4；两批已关闭。T32/T33 的 Event Browser 实际执行数量摘要
也已明确确认。历史证据见 `STATUS.md`；当前待验项为 T34–T37 与 T10 marker 增量。
Metal action 名称的跨 API 审查与后续对齐计划见 `ACTION_NAME_ALIGNMENT.md`。

## 原则

1. 先打通纵向链路，再增加 API 宽度：UI 启动、样例原生运行、截帧、加载事件、replay 一个三角形。
2. Replay 是产品目标，capture 是不可省略的输入前提；先支持受控测试程序，不把任意应用注入列入首版。
3. 每个功能必须有最小样例、`.rdc` 夹具或自动测试，以及 UI/数据验证方法。
4. 每个功能族保留最小验证记录；在转接或中断时更新短检查点，在集中回归后汇总批次证据。
5. 不用空实现伪装支持。未支持能力必须返回明确结果或在 UI 中禁用，而不是崩溃或给出错误数据。

## 当前推进节奏：功能族连续开发、定向验证、集中回归

2026-09-26 用户要求调整节奏。本节取代旧文档中“每个小切片配一份 PHASE、完整收尾、
同步全部索引”的节奏要求；不改变功能正确性和用户 L4 才能关闭阶段的规则。
现有小批脚本保留作重现证据，不再逐个调用其附带的从 T01 到最新 T 的联合回归。

### 一个开发波次

默认连续推进 **2–3 个已定边界的功能族**，不以完成一个 T 或消除几个标记作为停止点。
同族共用实现/校验/测试表，重载和批量接口一起覆盖；每族仍须实现到真实 capture/replay
语义闭环。共享资源模型等较大改动可以独立成族，不为凑数量混入不相关功能。

一个活跃 BATCH 文档内维护多族表格即可：`范围与不支持边界 | T/fixture | 必跑旧 T |
负例/回退断言 | 结果与日志 | UI增量`。低/中风险族不强制另写 PHASE；需要架构设计时才拆。
T 编号继续唯一，可在一个 fixture 内组合多接口/多个事件阶段，不要求一个接口一份 capture。

### 三道自动验证关口

| 时机 | 必须做 | 暂不重复做 |
| --- | --- | --- |
| 每族开发完成 | 增量编译；本族 native/capture/参数检查/Replay API 精确断言；关键非法输入；前后 seek；明确列出的受影响旧 T | 全历史 replay/负例/lifecycle、全套 hash/Qt app 打包、多份状态长文 |
| 切换功能族 | 在后续修改后的代码上重跑受影响的本波次前族；9份代表性快速检查；有创建/释放/回退变化时跑定向 lifecycle | 已通过且不受修改影响的 native 录制与旧测试；未受影响的负例组合 |
| 集中关口 | 最新构建上全量现存 capture API/CLI、已接入负例、Metal验证层与lifecycle；补入本波次新测试；app库同步、正式capture/hash和QA增量整理一次 | 不因到关口就重录所有历史 capture；不运行 Computer Use |

集中关口最迟在 **第3个功能族后、开始第4族前**执行；用户要求集中 QA/交付前也必须执行。
会话切换本身不强制全量，须留下“定向通过、集中回归待做”的明确状态、未跑命令和下一步。
通用资源所有权/初值、chunk格式、同步调度/epoch改动：立刻扩大旧路径与生命周期验证；
若影响无法用明确 T 集合界定，**当前族就触发集中关口**，不能留到后面碰运气。
发现回归先修复并清除相关验证欠项，再继续扩功能；验证失败不能被“已实现”掩盖。

快速旧capture入口（不重新录制，也不构建/同步Qt app）：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh t14 t22 t25
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t54
```

`--sentinel` 为 T01/T02/T09/T11/T12/T35/T49/T52/T53，覆盖基础draw/indexed、子资源、
compute、argument buffer、command创建、CPU更新、Event和ICB回退；按族追加相关旧/新T，
不把9份代表场景当作充分覆盖所有风险。入口会校验文件、去重、增量构建CLI/库、重新编译
API helper、开Metal验证层并逐份CLI replay，失败/超时立即非零退出；只看摘要，失败读日志。
这只是 replay 定向工具，**不代替本族 native/capture、负例或生命周期**。
缺失指定capture时会明确报错，不自动跳过。

现有集中 replay 基线入口为：

```sh
RENDERDOC_METAL_LAST_TEST=61 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

此入口当前覆盖 T01–T61 + T10 marker，并非所有历史脚本/负例或全部重新capture的总和；
新族要扩展清单和精确计数，不能沿用旧LAST_TEST声称覆盖新增功能。只有捕获/注入/初始化路径风险
需要时，追加旧样例重新录制/源码兼容测试；发布/合并前再按完整范围补齐T00及其他历史检查。
旧一键完整录制脚本仍保留，按明确触发原因运行。

### 不可延后的质量底线

- 每个入口有正常语义与边界证据；有写入/别名/同步的必须测数据、资源依赖及回退，不只测能打开。
- 非法资源身份/类型、溢出/越界、关键状态顺序在native调用前拒绝；崩溃/挂起不是正常拒绝。
- 同族负例用表驱动共享生成器，不为每个重载复制一整份脚本；独有语义仍单独断言。
  已有负例不删除，跨族全集集中跑；不靠增加相似负例数量代表覆盖提高。
- 共用native/capture fixture和API helper；避免每个小功能重复搭建构建/日志/导出脚手架。
- 状态区分“实现中 / 定向通过、集中待跑 / 集中通过、UI待验 / 用户已验”，不能混写。

### 记录只维护一个来源

开发中主要维护 `STATUS.md` 顶部短检查点和**一份活跃BATCH增量表**。检查点包含基线、
本波次族数、改动边界、已跑/欠跑测试与日志、首个未完成项；恢复时不重读长历史/重跑基线。
架构决策写在该BATCH一次，其余只链接。`QA_PENDING.md` 保留每个新T的短状态行；
`QA_CONSOLIDATED.md` 只追加UI可见差异，共用准备/资源跳转/状态栏步骤，不复制自动数值测试。
仅集中关口或实质范围变化时更新TEST_MATRIX；README/PLAN/HANDOFF通常只保留稳定入口，
不再每族同步数量、hash和相同摘要。旧PHASE/BATCH证据保留，不追溯重写。

最新自动基线见STATUS顶部及其BATCH链接；全部未验项目保留在QA_PENDING。
以后结果简报给新增能力、计数变化、**本次实际验证层级与欠项**、
QA总单链接；不把暂缓自动回归转嫁给用户。详细点击/EID在集中QA前基于最终capture整理。

### 2026-09-26候选顺序（前三项已由BATCH54–57推进）

当时盘点：148 bridge中render encoder69、device41、其余38；旧未处理chunk87。
计数只是缺口索引，不等于剩余工作量，已有转发的旧chunk也需区分兼容分支与新功能。

1. **现有路径的重载兼容**：indexed instanced draw、旧blit无options chunk等。
   优先复用已实现语义和fixture；先确认capture实际产生何种chunk，不能删除旧分支充数。
2. **Function创建族**：constants/descriptor的同步与异步变体，复用已有library/PSO
   依赖和异步所有权处理；常量值、失败返回、descriptor快照单独测，不纳入intersection功能。
3. **Argument encoder族**：顶层buffer/constants/arrays，复用T12并扩展资源引用与CPU
   修改测试；嵌套encoder是否纳入由依赖审查决定，不先承诺完整支持。
4. **Texture views/资源别名**：三个view重载与buffer-backed texture按共享存储模型推进，
   与heap/aliasable分开评估；这是资源基础工作，允许提前触发广泛回归，不当作低风险转发。

Mesh/object、tile、tessellation、ray tracing、SharedEvent/外部资源等各为独立系统族，
不混进上述快速波次。若已有可从终端稳定捕获的目标应用，以其首个阻塞优先调整顺序；
尚无该证据时以上述可组合能力推进，不声称补齐计数即可跑通UE，也不擅自修改引擎/签名。

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
完成 T28 compute `dispatchThreads` 与 T29 compute buffer binding；其后批次
`BATCH31-32.md` 的 T30 compute sampler 直接绑定与 T31 compute texture/sampler/
buffer 批量绑定已通过自动验证与用户 GUI L4，BATCH31-32 已关闭。
T32/T33 已覆盖 compute 间接 dispatch 的 CPU 参数和 GPU 生成参数；
后续 action 名称一致性工作见 `ACTION_NAME_ALIGNMENT.md`；
compute ICB、heap、blit ICB 管理和多 queue 仍由后续独立场景推进。

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

- [x] M6.1 支持 compute pipeline、dispatch threadgroups/threads 和资源绑定。（T11 完成
  `dispatchThreadgroups` 与直接读写 2D texture；T28 `dispatchThreads`、T29 compute
  buffer、T30 sampler、T31 批量资源绑定的自动验证与用户 L4 均已通过。
  间接 dispatch 另由 T32/T33 覆盖。）
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
