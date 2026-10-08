2026-10-08 UI 交付候选已备齐：[1280×720 UE Lumen/Nanite/VSM 唯一当前卡](UE_UI_CAPTURE_2026-10-08.md)。交付 c827cc7e 捕获在 backend/bundle1845eca8完成三整帧GPU重放与两次EID0；同帧Native像素比较FAIL（18133/23804/15578像素），整体INCOMPLETE，GUI手工验收待用户。独立direct/indirect/wide view/并发Drawable、五attachmentless与真实契约负例、官方Metal2均在同候选实跑通过；完整converted矩阵本次未跑。fresh1845的a877捕获Native成功但背景placement重叠replay拒绝，根因未闭合、不交付为正常文件。旧f1 compute RT完成证据独立，render RT不开启；文件、命令与manifest全外盘。

2026-10-08：对应 compute RT 开启验收已完成。当前 backend/bundle `f1ba1862`，`supportsRaytracing` 跟随 Native 设备，公开路径无需 probe；`supportsRaytracingFromRender` 保持false。开启后官方Metal2、真实UE/Lumen整帧三次EID0与同帧Native零差、query事件/绑定、独立合法变体、相关生命周期/frame及完整转换矩阵均在同候选实跑通过。GPU COMPLETED / 输出 MATCH（UE EXACT_MATCH）/ overall PASS仅对应记录的compute/API/预算及本机设备范围；Metal3扩展、render-stage、物理不支持设备实跑及原Qt GUI崩溃未认证。旧1dc FAIL不改写，旧hash不继承。[最终唯一记录](HEAP_BACKING_RECOVERY_2026-10-08.md#compute-rt-能力开启与最终验收完成f1ba18622026-10-08)。持续任务已按达标约定停止（metal automation已删除），不继续重跑或自动扩展验收范围。

2026-10-08 当前c88d10d4：按需控制输入/真实CPU恢复分离、独立Native、第二份UE无损整帧/reset/query绑定及scoped T2通过；M4生产捕获流程与固定最终验收仍开放，两个生产RT flagsfalse。[当前唯一证据](HEAP_BACKING_RECOVERY_2026-10-08.md#按需控制输入与跨捕获恢复候选-c88d10d4)，外盘scalar-interval-evidence.json；历史结果只属于其hash。

2026-10-08 当前c54b5fb4：通用间接恢复/实际GPU预算已整改，独立Native/T2通过，真实UE整帧原JPEG逐字节一致、两次EID0 raw输出一致及5个query事件绑定/往返通过。M3跨捕获/无损基准与M4最终启用/生产路径未完，两flagsfalse；[当前唯一证据](HEAP_BACKING_RECOVERY_2026-10-08.md#原生间接调度恢复与执行参数分离候选-c54b5fb4)。旧hash结果独立。

2026-10-08 候选 f3e2d595：来源/普通GPU数据通用整改及定向/相关集成通过；UE整帧GPU命令完成但单一间接数值差异，输出验收未完成。[唯一记录](HEAP_BACKING_RECOVERY_2026-10-08.md#指针条件来源与普通-gpu-贡献数据恢复候选-f3e2d595)；旧hash证据不算本库通过，两RT flagsfalse。

2026-10-08 Native 索引实际输入恢复与可选CPU投影已分离，候选 d152c324 的关键/独立合法组合和 scoped T2通过，UE仍在后续指针來源阻塞；[唯一记录](HEAP_BACKING_RECOVERY_2026-10-08.md#native-索引输入恢复与可选数值投影候选-d152c324)。不继承旧hash结果。

2026-10-08 当前M1候选 `340e330b`：Native深度/比较sampler与实际绑定恢复、完整独立Native/相关验证及UE索引分析阻塞见 [唯一批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#native-深度采样与实际绑定恢复候选-340e330b)。M2整帧GPU未完成，两RT flagsfalse，旧hash范围独立。

2026-10-08 当前M1候选 `7b46ae65`：同module实际函数/helper参数恢复、真实Native与UE未完成阶段见 [唯一批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#同模块函数身份与原生控制效果候选-7b46ae65)。两RT flagsfalse；旧hash范围独立。

2026-10-08 当前M1候选 `29c8aa53`：入口属性/typed table恢复、真实Native与UE未完成阶段见 [唯一批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#入口属性与-typed-table-实际绑定恢复候选-29c8aa53)。两RT flagsfalse；历史记录只属于其hash。

2026-10-08 当前M1候选 `9d987f9f`：shader自有数值常量/外部来源分离、独立Native及UE实际未完成阶段见 [唯一批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#shader-自带数值常量与外部缓冲来源分离当前候选-9d987f9f)。两RT flagsfalse，旧记录仅属于其hash。

2026-10-08 当前M1整数纹理ABI/规则格式恢复候选 `d22c3c1d`：实际机制、独立Native完整输出/事件、相关回归及UE尚未完成范围见 [唯一批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#整数纹理-abi-与规则格式字节恢复当前候选-d22c3c1d)。两RT flagsfalse，旧记录仅对应其hash。

2026-10-08 当前候选`6b35e12e`的原纹理同步、子资源生产链、真实Native与UE阶段见 [当前M1批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#原生纹理同步与投影子资源生产链当前候选-6b35e12e)。RT未开启，历史hash不计当前验收。

2026-10-08 当前执行策略采用 [复盘](EXECUTION_REVIEW_2026-10-08.md)；当前M1结果只见 [heap backing闭环记录](HEAP_BACKING_RECOVERY_2026-10-08.md)。冲突的历史场景准入、CPU表达式/producer证明义务和旧负例oracle已替代；历史测试不算当前候选验收，最终RT开启门槛保持。

2026-10-07 [B544 Native GPU定义区间与通用dispatch绑定](GENERIC_API_RECOVERY.md)：固定backend/bundle `af26d32b`。真实在前Native GPU store的定义区间与CPU数值分开；未知clz值仍Unknown，精确非零compute写按已恢复地址/范围/提交发布4byte区间、统一预算，不造padding/CPU值。conditional/ranged/atomic/raster/未知pointer不借此保证初始化。direct/indirect sourced dispatch以实际闭包和PSO反射绑定校验取代slot0/2D-copy形状限制。两fresh真query变体cross/slot2/offset64/1KiB→same/slot7/offset65532/64KiB、实例2/各56事件/真实GPUword完整输出通过，未来producer与缺producer仍GPU前拒绝；fresh旧恢复相关路径、converted RayQuery及29旧capture58API/CLI通过，128坏组256拒绝。UE禁GPU预检仍API4在非零候选14714内容/producer未证明处，peak1364377600bytes、无wait/strict；没证据实际访问该候选，不能伪造producer。下一动态候选/physical backing/定义区间恢复义务边界，非扩CPUshader数值。完整UE及最终集中门槛未验、两flagsfalse、目标active；新产物外盘，失败/当前产品源码文档哈希保留，不提交推送重置。

2026-10-07 [B544 零字段/稀疏 namespace 与统一复制预算整改](GENERIC_API_RECOVERY.md)：backend/bundle `c52ba022`。物理buffer零字段按真实initial/CPU publication/匹配full24 GPU copy保存消费点证明，retired/纹理/未发布零行不虚构buffer依赖；opaque/alias writer与CPU提交更新使旧事实失效，非零缺来源/初态/producer仍拒绝。GPU全零descriptor的空引用图不再误判无producer。ordinary/typed/线性copy场景计数改已有整帧预算，保留单次1MiB真实恢复边界、不扩大上限。独立Native的3/17/257稀疏行、R32Uint2×3→RGBA8Unorm7×5、动态实例/偏移及GPU背景+帧重发布通过；fresh部分Private/可变/texel、converted RayQuery与29旧capture58API/CLI通过，111坏组222GPU前拒绝，缺冗余raw initial但完整typed publication合法控制通过；未声明地址writer仍恢复拒绝。UE无GPU预检越过退休零字段，下一非零placement候选14714缺内容/producer证明，仍API4，peak1357774848bytes，无wait/strict，不能假装候选被实际访问。下一physical backing/定义区间与动态候选恢复资格，非CPU shader数值扩许。完整UE/最终官方full与能力验收未跑，两flagsfalse、目标active；新产物全外盘、失败保留、无提交/推送/重置。

2026-10-07 [B544 动态 typed namespace 与部分初态通用整改](GENERIC_API_RECOVERY.md)：固定 backend/bundle `5673f351`。真实 GPU query/metadata 动态选择 24-byte typed table 的缓冲字段时，保留已恢复 namespace/ABI 字段而非要求 CPU 求出行号；按消费点冻结的实际槽位建立恢复候选和提交依赖，候选不冒充实际访问，展示 partial。普通帧出生 Private/Tracked 缓冲的地址重定位不再依赖 CBV 分类；真实部分上传区间独立于 CPU 数值，动态绑定不要求整个 allocation 初始化，缺来源/初态/生产者仍拒绝。两 fresh 部分缓冲 query 变体（实例2选第3缓冲/4-byte 上传、1KiB→64KiB、offset64→65532、Header/贡献/输出偏移）各56事件/EID0/完整输出及真实定义字校验通过；fresh GPU初态+帧重发布、读改写/local atomic/部分texel、converted RayQuery与29旧capture58 API/CLI通过。91坏组182 API/CLI均GPU前拒绝。UE禁GPU预检消除56未知缓冲并越过64KiB/4-byte恢复，下一table25/slot2760已退休纹理的null缓冲字段缺当前namespace证明，仍API4，peak1207173120bytes，无wait/strict；非UE整帧PASS。下一补零字段/退休槽与GPU发布的通用证明，非零失效来源不得跳过；不扩CPU shader表达式许可。官方/full/UE整帧最终验收未跑，两flagsfalse，目标active，失败和外盘检查点保留，无提交推送/重置。

2026-10-07 [B544 私有指针与 Native query API 通用整改](GENERIC_API_RECOVERY.md)：固定 backend/bundle `ec370d18`。非逃逸 private pointer slot按CFG所有在前writer保留typed对象/offset，分支/循环与LLVM生命周期独立数值；未初始化、未来writer、未知来源/外部逃逸仍拒绝。三角形实例query按Native返回ABI分类整个数值/向量/矩阵getter族，GPU计算、展示partial，设备pointer-return不借此放行。两fresh Native真query（1→3实例、Header0→64/贡献0→16/输出0→32、两private来源分支）完整输出/48事件/EID0/AS+UAV公共查询通过，缺AS/child/贡献初态仍GPU前拒绝；fresh读改写/local atomic/部分texel/converted RayQuery与29旧capture58API/CLI通过。UE无GPU预检越过3246400/4065；1591744/6399 AS2/unknownAS0/unknownCall0，剩unknownBuffer56，commit4981760仍API4，peak1523056640bytes。不是UE整帧PASS，flagsfalse、目标active；下一补真实动态descriptor/缓冲地址namespace闭包，不扩CPU shader表达式许可。新产物外盘，保留中间FAIL、源码/产品/文档哈希，无提交推送/重置。

2026-10-07 [B544 通用 Native query/AS 恢复闭包](GENERIC_API_RECOVERY.md)：backend/bundle `25632fabc7d45be667f48871f5ca14c4572ce45d728470d096bdeb706916b4c8`。移除 runtime 一律 queryReset 拒绝；typed kind3 槽位→Header→Native AS/贡献缓冲双字段来源、dispatch 时冻结构建图和实际提交依赖独立验证，GPU 负责交点与动态索引，展示 partial。私有 query 调用及私有目标存储不误当捕获 device buffer。移除 default direct TLAS 对不同 BLAS 提交的历史要求，保留实际在前构建/冻结子配方与设备检查。同提交 1/3 实例、Header0/64、贡献0/16、动态输出0/32的两 Native 光追 sample native/capture/API/CLI、96事件/EID0、40公共查询点/80 descriptor-location检查通过；共24坏组48无GPUwait拒绝。可变输入/local atomics、部分texel、fresh旧RayQuery及29旧capture58 API/CLI通过。UE禁initial/frameGPU预检仍API4：实际3246400/4065的AS未知、10调用及私有store误报消失，剩unknownBuffer1；peak1284096000bytes，无wait/strict。下一先用独立 Native 私有pointer spill复现，补来源传递而非shader数值模拟。UE整帧与光追开启集中门槛未完成，两生产flagsfalse。

2026-10-07 [B544 可变数值事实与 Native 本地内存通用整改](GENERIC_API_RECOVERY.md)：backend/bundle `7bb1d560`。同dispatch实际写入使重叠scalar事实失效并重新分析，保留typed对象/地址来源与真实初态/producer；已恢复的合法读改写由原shader执行，展示partial。精确范围保留共享backing的分离root/texel，未知写范围保守覆盖allocation；Native addrspace(3)原子不误当捕获device buffer，不模拟结果。两fresh读改写+local atomic偏移变体、部分texel初态/动态GPU descriptor、fresh RayQuery与29旧capture58 API/CLI通过；缺RMW初态/缺来源/原始VA/只读等坏capture仍GPU前拒绝。UE无GPU预检越过1584704/3993和1596480/4007，约28秒到实际intersection-query入口3246400/4065：AS1/unknownAS1/unknownBuffer2/unknownCall10，commit4981696仍API4，peak1406369792bytes、无wait/strict。不是UE重放PASS，两flagsfalse，继续通用AS/Header来源及RT依赖闭包。

2026-10-07 最新任务整改（覆盖历史场景许可策略）：先完成一组通用资源恢复与重定位机制，再继续 UE/Lumen 新案例。实际修改纹理/frame-view/coverage/runtime AIR preflight，区分 API/设备、恢复缺口、统一预算、历史场景限制与展示分析；coverage 不积累场景等级，Native 动态计算与 partial 展示独立。具体执行规则、逐函数 DX12/Vulkan 对照及剩余边界见 [GENERIC_API_RECOVERY.md](GENERIC_API_RECOVERY.md) 与根 AGENTS.md；光追开启门槛不变。持续任务 metal 已同步。

2026-10-06 [PHASE59](PHASE59.md) / [B520](BATCH520_UE_BOUNDED_HEAPS.md)：使coverage65描述表预检单heap范围与现有原生重放576MiB一致，新增完整heap字段预检，保留整个backing收费和总GPU/CPU预算。backend/bundleda2234f1、GUI3ba30e36；129/192MiB四份native→capture→API/CLI、48选择通过，31坏组62拒绝（含六heap超总预算、整数溢出和出生/descriptor错误）；旧46纹理/view API/CLI、旧view四份native capture replay、IR64、官方两scene/270事件/10查询/六能力、CPU预算组件通过。原UE70heap共5.267GB，CPU估计native5.322GB/initial4.362GB，仍超总预算拒绝，无UE GPU运行；完整78IR/75RT/308/旧84反例/Qt/ARC未跑、两能力false。继续小规模converted inline-query AS header/root闭包及低成本UE截帧设置，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B519](BATCH519_UE_FRAME_MIP_VIEWS.md)：补coverage65 R16Float帧内单mip view投影、父资源usage与物理mip一致性。backend/bundle3d2452ec、GUI3ba30e36；5shape十份native→capture→API/CLI、120事件/EID0、view全部rawbits/PickPixel与parent对应通过；29坏组58拒绝，旧36纹理capture API/CLI、旧RGBA8/R16 view四份native/capture/API/CLI、IR64、官方两scene/270事件/10查询/六能力通过。原UE只CPU65越过frame views，下一静态分配guard拒绝（native约4.92GB、initial约4.36GB），未启动UE GPU、未放宽总预算；完整78IR/75RT/308/旧84反例/Qt/ARC未跑、两能力false。下一核实真实heap/初态预算并补typed inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B518](BATCH518_UE_HALF_ARRAY_EXTENT.md)：补实际RGBA16Float二维数组RW尺寸64→512（仅coverage65，≤8层），保留旧RT/旧coverage尺寸。backend/bundle5f1a2f18、GUI3ba30e36；4新shape八份native→capture→API/CLI及旧coverage64+RT7两份、120事件/EID0、每层half bits/PickPixel/usage/GPU ID通过；19坏组38拒绝，旧R16/packed二维/R8各六份/R11八份API/CLI、IR64、官方两scene/270事件/10查询/六能力通过。真实UE仅CPU65越过所有此前texture创建拒绝，下一frame texture view拒绝；原XML有11841的0–7 mip单层views，具体失败分支待精确诊断。原capture不变，未跑UE GPU/输出、78IR/75RT/308/旧84间接反例/Qt/ARC；两能力false，继续mip view语义和typed inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B517](BATCH517_UE_PACKED_FLOAT_ARRAY.md)：补实际Lumen RG11B10Float二维数组RW（coverage65、≤512、1–8层、Tracked/单mip），每层独立red及11/11/10原始位值/PickPixel验证。backend/bundle3d8bf082、GUI3ba30e36；4场景八份native→capture→API/CLI、96事件/EID0/usage/GPU ID，19坏组38拒绝通过；旧R16/packed二维/R8数组各六份、旧RG11二维两份native capture replay、IR64、官方两scene/270事件/10查询/六能力通过。原UE CPU65越过12052–57，下一12058 RGBA16Float二维数组320x240 array1 usage3仍拒绝；未修改原capture、未重跑UE GPU。未跑78IR/75RT/308/旧84间接反例/Qt/ARC和UE输出，两能力false；继续真实half数组及inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B516](BATCH516_UE_R8_ARRAY_TEXTURE.md)：补实际Lumen R8Unorm二维数组RW placement（coverage65、≤512、1–8层、单mip/Tracked），每层不同值核验。backend/bundlee6ecb223、GUI3ba30e36；320x240×1/512x512×8/3x5×3六份native→capture→API/CLI、72事件/EID0、全部层字节/PickPixel/读写usage/不同GPU ID通过；19坏组38拒绝，旧R16六份/packed二维六份API/CLI、旧单层R8和float数组四份native capture replay、IR64、官方两scene/270事件/10查询/六能力通过。UE仅CPU65越过12051，下一12052 RG11B10Float二维数组320x240 array1 usage3拒绝，原capture不变。未跑78IR/75RT/308/旧84反例/Qt/ARC和UE输出；两能力false，继续实际数组格式与inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B515](BATCH515_UE_PACKED_2D_TEXTURE.md)：补实际Lumen RGB10A2Unorm二维RW placement（coverage65、≤512、单mip、tracked）；旧3D/旧coverage保持。backend/bundled8f74eec、GUI3ba30e36；320x240/512x512/3x5六份native→capture→API/CLI、72事件/EID0、packed全部bits/PickPixel/正确读写usage/不同GPU ID通过；18坏组36拒绝、旧R16六份/旧packed3D两份native capture replay、IR64、官方两scene/270事件/10查询/六能力通过。实际UE仅CPU65，越过12050，下一12051 R8Unorm二维数组320x240 array1 usage3仍拒绝，未修改原capture、未重跑UE GPU。未跑78IR/75RT/308/Qt/ARC及旧84间接反例，B514结果仅旧库；两能力false，继续数组形态与inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B514](BATCH514_UE_FRAME_R16_MIPS.md)：补实际UE的Private/Tracked placement R16Float RW二维多mip（≤512、完整mip预算）；修默认compute encoder误读dispatchType越界，AIR补typed ulong→pointer/texture句柄与air.buffer入口。backend/bundlecc91bb09、GUI3ba30e36；8/9/10mip六份及旧array/volume/float32/二维八份native→capture→replay通过，最终14份核验两写/读事件usage、逐像素/PickPixel/168选择-EID0及不同GPU ID；18坏mip组36拒绝、84坏间接证据168拒绝/2执行错配4拒绝/2控制、旧16间接112seek、IR multi64、官方2scene/270事件/10查询/6能力、32CPU+AIR通过。失败和oracle纠正保留；原UE CPU65越过11841，下一12050 RGB10A2二维320x240 usage3仍拒绝。UE输出/完整重放、78IR/75RT/308/Qt/ARC未验，冻结根因未修、两能力false；继续下一实际packed二维资源与typed inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B513](BATCH513_UE_LUMEN_QUERY_AND_STREAM_SCAN.md)：B512验收后已回UE5.8.3有限Lumen HWRT截帧，原库ad0a保存新帧并owned cleanup（editor -9，不计自行正常关闭），6间接TLAS/643compute，280native编译完成。原metallib 58个Lumen模块全部CPU反汇编，PSO/function/library+原per-use组确认2shader/4非零硬件ray query调度；输出及完整重放未验。修ScanDescriptorMetadata对前向压缩流的两处非法回退，保持typed证据/guard；修旧三反例生成器零chunk预算，保留失败及两拒绝fixture的oracle纠正。最终backend/bundle77a7fa99、GUI3ba30e36；8compute+8render/112seek-reset、84组坏证据API+CLI168拒绝/2执行点错配4拒绝/2拒绝控制、64geometry IR定向、官方2scene/270事件与6能力通过，32CPU tests通过。原UE正常打开仍拒绝未声明资源，CPU65进一步拒绝R16Float256x128/mip8；77未重新UE截帧、完整78IR/75RT/308/原Qt/ARC/UE图像事件未验，不能继承ad0a全量。报告库存变化0仅stat，不证明旧冻结根因。两能力false、持续active、未提交推送；下一先实际UE多mip纹理与typed inline-query资源包/AS header producer闭包，再实际输出/事件/EID0。

2026-10-06 [PHASE59](PHASE59.md) / [B512](BATCH512_IR_FRAME_MULTI_GEOMETRY.md)：参照DX12/Vulkan多几何BLAS，补Private整buffer encoder-end冻结、新append chunk/current recipe/IR预检，保留几何顺序、Float3/4和UInt16/32原offset/stride及IFT槽；仅完整Private/Tracked/standalone usageNone使用新路，其他接口保持。修GeometryIndex所需SM6.6与64几何补写chunk参数预算，强化逐步日志检查，中间失败保留。最终backend/bundlead0a0248、GUI3ba30e36；串行78 native/capture/API8268事件（新multi6/1446）、2088坏IR/135合法控制（multi579/30）、1坏列表/1往返、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13反例/6能力通过。40正常重开growth1785856B，另30失败+10成功/10CPU导出/30seek-reset growth638976B；6504最终日志无断言/overrun/资源表/旧关闭诊断，CPU互斥/语法/diff通过、起止hash一致。2及64几何最后命中GeometryIndex贡献，逆序变成0，未引用NaN允许、引用NaN拒绝；新目标前后事件usage/EID0独立。UE/75RT/308/原Qt/ARC/独立旧table未跑、系统冻结根因未修、能力false、持续active、未提交/推送。下一按用户最新要求先有限UE Lumen HWRT新截帧/实际调度，按真实缺口补producer/接口；refit/copy/异buffer仍未支持。

2026-10-06 [PHASE59](PHASE59.md) / [B511](BATCH511_IR_FRAME_TRIANGLE_GEOMETRY.md)：参照DX12/Vulkan动态BLAS，补Private顶点/索引encoder-end冻结、新append chunk/current recipe/IR预检/新AS目标及staging生命周期；完整已验证输入与usageNone才走新路，其他入口保留。修44有效bytes需48最后stride填充，并修新目标事件usage oracle及SDObject访问器编译错误，失败完整保留。最终backend/bundled15e1e52、GUI3ba30e36；串行72 native/capture/API6822事件（新geometry6/1401）、1509坏IR/105合法控制（geometry471/24）、1坏列表/1往返、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13反例/6能力通过。40正常重开growth1130496B，另30失败+10成功/10CPU导出/30seek-reset growth344064B；4387最终日志无断言/overrun/资源表/旧关闭诊断，CPU gate互斥/语法/diff通过、起止hash一致。新目标无build明确拒绝，前后BLAS读usage互不混淆。UE/75RT/308/原Qt/ARC/独立旧table未跑、旧冻结根因未修、能力false、持续active、未提交/推送。下一IR多几何/refit/copy及可靠UE producer布局接入。

2026-10-06 [PHASE59](PHASE59.md) / [B510](BATCH510_INDIRECT_AS_SCRATCH_OFFSETS.md)：参照DX12 Buffer+Offset/VK alignment及Metal三角形路径，补间接TLAS非零scratchOffset，append新chunk/冻结补写/core/IR预检、256对齐与length-offset校验，旧零offset格式保持；8新场景GPU保护区及事件/EID0验证。修条件序列化宏缺完整块误读旧padding，失败完整保留。最终backend/bundlee11c809e、GUI3ba30e36；串行66 native/capture/API5421事件（offset8/1086）、1038坏IR/81合法控制（offset frame176/16）、1坏列表/1往返、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13反例/6能力通过。40正常重开growth1064960B，另30失败+10成功/10CPU导出/30seek-reset growth393216B；3283最终日志无断言/overrun/资源表/旧关闭诊断，CPU gate互斥/语法/diff通过，起止hash一致。用户ForceCrash具体offset未证实；UE/75RT/308/原Qt/ARC/独立旧table未跑、系统冻结根因未修、能力false、持续active、未提交/推送。下一帧内AS目标/几何及可靠UE producer接入。

2026-10-06 [PHASE59](PHASE59.md) / [B509](BATCH509_IR_FRAME_INDIRECT_BUILDS.md)：补IR帧内Private间接TLAS当前配方、typed冻结子BLAS/UserID、提交/encoder顺序和typed blit不可变输入保护；两次TraceRay旧/新输出及EID0独立恢复，覆盖初始空/屏蔽、heap/local/global。修帧AS补写文件流metadata flags断言，修旧合法header控制长度及实际commit位置；失败完整保留。最终backend/bundle d8d0b308、GUI3ba30e36；串行58native/capture/API4335事件（新7/1188）、862坏IR/65控制（新156/12）、1坏列表/1往返、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13反例/6能力通过。40正常重开growth3588096B，另30失败+10成功/10CPU导出/30seek-reset growth737280B；2770最终日志无断言/overrun/资源表/旧关闭诊断，CPU新gate互斥/语法/diff通过，起止hash一致。初始列表外层错误分类差异未修改。下一先补UE非零scratchOffset（同一bridge入口仍拒绝，仅源码缺口、未证明用户崩溃触发条件），再补目标/几何/producer；UE/75RT/308/原Qt/ARC/独立旧table未跑、冻结根因未修、能力false，目标active、未提交/推送。

# PHASE59：转换 DXR 的 IR 参数与 shader record 重放

接续 PHASE58 的 UE 适配，先用已在本机验证的 DXC→Apple IR 转换样例补完整参数链，再扩大到 UE 实际需要的布局。本阶段遵循 DX12 `D3D12RTManager::PatchRayDispatch` 的 SBT/根参数依赖和 VK `Serialise_vkCmdTraceRaysKHR` 的区域语义。AS、函数表和普通资源地址分别关联，不扫描任意整数猜资源。

[B501](BATCH501_TYPED_IR_RAY_DISPATCH.md) 已完成第一份真实转换 TraceRay 的 native→capture→replay 闭环。最终库 `10c28bf1`：命中73/未命中11，16事件三方向/EID0，强制不同 VA/AS-ID 分配顺序，完整152-byte packet/GRS/SBT/AS-header、资源 usage、54坏输入及合法hit stride32控制通过。官方两scene及相关旧回归、40次生命周期通过。生产能力仍false。

首个声明是有边界的 schema1：Shared CPU静态数据；两个直接根参数（AS header SRV、uint32 UAV）；VisibleFunction索引；32-byte records；null static sampler；深度2；无callable/资源或sampler heap/额外IFT指针。最多4096个ray、64个实例、每区域64 records。未覆盖的布局在调度前拒绝。

[B502](BATCH502_IR_NULL_SHADERS_AND_ANY_HIT.md) 已验证并补齐 UE `pad0=~0ull`、空 miss/closest-hit 和转换 any-hit 的 VFT 关联；immutable function handle 的 DXR 角色声明防止错函数调用。当前库28d2a162：九组真实 native/capture/replay、432次事件/EID0、强制AS-ID/VA变化，69反例/7合法控制、官方sample/相关旧回归/40生命周期通过。pad0保留任意标量，raygen仍不可为空。不是完整UE验收。

[B503](BATCH503_IR_LOCAL_ROOTS_AND_STATIC_SAMPLERS.md) 已补并验证 local CBV/SRV/constants、texture table和static sampler（包括UE同六种组合、实际slot3采样），96-byte记录与typed资源usage；局部输出131/243、any-hit243/243、texture ID1→2、合法sampler ID重编号285→29/288→32通过。当前库285f8768：15场景/810事件/EID0、185反例/15控制、官方sample/相关旧回归/40生命周期通过。global仍两直接根，metadata/bias仅0，Private/GPU IR与嵌套UB/heap/callable未支持。

[B504](BATCH504_IR_GLOBAL_ROOTS_AND_REFLECTION.md) 补多根GRS：不可变PSO布局、CBV/SRV/constants/texture table/六static sampler与明确AS-header/UAV，8-word真实SDK GRS；全局输出1284/1222、全局+局部1342/1454、组合any-hit1454/1454。当前库5566f91b：最终串行20组/1155事件/EID0、325坏IR/21合法控制、官方sample/相关旧回归/40生命周期通过。全局texture1→2、局部2→3、AS2→3/VA变化通过。20份实际SDK shader reflection保存并核验；首次测试短暂重叠不计验收，加锁后全部串行重跑。仍只有Shared静态IR输入、heap/callable与metadata/bias非0未支持。

UE源码已确认普通RT shader通过离线converter生成metallib，FMetalCodeHeader的GlobalRootParams/LocalRootParams与加载到Metal的data分离；runtime只合成dispatch/intersection wrapper，function还会延迟创建，不能通过最近一次root创建或converter调用顺序猜关联。现有隔离MetalRHI producer提供接入位置，须按已知shader/header/library/export/function/PSO建立显式可靠关联。实际UE GRS有static/dynamic UB与末槽static sampler，dispatch同时带nonzero资源/sampler heap；本批fixture两heap仍为0，不能计UE支持。

[B505](BATCH505_IR_DESCRIPTOR_HEAPS.md) 已补直接索引资源/sampler heap：不可变PSO slot类型、raw SRV有界view/byte-size metadata、texture/sampler、动态slot0/3及零洞；packet两heap地址不再要求0。真实HLSL6_6直接索引heap，输出96/48、局部154/280、全局+局部1365/1491。当前库1b40caee：27场景/1635事件/EID0、435坏IR/27控制、官方sample/相关旧回归/40生命周期通过；全流程串行与新heap gate互斥检查通过。28份SDK reflection与root flags3072验证，SDK不列heap实际成员，须来源已知producer类型，不能推断GPU整数。

[B506](BATCH506_IR_HEAP_ACCELERATION_STRUCTURES.md) 已补资源heap的AS header及无直接AS根的TraceRay。使用几何平移不同的第二TLAS，输出34/110、local266/168、global+local1477/1379；typed身份与贡献、子BLAS/IFT、CS依赖、别名及帧内保护共用全局校验。当前库b0094506：最终串行36场景/2277事件/EID0、550坏IR/39合法控制，官方2scene/270事件/10查询/13反例、相关旧回归和40生命周期(growth933888bytes)通过。64份实际SDK反射（新AS36份）、heap-only无AS根布局及新gate互斥核验通过。B505官方反例误写41已按日志更正13；41为另行旧argument。当前IR header仅允许direct TLAS kind5，UE真实构建用Indirect，仍不是完整UE支持。

[B507](BATCH507_IR_INDIRECT_TLAS.md) 补kind9间接/kind10空/kind11全屏蔽TLAS初态的IR闭包，复用72→68-byte保留UserID的typed BLAS转换。活跃InstanceID73输出146/11，Private输入GPU上传/AS构建后清零仍正确；heap-only34/183、local266/241、global+local1477/1452。库ee222fe1：最终串行51场景/3147事件、706坏IR/53控制，合法UserID74输出147/11，60份新SDK/冻结配方/Private清零及新gate互斥通过；官方/相关旧回归/40有限重开通过(growth1589248bytes)。首轮oracle与脚本错误保留。所有帧内AS encoder仍被IR静态契约提前拒绝；eResDevice关闭错误在B506亦已存在，当前有限生命周期不足以证明完整清理。

[B508](BATCH508_INITIAL_LIST_AND_DEVICE_CLOSE.md) 已修owner device关闭注册和CPU structured-export资源表清理，解码与DX12/Vulkan相同的NeededInitials WrittenRecord元数据；typed Metal初态仍独立恢复，不调用未实现的generic初态。库5f4db1c7：最终51场景/3147事件、706坏IR/53控制、1坏列表/1往返控制、官方与旧定向回归通过；40重开growth409600bytes，另30失败+10成功关闭/CPU导出/seek/reset growth1081344bytes。CPU/native LLDB断点证明owner不再作为child释放，最终2326有效日志无旧device/资源表/断言诊断。首次日志锁/零长度坏chunk构造失败保留，B507空TLAS反例沿用旧chunk长度导致的断言已修测试并重跑5组156/14；CPU互斥与语法检查通过。这一批关闭修复不等于ARC任意提前释放或完整UE生命周期验收。

接下来补帧内AS当前配方/preflight更新和EID0重置，再补heap CBV/UAV及typed texture-buffer view，再补UE producer可靠shader/header/library/export/function/PSO关联；增加nested UB闭包、大heap/区域范围和Private/GPU执行点来源、动态heap及必要SBT/callable。显式fixture注解不替代生产UE支持。最后进入实际UE RT调度输出、事件/EID0和绑定验收。

UE本身、最终集中回归、原生能力判定及尚未闭环的资源生命周期都必须达到持久启用计划门槛后才开启能力。B495系统停滞根因未修；禁止直接旧长UE/full followup。AS内部查看、RT shader单步、RT Pixel History不在支持承诺内。持续目标active，未提交/推送。

[B521](BATCH521_CONVERTED_INLINE_QUERY.md) 新增独立 converted inline RayQuery 支持：root slot2、两直接根、64-byte AS header，完整类型命名空间/immutable 初态/输出闭包；参照 VK 与 DX12 AS descriptor 重定位，inline query 沿普通compute dispatch，无 SBT 或函数表。最终e72514c6两份0/8偏移场景四结果/32事件/EID0/不同VA-ASID/保护字通过，39坏组78拒绝、旧纹理heap50/IR四场景/fresh TraceRay/官方sample通过，588完成日志无严格诊断。未声明控制拒绝。仅静态Shared普通TLAS，实际UE Private/GPU header、间接TLAS/帧内更新、typed producer及低工作集待补；原UE仍CPU预算拒绝。初始fixture -11和后续编译/检查器失败均保留，不计通过，用户Qt/AS崩溃和冻结未闭环。下一B522复用typed间接TLAS初态供query，再补UE所需header和producer；RT两个能力false。

B522进行中：新增inline query间接/空/全屏蔽TLAS初态及UserID真实结果验证；保持静态Shared ABI及未开启能力，见BATCH522，最终验收待跑。

[B522](BATCH522_INLINE_QUERY_INDIRECT_TLAS.md) 完成静态inline query kind9/10/11间接/空/全屏蔽TLAS及UserID真实输出。6f8bab50八场景/128事件/EID0，Private输入构建后清零、64实例最后命中、149坏组298拒绝、旧query2/TraceRay4/官方sample及能力通过，1108有效日志无严格诊断。B521旧texture/full回归未重跑，不继承。下一B523帧内TLAS冻结配方/当前状态与前后query，再补Private/GPU header、typed producer和低工作集UE；能力仍false。

B523进行中：typed Private帧内TLAS重建前后inline query、初态/事件回退与EID0，保留Shared参数契约，见BATCH523。

[B523](BATCH523_INLINE_QUERY_FRAME_TLAS.md) 完成已初始化TLAS的Private帧内当前配方供query消费：六场景/144事件/EID0、两roots VA/保护字、两个输出独立usage、BLAS前后依赖、Private source重放清零/EID0；239坏组478拒绝/六合法控制，旧query十份/TraceRay四份/官方及能力通过。f71629a0严格1744日志无诊断；oracle/控制构造失败保留，能力false，UE仍无整帧GPU验证。下一B524补query帧内Private BLAS几何当前配方，再header/producer与UE低工作集。

[B524](BATCH524_INLINE_QUERY_FRAME_GEOMETRY.md) 完成query Private帧内几何当前配方：八场景/192事件，499坏组998拒绝、十合法控制；64几何与64实例真实最后命中4398，源清零、事件回退/EID0与VA-ASID/usage通过。4d350b89库，旧query16/IR4/官方和能力通过，3358日志无严格诊断。保留首次缺初态fixture拒绝及修复后完整复验。合法65实例native/capture成功、按重放64上限预提交拒绝；未声明控制亦拒绝。UE GPU和旧全量未跑，两能力false。下一B525新TLAS目标，再Private/GPU header/producer及UE低工作集。

2026-10-06 [PHASE59](PHASE59.md) / [B525](BATCH525_INLINE_QUERY_NEW_TLAS.md)：query允许顺序验证的帧内新TLAS，无初态目标仅在typed冻结build提交后可用；独立前后Shared header/roots。backend/bundle99e80e2d、GUI3ba30e36；四native/capture/API/CLI、96事件/EID0、不同ASID/VA/两输出及前后usage通过；176坏组352无GPU wait拒绝含缺build/错target/提前query，四合法控制；旧query24 API（此前B524八份仅API）及旧16 API/CLI、TraceRay四份API/CLI、官方2scene/270事件/10查询/六能力通过，1280严格日志无诊断。保留首次MTLResourceID类型编译失败，._impl修复后全验；旧texture/full/Qt-ARC未重跑，无UE GPU。源码确认UE TLAS header为Shared动态子分配（纠正此前Private header猜测），下一补有界Shared header偏移、typed producer及低工作集UE；两能力false、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B526](BATCH526_INLINE_QUERY_HEADER_RANGES.md)：query解析声明buffer+8对齐offset中的64-byte Shared AS header，保留whole backing≤16KiB/不可变来源/typed字段/地址唯一性；原chunk格式保持。backend/bundlee231e2bd、GUI3ba30e36；八native/capture/API/CLI、176事件/EID0、offset32/4096/16304与最后16字节保护区、frame/newTarget/multi64组合通过；236坏组472无GPU wait拒绝、五合法控制。旧query28（B524八份API，其余20 API/CLI）、TraceRay四份API/CLI、官方2scene/270事件/10查询/六能力通过，1734完成日志无严格诊断。旧texture/full/32CPU-AIR/Qt-ARC/官方坏13未重跑，无UE GPU。动态header、heap/nested绑定、producer及UE预算仍待补；下一先隔离低工作集UE场景和新截帧再补实际暴露缺口，两能力false、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B527](BATCH527_UE_SMALL_RAY_SCENE.md)：新增隔离3 mesh/2 light Blueprint工程与有限NullRHI生成器（两关卡保存通过），禁用无关默认插件并配置必需SkinCache；小UE有限capture成功、owned cleanup -9，不计正常退出。e231e2bd库，capture1b19090e/56,406,168bytes，3间接TLAS/99direct+56indirectcompute/163编译完成；60原metallib AIR全部审查、2真实非零RayQuery [2,72,1]/[786,1,1]，输出和重放未验。45heap共3,029,598,208bytes（旧5,266,571,264），初态blob1,227,522,505（旧4,362,319,907）；未证明总预算通过。normal-open API4因frame-born identity，CPU65 API4因Depth16 8192x2048 frame texture，均无GPU wait/断言；保留首轮缺SkinCache Fatal及第二轮startup timeout，未计通过。原用户工程/installed engine未改，两能力false。下一B528小场景CSM分辨率/成本验证及实际typed producer闭包，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B528](BATCH528_UE_BOUNDED_SHADOW_SCENE.md)：补UE isolated small-shadow命令配置，实际cvar256/256/1与Depth16 frame256x256核验，越过B527 8192x2048拒绝；新capture9fa35417/62,399,341bytes、6间接TLAS/186direct+112indirectcompute，60 AIR条目全部审查、2shader/4非零RayQuery；owned capture cleanup -9不计正常退出，输出与GPU重放未验。43heap2,962,489,344bytes，初态blob1,608,781,321，未证明总预算通过（比B527初态增加，不宣称单调优化）。CPU65下一拒绝R32Uint3D192x48x48/usage3，normal frame-born identity拒绝，两API4无GPUwait/严格诊断。backend/bundlee231e2bd、GUI3ba30e36，两能力false，无driver guard放宽、原用户工程/installed engine未改。下一B529实际R32Uint3D frame extent与native/sample反例，再typed动态AS header/producer及预算闭包；持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B529](BATCH529_UE_UINT_VOLUME.md)：补coverage65 Private/Tracked RW单mip R32Uint3D≤256x64x64、array1，复用typed placement/lifetime/预算。backend/bundleaee8c20d、GUI3ba30e36；3shape六native/capture/API/CLI、72选择/EID0、各z不同uint/全体素/PickPixel/两write+read usage/不同ID通过；20坏组40拒绝；fresh整数array/2Datomic四capture、旧50texture/view/heap+四floatvolume API/CLI、八query/四IR、官方2scene/270事件/10查询/六能力/CPU预算通过，524完成日志无严格诊断。保留首次sample每dispatch超262144预算拒绝（改每线程≤4相邻体素、guard不变）及fresh raw-source无AIR usage缺失失败（改对应编译metallib后通过）；此前已通过54旧纹理没有重复跑，不将失败计通过。原B528UE仅CPU65越过R32Uint，下一RG11B10Float3D10x8x26拒绝；没有新UE GPU/full/Qt-ARC。两能力false，下一B530 packed-float volume实际sample，再动态header/producer/预算闭包，持续active、未提交推送。

B530进行中：实际packed-float volume与native/capture/replay/反例/UE CPU下一缺口，见BATCH530。


2026-10-06 [PHASE59](PHASE59.md) / [B530](BATCH530_UE_PACKED_FLOAT_VOLUME.md)：补coverage65 Private/Tracked RG11B10Float单mip RW3D≤64³，wholeheap/预算保持；backend/bundle99df52e4、GUI3ba30e36。三shape六native/capture/API/CLI、72事件/EID0、每z独立精确11-bit值/全部packedbits/PickPixel/usage/ID通过；20坏组40 GPU前拒绝。旧60texture/view/heap/volume、八query/四IR API/CLI、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；544完成日志无严格诊断。首轮wrong_format=92与原RG11B10Float值相同，合法OpenCapture成功，保留FAIL，不声称GPU前拒绝；换Depth32Float3D完整复验。实际B528UE仅CPU65越过packed3D，下一RG11B10Float二维4096x4096 RW拒绝，无新UE/GPU/full/Qt-ARC；两能力false，下一B531有界大packed二维sample，再动态header/producer/预算闭包，持续active、未提交推送。


2026-10-06 [PHASE59](PHASE59.md) / [B531](BATCH531_UE_LARGE_PACKED_TEXTURE.md)：补coverage65 Private/Tracked RG11B10Float二维单mip RW≤4096²，wholeheap/总预算保持；backend/bundle3f7e0854、GUI3ba30e36。4096²/2048x1376/3x5六native/capture/API/CLI、72事件/EID0、全部packedbits/PickPixel/usage/ID通过；每线程≤8x8像素守262144调度预算，20坏组40拒绝。旧66texture/view/heap/volume、八query/四IR、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；568最终日志无严格诊断。实际B528UE仅CPU65越过大packed2D，下一RGB10A2Unorm二维320x240 usage7拒绝，无新UE/GPU/full/Qt-ARC；两能力false，下一B532 packed二维RT用途与clear/sample，随后小Lumen cache/typed动态header/producer及预算闭包，持续active、未提交推送。


2026-10-06 [PHASE59](PHASE59.md) / [B532](BATCH532_UE_PACKED_RENDER_TEXTURE.md)：coverage65 RGB10A2Unorm2D≤512单mip读写可带RT7；backend/bundledba15ac3、GUI3ba30e36。三shape六native/capture/API/CLI、96事件/EID0、native clear读32/96/160、每像素packedbits/PickPixel及独立clear EID往返通过；20坏组40拒绝。旧72texture/view/heap/volume、八query/四IR、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；592日志无严格诊断。实际B528UE仅CPU65越过RGB10A2 usage7，下一BGRA8Unorm_sRGB2D320x240 usage7拒绝；无新UE/GPU/full/Qt-ARC，两能力false。下一B533实际小Lumen缓存新UE截帧并核验保留HW query及成本，再补sRGB与typed动态header/producer，持续active、未提交推送。


2026-10-06 [PHASE59](PHASE59.md) / [B533](BATCH533_UE_SMALL_LUMEN_CACHES.md)：新增隔离--small-lumen-caches及运行cvar数值/来源检查，surface512²/grid4/probe8/atlas32实际生效；dba15ac3库，newUEcapture9238d0c3/70,728,712bytes、3间接TLAS/99direct+56indirectcompute，60 AIR全审查0unvalidated、2shader/2非零HW query [2,64,1]/[132,1,1]，owned cleanup -9不计正常退出，输出/重放未验。52heap3,556,769,792bytes比B528增加，初态blob1,170,276,095减少，heap alone超过3GiB-128MiB硬上限，不宣称预算达标。normal/CPU65 API4且无GPUwait/严格诊断，下一R16Uint2D4096x16 usage3；源码找到isolated provider block64MiB（原installed512MiB），可独立减小粒度、guard保持。两能力false，下一B534实际R16Uint二维样例，再有界provider heap粒度与typed动态header/sRGB，未改原用户工程/installed engine、持续active、未提交推送。


2026-10-06 [PHASE59](PHASE59.md) / [B534](BATCH534_UE_UINT16_TEXTURE.md)：coverage65 Private/Tracked R16Uint2D单mip RW≤4096x512，unsigned16 raw/PickPixel与每row32768+y→65535-y；backend/bundlef4f8b745、GUI3ba30e36。4096x16/4096x512/3x5六native/capture/API/CLI、72事件/EID0、ID/usage/全部像素通过；八像素/线程守262144预算，20坏组40拒绝。旧78texture/view/heap/volume、八query/四IR、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；616日志无严格诊断。B533实际新UE CPU65已越过frame创建，native3,593,011,456/initial1,170,276,095/snapshots12,957,904，GPU静态总预算拒绝，无frame GPU。两flagsfalse，下一B535独立provider16MiB block及实际低heap新UE，保留安装引擎/原用户工程和guard；旧B528 sRGB仍未支持，typed动态header/producer待补。持续active、未提交推送。


2026-10-06 [B535](BATCH535_UE_SMALL_PLACEMENT_BLOCKS.md)：隔离provider placement block64→16MiB，唯一源码变更MetalBuffer.cpp、98exports完整，新模块ee945869；--rhi明确选择并记录hash。f4f8b745 backend/bundle不变，新UE capture fbe1b7ac/73,738,545bytes、3间接TLAS/96direct+56indirectcompute、60 AIR全部审查/2shader两非零HW query，owned cleanup -9。80heap共1,731,791,872bytes（旧3.557GB），初态1,175,476,527反略增；CPU65首次接受metadata/预算(native1,765,703,936/snapshot12,835,615)，未执行GPU重放。pre-submit65 API4于RGBA16Uint TextureBuffer8192/row65536创建，无GPUwait/初态上传；10保存日志无严格诊断。生产flagsfalse，UE输出/事件/EID0/typed动态header未验，原引擎/用户工程及guard保持；下一B536 typed RGBA16Uint view真实读写sample与旧回归，再当前UE提交前预检。持续active、未提交推送。


2026-10-06 [B536](BATCH536_UE_UINT16_BUFFER_TEXTURE.md)：补RGBA16Uint TextureBuffer8byte创建/读回/PickPixel及authoritative reflection的纯write typedUAV路径；backend/bundle5bbd0636、GUI3ba30e36。3shape六native/capture/API/CLI、408raw/pixel、24seek cycles/88事件选择、真实高位RGBA与RO/RW、零/非零offset、usage及EID0输出清零通过。72格式/范围/初态坏组144上传前拒绝，3writer组6加载期拒绝（发生初态上传，不计GPU前）。旧16格式两份/旧large R32三份API/CLI、新163初态纹理两capture12696subresource、八query/IR64、官方两scene270事件10查询及六能力通过，630完成日志无严格诊断。实际B535UE仅pre-submit65已越过buffer-view创建，下一slot25:27432/gen5508/type4无typedsource，API4且无GPUwait/初态上传；未验UE frameGPU/output/EID0。首三失败/oracle误判保留，生产flagsfalse、guard不放宽；下一B537显式AS-header factory/producer与typed动态header闭包，持续active、未提交推送。


2026-10-06 [PHASE59](PHASE59.md) / [B537](BATCH537_TYPED_AS_HEADER_PROVENANCE.md)：新增显式64-byte AS header对象/贡献来源与静态coverage3双字段重定位，UE factory kind3及实际header write捕获；Shared placement只授予捕获事实。最终backend/bundle2b363cb3、GUI3ba30e36、provider aba8b52b/98exports。八native/capture/API/CLI、152事件/EID0、12 annotation拒绝/同值重复接受、343坏组686无GPUwait拒绝/五合法控制、47旧回归（官方两scene270事件10查询/六能力）通过，2378日志无严格诊断。新UE b35de06d/65,098,798bytes、6header（三frame）/23kind3来源全核对、3TLAS/99direct+56indirect/60AIR全审查/两非零HW query；owned cleanup -9。68heap1,558,989,824/初态1,123,342,185，未宣称总预算通过；CPU65/pre-submit65动态header契约拒绝、无初态上传/frameGPU，UE输出/事件/EID0未验。保留两build/空fixture/首UE placement捕获拒绝；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两能力false、未提交推送。下一B538 typed动态header版本/Private贡献/descriptor producer-consumer及UE query PSO/root闭包，持续active。


2026-10-06 [PHASE59](PHASE59.md) / [B538](BATCH538_DYNAMIC_AS_HEADER_SNAPSHOTS.md)：补 StartCapture 最新 header 初态冻结、coverage3 同 Header 帧内显式版本/双字段重定位与提交顺序、Shared placement header和不可变 Private 贡献源，EID0恢复版本/raw。最终backend/bundle b71d6e4a、GUI3ba30e36、复用provider aba8b52b。七场景八capture native/capture/API/CLI、208事件选择/EID0、12 annotation拒绝、195坏组390无GPUwait拒绝/三合法控制、47旧回归（官方两scene270事件10查询/六能力）及20 controller生命周期PASS（growth425984bytes），1469日志无严格诊断。新UE c41abb06/66,705,869bytes、3TLAS/93direct+56indirect/60AIR全审查/两非零HW query；6header身份匹配、三背景snapshot与初态逐字节一致、26kind3记录，三frame header确为帧内新建，owned cleanup -9。65heap1,488,096,256/初态1,122,325,471，未宣称总预算通过；normal/CPU65/pre-submit65仍提交前拒绝UE新header/query闭包，无初态上传/frameGPU，UE输出/事件/EID0未验。保留build/fixture/oracle失败及修复复验；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两能力false、未提交推送。下一B539帧内新header、kind3当前slot/query consumer/依赖闭包，再实际UE验收；持续任务未完成、继续active。



2026-10-06 [PHASE59](PHASE59.md) / [B539](BATCH539_TYPED_HEAP_QUERY_CONSUMERS.md)：完成coverage65初态AS/CPU kind3槽位/typed query PSO重放，双字段Header及descriptor VA重定位、依赖与EID0；限制frame AS build、GPU Header/贡献改写、未声明消费者，未全局放宽。最终backend/bundle1e13bd74、GUI3ba30e36、provider复用aba8b52b。四新sample native/capture/API/CLI、64事件；六旧动态场景七新capture/192事件，347坏组694无GPUwait拒绝/七合法控制、47旧回归（官方两scene270事件10query/六能力）及20 controller生命周期PASS（growth442368bytes），3762日志无严格诊断。复用B538UE c41abb06，仅最终库normal/CPU65/pre-submit65提交前拒绝frame新Header，无初态上传/frameGPU；无新UE capture/UE输出事件EID0验收。保留helper编译/空TLAS反例/遗漏oracle失败与完整复验；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两生产能力false、未提交推送。下一B540 Header帧内创建/发布+当前AS build/CPU slot消费时序，再实际Lumen PSO/root/indirect/GPU producer与UE验收；持续active。



2026-10-06 [PHASE59](PHASE59.md) / [B540](BATCH540_FRAME_HEADER_HEAP_QUERY.md)：补coverage65新Shared64-byte Header创建/发布、CPU kind3槽位与当前Private typed TLAS build/query提交顺序及EID0；复用VK/DX12已知对象/冻结输入，未放宽未声明namespace。最终backend/bundlebfeca02b、GUI3ba30e36、provider复用aba8b52b。四新sample native/capture/API/CLI、128事件；四旧heap-query/64事件、六旧动态场景七capture/192事件，422坏组844无GPUwait拒绝/七合法控制、47旧回归（官方两scene270事件10query/六能力）及20 controller生命周期PASS（growth458752bytes），2985日志无严格诊断。复用B538UE c41abb06，最终normal/CPU65/pre-submit65仍提交前拒绝，无新UE/frameGPU/输出验收。新提取实际三TLAS输入options544 Private placement589824bytes、scratch1376256，构建snapshot皆空/children0；不能用standalone sample通过覆盖UE。保留首build/空initialshadow/gate单query计数失败与完整修复复验；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两生产能力false、未提交推送。下一B541实际Private placement每build快照/known-child闭包、GPU producer与sample→新UE，再Lumen PSO/root/indirect契约；持续active。



2026-10-07 [PHASE59](PHASE59.md) / [B541](BATCH541_PRIVATE_COMPACT_SIZE_AND_UE_AS_INPUT.md)：定位UE空packet真正前置缺口为Private compact-size读取/独立query submission及native compact被跳过；补encoder-end4/8-byte冻结、query build来源、保留native转发且未知replay拒绝。最终backend/bundlef6a8029d、GUI3ba30e36、provider复用aba8b52b。六新placement/compact/空frame sample192事件，四旧heap64事件，168坏组336无GPUwait拒绝/六合法、56回归检查（含官方两scene270事件10query/六能力）、fresh四Sharedcompact18检查及20controller lifecycle PASS（growth3506176），1436完成日志无严格诊断。新UE716106d8，主TLAS216bytes+known两child7097/7101（GPU67/71），两个空TLASbytes0；60AIR全部审查/两非零HW query[2,7,1]/[132,1,1]，nativecompact错误消失，但两BLAS初态recipe缺失。normal/CPU65/pre-submit65仍动态heap-query闭包API4提交前拒绝，UE输出/事件/EID0/frameGPU未验；61heap1419217920/初态1110926467不宣称总预算通过。保留中间失败；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两flagsfalse、无提交推送。下一B542实际堆内BLAS几何/索引冻结与compact子配方，再Lumen PSO/root/indirect/GPU producer；持续active。


2026-10-07 [PHASE59](PHASE59.md) / [B542](BATCH542_PRIVATE_PLACEMENT_GEOMETRY.md)：补Private/Tracked placement BLAS顶点/索引encoder-end冻结、kind1/2/8初态及帧内重建，复用VK/DX12 typed build-input路径与原预算/退休/范围/compact容量核验。backend/bundlea138d70a、GUI3ba30e36、provideraba8b52b；14新query448事件+4旧heap64事件、714坏组1428无GPUwait拒绝/26合法、56旧回归（含官方两scene270事件10query/六能力）、fresh四Sharedcompact18检查与20controller lifecycle PASS（growth11190272），4994完成日志无严格诊断。新UE9359c9e2/69862532bytes，主TLAS216bytes+两compact kind2 child6859/6863配方齐全（648/288与48/12bytes），另两空TLAS；60AIR全审查/两非零HW query[2,66,1]/[132,1,1]，实际root2为40/48byte CBV表。normal身份guard、CPU65/pre-submit65动态heap-query闭包均API4无GPUwait/初态上传/frameGPU；64heap1491815424/初态1127834757预算未验。保留fixture/审计脚本失败；UE输出/事件/EID0、full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两flagsfalse、无提交推送。下一B543实际Lumen PSO/CBV-root/间接query和GPU producer-consumer声明，再UE整帧；持续active。


2026-10-07 [PHASE59](PHASE59.md) / [B543](BATCH543_TYPED_HEAP_QUERY_INDIRECT.md)：参照VK FetchIndirectData/DX12 SaveExecuteIndirectParameters，补typed heap-query间接调度，逐encoder/ordinal/epoch核验原生GPU参数，保留原间接调用、零工作量与预算/闭包；修正typed只读资源误标CS_RW。最终backend/bundle2d7439ae、GUI3ba30e36、provideraba8b52b；八新间接query416事件（同encoder0/1/2、GPU1/132/0、末端offset/unretained/64几何/空TLAS）+14旧direct448事件+4旧heap64事件均Native/capture/API/CLI PASS。新72坏组144无GPUwait拒绝、6执行值不符组12次GPU核对后拒绝、2合法；普通计算间接八新capture56reset/seek、176坏组352无GPUwait/2执行不符组4拒绝PASS。56回归含官方两scene270事件10query/六能力、fresh四Sharedcompact18检查、20controller lifecycle（growth3063808）PASS，1798完成日志无严格诊断。复用B542实际UE9359c9e2，当前库normal/CPU65/pre-submit65仍API4无GPUwait/初态上传/frameGPU，UE完整输出与预算未验；40/48-byte CBV-root/二维调度/GPU producer-consumer仍缺。中间失败与502a结果保留不计最终PASS；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两flagsfalse、未提交推送。下一B544 typed CBV query闭包→实际Lumen，再短UE重放；持续active。


2026-10-07 推进方式更新 / [B544开发中](BATCH544_API_RT_INTEGRATION.md)：按用户最新要求改为通用Metal API资源/根绑定/AS/调度链的大批次集成，开发中定向检查，集成后固定库一次集中验收；Lumen为实际应用目标，不按shader/pass/效果特判，继续参照VK/DX12。219产物目录、240184文件、18575982436bytes（17.3GiB）逐文件校验移至外盘20261007-015524并保留原路径链接，删除58可重建Xcode cache；build约2.3GiB、可用约33GiB，原工程/原captures保留。持续任务规则已更新，新批次先用metal_artifact_storage_macos.py外盘建目录。B544新增通用CBV读范围/heap UAV与多维参数检查，开发库/bundle21b6dec8构建通过；40-byte真实query独立API3387/528uint/48事件通过，48-byte二维[2,66,1]、64几何/实例/unretained完整开发gate4426/528uint/56事件通过，初期构建/oracle失败保留。不是本批最终验收；实际UE根还含static sampler/帧内/Private资源与texture输出，继续合并公共闭包后一次集中回归再UE。两flagsfalse，不把B543旧库验收当新库PASS，未提交推送、持续active。详见[通用API集成推进规则](RT_API_INTEGRATION_WORKFLOW.md)。


2026-10-07 [B544通用API集成继续开发](BATCH544_API_RT_INTEGRATION.md)：保持大批次合并/开发定向验证/固定最终库集中验收，Lumen仅作实际应用验收，接口与初态/重放继续参照VK/DX12。新增共用IR compute混合根类型与texture SRV依赖，static sampler逐slot来源/身份重定位，Private（含Tracked placement）初态CBV范围读取；不按shader/pass/效果特判。当前开发backend/bundle e8f709bf、GUI3ba30e36：40-byte四Private CBV+六sampler、48-byte五Private placement CBV+六sampler/二维[2,66,1]/64几何实例/unretained，两fresh Native/capture/API/CLI PASS（528uint各、48/56事件及EID0），六真实texture sample AIR调用/所有sampler ID变化/完整padding与read-only usage；27坏组54无GPUwait拒绝+合法对照、五旧capture API/CLI十检查PASS。mixed5早期初态拒绝/验收工具悬空SD引用崩溃已保留并修正，不等同此前UE/Qt崩溃修复。B544未关闭、未集中验收；下一帧内CBV producer-consumer与texture/Private输出通用闭包，再一次集中相关回归→短UE实际光追截帧/输出/事件/预算。两flagsfalse，无新UE或本库官方/full/lifecycle/Qt-ARC验收，不继承旧库。219目录17.3GiB外盘校验迁移与58 cache清理已完成，原路径链接、活动B544产物继续外盘；内盘约32GiB可用。未提交推送/重置，持续active。

当前开发检查点的实现/许可来源/命令入口/实际通过与未跑范围详见[B544](BATCH544_API_RT_INTEGRATION.md)的混合根与Private初态CBV段；本phase未达到能力启用终点。

### B544 帧内/较大 CBV 与 Private UAV 开发检查点

同一大批次继续补齐帧内 CBV 创建/CPU 与已知 Shared→Private blit 发布/提交顺序、2MiB backing 内的 CBV 子范围，以及 Private standalone/Tracked placement UAV 输出。实现参照 VK/DX12 的资源+offset、初态与提交依赖，不按 Lumen/shader/pass 特判。当前开发 backend/bundle `466a6485`、GUI `3ba30e36`：两新 Private 输出场景各 Native/capture/API/CLI PASS（528uint、56事件/EID0、二维[2,66,1]与零调度），其中一项组合五个帧内2MiB WriteCombined CBV、offset58624；四输出坏组八无GPUwait拒绝+合法对照，七旧capture十四API/CLI定向检查PASS。早期86f帧内三场景/34坏组与479d大初态/c9aa大帧内检查分别保留自己的库哈希，不计当前库最终验收。B544未关闭；texture UAV、多输出/更广GPU生产及真实PSO元数据闭包尚需继续，随后固定库集中相关回归→短UE真实光追截帧/输出/事件/预算。当前库未跑官方/full/lifecycle/UE/Qt-ARC，两生产flags仍false；此前用户崩溃/重启修复未证明。产物和检查点直接写外盘，219目录约17.3GiB迁移/58缓存清理保持有效，内盘约32GiB可用；本轮删三个过时patch脚本，保留失败证据。无提交推送/重置，持续active。


详细分库结果、失败与续作见 [B544](BATCH544_API_RT_INTEGRATION.md)。本批仍开发中，不新增小批次验收。


## 2026-10-07 纹理 SRV/UAV 与实际 PSO 绑定元数据继续集成

2026-10-07 [B544 通用纹理与 PSO 绑定 ABI 集成开发](BATCH544_API_RT_INTEGRATION.md)：同一大批次补齐类型化 texture SRV/UAV、Private/Tracked placement、原始创建 usage 核验及原生小堆范围；新增不可变 PSO 反射/运行时 ABI 捕获，保持 compiler JSON 与 runtime binding facts 的来源区别。当前开发 backend/bundle `96cf2591`、GUI `3ba30e36`、provider `2496f103`：新复合 Native/capture/API/CLI PASS（528 texels、56事件/EID0、48-byte帧内根/二维调度）；16坏组32无GPUwait拒绝+合法控制，10旧capture20 API/CLI定向回归PASS。实际新UE `403f90ea`/75,234,193bytes：119 PSO ABI/AIR核验、6光追PSO/5非零调度、40–72-byte根及原生线程组/绑定payload长度逐次一致，未依赖Lumen分组筛选；旧Lumen范围60 AIR/两非零query也通过。UE CPU65仍因动态AS Header/heap消费契约拒绝、无GPUwait/初态上传/frameGPU；UE输出/事件/EID0及总预算未验。B544未关闭，下一合并当前执行点动态/多输出和GPU生产依赖闭包，再固定库集中相关回归与短UE完整验收。两生产flagsfalse，无本库官方/full/lifecycle/Qt-ARC验收，不继承旧库；未证明历史Qt/UE崩溃或重启已修复。新产物继续外盘，未提交/推送/重置，持续active。

分库范围、完整hash、失败复验、真实UE的六query PSO及续作见[B544](BATCH544_API_RT_INTEGRATION.md)。本阶段仍集成开发中，没有新增小批次或开启生产能力。


本轮追加迁移34个已关闭frozen-validation目录（1,021文件/1,265,807,006bytes，约1.18GiB），逐文件SHA及内部symlink完全核对后保留原路径链接；清单 `development/additional-frozen-storage-migration.json`。此前219目录17.3GiB/58可重建缓存清理不变；当前新产物全部外盘，主构建/工作dylib保留，内盘约32GiB可用。没有删capture或失败证据。


## B544 执行点多输出与 GPU 生产消费开发检查点

2026-10-07 [B544 通用多 UAV 与执行点依赖集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，PSO输出角色与资源身份解耦，多个texture UAV按当前heap slot解析；已验证槽位换绑、query纹理GPU输出→后续SRV消费的提交依赖及零调度。未证明的GPU buffer writer保持opaque，不能把旧初态用作后续消费证明。实现参照VK/DX12，无UE/Lumen/shader名分支。当前开发backend/bundle `105986c1`、GUI `3ba30e36`、provider `2496f103`；同一当前库两fresh Native/capture/API/CLI PASS：Private placement/五帧内2MiB CBV与Shared UAV/四帧内Private CBV，每项五纹理各528texels、48事件选择/EID0、144身份检查、36公共descriptor查询、完整CBV/padding/六sampler身份及只读usage，结果4248→4560和2375→2687。12坏组24无GPUwait拒绝+合法副本、10旧capture20 API/CLI定向检查通过，两真实compiler/AIR证明各两非零query；158完成日志无严格诊断。新19源码/四产品检查点及全部产物外盘，内盘约32GiB。B544未关闭，实际UE的通用动态AS Header/heap消费闭包及整帧GPU输出/定位/EID0/预算仍未验，本轮未重复启动已知失败UE；当前库官方/full/lifecycle/Qt-ARC未跑，两生产flagsfalse，历史崩溃/重启修复未证明。无提交推送/重置，持续active。

本次不关闭phase或新增小批次；统一证据 `development/multi-UAV-current-integration-manifest.json`、外盘 `checkpoint-multi-UAV-105986/manifest.json`。下一仍在B544补实际PSO/根ABI与当前heap索引、动态AS Header的通用消费契约，合并更广生产/帧内资源/生命周期后再冻结库集中验收和短UE完整重放。


## B544 通用 AS 绑定与运行时 ABI 开发检查点

2026-10-07 [B544 通用 AS descriptor 与运行时绑定集成](BATCH544_API_RT_INTEGRATION.md)：同一大批次继续开发，参照DX12当前root table/AS descriptor和VK执行点参数/usage；AIR uniform解析支持匿名AS Header及实际AS调用，公共GetDescriptorAccess/GetDescriptors返回所绑定AS而非Header buffer。严格解析runtime binding ABI，核验根布局/绑定点/线程组与实际dispatch；runtime事实不能声明shader访问或开启消费资格，setBuffer替代setBytes会清空旧inline数据。当前backend/bundle `8caaecc0`、GUI `3ba30e36`、provider `2496f103`：两已有复合capture当前API/CLI PASS，每项48事件/EID0、144身份、48公共descriptor检查（含12 AS），五纹理全部528texels/CBV padding/六sampler；10 ABI坏组20无GPUwait拒绝+合法控制，10旧capture20 API/CLI PASS。中间71c9库两fresh Native/capture/API/CLI、21坏组42拒绝/两控制/旧20检查分别保留其hash，不冒充当前库fresh capture或最终验收。CPU解析119实际UE runtime payload+1 compiler payload及六真实query AIR形状通过；AIR使用合成loader，仅证明解析覆盖8 AS reset/93纹理调用，不证明真实资源依赖/UE输出。当前110完成日志无严格诊断，27源码/四产品校验备份及产物全外盘，内盘约32GiB。B544仍开发中，实际UE动态AS Header/自动PSO-heap消费闭包和整帧GPU/事件/EID0/预算未验，未重复已知失败UE；当前库官方/full/lifecycle/Qt-ARC未跑，两flagsfalse，历史崩溃/重启修复未证明。无提交推送/重置，持续active。

没有关闭phase或新增小批次。当前证据 `development/AS-ABI-current-integration-manifest.json`；后续在同一B544补实际执行点的uniform bytes/typed source/producer提交依赖，再集中最终回归与UE整帧验收。

## B544 执行点实际 uniform/AIR 开发检查点

2026-10-07 [B544 执行点 uniform 值与实际 AIR 消费集成](BATCH544_API_RT_INTEGRATION.md)：同一大批次继续开发，按提交/dispatch fileOffset 保存已证明 CBV 字节及当前 typed heap/Header 快照，用真实 AIR 核验 AS、SRV/UAV 角色和纹理数值类型；Private 帧内 CBV 支持既有预算内的 2MiB backing/offset58624，未知 GPU writer 仍拒绝。当前 backend/bundle `45c2a584`、GUI `3ba30e36`、provider `2496f103`：两 fresh Native/capture/API/CLI PASS（大 Private CBV+Private placement 纹理、小 Private placement CBV+Shared 纹理）；每项48事件/EID0、144身份、48公共descriptor/AS检查、五纹理各528texels、全部CBV padding/六sampler。35坏组70无GPUwait拒绝+3合法控制，10旧capture20 API/CLI通过；真实消费值追踪分别54/48 scalar读取，三执行点均解析1 AS/8 texture调用且无unknown，两compiler/AIR审计及CPU numeric测试通过。316完成日志无严格诊断，28源码/四产品检查点及所有产物外盘，内盘约32GiB。B544未关闭；实际UE自动PSO消费完整依赖/动态Header及整帧GPU输出/事件/EID0/预算未验，当前库官方/full/lifecycle/Qt-ARC未跑，两flagsfalse。保留测试夹具/日志留存与早期构建失败，不声称历史崩溃/重启修复；无提交推送/重置，持续active。

证据与失败复验见B544；保持同一大批次，未关闭PHASE59。

## B544 通用 sampler 消费开发检查点

2026-10-07 [B544 实际 sampler 消费与公共查询集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，参考DX12/VK sampler descriptor access/query，用实际AIR sample/gather调用、typed根指针及当前sampler表来源核验消费；公共GetDescriptorAccess/GetDescriptors/GetSamplerDescriptors返回当前sampler身份/参数，支持非零表偏移和EID0清空。当前backend/bundle `aca98095`、GUI `3ba30e36`、provider `2496f103`：两已有45c2真实capture当前扩展API/CLI PASS，各72 sampler参数/身份查询、48事件/EID0、144纹理身份及48 texture/AS公共查询、五纹理528texels/全部CBV padding/六sampler重定位；三执行点均解析6 sampler且无unknown。41坏组82无GPUwait拒绝+3合法控制，10旧capture20 API/CLI通过；真实编译七sampler程序在六项表之外的消费明确报sampler7/unknown1并拒绝，没有运行该坏程序的native调度。318完成日志无严格诊断、28源码/四产品与复现输入检查点外盘。中间工具SD生命周期错误及纯CBV第二调度误拒绝已修复复验并保留FAIL；未证明历史Qt/UE崩溃/重启修复。B544未关闭，当前无fresh Native/capture、官方/full/lifecycle/Qt-ARC/UE整帧GPU验收，不继承旧库PASS，两flagsfalse；下一完整buffer/其他AIR依赖及真实runtime PSO闭包→固定库集中验收→UE整帧。内盘约32GiB，持续active，无提交推送/重置。

sample/gather的当前sampler来源和参数已在两真实capture定向验证；未证明全部buffer/其他intrinsic/动态索引或UE整帧。两生产flags仍false，持续目标和最终启用门槛不变。


2026-10-07 [B544 buffer 实际消费与纹理尺寸查询](BATCH544_API_RT_INTEGRATION.md)：保持同一大批次，新增 AIR 全局 buffer load/store/atomic 依赖报告、实际 thread-grid 与 CBV scalar 的保守地址范围、当前 typed buffer descriptor 查询及尺寸 intrinsic 资源依赖；参照DX12/VK资源+offset/当前descriptor，未加UE/Lumen特判。当前 backend/bundle `ec73a2f1`：两 fresh native/capture 后的当前 API/CLI 通过，Private placement buffer 528uint/12公共buffer查询、五纹理各528texels/144身份/48公共descriptor/72sampler查询，均48事件往返/EID0；真实三消费点各14buffer读，raw另1写，9/11texture和6sampler且无unknown。58坏组116无GPUwait拒绝+3合法控制、10旧capture20API/CLI通过；456日志无严格诊断，28源码/四产品外盘检查点。首次构建/工具编译/80-byte工具重建/证明脚本旧假设FAIL保留并修正，未计PASS。B544/PHASE59和RT启用验收仍开放，两flagsfalse；当前库官方/full/lifecycle/Qt-ARC/UE整帧GPU未跑，不能证明历史崩溃/重启修复。优先完成通用runtime PSO/实际资源依赖/动态Header闭包再固定库集中验收及UE；新产物外盘，内盘约32GiB，无提交推送/重置。

详见同一B544的“实际 buffer 消费与 metadata intrinsic”结果表和外盘 `development/buffer-consumer-current-integration-manifest.json`；保持大批次开发定向检查，最终集中验收尚未完成。


2026-10-07 [B544 typed buffer SRV/UAV 集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，参照DX12 typed buffer/VK texel buffer，支持底层Metal Private buffer texture当前parent/view、非零offset/format/range、完整parent初态及读写依赖；捕获侧parent查询加锁，view写使parent后续scalar证明失效，parent读写/CBV/AS输入别名拒绝，无UE/Lumen特判。当前 backend/bundle `9fefb511`：Private standalone与Tracked placement两fresh Native/capture/API/CLI PASS，各实际texel读/write/width查询、4253/4253/0/0、528uint、48事件往返/EID0、24公共typed buffer descriptor/location检查、完整父buffer padding/只读SRV/六sampler重定位。75坏组150无GPUwait拒绝+3合法控制，另已生存parent读写重叠反例在通用闭包line825拒绝/原始控制通过；10旧capture20与两已有ec73 capture4 API/CLI通过。两实际AIR/三消费点12texture/13buffer读/6sampler无unknown，各42scalar事实；576日志无严格诊断，30源码/四产品检查点与全部新产物外盘。构建/捕获parent映射/工具AIR旧假设FAIL保留，不计PASS。B544/PHASE59未关闭，官方/full/lifecycle/Qt-ARC/UE整帧当前库未验，两生产flagsfalse；下一通用runtime PSO/实际heap依赖和动态Header，固定版本集中回归再UE实际截帧验收。内盘约32GiB，无提交推送/重置。

详细范围、失败、当前hash及逐项结果见B544“通用 typed buffer SRV/UAV 集成”和外盘 `development/texel-buffer-current-integration-manifest.json`；尚未进入固定最终版集中启用验收。


## 通用 AS Header 发布与实际 UE 无 GPU 加载（继续同一 B544）

按用户调整后的通用API大批次规则推进，未另开微批次。对照仓库DX12/VK的descriptor source identity、GPU VA/resource+offset和提交快照方式，将Metal公有64-byte AS Header的记录/重定位与shader消费资格分开：coverage65的初态/帧内Header、slot kind3的已验证Header source、AS build/fill前置与CPU提交快照都不再要求测试query PSO声明。完整tuple、保留字零值、Captured AS ID/贡献GPU VA、Shared Header storage/range、无alias身份、frame birth及提交先后仍验证；kind3 shader实际消费仍需原有完整支持闭包。保留compute而移除PSO声明的控制继续在GPU前拒绝。

独立重放实际发现提交CPU快照用旧PSO条件选择Header overlay，导致捕获的AS ID/VA覆盖已重定位Header；现按m_RayASHeaderCurrent中的真实Header buffer身份选择，不把普通descriptor backing当不可变Header。实际UE首次native无GPU加载另暴露贡献allocation为64KiB、旧publication校验硬限16KiB；公有Header只发布GPU pointer，现按贡献identity、storage、无alias、offset/至少4-byte范围与溢出验证，consumer另行证明实际读范围/完整初态，未放宽未知GPU producer或消费资格。

当前backend及bundle SHA256 `23c61b9ae4196577b9fee4ab72f9d2f9217adb09c1deed443cc2ad96346a8bb0`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。32源码/四产品SHA备份 `checkpoint-header-publication-23c61b9a/manifest.json`，汇总 `development/header-publication-current-integration-manifest.json`，B544链接均指向 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`。构建0（保留既有GUI SDK warning），没有提交/推送/重置/覆盖用户改动。

| 当前开发结果（外盘development） | 实际通过/拒绝/未运行范围 |
| --- | --- |
| `header-independent-large-contribution/manifest.json` | 两派生capture的Header-only API及3-loop CLI通过：从真实已捕获样本去掉query/root/heap角色声明及所有compute encoder工作，保留真实AS build/原输入清零/提交；另将贡献完整初态与allocation扩到64KiB、padding=0xcd。每项1 AS build、0 compute dispatch、28事件前后往返/EID0、Header ID/VA改变且稳定、保留字零、贡献完整内容及padding不变。不是当前fresh native/capture，也不冒充独立原生场景。 |
| 同一manifest 10坏组 | reserved-word、AS identity、贡献identity、Header offset/length/Private、贡献offset、source alias、birth-after-write、unqualified-consumer：20 API4/CLI1拒绝，均无GPUwait。最后一例保留实际query compute并移除声明，证明公有Header发布解耦没有授予未知shader消费权限。 |
| `header-publication-consumer-regression/development-regression-manifest.json` | 四已有capture当前8 API/CLI通过：两Private standalone/Tracked placement texel SRV/UAV（4253/4253、各48事件/EID0与24 typed公共API检查）、五texture multi-UAV（2375→2687、48事件/EID0/144身份及48descriptor查询）、已有B512 TraceRay。只计当前定向重放，不计当前fresh native/capture或完整旧回归。 |
| `typed-header-runtime-current-CPU/manifest.json` | c0ba中间库CPU-only coverage65接受真实UE `403f90ea8c23638b8c817701725e31885c7929c09fac169a99e7728a66ac092b`，无loading/GPU；不可当成当前23c库完整验收。 |
| `typed-header-runtime-pre-submit/manifest.json` | c0ba中间库native无GPU加载在初态DeclareRayASHeader因64KiB贡献allocation被旧16KiB条件拒绝；峰值RSS1,540,292,608bytes；失败保留。 |
| `typed-header-runtime-large-contribution-pre-submit/manifest.json` | 当前23c库metadata/加载已越过三初态Header及其64KiB贡献pointer/完整初态，ValidateDescriptorFrame在MTLHeap::newBuffer(offset)、streamOffset32128拒绝。exit4是诊断/拒绝退出，accepted=false；无GPUwait、无initial upload/frame GPU，峰值RSS1,574,125,568bytes、45秒/3GiB RSS上限未触发。不能计UE输出或replay通过。 |
| `typed-header-runtime-large-contribution-pre-submit/allocation-inventory.json` | CPU离线67 heap=1,521,585,152bytes、6 standalone buffer=23,068,928bytes，共1,544,654,080bytes物理backing；1142 heap buffer/7 heap AS不重复累加，buffer texture/view不当作独立allocation。zip原始payload1,138,534,078bytes。库存不含原生driver/compiler开销；本机16GiB，运行前memory_pressure -Q报告free69%。 |

92当前完成日志无Assertion/ForceCrash/overrun/resource-map残留等严格诊断；构建/GPU/诊断均沿共享锁串行且已退出。开发工具两个构建失败（使用旧OpenCapture签名、Message()链接缺DoStringise）分别保留first/api-corrected目录；link-corrected/diagnostics中的API6明确是上述提交快照遗漏重定位，后续修复通过，未改写旧FAIL。此前901c/c0ba中间manifest也保留各自hash，不冒充当前23c验收。

下一实际缺口是placement buffer frame closure。按当前序列化32-byte header/64对齐离线推算，offset32128对应chunk29781，Private Tracked heap3138/buffer14712、length229376、offset1662976，与背景buffer14617 length65536 offset1792000重叠，可能触及旧DescriptorPlacementAliasLimit=128KiB；这是待补原生定位诊断确认的候选，不把简单提高预算当作完成生命周期证明。此前口头Shared描述已更正，不把该拒绝认定为Shared。之后继续真实runtime PSO/当前heap/root/提交writer依赖；禁止引擎、Lumen、pass、shader名称权限分支。

当前库官方sample集中回归/full IR/RT/frame-family/controller lifecycle/Qt-ARC、UE initial GPU上传/整帧输出/事件定位/EID0/资源绑定/总体GPU预算均未跑，Qt/UE崩溃/系统重启修复未证明。两supportsRaytracing生产flags仍false，B544/PHASE59开放；固定功能集成后的最终库集中相关验收，再执行短UE真实光追截帧。所有新产物/检查点直接外盘，内盘约32GiB/外盘1.6TiB，保留capture/失败/系统证据，仅删可重建缓存。

## B544 通用 buffer alias / 无窗口 capture / child-device 生命周期开发证据（2026-10-07）

继续用户规定的大粒度B544/PHASE59；本节是开发集成检查，不新开批次，不关闭最终RT验收。Lumen仅是实际验收目标，代码没有UE、Lumen、pass或shader名称权限分支。参考本仓库DX12 `d3d12_device.cpp:2995/3052`、VK `vk_core.cpp:2887/3002`允许无swapchain/backbuffer的EndCaptureFrame及条件thumbnail；参考DX12 `d3d12_replay.cpp:1995`、VK `vk_replay.cpp:2990`区分当前descriptor store资源保持与实际shader descriptor access。

通用Metal实现：coverage65 placement buffer physical alias的旧/新allocation预算单独设为1MiB，不扩大texture的128KiB预算；仍验证Tracked heap、native footprint/alignment/range、birth与写入/提交顺序、retired descriptor来源及AS冻结证明。背景和帧内buffer的物理重叠GPU writer使旧内容/scalar事实失效，未知GPU producer不继承旧初态。本次仍按整个allocation保守失效，尚未实现partial known-copy到alias的scalar区间传播。真正无窗口capture可无drawable保存，显式窗口仍要求对应backbuffer；pending enqueue reservation仍拒绝。capture child ObjC代理保持owner device直到child清理结束，device销毁前释放内部frame capture record，支持空typed表中的普通资源操作；没有授予未证明shader/RT消费资格。

当前backend/bundle SHA256 `63ef3240d9c5df8ac8fba990da0daa1663febfb44f2d51afa6c75b9823ea5e7e`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建0、diff-check0；39源码/四产品SHA检查点 `checkpoint-placement-buffer-63ef3240/manifest.json`，汇总 `development/placement-buffer-current-integration-manifest.json`。根为 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544路径保留symlink；构建/GPU/pre-submit均共享锁串行执行且已退出。

| 当前库实际证据（外盘development） | 结果与范围 |
| --- | --- |
| `placement-buffer-current-63ef3240/manifest.json` | 两fresh Native/capture/API/CLI、229376与1048576-byte新Private Tracked alias叠在旧1MiB背景buffer；每项48事件前后往返/EID0、完整A/B/readback物理重叠值与未写padding、CopyDst usage通过。无drawable、无descriptor表、无RT dispatch，不能冒充RT输出验收。8坏组16 API4/CLI1拒绝且无GPUwait：frame/old超1MiB、unaligned、outside heap、duplicate birth、birth after writer、Untracked、copy越界。 |
| `placement-buffer-owner-current-63ef3240/manifest.json` | fresh 1MiB Native/capture/API/CLI/export通过；StartFrameCapture之前释放应用device引用，queue/heap/buffer的device identity相同，48事件/EID0和干净退出通过。 |
| `placement-buffer-lifecycle/manifest.json` | 同一process两坏capture拒绝后再完整打开有效headless capture、读取初态list、核验prefix/overlap输出及EID0/action/EID0、关闭controller，10轮（20拒绝+10正常打开）PASS，Resident growth9,027,584bytes（64MiB上限）。不是完整所有RT/controller生命周期矩阵。 |
| `placement-buffer-fresh-RT/manifest.json` | 当前native源码重建，复用已审查固定converted metallib/reflection；fresh Native/capture及API/3-loop CLI通过，真实RayQuery4253/4253/0/0、528uint、Private placement typed texel SRV/UAV、纹理尺寸/采样、六根/帧内Private CBV、64几何BLAS及Private TLAS、1/132/0间接query、48事件/EID0与24公共typed descriptor查询。不是重新下载/重新编译官方sample，也不是UE调度。 |
| `placement-buffer-consumer-regression/development-regression-manifest.json` | 四已有capture当前8 API/CLI：Private/placement texel、multi-UAV2375→2687、B512 TraceRay；只计当前定向重放，不能计fresh capture/全量旧回归。 |
| `placement-buffer-UE-submission-diagnostic/manifest.json` | 实际UEcapture403f90ea已越过旧229376-byte alias阻塞，在MTLComputeCommandEncoder::dispatchThreadgroups offset246400/encoder14984/pipeline3731/table25/slot27264拒绝；exit4 accepted=false，无initial uploads/frame GPU/GPUwait/严格诊断，峰值RSS1,575,649,280bytes，45秒/3GiB限制未触发。普通PSO AIR审计未发现ray调用，但含CBV动态descriptor索引与buffer store；绑定全局表中的AS slot并非实际AS消费。不能直接删除资格检查后执行未知GPU地址。 |

118当前完成日志无Assertion/ForceCrash/overrun/resource-map/record残留等严格诊断。保留新复现的capture结束后Buffer dealloc→ResourceManager释放→owner失效两份native .ips及SHA（`placement-buffer-crash-evidence/manifest.json`）；新fixture从capture首次discard、owner悬空、internal frame record残留、empty table误拒绝到修复的FAIL目录及首compile失败全部保留。旧876a开发通过仅归其hash，不计当前库验收。用户历史Qt/UE ForceCrash及系统重启尚未由本复现证明修复。

下一继续同一B544通用runtime PSO绑定与实际资源消费闭包：分开全局typed heap/AS Header发布与shader实际AS消费，按真实AIR、immutable runtime根ABI、当前descriptor来源、GPU writer/提交/资源生命周期证明普通动态buffer/texture读写及RT消费；仅删除AS guard或根据shader名称/无ray字符串放行均不接受。未知地址、GPU producer和不支持能力仍拒绝。相关功能集成后固定最终库集中官方sample/full IR/RT/frame-family/Qt-ARC/生命周期与损坏回归，再短UE真实光追截帧、输出/重放/事件/EID0/资源绑定/总体GPU预算。当前这些最终项目及UE initial GPU上传/frame执行均未运行，两supportsRaytracing生产flags仍false；AS内部、RT shader单步/RT Pixel History仍不承诺。

全部新增capture、失败、构建/回放日志、native/replay helper、系统证据、检查点直接外盘；内盘约32GiB，外盘约1.6TiB。先前迁移219目录17.3GiB及34 frozen目录1.18GiB/58 cache清理证据继续保留，本轮未产生需要再迁移的本地测试目录，未删除capture/故障证据或主构建。没有提交、推送、重置或覆盖用户RDHeaderView改动。

## B544 通用 runtime PSO 实际 descriptor/buffer 消费（2026-10-07）

继续同一大批次开发，没有新建微批次或关闭PHASE59。参考本仓库DX12 `d3d12_replay.cpp:1995`的当前PSO/static descriptor access/bindless feedback与 `d3d12_command_list_wrap.cpp` root CBV已知resource+offset、VK `vk_replay.cpp:2990`当前pipeline descriptor access及普通command提交/资源保持。Metal的runtime元数据只提供根布局/绑定点/线程组；shader访问由实际捕获AIR与当前typed slot/根来源、提交时CBV字节建立，不使用UE/Lumen/pass/shader名称权限分支，不假造测试query PSO声明。

新增普通runtime路径在完整frame preflight中冻结根ABI、inline pointer来源及当前slot快照，在各command commit顺序使用背景完整初态、捕获CPU提交快照与已证明GPU blit字节计算实际buffer地址依赖。实际读写需当前typed来源、native allocation/range和SRV/UAV角色；静态sampler来源受根table及count边界约束。对真实buffer/texture writer及其parent/Tracked物理alias使旧scalar事实失效；resource write residency是权限，不能把未消费/未写CBV当作GPU改写。未知GPU输入、地址、调用和不完整资源消费仍拒绝。AS slot可被全局表保持但不能因此授予shader消费权限；runtime RT消费仍需后续AS/Header/完整消费闭包，本路径对真实query reset继续拒绝。新增AIR unsigned icmp/known select只选择已知条件的typed分支，原始inttoptr sentinel/unknown条件不变成合法指针；修正struct GEP贪婪解析把动态索引错认作base的问题。没有运行或解释RT shader输出，没有增加AS内部查看/RT单步/RT Pixel History。

当前backend/bundle SHA256 `f68bb0be738f4081227cd9b8249143a4c0ea63d7d3a4df43b58d0fdde61da405`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建0、相关diff-check0；43源码/四产品校验检查点 `checkpoint-runtime-consumer-f68bb0be/manifest.json`，汇总 `development/runtime-consumer-current-integration-manifest.json`。所有产物位于 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544路径仍为symlink。构建及GPU/pre-submit验证共用独占锁、没有重叠启动UE/qrenderdoc。

| 当前库实际证据（development） | 实际结果与范围 |
| --- | --- |
| `runtime-consumer-RT-boundary/manifest.json` | 自有MIT compiled MSL场景，准确声明与实际native两个buffer参数/16-byte roots一致的runtime ABI（不是官方sample、不是IR converter输出、不是shader访问声明）。fresh Native/capture/API/CLI PASS，背景真实BLAS/TLAS及未消费公有AS Header/slot；两次Shared→Private CBV offset16上传，第一dispatch动态选A[3]=123，第二选B[7]=456，完整64uint/每buffer未写padding、CBV padding和48事件前后往返/EID0通过。write residency包含只读CBV和两输出，实际消费仍分别只有当前目标。无RT dispatch。 |
| 同一manifest + `runtime-public-query/manifest.json` | 原API20次GetDescriptorAccess检查；随后同一真实capture用扩展helper额外20次access/GetDescriptors/GetDescriptorLocations联合核验，输出A/B身份、ReadWriteBuffer类别、offset0/size256/RW location与48事件/EID0全部通过，AS未出现在实际access中。扩展helper不是新native capture，不把两组查询数量合并冒充更多事件。 |
| 同一manifest 9坏组 | 18 API4/CLI1拒绝、均无GPUwait：unknown-index、AS-as-buffer、write-outside、readonly-UAV、missing-ABI、missing-root、missing-source、unknown-GPU-input、unqualified-runtime-ray。最后一例替换为已验证真实RayQuery metallib/entry main，保留相同runtime布局元数据；日志valid=1、AS=1/unknownAS=1、unknownCall=8，完整frame在commit前拒绝，未执行错误根/原始GPU指针的RT shader。该库SHA20263b11a1dda2a42560ab772cc7e0bb2d44c0291bc16daeeb7f6aaafe648924。 |
| 同一manifest CPU checks | 既有AIR parser检查和新增known compare/select合法typed pointer、raw sentinel、unknown predicate、unknown external call检查通过；真实compiled MSL动态struct GEP由fresh捕获/重放覆盖。不是完整AIR/LLVM解释器。 |
| `runtime-consumer-fresh-RT/manifest.json` | 当前native源码重建，复用已有固定converted metallib/reflection；fresh Native/capture/API/CLI通过，真实RayQuery4253/4253/0/0、528uint、64几何BLAS/Private TLAS、GPU/Private输入冻结清零、typed texel SRV/UAV、CBV/六sampler、1/132/0间接query、48事件/EID0及24 typed公共查询。此场景仍使用已有完整显式query支持闭包，不代表实际UE runtime RT consumer已支持。 |
| `runtime-consumer-regression/development-regression-manifest.json` | 四已有capture当前8 API/CLI：Private/placement texel、multi-UAV2375→2687、B512 TraceRay；只计定向旧capture重放，不计fresh/full最终验收。 |
| `runtime-consumer-UE-pre-submit/manifest.json` | 实际UEcapture403f90ea当前通用验证在首个PSO437 dispatch offset32320/首提交commit42432拒绝：valid=1/read9/write3/unknownBuffer7、AS/texture/sampler/unknownCall均0。首shader的phi循环、三buffer按输入索引scatter/copy尚未有地址闭包；此前普通非AS dispatch没有完整runtime消费证明，不能宣称已越过旧3731阻塞。exit4、accepted=false，无initial uploads/frame GPU/GPUwait、无严格诊断；peak RSS1,580,875,776bytes，45秒/3GiB上限未触发。不是UE replay/output PASS。 |

108当前完成日志无Assertion/ForceCrash/overrun/resource-map/record残留等严格诊断。保留首次backend编译的ResourceId/bytebuf类型错误、源MSL而非AIR的控制拒绝、compiled MSL动态struct GEP错误、replay工具GetDescriptorAccess旧签名错误及其FAIL日志；后续对应修复/复验完成，未改写旧结果。之前63ef的alias/headless/lifecycle证据只归旧库，当前controller全量/lifecycle并未重跑；历史UE/Qt崩溃及系统重启仍未证明修复。

下一实际任务继续通用buffer输入/producer消费链：按immutable ABI和真实提交时输入，支持能证明边界的循环phi、data-indexed buffer/scatter访问、已知copy经物理alias后的区间事实传播；未知GPU生成索引/地址仍须来源/范围证明。Actual UE PSO437仅用于定位/验证，不在实现中按其ID/名称识别或放行。随后完整runtime texture/sampler/AS Header与真正RT消费闭包，固定最终库集中官方sample/full IR/RT/frame-family/Qt-ARC/lifecycle/损坏回归，再UE真实RT截帧/整帧输出/重放/事件/EID0/绑定/总体GPU预算。当前这些最终项目与UE initial uploads/frame GPU全部未完成，supportsRaytracing及FromRender生产flags仍false，目标active；没有缩小到普通runtime store作为完成目标。

新中间/测试/检查点文件全在外盘，内盘约32GiB、外盘约1.6TiB；无需新增本地大目录迁移，保留capture/故障/系统证据，不删主构建。没有提交/推送/重置或改写用户RDHeaderView改动。

## B544 通用 GPU 拷贝输入链集成开发（2026-10-07）

继续同一B544/PHASE59，开发必要的定向检查，未关闭批次或运行最终集中验收。参照DX12 `d3d12_command_list_wrap.cpp:5193`原生CopyBufferRegion/事件/资源+offset及VK当前descriptor资源与提交语义，Metal的普通buffer copy仍调用Native API；新增输入证明只传播实际已发布字节，不解释shader输出，不按UE/Lumen/pass/entry名授予权限。

Compute输入copy现在在commit按已捕获CPU更新、前序copy和实际shader writer顺序取源；Private/帧内源可通过多级copy获得逐字节known事实，未写区间保持unknown。目标小范围分段copy不覆盖其余padding。源不在CPU事实预算时仅允许已有Shared上传完整证明；已知GPU writer禁止回退静态旧字节。真实writer及物理alias加入失效集合，延迟创建的buffer事实也不能重新seed旧初态；没有扩大Native调度、heap或总内存预算，也没有将未知GPU计算结果当作CPU事实。物理alias已知区间传播与更广循环/RT consumer仍待实现。

当前backend和app内bundle SHA256 `f1988b677fd8001120fc941bf0d754e9db47404e79d6cb1705de4a119a4b312d`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`、provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建0、diff-check0。外盘根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544保持symlink；`checkpoint-runtime-copy-chain-f1988b67/manifest.json`保存43源码/四产品SHA与复现脚本，汇总 `development/runtime-copy-chain-integration-manifest.json`。

| 当前库development证据 | 实际结果/限制 |
| --- | --- |
| `runtime-copy-chain-direct-final/manifest.json` | fresh普通runtime compiled MSL准确根ABI场景Native/capture/API/CLI通过，2实际stores A[3]=123/B[7]=456、全64uint输出/CBV padding、48事件往返/EID0、20当前typed公共access/descriptor/location身份检查。背景真实BLAS/TLAS+未消费AS slot；没有RT dispatch，非官方sample/非IR converter runtime shader。9坏组18 GPU前拒绝。 |
| `runtime-copy-chain-background-final/manifest.json` | fresh背景Private64-byte staging：Shared16字节→Privateoffset32→两段Private CBV offset16/24，各8字节，同command；完整Native/capture/API/CLI及48事件/EID0/20公共查询通过。12坏组24 GPU前拒绝，包括缺producer、未发布范围及源越界。 |
| `runtime-copy-chain-frame-final/manifest.json` | 上述链的Private staging在capture期间创建，无初态字节；只有真实copy发布范围可用于消费。fresh Native/capture/API/CLI、48事件/EID0/20查询、12坏组24 GPU前拒绝通过。 |
| `runtime-copy-chain-separate-final/manifest.json` | 帧内Private staging在前一个command提交，分段Private→Private CBV和consumer在后一个command；两轮CPU上传/不同值。fresh Native/capture/API/CLI、48事件/EID0/20查询、12坏组24 GPU前拒绝通过。 |
| `runtime-copy-chain-opaque-final/manifest.json` | fresh真实native/capture仍输出A[3]=123/B[7]=456；Shared上传→Private staging→实际runtime shader写该staging→Private CBV copy。shader写的字节没有CPU publication证明，API4/CLI1在完整frame GPU前拒绝，无GPUwait。即使原生写回同值，也不能由旧initial/CPU source证明新GPU字节；此项是预期拒绝控制，不计replay/output PASS。 |
| `runtime-copy-chain-fresh-RT/manifest.json` | native当前源码重建，复用已有固定converted metallib/reflection；fresh Native/capture/API/CLI通过，实际RayQuery4253/4253/0/0、528uint、64几何BLAS/Private TLAS、GPU构建输入冻结/清零、Private texel SRV/UAV、帧内CBV/六sampler、间接1/132/0、48事件/EID0和24 typed公共查询。仍属于已有完整显式query闭包，不证明UE runtime RT consumer可用。 |
| `runtime-copy-chain-regression/development-regression-manifest.json` | 四已有capture当前8 API/CLI通过：Private/placement texel、multi-UAV2375→2687、B512 TraceRay；只计当前定向重放，不计fresh/全量集中验收。 |
| `runtime-copy-chain-UE-input-audit.json` | 实际403f90ea UEcapture的CPU初态+首consumer owner14706提交前CPU snapshot检查：Shared WriteCombined buffer4311/2MiB、root17408初态是旧float字节，snapshot16898+526发布后真实参数[4,1,0,2]；临时heap14704 slot0→同buffer16896的96-byte descriptor payload，slot1→17152四索引[860,861,862,863]，slot2→table25目标。明确输入/当前来源，既有逐24-byte producer契约需保留；仅CPU审计，不是Native preflight/UE frame GPU/输出验收。ID/entry仅定位，不进入实现规则。 |

四positive合计192事件/EID0、80公共查询，45坏组90 API4/CLI1无GPUwait拒绝；另一opaque writer fresh控制两次预期GPU前拒绝。404当前完成日志无Assertion/ForceCrash/overrun/resource-map/record残留等严格诊断。早期d2525d6a frame/separate结果仅归旧hash，最终四positive/fresh opaque均用当前f1988b677fd8001120fc941bf0d754e9db47404e79d6cb1705de4a119a4b312d复验。保留 `runtime-copy-chain-regression-name-collision/artifact-note.json`：克隆复现脚本初次输出目录未完全替换，旧目录被当前结果覆盖；已保存混合append日志、从旧汇总保留原f68结果元数据，修正脚本在独立当前目录重跑8项通过，不能声称被覆盖的旧stdout恢复。checkpoint脚本过早读取缺失新目录的错误不计测试FAIL/PASS，最终正确目录与汇总已验证。

下一仍是同一B544通用有界loop phi、实际输入索引/typed descriptor GPU生产消费及动态Header/真正RT consumer闭包。实际首PSO的四次循环及四个24-byte散写是定位证据；按immutable根、当前源身份、已发布输入和准确producer source/destination验证，不能硬编码这个shader/UE效果，也不能跳过7 unknownBuffer后提交。当前未重复未变化的UE pre-submit loop失败；当前库UE初态上传/整帧GPU/输出/事件/EID0/绑定、官方/full IR/RT/frame-family/controller lifecycle/Qt-ARC最终集中验收均未运行。两supportsRaytracing生产flags仍false，历史UE/Qt崩溃和系统重启未证明修复，目标active。

持续任务已保存大批次、通用API、DX12/VK参照及外盘优先规则，当前ACTIVE。所有新产物/检查点直接外盘，检查无本地非symlink Metal测试产物目录，内盘约32GiB/外盘1.6TiB；仅删本轮生成19,066-byte可重建Python cache。此前219目录17.3GiB+34 frozen目录1.18GiB迁移/58 cache清理证据保留。不删除原始capture/故障/系统证据或主构建，不覆盖用户RDHeaderView改动，无提交、推送、重置。

## B544 通用有界循环、逻辑buffer view与真实消费（开发中，acc8b703）

仍在同一B544/PHASE59集成批次；以下是必要定向开发检查，不关闭批次、不替代最终固定版本集中验收。

- 延续DX12 `d3d12_command_list_wrap.cpp:2030` 的root资源+offset、`d3d12_replay.cpp:1195` 的buffer descriptor呈现及 `:1995` 的实际descriptor access，VK `wrappers/vk_cmd_funcs.cpp:3805` 的执行点dynamic offset/descriptor绑定。底层Metal继续原生执行shader和copy；当前增加的是资源来源、提交时输入与地址范围证明，没有模拟shader输出或使用引擎/pass/shader名称放行。
- AIR支持可证明的单block、零起点、step1、整数i32/i64循环（正界限≤65536），`ult/ne`回边或`eq`退出；bound必须独立于phi递归。限定三次分析传播，不执行迭代。实际已发布的readonly整数输入可给出保守min/max，逐字节known、range≤64KiB；默认旧loader不接受ranged load，runtime/public查询显式启用。零count、非单位step、recurrence bound、未知calls/地址和无法证明输入继续拒绝；未宣称一般循环/任意控制流已支持。
- buffer view使用已捕获24-byte descriptor低32位metadata长度与已知source offset；有长度时必须在native allocation内并约束实际消费，legacy unsized保留已有行为。统一wire tags0/1 BufferSRV/UAV、2/3 TypedBufferSRV/UAV、4/5 TextureSRV/UAV、6CBV、7Sampler；对象/source field仍独立验证，buffer写仅UAV角色。普通数据buffer即使另有descriptor lease，也不会按metadata slot误读。tag0/1/2/3有本轮fresh证据；不把tag6公共呈现或所有format/图形stage称为本轮已逐项验收。
- scalar/index输入参与实际writer重叠检查，包含buffer和texture parent/物理alias；shader能改写当前范围时，拒绝依靠旧输入证明后续loop地址。后续真实writer仍使旧字节失效，不重载旧初态。新增counterexample让索引源指向输出，第一次写覆盖后续索引；明确观测`Metal runtime scalar writer overlap`，在GPU前拒绝。另逻辑64-byte descriptor不足以覆盖native256-byte内的index55写，仍拒绝。

当前backend及bundle SHA256均 `acc8b703d59d48f80e5979ca4d56ff9ca5b25b29797c52a3049f605a57bdae56`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建terminal0；45源码/四产品检查点 `build-macos-debug/metal-ray-b544/checkpoint-runtime-loop-acc8b703/manifest.json`。全部新产物在该B544外盘链接下，合并证据 `development/runtime-loop-integration-manifest.json`。

| 范围 | 当前acc8实际结果与限制 |
|---|---|
| `runtime-loop-overlap-guards` / `runtime-loop-separate-types` | 两fresh ordinary Native/capture/API/CLI PASS，直接及Shared→Private→Private跨提交输入，type0/1/2/3；各48事件往返/EID0、20公共access和60 descriptor检查，完整64uint输出、输入/CBV/padding；14+15坏组共58 API4/CLI1无GPUwait拒绝。Native compiled MSL准确runtime ABI，**没有RT调用，不能称为光追sample验收**。 |
| `runtime-loop-fresh-RT` | fresh真实converted RayQuery Native/capture/API/CLI PASS，4253/4253/0/0、528uint、Private64几何/TLAS、typed texel/六sampler、六混合根及间接1→132→0、48事件/EID0及公共查询；复用已审查compiled shader，不是新官方sample或实际UE。 |
| `runtime-loop-regression` | 四已有capture的8 API/CLI PASS：Private/placement typed buffer、multi-UAV2375→2687及B512 TraceRay。是旧capture当前库复验，不称fresh。以上244完成日志严格诊断为空。 |
| `runtime-loop-UE-CPU/proof-final-manifest.json` | 原始捕获的submitted root `[4,1,0,2]`、索引860–863和96-byte payload，actual AIR报告1loop/9reads/3writes/0unknown/0calls；CPU PASS，未执行GPU、未核验输出。 |
| `runtime-loop-UE-buffer-types/manifest.json` | 当前真实capture预检越过first scatter及后续相同compute，停在draw streamOffset283456（encoder15002，render PSO3376；旧diagnostic误印compute pipeline0）。exit4，peak RSS1,601,683,456bytes，45s/3GiB限制未触发，无initial uploads/frame GPU/GPUwait/严格诊断；**REJECTED/INCOMPLETE**，不能计UE重放通过。 |
| `runtime-loop-UE-graphics-AIR/manifest.json` | 同capture的depth-only PSO、vertex main及linked stage-in原始AIR/函数/library hash留证。没有fragment，两函数无query调用，main有visible-function-reference、linked函数读取vertex/instance数据。CPU事实不授予draw执行资格；下一补通用graphics参数/linked function/实际descriptor消费闭包，不能仅凭无ray调用放行未知访问。 |

保留中间失败：`runtime-loop-first` helper enum编译失败；`runtime-loop-native-first` Native/capture正确但公共descriptor size断言失败；`runtime-loop-subrange` dd3f PASS属于旧库；`runtime-loop-UE-range-proof` 35e4在type0旧guard拒绝。`runtime-loop-buffer-types` 当前acc8早期direct12坏组PASS仅补充证据，不与上表重复累计。没有重写旧失败manifest为PASS。

当前生产supportsRaytracing及FromRender仍false；仍缺runtime RT真实AS/Header消费闭包、graphics linked/动态输入、实际UE完整上传/输出/重放/事件/EID0/绑定及最终official/full IR/RT/frame-family/lifecycle/Qt-ARC集中检查。历史Qt/UE崩溃/系统重启未证明修复，不继承旧hash的final PASS。持续目标active，无需用户决策，继续按通用API链集成后固定版本集中验收。

## 2026-10-07 通用 Native graphics linked function 集成检查点（39ff0128；B544继续）

参考仓库DX12 `d3d12_device_wrap.cpp::Serialise_CreateGraphicsPipelineState` 和 VK graphics pipeline/state capture，将实际Native函数及attached linkedFunctions和逐事件绑定身份作为来源；DX12 `d3d12_initstate.cpp` / VK `vk_initstate.cpp` 保留 AS portable build 输入与子资源依赖，Metal继续保存配方而不查看AS内部。AIR是当前保守消费/地址证明的Metal内部实现，不提供RT shader单步或PixelHistory。Native/converted入口只取直接argument列表，忽略嵌套member ordinal；converted追加的shader metadata不能替代argument列表。

- `GetGraphicsAIR`取实际PSO的vertex/fragment函数及attached linked函数（functions/binary/private/groups，受容量约束），拒绝未解析函数/递归/越界/未知地址。Native vertex_id由已发布index bytes及baseVertex推导，instance_id/base_instance来自真实draw；linked函数参数由caller SSA传递，返回aggregate保留整数索引及资源依赖。float数学只产生未知浮点值，不能授予地址许可。
- 复用提交有序typed root/heap、当前Shared创建数据、CPU更新和Private copy证明；创建数据只在未被真实writer/物理alias失效时保存，descriptor地址不作为普通scalar猜测。创建数据副本纳入既有64MiB证明预算。公共GetDescriptorAccess/GetDescriptors/GetDescriptorLocations使用该事件、阶段和管线的已通过实际消费记录，EID0清空，shader replacement不复用旧记录。
- Header snapshot原先在background状态调用manager帧引用，后者直接忽略，导致unused AS heap场景有TLAS配方却没有BLAS初态。快照移入active状态，并标记当前AS配方的primitive children与输入/索引/尺寸依赖；不扩大RT consumer资格。

全部产物根为外盘链接 `build-macos-debug/metal-ray-b544/development/`，构建/GPU使用共享串行锁，无UE/qrenderdoc并发；最终 backend/bundle SHA `39ff012838798c14ac69debb032abaf32235c01da8aa3837b6af5cfa05db0319`。GUI/provider沿用已有产品hash，不把未运行GUI/UE操作计为通过。

| 当前39ff结果 | 证据/范围 |
|---|---|
| CPU PASS | `graphics-integration-checks/CPU.log`：实际位置计算/linked函数返回索引、instance/base subtraction、缺失linked、嵌套ordinal与converted追加metadata。 |
| fresh Native/capture/API/CLI PASS | `graphics-linked-final/manifest.json`：Native linked vertex stage-in，两个indexed draw/baseInstance0与1、unused有效AS Header heap，32事件往返/EID0、每次完整256float depth（first .25/second .75/EID0 1）。公共16实际vertex access及descriptor身份/16-byte view。`graphics-public-locations/manifest.json`补GetDescriptorLocations与EID0无heap access，复用同capture，不能另计fresh/重复累计。普通graphics，不是RT/UE验收。 |
| 6坏组12 GPU前拒绝 PASS | `graphics-linked-final-invalid/manifest.json`：published instance selector越界、index vertex越界、typed inline source缺失、创建/CPU输入缺失、stride越界、detached linked function；API4/CLI1，严格诊断/GPUwait空。 |
| fresh converted RayQuery PASS | `graphics-linked-final-RT/manifest.json`：当前源码Native/capture/API/CLI，4253/4253/0/0、528uint、64几何/实例、Private placement typed buffer output、六sampler/CBV、二维间接1→132→0、48事件/EID0和公共查询。metallib/reflection复用既有转换产品并记hash，没有声称新转换。 |
| fresh普通runtime循环/copy PASS | `graphics-linked-final-loop/manifest.json`：Shared→Private→Private跨提交，48事件/EID0、20 access/60 descriptor、完整输出/只读/padding；17坏组34拒绝含writer overlap、未知input和view length。 |
| 四旧capture8 API/CLI PASS | `graphics-linked-final-regression/development-regression-manifest.json`：Private/placement texel buffer、多UAV、B512 TraceRay。不是fresh capture或最终全回归。 |
| UE REJECTED/INCOMPLETE | `graphics-linked-current-UE-pre-submit/manifest.json`：原始true Lumen HWRT capture403f90ea，45s/3GiB限额内peak1,612,087,296bytes，无initial/frameGPU/GPUwait。旧draw AS资格guard已越过，但实际graphics commit消费报告尚未出现，不能声称UE graphics闭包通过。当前frame MRT renderpass303616、三个新Private placement颜色target512²（RGBA8/RG11B10Float）、Load/UnknownStore及新depth/stencil Load初态/alias语义拒绝。下一核验通用frame attachment Load/DontCare/alias与已发布写入，再证明完整graphics和runtime RT消费闭包。 |

失败证据保留：first/second graphics build编译错误、测试shader保留字、incorrect draw annotation的helper abort、未支持captured verification readback（后移到EndCapture之外，原渲染帧仍完整）、遗漏BLAS初态、public query未实现、AIR converted附加metadata解析回归（`graphics-linked-fresh-RT`/`graphics-linked-regression` FAIL，后续39ff修复复验PASS）。首损坏脚本仅改creation被有效commit CPU snapshot覆盖，实际有效输入加载成功，已修正为同时修改published输入；该早期FAIL不计坏组通过。原失败manifest不重写为PASS。

本轮集成检查通过不等于B544关闭；production supportsRaytracing及FromRender仍false。当前固定库official sample/final full IR/RT/frame-family/lifecycle/Qt-ARC/UE整帧输出、事件/EID0与绑定未跑；不继承旧hash的最终验收。任务active，无需用户决策。历史用户Qt/UE ForceCrash/系统重启未证明已修复。源码/四产品/七文档检查点：`checkpoint-graphics-linked-39ff0128/manifest.json`。


## B544 continuation — native frame attachments and typed pointer publication (2026-10-07)

仍为 B544/PHASE59 大批次中的实际开发检查，固定本轮最后版本集中复验，不是最终 RT enablement。

本地参照：`d3d12_command_list4_wrap.cpp::Serialise_BeginRenderPass` 保留 Preserve/Discard，仅 Clear 执行清除；Vulkan `vk_cmd_funcs.cpp` 保留原 load op。Metal frame-born 新图像首次 Load 可以含未定义像素，不强制先 Clear，不把任意 draw 当成全覆盖初始化证明；原生操作、资源出生/extent/alias/usage、level/slice、PSO 格式、depth/stencil 配对、deferred Store 及预算仍核验。

`metal_descriptor_tables.cpp` 对已有完整初态的普通 uncompressed regular colour 使用格式/comp type/block shape 验证，涵盖实际规范化/浮点家族；保持旧 coverage、cube/volume/packed/int 专用范围。`metal_common.cpp` 补既有 R8/R16 Snorm 的 readback block shape。非法 Load 枚举和未支持 color store options 拒绝。新 MIT 源码 `metal_frame_attachment_load_{capture.mm,replay.cpp,gate.py,invalid.py,cases.h}` 与 `.metal` 以实际 fullscreen draw 完整覆盖 16² MRT 和 D32S8，独立固定 byte oracle。两个提交 first→second/first/last/0 往返，后一次 depth=.75/stencil=42，前一次 .25/7；帧内附件 EID0 无资源，background 首附件 EID0 恢复零初态。

DX12 `d3d12_command_list_wrap.cpp::Serialise_SetComputeRootShaderResourceView` 与 Vulkan `vk_descriptor_funcs.cpp` 的 descriptor address 用资源+offset 重定位，并不在发布地址时消费被指内存。Metal `DeclareRayASHeader` 同样仅发布 AS 身份及贡献指针；去掉 contribution 曾写入即拒绝的整资源门槛。CPU Header 自身 GPU writer、未提交 reader、真实身份/offset/64 bytes/padding/来源仍验证；实际 consumer 的 current contents、范围、别名/提交证明保持。AS 为黑盒，不承诺内部查看、shader debug 或 RT PixelHistory。

当前 backend/bundle SHA256 `08677fd7691535b85a10e6323ee725b87c705c4bc14b803f0ae1ea4d0bc02338`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。最后 build0、相关 diff-check0。根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，所有以下路径相对该根；内部链接保留。

| 当前版本范围 | 证据与结果 |
|---|---|
| Fresh attachments PASS | `development/frame-native-final-attachments/manifest.json`：12 Native/capture/API/CLI，first Load 和 Discard→Load，以及 background R16Unorm/RG8Unorm/R16Float/RG16Float/R32Float/RG32Float/RGBA32Float/R8Snorm/R16Snorm/RG16Unorm；每场景32选择，共384，三个颜色及depth/stencil每plane完整256像素，EID0通过。 |
| Malformed attachments PASS | `frame-native-final-attachment-invalid` 和 `frame-native-final-initial-invalid`：分别19坏组38 API4/CLI1；共38组76，含枚举/options/level/slice/resolve/重复目标/出生/配对/PSO/缺失deferred store；无GPUwait/严格诊断。 |
| Header independent PASS | `frame-native-final-header-invalid/manifest.json`：派生 Header-only/64KiB contribution 两合法控制4 API/CLI；10坏组20 GPU前拒绝，实际 reserved/identity/offset/length/storage/source/birth/unqualified consumer。是派生检查，非 fresh Native Header-only。 |
| Fresh RayQuery PASS | `frame-native-final-RT/manifest.json`：真实 Native/capture/API/CLI，4253/4253/0/0、528uint，Private placement input/output/SRV、64 TLAS/geometry、6CBV/sampler、indirect1→132→0、原helper48事件及公共typed查询。shader/reflection复用并记录hash，不冒称重新conversion或official。 |
| Related old PASS | `frame-native-final-related-regression/development-regression-manifest.json`：21旧capture42 API/CLI；Private/placement texel、多UAV、B512 TraceRay、两compute→frame Load、linked graphics、7 depth/deferred/parallel族各两capture。MRT helper按 CommandBufferBoundary 与 encoder scope分别计数；第二capture独立oracle为186→122（第一122→186）。先前三 scope/oracle失败保留，不算backend通过或丢弃失败。 |
| UE REJECTED/INCOMPLETE | `frame-native-header-identity-UE-pre-submit/manifest.json`：实际 capture403f90ea，三Header发布已越过；PSO3731普通buffer writer已通过实际consumer报告，但PSO1707 dispatch247360 valid1/read8/write1/unknownBuffer3，于command commit557952拒绝。peak1,637,203,968bytes，45s/3GiB上限未触发，无initial/frameGPU/GPUwait/严格诊断。实际graphics commit报告仍未到达，不能计UE graphics/output/replay PASS。 |
| Extended contribution consumer FAIL/UNSUPPORTED | `frame-native-contribution-publication-RT/manifest.json`：新 Native GPU contribution copy→Header→query 原生4253/4253/0/0及capture成功，API4，未跑其CLI；`frame-contribution-copy-rejection-check/manifest.json` 当前 trace确认 dispatch(indirect)11089856 GPU前拒绝、无严格诊断。`frame-native-GPU-contribution-header-only/manifest.json` 派生扩展控制亦API4，未跑余下case，保留FAIL。这些不能因为新指针规则计为支持，下一补底层实际GPU贡献版本/consumer闭包。 |

其他保留的中间结果：`frame-native-load-first` helper编译错误RDCMAX；`frame-native-color-family` Snorm Native/capture成功而 API创建拒绝；diagnostic build失败的 `%u`/size_t 后改 `%zu`；cec5/f925/124f/75ae 中间测试和实际UE303616→387136→396736→412928阻塞定位各留证，其通过不继承到0867。最终汇总 `development/frame-integration-evidence-manifest.json`，校验检查点 `checkpoint-frame-integration-08677fd7/manifest.json`。

下一仍同一大B544：按实际 PSO1707 的3未解析buffer地址与当前输入/producer证明通用地址闭包，之后完整 graphics、真正RT consumer与GPU contribution版本；不能按UE/Lumen/shader名特判，不能用旧初态覆盖未知新GPU数据，不能忽略unknownBuffer提交。保持支持RT生产flags false。当前没有最终official两scene/full IR/full RT/全部frame/lifecycle/Qt-ARC/native能力复验，也没有UE整帧GPU输出、事件/EID0和绑定通过证据；历史ForceCrash/Qt崩溃/系统重启没有证明修复。任务active，无用户阻塞，无自动提交推送。

## B544 continuation — dispatch / scalar / sourced GPU descriptor development and direction correction (2026-10-07)

仍是同一大集成批次，未关闭 B544，未启用生产 RT。采用用户最新“恢复/重定位/提交/展示分离”准则，见 `RT_API_INTEGRATION_WORKFLOW.md`；本节历史展示 guard 的拒绝记录不代表未来必须永久拒绝对应动态访问。

本轮已实现真实 dispatch 的 threadgroup 三维坐标和 local linear index、保守无零除数的 unsigned div/rem 范围；精确无条件 compute 整数 store 的逐字节版本可作为可选当前值事实，CPU/copy/物理 alias/后续未知写入失效，不写回伪造 GPU 输出；raster coverage/条件/原子/未知 store 不发布已知值。稀疏状态计入节点预算。已验证的 GPU descriptor copy 元数据与 typed source 相等时，runtime 可读它的重定位来源；GPU-owned destination 仍由真实复制写入，绝不改为 CPU 上传。上述都是通用资源/地址语义，没有 UE/Lumen/shader名特判。

最后 backend/bundle SHA256 `aeec766b1fe34bb432b992ce4208c13624187f9d107d7ddf1f897e1cd40d5a8d`。产物根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，以下路径相对根；完整检查点 `checkpoint-replay-display-direction-aeec766b/manifest.json`。

| 范围 / 对应版本 | 实际证据 |
|---|---|
| 当前 GPU descriptor Native/capture/replay PASS（aeec） | `development/GPU-descriptor-runtime-native/manifest.json`：初始 heap 两槽为互换对象，真实 GPU copy 后正确写 A[3]=123/B[7]=456，完整 256-byte outputs/padding；48事件/EID0及20公共访问/descriptor点，跨提交 frame Private CBV 拷贝；16派生组32 API4/CLI1于GPU前拒绝，含缺descriptor copy、GPU值与来源不匹配。普通数值变异类旧 guard 的政策仍需重新分类，不能把所有旧拒绝当最终API验收要求。 |
| 前一固定版本集中定向 PASS（ac0d） | `development/invocation-scalar-final-manifest.json`：counter/group/loop 三 fresh Native/capture/API/CLI、144事件/EID0/60公共access点（descriptor共100），41派生组82拒绝；三合法Native/capture但conditional/overwrite/opaque replay被旧guard保守拒绝的控制，非支持PASS；fresh converted RayQuery4253/4253/0/0、528uint、48事件/公共查询，21旧capture42 API/CLI通过。是ac0d版本证据，不自动继承为aeec的全部回归。 |
| 当前 UE REJECTED / INCOMPLETE（aeec） | `development/GPU-descriptor-runtime-UE-pre-submit/manifest.json`，实际403f90ea capture；越过first PSO1707 dispatch247360（unknownBuffer0），第二dispatch248128读取14823/14824时 unknown byte/unknownBuffer1，于commit557952仍拒绝。peak RSS1,635,532,800 bytes，未触发45s/3GiB限制，无initial/frame GPU/wait/严格诊断。真实捕获有4-byte输入copy，64线程中分支只消费有效范围；这是拆展示门槛的具体样本，不能据此宣称UE资源恢复或整帧输出验收完成。 |
| CPU/方向调整 | `development/group-invocation-CPU/post-direction-{build,test}.log` exit0。后续未构建的分支范围扩展已从backend源码撤下，归档 `development/guarded-range-experiment-not-built/manifest.json`。新 `metal_runtime_guarded_index.metal` 及Native fixture参数保留作未来“分析不完整但合法重放”验收，Native/capture/replay尚未运行，不能计通过。 |

保留实际失败：首group fixture tg_size元数据不准确；scalar proof test两次编译错误；首counter跨提交opaque旧门槛拒绝；增加合法槽后unknown-index反例使用旧索引3而得到API0/GPUwait，改为实际越界namespace4；这些失败及正确复验分别保留。诊断 ac0d 指出 GPUexpected 索引槽已有复制来源，aeec补后推进下一dispatch。临时编辑脚本归档后删除 `/tmp` 副本，清理本轮生成的 .pyc，内盘可用约32GiB；源码/用户工程/失败证据不删除，未提交推送/重置。

当前最终 official/full IR/full RT/全部frame/lifecycle/Qt-ARC/native能力判断和 UE整帧GPU输出、真正RT replay、事件/EID0/绑定均未完成，历史崩溃/重启未证明修复。两生产flags false，目标active，无用户阻塞。下一按新的分层准则完成真正的API恢复资格，再用Native sample及UE输出驱动修复，不继续把未知展示转换成新拒绝白名单。

## B544 continuation — sourced buffer restoration versus numerical access display (2026-10-07)

仍在 B544/PHASE59 大批次开发，不关闭批次、不启用光追。按用户最新准则与本地 DX12 `Serialise_Dispatch`、Vulkan `Serialise_vkCmdDispatch`/两者 `GetDescriptorAccess` 将已恢复绑定和可选数值展示开始分开。

`validateRuntimeDispatch` 对已确定资源身份的指针，独立验证已重定位 descriptor/source、逻辑 view、出生/别名、已有初态或 Native 生产者、提交与可写角色；GPU 动态偏移不能由 CPU 展示计算完整时，保留 Native 调度。分支内的静态候选 interval 越界不能直接等同实际执行越界，改为部分展示；descriptor view 本身的错误范围仍拒绝。未知实际写入仍整体使旧 CPU scalar/物理别名事实失效，不用残留 base 猜写入区间，不上传 CPU 模拟 shader 输出。新增独立 `m_IRRuntimeDescriptorAccessComplete`，公共 compute/graphics 查询只返回已识别 descriptor，按当前正确 pipeline 身份选取；不把全 heap 驻留声称为实际访问。现有未知对象/调用/RT-AS qualification 门槛尚未全部拆开，这是首段机制，不能宣称已完成所有独立恢复资格。

新 Native guarded-index fixture 在真实 64 local threads 中仅 q<1 消费 Private GPU-produced counter；候选 counter range/输出偏移的 CPU 分析不完整，Native 调度正确。此轮未再扩充 branch 数值解释器；AIR 仅记录静态候选含控制流，用于标识展示的不确定性及禁止发布条件 store 的已知 bytes。

当前 backend/bundle SHA256 `d17d244ec0a54e4b2190b8d965fb9bba7bc40ecd8ed4fee96f3c28117793186e`；GUI仍 `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。两个构建均0，第二只修当前 compute/graphics pipeline 身份选择；日志分别保留。全部产物根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，以下路径相对根，检查点 `checkpoint-restoration-display-d17d244e/manifest.json`。

| 当前固定版本实际范围 | 结果与证据 |
|---|---|
| Fresh guarded dynamic access PASS | `development/restoration-display-guarded-native/manifest.json`：Native/capture/API/CLI/full outputs与padding通过，48事件前后/往返/EID0、20公共access与descriptor/location点；实际 preflight 两次 `accessDisplay=partial`。9结构/来源/类型/ABI/未资格RT坏组18 API4/CLI1于GPU前拒绝。三旧数值/producer改动控制未跑，并记录到manifest；未将它们伪装为坏capture通过。 |
| Related fresh ordinary dispatch PASS | GPU-descriptor separate-copy 和 scalar-counter两 fresh Native/capture/API/CLI，分别48事件/EID0、20公共查询；15+11坏组52 API/CLI拒绝，含真实descriptor GPU拷贝缺失/来源/字节身份错误。三新场景共144事件/EID0、60公共access/descriptor点，35坏组70拒绝。 |
| Fresh converted RayQuery PASS | `development/restoration-display-RT/manifest.json`：复用并记录真正 converter 的 metallib/reflection，fresh Native/capture/API/CLI；4253/4253/0/0、528uint、48事件往返及typed公共查询，64几何/TLAS、Private placement、6CBV/sampler、原生2D indirect1→132→0。是开发 fixture，不冒称新 official/UE验收。 |
| Related existing capture PASS | `development/restoration-display-related/development-regression-manifest.json`：21旧capture42 API/CLI，texel Private/placement、多UAV、B512 TraceRay、两compute→frame Load、linked graphics及14 depth/deferred/parallel。 |
| Actual UE INCOMPLETE | `development/restoration-display-UE-pre-submit/manifest.json`：真实 capture403f90ea，无initial/frame GPU/GPUwait；仍于PSO1707 dispatch248128、commit557952 unknownBuffer1拒绝，peak RSS1,636,614,144bytes，没有45s/3GiB超限。实际AIR `%65 = select ... %62, inttoptr(i64 1024 ...)` 在输入值未知时丢失已有 pointer identity；该值未知首先来自14823/14824仅4-byte有效copy而CPU候选读256bytes。**这是当前分析的指针来源丢失，尚未证明捕获缺少实际GPU地址恢复，不能当成新feature永久不支持。** 下一补独立 pointer provenance/namespace恢复资格，Native保留原选择与合法实际访问；不通过继续解释分支计算/q范围解决全部API。 |

`development/restoration-display-evidence.json` 保存所有对应库manifest及361日志SHA256，严格诊断0；CPU测试同时核验 unknown offset 保留已知对象、缺对象仍未知、条件store不发布精确值。没有修改生产flags，两者仍false。最终 official/full IR/full RT/全部生命周期/Qt-ARC/native capability 和 UE整帧GPU输出/实际RT replay/events/EID0/绑定仍未完成；历史崩溃/系统重启未证明修复。构建、GPU及UE诊断共享锁且串行，全产物外盘，没有reset/commit/push。

## B544 continuation — pointer provenance, Native call effects and API color attachment extent (2026-10-07)

继续同一 B544/PHASE59 大批次。资源恢复、地址重定位、提交依赖与访问展示保持独立；本轮没有增加 fabs/max/dot/convert 等 shader 数值模拟，也没有 UE/Lumen/pass/函数名支持白名单。实现沿用仓库 DX12/Vulkan 的原生调度和静态/可选 feedback 展示方法。

分支指针保留所有已知资源来源，标量 GEP、结构字段投影、bitcast 与原子/声明的参数内存效果继续携带来源；数值偏移不完整时 Native 调度可执行，展示标记部分，未知写使原 buffer/物理 alias 的旧 scalar 事实失效。未知或 GPU 整数地址分支不被丢弃，不把整数1024当来源。只有 shader 字节码固定常量可作为常量指针分支；独立对照捕获原 GPU buffer VA/范围，命中则要求真正 shader 地址重定位并于 GPU 前拒绝。1024仅是 fixture 常量，**不是 Metal 官方保留地址、不是后端白名单，也没有承诺非法实际访问可执行**。常量经未知/非零 GEP 派生仍不合格。当前正例的真实 Native 分支始终选择有效已恢复对象。

数值调用按实际 module 的精确 `readnone`/`memory(none)` 声明判断无资源效果，仅接受无指针/opaque handle 的数值实参；数值返回值保持未知。`argmemonly` 通过所有显式 pointer 来源记录资源候选，无字节覆盖保证，不能误匹配 `inaccessiblemem_or_argmemonly`。AIR 原子支持实际下划线和点命名形式，只记录资源读写，GPU 执行其计算；不发布 CPU 原子输出。CPU 核验未知/缺来源、两对象分支、GPU整数、零字段投影/派生常量和声明边界。声明语义参照 [LLVM11 LangRef](https://releases.llvm.org/11.0.0/docs/LangRef.html) 与 [LLVM LangRef](https://llvm.org/docs/LangRef.html)。

通用颜色附件范围由 coverage65 的 API 8槽支持贯通 renderpass、普通 draw 和 mesh target/PSO 验证；旧 coverage 兼容范围保持，真实格式、初态/Native producer、出生、重叠、延后 Store 与提交检查不取消。参照 DX12 `Serialise_OMSetRenderTargets` 和 Vulkan `Serialise_vkCmdBeginRendering` 的 API 附件数量/当前资源处理，不按 Lumen GBuffer 数量特判。新 MRT fixture 验证6/8附件、deferred/parallel，每个颜色附件逐像素比较，包含往返与 EID0。

当前固定 backend/bundle SHA256 `2133a76f7d7c81ad6fd9e519802a1f20fcaeb720db567f90adbcea3eca6d2111`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建均串行0，修改后对当前固定库集中运行以下相关验证；本批未关闭，生产两个 RT flags 仍 false。

| 当前固定版本实际验证 | 结果/限制与产物（相对外盘根） |
|---|---|
| Fresh selected-pointer atomic Native/capture/API/CLI PASS | `development/native-pointer-effects-current-native/manifest.json`：Private GPU counter、64 threads/实际分支、数值函数与选中指针真实 atomic exchange，完整输出A[3]=123/B[7]=456及padding；48事件/EID0，20公共access/descriptor/location点，展示partial。10结构/来源/类型/ABI/身份/未资格RT坏组20 API4/CLI1 GPU前拒绝，包含嵌入已捕获VA反例。三旧数值/producer oracle未跑，manifest明确记录；非RT dispatch/非官方sample。 |
| Fresh API MRT extent PASS | `development/native-pointer-effects-MRT-extent-source/manifest.json`：6direct、6deferred、8deferred、8parallel四 Native/capture，各两capture，总8份API/CLI完整颜色/跨pass输出，每份25事件选择含4次EID0，共200。5坏组10 GPU前拒绝：缺第8附件、重复附件、格式交换、缺最后延后Store、越界Store index8。capture/helper SHA在同目录artifact-inventory.json；不是 UE/RT验收。 |
| Fresh converted RayQuery PASS | `development/native-pointer-effects-RT/manifest.json`：真实 converter metallib/reflection复用并记录hash，新 Native/capture/API/CLI输出4253/4253/0/0、528uint，48事件/EID0/24typed公共查询；64几何/TLAS、Private placement、6CBV/sampler、2D indirect1→132→0。既有显式query闭包，不证明UE runtime RT consumer已可用。 |
| Related existing captures PASS | `development/native-pointer-effects-related/development-regression-manifest.json`：21旧capture42 API/CLI，包括Private/placement texel、多UAV、B512 TraceRay、两compute→frame Load、linked graphics和14depth/deferred/parallel。固定当前库的相关集中验证，不冒称full旧回归。 |
| Actual UE INCOMPLETE | `development/native-pointer-effects-MRT-UE-pre-submit/manifest.json`：真实403f90ea capture，无initial/frame GPU/GPUwait，peak RSS1,180,876,800bytes，无45s/3GiB超限和strict诊断。越过1707/3776指针/调用与首commit557952阻塞，越过3233472六颜色附件创建；当前draw3330304在stage2 shader identity为空/可选阶段旧guard拒绝。下一核实 Native PSO/capture字段，区分合法无fragment阶段与捕获身份遗漏，再补相应API语义；不直接将所有缺shader/缺地址放行。UE整帧GPU输出、实际RT consumer、事件/EID0/绑定仍未到达。 |

中间版本有各自真实证据，不能继承为当前库全部PASS：8308指针分支正例通过而UE仍unknownCall15；da0f数值调用声明/原子命名正例通过，UE3776 unknownCall0但unknownBuffer1；472b结构投影+原子指针样例通过、UE无GPU推进至3233472六MRT。所有实际FAIL、CPU编译/旧oracle/stale binary、首Native原子拒绝均保留。首MRT测试使用非默认 `MTLCompileOptions.preprocessorMacros`，该捕获源码编译选项当前仍未支持，Native/capture正确而API5拒绝；改为shader源内define、默认选项后同库完整通过，没有宣称支持该编译选项。runner日志被helper重命名后读文件失败亦保留，修正日志读取，不把它算重放PASS。

`development/native-pointer-effects-evidence.json`记录当前5项manifest、243完成日志SHA与strict0（仅本固定版本列明范围）；检查点 `checkpoint-native-pointer-effects-2133a76f/manifest.json`保存源码/四产品/九文档。全产物仍在 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544路径为symlink；内盘32GiB/外盘1.6TiB，未产生内部测试产物、未删除capture/失败证据，无reset/commit/push。最终official/full IR/full RT/lifecycle/Qt-ARC/native能力与UE整帧验收未运行，历史Qt/UE崩溃和系统重启未证明修复，目标active。

## B544 continuation — optional Native stages and synchronization effects (2026-10-07)

继续同一 B544/PHASE59 大批次；按资源恢复、地址重定位、提交依赖与访问展示四层推进，没有新增 shader 数值/线程组算法模拟，没有引擎/pass/shader-name支持分支。

实际 UE capture `403f90ea8c23638b8c817701725e31885c7929c09fac169a99e7728a66ac092b` 的 PSO3376/3386/3470 明确记录 fragmentFunction=0、有效vertex function；3470同时有R8Unorm颜色和Depth32Float_Stencil8格式/实际pass附件。`development/optional-fragment-UE-records.json`是CPU capture字段证据，不冒称GPU输出。参照本地 DX12 `Serialise_CreateGraphicsPipelineState` 对零bytecode不伪造shader，以及 Vulkan `Serialise_vkCreateGraphicsPipelines` 的实际stage数组，coverage65普通vertex管线允许Native可选fragment阶段，不要求颜色附件为空或有效但未执行的fragment绑定为空。管线/对象/绑定范围与Native创建检查独立保留，缺失vertex/错误shader仍不自动放行。`GetGraphicsAIR`已按明确无fragment返回空入口，不为它生成Native消费任务。

公共 `GetDescriptorAccess`过滤当前没有Native shader的阶段，保留`GetPipelineState`中的有效绑定；驻留/残留binding不是已执行访问。新样例的首轮Native与重放颜色/深度正确，但public fragment access仍存在，API95暴露并修复此展示问题；不把仅Native正确当成公开访问验收通过。生产opaque MetalFX资源展示的既有独立分支保持。

实际compute PSO1624的AIR包含Native wg/simdgroup barrier和atomic fence，旧分类把fence当device-buffer原子、barrier当未知调用。现在按真正Native同步签名区分资源效果与同步效果：原shader/原GPU barrier/fence照常执行，threadgroup内容和数值返回仍未知，不发布CPU线程组输出或旧scalar事实。同步不能代替device-buffer读写/地址来源证明；真正buffer atomic仍记录其源对象的读写并使未知写入事实失效，未知外部调用仍需资源/恢复资格。CPU测试含三类同步及未知调用负例；匹配表达式在入口分析开始时构造，避免逐instruction重复创建。**Fresh Native样例实际执行wg/simdgroup barrier与threadgroup读写、Private GPU counter、真实atomic exchange；AIR atomic fence此轮只有CPU与真实UE无GPU预检证据，UE实际GPU fence尚未验。**

当前固定 backend/bundle SHA256 `f9718c71e8b29410ca5d5c9be05cd0fd79aea7f51010dbf5e2c9ee8103168ff9`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`、provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建/GPU/UE预检共享锁串行，最终库集中执行以下相关验证：

| 当前版本实际范围 | 结果与证据（相对外盘根） |
|---|---|
| Fresh Native同步与动态来源 PASS | `development/native-synchronization-native/manifest.json`：Native/capture/API/CLI完整A[3]=123/B[7]=456及padding；64线程共享数组实际GPU产生索引，wg/simd同步，unknown numerical offset保持来源/展示partial；48事件/EID0和20public access/descriptor/location点。10结构/来源/类型/身份/未资格RT坏组20 API4/CLI1 GPU前拒绝。CPU新同步/原子/条件来源/失效来源测试0；三旧数值oracle未运行并在manifest标明，不算拒绝PASS。实际AIR保存在shader-disassembly.ll。非RT dispatch/非官方sample。 |
| Fresh Native无fragment颜色+深度 PASS | `development/optional-fragment-current-native/manifest.json`：d32 direct、d32 deferred、d32s8 parallel三场景各两capture，总6 Native/capture/API/CLI；独立颜色附件全4像素保持80/64/128/255，深度/模板值、两普通MRT/cross-pass输出/padding、shader身份正确，零fragment public access；各29事件选择含4 EID0，共174选择/24重置。4坏组8 GPU前拒绝：缺颜色、颜色错用深度、缺deferredStore、未执行fragment绑定的非法范围。同目录artifact-inventory.json记录capture/helper SHA。 |
| Fresh converted RayQuery PASS | `development/optional-fragment-RT/manifest.json`：复用已记录真实converter metallib/reflection，新Native/capture/API/CLI4253/4253/0/0、528uint、48事件/EID0/24typed查询，64几何/TLAS、Private placement、六CBV/sampler、2D间接1→132→0。已有显式query闭包，不是UE runtime RT验收。 |
| Related existing captures PASS | `development/optional-fragment-related/development-regression-manifest.json`：29旧capture58 API/CLI，原21texel/multiUAV/B512 TraceRay/frame Load/linked/depth/deferred/parallel，加此前8份6/8 MRT direct/deferred/parallel。是当前固定库的相关集中回归，不冒称完整旧RT/全库。 |
| Actual UE INCOMPLETE | `development/native-synchronization-UE-pre-submit/manifest.json`：API4、无initial/frame GPU/GPUwait、peak RSS1,163,837,440bytes，未超45s/3GiB，无strict诊断。越过draw3330304的旧fragment guard，推进commit3517632；PSO1624/dispatch265216从unknownCall35/unknownBuffer19变为unknownCall0/unknownBuffer4（read12/write1）。实际AIR是已知descriptor来源/null选择后动态GEP的四个读；当前derived literal-null资格拒绝，尚未证明有缺失GPU指针。下一独立处理这些来源和真正地址字段的恢复需求，**不模拟GPU共享数组、prefix/max或输入数值来消除unknown**。shader输出、实际RT消费、UE整帧/事件/EID0/绑定仍未验。 |

保留首次public API `GetDescriptorAccess`签名编译错误与修正、首API95 ghost access失败、中间b417 Native正例/UE同步拒绝的独立哈希范围；不将中间通过或当前UE拒绝改写成PASS。`development/native-synchronization-next-pointer-source.json`记录下一实际机制缺口。未知派生null读的拒绝不是新的永久不支持feature结论；恢复需求要独立核实。

`development/optional-stages-synchronization-evidence.json`保存当前5项manifest、251日志SHA与strict0（仅列明范围），检查点 `checkpoint-optional-stages-synchronization-f9718c71/manifest.json`保存64源码、四产品、九文档及复现脚本。所有产物外盘 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544保持symlink，内盘32GiB/外盘1.6TiB，无reset/commit/push、无用户文件覆盖。最终official/full IR/full RT/lifecycle/Qt-ARC/native能力与UE整帧未验，历史Qt/UE崩溃和系统重启未证明修复；两生产能力false，目标active，本大批次未关闭。

## B544 continuation — conditional sources, literal relocation and object metadata (2026-10-07)

同一B544/PHASE59大批次继续，按用户要求独立处理资源恢复、地址重定位、提交依赖与访问展示。参照本地DX12 `Serialise_Dispatch` 的原GPU调度/`GetDescriptorAccess` 的可选动态反馈和Vulkan静态/反馈访问展示，不能把CPU数值不完整等同资源丢失。没有CPU threadgroup/SIMD/prefix算法模拟，也没有引擎、pass、shader名或1024数值白名单。

已恢复buffer/null分支后的GPU相对索引保留资源身份；null本身不成为恢复对象。缺失真实source、opaque指针、typed GPU地址当数值索引的scalar/struct GEP仍拒绝。旧CPU range/loop断言改为检查来源与未知范围，不借数值模拟消除unknown。未知literal算术没有授予通用GPU地址恢复。

固定shader字面量及派生memory byte span独立匹配捕获GPU身份/真实资源范围，包含跨入资源起始位置的访问。真实捕获VA需要重定位；普通bytecode常量保持Native语义。literal alternatives在CPU已知predicate选择后仍保留并沿GEP投影，不能借CPU分支绕过旧VA检查。GPU指针数据没有变成数值启发式或伪造GPU输出。

Native纹理尺寸/level查询读取对象信息，不要求像素初态；preflight帧对象从已验证factory记录取尺寸/格式，Native对象仍在原事件出生。pixel读取仍需初态/producer。Native无参且实际声明readonly/inaccessiblememonly的内部调用没有捕获资源输入，执行原GPU调用，opaque返回仍unknown；外部/写入/带参数或可访问资源调用不按此授予来源，返回handle不能伪装buffer。真实pixel读取负例中的隐式read sampler验证该副作用分类，没有CPU构建sampler或模拟返回。

固定backend/bundle SHA256 `529d0a7b11e5afa462bb83eba0154e4387e999394b95a31e93e2d9cb73ac2183`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`、provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建/GPU/UE预检共享锁串行，当前库集中验证：

| 当前实际范围 | 结果/产物（相对外盘根） |
|---|---|
| Fresh Native三种机制集成 PASS | `development/texture-metadata-native/manifest.json`：Private counter→64线程共享数组/原barrier→nullable相对读→条件atomic/连续四写，A[3..6]=123..126、B[7..10]=456..459，全64uint/每buffer和CBV padding、48事件往返/EID0、20公开查询点/40descriptor-location检查。frame-born Private placement R32Uint 8x8无纹理初态/无pixel read，GPU width/height决定真实写入；非RT dispatch/非官方sample。15坏组30 API4/CLI1 GPU前拒绝，含base/derived/spanning捕获VA、缺dimension source/错CBV role、真实pixel读取缺初态；后者明确命中 `Metal runtime texture contents unavailable: resource=ResourceId::44 pixelRead=1`，无GPUwait。三旧数值controls未跑，不算拒绝PASS。实际两份AIR/capture/helper哈希见该目录。 |
| Fresh converted RayQuery PASS | `development/texture-metadata-RT/manifest.json`：复用已记录converter metallib/reflection，新Native/capture/API/CLI4253/4253/0/0、528uint、48事件/EID0/24typed查询，64几何/TLAS、Private placement、六CBV/sampler、2D间接1→132→0；已有显式query闭包，不是UE runtime RT验收。 |
| Fresh Native可选fragment PASS | `development/texture-metadata-fragment-native/manifest.json`：d32 direct/deferred、d32s8 parallel共6 fresh capture，174选择/24 EID0、颜色/深度/模板/MRT/padding与零不存在fragment阶段access；4坏组8 GPU前拒绝。 |
| Related existing captures PASS | `development/texture-metadata-related/development-regression-manifest.json`：29旧capture58 API/CLI，Private/placement texel、multi-UAV、B512 TraceRay、frame Load、linked graphics、depth/deferred/parallel及6/8 MRT；不是全库/full RT/full IR。 |
| Actual UE INCOMPLETE | `development/texture-metadata-UE-pre-submit/manifest.json`：复用实际capture `403f90ea8c23638b8c817701725e31885c7929c09fac169a99e7728a66ac092b`，API4、peak RSS1,419,739,136bytes，未超45s/3GiB，无initial/frame GPU/GPUwait/strict诊断。越过1624 nullable读、1625连续写及3830尺寸消费，commit3517632仍未通过；下一3836/dispatch274240 unknownBuffer0、unknownCall10。实际AIR为SIMD ballot4/broadcast-first2/is-first2和texture-buffer fetch-add2；需Native副作用、texture atomics真实读写/初态/提交依赖，不能模拟lane结果或只删unknownCall，再runtime AS/Header闭包。 |

`development/texture-metadata-records-next-effects.json`保存实际factory/AIR与下一调用统计，是CPU审计，不是UE GPU输出。`development/provenance-metadata-evidence.json`保存5项当前manifest、285完成日志SHA/strict0（仅列明范围）；`checkpoint-provenance-metadata-529d0a7b/manifest.json`保存64源码、四产品、九文档。全部产物外盘 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`；内盘32GiB/外盘1.6TiB，无reset/commit/push、未覆盖用户改动。

中间CPU旧assert、Native7 oracle/fixture、MSL/API编译、standalone budget、future metadata与未命中pixel guard失败均保留，详见failures_preserved。standalone frame texture constructor未实现，不以placement成功覆盖。最终official/full IR/full RT/lifecycle/Qt-ARC/native能力及UE整帧输出/事件/EID0/绑定未跑；原Qt/UE崩溃和重启未证明修复。两生产flags false，目标active，B544/PHASE59尚未关闭。

## B544 continuation — Native SIMD, texture atomics and readback (2026-10-07)

同一B544/PHASE59大批次继续。资源恢复、地址重定位、提交依赖、shader访问展示独立；动态偏移和Native SIMD/原子数值由真实GPU执行，展示partial不等于恢复失败。没有CPU lane/atomic结果模拟或UE/Lumen/pass/PSO名称/ID支持分支。参照本地DX12 Serialise_Dispatch/Serialise_CopyTextureRegion（d3d12_command_list_wrap.cpp:5289、Native调用5320–5330）及Vulkan Serialise_vkCmdCopyImageToBuffer（vk_draw_funcs.cpp:1884、Native调用1908起）的原生执行、事件和资源角色，访问反馈展示仍独立。

实际声明convergent且无device-pointer的Native SIMD按寄存器效果执行，返回数值保持unknown；非convergent、外部及pointer参数不获来源。pointer phi与select保留全部来源候选，missing/opaque/cyclic来源不猜测，null不成为资源。buffer/texture原子区分store写、load读、RMW读写；load/RMW验证先前像素初态或真实producer和提交关系，store不要求旧像素。帧内Private texture writer通过已验证factory记录保留，不因Native对象尚未出生丢失提交依赖。

通用texture→buffer预检核验源初态或同提交/已提交producer、对象生命周期/alias、buffer范围、slice/mip/region/row/image pitch及16MiB聚合copy预算。Native与帧factory共用布局/overflow判断，真实transfer不变，不创建对象/像素或CPU shader输出；写回清除CPU旧字节/alias事实。已有BC1/BC5和普通颜色范围保持，depth/其它packed/compressed范围未扩大。更广buffer-backed texture producer/alias和同dispatch初始化尚未完整证明。

固定backend/bundle SHA256 8d6ec636db55ff213f58f774d118d70672627472a62c8634aeab89ee06d380a0，GUI 3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91，provider 2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619。构建0、CPU0、diff-check0。构建/GPU/UE共享锁串行；UE只45秒/3GiB预检，无initial uploads/frame GPU。

| 当前固定库范围 | 实际结果/产物（development下） |
|---|---|
| Fresh Native同/跨提交链 PASS | native-lanes-atomic-complete、native-lanes-atomic-separate-complete各manifest：Native/capture/API/CLI，frame-born Private placement R32Uint 8x8无像素初态，atomic_store产生0x12345+i，atomic_load驱动nullable相对buffer读，atomic_fetch_add最终0x12346+i；真实SIMD ballot/broadcast-first/is-first和threadgroup/simdgroup barrier、buffer atomic exchange/四连续写A[3..6]=123..126/B[7..10]=456..459。全64uint/CBV/counter padding、64纹理像素、texture→buffer offset32/row256/2112-byte全部padding。各72事件往返/EID0、20公共点/40descriptor-location。各19坏组、共76 API4/CLI1 GPU前拒绝，含缺producer/readonly atomic/readback越界/短row/缺submit、base/derived/spanning捕获VA。缺producer实际命中pixel contents guard，无GPUwait；非RT dispatch/非官方sample。 |
| Fresh metadata-only PASS | native-lanes-metadata-regression/manifest.json：width/height无像素初态仍Native/capture/API/CLI通过，48事件/EID0/20公共点/40descriptor-location；15坏组30 GPU前拒绝，真实pixel读缺初态仍命中contents guard。三旧数值controls未跑并显式记录，不计拒绝PASS。 |
| Fresh converted RayQuery PASS | native-lanes-atomic-RT/manifest.json：复用记录的converter metallib/reflection，新Native/capture/API/CLI4253/4253/0/0、528uint、48事件/EID0/24typed查询，64几何/TLAS/Private placement、六CBV/sampler、间接1→132→0；已有显式query闭包，不是UE runtime RT验收或新官方下载。 |
| Fresh可选fragment PASS | native-lanes-atomic-fragment-native/manifest.json：d32 direct/deferred、d32s8 parallel共6 fresh，174选择/24 EID0、颜色/深度/模板/MRT/padding、零不存在fragment access；4坏组8 GPU前拒绝。 |
| Related captures PASS | native-lanes-atomic-related/development-regression-manifest.json：29旧capture58 API/CLI，Private/placement texel、multiUAV、B512 TraceRay、frame Load、linked graphics、depth/deferred/parallel、6/8 MRT；不是全库/full RT/full IR。 |
| Actual UE INCOMPLETE | native-lanes-atomic-UE-pre-submit/manifest.json：actual capture403f90ea、API4、peak1,275,609,088bytes，45s/3GiB未触发，无initial/frame GPU/GPUwait/strict。越过3836/274240 SIMD和texture-buffer atomics，unknownBuffer/unknownCall0、accessDisplay partial；下一3853/277440是buffer-texture get_width+write，unknownBuffer0/unknownCall0，commit3517632仍拒绝。核实对象metadata/format/写角色/alias/lifetime，唯一拒绝分支尚待诊断，不能继续CPU数值模拟。 |

三fresh普通链合计192事件/EID0、60公共点/120descriptor-location，相关57坏组114 API4/CLI1 GPU前拒绝。native-lanes-atomic-artifact-inventory.json保存两capture/真实AIR/helper哈希。native-lanes-atomic-next-view-audit.json是CPU审计：owner提交前publication将4311/offset24064旧initial[2992368640,64,0,0]改为[1306,2160,4,16]；heap25 slot31344→14847，factory MTLBuffer::newTextureWithDescriptor。ID仅定位，不成为实现规则；不能用编码时旧initial证明提交时GPU输入，不是UE输出PASS。

native-lanes-atomic-evidence.json汇总7manifest、565scoped完成日志SHA/strict0，仅列明范围；中间CPU/MSL/API/layout/negative schema失败保留（failures_preserved），中间positive不计完整组PASS。checkpoint-native-lanes-atomic-8d6ec636/manifest.json保存65源码/四产品/九文档与复现脚本。产物外盘 /Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544，内部B544 symlink；内盘31GiB、外盘1.6TiB。仅清理精确可重建cache，不删原capture/故障、主构建、用户工程；无reset/commit/push/RDHeaderView覆盖。

下一继续通用frame buffer-texture view恢复、初态/producer/alias、runtime AS/Header/namespace和必要GPU地址恢复，再UE真正RT整帧/输出/事件/EID0/绑定及最终官方/full IR/full RT/lifecycle/Qt-ARC/native能力集中验收。最终项目未完成，standalone frame texture constructor仍不完整；两supportsRaytracing生产flagsfalse，历史Qt/UE崩溃/系统重启未证明修复。目标active，B544/PHASE59未关闭，持续任务四层准则仍有效。

## B544 continuation — buffer-texture views and current copy state (2026-10-07)

同一B544/PHASE59大批次继续，用户四层准则持续有效。参照仓库 Vulkan vk_resource_funcs.cpp::Serialise_vkCreateBufferView（2387起）与 DX12 d3d12_device_wrap.cpp::CreateUnorderedAccessView（1517起）的 Native 对象/格式/资源+offset描述，恢复 buffer-texture factory 元数据和真实视图范围；不是 UE/Lumen/shader/PSO名称或ID适配。Native动态数值保持GPU执行，partial访问展示不等于资源未恢复。

帧内MTLBuffer::newTextureWithDescriptor通过已有factory/格式/layout/资源出生/生命周期资格后，preflight可读其width/height/format而不提前创建Native对象。texture-buffer写与CPU地址输入只核验逻辑texel字节范围及物理heap alias；同一backing的根参数与像素范围不重叠可以正常重放，实际重叠仍拒绝。Native已有视图与帧factory共用逻辑范围判断。

当前scalar字节有效性按提交顺序维护：真实opaque writer清除dense/sparse事实，并阻止旧初态重新seed；后续已验证CPU上传或Native copy可重新建立当前精确字节。历史opaque标记不再永久排除后续正确拷贝，copy源也使用当前有效字节而不回退旧初态。未知GPU字节仍不变成CPU数值或捕获地址来源。独立restoration intervals记录完整初态、当前CPU上传与copy链的内容覆盖，不依赖CPU是否知道像素数值；copy先清除目标被替换范围再传播源覆盖，缺初始化的源不继承目标旧像素。仅有16-byte根参数上传不能证明offset256/256-byte texel像素已恢复。更广GPU buffer producer与动态heap namespace仍需补真实恢复，不能将本次保守拒绝永久作为纯展示失败的规则。

固定backend/bundle SHA256 4163d391ea5486af9f3d7e9ff9f8bb4a46b8c498662b07c8fd7be2823bb1594f，GUI 3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91，provider 2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619。构建0、CPU0、diff-check0。构建/GPU共享锁串行，UE只45秒/3GiB无initial/frameGPU预检。

| 固定当前库检查 | 实际证据（development下） |
|---|---|
| Two fresh view groups PASS | frame-texel-view-final-bounded、frame-texel-view-final-shared-root各manifest，Native/capture/API/CLI真实编译MSL，Private placement 1024-byte backing/R32Uint 64 texels、offset256/row256。两次完整64像素0x12346+i→0x12347+i和1024-byte padding/根参数，Native SIMD/条件来源及A/B四连续写保持GPU执行；各48事件往返/EID0、20 public点/40descriptor-location。Shared-root根16..32与像素256..512，真实逐thread先read(local)再write(local)，不制造CPU shader结果；移除1024-byte像素上传而保留16-byte根上传，实际命中pixel contents guard，无GPUwait。各19/20坏组，共78 API4/CLI1 GPU前拒绝，含readonly/格式/offset/alias/缺source、真实根与view重叠、捕获VA base/derived/spanning。frame view backing本身未直接在EID0查询，既有输出/根/counter重置已验证，不扩大该scope。 |
| Two fresh atomics groups PASS | frame-texel-view-final-atomic-complete、frame-texel-view-final-atomic-submit，原同/跨提交Native store→load/RMW、8x8纹理、SIMD及2112-byte texture→buffer像素/padding各72事件；各19坏组/共76 GPU前拒绝。与两view组共240事件/EID0、80公共点/160descriptor-location和77坏组154拒绝。 |
| Unknown writer control, not final supported scope | frame-texel-view-final-opaque/manifest.json：Native/capture正确，未知shader writer→copy后消费仍API4/CLI1 GPU前拒绝，未回退旧scalar初态。动态heap selector/指针namespace资格后续需独立补恢复，不能将CPU数值未知本身永久定为拒绝条件，也不计该控制为功能已支持。 |
| Fresh actual RayQuery PASS | frame-texel-view-final-RT，复用已记录converted shader/reflection，新Native/capture/API/CLI4253/4253/0/0、528uint、48事件/EID0/24typed查询、64几何/TLAS/六CBV/sampler/Private placement/间接1→132→0；不是新下载官方sample或UE runtime RT验收。 |
| Related capture regression PASS | frame-texel-view-final-related/development-regression-manifest.json，29旧capture/58 API/CLI，Private/placement texel、multiUAV、B512 TraceRay、frame Load、linked/depth/deferred/parallel、6/8 MRT。不是full IR/full RT/full库验收；旧8d库fresh可选fragment不继承为本库fresh结果。 |
| Actual UE remains INCOMPLETE | frame-texel-view-final-UE-pre-submit，actual403f90ea/API4，peak1,421,934,592bytes，45s/3GiB未触发，无initial/frameGPU/GPUwait/strict。已越过3853/277440 frame buffer-texture metadata/write资格，下一3854/278528，commit3517632仍拒绝；多个已有background GPU descriptor被当frame producer缺来源，另新Private14851/14852输入数值未知。下一先区分已验证/重定位的background GPU初态与frame实际producer/address namespace，不能增加CPU shader数值表达式来替代。 |

frame-texel-view-next-descriptor-audit.json是CPU provenance审计：heap25/slot31320最后background event3与捕获初态24bytes逐字节一致，typed source14156；初态校验/overlay已有PatchDescriptorSlot。ID仅定位证据，不成为支持分支；未证明frame GPU重放。需要核实当前slot身份/生命周期/namespace和Private producer链，不能将gpuExpected历史标记与缺真实GPU指针混为一谈。

frame-texel-view-evidence.json汇总8manifest、695scoped完成日志SHA/strict0，保留早期Native/MSL/API/oracle失败。新unknown-index从捕获实际bindings选择真正缺slot，不因新增合法buffer/texture槽而人为扩大拒绝；三旧数值控制仍未跑并显式记录，不计PASS。frame-texel-view-final-grouped-manifest.json保留首次atomic测试oracle失败；corrected continuation和两完整atomic目录才计whole-group PASS。ea10中间range组和早期005139e6/6dcd views各保留自己的库哈希，不计当前最终验收。

checkpoint-frame-texel-view-4163d391/manifest.json保存65源码/四产品/九文档与复现脚本，证据及产物全外盘 /Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544；内盘31GiB、外盘1.6TiB，无reset/commit/push、未覆盖RDHeaderView用户改动。B544未关闭、目标active，两生产supportsRaytracing仍false。UE真正RT整帧输出、事件/EID0/绑定及最终官方/full IR/full RT/lifecycle/Qt-ARC/native capability集中验收尚未完成；standalone frame texture和更广GPU namespace仍未完整支持，历史崩溃/重启未证明修复。

## Generic resource recovery first group (2026-10-07)

本组机制整改和所列相关回归已完成；B544/PHASE59整批及光追启用仍未完成。优先通用 API 机制的任务调整已写入根 AGENTS.md、持续任务 metal 和 [GENERIC_API_RECOVERY.md](GENERIC_API_RECOVERY.md)，覆盖历史按场景逐项增加许可策略。UE/Lumen只是验收负载。

实际修改：`ValidDescriptorFrameTexture` 依已支持 block/plane 格式族、类型与所有 mip/slice 计算恢复布局；`ProjectDescriptorFrameTextureView` 和 Native subset-view factory 使用同一范围投影；single-sample placement factory 共用布局。移除旧 R16/packed/array/volume/尺寸/usage 组合与 mip-view 特批，以及 allowGPUOptimizedContents=true/identity swizzle 强制条件。统一 staging/table/plain-copy 和 frame preflight 记账预算替代 coverage 容量/2或256 command/4或128 dispatch 许可；Native footprint、总 GPU 工作量、API范围/溢出/生存期/初态/alias/真实生产者检查保留。128MiB记账+每chunk4096预留是保守策略估算，不是精确RSS界限，不仅扩大数字。

Native argument-buffer 实际调度保存声明 stride（含16-byte）、真正 inline/root/table/typed slot 快照；Native metadata不误当 converted IR byte-count。生产者来自实际 shader/clear/提交关系，useResource(Write)不单独制造初态。Native source-compiled library 无捕获 AIR 时资格走独立恢复/提交检查，原 shader 运行，动态纹理 access 标 unknown；直接API buffer绑定仍可展示。只按本次绑定table收集可能 opaque效果，不把驻留heap所有资源伪造为实际访问。shader CPU数值未被模拟。

逐函数 DX12/Vulkan 证据见通用文档：vkCreateImage/vkCreateImageView完整CreateInfo/Native创建；Prepare/Apply_InitialState bound-memory/subresource/undefined策略；D3D12 Apply_InitialState footprints/mip/array/plane；GetRefIDs/GetResIDFromAddr的resource+offset；ExecuteCommandLists DataUploadSync及跨queue同步、vkQueueSubmit等待；GetDescriptorAccess与NativeDispatch独立。Metal采用相同对象/子资源/提交闭包，额外处理不可平移的texture gpuResourceID和typed argument-buffer字段。无引擎/pass/shader名称、PSOhash或固定EID生产准入。

| 结果 / 范围 | 真实证据 |
|---|---|
| PASS 通用 native 预编译 library | `generic-resource-final-native-accepted/manifest.json`，六变体：R16/R32Float，769×17/513×129/1025×9，4/5mip及多mip view，非零placement偏移，19层parent取base3/count9子视图，132实际dispatch；各native/两capture/API四次完整像素与mip/layer/PickPixel/nativeID往返/EID0、CLI loops3。 |
| PASS Native JIT / 展示未知 | `generic-resource-native-JIT-validated/manifest.json`，同六变体以newLibraryWithSource真实运行，无捕获AIR；完整输出和往返oracle不放宽，动态texture公开access不伪造。每组16种损坏capture来源/producer/format/type/usage/mip/view/offset/range均API4/CLI1且无GPUwait。源/MSL/metallib provenance附加核验不改变已运行checks。 |
| PASS GPU 初态 / 动态 Native sampler | `generic-resource-final-dynamic-sampler/manifest.json`，真实initial/frame GPU descriptor、合法同表copy、动态sampler heap、Native SIMD/纹理atomics与独立提交；两capture/72事件/EID0、输出/padding、28坏组GPU前拒绝。普通Native负载，不以其结果替代RT验收。 |
| PASS 部分 texel 初态 / 共用 root backing | `generic-resource-final-partial-texel/manifest.json`，真实16-byte初始像素前缀，其余先未定义后真实GPU生产，合法不重叠root/view共用backing；两capture/48事件/EID0，26坏组GPU前拒绝。 |
| PASS fresh converted RayQuery | `generic-resource-final-RT/manifest.json`，Native/capture/API/CLI真实4253/4253/0/0和528uint，48事件/EID0/24typed查询，Private placement64几何/TLAS/六CBV/sampler/间接1→132→0；复用既有转换shader/reflection并记录hash，不是新官方sample或整套RT。 |
| PASS related regression | `generic-resource-final-related/development-regression-manifest.json`，29旧capture/58 API与CLI：Private/placement texel、multiUAV、TraceRay、frame compute→Load、linked、depth/D16/D32/D32S8、deferred/parallel与6/8MRT。不是全量回归。 |
| UE REJECTED / INCOMPLETE | `generic-resource-UE-pre-submit/manifest.json`，原capture403f90ea，45s/3GiB监督下禁用initial/frameGPU；peak1448443904bytes，未触发监督停止，无GPUwait/严格诊断。越过旧调度数量限制，后续PSO3906/offset297152有4 unresolvedCalls，原commit4502144 API4。ID仅用于证据定位；下一先判明调用的资源/地址/生产者恢复与可选展示缺口，不按该名称或ID放行。 |

固定backend/bundle SHA256 `d907eec86fdbddf3c646a708cf890d5a045379b958c01d9d1b43e1177141ad45`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。`generic-resource-evidence.json`保存七manifest及697个所列完成日志SHA，strict诊断0，不把UE supervised driver exit0当UE成功。

开发失败保留：首轮missing-producer错误接受；stride16被漏记、Native metadata误用IR长度和write residency伪造初态的定向正/负失败，修正后producer控制GPU前拒绝；array19 Native fixture物理budget不足退出3，改sample真实footprint预算后fresh完成；旧related首次Native source-compiled AIR/validModule拒绝；Native JIT第一次oracle错误要求全部公开绑定为空退出14，改为动态texture未知且直接API绑定仍允许，重新完整验收。中间编译FAIL/旧候选e46证据各保留自身hash，不覆盖为当前PASS。

剩余分类：API/设备及范围、Native heap alignment/extent/sample支持持续校验；8192²/depth256/array128/logical128MiB是当前初态/读回统一实现/预算范围，不是设备最大值。其他coverage族分支、standalone frame texture、不同格式reinterpretation、nested view、MSAA descriptor恢复未全部改造，不继续增添许可等级。2DArray→Cube投影尚未实现；非identity swizzle本组仅删历史限制，完整fresh native验收未跑。非空AIR的未知资源namespace/call仍需独立恢复，不能直接全部删除拒绝。

未运行：UE整帧真实RT输出/事件往返/绑定验收、当前库最终官方sample、full IR/full RT/lifecycle/Qt-ARC/最终native capability集中验收、三个历史数值展示坏控制。生产两supportsRaytracing保持false，目标active。历史Qt/ForceCrash/死机重启不因本组scope声称全部修复。

中间文件与可恢复源码/文档/最终四产品检查点均在外盘 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`；内盘31GiB、外盘1.6TiB。无reset/commit/push，未编辑用户RDHeaderView。本组稳定后回到UE暴露的通用调用/namespace/提交恢复，不继续按Lumen功能新增许可。

## Native API effects and resource-specific dependency recovery (2026-10-07)

仍属B544/PHASE59较大通用能力批次；未完成光追启用。接续首组纹理/view/预算整改，本轮修改两处真实机制，不按UE功能、pass/shader名、PSOhash或固定EID决定资格。

- Native register-only bit API的完整声明signature可确认资源效果，即使旧AIR省略readnone。`RegisterBitIntrinsicDeclaration` 按return/input类型、operation family和可选控制参数分类；clz/ctz/popcount/reverse_bits的scalar/vector prototype与未知外部调用、pointer参数和错类型区分。结果不计算、不产生CPU索引/地址事实。CPU测试以已知数值输入仍确认动态offset/返回未知，以及真实source identity保留；错误signature/外部call仍unknown。Native实际验证重点是uint32 clz/popcount驱动GPU索引，其他宽度/vector仅做signature CPU检查，不宣称全部Native验收。
- 通过actual typed slots、对象/offset、初态、真实producer和提交闭包恢复资源；AIR效果分类不替代这些检查。Native effect classification/prototype表为固定缓存，非bit声明快速排除；不缓存shader数值结果或生存期状态。
- CPU-only UE采样日志实际显示普通resource依赖记账在`noteResource`反复扫描全m_DescriptorSlotShadow。按其已有resource+offset有序key使用lower_bound，仅遍历该resource字段；保留live/data、borrowed/consumer、真实source及backing/alias/command引用行为。未跳过本resource字段，没有把全heap当实际shader访问。没有扩监督时限或放松预算。

### 具体DX12/Vulkan对照及Metal差异

`WrappedID3D12Device::Serialise_CreateComputePipelineState`（d3d12_device_wrap.cpp:853）保存完整PSO/shader bytes/root signature，安排Native编译及派生资源；`WrappedVulkan::Serialise_vkCreateComputePipelines`（wrappers/vk_shader_funcs.cpp:1072）保存CreateInfo、layout/module与Native deferred handle。`WrappedID3D12GraphicsCommandList::Serialise_Dispatch`（d3d12_command_list_wrap.cpp:3530）和`WrappedVulkan::Serialise_vkCmdDispatch`（wrappers/vk_draw_funcs.cpp:1201）保留原Native调度和维度；不要求CPU计算shader bit值。Metal原PSO/library/Native dispatch继续执行，只补缺AIR effect metadata的API分类，GPU结果不被CPU替代。

`D3D12Replay::GetDescriptorAccess`（d3d12_replay.cpp:1995）结合static和valid dynamic feedback，并依据本次bound heaps定位；`VulkanReplay::GetDescriptorAccess`（vk_replay.cpp:2990）从实际compute/graphics descriptor sets映射storage，独立展示。Metal partial/unknown和恢复资格继续独立，不因bit数值未知产生伪造访问。

`D3D12Descriptor::GetRefIDs`（d3d12_manager.cpp:555）按type取ResourceId，CBV由`GetResIDFromAddr`（d3d12_resources.h:1536）解析对象+offset。`D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp:1914/texture分支2130+）按真实mip/array/plane与CopyableFootprints恢复；Vulkan`Prepare_InitialState`/`Apply_InitialState`按bound memory/subresource/undefined策略。Metal保留typed GPU identity和真正初态，texture句柄需额外gpuResourceID Patch而不能当可平移VA；本轮不修改或绕过这些来源/初态检查。

`WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists`（d3d12_command_queue_wrap.cpp:440/904–958）对动态写及本command的bound descriptor ranges获取对象引用，然后AddResourceReferences；先DataUploadSync及跨queue同步。`WrappedVulkan::Serialise_vkQueueSubmit`（wrappers/vk_queue_funcs.cpp:1283/1300+）处理原command与等待/跨queue关系。Metal复用按实际对象/表范围记录引用，保留原encoder/command/committed/producer集合以及view parent/alias检查；范围索引替代无效的跨resource全表扫描，不改变所需依赖。

### 实际验证与限制

| 结果 / 范围 | 证据与实际行为 |
|---|---|
| Baseline modern PASS / legacy FAIL | `register-effects-baseline`在旧d907库modern声明Native/capture/API/CLI通过；`register-effects-legacy-baseline`由相同独立Native fixture重组AIR，仅修改register declarations的effect属性、保持输入module指令/资源metadata，Native/capture完整输出通过，旧库API4/unknownCall2且GPU前拒绝。不是仅合成CPU报告。 |
| PASS 最终Native API effect集成 | `resource-dependency-final-dynamic-sampler-legacy`、`resource-dependency-final-partial-texel-legacy`，新鲜legacy library/Native/capture/API/CLI；Native GPU生成索引clz、SIMD、实际texture atomics、动态sampler、GPU-owned descriptor初态/合法同表copy与部分texel初态/共用root backing。两capture各72/48事件/EID0、20公开access与40location、完整buffer/pixel/padding；28/26坏组各API4/CLI1且GPU前拒绝，包括真实来源/namespace、captured VA、初态、producer、只读、alias边界。 |
| PASS 最终通用恢复变体 | `resource-dependency-final-generic`、`resource-dependency-final-JIT`各六Native变体、两capture/完整mip/layer像素/PickPixel/nativeID/四次往返/EID0/CLI；R16/R32浮点多尺寸/offset/mip/view、19层subview、132实际dispatch。JIT无AIR动态纹理展示unknown；两组16坏capture各GPU前拒绝。不将前一d907结果继承为当前库，实际重新跑。 |
| PASS fresh RT / related regression | `resource-dependency-final-RT`真RayQuery4253/4253/0/0与528uint、48事件/EID0，复用有provenance的converted shader/reflection；`resource-dependency-final-related`29旧capture58API/CLI含TraceRay、texel/multiUAV/NativeCompute→Load/linked/depth/deferred/parallel/MRT。不是full IR/full RT或官方最终验收。 |
| UE INCOMPLETE, bounded preflight completed | `resource-dependency-final-UE-pre-submit`仍用原403f90ea capture，禁用initial/frameGPU、45s/3GiB监督。约25s到API4而非超时，无GPUwait/strict，peak1623212032bytes；越过PSO3906/offset297152原4未知调用、后续graphics及更多提交。当前offset1584704/PSO3993是同一buffer2511读0…5044与写64…5108交叠，commit4981696拒绝。ID/名称仅定位证据；不因此按名称放行。 |

`register-effects-validated-UE-pre-submit`（2f3396）和`register-effects-stable-UE-pre-submit`（295be53）均timeout/-9、不计PASS。声明prototype缓存单独并未让UE完成；`register-effects-CPU-profile`短时CPU采样揭示资源依赖全表扫描，峰值1471643648bytes/无GPUwait，仍监督停止。最终范围索引后的约25s API4才是当前结果，未冒称整个性能问题或历史死机已经修复。CPU新增测试首次名称冲突编译FAIL保留，改测试变量后重跑，未覆盖旧报告。

最终backend/bundle SHA256 `9e44a610f61f82d98f1f2aedfacf21f73ddc18b088f17e6a35405c8ee2f1d1ee`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。`resource-dependency-evidence.json`保存七manifest和711所列完成日志SHA、strict0。`resource-dependency-final-related-checks-manifest.json`的UE driver exit0仅代表监督诊断运行正常，不是整帧重放通过。当前库两生产supportsRaytracing保持false。

下一批：`resource-dependency-next-inout-audit`CPU-only提取实际library2576/entry、buffer birth，保存库/AIR哈希，无GPU/native/replay验收。ScalarRead的CPU值当前用于可选数值区间/namespace推断，重放应允许已恢复的合法读写资源；但同dispatch写入可能使旧事实失效。先独立Native sample验证失效事实、地址字段/对象namespace及真实producer范围，重新建立保守恢复证明/partial展示；不能直接删除overlap检查或借旧initial数值来选GPU资源。剩余coverage族、standalone/更多view/更广pointer恢复仍需通用机制整改。

未运行/未完成：UE整帧真实RT输出/重放/事件往返/绑定，当前库最终官方sample、full IR/full RT/lifecycle/Qt-ARC/native capability集中门槛，三历史数值坏控制，非identity swizzle/更广view独立组。目标active、B544/PHASE59未关闭，无commit/push/reset；未修改RDHeaderView用户代码。源码/四最终产品/执行文档/CPU profile/失败与下一CPU审计检查点全部外盘，内盘31GiB、外盘1.6TiB。所有构建与GPU测试串行共享锁。

## B544 可变输入事实与 Native 本地内存效果（2026-10-07）

继续同一“通用资源恢复与重定位”能力批次，不新增微批次/场景许可。当前稳定backend与bundle SHA256 `7bb1d5605c788d7d08e77ce2e40dd88681fd81c5e2ec784f1d75ec66dc4b5765`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。构建0、diff-check0；生产supportsRaytracing及FromRender仍false，完整启用目标active。

实际修改 `ValidateDescriptorFrame` 的runtime dispatch机制：从已发布字节得到的CPU scalar事实在同dispatch有实际buffer/texture writer重叠时失效，删除对应提交proof并重新分析；typed slot、对象+offset及地址重定位独立保留。未知GPU相对偏移允许partial展示；重新分析失去descriptor/AS/pointer来源时仍拒绝，不沿用旧数值选择对象。known writer范围按逻辑buffer/texel区间与物理heap alias相交，保留同allocation内互不重叠的root/像素；unknown writer范围保守覆盖allocation。单调失效迭代的8轮是统一CPU分析预算，不按scene/PSO/EID许可；本组没有增大buffer/texture/work预算。Native indexed draw的真实index输入同时被写仍需额外恢复/顺序证明，保留拒绝。

`UniformResourceAccess` 现在把实际LLVM atomicrmw/cmpxchg及已识别Native local atomic API的addrspace(3)操作与device资源效果分开；出现其他/未知指针时不借此放行。原shader负责本地分配、原子、barrier和数值结果，CPU结果保持unknown，不推导shader输出。独立CPU检查确认本地atomic结果不能证明动态目标offset；device atomic缺来源继续拒绝。没有用UE功能、pass/entry名、PSO hash或固定EID决定生产支持，文中IDs仅审计定位。

每项修改的仓库对照：DX12 `WrappedID3D12GraphicsCommandList::Serialise_Dispatch`（d3d12_command_list_wrap.cpp:3530）及VK `WrappedVulkan::Serialise_vkCmdDispatch`（vk_draw_funcs.cpp:1201）捕获原维度并调用Native Dispatch，不在CPU执行线程组分配或shader数值。DX12 `D3D12Replay::GetDescriptorAccess`（d3d12_replay.cpp:1995）当前PSO/static/有效动态反馈与VK `VulkanReplay::GetDescriptorAccess`（vk_replay.cpp:2990）当前pipeline/descriptorSet展示独立；Metal采用同一分离方式，额外需typed argument-buffer字段及不可平移gpuResourceID来源恢复。`Serialise_CreateComputePipelineState`（d3d12_device_wrap.cpp:853）/`Serialise_vkCreateComputePipelines`（vk_shader_funcs.cpp:1072）捕获完整PSO/shader与原生重建，本组不修改捕获shader指令以“适配UE”。初态参考 `D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp:1914）资源/descriptor heap恢复与 `WrappedVulkan::Apply_InitialState`（vk_initstate.cpp:1835）descriptor写入及bound资源初态；Metal仍使用真实初态/已提交producer，不把失效数值当新GPU内容。地址参考 `D3D12Descriptor::GetRefIDs`（d3d12_manager.cpp:555）及 `WrappedID3D12Resource::GetResIDFromAddr`（d3d12_resources.h:1536）的resource+offset，VK恢复实际descriptor对象/handle；Metal仍需要实物typed来源，不猜整数地址。提交参考 `Serialise_ExecuteCommandLists`（d3d12_command_queue_wrap.cpp:440及904）DataUploadSync/跨队列同步、当前绑定descriptor引用，与 `Serialise_vkQueueSubmit`（vk_queue_funcs.cpp:1283）原command/等待和跨queue同步；Metal继续同command/committed producer、opaque写版本及alias闭包，驻留write不是producer。

| 当前库实际证据（外盘development） | 结果及真实范围 |
|---|---|
| `mutable-local-final-zero` / `mutable-local-final-offset32` | 自有MIT独立Native MSL，实际同invocation读改写、dynamic atomic目标及threadgroup allocator原子。fresh Native/capture/API/3-loop CLI；A[3]=124/B[7]=457与偏移+32的A[35]=124/B[39]=457，完整64uint/未写padding、两次Private CBV上传、每capture48事件往返/EID0、20公开access/descriptor/location通过。输入初态移除、缺source/root/ABI、原始/派生captured VA、AS-as-buffer、只读等各13坏组API4/CLI1且GPU前拒绝。Native调用结果不做CPU模拟；不是官方sample/UE/RT dispatch。 |
| `mutable-local-final-partial-texel` | fresh GPU-owned descriptor初态、实际合法同表非重叠copy、Private texel view与共享root backing、只有16-byte像素初态且其余未定义到真正GPU writer、Native SIMD及legacy register声明。完整buffer/pixel/padding、48事件/EID0和20公开access/40descriptor-location通过；26坏组GPU前拒绝。故意把view移到root区间使根事实失效，重分析后unknown descriptor namespace明确拒绝；不是删除overlap检查后GPU运行。 |
| `mutable-local-final-RT` | 当前native源码重建，复用有provenance的converted shader/reflection；fresh Native/capture/API/CLI，真实RayQuery4253/4253/0/0、528uint、64几何/Private TLAS、Private/帧GPU输入、48事件/EID0及typed公共查询通过。不是新官方下载/full RT或UE runtime AS消费支持。 |
| `mutable-local-final-related` | 29已有capture58 API/CLI通过：texel/multi-UAV、TraceRay、NativeCompute→Load、linked/depth/deferred/parallel/六八MRT。只计当前定向旧capture重放，不计fresh/全量启用验收。 |
| `mutable-local-final-UE-pre-submit` | 原403f90ea真实UEcapture；禁用initial上传/frameGPU，45s/3GiB监督，约28秒到API4、无GPUwait/strict，peak1406369792bytes。旧3993 scalar读写及4007本地atomic拒绝已越过；当前3246400/4065真实AIR含allocate/reset/next/commit/get intersection query，AS1/unknownAS1/unknownBuffer2/unknownCall10，commit4981696仍拒绝。监督driver0只表示诊断正常完成，不代表replay或RT输出PASS。 |

失败和中间证据保留：旧9e44 Native合法读改写可捕获但API3/旧scalar overlap拒绝（CLI未跑）；初版只在unknown offset目标读改写的baseline本来就PASS，不能当复现FAIL。6914中间库positive只归其hash。d7de候选三positive/freshRT/29旧capture通过，UE到4007 addrspace(3)误归外部buffer；本地atomic独立fixtureNative/capture成功而API3/unknownBuffer2拒绝。f9aa第一次local分类仍失败，定位为把getelementptr中的“ptr ”误当opaque pointer，修正字边界并新增inline GEP检查后当前候选通过。第一partial-texel跑到正确GPU前拒绝，但旧oracle只认旧overlap文字而FAIL；更新为核验“失效+unknown namespace”，保留旧FAIL并在独立目录完整复验。不是覆盖失败结果或继承旧hash PASS。

`mutable-local-evidence.json`保存六manifest和503完成日志SHA，strict0；`checkpoint-mutable-local-7bb1d560/manifest.json`保存当前源码/四产品/执行文档与复现脚本、CPU审计。外盘根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544保持symlink。内盘约31GiB/外盘1.6TiB，所有构建及GPU/pre-submit串行共享锁，没有启动重叠UE或qrenderdoc。没有提交、推送、重置、改写用户RDHeaderView代码。

下一继续实际通用AS/Header的typed source/Native句柄重定位与已提交构建依赖，并按Native intersection-query API效果分类保持黑盒；不能通过删除queryReset/unknownAS/未知pointer检查提交。`mutable-local-next-RT-namespace-audit`保留实际library/entry/hash/10个Native query调用的CPU审计，未执行replay GPU或验RT输出。官方/full IR/full RT/lifecycle/Qt-ARC/native设备集中门槛与实际UE整帧RT输出、重放、事件/EID0、绑定仍未完成；其余coverage族、standalone frame texture/更广view与真实GPU指针发布机制仍需补。AS内部、RT shader单步和RT Pixel History不支持。

2026-10-07 [B544 typed namespace 行窗口与真实读取宽度](GENERIC_API_RECOVERY.md)：固定backend/bundle `fb456ca1`。typed地址保存真实API invocation界及ABI行窗口，动态读取不再被不可达冷行内容阻断；全部物理字段仍重定位，窗口内来源/initial/producer/生命周期/提交校验，范围未知仍恢复完整namespace，候选展示partial。缓存区分窗口，只复用已验证读取宽度，不把allocation容量当内容证明；Native volatile内存效果保留，不计算shader数值。三fresh真query变体1/17/257冷行、1KiB→64KiB/offset64→65532、Header/贡献/输出偏移、cross upload→same-submit slot7 GPU producer完整输出/各56事件通过；放宽调度触及冷行、越声明界、2byte上传不能证明4byte读取及缺/未来producer均GPU前拒绝。fresh旧范围相关恢复、converted RayQuery、29旧capture58API/CLI通过，115坏组230拒绝。UE禁GPU预检仍API4于非零候选14714内容/producer，peak1420476416bytes，无wait/strict；不是实际访问证据、不是UE整帧PASS。下一按真实物理backing/输入恢复义务补机制，不扩CPUshader许可或跳过缺行；完整UE/官方/full/最终启用未验，两flagsfalse、目标active。当前证据/失败/源码与产品检查点全外盘，无提交推送重置。


2026-10-07 [B544 placement buffer 物理字节恢复](GENERIC_API_RECOVERY.md)：固定 backend/bundle `c573bda0`。按真实 Native heap/offset/逻辑范围共享已恢复字节，重新创建的合法 buffer alias 不再丢失前一对象的 upload/GPU 定义区间；unknown-source copy 覆盖使物理证明失效，旧初态先统一播种，禁止后来 alias 复活旧内容。CPU 数值和 typed 地址来源仍独立，纹理/AS opaque footprint 不证明 buffer 内容，未来 producer 不能借给早期消费。三 fresh alias query 变体改变 size/heap offset/读取偏移/上传→Native GPU store/绑定槽，完整输出和事件往返通过；fresh 窗口 GPU、retired/部分 texel、converted RayQuery 与29旧 capture 的58 API/CLI通过，121坏组242 GPU前拒绝。UE 同捕获禁GPU预检仍 API4 于候选14714的内容/producer缺口，peak1296728064bytes、无GPUwait/strict，不算整帧PASS。实际Native纹理footprint审计没有重叠，下一补真实 creation input/生产者恢复机制，不跳过候选或伪造零初态。完整 UE/最终官方/full/设备启用未验，两RT flags false、目标 active。产物和失败证据均外盘，无提交推送重置。


2026-10-08 [B544 API 创建初态通用恢复](GENERIC_API_RECOVERY.md)：backend/bundle `568e50eb`。真实 MTLDevice::newBuffer(length) 的零初始化契约、newBuffer(bytes) 的完整原输入在实际出生点发布恢复区间，seek 仍执行原 Native factory；不要求多余 upload，不应用于背景帧初态或 heap buffer，不制造 CPU shader 数值。bytes factory 缺/短 payload 明确拒绝，不能退化为 length factory。三 fresh Private零/Shared零/Shared bytes sample 改变16KiB→64KiB、offset1024→65532、Header/贡献/动态query/输出偏移，完整创建字节、输出与事件/EID0通过；改为实际 heap 输入的 alias/窗口/producer负例与相关恢复、fresh converted RayQuery、29旧capture58 API/CLI通过。九 fresh runtime464事件，167坏组334 GPU前拒绝。修正旧 partial/cold fixture 把设备新建 buffer 当 undefined 的错误语义；旧拒绝记录保留但不继续作为合法 API 的拒绝依据。UE14714仍是heap factory，CPU审计证明该契约不适用；本候选未重跑无变化UE，不继承旧库UE结果，整帧仍未完成。继续真实heap输入恢复，全部最终RT门槛未完成、flags false，较大批次 active；产物/失败/检查点外盘，无提交推送重置。
