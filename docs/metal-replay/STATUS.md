2026-10-08 UI 交付候选已备齐：[1280×720 UE Lumen/Nanite/VSM 唯一当前卡](UE_UI_CAPTURE_2026-10-08.md)。交付 c827cc7e 捕获在 backend/bundle1845eca8完成三整帧GPU重放与两次EID0；同帧Native像素比较FAIL（18133/23804/15578像素），整体INCOMPLETE，GUI手工验收待用户。独立direct/indirect/wide view/并发Drawable、五attachmentless与真实契约负例、官方Metal2均在同候选实跑通过；完整converted矩阵本次未跑。fresh1845的a877捕获Native成功但背景placement重叠replay拒绝，根因未闭合、不交付为正常文件。旧f1 compute RT完成证据独立，render RT不开启；文件、命令与manifest全外盘。

2026-10-08：对应 compute RT 开启验收已完成。当前 backend/bundle `f1ba1862`，`supportsRaytracing` 跟随 Native 设备，公开路径无需 probe；`supportsRaytracingFromRender` 保持false。开启后官方Metal2、真实UE/Lumen整帧三次EID0与同帧Native零差、query事件/绑定、独立合法变体、相关生命周期/frame及完整转换矩阵均在同候选实跑通过。GPU COMPLETED / 输出 MATCH（UE EXACT_MATCH）/ overall PASS仅对应记录的compute/API/预算及本机设备范围；Metal3扩展、render-stage、物理不支持设备实跑及原Qt GUI崩溃未认证。旧1dc FAIL不改写，旧hash不继承。[最终唯一记录](HEAP_BACKING_RECOVERY_2026-10-08.md#compute-rt-能力开启与最终验收完成f1ba18622026-10-08)。持续任务已按达标约定停止（metal automation已删除），不继续重跑或自动扩展验收范围。

## 当前执行规则（2026-10-08，覆盖冲突旧要求）

已冻结的先前稳定backend/bundle `c88d10d4`。实际修改heap/动态绑定恢复：CPU上传的真实恢复区间与可选dense数值缓存分开；小段控制输入从原初态/有序CPU发布按需观察，GPU/alias失效及真实typed地址/发布检查保留，不增预算或CPU表达式。独立40/36MiB Native变体通过，旧c54根因被独立复现，真实地址损坏仍GPU前拒绝。第二份UE971捕获实际352×256资源变体、5次非零query，900×640整帧replay；按原point采样的896×637无损PNG像素全相同，两次EID0 raw hash一致，5 query事件/绑定往返及scoped T2通过。M1本批/M2输出/M3本变体验证达成；M4生产捕获流程及固定最终官方/生命周期/设备/开启复验未完，两flagsfalse。下一通用capture协议事实持久化与未改声明的UE截帧重放，稳定后集中最终验收。[唯一批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#按需控制输入与跨捕获恢复候选-c88d10d4)，外盘scalar-interval-evidence.json；旧hash独立，不重做已完成工作。

2026-10-08 [B544 API 创建初态通用恢复](GENERIC_API_RECOVERY.md)：backend/bundle `568e50eb`。真实 MTLDevice::newBuffer(length) 的零初始化契约、newBuffer(bytes) 的完整原输入在实际出生点发布恢复区间，seek 仍执行原 Native factory；不要求多余 upload，不应用于背景帧初态或 heap buffer，不制造 CPU shader 数值。bytes factory 缺/短 payload 明确拒绝，不能退化为 length factory。三 fresh Private零/Shared零/Shared bytes sample 改变16KiB→64KiB、offset1024→65532、Header/贡献/动态query/输出偏移，完整创建字节、输出与事件/EID0通过；改为实际 heap 输入的 alias/窗口/producer负例与相关恢复、fresh converted RayQuery、29旧capture58 API/CLI通过。九 fresh runtime464事件，167坏组334 GPU前拒绝。修正旧 partial/cold fixture 把设备新建 buffer 当 undefined 的错误语义；旧拒绝记录保留但不继续作为合法 API 的拒绝依据。UE14714仍是heap factory，CPU审计证明该契约不适用；本候选未重跑无变化UE，不继承旧库UE结果，整帧仍未完成。继续真实heap输入恢复，全部最终RT门槛未完成、flags false，较大批次 active；产物/失败/检查点外盘，无提交推送重置。

2026-10-07 [B544 placement buffer 物理字节恢复](GENERIC_API_RECOVERY.md)：固定 backend/bundle `c573bda0`。按真实 Native heap/offset/逻辑范围共享已恢复字节，重新创建的合法 buffer alias 不再丢失前一对象的 upload/GPU 定义区间；unknown-source copy 覆盖使物理证明失效，旧初态先统一播种，禁止后来 alias 复活旧内容。CPU 数值和 typed 地址来源仍独立，纹理/AS opaque footprint 不证明 buffer 内容，未来 producer 不能借给早期消费。三 fresh alias query 变体改变 size/heap offset/读取偏移/上传→Native GPU store/绑定槽，完整输出和事件往返通过；fresh 窗口 GPU、retired/部分 texel、converted RayQuery 与29旧 capture 的58 API/CLI通过，121坏组242 GPU前拒绝。UE 同捕获禁GPU预检仍 API4 于候选14714的内容/producer缺口，peak1296728064bytes、无GPUwait/strict，不算整帧PASS。实际Native纹理footprint审计没有重叠，下一补真实 creation input/生产者恢复机制，不跳过候选或伪造零初态。完整 UE/最终官方/full/设备启用未验，两RT flags false、目标 active。产物和失败证据均外盘，无提交推送重置。

2026-10-07 [B544 typed namespace 行窗口与真实读取宽度](GENERIC_API_RECOVERY.md)：固定backend/bundle `fb456ca1`。typed地址保存真实API invocation界及ABI行窗口，动态读取不再被不可达冷行内容阻断；全部物理字段仍重定位，窗口内来源/initial/producer/生命周期/提交校验，范围未知仍恢复完整namespace，候选展示partial。缓存区分窗口，只复用已验证读取宽度，不把allocation容量当内容证明；Native volatile内存效果保留，不计算shader数值。三fresh真query变体1/17/257冷行、1KiB→64KiB/offset64→65532、Header/贡献/输出偏移、cross upload→same-submit slot7 GPU producer完整输出/各56事件通过；放宽调度触及冷行、越声明界、2byte上传不能证明4byte读取及缺/未来producer均GPU前拒绝。fresh旧范围相关恢复、converted RayQuery、29旧capture58API/CLI通过，115坏组230拒绝。UE禁GPU预检仍API4于非零候选14714内容/producer，peak1420476416bytes，无wait/strict；不是实际访问证据、不是UE整帧PASS。下一按真实物理backing/输入恢复义务补机制，不扩CPUshader许可或跳过缺行；完整UE/官方/full/最终启用未验，两flagsfalse、目标active。当前证据/失败/源码与产品检查点全外盘，无提交推送重置。

2026-10-07 [B544 Native GPU定义区间与通用dispatch绑定](GENERIC_API_RECOVERY.md)：固定backend/bundle `af26d32b`。真实在前Native GPU store的定义区间与CPU数值分开；未知clz值仍Unknown，精确非零compute写按已恢复地址/范围/提交发布4byte区间、统一预算，不造padding/CPU值。conditional/ranged/atomic/raster/未知pointer不借此保证初始化。direct/indirect sourced dispatch以实际闭包和PSO反射绑定校验取代slot0/2D-copy形状限制。两fresh真query变体cross/slot2/offset64/1KiB→same/slot7/offset65532/64KiB、实例2/各56事件/真实GPUword完整输出通过，未来producer与缺producer仍GPU前拒绝；fresh旧恢复相关路径、converted RayQuery及29旧capture58API/CLI通过，128坏组256拒绝。UE禁GPU预检仍API4在非零候选14714内容/producer未证明处，peak1364377600bytes、无wait/strict；没证据实际访问该候选，不能伪造producer。下一动态候选/physical backing/定义区间恢复义务边界，非扩CPUshader数值。完整UE及最终集中门槛未验、两flagsfalse、目标active；新产物外盘，失败/当前产品源码文档哈希保留，不提交推送重置。

2026-10-07 [B544 零字段/稀疏 namespace 与统一复制预算整改](GENERIC_API_RECOVERY.md)：backend/bundle `c52ba022`。物理buffer零字段按真实initial/CPU publication/匹配full24 GPU copy保存消费点证明，retired/纹理/未发布零行不虚构buffer依赖；opaque/alias writer与CPU提交更新使旧事实失效，非零缺来源/初态/producer仍拒绝。GPU全零descriptor的空引用图不再误判无producer。ordinary/typed/线性copy场景计数改已有整帧预算，保留单次1MiB真实恢复边界、不扩大上限。独立Native的3/17/257稀疏行、R32Uint2×3→RGBA8Unorm7×5、动态实例/偏移及GPU背景+帧重发布通过；fresh部分Private/可变/texel、converted RayQuery与29旧capture58API/CLI通过，111坏组222GPU前拒绝，缺冗余raw initial但完整typed publication合法控制通过；未声明地址writer仍恢复拒绝。UE无GPU预检越过退休零字段，下一非零placement候选14714缺内容/producer证明，仍API4，peak1357774848bytes，无wait/strict，不能假装候选被实际访问。下一physical backing/定义区间与动态候选恢复资格，非CPU shader数值扩许。完整UE/最终官方full与能力验收未跑，两flagsfalse、目标active；新产物全外盘、失败保留、无提交/推送/重置。

2026-10-07 [B544 动态 typed namespace 与部分初态通用整改](GENERIC_API_RECOVERY.md)：固定 backend/bundle `5673f351`。真实 GPU query/metadata 动态选择 24-byte typed table 的缓冲字段时，保留已恢复 namespace/ABI 字段而非要求 CPU 求出行号；按消费点冻结的实际槽位建立恢复候选和提交依赖，候选不冒充实际访问，展示 partial。普通帧出生 Private/Tracked 缓冲的地址重定位不再依赖 CBV 分类；真实部分上传区间独立于 CPU 数值，动态绑定不要求整个 allocation 初始化，缺来源/初态/生产者仍拒绝。两 fresh 部分缓冲 query 变体（实例2选第3缓冲/4-byte 上传、1KiB→64KiB、offset64→65532、Header/贡献/输出偏移）各56事件/EID0/完整输出及真实定义字校验通过；fresh GPU初态+帧重发布、读改写/local atomic/部分texel、converted RayQuery与29旧capture58 API/CLI通过。91坏组182 API/CLI均GPU前拒绝。UE禁GPU预检消除56未知缓冲并越过64KiB/4-byte恢复，下一table25/slot2760已退休纹理的null缓冲字段缺当前namespace证明，仍API4，peak1207173120bytes，无wait/strict；非UE整帧PASS。下一补零字段/退休槽与GPU发布的通用证明，非零失效来源不得跳过；不扩CPU shader表达式许可。官方/full/UE整帧最终验收未跑，两flagsfalse，目标active，失败和外盘检查点保留，无提交推送/重置。

2026-10-07 [B544 私有指针与 Native query API 通用整改](GENERIC_API_RECOVERY.md)：固定 backend/bundle `ec370d18`。非逃逸 private pointer slot按CFG所有在前writer保留typed对象/offset，分支/循环与LLVM生命周期独立数值；未初始化、未来writer、未知来源/外部逃逸仍拒绝。三角形实例query按Native返回ABI分类整个数值/向量/矩阵getter族，GPU计算、展示partial，设备pointer-return不借此放行。两fresh Native真query（1→3实例、Header0→64/贡献0→16/输出0→32、两private来源分支）完整输出/48事件/EID0/AS+UAV公共查询通过，缺AS/child/贡献初态仍GPU前拒绝；fresh读改写/local atomic/部分texel/converted RayQuery与29旧capture58API/CLI通过。UE无GPU预检越过3246400/4065；1591744/6399 AS2/unknownAS0/unknownCall0，剩unknownBuffer56，commit4981760仍API4，peak1523056640bytes。不是UE整帧PASS，flagsfalse、目标active；下一补真实动态descriptor/缓冲地址namespace闭包，不扩CPU shader表达式许可。新产物外盘，保留中间FAIL、源码/产品/文档哈希，无提交推送/重置。

2026-10-07 [B544 通用 Native query/AS 恢复闭包](GENERIC_API_RECOVERY.md)：backend/bundle `25632fabc7d45be667f48871f5ca14c4572ce45d728470d096bdeb706916b4c8`。移除 runtime 一律 queryReset 拒绝；typed kind3 槽位→Header→Native AS/贡献缓冲双字段来源、dispatch 时冻结构建图和实际提交依赖独立验证，GPU 负责交点与动态索引，展示 partial。私有 query 调用及私有目标存储不误当捕获 device buffer。移除 default direct TLAS 对不同 BLAS 提交的历史要求，保留实际在前构建/冻结子配方与设备检查。同提交 1/3 实例、Header0/64、贡献0/16、动态输出0/32的两 Native 光追 sample native/capture/API/CLI、96事件/EID0、40公共查询点/80 descriptor-location检查通过；共24坏组48无GPUwait拒绝。可变输入/local atomics、部分texel、fresh旧RayQuery及29旧capture58 API/CLI通过。UE禁initial/frameGPU预检仍API4：实际3246400/4065的AS未知、10调用及私有store误报消失，剩unknownBuffer1；peak1284096000bytes，无wait/strict。下一先用独立 Native 私有pointer spill复现，补来源传递而非shader数值模拟。UE整帧与光追开启集中门槛未完成，两生产flagsfalse。

2026-10-07 [B544 可变数值事实与 Native 本地内存通用整改](GENERIC_API_RECOVERY.md)：backend/bundle `7bb1d560`。同dispatch实际写入使重叠scalar事实失效并重新分析，保留typed对象/地址来源与真实初态/producer；已恢复的合法读改写由原shader执行，展示partial。精确范围保留共享backing的分离root/texel，未知写范围保守覆盖allocation；Native addrspace(3)原子不误当捕获device buffer，不模拟结果。两fresh读改写+local atomic偏移变体、部分texel初态/动态GPU descriptor、fresh RayQuery与29旧capture58 API/CLI通过；缺RMW初态/缺来源/原始VA/只读等坏capture仍GPU前拒绝。UE无GPU预检越过1584704/3993和1596480/4007，约28秒到实际intersection-query入口3246400/4065：AS1/unknownAS1/unknownBuffer2/unknownCall10，commit4981696仍API4，peak1406369792bytes、无wait/strict。不是UE重放PASS，两flagsfalse，继续通用AS/Header来源及RT依赖闭包。

2026-10-07 [B544 Native API效果与资源依赖索引整改](GENERIC_API_RECOVERY.md)：backend/bundle `9e44a610`。寄存器bit函数按完整Native prototype分类效果，旧AIR缺readnone不当未知资源调用，结果仍unknown、不模拟CPU数值；指针/外部/错误signature不借此放行。资源依赖由全局descriptor逐资源扫描改为实际resource的有序区间，来源/生存期/提交校验相同。独立modern/legacy Native先复现误拒绝；最终通用预编译/JIT各六变体、动态sampler/部分texel GPU初态、fresh RayQuery与29旧capture58API/CLI通过。UE原四调用已越过，资源索引优化后同45s/3GiB限制内约25s结束，无GPUwait/strict；仍API4，下一1584704/PSO3993的同dispatch scalar读写重叠，commit4981696，peak1623212032bytes。不是UE PASS；先拆失效CPU事实与真实地址namespace/依赖，不直接删检查。两flagsfalse，目标active，完整源码/产品/文档与失败证据继续外盘保存。

2026-10-07 [B544 通用资源恢复首组整改](GENERIC_API_RECOVERY.md)：稳定 backend/bundle `d907eec8`。实际删除 frame texture/view 的格式-尺寸-mip-usage/coverage 组合许可，共享格式族/子资源布局与 Native view 投影；buffer/table/copy 改统一预算，command/dispatch 数量许可改整帧记账预算。Native stride 声明、metadata 与 converted ABI 分开；真实 shader/clear producer 与 write residency 分开；Native JIT 无 AIR 可重放，动态纹理展示 unknown。预编译/JIT 各六 native 变体及完整像素/往返/EID0、损坏 capture GPU前拒绝；dynamic sampler/部分 texel 初态/fresh RayQuery与29旧capture58 API/CLI通过。实际 UE 无 GPU 预检越过旧数量上限，下一297152/PSO3906的4未知调用效果，commit4502144仍API4，peak1448443904bytes，无GPUwait/strict；整帧未验，flagsfalse。根 AGENTS 与持续提示已覆盖旧策略，其余coverage族/standalone/更多view机制待后续通用批次，未冒称已全部清除。

下一恢复批次的CPU审计已定位：`development/generic-resource-next-effects-audit` 提取实际Native library/entry并保存hash，四个未知调用对应4次 `air.clz.i32(i32,i1)`，声明仅nounwind而无readnone。此为调用效果分类/可选展示问题的候选，不已证明输出或整帧重放；下一先按Native数值API效果分类、独立sample验证，不编写CPU clz结果模拟或按UE shader名称放行。

2026-10-07 最新任务整改（覆盖历史场景许可策略）：先完成一组通用资源恢复与重定位机制，再继续 UE/Lumen 新案例。实际修改纹理/frame-view/coverage/runtime AIR preflight，区分 API/设备、恢复缺口、统一预算、历史场景限制与展示分析；coverage 不积累场景等级，Native 动态计算与 partial 展示独立。具体执行规则、逐函数 DX12/Vulkan 对照及剩余边界见 [GENERIC_API_RECOVERY.md](GENERIC_API_RECOVERY.md) 与根 AGENTS.md；光追开启门槛不变。持续任务 metal 已同步。

2026-10-07 [B544 buffer-texture/当前copy状态集成](BATCH544_API_RT_INTEGRATION.md)：固定backend/bundle4163d391。按VK/DX12对象/offset恢复帧view metadata，逻辑像素范围与根参数可共用backing；当前copy字节有效性独立历史opaque标记，像素restoration intervals独立CPU数值分析。两fresh view/两同跨提交atomics合计240事件、77坏组154 GPU前拒绝，shared-root真实先读像素且缺像素上传保留根上传仍拒绝；fresh RayQuery4253/4253/0/0与528uint/48事件、29旧capture58 API/CLI通过。UE已越过3853，到3854/278528 background GPU descriptor初态/namespace与Private producer链，仍API4，无initial/frameGPU/GPUwait，peak1,421,934,592bytes。未知writer动态heap资格仍需恢复，不将CPU展示未知永久拒绝；flagsfalse/目标active、整帧/full/final未验；8manifest/65源码四产品九文档全外盘，旧oracle/MSL/API失败保留。

2026-10-07 [B544 Native SIMD/纹理原子与读回集成](BATCH544_API_RT_INTEGRATION.md)：固定backend/bundle8d6ec636。Native lane不模拟CPU结果，phi保留来源，atomic store/load/RMW及真实producer/提交独立；texture→buffer共享布局/生命周期/alias/预算校验。两fresh同/跨提交144事件/EID0/64pixel/2112-byte读回padding、38坏组76拒绝；metadata-only48事件/15坏组30拒绝；fresh RayQuery4253/4253/0/0与528uint、6fresh无fragment174事件/4坏组8拒绝、29旧capture58API/CLI通过。UE越过3836，下一3853/277440 frame buffer-texture对象/写资格，unknownBuffer0/unknownCall0，commit3517632仍API4，peak1,275,609,088bytes，无initial/frameGPU/GPUwait；下一诊断实际view恢复，不扩CPU数值语法。整帧/最终官方/full未验，flagsfalse、目标active；7manifest/65源码四产品九文档检查点全外盘，中间FAIL保留。

2026-10-07 [B544 条件来源/重定位与纹理对象信息集成](BATCH544_API_RT_INTEGRATION.md)：backend/bundle529d0a7b。已恢复buffer/null动态读及字面量连续写保持Native来源；base/derived/spanning捕获VA独立要求重定位，不用CPU predicate绕过。尺寸查询用恢复对象/factory信息，pixel read仍验证初态/producer，Native内部readonly调用不生成假返回来源。fresh集成48事件/20查询点/40descriptor-location与15坏组30拒绝、fresh RayQuery4253/4253/0/0和528uint/48事件、6 fresh无fragment174事件/4坏组8拒绝、29旧capture58 API/CLI通过。实际UE越过1624/1625/3830，下一3836/274240 SIMD/texture atomics副作用及读写初态，commit3517632仍API4，无initial/frame GPU，peak1,419,739,136bytes；下一不模拟lane算法/不只删unknownCall，补恢复/提交闭包。standalone frame texture仍未实现，不能以placement成功替代。flags false、目标active，9文档/64源码四产品检查点全外盘；final/full/official/UE整帧未验。

2026-10-07 [B544 可选shader阶段与Native同步集成](BATCH544_API_RT_INTEGRATION.md)：backend/bundlef9718c71。按实际Native PSO允许普通vertex管线无fragment/有颜色和深度及合法残留fragment绑定，公开access不报告不存在shader的阶段；Native线程组barrier/fence与buffer原子效果分开，不模拟线程组算法。6fresh无fragmentcapture/174事件与4坏组8拒绝、fresh线程组同步/原子48事件/20查询及10坏组20拒绝、fresh RayQuery4253/4253/0/0与528uint/48事件、29旧capture58 API/CLI通过。实际UE无GPU越过draw3330304，推进commit3517632；PSO1624 unknownCall0，剩4 derived-null/GEP资格拒绝，未证明缺地址。下一独立来源/地址恢复，不增加数值/共享数组模拟。peak1,163,837,440bytes，flagsfalse、目标active；full/official/UE整帧未验，证据/64源码四产品九文档检查点全外盘。

2026-10-07 [B544 指针来源/原生调用效果与附件范围集成](BATCH544_API_RT_INTEGRATION.md)：backend/bundle2133a76f。分支、结构投影和原子保留资源来源；数值调用按实际声明记录资源效果，GPU仍计算，展示partial不拒绝，旧writer事实失效；真正嵌入捕获VA/未知GPU指针仍需恢复。8 fresh六/八MRT、200事件/EID0和5坏组10拒绝；fresh原子分支48事件/20公共查询与10坏组20拒绝；fresh RayQuery4253/4253/0/0、528uint/48事件，21旧capture42 API/CLI通过。实际UE无GPU越过首commit557952与六附件3233472，当前draw3330304的fragment阶段身份/可选性待核实，peak1,180,876,800bytes。未完成UE整帧/最终启用验收，生产flagsfalse、目标active；9文档/外盘检查点继续保存最新四层准则，不以CPU数值语法扩展代替API恢复。

2026-10-07 [B544 恢复与展示首段解耦](BATCH544_API_RT_INTEGRATION.md)：backend/bundle d17d244e。已恢复对象的动态 buffer 偏移及分支候选范围可保留 Native 重放、展示部分；未知写使旧scalar事实失效，真正来源/类型/初态/提交约束保持。三fresh普通Native/capture/API/CLI、144事件/EID0/60公共查询及35坏组70拒绝；fresh converted RayQuery4253/4253/0/0、528uint/48事件；21旧capture42 API/CLI通过。UE仍在248128 select丢失pointer identity处无GPU拒绝，不证明实际地址未恢复，下一补独立 pointer provenance/namespace资格。尚未完成全路径解耦/UE整帧/最终启用验收；flagsfalse、目标active。证据和检查点在外盘。

2026-10-07 用户最新方向（覆盖历史 unknown 一律拒绝策略）：将资源恢复、地址重定位、提交依赖与 shader 访问展示独立处理。正确重放的动态访问不能仅因 CPU 展示分析不完整而永久拒绝；未恢复 GPU 指针、失效初态、缺失资源/生产者必须补恢复机制。下一 B544 优先拆恢复资格与访问展示，不继续逐个扩充 CPU shader 表达式作为完整 API 适配。执行规则见 [RT_API_INTEGRATION_WORKFLOW.md](RT_API_INTEGRATION_WORKFLOW.md)。方向已调整，当前 backend 的既有展示 hard gate 尚未全部拆除，不能冒称已经实现解耦。

2026-10-07 [B544 通用附件 Load/颜色格式族与 Header 指针发布集成](BATCH544_API_RT_INTEGRATION.md)：仍为同一大批次，不增加微批。按本地 DX12 Preserve/Vulkan Load 保留 frame-born Private placement MRT/depth/stencil 的 Load/Discard，未定义像素不生成 CPU 地址事实；既有完整初态的普通 uncompressed UNorm/SNorm/float 颜色格式按 API 格式族支持，补 R8/R16 Snorm 尺寸查询，保留出生/alias/范围/usage/PSO/store 与预算校验。Header 的贡献指针发布不再要求被指缓冲从未被 GPU 写入，实际读取/已知内容和提交闭包仍单独验证。当前 backend/bundle `08677fd7`：12 fresh Native/capture/API/CLI 场景、384事件选择/EID0、每 plane 256像素；38附件坏组76 GPU前拒绝；Header 派生两合法控制及10坏组20拒绝；fresh converted RayQuery 4253/4253/0/0、528uint与公共查询/事件通过；21旧capture42 API/CLI通过。实际 UE 无GPU预检越过三 Header，当前首提交557952于 compute PSO1707/dispatch247360的3 unknownBuffer拒绝，peak1,637,203,968bytes；graphics commit消费证明、UE整帧输出/EID0/绑定仍未到达。新增GPU contribution复制 Native/capture成功，但实际 query consumer 及派生 Header-only 均拒绝，分别保留 FAIL/预期拒绝证据，不算支持；保留编译/旧helper范围计数及第二capture oracle失败和修复复验。全量official/IR/RT/lifecycle/Qt-ARC最终集中验收未跑，生产flagsfalse、任务active，历史崩溃/重启未证明修复。全部产物外盘，内盘32GiB，无提交推送/重置。

2026-10-07 [B544 通用 graphics linked stage 与 AS 初态依赖继续集成](BATCH544_API_RT_INTEGRATION.md)：仍为同一大批次。按真实 Native linkedFunctions 身份、vertex/instance/base/index 参数、当前 typed inline/heap 来源和提交输入证明普通 graphics buffer 消费，保留 AS 黑盒；完整创建数据可作初态但 writer/alias 后不回退，CPU/GPU 发布与当前逻辑 view 继续核验。修复 Header 快照在 background 状态的帧引用被忽略而遗漏 BLAS 初态的问题；公共查询按已证明事件/阶段/管线呈现实际 heap buffer access，不扫描闲置 AS，不按 UE/Lumen/shader 名授予支持。当前 backend/bundle `39ff0128`：fresh linked graphics Native/capture/API/CLI PASS，32事件/EID0、每次256深度像素、16 access/descriptor/location检查；6坏组12 GPU前拒绝。真实 converted RayQuery fresh PASS（4253/4253/0/0、528uint/48事件）；跨提交Private循环链 fresh 48事件/20 access/60 descriptor及17坏组34拒绝，四旧capture8 API/CLI通过。AIR入口直接参数/嵌套ordinal、converted附加metadata、linked返回值/缺失调用/instance下溢CPU通过。实际UE无GPU预检已越过旧depth-only draw资格guard，但其graphics提交闭包仍未到达；当前停在frame MRT renderpass303616的Load/initial/alias初始化语义，peak RSS1,612,087,296bytes，无initial/frameGPU/GPUwait，不计UE重放PASS。中间metadata回归及修复后复验分别留证。本批未关闭，生产flags仍false，最终official/full/lifecycle/Qt-ARC/UE整帧未验，历史崩溃/重启未证明修复。新产物全外盘，内盘32GiB，无提交推送/重置，目标active。

2026-10-07 [B544 通用有界循环和描述符角色继续集成](BATCH544_API_RT_INTEGRATION.md)：同一大批次，按提交时已发布输入证明零起点、单步有界循环和间接索引范围，保留typed pointer来源；读写重叠（含物理alias）、未知/越界输入拒绝。统一buffer SRV/UAV、typed buffer与CBV角色，公共buffer查询保留逻辑view长度，无UE/Lumen/shader名分支。backend/bundle `acc8b703`：两fresh循环Native/capture/API/CLI通过（直接和跨提交Private拷贝，96事件/EID0、40 access及120 descriptor检查、完整输出/只读/padding）；29坏组58无GPUwait拒绝，含实际触发的索引自写重叠和逻辑view越界。fresh converted RayQuery4253/4253/0/0、528uint/48事件及四旧capture8 API/CLI通过；244完成日志无严格诊断，45源码/四产品外盘检查点。UE实际首scatter和后续同类compute闭包通过，当前no-GPU预检在depth-only graphics draw的全局AS槽资格拒绝（峰值1,601,683,456bytes，无initial/frame GPU/GPUwait）；CPU原始AIR/当前输入和vertex linked stage-in调用已留证，均非UE输出验收。下一通用graphics linked function/vertex-instance输入及描述符实际消费，再补RT runtime consumer闭包；最终集中验收未完成，两生产flagsfalse、任务active。新产物外盘，内盘32GiB，无提交推送/重置。

2026-10-07 [B544 通用 GPU 拷贝输入链继续集成](BATCH544_API_RT_INTEGRATION.md)：仍在同一大批次开发，按DX12/VK原生copy及提交顺序补齐Shared→Private→Private CBV输入，支持背景/帧内Private暂存、非零offset/分段拷贝、同提交/跨提交；真实shader writer使旧字节失效，未知GPU输出不回退旧初态。当前backend/bundle `f1988b67`：direct及三种copy-chain四fresh Native/capture/API/CLI通过（192事件/EID0、80公共access/descriptor/location检查、完整输出/padding）；45坏组90无GPUwait拒绝，另fresh合法native/capture中的opaque shader writer→copy链两次GPU前拒绝。fresh converted RayQuery4253/4253/0/0、528uint/48事件及四旧capture8 API/CLI通过；404日志无严格诊断，43源码/四产品与七文档外盘检查点。实际UE CPU审计确定首consumer提交参数[4,1,0,2]，四scatter目标索引860–863、96-byte typed descriptor payload；不是GPU验收。下一通用有界循环/data-indexed typed descriptor GPU producer及来源/重定位闭包，无UE/Lumen/shader名分支；未重复未变化的UE loop阻塞，当前UE initial/frame GPU/full集中验收未跑，两生产flagsfalse，目标active。新产物全外盘，内盘32GiB，清理19,066-byte可重建cache、没有本地Metal测试产物目录，无提交推送/重置。

2026-10-07 [B544 通用 runtime PSO 实际消费集成](BATCH544_API_RT_INTEGRATION.md)：同一大批次按immutable根ABI、实际AIR/当前typed descriptor来源和提交时已发布CBV字节支持普通runtime buffer访问，不再将绑定全局表中的未消费AS槽当作query许可；candidate必须在整帧GPU前完成提交闭包证明。write residency与实际shader writer分开，真实writer使parent/物理alias scalar事实失效，未知索引/范围/调用与runtime RT consumer仍拒绝。当前backend/bundle `f68bb0be`：一fresh原生compiled MSL准确ABI场景Native/capture/API/CLI通过，Private CBV GPU上传驱动A[3]=123→B[7]=456，完整输出/未写padding、48事件/EID0和20公共access/descriptor/location身份查询；9坏组18无GPUwait拒绝，含换入真实RayQuery metallib保留runtime元数据的反例（AS=1/unknownAS=1/unknownCall=8）。CPU icmp/select/动态struct GEP/未知调用测试、fresh converted RayQuery4253/4253/0/0及四旧capture8 API/CLI通过。43源码/四产品检查点/108日志外盘，严格诊断为空。实际UE启用新通用消费验证后在首个PSO437的循环/data-indexed scatter buffer访问拒绝（7 unknownBuffer），此前该普通PSO尚无完整消费证明；无initial uploads/frame GPU/GPUwait，peak RSS1,580,875,776bytes。下一按通用loop phi/实际输入索引及producer/alias闭包补齐，不能跳过验证或按shader名称适配；最终集中验收和UE GPU输出未完成，两生产flagsfalse，任务active。

2026-10-07 [B544 通用 placement buffer、无窗口捕获与设备生命周期集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，参照DX12/VK将buffer物理alias范围、逻辑descriptor retirement、资源保持与shader消费分开；buffer alias预算1MiB，texture预算不变，重叠writer使旧scalar事实失效。无窗口capture保存以及child代理持有device/内部frame record清理已补齐。当前backend/bundle `63ef3240`：224KiB/1MiB两fresh Native/capture/API/CLI、各48事件/EID0/完整物理重叠和padding；1MiB提前释放device引用fresh控制通过；8坏组16无GPUwait拒绝；20失败加载→10完整controller生命周期PASS（增长9,027,584bytes）；一fresh真实converted RayQuery Native/capture/API/CLI 4253/4253/0/0、528uint/48事件及公共typed查询通过，四旧capture8 API/CLI通过。118日志无严格诊断，39源码/四产品检查点外盘。实际UE已越过229376-byte alias阻塞，当前no-GPU在普通compute PSO3731/encoder14984的AS submission closure拒绝：绑定全局表中未消费AS槽被误作query权限；实际AIR含动态索引buffer store，下一按底层实际消费/当前typed根/descriptor/writer提交闭包实现，不能仅凭无ray调用放行未知地址。UE GPU输出/最终集中回归未验，生产flags仍false，历史UE/Qt崩溃和系统重启未证明修复。内盘32GiB/外盘1.6TiB，无提交推送，任务active。

2026-10-07 [B544 typed buffer SRV/UAV 集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，参照DX12 typed buffer/VK texel buffer，支持底层Metal Private buffer texture当前parent/view、非零offset/format/range、完整parent初态及读写依赖；捕获侧parent查询加锁，view写使parent后续scalar证明失效，parent读写/CBV/AS输入别名拒绝，无UE/Lumen特判。当前 backend/bundle `9fefb511`：Private standalone与Tracked placement两fresh Native/capture/API/CLI PASS，各实际texel读/write/width查询、4253/4253/0/0、528uint、48事件往返/EID0、24公共typed buffer descriptor/location检查、完整父buffer padding/只读SRV/六sampler重定位。75坏组150无GPUwait拒绝+3合法控制，另已生存parent读写重叠反例在通用闭包line825拒绝/原始控制通过；10旧capture20与两已有ec73 capture4 API/CLI通过。两实际AIR/三消费点12texture/13buffer读/6sampler无unknown，各42scalar事实；576日志无严格诊断，30源码/四产品检查点与全部新产物外盘。构建/捕获parent映射/工具AIR旧假设FAIL保留，不计PASS。B544/PHASE59未关闭，官方/full/lifecycle/Qt-ARC/UE整帧当前库未验，两生产flagsfalse；下一通用runtime PSO/实际heap依赖和动态Header，固定版本集中回归再UE实际截帧验收。内盘约32GiB，无提交推送/重置。

2026-10-07 [B544 通用 AS Header 发布集成开发](BATCH544_API_RT_INTEGRATION.md)：同一大批次解除公有64-byte Header记录、初态、更新、提交快照重定位对测试query PSO声明的依赖；shader消费资格仍独立且未知消费拒绝。Header发布只验证贡献buffer GPU pointer身份和范围，不按整块贡献allocation的16KiB旧夹具上限拒绝；实际consumer继续证明完整初态与读范围。当前backend/bundle `23c61b9a`，GUI `3ba30e36`、provider `2496f103`；32源码/四产品检查点和所有产物外盘。两个派生Header-only control（原12-byte与64KiB贡献buffer）API/CLI PASS，各1 AS build/0compute dispatch/28事件往返/EID0，ID/VA重定位与贡献/padding保持；10坏组20无GPUwait拒绝，含保留真实compute但移除query声明的消费控制。四已有capture当前8 API/CLI定向回归PASS（Private/placement typed buffer、multi-UAV、TraceRay），不计fresh capture。92完成日志无严格诊断。实际UE元数据CPU65在c0ba库接受；当前23c库无GPU加载已通过三初态Header/64KiB贡献身份与初态阶段，帧preflight在MTLHeap::newBuffer(offset) offset32128拒绝、无GPUwait，峰值RSS1,574,125,568bytes；不计UE重放PASS。当前官方/full/lifecycle/Qt-ARC、UE GPU上传/整帧输出/事件/EID0未跑，两生产flagsfalse、历史崩溃/重启未证明已修复。下一定位通用Private placement重叠/旧128KiB alias预算与runtime PSO绑定闭包，不增加UE/Lumen分支；功能集成后固定库集中验收。内盘约32GiB/外盘1.6TiB，持续active。

2026-10-07 [B544 buffer 实际消费与纹理尺寸查询](BATCH544_API_RT_INTEGRATION.md)：保持同一大批次，新增 AIR 全局 buffer load/store/atomic 依赖报告、实际 thread-grid 与 CBV scalar 的保守地址范围、当前 typed buffer descriptor 查询及尺寸 intrinsic 资源依赖；参照DX12/VK资源+offset/当前descriptor，未加UE/Lumen特判。当前 backend/bundle `ec73a2f1`：两 fresh native/capture 后的当前 API/CLI 通过，Private placement buffer 528uint/12公共buffer查询、五纹理各528texels/144身份/48公共descriptor/72sampler查询，均48事件往返/EID0；真实三消费点各14buffer读，raw另1写，9/11texture和6sampler且无unknown。58坏组116无GPUwait拒绝+3合法控制、10旧capture20API/CLI通过；456日志无严格诊断，28源码/四产品外盘检查点。首次构建/工具编译/80-byte工具重建/证明脚本旧假设FAIL保留并修正，未计PASS。B544/PHASE59和RT启用验收仍开放，两flagsfalse；当前库官方/full/lifecycle/Qt-ARC/UE整帧GPU未跑，不能证明历史崩溃/重启修复。优先完成通用runtime PSO/实际资源依赖/动态Header闭包再固定库集中验收及UE；新产物外盘，内盘约32GiB，无提交推送/重置。

2026-10-07 [B544 实际 sampler 消费与公共查询集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，参考DX12/VK sampler descriptor access/query，用实际AIR sample/gather调用、typed根指针及当前sampler表来源核验消费；公共GetDescriptorAccess/GetDescriptors/GetSamplerDescriptors返回当前sampler身份/参数，支持非零表偏移和EID0清空。当前backend/bundle `aca98095`、GUI `3ba30e36`、provider `2496f103`：两已有45c2真实capture当前扩展API/CLI PASS，各72 sampler参数/身份查询、48事件/EID0、144纹理身份及48 texture/AS公共查询、五纹理528texels/全部CBV padding/六sampler重定位；三执行点均解析6 sampler且无unknown。41坏组82无GPUwait拒绝+3合法控制，10旧capture20 API/CLI通过；真实编译七sampler程序在六项表之外的消费明确报sampler7/unknown1并拒绝，没有运行该坏程序的native调度。318完成日志无严格诊断、28源码/四产品与复现输入检查点外盘。中间工具SD生命周期错误及纯CBV第二调度误拒绝已修复复验并保留FAIL；未证明历史Qt/UE崩溃/重启修复。B544未关闭，当前无fresh Native/capture、官方/full/lifecycle/Qt-ARC/UE整帧GPU验收，不继承旧库PASS，两flagsfalse；下一完整buffer/其他AIR依赖及真实runtime PSO闭包→固定库集中验收→UE整帧。内盘约32GiB，持续active，无提交推送/重置。

2026-10-07 [B544 执行点 uniform 值与实际 AIR 消费集成](BATCH544_API_RT_INTEGRATION.md)：同一大批次继续开发，按提交/dispatch fileOffset 保存已证明 CBV 字节及当前 typed heap/Header 快照，用真实 AIR 核验 AS、SRV/UAV 角色和纹理数值类型；Private 帧内 CBV 支持既有预算内的 2MiB backing/offset58624，未知 GPU writer 仍拒绝。当前 backend/bundle `45c2a584`、GUI `3ba30e36`、provider `2496f103`：两 fresh Native/capture/API/CLI PASS（大 Private CBV+Private placement 纹理、小 Private placement CBV+Shared 纹理）；每项48事件/EID0、144身份、48公共descriptor/AS检查、五纹理各528texels、全部CBV padding/六sampler。35坏组70无GPUwait拒绝+3合法控制，10旧capture20 API/CLI通过；真实消费值追踪分别54/48 scalar读取，三执行点均解析1 AS/8 texture调用且无unknown，两compiler/AIR审计及CPU numeric测试通过。316完成日志无严格诊断，28源码/四产品检查点及所有产物外盘，内盘约32GiB。B544未关闭；实际UE自动PSO消费完整依赖/动态Header及整帧GPU输出/事件/EID0/预算未验，当前库官方/full/lifecycle/Qt-ARC未跑，两flagsfalse。保留测试夹具/日志留存与早期构建失败，不声称历史崩溃/重启修复；无提交推送/重置，持续active。

2026-10-07 [B544 通用 AS descriptor 与运行时绑定集成](BATCH544_API_RT_INTEGRATION.md)：同一大批次继续开发，参照DX12当前root table/AS descriptor和VK执行点参数/usage；AIR uniform解析支持匿名AS Header及实际AS调用，公共GetDescriptorAccess/GetDescriptors返回所绑定AS而非Header buffer。严格解析runtime binding ABI，核验根布局/绑定点/线程组与实际dispatch；runtime事实不能声明shader访问或开启消费资格，setBuffer替代setBytes会清空旧inline数据。当前backend/bundle `8caaecc0`、GUI `3ba30e36`、provider `2496f103`：两已有复合capture当前API/CLI PASS，每项48事件/EID0、144身份、48公共descriptor检查（含12 AS），五纹理全部528texels/CBV padding/六sampler；10 ABI坏组20无GPUwait拒绝+合法控制，10旧capture20 API/CLI PASS。中间71c9库两fresh Native/capture/API/CLI、21坏组42拒绝/两控制/旧20检查分别保留其hash，不冒充当前库fresh capture或最终验收。CPU解析119实际UE runtime payload+1 compiler payload及六真实query AIR形状通过；AIR使用合成loader，仅证明解析覆盖8 AS reset/93纹理调用，不证明真实资源依赖/UE输出。当前110完成日志无严格诊断，27源码/四产品校验备份及产物全外盘，内盘约32GiB。B544仍开发中，实际UE动态AS Header/自动PSO-heap消费闭包和整帧GPU/事件/EID0/预算未验，未重复已知失败UE；当前库官方/full/lifecycle/Qt-ARC未跑，两flagsfalse，历史崩溃/重启修复未证明。无提交推送/重置，持续active。

2026-10-07 [B544 通用多 UAV 与执行点依赖集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，PSO输出角色与资源身份解耦，多个texture UAV按当前heap slot解析；已验证槽位换绑、query纹理GPU输出→后续SRV消费的提交依赖及零调度。未证明的GPU buffer writer保持opaque，不能把旧初态用作后续消费证明。实现参照VK/DX12，无UE/Lumen/shader名分支。当前开发backend/bundle `105986c1`、GUI `3ba30e36`、provider `2496f103`；同一当前库两fresh Native/capture/API/CLI PASS：Private placement/五帧内2MiB CBV与Shared UAV/四帧内Private CBV，每项五纹理各528texels、48事件选择/EID0、144身份检查、36公共descriptor查询、完整CBV/padding/六sampler身份及只读usage，结果4248→4560和2375→2687。12坏组24无GPUwait拒绝+合法副本、10旧capture20 API/CLI定向检查通过，两真实compiler/AIR证明各两非零query；158完成日志无严格诊断。新19源码/四产品检查点及全部产物外盘，内盘约32GiB。B544未关闭，实际UE的通用动态AS Header/heap消费闭包及整帧GPU输出/定位/EID0/预算仍未验，本轮未重复启动已知失败UE；当前库官方/full/lifecycle/Qt-ARC未跑，两生产flagsfalse，历史崩溃/重启修复未证明。无提交推送/重置，持续active。

2026-10-07 [B544 通用纹理与 PSO 绑定 ABI 集成开发](BATCH544_API_RT_INTEGRATION.md)：同一大批次补齐类型化 texture SRV/UAV、Private/Tracked placement、原始创建 usage 核验及原生小堆范围；新增不可变 PSO 反射/运行时 ABI 捕获，保持 compiler JSON 与 runtime binding facts 的来源区别。当前开发 backend/bundle `96cf2591`、GUI `3ba30e36`、provider `2496f103`：新复合 Native/capture/API/CLI PASS（528 texels、56事件/EID0、48-byte帧内根/二维调度）；16坏组32无GPUwait拒绝+合法控制，10旧capture20 API/CLI定向回归PASS。实际新UE `403f90ea`/75,234,193bytes：119 PSO ABI/AIR核验、6光追PSO/5非零调度、40–72-byte根及原生线程组/绑定payload长度逐次一致，未依赖Lumen分组筛选；旧Lumen范围60 AIR/两非零query也通过。UE CPU65仍因动态AS Header/heap消费契约拒绝、无GPUwait/初态上传/frameGPU；UE输出/事件/EID0及总预算未验。B544未关闭，下一合并当前执行点动态/多输出和GPU生产依赖闭包，再固定库集中相关回归与短UE完整验收。两生产flagsfalse，无本库官方/full/lifecycle/Qt-ARC验收，不继承旧库；未证明历史Qt/UE崩溃或重启已修复。新产物继续外盘；追加34个frozen-validation目录1.18GiB校验迁移，保留路径链接（此前219目录17.3GiB迁移仍有效），未提交/推送/重置，持续active。

2026-10-07 [B544 通用 API 集成开发检查点](BATCH544_API_RT_INTEGRATION.md)：同一大批次继续补齐帧内 CBV 创建/CPU 与已知 Shared→Private blit 发布/提交顺序、2MiB backing 内的 CBV 子范围，以及 Private standalone/Tracked placement UAV 输出。实现参照 VK/DX12 的资源+offset、初态与提交依赖，不按 Lumen/shader/pass 特判。当前开发 backend/bundle `466a6485`、GUI `3ba30e36`：两新 Private 输出场景各 Native/capture/API/CLI PASS（528uint、56事件/EID0、二维[2,66,1]与零调度），其中一项组合五个帧内2MiB WriteCombined CBV、offset58624；四输出坏组八无GPUwait拒绝+合法对照，七旧capture十四API/CLI定向检查PASS。早期86f帧内三场景/34坏组与479d大初态/c9aa大帧内检查分别保留自己的库哈希，不计当前库最终验收。B544未关闭；texture UAV、多输出/更广GPU生产及真实PSO元数据闭包尚需继续，随后固定库集中相关回归→短UE真实光追截帧/输出/事件/预算。当前库未跑官方/full/lifecycle/UE/Qt-ARC，两生产flags仍false；此前用户崩溃/重启修复未证明。产物和检查点直接写外盘，219目录约17.3GiB迁移/58缓存清理保持有效，内盘约32GiB可用；本轮删三个过时patch脚本，保留失败证据。无提交推送/重置，持续active。

2026-10-07 [B544通用API集成继续开发](BATCH544_API_RT_INTEGRATION.md)：保持大批次合并/开发定向验证/固定最终库集中验收，Lumen仅作实际应用验收，接口与初态/重放继续参照VK/DX12。新增共用IR compute混合根类型与texture SRV依赖，static sampler逐slot来源/身份重定位，Private（含Tracked placement）初态CBV范围读取；不按shader/pass/效果特判。当前开发backend/bundle e8f709bf、GUI3ba30e36：40-byte四Private CBV+六sampler、48-byte五Private placement CBV+六sampler/二维[2,66,1]/64几何实例/unretained，两fresh Native/capture/API/CLI PASS（528uint各、48/56事件及EID0），六真实texture sample AIR调用/所有sampler ID变化/完整padding与read-only usage；27坏组54无GPUwait拒绝+合法对照、五旧capture API/CLI十检查PASS。mixed5早期初态拒绝/验收工具悬空SD引用崩溃已保留并修正，不等同此前UE/Qt崩溃修复。B544未关闭、未集中验收；下一帧内CBV producer-consumer与texture/Private输出通用闭包，再一次集中相关回归→短UE实际光追截帧/输出/事件/预算。两flagsfalse，无新UE或本库官方/full/lifecycle/Qt-ARC验收，不继承旧库。219目录17.3GiB外盘校验迁移与58 cache清理已完成，原路径链接、活动B544产物继续外盘；内盘约32GiB可用。未提交推送/重置，持续active。

2026-10-07 [PHASE59](PHASE59.md) / [B543](BATCH543_TYPED_HEAP_QUERY_INDIRECT.md)：参照VK FetchIndirectData/DX12 SaveExecuteIndirectParameters，补typed heap-query间接调度，逐encoder/ordinal/epoch核验原生GPU参数，保留原间接调用、零工作量与预算/闭包；修正typed只读资源误标CS_RW。最终backend/bundle2d7439ae、GUI3ba30e36、provideraba8b52b；八新间接query416事件（同encoder0/1/2、GPU1/132/0、末端offset/unretained/64几何/空TLAS）+14旧direct448事件+4旧heap64事件均Native/capture/API/CLI PASS。新72坏组144无GPUwait拒绝、6执行值不符组12次GPU核对后拒绝、2合法；普通计算间接八新capture56reset/seek、176坏组352无GPUwait/2执行不符组4拒绝PASS。56回归含官方两scene270事件10query/六能力、fresh四Sharedcompact18检查、20controller lifecycle（growth3063808）PASS，1798完成日志无严格诊断。复用B542实际UE9359c9e2，当前库normal/CPU65/pre-submit65仍API4无GPUwait/初态上传/frameGPU，UE完整输出与预算未验；40/48-byte CBV-root/二维调度/GPU producer-consumer仍缺。中间失败与502a结果保留不计最终PASS；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两flagsfalse、未提交推送。下一B544 typed CBV query闭包→实际Lumen，再短UE重放；持续active。

2026-10-07 [PHASE59](PHASE59.md) / [B542](BATCH542_PRIVATE_PLACEMENT_GEOMETRY.md)：补Private/Tracked placement BLAS顶点/索引encoder-end冻结、kind1/2/8初态及帧内重建，复用VK/DX12 typed build-input路径与原预算/退休/范围/compact容量核验。backend/bundlea138d70a、GUI3ba30e36、provideraba8b52b；14新query448事件+4旧heap64事件、714坏组1428无GPUwait拒绝/26合法、56旧回归（含官方两scene270事件10query/六能力）、fresh四Sharedcompact18检查与20controller lifecycle PASS（growth11190272），4994完成日志无严格诊断。新UE9359c9e2/69862532bytes，主TLAS216bytes+两compact kind2 child6859/6863配方齐全（648/288与48/12bytes），另两空TLAS；60AIR全审查/两非零HW query[2,66,1]/[132,1,1]，实际root2为40/48byte CBV表。normal身份guard、CPU65/pre-submit65动态heap-query闭包均API4无GPUwait/初态上传/frameGPU；64heap1491815424/初态1127834757预算未验。保留fixture/审计脚本失败；UE输出/事件/EID0、full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两flagsfalse、无提交推送。下一B543实际Lumen PSO/CBV-root/间接query和GPU producer-consumer声明，再UE整帧；持续active。

2026-10-07 [PHASE59](PHASE59.md) / [B541](BATCH541_PRIVATE_COMPACT_SIZE_AND_UE_AS_INPUT.md)：定位UE空packet真正前置缺口为Private compact-size读取/独立query submission及native compact被跳过；补encoder-end4/8-byte冻结、query build来源、保留native转发且未知replay拒绝。最终backend/bundlef6a8029d、GUI3ba30e36、provider复用aba8b52b。六新placement/compact/空frame sample192事件，四旧heap64事件，168坏组336无GPUwait拒绝/六合法、56回归检查（含官方两scene270事件10query/六能力）、fresh四Sharedcompact18检查及20controller lifecycle PASS（growth3506176），1436完成日志无严格诊断。新UE716106d8，主TLAS216bytes+known两child7097/7101（GPU67/71），两个空TLASbytes0；60AIR全部审查/两非零HW query[2,7,1]/[132,1,1]，nativecompact错误消失，但两BLAS初态recipe缺失。normal/CPU65/pre-submit65仍动态heap-query闭包API4提交前拒绝，UE输出/事件/EID0/frameGPU未验；61heap1419217920/初态1110926467不宣称总预算通过。保留中间失败；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两flagsfalse、无提交推送。下一B542实际堆内BLAS几何/索引冻结与compact子配方，再Lumen PSO/root/indirect/GPU producer；持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B540](BATCH540_FRAME_HEADER_HEAP_QUERY.md)：补coverage65新Shared64-byte Header创建/发布、CPU kind3槽位与当前Private typed TLAS build/query提交顺序及EID0；复用VK/DX12已知对象/冻结输入，未放宽未声明namespace。最终backend/bundlebfeca02b、GUI3ba30e36、provider复用aba8b52b。四新sample native/capture/API/CLI、128事件；四旧heap-query/64事件、六旧动态场景七capture/192事件，422坏组844无GPUwait拒绝/七合法控制、47旧回归（官方两scene270事件10query/六能力）及20 controller生命周期PASS（growth458752bytes），2985日志无严格诊断。复用B538UE c41abb06，最终normal/CPU65/pre-submit65仍提交前拒绝，无新UE/frameGPU/输出验收。新提取实际三TLAS输入options544 Private placement589824bytes、scratch1376256，构建snapshot皆空/children0；不能用standalone sample通过覆盖UE。保留首build/空initialshadow/gate单query计数失败与完整修复复验；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两生产能力false、未提交推送。下一B541实际Private placement每build快照/known-child闭包、GPU producer与sample→新UE，再Lumen PSO/root/indirect契约；持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B539](BATCH539_TYPED_HEAP_QUERY_CONSUMERS.md)：完成coverage65初态AS/CPU kind3槽位/typed query PSO重放，双字段Header及descriptor VA重定位、依赖与EID0；限制frame AS build、GPU Header/贡献改写、未声明消费者，未全局放宽。最终backend/bundle1e13bd74、GUI3ba30e36、provider复用aba8b52b。四新sample native/capture/API/CLI、64事件；六旧动态场景七新capture/192事件，347坏组694无GPUwait拒绝/七合法控制、47旧回归（官方两scene270事件10query/六能力）及20 controller生命周期PASS（growth442368bytes），3762日志无严格诊断。复用B538UE c41abb06，仅最终库normal/CPU65/pre-submit65提交前拒绝frame新Header，无初态上传/frameGPU；无新UE capture/UE输出事件EID0验收。保留helper编译/空TLAS反例/遗漏oracle失败与完整复验；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两生产能力false、未提交推送。下一B540 Header帧内创建/发布+当前AS build/CPU slot消费时序，再实际Lumen PSO/root/indirect/GPU producer与UE验收；持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B538](BATCH538_DYNAMIC_AS_HEADER_SNAPSHOTS.md)：补 StartCapture 最新 header 初态冻结、coverage3 同 Header 帧内显式版本/双字段重定位与提交顺序、Shared placement header和不可变 Private 贡献源，EID0恢复版本/raw。最终backend/bundle b71d6e4a、GUI3ba30e36、复用provider aba8b52b。七场景八capture native/capture/API/CLI、208事件选择/EID0、12 annotation拒绝、195坏组390无GPUwait拒绝/三合法控制、47旧回归（官方两scene270事件10查询/六能力）及20 controller生命周期PASS（growth425984bytes），1469日志无严格诊断。新UE c41abb06/66,705,869bytes、3TLAS/93direct+56indirect/60AIR全审查/两非零HW query；6header身份匹配、三背景snapshot与初态逐字节一致、26kind3记录，三frame header确为帧内新建，owned cleanup -9。65heap1,488,096,256/初态1,122,325,471，未宣称总预算通过；normal/CPU65/pre-submit65仍提交前拒绝UE新header/query闭包，无初态上传/frameGPU，UE输出/事件/EID0未验。保留build/fixture/oracle失败及修复复验；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两能力false、未提交推送。下一B539帧内新header、kind3当前slot/query consumer/依赖闭包，再实际UE验收；持续任务未完成、继续active。

2026-10-06 [PHASE59](PHASE59.md) / [B537](BATCH537_TYPED_AS_HEADER_PROVENANCE.md)：新增显式64-byte AS header对象/贡献来源与静态coverage3双字段重定位，UE factory kind3及实际header write捕获；Shared placement只授予捕获事实。最终backend/bundle2b363cb3、GUI3ba30e36、provider aba8b52b/98exports。八native/capture/API/CLI、152事件/EID0、12 annotation拒绝/同值重复接受、343坏组686无GPUwait拒绝/五合法控制、47旧回归（官方两scene270事件10查询/六能力）通过，2378日志无严格诊断。新UE b35de06d/65,098,798bytes、6header（三frame）/23kind3来源全核对、3TLAS/99direct+56indirect/60AIR全审查/两非零HW query；owned cleanup -9。68heap1,558,989,824/初态1,123,342,185，未宣称总预算通过；CPU65/pre-submit65动态header契约拒绝、无初态上传/frameGPU，UE输出/事件/EID0未验。保留两build/空fixture/首UE placement捕获拒绝；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两能力false、未提交推送。下一B538 typed动态header版本/Private贡献/descriptor producer-consumer及UE query PSO/root闭包，持续active。

2026-10-06 [B536](BATCH536_UE_UINT16_BUFFER_TEXTURE.md)：补RGBA16Uint TextureBuffer8byte创建/读回/PickPixel及authoritative reflection的纯write typedUAV路径；backend/bundle5bbd0636、GUI3ba30e36。3shape六native/capture/API/CLI、408raw/pixel、24seek cycles/88事件选择、真实高位RGBA与RO/RW、零/非零offset、usage及EID0输出清零通过。72格式/范围/初态坏组144上传前拒绝，3writer组6加载期拒绝（发生初态上传，不计GPU前）。旧16格式两份/旧large R32三份API/CLI、新163初态纹理两capture12696subresource、八query/IR64、官方两scene270事件10查询及六能力通过，630完成日志无严格诊断。实际B535UE仅pre-submit65已越过buffer-view创建，下一slot25:27432/gen5508/type4无typedsource，API4且无GPUwait/初态上传；未验UE frameGPU/output/EID0。首三失败/oracle误判保留，生产flagsfalse、guard不放宽；下一B537显式AS-header factory/producer与typed动态header闭包，持续active、未提交推送。

2026-10-06 [B535](BATCH535_UE_SMALL_PLACEMENT_BLOCKS.md)：隔离provider placement block64→16MiB，唯一源码变更MetalBuffer.cpp、98exports完整，新模块ee945869；--rhi明确选择并记录hash。f4f8b745 backend/bundle不变，新UE capture fbe1b7ac/73,738,545bytes、3间接TLAS/96direct+56indirectcompute、60 AIR全部审查/2shader两非零HW query，owned cleanup -9。80heap共1,731,791,872bytes（旧3.557GB），初态1,175,476,527反略增；CPU65首次接受metadata/预算(native1,765,703,936/snapshot12,835,615)，未执行GPU重放。pre-submit65 API4于RGBA16Uint TextureBuffer8192/row65536创建，无GPUwait/初态上传；10保存日志无严格诊断。生产flagsfalse，UE输出/事件/EID0/typed动态header未验，原引擎/用户工程及guard保持；下一B536 typed RGBA16Uint view真实读写sample与旧回归，再当前UE提交前预检。持续active、未提交推送。


2026-10-06 [PHASE59](PHASE59.md) / [B534](BATCH534_UE_UINT16_TEXTURE.md)：coverage65 Private/Tracked R16Uint2D单mip RW≤4096x512，unsigned16 raw/PickPixel与每row32768+y→65535-y；backend/bundlef4f8b745、GUI3ba30e36。4096x16/4096x512/3x5六native/capture/API/CLI、72事件/EID0、ID/usage/全部像素通过；八像素/线程守262144预算，20坏组40拒绝。旧78texture/view/heap/volume、八query/四IR、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；616日志无严格诊断。B533实际新UE CPU65已越过frame创建，native3,593,011,456/initial1,170,276,095/snapshots12,957,904，GPU静态总预算拒绝，无frame GPU。两flagsfalse，下一B535独立provider16MiB block及实际低heap新UE，保留安装引擎/原用户工程和guard；旧B528 sRGB仍未支持，typed动态header/producer待补。持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B533](BATCH533_UE_SMALL_LUMEN_CACHES.md)：新增隔离--small-lumen-caches及运行cvar数值/来源检查，surface512²/grid4/probe8/atlas32实际生效；dba15ac3库，newUEcapture9238d0c3/70,728,712bytes、3间接TLAS/99direct+56indirectcompute，60 AIR全审查0unvalidated、2shader/2非零HW query [2,64,1]/[132,1,1]，owned cleanup -9不计正常退出，输出/重放未验。52heap3,556,769,792bytes比B528增加，初态blob1,170,276,095减少，heap alone超过3GiB-128MiB硬上限，不宣称预算达标。normal/CPU65 API4且无GPUwait/严格诊断，下一R16Uint2D4096x16 usage3；源码找到isolated provider block64MiB（原installed512MiB），可独立减小粒度、guard保持。两能力false，下一B534实际R16Uint二维样例，再有界provider heap粒度与typed动态header/sRGB，未改原用户工程/installed engine、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B532](BATCH532_UE_PACKED_RENDER_TEXTURE.md)：coverage65 RGB10A2Unorm2D≤512单mip读写可带RT7；backend/bundledba15ac3、GUI3ba30e36。三shape六native/capture/API/CLI、96事件/EID0、native clear读32/96/160、每像素packedbits/PickPixel及独立clear EID往返通过；20坏组40拒绝。旧72texture/view/heap/volume、八query/四IR、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；592日志无严格诊断。实际B528UE仅CPU65越过RGB10A2 usage7，下一BGRA8Unorm_sRGB2D320x240 usage7拒绝；无新UE/GPU/full/Qt-ARC，两能力false。下一B533实际小Lumen缓存新UE截帧并核验保留HW query及成本，再补sRGB与typed动态header/producer，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B531](BATCH531_UE_LARGE_PACKED_TEXTURE.md)：补coverage65 Private/Tracked RG11B10Float二维单mip RW≤4096²，wholeheap/总预算保持；backend/bundle3f7e0854、GUI3ba30e36。4096²/2048x1376/3x5六native/capture/API/CLI、72事件/EID0、全部packedbits/PickPixel/usage/ID通过；每线程≤8x8像素守262144调度预算，20坏组40拒绝。旧66texture/view/heap/volume、八query/四IR、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；568最终日志无严格诊断。实际B528UE仅CPU65越过大packed2D，下一RGB10A2Unorm二维320x240 usage7拒绝，无新UE/GPU/full/Qt-ARC；两能力false，下一B532 packed二维RT用途与clear/sample，随后小Lumen cache/typed动态header/producer及预算闭包，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B530](BATCH530_UE_PACKED_FLOAT_VOLUME.md)：补coverage65 Private/Tracked RG11B10Float单mip RW3D≤64³，wholeheap/预算保持；backend/bundle99df52e4、GUI3ba30e36。三shape六native/capture/API/CLI、72事件/EID0、每z独立精确11-bit值/全部packedbits/PickPixel/usage/ID通过；20坏组40 GPU前拒绝。旧60texture/view/heap/volume、八query/四IR API/CLI、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；544完成日志无严格诊断。首轮wrong_format=92与原RG11B10Float值相同，合法OpenCapture成功，保留FAIL，不声称GPU前拒绝；换Depth32Float3D完整复验。实际B528UE仅CPU65越过packed3D，下一RG11B10Float二维4096x4096 RW拒绝，无新UE/GPU/full/Qt-ARC；两能力false，下一B531有界大packed二维sample，再动态header/producer/预算闭包，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B529](BATCH529_UE_UINT_VOLUME.md)：补coverage65 Private/Tracked RW单mip R32Uint3D≤256x64x64、array1，复用typed placement/lifetime/预算。backend/bundleaee8c20d、GUI3ba30e36；3shape六native/capture/API/CLI、72选择/EID0、各z不同uint/全体素/PickPixel/两write+read usage/不同ID通过；20坏组40拒绝；fresh整数array/2Datomic四capture、旧50texture/view/heap+四floatvolume API/CLI、八query/四IR、官方2scene/270事件/10查询/六能力/CPU预算通过，524完成日志无严格诊断。保留首次sample每dispatch超262144预算拒绝（改每线程≤4相邻体素、guard不变）及fresh raw-source无AIR usage缺失失败（改对应编译metallib后通过）；此前已通过54旧纹理没有重复跑，不将失败计通过。原B528UE仅CPU65越过R32Uint，下一RG11B10Float3D10x8x26拒绝；没有新UE GPU/full/Qt-ARC。两能力false，下一B530 packed-float volume实际sample，再动态header/producer/预算闭包，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B528](BATCH528_UE_BOUNDED_SHADOW_SCENE.md)：补UE isolated small-shadow命令配置，实际cvar256/256/1与Depth16 frame256x256核验，越过B527 8192x2048拒绝；新capture9fa35417/62,399,341bytes、6间接TLAS/186direct+112indirectcompute，60 AIR条目全部审查、2shader/4非零RayQuery；owned capture cleanup -9不计正常退出，输出与GPU重放未验。43heap2,962,489,344bytes，初态blob1,608,781,321，未证明总预算通过（比B527初态增加，不宣称单调优化）。CPU65下一拒绝R32Uint3D192x48x48/usage3，normal frame-born identity拒绝，两API4无GPUwait/严格诊断。backend/bundlee231e2bd、GUI3ba30e36，两能力false，无driver guard放宽、原用户工程/installed engine未改。下一B529实际R32Uint3D frame extent与native/sample反例，再typed动态AS header/producer及预算闭包；持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B527](BATCH527_UE_SMALL_RAY_SCENE.md)：新增隔离3 mesh/2 light Blueprint工程与有限NullRHI生成器（两关卡保存通过），禁用无关默认插件并配置必需SkinCache；小UE有限capture成功、owned cleanup -9，不计正常退出。e231e2bd库，capture1b19090e/56,406,168bytes，3间接TLAS/99direct+56indirectcompute/163编译完成；60原metallib AIR全部审查、2真实非零RayQuery [2,72,1]/[786,1,1]，输出和重放未验。45heap共3,029,598,208bytes（旧5,266,571,264），初态blob1,227,522,505（旧4,362,319,907）；未证明总预算通过。normal-open API4因frame-born identity，CPU65 API4因Depth16 8192x2048 frame texture，均无GPU wait/断言；保留首轮缺SkinCache Fatal及第二轮startup timeout，未计通过。原用户工程/installed engine未改，两能力false。下一B528小场景CSM分辨率/成本验证及实际typed producer闭包，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B526](BATCH526_INLINE_QUERY_HEADER_RANGES.md)：query解析声明buffer+8对齐offset中的64-byte Shared AS header，保留whole backing≤16KiB/不可变来源/typed字段/地址唯一性；原chunk格式保持。backend/bundlee231e2bd、GUI3ba30e36；八native/capture/API/CLI、176事件/EID0、offset32/4096/16304与最后16字节保护区、frame/newTarget/multi64组合通过；236坏组472无GPU wait拒绝、五合法控制。旧query28（B524八份API，其余20 API/CLI）、TraceRay四份API/CLI、官方2scene/270事件/10查询/六能力通过，1734完成日志无严格诊断。旧texture/full/32CPU-AIR/Qt-ARC/官方坏13未重跑，无UE GPU。动态header、heap/nested绑定、producer及UE预算仍待补；下一先隔离低工作集UE场景和新截帧再补实际暴露缺口，两能力false、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B525](BATCH525_INLINE_QUERY_NEW_TLAS.md)：query允许顺序验证的帧内新TLAS，无初态目标仅在typed冻结build提交后可用；独立前后Shared header/roots。backend/bundle99e80e2d、GUI3ba30e36；四native/capture/API/CLI、96事件/EID0、不同ASID/VA/两输出及前后usage通过；176坏组352无GPU wait拒绝含缺build/错target/提前query，四合法控制；旧query24 API（此前B524八份仅API）及旧16 API/CLI、TraceRay四份API/CLI、官方2scene/270事件/10查询/六能力通过，1280严格日志无诊断。保留首次MTLResourceID类型编译失败，._impl修复后全验；旧texture/full/Qt-ARC未重跑，无UE GPU。源码确认UE TLAS header为Shared动态子分配（纠正此前Private header猜测），下一补有界Shared header偏移、typed producer及低工作集UE；两能力false、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B524](BATCH524_INLINE_QUERY_FRAME_GEOMETRY.md)：query消费Private帧内BLAS当前kind1/2/8配方，复用冻结验证及几何顺序/索引/finite约束。backend/bundle4d350b89、GUI3ba30e36；八native→capture→API/CLI、192事件/EID0、64几何+64实例最后命中4398、源清零、前后VA/ASID/usage通过；499坏组998无GPU wait拒绝、十合法控制通过；合法65实例native/capture成功而重放按上限拒绝，未声明控制拒绝。旧query16/TraceRay4、官方2scene/270事件/10查询/六能力通过，3358完成日志无严格诊断。保留首次读回buffer缺初态拒绝，明确零初态后完整复验；未放宽预检。旧50texture/full78IR/75RT/308/旧84/32CPU-AIR/Qt-ARC/官方13反例未重跑，无UE GPU，不继承旧hash。下一B525新TLAS目标，再Private/GPU header/typed producer与低工作集UE；两能力false、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B523](BATCH523_INLINE_QUERY_FRAME_TLAS.md)：query消费同一已初始化TLAS的Private帧内typed冻结当前配方，子BLAS仍初态；Shared roots/header immutable。backend/bundlef71629a0、GUI3ba30e36；六native→capture→API/CLI、144事件/EID0、UserID73→74(2241→2258)、64实例3312→3329及空/屏蔽0→命中、两输出usage/两roots VA/保护字/AS-ID/BLAS前后usage/Private source清零与EID0通过；239坏组478 API/CLI无GPU wait拒绝、六合法UserID75/child-ID重编号控制通过。旧query十份/TraceRay四份、官方2scene/270事件/10查询/六能力及未声明控制通过，1744完成日志无严格诊断。保留oracle SDObject编译失败及合法控制漏CPU快照两次拒绝，修构造并完整复验。旧50texture/full78IR/75RT/308/旧84/Qt/ARC未重跑，UE预算阻塞未变化、本轮无UE GPU。下一B524 query消费Private帧内BLAS几何配方，再Private/GPU header/typed producer与小工作集UE；两能力false、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B522](BATCH522_INLINE_QUERY_INDIRECT_TLAS.md)：inline query复用既有kind9/10/11间接/空/全屏蔽TLAStyped初態、72→68-byte UserID转换；保持Shared静态roots/header/output，子BLAS初态不可变。backend/bundle6f8bab50、GUI3ba30e36；八份native→capture→API/CLI、128事件/EID0/不同VA-ASID/保护字、UserID73→2241与64实例最后UserID136→3312、Private输入上传后88/5128bytes清零、空/屏蔽全0通过；149坏组298 API/CLI预提交拒绝、无GPU wait；旧query两份/TraceRay四份、官方2scene/270事件/10查询/六能力和未声明拒绝控制通过，1108完成日志无严格诊断。旧50纹理/full78IR/75RT/308/旧84/Qt/ARC未重跑、不继承旧hash；UE预算阻塞未变化未无效重跑，本轮无UE GPU。Private/GPU header/heap query/typed producer和帧内TLAS更新仍缺，下一B523复用已捕获的帧内冻结配方与事件状态；两能力false，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B521](BATCH521_CONVERTED_INLINE_QUERY.md)：补独立converted inline RayQuery的不可变PSO→roots/offset/header/output声明、typed AS-ID/VA闭包、slot2调度与usage/初态/EID0；没有伪造IR TraceRay packet/SBT。backend/bundlee72514c6、GUI3ba30e36；root0/8两份native→capture→API/CLI、四结果1000/1000/0/0、32事件/EID0/强制VA和AS-ID变化/保护字通过；39坏组78 API/CLI拒绝且无GPU wait，未声明控制拒绝。旧50纹理/view/heap API/CLI、旧view四份native capture replay、四个Private/frame/multi/local/heap IR、fresh default TraceRay、官方两scene/270事件/10查询/六能力/CPU预算通过；588最终日志无严格诊断。初始fixture崩溃及编译/检查器失败保留，AS标签未捕获、同CB BLAS→TLAS仍拒绝。当前仅Shared静态普通TLAS≤64、1D直接query，UE Private/GPU header/typed producer/indirect TLAS/frame更新/低工作集仍待补；原UE CPU总预算拒绝，未跑UE GPU/完整78IR/75RT/308/旧84反例/Qt/ARC，两能力false。下一先复用现有typed间接TLAS初态供query，再补实际UE header和producer；持续active、未提交推送。

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

2026-10-06 [PHASE59](PHASE59.md) / [B508](BATCH508_INITIAL_LIST_AND_DEVICE_CLOSE.md)：修owner device在manager Shutdown前解除注册，并覆盖CPU structured-export dummy；按DX12/Vulkan WrittenRecord格式解码初始资源列表，保留ID/written而不调用未实现的generic初态。backend/bundle5f4db1c7、GUI3ba30e36；最终串行51组native/capture/API3147事件/EID0/CLI3、706坏IR/53控制、1坏初始列表/1往返控制、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13反例/6能力通过。40重开growth409600bytes，另30失败+10成功/10CPU导出/30seek/reset growth1081344bytes；有效2326日志无旧device/资源表/断言诊断，CPU/native LLDB owner-release断点均未命中且exit0。新测试日志锁及零长度坏chunk构造失败保留；B507已存在的空TLAS反例chunk长度错误已修正，5间接组156/14重跑无断言，两个CPU互斥检查与语法/diff通过。IR帧内AS仍被提前拒绝，下一当前配方/preflight/EID0与可靠UE producer；UE/75 RT/集中/原Qt/ARC/独立旧table未跑，死机根因未修、能力false，持续active、未提交/推送。

2026-10-06 [PHASE59](PHASE59.md) / [B507](BATCH507_IR_INDIRECT_TLAS.md)：补IR global/heap header消费kind9间接、kind10空、kind11全屏蔽TLAS，复用72→68-byte typed子BLAS/UserID转换与IFT校验；InstanceID73真实输出146/11，Private/GPU输入建AS后清零仍正确，heap-only34/183、局部266/241、全局+局部1477/1452。backend/bundleee222fe1、GUI3ba30e36；最终串行51组native/capture/API3147事件/EID0/CLI3、706坏IR/53控制（新间接156/14）、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13坏capture/6能力及40有限重开(growth1589248bytes) PASS，hash一致。60份新增SDK反射/冻结recipe/UserID/Private清零及CPU语法/diff/新gate锁通过；合法UserID74输出147/11。测试ID强制变化、unused-AS usage、脚本blobs初始化、诊断helper编译和SDK枚举期望修正，失败保留。当前仍IR静态契约，所有帧内AS encoder提前拒绝；成功日志已有eResDevice关闭错误（B506亦存在），须修owner清理且不把有限重开计完整生命周期。UE/75 RT/集中/原Qt/ARC/旧独立table未跑，系统冻结根因未修、能力false。下一owner device关闭与帧内AS配方/preflight/EID0，随后可靠UE producer/typed view等；持续active、未提交/推送。

2026-10-06 [PHASE59](PHASE59.md) / [B506](BATCH506_IR_HEAP_ACCELERATION_STRUCTURES.md)：补IR资源heap AS-header kind3(bytes64、metadata0)，共用TLAS/BLAS/贡献/IFT typed闭包；支持无直接AS根的真实TraceRay，第二TLAS平移交换命中，输出34/110、局部266/168、全局+局部1477/1379。backend/bundle b0094506、GUI3ba30e36；最终串行36组native/capture/API2277事件/EID0/CLI3、550坏IR/39合法控制（新AS115/12）、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/**13坏官方capture**/6能力、40生命周期(growth933888bytes) PASS，起止hash一致。64份SDK反射(新AS36份)/heap-only真实UAV+常量根、CPU语法/diff/新gate互斥PASS。首个帧内坏header测试构造无效已修正为实际chunk注入，失败保留；B505官方反例误写41已按原日志纠正13。IR仍只接受direct TLAS kind5，实际UE用Indirect；heap CBV/UAV/typed view、nested UB/大heap/Private或GPU IR/动态heap及可靠UE producer关联仍缺。UE/75 RT/集中/原Qt/ARC/独立旧table未跑，死机根因未修、能力false，下一补IR间接TLAS及typed view；未提交/推送、持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B505](BATCH505_IR_DESCRIPTOR_HEAPS.md)：补immutable PSO→IR资源/sampler heap entry声明，直接索引raw SRV view/texture/sampler、标量buffer-size metadata、动态slot及零洞，full typed闭包/usage/重定位。backend/bundle1b40caee、GUI3ba30e36；27组native/capture/API1635事件/EID0/CLI3，heap96/48、local154/280、global+local1365/1491，强制AS-ID/VA/texture变化通过；435坏IR/27合法控制、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13坏sample（B506按原日志纠正）/6能力/40生命周期(growth1769472bytes) PASS，最终全串行。28份SDK reflection/root flags3072和新gate互斥CPU检查PASS；SDK reflection不提供直接索引heap成员，须producer已知类型，不推断位型。首次编译Max断言未更新已修，失败日志保留。heap AS/CBV/UAV/typed texture-buffer view、nested UB、大heap/Private或GPU IR/动态heap及可靠UE shader自动关联仍缺；UE/75 RT/集中/原Qt/ARC/独立旧table未跑，死机根因未修，能力false。下一真实heap AS/view与隔离UE producer接入；持续active、未提交/推送。

2026-10-06 [PHASE59](PHASE59.md) / [B504](BATCH504_IR_GLOBAL_ROOTS_AND_REFLECTION.md)：补多根GRS及immutable PSO→global CBV/SRV/constants/texture/static sampler/AS-header/UAV布局，8-word真实GRS与global+local组合。backend/bundle5566f91b、GUI3ba30e36；最终串行20组native/capture/API1155事件/EID0/CLI3，输出1284/1222、组合1342/1454及any-hit1454/1454；强制AS2→3/VA/global texture1→2/local2→3，325坏IR+21合法控制、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/41坏sample/6能力/40生命周期(growth1441792bytes) PASS。20份SDK真实reflection布局/entry/metallib hash与IR gate互斥CPU检查PASS。首轮短暂GPU测试重叠已保留、不计验收，新增互斥后完整串行重跑。UE普通shader离线转换→运行时metallib，仅compiler hook不能可靠关联；heap/嵌套UB/Private或GPU IR/callable仍缺、UE/75 RT/集中/原Qt/ARC/独立旧table未跑，死机根因未修、能力false。下一真实IR资源heap及可靠UE producer shader布局接入；未提交/推送、持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B503](BATCH503_IR_LOCAL_ROOTS_AND_STATIC_SAMPLERS.md)：补immutable shader→local CBV/SRV/constants/texture table/static sampler布局，96-byte SBT及typed资源链。backend/bundle285f8768、GUI3ba30e36；15组native/capture/API810事件/EID0/CLI3，局部输出131/243与any-hit243/243，UE同六sampler(slot3)输出通过，texture1→2及合法sampler285→29/288→32通过。185坏capture+15合法控制、18旧targeted/41旧argument/6旧packet/旧B501与B502 IR、官方2scene/270事件/10查询/41坏sample/6能力查询、40生命周期(growth1769472bytes) PASS。中间转换/编译/测试构造失败保留；未跑UE/75 RT/集中/原Qt/ARC/独立旧table。global仍2根，heap/callable/Private或GPU IR/嵌套UB/非零texture-sampler metadata未支持；死机根因未修、能力false。下一多根GRS/IR资源表与UE自动关联接入；显式fixture注解不冒充通用UE。未提交/推送、持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B502](BATCH502_IR_NULL_SHADERS_AND_ANY_HIT.md)：修正 VisibleFunction any-hit 的 VFT/几何IFT关联、保留UE pad0标量、允许空miss/closest-hit，并补immutable shader role声明及错角色拒绝。backend/bundle28d2a162、GUI3ba30e36；9组native/capture/API432事件/EID0/强制AS-ID/VA变化/CLI3，69坏capture+7合法控制、18旧targeted/41旧argument/6旧packet/B501旧IR、官方2scene/270事件/10查询/41坏sample/6能力查询、40生命周期(growth1900544bytes) PASS。原测试构造失败保留。未跑UE/75 RT/集中/原Qt/ARC/独立旧table；local root/static sampler/heap/GPU IR未支持，死机根因未修、能力false。下一扩真实fixture的local root/static sampler，禁止旧长UE/full；未提交/推送、持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B501](BATCH501_TYPED_IR_RAY_DISPATCH.md)：完整typed IR packet/SBT/GRS/AS-header重定位与useResources(TLAS)的BLAS初态依赖已补，首次转换TraceRay native/capture/replay73/11、16事件三方向/EID0/资源usage/强制AS ID2→3及VA变化PASS。最终backend/bundle10c28bf1、GUI3ba30e36；54坏capture+合法stride32、官方2scene/270事件/10查询/41坏sample/6能力查询、18旧targeted/41旧argument反例/6旧packet、旧typed table10反例及4×10生命周期(growth1343488bytes) PASS。首个schema仅Shared静态、两直接根、32-byte records/无static sampler或local root；UE未验，75 RT/集中/原Qt/ARC未跑，死机根因未修、能力false。下一用真实fixture补UE pad0=~0ull/Null shader identifier，再扩local root/static sampler/GRS，禁止旧长UE/full。失败日志和最终hash清单保留，未提交/推送、目标active。

2026-10-06 [B500](BATCH500_TLAS_PACKETS_AND_CONVERTED_DXR_SAMPLE.md)：补TLAS参数GPU路径；12例BLAS/TLAS native/capture/API792事件/EID0/CLI、294坏输入、4帧内明确拒绝、18旧targeted/41旧反例/130生命周期(growth327680bytes) PASS。新增UE5.8.3 DXC→Apple IR真实TraceRay样例，RaygenIndirection+VFT/IFT、152-byte参数/32-byteSBT ID/64-byteAS header，native与精确probe capture命中73/未命中11；CPU完整ABI/来源及1次native编译审计PASS。IR离线明确拒绝无声明raw地址，未修重定位、不计replay成功。backend/bundle仍03fc7b32、GUI3ba30e36，B499同hash官方证据有效，本批未重跑。UE/75 RT/集中/原Qt/ARC未验，死机根因未修、能力false。下一直接用converted fixture补完整typed IR packet/SBT/GRS/AS header声明重定位，禁止绕过guard/直接旧长UE/full。失败日志保留，未提交/推送，目标active。

2026-10-06 [B499](BATCH499_TYPED_RAY_ARGUMENT_RESOURCES.md)：补MTLArgumentEncoder AS/IFT typed编码和AS/IFT/VFT初态重新编码，compute调度前验证成员、已构建AS种类与函数表PSO，补CS usage。六例Shared/Managed、function/device、标量/函数表数组native/capture/API396事件/EID0/CLI PASS，120坏输入及2帧内明确拒绝PASS；18旧targeted/41旧反例/70生命周期(growth409600bytes) PASS。新backend/bundle03fc7b32、GUI3ba30e36；当前库官方2scene/270事件/10查询/41坏sample/6能力查询PASS。UE raw IR/SBT尚未实现，instance参数GPU/嵌套/render/帧内重编码未验或不支持；UE/75 RT/集中/原Qt/ARC未跑，死机根因未修，能力false。下一typed TLAS packet和缺失/错误AS种类反例→完整UE IR声明重定位，禁止直接旧长UE/full。未提交/推送，持续任务未完成。

2026-10-06 [B498](BATCH498_SIX_NATIVE_RAY_COMPILE_APIS.md)：六种sync/async compute创建入口均用真实opaque triangle ray query验证1/0；exact1 trace有6begin/6end/3submitted、callback native线程可关联，unset/0/true/空关闭。enabled capture32事件×3/EID0/PSO+AS绑定、23坏creation、19旧targeted、30生命周期(growth589824bytes) PASS。修正生命周期检查漏认帧前AS无IFT inline ray；失败日志保留。UE IR实际header CPU布局152/104/32bytes，记录完整SBT/GRS/函数表/嵌套record依赖，未实现IR重定位。backend/bundle未改仍eea2539f，GUI3ba30e36，B497同hash官方证据有效；本批UE/75 RT/集中/原Qt/ARC未跑，死机根因未修，能力false。下一完整typed IR声明与重定位，禁止直接旧长UE/full；未提交/推送。

2026-10-06 [B497](BATCH497_FUNCTION_TABLE_GPU_IDENTITY.md)：按DX12 shader identifier关联/VK group handle原生转发补VFT/IFT gpuResourceID及typed依赖，查询未绑定表也捕获；没有实现UE raw IR packet重定位。最终backend/bundle eea2539fb069dae9d59d2c32ad962c562ee83aecb09d57be195e50538f123c57，GUI3ba30e36。四例native/capture/API714次事件/EID0/CLI PASS，77坏capture/8合法副本、3份旧RT408事件、18份旧函数表targeted、50生命周期(growth589824bytes) PASS；官方2scene/270事件/10尺寸/41坏sample/6能力查询PASS。四次真实descriptor-sync取证完整，新增系统native thread ID和日志shared-lock保留；10+21项CPU、preflight/语法/diff PASS。早期编译/日志/测试oracle失败均保留。UE/75 RT/集中/原Qt/ARC未验，死机根因未修，两能力false。下一UE IR typed ABI及六入口有限诊断，继续禁止直接旧长UE/full followup；未提交/推送，持续任务未完成。

2026-10-06 [B496](BATCH496_NATIVE_COMPUTE_COMPILE_TRACES.md)：新增仅capture且精确env=1的native compute编译取证，覆盖function/function-options/descriptor三类同步及三类异步。begin/submitted/native end按process/token关联，记录线程、函数名/label bytes、linked表数量、options/stackDepth、native结果与耗时；async可识别调用未返回/等待callback，不改变native调用次数、ABI、回放格式或支持范围。监督器退出时保存compile-trace.json；9项协议审计+20项监督/INI CPU测试PASS，串行renderdoccmd/app构建及preflight/语法/diff PASS。最终backend/bundle acd0c0d697b41c83390419b972e0a508358226371f4ced65e111123519369c14，GUI3ba30e36；111c4787已保留baseline。旧UE日志没有新trace，结果NOT OBSERVED，不能定位旧待编译函数。未运行GPU/UE/sample/75 RT/集中，新库不能继承111或08028旧库验收。两能力false；死机根因未修，先CPU推进UE IR/函数表typed ABI与编译证据，不直接重跑旧长UE/全量。持续任务未完成、未提交/推送。

2026-10-06 [B495](BATCH495_UE_RT_FREEZE_DIAGNOSTICS.md)：系统重启取证与UE诊断监督。23:50:44启动的111c4787 UE session在frame127停滞；WindowServer连续40s未checkin，UE线程等待高CPU的MTLCompilerService，forceReset为btn_rst/force_off。未证实GPU内核panic或具体根因。新增12s帧进展检测（同帧后台日志不续期）、1s栈取证/2s采样上限、owned group清理及outer timeout收尾，原子状态记录；修复RT隔离启动INI沿用1728×1020最大化窗口，固定640×480且原设置hash不变。18项CPU失败注入/INI测试、preflight/语法/diff PASS；重启后未启动UE/GPU、未改后端二进制。B494官方2scene/10查询/41坏sample/6能力查询已PASS；UE截帧未完成，75份RT/本库集中未运行，manifest已纠正INTERRUPTED。backend/bundle111c4787c2a7fddf59bdd7503e2ea89f8227c23d72962f584942cb7e2e65a14d，GUI3ba30e36。公开能力false。接续先CPU定位待编译管线/RT ABI及补取证；不要直接重启旧run_followup.py、全量或长UE运行。监督器不能保证阻止系统级死锁，根因未修复，持续任务未完成。

2026-10-05 B494定向验收：帧内Private/Tracked（独立或placement）间接实例输入按AS encoder消费点冻结，提交完成后保持原API chunk位置/metadata写入；重放typed live child/UserID staging或全禁用Indirect。精确范围别名仅在已验证冻结build、native encoder关闭且无descriptor backing冲突时允许，EID0清空资格。最终backend/bundle 111c4787c2a7fddf59bdd7503e2ea89f8227c23d72962f584942cb7e2e65a14d，GUI3ba30e36。七例native/capture/API954事件/EID0/CLI3 PASS，含同TLAS两build原始userID73/74、两种inactive完整bytes oracle及两份合法CPU别名提前创建控制；5×43+2×37+6=295坏输入/alias反例、20旧检查/T12416及8×10生命周期PASS（growth1081344bytes/起止hash一致）。产物captures/metal-ray-b494、metal-ray-b494/indirect-manifest.json。两次实现/判定FAIL日志保留，不计通过。官方/UE/75份RT和本库集中尚未跑；08028e4c集中308/7784/3080/growth0属于旧库。公开能力false，ARC/原Qt crash/UE RT未闭环，未提交/推送。

B493 08028e4c接续验收：官方两scene/10尺寸查询/41坏sample/6能力查询、68份B482–493 RT7785事件、AS身份旧反例/schema2/旧Indirect与空TLAS反例PASS；固定库集中308/7784/3080 PASS，growth0bytes/exit0/工作和冻结库起止hash一致。产物frozen-validation-08028e4c/full-regression.log与metal-ray-b493/followup-manifest.json。UE session20261005-225938进入截帧后Private/Tracked placement、54实例、background0被bridge主动ForceCrash拒绝；仍未证明RT dispatch/输出/离线。B494已编辑，待串行构建/GPU，不能计此证据；生产能力false。

B494开发检查点：08028e4c实际UE session20261005-225938越过零候选并初始化，截帧54-instance Private/Tracked placement输入在background=0被bridge拒绝。新增帧内encoder-end快照、保持原chunk顺序/API metadata的延后序列化、typed snapshot重放，六场景脚本已编辑，未构建/未GPU验收；B493集中仍串行运行，不重叠构建。公开能力false，UE RT输出/离线未证明。

2026-10-05 B493定向验收：全禁用Indirect槽位kind11/schema9，仅接受每packet ASID0/mask0，完整transform/flags/IFT offset/userID冻结。最终backend/bundle 08028e4c7e2048c3ac37d0de00d712de139c3026b4960d8c836d4cc12f3b255f，GUI3ba30e36。五例Shared帧前/帧内、Private/placement帧前、同CB distinct alias擦零：native/capture/API588事件/EID0/CLI及完整72-byte snapshot oracle PASS；4×43+36=208坏输入、20旧检查/T12416反例、6×10生命周期PASS（growth425984bytes/hash一致）。产物captures/metal-ray-b493、build-macos-debug/metal-ray-b493/indirect-manifest.json。官方/UE/68份RT与本库集中接续中；ARC/原Qt crash/GUI/UE RT未闭环，公开能力false，未提交/推送。

B493开发检查点：native inactive探针已证实count1、ASID0/mask0可构建并ray miss；产物build-macos-debug/metal-ray-b492/inactive-native-probe.log。新增kind11/schema9全禁用槽位配方、Private执行点冻结及完整72-byte packet oracle，五场景与反例脚本已编辑/语法检查；尚未构建或GPU验收。当前仍ae81e24a，公开能力false。实际UE的零候选输入需新诊断，不推定语义。

B492 ae81e24a接续结果：官方两scene/10尺寸查询/41坏sample/6能力查询、63份B482–492 RT7197事件、AS身份旧反例/schema2/旧Indirect46反例PASS，冻结/工作库起止hash一致。UE session20261005-222357已越过zero count，下一FAIL为private间接primitive候选count0（limit1024），instanceCount尚需补诊断，不能推定为null AS。当前库集中未跑；B491 e6集中308/7784/3080/growth0 PASS仅旧库。下一Native inactive instance探针及UE精确候选/参数诊断；生产能力false。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B492](BATCH492_RAY_EMPTY_INDIRECT_TLAS.md)：补Indirect零count尺寸/heap查询及kind10/schema8空TLAS配方，不读输入、不猜子AS，严格offset<length。backend/bundle ae81e24a，GUI3ba30e36。五例Shared帧前/帧内、Private/placement帧前及同CB alias：native/capture尺寸一致、ray miss0/0/0/0、API588事件/EID0/CLI、169坏输入、20旧检查/T12416反例与60生命周期PASS（growth589824bytes/hash一致）。官方/UE/63份RT接续中，本库集中未跑，e6固定集中308/7784/3080/growth0 PASS为旧库。ARC/原Qt crash/GUI未闭环，两能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B491](BATCH491_RAY_MANY_INDIRECT_CHILDREN.md)：间接TLAS typed子AS预算1024（GPU实际最多128已验），直接仍4；固定native数组越界已修。backend/bundle e6f18f11，GUI3ba30e36。五例native/capture/API588事件/CLI、226坏输入、20旧检查/T12416及60生命周期PASS（growth327680bytes）；官方两scene/10查询/41坏sample/6能力查询、58份RT6609事件、身份旧反例/schema2和集中308/7784/3080 PASS，集中growth0bytes/exit0/起止hash一致。UE session20261005-215022已越过多候选guard，下一FAIL为Private placement间接count=0；B492源码已编辑，等待native基线及新构建验证，不计本证据。ARC/原Qt crash/GUI未闭环，两能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B490](BATCH490_RAY_PLACEMENT_INSTANCE_INPUTS.md)：帧前Tracked placement heap间接实例输入已冻结并按kind9独立packed初态恢复；区分原生birth aliasability与显式退休，scratch/AS输出同heap重叠拒绝。backend/bundle0495cfde，GUI3ba30e36。七例native/capture/API714事件/CLI、322坏输入、20旧检查/T12416反例及8×10生命周期PASS（growth311296bytes）；distinct alias同CB/后来CB擦零通过。官方两scene/10查询/41坏sample/6能力查询PASS；UE越过heap guard后FAIL于1–4候选检查（session20261005-210821），固定库集中308/7784/3080 PASS，growth13074432bytes/hash一致。仍≤4候选、帧内Private/Untracked/ARC/原Qt crash/GUI未验，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B489](BATCH489_RAY_PRIVATE_INDIRECT_SNAPSHOTS.md)：Tracked独立Private间接实例已按AS encoder执行点冻结并按真实packet筛选typed子AS，帧前schema7恢复。backend/bundle349df416，GUI3ba30e36；八例native/capture/API816事件/CLI、368坏输入、20旧检查/T12416反例及9×10生命周期PASS（growth327680bytes）。官方两scene/10查询/41坏sample/6能力查询PASS，UE新诊断FAIL于间接实例输入guard（session20261005-203243），下一补精确诊断；本库集中全量未跑，c924730b集中308/7784/3080 PASS仅为旧库。帧内Private/heap/untracked/超过4候选仍未支持；ARC/原Qt crash/GUI未验，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B488](BATCH488_RAY_TYPED_INDIRECT_INSTANCES.md)：Shared Indirect实例按查询过的typed AS ID关联并规范化为UserID+live child handles，schema7初态/帧内/compact/copy已补；backend/bundle c924730b，GUI3ba30e36。七例native/capture/732事件/CLI、318坏输入、20旧检查/T12416反例、8×10生命周期PASS（growth720896bytes）。官方/38份RT/4491事件/身份旧反例/schema2及集中308/7784/3080 PASS，growth4653056bytes/hash一致；B486 a2e87831集中308/7784/3080 PASS/growth0为旧库。下一Private/GPU实例，UE同一guard不重跑，ARC/原Qt crash/GUI未验，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B487](BATCH487_RAY_USER_ID_INSTANCES.md)：补UserID直接TLAS、source偏移/填充步长及schema6初态，保留完整uint32编号；backend/bundle26303b3c，GUI3ba30e36。七例native/capture/732事件/CLI、284坏输入、20旧检查/T12416反例、8×10生命周期PASS（growth458752bytes）；官方两scene/10查询/41坏sample/6能力查询、24份旧RT/3027事件、7份Managed/flags及schema2兼容/24坏TLAS复验PASS。本库全量/UE/GUI未跑，B486 a2e87831固定308/7784/3080 PASS、growth0/hash一致。下一Shared间接实例typed关联；Private/Indirect/ARC/原Qt crash仍未闭环，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B486](BATCH486_RAY_ENCODER_INPUT_SNAPSHOTS.md)：补齐Tracked独立Private多indexed输入在AS encoder结束位置冻结，支持同CB GPU上传及构建后覆盖；backend/bundle a2e87831、GUI3ba30e36。四例408事件/CLI、120坏输入、20旧检查/T12416反例、5×10生命周期PASS（growth409600bytes）；Shared同CB边界native/capture正确、离线预期干净拒绝，未扩大支持。官方两scene/10查询/41坏输入/6查询PASS，固定库24份RT/3027事件及集中308/7784/3080 PASS、growth0/hash一致。Indirect TLAS原生四例0/73/0/73通过、旧库capture SIGTRAP失败，UE/ARC/原Qt crash/GUI未验，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B485](BATCH485_RAY_COMPUTE_VFT_CAPACITY.md)：修复UE触发的compute VFT count>32限制，按65536项重放分配预算保留原生创建结果；不扩大IFT offset/render能力。backend/bundle80c2d706，GUI3ba30e36。四例真实ray调用33/256/65536表末槽native/capture/705事件/CLI、32坏输入、20旧检查及T12416反例、5×10生命周期PASS（growth196608bytes）；官方两scene/10查询/41坏输入/6能力查询PASS。UE越过VFT后失败于Indirect TLAS构建；固定库20份RT/2619事件复验PASS，本80c2d706集中308/7784/3080 PASS、growth0bytes、起止hash一致。ARC/原用户crash/UE RT调度与离线未验，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B484](BATCH484_RAY_AS_GPU_IDENTITY.md)：AS gpuResourceID精确native查询、ResourceId关联/去重和查询依赖已补，backend/bundle cde95930，GUI3ba30e36。六例native/capture/738事件/API/CLI、36坏输入+6合法副本、18旧检查和7×10生命周期PASS（growth409600bytes）；官方两scene/10尺寸查询/41坏输入/6能力查询PASS。真实UE越过getter，随后compute VFT容量>32被拒绝并在管线断言，B485继续。本库全量未跑，32333bb6上波308/7784/3080已PASS、growth0/hash一致；ARC和用户原crash未验，RT dispatch/离线未证明，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B483](BATCH483_RAY_UNBOUND_SIZE_AND_UI_LIFETIME.md)：无输入/indirect TLAS尺寸及heap查询已与原生10组一致；backend/bundle32333bb6、GUI3ba30e36。官方两scene新截/离线像素/44与46事件×3方向/41坏输入及6能力查询通过。Qt header旧模型回调问题修复前FAIL、修复后PASS，当前独立CPU模型/销毁/滚动测试PASS；用户提供的原崩溃未复现，完整操作场景待回复。真实UE越过无输入查询，下一阻塞AS gpuResourceID；RT dispatch/离线未验，ARC未修，能力false。固定32333bb6全量308/7784/3080 PASS、growth0bytes、起止哈希一致（frozen-validation-32333bb6），持续任务继续，未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B482](BATCH482_RAY_MULTIPLE_INDEXED_GEOMETRIES.md)：已补2–64段共享VB/IB indexed geometry捕获/重放、schema5帧前输入冻结、Shared TLAS子快照materialise及compact/copy。最终backend/bundle cad38602一致；十例native/capture/1,176事件选择/CLI、300坏输入、18旧例及11×10生命周期通过（growth688128bytes），官方两scene新截/离线像素/事件/41坏输入及6能力查询通过。真实UE已越过两geometry构建，当前失败于无instance buffer/children的TLAS尺寸查询，B483继续补。UI崩溃线索为尺寸测量/疑似栈耗尽，header旧模型回调生命周期修复验证中；原报告未复现。ARC、UE dispatch/离线、本库全量及GUI未验；公开能力false，持续任务继续，未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B481](BATCH481_RAY_HEAP_AS.md)：heap AS 自动/placement、帧前/帧内创建与 heap/offset 查询已补齐。backend/bundle 9122ac7a一致；六例native/capture/API/CLI通过、846事件选择、78坏捕获、18旧例和7×10生命周期通过（growth278528bytes）。官方两scene重新截帧/像素/事件/CLI、41坏输入及6能力查询通过。真实UE首个阻塞已定位并修复为未包装heap AS；当前进入usage0/motion1/两段Private indexed geometry，B482继续。ARC、UE RT dispatch/离线/GUI/本库全量未通过或未运行；生产能力仍false，目标active，未提交/推送。

2026-10-05 当前 [PHASE57](PHASE57.md) / [B480](BATCH480_RAY_UE_DIAGNOSTIC_AND_TABLE_IDS.md)：backend/bundle e0e4df24一致。compute VFT/IFT 单表/数组身份拒绝和合法 nil 恢复、6项能力查询、官方两种光追 sample 新截/像素/44与46事件三方向/41坏输入通过；basics定向2,070命名事件、20×10生命周期通过（growth540672bytes）。默认两项能力false，进程诊断开关只按原生值放行compute。UE已初始化并真实进入AS构建，primitive descriptor兼容检查拒绝；B481正定位usage/geometryCount。ARC、GUI和UE RT回放未通过；本库未重跑全量。B479固定a35028ba全量308/7784/3080已通过。目标active，未提交/推送。

2026-10-05 当前 [PHASE57](PHASE57.md) / [B479](BATCH479_RAY_BOX_INITIALS_AND_TABLE_ORDER.md)：官方 triangle/procedural 两场景 native→capture→offline 像素逐字节一致，44/46 事件各三方向和 CLI 各3 loops通过。补齐 boxes 初态、compact 依赖、先函数表后 pipeline 的合法顺序；backend/bundle a35028ba 一致。22 旧 RT 例 + pointer、2,574 命名事件选择、27 T60 +41 官方坏输入通过。固定库全量308/7784/3080通过、growth3784704bytes、起止哈希一致（两轮旧负例失败记录保留）；新 RT 生命周期14×10通过、growth0bytes。B480诊断开关/compute表ID构建验证中，ARC 提前释放、GUI/真实 UE RT 待验，两项能力仍 false，持续目标 active，未提交/推送。

2026-10-05 当前 [PHASE57](PHASE57.md) / [B478](BATCH478_RAY_ARGUMENTS_AND_DISPATCH.md)：官方 triangle 新捕获和离线像素 oracle、44 事件三轮前后定位通过；只读 pointer 参数描述、Managed packet 和 AS residency 已补齐，backend/bundle 5a5d1683 一致。22 旧 RT 例、独立 pointer fixture、T60/27 坏输入通过，总 2,436 事件选择。procedural 捕获输出仍与原生一致，离线卡在 boxes/TLAS 初态；B479 开发中。全量/GUI/UE RT/生命周期未验，两项能力仍 false，持续目标 active，未提交/推送。

2026-10-05 当前 [PHASE57](PHASE57.md) / [B477](BATCH477_RAY_GPU_INPUT_SNAPSHOTS.md)：官方两种光追场景捕获通过，与 native 输出逐字节一致；新增 GPU 输入快照、实例 flags/mask/IFT offset 和 Managed 初态 seek 恢复。backend 582c068b 下 10 新例 + 12 旧例 API/CLI、2,262 事件选择、24 损坏输入及 schema2 兼容通过。离线官方 sample 卡在 pointer argument encoder，B478 正在修复；程序化 boxes 初态、ARC 生命周期、全量/GUI/UE 光追未验。两项能力仍 false，持续目标 active，未提交/推送。

2026-10-05 当前 [PHASE57](PHASE57.md) / [B476](BATCH476_APPLE_RAY_SAMPLE_AND_SIZE_QUERIES.md)：已建立每小时接续的光追持续任务（id metal），规则与启用门槛见 [持续计划](RAYTRACING_ENABLEMENT.md)。接入固定版本 Apple 官方 sample：两种 scene 原生各两次、输出逐字节可重复；补齐 Managed/Private TLAS 尺寸与 heap 查询，5 组/两接口与原生一致。最终 backend/bundle 309d1058 一致；3 个 Shared TLAS 重截/API/CLI（306 事件选择）及12旧例通过。官方 capture-probe 仍失败于 dispatch 缺少有效 TLAS 绑定，离线 sample/UE 光追未验；设备/子资源 ARC 释放顺序断言待修。两项能力仍 false，下一步按 VK/DX12 执行点输入复制方式补 Managed/GPU 输入及真实构建链，保留未提交修改。

2026-10-05 当前 PHASE56：B474–475补齐refittable/refit/普通copy/compact AS初态、nil原地refit、每轮逻辑状态重置、帧内compact尺寸查询及提交前源版本校验。最终backend/bundle b5451e79一致：47新+171旧正例、5,199事件选择、281损坏输入、4语义边界、490生命周期通过；schema1/2/3兼容。受控Shared三角形AS基本生命周期链可用；Private/同CB GPU输入等未闭环，两项光追能力仍false。未跑全量/GUI/真实UE光追，未提交/推送。见 [PHASE56](PHASE56.md)、[B474](BATCH474_RAY_REFIT_AND_COPY_INITIALS.md)、[B475](BATCH475_RAY_COMPACT_INITIALS.md)。

2026-10-05 当前 PHASE55：B471–473 补齐帧前非索引/索引 BLAS、TLAS 初态；提交时冻结输入，按依赖重建并在 seek 重置恢复。最终backend/bundle 8594954e 一致：16新+30旧正例、1,785次事件选择、177损坏输入及3项语义边界、180生命周期（增长360,448 bytes）通过。最终定向结果见 [B473](BATCH473_RAY_INDEXED_INITIALS.md)，范围和接续优先级见 [PHASE55](PHASE55.md)。设备光追能力仍 false；未跑全量/GUI/真实 UE 光追。保留工作区修改，未提交/推送。

2026-10-05 B467：Metal 3 Temporal MetalFX opaque 捕获/回放、同 scaler 跨 command buffer 历史、公开输入/输出/参数与 Texture Viewer 跟随及按角色区分的 Usage；VRR 在 RS 显示 map、逻辑/各层物理尺寸及速率，StateObject 在 draw/mesh draw 记录 Rasterization Rate Map Usage，按 DX12 的阶段位置放置。修复 descriptor-name 异步回调访问旧缩略图的切捕获崩溃。六份新帧、normal/auto/minimal 两输出 native 字节一致、缺失历史警告/捕获 reset 恢复、45 malformed、15旧帧+7 B465帧及真实GUI三轮切换/六资源跟随通过；未跑全量。此前私有历史不能导出，首捕获无 reset 时明确告知近似回放，不能宣称任意中途截帧精确。后端/bundle a371c140；当前测试窗口 Temporal EID17输出。详见 [B467](BATCH467_TEMPORAL_METALFX_VRR.md)。保留此前修改，未提交/推送。

2026-10-05 B469–B470：按用户Vulkan/DX12对照准则补齐AS marker三接口、AS fence两接口、IFT单槽/range清空恢复，修复visible/IFT未知非空handle变nil漏洞；使用公共事件树及既有fence epoch模型。最终backend/bundle ae2a2895一致：两种native/capture GPU ray0/1/0/1、54/65事件各3方向（357选择）、112异常拒绝、30旧帧API/CLI、两新帧各3CLI循环、40生命周期（growth720896B）通过。帧前build变体native/capture正确但离线明确拒绝AS绑定，证实AS初态恢复仍是通用光追阻塞；两项raytracing查询仍false。未跑全量/GUI/真实UE光追验收；未提交/推送。见[PHASE54](PHASE54.md)、[B469](BATCH469_RAY_MARKERS_AND_TABLE_CLEAR.md)、[B470](BATCH470_RAY_AS_FENCES_AND_BASELINE.md)。

2026-10-05 B468：修复普通 Shared buffer 在同一提交内于所选 draw 后创建导致的部分回放完成失败；保留 validated birth/bounds，仅延后未来独立 allocation 的合法快照，heap/descriptor alias 规则不放宽。原失败帧、新普通25 API事件三方向／字节／像素／Usage、两种VRR后创建buffer、8非法数据、B467六旧帧、完整定向event-navigation和实际GUI跳转通过；未跑全量。backend/bundle同为4bda5de2，GUI留在普通帧EID13输出。见 [B468](BATCH468_FUTURE_SHARED_PARTIAL_REPLAY.md)，未提交／推送。

2026-10-05 B465 UI整改：补齐 MetalFX 的 Texture Viewer Inputs/Outputs、真实资源跟随和明确 MetalFX - Input/Output Usage；Fetch Usage 更正为 Vulkan 同样的 FB Input（保留 FB Color），Memoryless 的 Clear/Discard 验证通过。特性移至 FS/Tile 末尾，默认仅实际声明显示，Show Unused 可查看其他元数据。新增 combined 捕获证明 FS fetch/ROG 与 OM 固定加法混合可同时启用，保留实际 BS。当前后端/bundle 同为f0280e36；七份新帧像素/descriptor/Usage、20份旧帧定向、真实GUI缩略图/跟随/条件显隐检查通过；未重跑全量，右键菜单鼠标操作及布局满意度待用户验收。当前 GUI 为 MetalFX EID6输出。见 [B465](BATCH465_TILE_FETCH_MEMORYLESS_ROG_METALFX.md)，未提交/推送。

2026-10-05 B465：接入 Tile 独立管线页面、Framebuffer Fetch 的 FS Input Attachment / OM 同资源输出、Programmable Blending / ROG shader 元数据、Memoryless/load/store 附件参数，以及公开 Metal 3 MetalFX Spatial opaque 捕获/回放和参数页。六份新捕获与像素/状态检查通过，MetalFX 输出与 native 逐字节一致；20 份旧帧定向回放和 44 个异常输入拒绝通过，未重跑全量。最终 GUI 27 次快速切换通过，实际 Tile/FS/OM/MetalFX 参数与 MetalFX 输出纹理跳转已检查，修复了缩略图异步回调生命周期崩溃。后端/bundle 同为48afd5d3，当前窗口 Tile EID13。不含 Metal 单步调试、全部 Tile 格式或 Temporal/Metal4 MetalFX；布局由用户验收。捕获与操作见 [B465](BATCH465_TILE_FETCH_MEMORYLESS_ROG_METALFX.md)。保留其他任务改动，未提交/推送。

2026-10-05 B466：本 fork 的 shader tools 已内置 app 并自动注册，View 提供 AIR/MSL/HLSL/GLSL，Edit/Compiler 接入 Apple AIR/MSL 编译；release 打包检查固定源码、hash、架构与许可证，无用户路径配置/运行时 Homebrew 或 Rust 依赖，仅 Apple 工具依赖 Xcode。高层重建仅预览，Edit 严格限无资源/无特化的简单 FS；当前 UE3928 fptoui、3612 fptosi 尚不能高层反编译，AIR 可用。定向工具/搬迁/超时/打包 PASS；实际 UI AIR Apply/Remove 与三种源码 View PASS（公开 Qt 选择，非鼠标下拉验收）；同一 UE 两 shader 在06ae及随后匹配当前 API 的固定48afd5d3上均两轮写资源字节一致/恢复/fatal0。新头旧库混用追加 probe 失败不计 PASS；另一任务已更新 backend，本轮未重跑全量，06ae全量仅属于B464。详情见 [B466](BATCH466_BUNDLED_SHADER_TOOLS.md)，未提交/推送。

2026-10-04 最新 B464：接入公共 Shader Edit/Apply/Remove 与 Shader Processors，补齐 MSL、嵌入 debug MSL、可重编译 AIR/MetalLib，保留 function constants、specializedName/原入口及所有依赖 native PSO 的原 descriptor。AIR 编译保持捕获 Metal/AIR 版本和部署目标，映射发布前等待 GPU，typed release 释放临时 function/PSO。View 可选择内置 AIR、Captured MSL 和匹配的外部 processors。最终06ae2318库与bundle一致：定向 source/AIR/debug/alias/frame-born PASS；首次候选 Tess/Task/Mesh 替换通过；最终固定全量308/7786/3080 PASS、growth1343488B/hash一致/exit0；全量后同一UE3928 FS/3612 CS两轮编辑/恢复、3612/3928/4036 Usage及整帧两轮reset通过，写目标/GBufferA/呈现图保持基线。实际UI通过微型源码Edit/红色Apply/错误展示/Remove恢复、外部View目标切换，同一UE3928 external AIR与3612内置AIR的Edit/Apply/Remove；停在3928 FS原shader。直接键盘源码输入自动化未验收，Save面板未重试；readonly VisBuffer64在无编辑control reset也变化，原因未定位。AIR→MSL候选metal2vulkan→SPIRV-Cross尚未构建或验证；linked function tables编辑明确拒绝。详见 [B464](BATCH464_SHADER_EDIT_AND_PROCESSORS.md)，操作见 [Shader Tools](../../util/shader_tools/README.md)。保留旧改动，未提交/推送。

2026-10-04 最新 B463：Metal Pipeline State 使用公共 flow chart/资源树/shader与CB viewer，对齐传统/Tessellator→TES/TS→MS路径、独立CS、灰/黑/红边框；shader头部与Resources→UAVs→Samplers→Constant Buffers顺序，IA布局/真实vertex流分类、Rasterizer矩阵及并列viewport/scissor、OM附件/混合/深度/模板布局补齐。新增inspection动态状态与细分数据，不改GPU setter/提交/等待或capture chunk。当前885fa783库与bundle一致：定向PASS；一次固定全量308/7786/3080 PASS、growth5996544B/hash一致/exit0；全量后同一UE3612/3928/4036及整帧两轮reset通过，GBufferA/呈现图保持基线。助手实际UI确认IA/RS/OM/FS/CS及native Tess/Task/Mesh页面、公共View/CB/资源跳转；Edit因backend尚无replacement明确禁用。Save原生面板自动化timeout（采样AppKit getxattr），未计UI PASS；UE flow坐标点击工具noWindowsAvailable也未计PASS，公开stage导航确认实际页面。重启后同一UE正常打开，留在3928 FS。详见 [B463](BATCH463_PIPELINE_UI_PARITY.md)。未提交/推送。

2026-10-04 最新 B462：修复 GBufferA 4036 假 CS_RW 和遗漏读取。移除 residency/lifetime 资源闭包的 Usage 误报；uniform bindless 按各 command buffer 的 submission CPU snapshot 解析，再统一 bake EID，GPU 提交顺序不变。原 UE 打开即有九个 Usage 事件：3415 Clear、3509 FB Color、3596/3612 CS_RW、3739/3824/3928/4096/4482 FS Resource；七个 shader 事件逐项跳转资源一致、4036 排除且访问前后列表稳定。补齐 compute/render 显式 resource Barrier Usage；不为 scope barrier 虚构资源。4036 的64³光照体积全体为常量SH系数，filter输入/输出与两轮reset一致，UI dispatch深度名称已修正。Windows差异有实际session日志证明：本次低负载capture关闭Lumen GI/Reflections、VSM、ShadowQuality等，不能作为全功能帧平齐验收。候选库与bundle a881aab3：late-uniform/CS/Task/Mesh/Barrier定向通过；固定全量308/7786/3080通过、growth0B/hash一致/exit0；全量后同一UE九Usage、整帧三份呈现原基线均通过。实际UI通过4036体积/3928法线、ResourceInspector八分组九事件与3739跳转；缩略图右键及外点击关闭因CUA窗口定位错误未验收。详见 [B462](BATCH462_SUBMISSION_BINDLESS_USAGE.md)。未提交/推送。

以下为历史记录，B460–B461 的4036 Usage及3928需选中才补入的解释已由B462纠正。

2026-10-04 最新 B460–B461：补齐 uniform bindless 的 CS、VS、FS、Object/Task、Mesh 阶段与 texture write/atomic，按真实 SRV/UAV 分类；公共 PipeState / Pipeline State 新增 Task/Mesh shader、资源和 sampler，以及 VS/FS/CS AS 输入。原 UE GBufferA 现在显示3415 Clear→3509 FB Color→3596/3612 CS_RW→3928 FS Resource（GPU 依赖索引在选中3928后解析），解释 Basepass 与光照数据不同的真实 Nanite producer。当前库与 bundle 库6362ccc0：原生 CS/Task/Mesh、8异常输入、已有 mesh/ray AS 定向通过；固定库全量308/7786/3080通过，growth2392064B/hash一致/exit0。全量后同一原 UE四EID各两次重置及整帧正常打开/两次reset通过，GBufferA各EID和三份呈现图保持原基线，capture hash不变。实际UI通过3612四个RW资源/Outputs/场景法线及3928六命名Inputs；Task/Mesh小捕获正常打开和管线识别通过，但自定义阶段按钮及右键菜单鼠标操作受CUA noWindowsAvailable影响未验收，不能记为UI通过。仍是可证明uniform地址解析，不代表逐invocation动态feedback或linked intersection/callable内部访问已平齐。详见 [B460–B461](BATCH460_461_BINDLESS_SHADER_STAGES.md)。未提交/推送。

2026-10-04 最新 B458–B459：参照 DX12/Vulkan 将 Metal 公共 EID 按提交映射，同一 UE GBufferA 已为3415 Clear→3509 FB Color→3928 FS Resource；修正 ICB API event去重、submit扁平scope、encoder结束后的signal/wait归属和AS Encoder归属。DontCare新增 MSAA逐样本、untracked standalone/heap、memoryless Private backing、3D/cube/rate map；修正部分回放 StoreDontCare丢失所选draw，以及resolve-only源Discard usage。当前库671c90b4/GUIc3a0e3b6（内嵌库同步）：18单样本+15MSAA定向、额外深度/模板独立Store和204周期通过（Store stress growth3325952B）；固定全量308/7786/3080通过（growth0B/hash一致/exit0）。全量后同一UE3928/3509、六light overlays/None恢复、Clear Before、正常打开和两次整帧重置均通过；SceneColor/GBuffer/深度与三份呈现图保持基线，原capture hash不变。实际UI通过3928/3509/54 Begin Blit跳转和场景法线显示，留在3928 GBufferA/None；右键自动操作未唤出菜单，菜单与外点击关闭未验收。本机2x/4x可验证，8x/D24S8不支持，不能记PASS；MSAA直接Texture Viewer读回为既有独立缺口，当前逐样本验证使用真实GPU consumer。参见 [B458](BATCH458_SUBMISSION_EVENTS.md)、[B459](BATCH459_EXTENDED_DISCARD.md)。未提交/推送。

2026-10-03 最新 B457：新 UE GBufferA 的3300 FS/3808 Clear/4057 FB Color是CPU编码EID：producer14143于6790提交，consumer14146于6791提交（同queue11），实际先写后读；Metal尚未使用DX12/Vulkan的提交时baked EID模型，usage菜单已注明编码顺序。新增公共 LOAD/STORE DONT CARE图案（单样本tracked颜色/深度/模板、并行store、mip/slice；Fastest跳过），上传staging创建引用已平衡，隔离回放增长从58.9MB降至0.74MB。最终库 `1437b5cb`、GUI `459b09eb`：定向及untracked边界通过；固定库全量308/7786/3080通过（growth999424B/hash一致/exit0）；全量后同一UE3300/4057、六light overlays、Clear Before及None恢复通过，正常打开+两次整帧呈现与GBuffer/深度均保持基线。解锁后实际UI正常打开同一UE并选择3300，GBufferA显示场景法线，停在RGB/None；右键自动操作未唤出菜单，标题及外点击关闭仍未验收，不能记为UI通过。EID架构仍未平齐DX12/Vulkan。详见 [B457](BATCH457_USAGE_ORDER_AND_DISCARD.md)。未提交/推送。

2026-10-03 最新 B455–B456：参照 Vulkan 补齐 Quad Overdraw 与 Triangle Size Draw/Pass，当前库 `8e332844`、GUI `48b2791c`。两项各十一种 Native 定向、同一 UE3300 两周期及 GBuffer Range/Histogram 复核通过；原资源字节不变。固定库合并验收全量308/7786/3080通过（growth5816320B/hash一致/exit0），全量后真实 UE全部覆盖层/统计复核及正常打开+两次整帧重置通过，GBuffer/深度及三份呈现图保持基线；bb793d8d 全量保留为历史。实际 UI 已正常打开并显示3300原图，但自动下拉框输入在独立标准 Qt 窗口同样无法切换，Quad/Triangle 渲染 UI 待确认，最新窗口停在3300 SceneColor/None。详见 [B455](BATCH455_VULKAN_QUAD_OVERDRAW.md)、[B456](BATCH456_VULKAN_TRIANGLE_SIZE.md)。持续推进，未提交/推送。

2026-10-03 最新 B454：参照 Vulkan/D3D12 完成原片元深度导出的 stencil-mask Depth Test，并修复 Depth/Clear Before 源 encoder context。库 `bb793d8d`：七种深度导出与八种 Clear Before Native 定向、同一 UE3112/3300两周期、固定库全量308/7786/3080（growth9502720B/hash一致/exit0）及全量后真实 UE复核通过。整帧正常打开和两次EID0恢复图都保持fc5f3afe基线。助手实际UI通过3112 Depth→None、3300 Clear Draw→Pass→None；未代替用户人工验收。参见 [B454](BATCH454_ORIGINAL_FRAGMENT_DEPTH_MASK.md)。Quad四桶计数 Native组件通过，正在构建正式回放候选，本库全量不归于下一候选。持续推进，未提交/推送。

2026-10-03 最新 B453：参照 Vulkan 补齐 Clear Before Draw/Pass，当前库 `a086f3e5`。八种 Native 双 MRT/原 shader/混合/discard/深度导出/stencil 定向及已有六覆盖层通过；同一 UE EID3300 清空/恢复两周期与全量后复核通过，完整 UE 三份呈现图保持基线。固定库全量308/7786/3080通过（growth6127616B/hash一致）。当前实际 UI 待手动解锁：CUA 明确报告 Mac 锁定，已请求解锁一次；未把终端结果记作 UI。下一项 original-FS depth stencil-mask 的64x32 Native组件已通过，正式回放集成仍待。详情 [B453](BATCH453_VULKAN_CLEAR_BEFORE_OVERLAYS.md)。目标 active，未提交/推送。

2026-10-03 最新 B452：参照 Vulkan 补齐真实 pass 事件列表及 Viewport/Scissor 覆盖层，当前库 `0776deb7`。七种 Native 定向、交错/parallel/blit pass oracle 和同一 UE EID3300 六覆盖层两周期通过，SceneColor/GBuffer/深度字节不变；本库未跑全量，上一库 ac2a9748 的全量 PASS 单独保留。助手 UI 的 Viewport/Scissor→Depth→None 切换和原图恢复通过，界面停在 EID3300 SceneColor。详情 [B452](BATCH452_VULKAN_PASS_AND_VIEWPORT_OVERLAY.md)。持续目标 active，未提交/推送。

2026-10-03 当前继续 Texture Viewer 对齐：[B449](BATCH449_TEXTURE_VIEW_INSPECTION.md) 的 Range/Histogram/数值显示已通过定向及同一 UE；[B450–451](BATCH450_451_NATIVE_DRAW_OVERLAYS.md) 新增五种真实绘制覆盖层、inline 注解恢复及临时 immutable state lifetime 修复。当前固定库 `ac2a9748` 的同一 UE EID3300 五覆盖层两周期和输入/输出字节检查通过，固定库全量308/7786/3080通过（growth8716288B/hash一致），全量后同一 UE 五覆盖层及 GBuffer Range/Histogram 两周期通过，完整 UE 两次 reset 呈现图像保持基线；实际助手 UI 的五绘制覆盖层、Range/自动范围/Histogram/数值覆盖层/Pixel Context 均通过，界面停在3300 SceneColor；未提交/推送。下方 ff444e68 等记录为历史，不能当作当前库的全量/UI。 持续对齐目标保持 active；五项通过不代表所有 Overlay 已与 Vulkan 平齐，后续优先 pass 归属和其余 N/A 项。

2026-10-03 最终解锁验收：当前 ff444e68 定向、固定库全量及全量后新旧 UE 均通过；实际助手 UI 现已完成（新 UE 六个命名输入、GBuffer 场景、usage Escape/鼠标外点击/跳转4057、公共 Shader Viewer 的 AIR 与 Captured MSL）。界面已重新打开新副本，停在3300 Fragment Shader。当前已报告问题与支持范围内的检查流程通过，用户画面反馈仍待；不代表任意 Metal API 或动态 bindless 全量反馈已经平齐。未提交/推送。详见 [B448](BATCH448_SUBMISSION_DESCRIPTOR_SCOPE.md)。

2026-10-03 [B446–B447](BATCH446_447_FRAME_FLOAT_AND_VISIBILITY.md)、[B448](BATCH448_SUBMISSION_DESCRIPTOR_SCOPE.md)：ff444e68定向、固定库全量308/7786/3080（growth5488640B/hash一致）及全量后新旧UE通过。新截帧光照3300的GBufferA/C与Nanite后4349、旧基线逐字节一致；三份呈现图像及原始JPEG精确一致，真实GBufferA名称与三类usage正确。旧3371/3551/188及整帧两次重置保持原图像。最终UI仍待再次解锁（终端完成后CUA重查仍锁定），无GPU进程存活；持续目标blocked（本次解锁恢复后连续三轮复查仍锁定，必须再次手动解锁完成最终UI），未提交/推送。

2026-10-03 [B444–B445](BATCH444_445_RESOURCE_USAGE_AND_UI.md)：补齐真实资源label、graphics附件usage和texture view parent usage；同一UE GBufferA已有3147 Clear/3310 FB Color/3371 FS Resource，GBuffer字节不变。开始对齐DX12/Vulkan shader紧凑头部、descriptor跟随标题及pass摘要；当前dafb37a9定向及同一UE两次重置通过，固定库全量308/7786/3080通过（growth9453568B），全量后同一UE3371/3551/188及整帧两次重置通过，GBuffer A/C与呈现图像SHA保持一致；UI已有部分通过，菜单外点击及最终界面待Mac解锁后验证。目标blocked（连续三轮确认Mac锁定，等待用户手动解锁），未提交/推送。

2026-10-03 [B443](BATCH443_UE_SHADER_INSPECTION.md)：同一 UE EID3371 已解析6个uniform静态bindless纹理输入（含GBufferA/B/C、深度），逐项核对表项来源与Native handle；提供捕获metallib的Apple AIR反汇编及原始二进制。当前70773cef定向及同一UE两次重置通过，固定库全量308/7786/3080通过（growth5783552B），全量后同一UE图像/输入复核通过；助手UI通过（六输入、GBuffer独立选择、Function4712实际AIR），未代替用户验收。不代表任意动态索引反馈或完整跨API功能平齐。未提交/推送。

2026-10-02 [B440–B442](BATCH440_442_UE_EVENT_NAVIGATION.md)：按用户 DX12/Vulkan 对照要求保留正常跨 pass scope；修正 marker 归属/树倒序、EID188 新鲜 table 范围误拒绝、内联参数 UI 漏显。实测发现并修复 EID3371 部分回放截断先提交的 Nanite producer：同一 UE GBufferC 非零像素从2361恢复35672，SHA与 post-Nanite 一致，两次 EID0 重置一致。当前 c7bb6da1 定向通过，固定库全量308/7786/3080通过，随后同一 UE 整帧及关键中间事件再次通过；最终助手 UI 验证通过（188/3182/3371/3551），界面停在3371 SceneColor，未代替用户验收；既有 stream reader/InitialContentsList 日志诊断保留，详见报告。此前95214a2已推送，本轮改动未提交/推送。

2026-10-02 [B438](BATCH438_INTERLEAVED_GPU_PRODUCER_OWNERSHIP.md)/[B439](BATCH439_RETIRED_PRIVATE_TEXTURE_BACKING.md)：当前 b814fdbb 已通过同一 UE faa8540e 的严格审计、正常 OpenCapture、整帧 EID9924 与反复 EID0 重置。三份原生900×640图像逐字节一致，重编码JPEG与原捕获缩略图逐字节一致；EID2017实际indirect为8,1,1，BasePass五MRT及两组64³体积MRT重复读回稳定。修复各自定向后立即返回同一UE，再进行一次当前库验收全量308/7786/3080，通过后再次返回同一UE通过。当前库 qrenderdoc 正常加载，实际第五MRT、体积Slice63及完整编辑器/天空/黄色平台观察通过，正常退出；这是助手UI操作记录，未声称用户人工验收或独立原始无损图像golden。校验归档763历史目标至外置CauseUseMac（12.22GiB逻辑数据），34原始项目截帧全部保留，清理后构建约3.2GiB，后续验收产物生成后约3.7GiB、可用约13GiB。无提交推送。当前使用入口及已验收副本见 [运行说明](../../util/ue/README.md)。

2026-10-02 [B408](BATCH408_INITIAL_CPU_GPU_DESCRIPTOR_VALUES.md)候选61最终51c5a0a7定向6/24/108+21通过：完整CPU初始值证明、late/generation/source拒绝；commit消费闭包只在draw/dispatch验证，真实UE无GPU预检继续推进。已实现metadata-only/pre-submit两个强制退出入口，不修改实际RDC coverage；目标active，未提交推送。

2026-10-02 [B426](BATCH426_DRAWABLE_CLEAR_BEFORE_SHADER_READ.md) 固定d6c890d4全量308/7786/3080通过、growth0B/hash一致；[B427](BATCH427_PER_GENERATION_SLOT_CONSUMERS.md) 6/24/210通过；[B428](BATCH428_ACQUIRED_DRAWABLE_INITIAL_PIXELS.md) 当前01db4c65、2/8/16通过，非零seedRGB10A2 acquired snapshot/NativeLoad/初始恢复验证。旧UE eb9386b7 缺currentLoad drawable初始像素不可伪造，已保留，正在自动重截；UI已可访问，整帧GPU/UI未验收，持续目标active，无提交推送。


2026-10-02 [B422](BATCH422_LAYERED_FRAME_VOLUME_TARGETS.md) / [B423](BATCH423_LOGICAL_GPU_TABLE_RETIREMENT.md) / [B424](BATCH424_SOURCED_LINEAR_TEXTURE_UPLOADS.md)：3D layered MRT 2/8/20、logical GPU retirement 6/24/180（含later resolve Native bytes）、Shared staging→R8/BGRA8_sRGB 4/16/38 通过；实际UE已继续到帧后段，最新 cdf6b4fc pre-submit 进行中，整帧GPU/UI未验收。最近全量仍6e963f9a 308/7786/3080，最新库全量待跑。持续目标active，无提交推送。


2026-10-02 [B406](BATCH406_REPLAY_ALLOCATION_BUDGET.md)6/24/110+15通过，真实UE candidate60 metadata/budget CPU-only通过；[B407](BATCH407_LARGE_BUFFER_TEXTURE.md)402984b1大TextureBuffer3cases/12reset-seeks/16API+CLI negatives通过。真实UE pre-submit已加载全部背景资源，停在frame DescriptorSlotEvent，未GPU上传/整帧回放；继续epoch/source定位，目标active。

2026-10-02 B405最终2bcf9403精确全量308/7786/3080通过，growth9306112B且hash不变。预算候选coverage60/CPU-only元数据入口对象编译中，原真实UE文件不改coverage，不运行整帧GPU；目标active。

2026-10-02 [B405](BATCH405_ATTACHMENTLESS_UAV_PLAN.md)v59最终2bcf9403定向6/42/252通过；B404、B403、B402、B401交叉通过，原生零附件UAV输出和fragment保留正确。精确组合全量进行中；实际UE34heap/initial CPU预算进一步量测，未授权整帧GPU/UI，持续目标active，无提交推送。

# Metal Replay 当前状态

2026-10-02 [B404](BATCH404_SOURCED_RENDER_INDIRECT_PLAN.md)v58 4c177a50定向22captures/88cycles/404API+CLI反例通过；zero/indexed/parallel/future MRT/D32S8和Native原参数、每像素正常。最近精确全量B403 a0f8b74a 308/7786/3080；新候选继续零附件UAV，不替UE整体coverage授权。目标active，UI未验收，无提交推送。

2026-10-02 B403最终a0f8b74a精确全量308/7786/3080通过，growth6750208B，端点hash一致；render capture/loading定向8/56/248通过。B404 coverage58有界sourced render indirect已对象编译，开始22captures/88cycles微型GPU，不授权实际UE整帧。目标active，人工UI未验收，无提交推送。

2026-10-02 [B403](BATCH403_RENDER_INDIRECT_PASS_END_PLAN.md)已完成render indirect逐使用capture/loading候选，Native pass-end机制复用Vulkan；八组合capture/replay与56 reset seeks/248异常组通过（b6356359），indexOffset保持原byte binding语义后重验。真实UE已自动重截eb9386b7：17render证据全部匹配、29280总work、11zero；31compute全部匹配、39616threads。隔离UE模块9d6349a6，无安装引擎改动；完整UE/UI仍未验收，持续目标active，无提交推送。

2026-10-02 v57精确26a25b7f…组合全量308 captures/7786 malformed/3080 lifecycle通过，resident growth7520256B且库hash一致。B400同库跨8/56/176也通过；B401、B402定向见下。B403 Native pass-end blit组件build-only完成，准备串行8组合GPU；已确认UE所有UAV写声明及TextureBuffer parent路径，当前capture还未接通。实际UE/UI未验收，目标active，无提交推送。

2026-10-02 [B400](BATCH400_CAPTURE_INDIRECT_ARGUMENTS_PLAN.md)7930a5b3定向8/56/176通过，真实重截1deff873的31次compute indirect证据全匹配（22零调用、39616总threads、最大22400）。[B401](BATCH401_SOURCED_DEPTH_ONLY.md)v56 e8f991a2定向14/56及depth-only seeks/80负例通过。[B402](BATCH402_SOURCED_COMPUTE_INDIRECT.md)v57 26a25b7f定向12captures/48cycles/150负例通过，包含Native1/2/0、Shared/Private参数及DS/MRT。当前库跨B400兼容验证后运行组合全量；实际UE还在CPU元数据拒绝，人工UI未验收，目标active，无提交推送。

2026-10-02 B399精确f12b4ea6…全量308/7786/3080通过，growth1441792B且hash一致，定向18/72/354通过。正式B400 capture indirect候选已对象编译，准备8captures/56seeks/176反例；render indirect17次Native参数范围与全部显式write footprint（含TextureBuffer parent）无重叠，仍需shader/声明完整性检查。实际UE/UI未验收，目标active。

2026-10-02 [B399](BATCH399_DEFERRED_SOURCED_STORE_ACTIONS.md)：v55精确f12b4ea6…18captures/72seeks/354反例通过，serial/parallel/5MRT/DS/future heap/counter所有partial/full pixels正确。全量准备中；B400独立Native捕获参数组件4组合通过1/3/2及unretained生命周期，待正式接入。整帧UE/UI未验收，目标active。

2026-10-02 B398精确8b15aaec…全量308/7786/3080通过，growth4931584B且结束hash一致；v55 deferred stores对象及夹具编译完成，接续18captures/72cycles微型定向。实际UE138附件全部final Store、Unknown94全部setter闭合，10个真实graphics PSO Native编译通过，无GPU提交。整帧/UI未验收，目标active。

2026-10-02 [B398](BATCH398_SOURCED_COUNTER_PASS_ATTACHMENTS.md)：v54精确8b15aaec…12 captures/48 seeks/150反例通过，共用Native counter验证，串行/并行和depth/stencil/heap/five MRT全部像素正确。全量运行中；继续v55延迟StoreAction，实际UE/UI未验收，目标active。

2026-10-02 [B395](BATCH395_PARTIAL_COPY_SUBMISSION_PREFIX.md)：v52精确5b329da9…按原提交順序、chunk/ResourceId重放缺失纯buffer-blit prefix及future birth。Same/split/late/later-staging/11-copy组合16 captures/64 cycles/48 first-draw seeks/578反例通过，帧末index全零后仍恢复正确Native索引与像素。当前UE135次frame-index的CPU字节/顺序闭合。v52全量准备中，整帧UE/UI未验收，目标active。

2026-10-02 v50精确008ca84b…全量308 captures/7786 malformed/3080 lifecycle通过，growth12681216B，结束hash一致；包含v47大copy/v48Private index/v49draw/v50submission snapshot。v51提交順序候选开始链接/微型late-upload验证，实际UE整帧及人工UI未验收，目标active。

2026-10-02 [B394](BATCH394_SUBMISSION_INDEX_ORDER_PENDING.md)：当前UE195直接indexed draws的CPU字节与提交顺序审计通过，135次frame index/9 future buffers；4次consumer先编码、producer后编码但先提交，v51候选改按commit模拟，尚未构建。v50精确全量继续运行、不替换Native库。间接48调用在19 encoders，存在同encoder写参数，需要逐使用点GPU快照。完整UE/UI尚未验收，目标active。

2026-10-02 [B393](BATCH393_SUBMISSION_INDEX_UPLOADS_PLAN.md)：精确008ca84b… v50 CPU快照按commit归属，Same/split submission的frame Private indices4 captures/16 seeks/146反例通过；已知复制范围与GPU写入检查接通，全量308/7786/3080运行中。继续当前UE实际索引和48 indirect分析，整帧/UI未验收，目标active。

2026-10-02 [B392](BATCH392_BOUNDED_DIRECT_DRAWS_PLAN.md)：精确43a58922… v49 Triangle/Strip、乱序索引、负baseVertex、双实例/baseInstance及256/512 draws两轮各4 captures/16 seeks/130反例通过。继续submission所属CPU快照→staging blit→帧内Private index证明；实际UE整帧和人工UI尚未验收，持续目标active。

2026-10-02 [B391](BATCH391_INITIALIZED_PRIVATE_INDEX_PLAN.md)：v48 精确257c4303…初始化Private UInt16/UInt32索引4 captures/16 seek cycles/116 API+CLI反例通过。v47普通copy256条端点也已通过；继续有界直接绘制参数、TriangleStrip和帧内索引上传。精确全量最近v46，完整UE/UI尚未验收，持续目标active。

2026-10-02 [B388](BATCH388_LARGE_ORDINARY_BUFFER_COPIES_PLAN.md) / [B389](BATCH389_UE_SMALL_PLACEMENT_HEAP_PLAN.md)：c5caf390…普通大blit6 captures/24 seek cycles/83反例通过，256条端点复核中。UE heap64重截12230757…完成并保留，913有效source/1937初始slots/62producer全部匹配，当前frame问题0，Native1748 placements范围问题0；38heaps总2.45GiB、127历史buffer overlaps仍需时序证明。466b749c v46精确全量308/7786/3080通过、growth8880128B；当前v47全量待后续组合。完整UE/UI仍未验收，持续目标active。

2026-10-02 [B387](BATCH387_DIRECT_DISPATCH_WORK_PLAN.md)：466b749c… v46直接dispatch 64³/512²/128×1和128次compute八捕获/32 seek cycles/158 API+CLI反例通过，公共Native线程组snapshot复用；精确全量运行中。隔离heap64模块0d648a90…已构建，原引擎hash不变，待串行重截验证。实际UE整帧/UI未验收，目标active。

2026-10-02 [B385](BATCH385_FRAME_NUMERIC_TEXTURES_PLAN.md)：4d69dbb0… v44五类frame来源10 captures/40seeks/160反例通过，真实texture atomic操作及完整像素/UInt PickPixel通过；继续UE真实shader批量52/256项typed更新。整帧/UI尚未验收，目标active。

2026-10-02 B384 精确 cf1bc8af… 全量308/7786/3080通过，resident growth13123584B；v44整数/单通道/打包frame纹理候选开始定向测试，包含真正的Metal3.1 texture atomic_exchange/fetch_add。UE763817d8整帧尚未GPU replay；人工UI仍锁屏待验。持续目标active。

2026-10-02 [B386](BATCH386_UE_BATCH_DESCRIPTOR_PRODUCERS.md)：6ddd8e91…真实UE shader 52/256项更新四捕获/16 seek cycles、全部slot与末槽vertex/fragment2×2像素通过，39+39反例及257条完整producer的40组重检通过；普通blit预算不变。继续直接dispatch，整帧/UI仍未验收，目标active。

2026-10-02 [B384](BATCH384_FRAME_VOLUME_EXTENTS_PLAN.md)：v43 RGBA16/32Float64³及二维格式8 captures/32 seeks/124反例、完整texel/首末depth PickPixel通过；精确新库全量运行中。继续实际uint/packed颜色frame来源，UE整帧/UI仍未验收，目标active。

2026-10-02 [B383](BATCH383_LARGE_TRACKED_PLACEMENT_ALIASES.md)：v42较大tracked placement共享16 captures/64 seeks/208 API+CLI negative groups通过，复用异步夹具，Native逻辑A/B、GPU/像素/EID0正确，旧21类674兼容通过。实际UE整帧未提交/UI锁屏，持续目标active。

2026-10-02 [B382](BATCH382_LARGE_TYPED_TABLE_PLAN.md)：4322af8c…18MiB主表+98304B sampler表2 captures/16dispatch seek+8reset/21 negatives通过；UE精确三槽注解修复并重截763817d8…，unknown非零0、当前frame slot/inline/68producer问题0，842有效初始source闭合。精确4322af8全量308/7786/3080通过，resident growth11632640B。

2026-10-02 [B381](BATCH381_LARGE_FRAME_BUFFER_PLAN.md)：最终150e4d32…placement128KiB/standalone Shared4MiB定向6 captures/24 seeks/67 negatives通过，Shared全数据、Private局部upload/overwrite/归零正确；958d133d…早期组合全量308/7786/3080/growth12632064B通过，150e4后CPU范围/retirement-prefix修复精确全量由后续B382包含验证。

2026-10-02 [B380](BATCH380_FRAME_ARRAY_VOLUME_UAV_PLAN.md)：v39帧内array/3D typed读写7b4e0848…六捕获/24seek/完整30texel及87 API+CLI反例通过；接通CPU已验证inline-only dispatch和source usage闭包，额外12seek/CS_RW验证通过。旧21类674兼容已通过；后续组合全量958d133d…308/7786/3080通过，growth12632064B，未扩间接dispatch/完整UE覆盖，目标active。

2026-10-02 [B379](BATCH379_FRAME_COLOR_SOURCE_PLAN.md)：v38帧内192×104 RG11/R16来源74f7ef23…直接/view八捕获32回跳/完整19968px与GPU值/114 API+CLI反例通过；使用独立heap范围避免未证明的旧纹理重叠，当前精确全量未重复。真实新帧19 texture/259 buffer/31 indirect dispatch继续适配，目标active。


2026-10-02 [B378](BATCH378_MANAGED_DRAWABLE_INITIAL_STATE.md)：Managed纹理GPU初始快照和背景drawable dirty接通，v37背景typed来源要求完整initial；7ef3fa13…两layer极小1×1 Managed背景源两捕获/八回跳/GPU122及恢复/30反例通过。修正用例窗口选择以记录presentation backbuffer；实际UE bfa3f117…重截完成，850有效来源=489纹理initial+361 buffer initial，无缺失；Native1697 placement范围0错误。精确7ef3fa13…全量308/7786/3080通过，growth9256960B；继续192×104帧内RG11B10适配，整帧/UI未验收，目标active。

2026-10-02 [B377](BATCH377_INITIALIZED_TEXTURE_FAMILY_PLAN.md)：v36完整initial的2D/array/cube/cube-array/3D/depth/stencil ID来源及父依赖接通；ada72925…159 typed texture slots、七类GPU309、两捕获/八回跳/12312子资源检查/130反例通过。公共8-bit Depth/S8解码补归一化，四format cases/141027断言通过。定位实际旧drawable Managed initial缺口，继续极小复现及重截；整帧/UI未验收，目标active。

2026-10-02 [B376](BATCH376_INITIALIZED_2D_COLOR_SOURCES.md)：v35完整initial的背景2D颜色来源/逻辑字节预算接通；e3c11635…R16Float192×104直接/parent-view共四捕获/16回跳、完整像素恢复及GPU122、82组反例通过；精确库全量308/7786/3080通过，growth0B。已初始化2D clear按attachment尺寸校验，完整UE/UI未验收，继续资源父依赖与实际格式，目标active。

2026-10-02 [B375](BATCH375_INITIALIZED_LARGE_BUFFER_SOURCES.md)：v34完整initial的背景buffer来源及CPU allocation预算接通；9e97baf5…Standalone Private65552B/memberOffset65540，两捕获/八回跳/GPU122及TextureBuffer像素恢复、52组反例通过。frame64KiB/heap1MiB未扩大，继续实际2D纹理范围。全量最近a9a106aa…通过，整帧/UI未验收，持续目标active。

2026-10-02 [B374](BATCH374_CUBE_VIEW_PARENT_INITIAL_STATE.md)：view复用捕获parent初始数据，取消额外baseline副本；cube->cube和完整初始内容cube source接通。RGBA/BGRA四捕获/16seek/76反例、实际RG11B10两捕获/八seek/38反例，GPU122及packed/API像素一致。非法cube几何在Native创建前拒绝，避免断言退出。继续v34已初始化大buffer source；全量a9a106aa…已通过，整帧/UI未验收，持续目标active。

2026-10-01 [B373](BATCH373_BUFFER_TEXTURE_PARENT_REPLAY.md)：v32 Private TextureBuffer sourced ID和parent buffer API读回/PickPixel接通；a9a106aa…两捕获/八回跳，offset256 GPU122及overwrite/reset像素正确，40反例拒绝。普通非descriptor buffer blit受16条/64KiB范围与提交/alias检查约束。35b6237e…16实际格式Native独立decode/128 raw+PickPixel/八回跳/28反例通过，补RG16Uint创建遗漏；精确a9a106aa…全量308/7786/3080通过，growth7045120B；真实UE359个type9来源parent数据完整，仍需全资源范围/coverage，整帧/UI未验收，持续目标active。

2026-10-01 [B372](BATCH372_CPU_TEXTURE_BIRTH_RETIREMENTS.md)：v31允许退役前导中经已有合法性检查的buffer texture/heap texture创建，明确view parent依赖及GPU ID；a6c6c93e…80退役四捕获/16seek/128反例通过，view复制像素byte1..4及drawable核对通过。前版978f9a40…全量308/7786/3080通过，本版旧21类/674反例兼容通过。实际CPU候选122槽/121失效texture，仍需192×104 heap/Private TextureBuffer/格式范围与完整coverage适配；整帧/UI未验收，持续目标active。

2026-10-01 [B371](BATCH371_UNSUBMITTED_BUFFER_BLIT_RETIREMENTS.md)：v30未提交普通buffer blit精确tuple及256退役上限接通；978f9a40…80槽四捕获/16seek/112反例通过。真实UE CPU审计v30在buffer texture birth31573停止，覆盖66槽；CPU texture birth候选在首次CPU内容更新31665停止，122冻结槽/121纹理来源、五条复制15200B全部有界，无提交。精确978f9a40…全量308/7786/3080通过，growth12058624B；继续v31合法创建及视图父buffer依赖适配；整帧/UI未验收，持续目标active。

2026-10-01 [B370](BATCH370_PLACEMENT_TEXTURE_SCOPE_PLAN.md)：placement复用通用格式/Native footprint，支持Shared和cube-array并校验heap storage。e4d05e4f…Private158纹理/12312检查/110反例、Shared20纹理/1536检查/80反例通过，旧21类674反例通过；精确库全量308/7786/3080通过、growth9928704B。b2b56a6c…两个Shared来源已完整，另121个stale来源在首次提交前退役，75条超过v29前缀64上限、46穿插未提交buffer blit；继续验证提交前非Shader复制的退役规则。整帧/UI未验收，持续目标active。

2026-10-01 [B369](BATCH369_SHARED_TEXTURE_INITIAL_STATE.md)：Shared纹理起点GPU快照/逐slice-mip恢复接通，04a4de2d…20纹理/两捕获/1536检查/八回跳/80反例通过；Private148纹理110反例和Private view76反例兼容通过。CPU upload12反例兼容通过，实际b2b56a6c…489纹理/642042264B字节匹配、两个Shared来源初始数据完整；placement类型/格式小用例已通过，准备旧回归。整帧/UI未验收，持续目标active。

2026-10-01 [B368](BATCH368_CPU_RESOURCE_BIRTH_RETIREMENTS.md)：v29允许纯CPU buffer birth穿插精确generation退役，首个GPU命令边界立即结束；cbfa65f9…四捕获/16seek/92反例通过。针对实际734378ff…49个早退役stale来源；旧v16兼容90反例通过，Shared初始内容继续适配。整帧/UI未验收，无提交/推送，持续目标active。

2026-10-01 [B367](BATCH367_PRIVATE_TEXTURE_SUBRESOURCES.md)：array/cube/cube-array/3D及depth/stencil逐slice/mip初始内容和公共DecodePixelData接通。精确eda11f64…148纹理/两捕获/11544分子资源检查/八回跳/90反例通过；Native depth clear独立读回通过。Private view兼容76反例通过，实际734378ff…485纹理/622624824B初始字节全部匹配；定位49个穿插CPU birth的早退役slot和Shared初始内容缺口。精确库全量308/7786/3080通过、growth11403264B，UI待完成，完整UE尚未GPU replay，持续目标active。

2026-10-01 [B366](BATCH366_BACKGROUND_PRIVATE_TEXTURE_VIEW_RESTORE.md)：Private view明确parent初始数据校验、起点上传后建立baseline的顺序修复和Native staging局部pool接通。0eb096fd…四捕获/16seek/76反例、direct兼容56反例、frame view MRT八捕获/32seek/387反例通过。未重复全量；21411d96…308/7786/3080已通过。人工UI与完整UE未验收；继续实际array/cube/3D/深度初始状态，持续目标active。

2026-10-01 [B365](BATCH365_PRIVATE_COLOR_TEXTURE_INITIAL_STATE.md)：普通Private颜色纹理初始像素、Native staging pitch/紧密mip序列化、缺失数据和预创建mip校验接通。精确21411d96…四捕获/16seek/56反例、三mip行像素通过；组合全量308/7786/3080通过，growth8536064B。实际49e66f85…重截保存363份纹理/186669370B初始像素，逐逻辑布局字节数全部匹配、帧内来源错误0。整帧仍未GPU replay，继续Private view初始恢复顺序及array/cube/3D/depth等真实缺口，持续目标active。

2026-10-01 [B364](BATCH364_FRAME_TEXTURE_VIEW_SOURCES.md)：frame Private subset view sourced GPU ID、8捕获/32seek/387反例、旧21类/674反例通过。窗口最大化和UE编辑器Grow两层尺寸原因已对照源码修复，实际9738816a…重截GBuffer为320×240；1917活slot/114producer全部匹配，帧内来源/inline/lifetime错误0。整帧尚未GPU replay，继续普通Private纹理初始内容，持续目标active，无提交/推送。

2026-10-01 [B363](BATCH363_DESCRIPTOR_BACKING_REUSE.md)：实际UE的12处typed backing overlap完成CPU归属/slot退役审计；新增明确ResourceId对校验和重叠CPU快照后的活表重定位。连续截帧的旧退休表initial依赖遗漏已修复。精确246a9d5c…8捕获/32seek/176反例，Private兼容220反例、跨kind MRT兼容331反例通过；旧21类674反例通过；全量308/7786/3080通过，驻留增长13205504B。完整UE尚未GPU replay，人工UI待解锁，持续目标active。


2026-10-01 [B361](BATCH361_PRIVATE_HEAP_DESCRIPTOR_SOURCES.md) / [B362](BATCH362_ENCODING_TIME_ALIASES_AND_PARTIAL_SNAPSHOTS.md)：Private来源8捕获/32seek/216反例，未提交CB期间Private alias220反例，Private buffer→MRT texture跨kind331反例通过。Shared partial tail遗漏future alias提交快照已修复；精确57ba6f4a…8捕获/32seek/204反例及8份非冗余snapshot捕获/32seek（GPU首读184/248、后续225/161）通过，旧21类/674反例通过。57ba6f4a…全量308/7786/3080已通过、growth11943936B。实际UE 0649fac8…12处descriptor backing alias仍需适配；完整UE尚未GPU replay，UI待解锁，持续目标active。


2026-10-01 [B359](BATCH359_FRAME_TEXTURE_DESCRIPTOR_RELOCATION.md) / [B360](BATCH360_TRACKED_ASYNC_HEAP_ALIASES.md)：frame Private texture→MRT→下一pass来源重定位通过八捕获/32seek/319反例，a9c8e85b…全量308/7786/3080通过，增长11501568B。tracked Shared heap异步alias与useHeaps间接CPU快照遗漏已修复，863f5998…八捕获/32seek/200反例及兼容MRT/旧alias/18帧通过；该新库全量308/7786/3080通过，增长13942784B。UE 0649fac8…重截已保留，369条CPU快照、1669初始来源全部匹配，帧内source/producer/inline错误0；完整UE GPU replay仍未通过，继续Private/cross-kind alias与实际绘制范围，UI待解锁，持续目标active。


2026-10-01 [B357](BATCH357_PARALLEL_FIVE_MRT.md) / [B358](BATCH358_FRAME_PLACEMENT_TEXTURE_BIRTHS.md)：parallel/MRT5 八捕获/32seek/223反例通过；修复 UE 帧内 texture/view 被提前写入 initial section 和 heap 父依赖遗漏，四捕获/24seek、18旧帧/13反例通过。新 UE ef14a1ed… 65 texture/14view 按帧内顺序记录，1676初始来源/112producer/2737inlineVA无帧内错误，125CB/peak32。当前库 ecc44d57…旧21类回归中；bfe86da…全量308/7786/3080已通过。完整UE尚未replay，继续future texture来源与实际资源范围；UI待解锁，持续目标active。


2026-10-01 [B354](BATCH354_LEADING_DESCRIPTOR_RETIREMENTS_AND_CAPTURE_SIZE.md) /
[B355](BATCH355_IMPLICIT_PLACEMENT_BUFFER_ALIASES.md)：稳定320×240重新捕获
f744c413…，1678帧初来源及内容、112GPU producer、2737inlineVA全匹配，
126CB/peak33、170pass/393draw，完整UE尚未提交GPU replay。物理placement
alias保留两份对象和旧descriptor：8捕获/32seek/184反例通过（f69220dd…）；
v16旧21类/674反例通过。B353精确5b08e331…全量308/7786/3080已经通过，
后续精确新库全量待跑；UI待解锁。持续推进signal-only与异步alias。


2026-10-01 [B353](BATCH353_INTERLEAVED_SUBMISSIONS_AND_PARTIAL_TAILS.md)：真实 UE
75 个 command buffer、峰值 11 的交错创建/提交已复现；修复 partial seek
尾部队列预约死锁和 future submission 过早恢复快照。库5b08e331…，retained/
unretained 四捕获、16轮 seek、GPU 累加308/像素186-122、114反例、旧九帧通过。
B352 的0f684365…已完成308正例/7786反例/3080生命周期完整回归，增长13549568B；
B353 全量复跑中。新的320x240真实FirstPerson截帧8ebcbfa6…已保存并自动退出，
尺寸来自插件实际日志；原大帧保留，正在做CPU审计。完整UE replay尚未通过，
UI仍待解锁；持续目标active。

2026-10-01 [B351](BATCH351_UE_DRAW_CONSTANTS_AND_INDEXED_SOURCES.md) /
[B352](BATCH352_COMMAND_BUFFER_QUEUE_PROXY_LIFETIME.md)：UE IR绘制常量、UInt16/UInt32
静态索引来源和冗余提交快照验证接通；发现并修复临时queue的包装父对象悬空。
21类/674反例、真实UE更新shader小图形/27反例、双队列完成回调、旧八帧通过；
库0f684365…，queue连续10×2捕获通过。此前93eb26b1…的308图像/CLI与分段反例
至T312、3080生命周期通过（增长12369920B），该新库完整累计复跑仍待执行。
新UE隔离诊断模块feeed228…已编译，开始低负载进程设置自动截帧并退出；原帧
已保留。完整UE画面/MRT/pass尚未完成，UI仍待解锁；持续目标active。


2026-10-01 [B350](BATCH350_REGRESSION_RESOURCE_AND_FAILURE_LIFECYCLES.md)：ICB 动作树、
无 coverage 的普通帧内 buffer 回跳、共享 DummyDriver 与提交快照归属已修复。
308 份图像/CLI 阶段、T49 87反例与相关定向通过，全量剩余反例/lifecycle 执行中。
库93eb26b1…；最新 UI 待解锁，完整 UE 未完成，持续推进 UE 绘制参数与索引路径。


2026-10-01 [B349](BATCH349_EXPLICIT_FRAME_HEAP_ALIASES.md)：v13 显式 heap 退役与
同地址不同资源重定位、连续两捕获/四次 seek 图像通过；十九类/560 反例通过。
全量首轮 T20 ICB action tree 崩溃已定位并修正，重跑中；最新 UI 待解锁，
完整 UE 未完成。持续目标 active，继续回归与真实 UE 绘制参数/索引路径。


2026-10-01 [B348](BATCH348_ASYNC_SUBMISSIONS_AND_CPU_SNAPSHOTS.md)：v12 同队列
无捕获端 CPU wait 的两提交与 Shared 快照恢复同步通过，GPU 累加 308/正确像素。
十八类 /507 API+CLI 反例、真实 UE 更新 shader 小图形链路、旧六帧定向通过。
库 b2edab43…；全量未跑，最新 UI 未验，完整 UE 未完成。持续推进 frame heap alias。


2026-10-01 [B347](BATCH347_FRAME_HEAP_DESCRIPTOR_PAYLOADS.md)：v11 帧内 heap
payload/table 创建与来源重定位接通，四轮重建仍 GPU 字节/像素正确；十七类/
453 API+CLI 反例通过。库52b42a31…；全量未跑，新增 UI 未验，完整 UE 未完成。
持续目标 active，继续无 CPU wait 的有序提交与 Shared 快照同步。


2026-10-01 [B346](BATCH346_COMPLETION_WAITS_AND_SOURCED_SUBMISSIONS.md)：完成等待
不再为空；v10 同队列两提交及 waited CPU 描述符更新、未保留资源方式通过。
十六类 /383 API+CLI 反例、旧六帧定向、两队列捕获回调/恢复通过。库64714dcb…。
全量未跑，新增 UI 未验，完整 UE 未完成；持续目标 active，推进 heap/ring/异步提交。


2026-10-01 [B345](BATCH345_SOURCED_MRT_AND_PASS_SCOPES.md)：v9 双 MRT 与跨 pass
采样、action.outputs / colorTargets / BeginPass-EndPass / 四轮 seek 全部正确；
十四类描述符与 284 组 API+CLI 反例通过，库 c5570049…（该批验证版本）。
全量未跑，新增 UI 未验，完整 UE 未完成；持续目标 active，推进完成等待与多提交。


2026-10-01 [B343](BATCH343_SOURCED_GRAPHICS_DESCRIPTOR_CONSUMERS.md) /
[B344](BATCH344_ACTUAL_UE_UPDATE_TO_GRAPHICS.md)：v9 顶点/片元 inline 按来源重编码，
真实 UE 更新 shader 接入小图形消费者；两捕获/四轮 seek/正确 GPU 字节与像素通过。
十三类描述符 / 241 组 API+CLI 反例通过，真实 UE 链路另有 27 组通过。
库 490bf140…；全量未跑、新增 UI 未验、完整 UE 尚未完成。持续目标 active，
继续 MRT / 多 pass / 多提交 / alias。


2026-10-01 [B342](BATCH342_ACTUAL_UE_DESCRIPTOR_UPDATE_SHADER.md)：从真实UE
截帧提取原编译UpdateDescriptorHandle shader，单条/四条更新原生GPU、各两捕获、
四轮seek与API字节/CLI通过；payload为opaque copy，尚无该真实UE条目的consumer。
库仍37063340…，全量未跑/新增UI未验/完整UE未完成，持续目标active；继续
vertex/fragment来源重编码和draw验证。


2026-10-01 [B341](BATCH341_FRAME_SOURCED_BUFFER_TABLES.md)：普通buffer创建与
临时表声明正确入帧；v8新建source/table回跳重建，强迫VA每轮改变仍采样正确。
十二类tiny/214 API+CLI负例及旧六帧定向通过。真实UE a5907a66…的1,479帧首槽/
106 producer来源全部匹配，仍CPU证据。库37063340…；全量未跑，新增UI未验，
完整UE未完成，持续目标active。继续真实UE更新shader的单pass GPU验证。


2026-10-01 [B339](BATCH339_UE_NATIVE_DESCRIPTOR_FIELDS.md) / [B340](BATCH340_GPU_TEXTURE_DESCRIPTOR_UPDATES.md)：
UE原生buffer/texture/sampler字段及Texture4/5 buffer view接通；GPU纹理描述符
计算更新后实际采样122→186/186→122、双捕获/seek/像素通过。十一类tiny /171
API+CLI负例、3D/BC上传GPU407及12负例、旧六帧定向通过。新UE b341dea4…
1,477帧首槽/31 producer来源匹配；仍CPU证据。全量未跑，新增UI未验，完整UE
图像/MRT/pass replay未完成；持续目标active，继续frame births/alias/render。
最终库a0f35fed…，未提交/推送。


2026-10-01 [BATCH338](BATCH338_SOURCED_COMPUTE_PRODUCERS.md)：计算更新tiny实际
GPU与seek通过，九类/123 API+CLI负例及旧六帧通过。真实UE新帧5039ca0f…的
1,477帧首槽和118 GPU producer/expected/source关联全部匹配；仍仅CPU证据。
下一项对齐UE真实Buffer/Texture/Sampler类型，再扩展frame/alias/render。
全量未跑，新增UI未验，完整UE未完成，持续目标active。库78130e17…。


2026-10-01 [BATCH337](BATCH337_SOURCED_GPU_DESCRIPTOR_COPY.md)：sourced slot GPU
复制实际41→80、GPU初始状态80→41及回跳通过；八类tiny/99 API+CLI负例通过，
库4bd948d2…。全量未跑，新增UI未验，完整UE仍未完成；继续计算scatter路径。


## M10 / BATCH334–336：UE 帧首来源闭合，小帧 sourced replay 与应用队列同步通过

B334 完成临时 payload、static sampler、各 stage inline 显式来源与冻结 BG
metadata。B335 sourced slot/inline 实际 GPU 80、普通常量及重复 seek 通过。
B336 对照 D3D12/Vulkan 修复应用队列帧首等待、每队列 enqueue/epoch 与自动
present 锁；真实 UE 新帧 a08ef579…的 1,476 个活槽均与 Initial Contents 完全
一致（旧帧 10 处 GPUExpected 差异）。tiny 两队列/回调/失败恢复/API+CLI
通过；七种描述符及 78 负例、旧六帧终端通过。当前库 12dd80aa…。
**全量未跑，本批人工 UI 未验，完整 UE 图像/MRT/pass replay 仍未完成。**
持续目标 active，继续 GPU 更新/多提交生命周期，不提交或推送。见
[B334](BATCH334_UE_SOURCES_AND_FROZEN_SNAPSHOT.md)、
[B335](BATCH335_SOURCED_SLOT_REPLAY.md)、
[B336](BATCH336_CAPTURE_QUEUE_COMPLETION.md)。


## M10 / BATCH332–333：inline小帧通过，官方局部MetalRHI provider进入真实截帧验证

compute typed setBytes的VA/常量/GPU字节/seek及56组负例通过；B333槽位两代诊断
六条payload/顺序与GPU前拒绝通过。六类tiny终端通过，当前库1ee8ba5e…；
**全量未跑，新增Viewer未验，完整UE replay仍未完成。** 官方5.8.3局部源码已取到，
无需用户另提供源码版；隔离MetalRHI 98/98导出、NullRHI成功、正常Metal NewMap
启动成功，capture倒计时修复后自动重截/CPU审计进行中。早期UBT导出误删123库
已按官方manifest全部恢复且复核。持续目标active，不提交/推送。见
[BATCH332](BATCH332_INLINE_DESCRIPTOR_BYTES.md)、[BATCH333](BATCH333_UE_PARTIAL_PROVIDER.md)。

## M10 / BATCH331：帧内buffer/view小帧重定位通过，UE provider仍缺失

coverage v3接通Shared/Private非重叠frame placement及支持的buffer-backed view，
先按CPU创建时序检查所有typed更新，回跳保留GPU等待并重建对象。五种小例各
两捕获、GPU输出/像素/seek和48API+CLI负例通过；旧六帧终端通过。最终库/app
`5fab70bfcc92…`，Private view人工Viewer EID15/36/15输出89/167/89通过；
**全量未跑，完整UE仍metadata拒绝，目标未完成。** 现有UE为预编译安装版，
插件缺私有slot/临时表/CPU来源/inline布局provider，已询问可重编译源码环境路径。
无需现在重截。见[BATCH331](BATCH331_FRAME_DESCRIPTOR_RESOURCES.md)。

## M10 / BATCH330：重复身份误拒绝修复，两类小帧人工UI通过

用户已解锁。508条帧内身份分为164条完全相同副本、344条新身份；修复副本
误拒绝，并拒绝状态冲突的sampler别名。真实延迟getter用例、两类GPU/seek、
15API+CLI负例和旧六帧通过。最终库/app `78e3cecc338b…`；人工Viewer两类
小帧的122/161/122与41/121/160/41、哨兵通过，正常退出。UE344个新身份
（278placement buffer、66buffer-backed view）及临时表/槽位/provenance/render
仍未支持，整帧GPU前拒绝。**定向终端与这两类小帧人工UI通过；全量未跑；
完整UE目标未完成，旧待验保留。** 见[BATCH330](BATCH330_DUPLICATE_IDENTITIES_AND_VIEWER.md)。

## M10 / BATCH328–329：小表真实 replay 通过，UE 保存崩溃已修，完整回放仍未完成

显式 typed descriptor 重定位接入 replay，CPU partial diff 与 GPU copy/显式 CPU
entry 混合更新分别得到122/161、41/121/160，双向seek与13个API+CLI负例通过。
最终库/app `bd255367cb60…`，旧六帧API+CLI通过。UE插件已记录实际primary表
布局，并修复present前end和thumbnail drawable生命周期；一次UE应用崩溃后
自动重截end=1，新帧 `7cd838c7…` 已保护备份。CPU证据有508条帧内首次身份
查询、至少344条来自帧内新资源，且缺临时表/有效槽位/CPU写入来源coverage。
完整UE仍在GPU前拒绝，未提交整帧。见[BATCH328](BATCH328_EXPLICIT_DESCRIPTOR_REPLAY.md)
与[BATCH329](BATCH329_UE_DESCRIPTOR_LAYOUTS_AND_DRAWABLE_LIFETIME.md)。
**定向终端通过；全量未跑；人工UI被本机锁屏阻止，已请求手动解锁；UE replay
目标未完成，无需再次重截。没有提交/推送，旧待验保留。**

## M10 / BATCH327：间接资源捕获遗漏已修，自动重截完成，UE replay仍未实现重定位

用户1f6b4010…帧已取得899条地址/ID诊断，却缺全部sampler创建；原生105
小夹具复现useHeap/raw packet间接资源被过滤。参照D3D12 RefBuffers，capture
开始保守引用所有查询过GPU身份的live对象，缓存getter也保留frame引用。
三变体×两次捕获、已销毁对象排除与GPU前拒绝通过，旧六帧API/CLI通过。
助手使用插件自动入口重截NewMap（无需用户再次操作），新帧1e4a2f80…含
68个sampler创建，109个非零sampler条目全部有唯一或同描述符别名候选；资源
表在最后CPU快照后仍缺32个VA/123个texture ID，不能证明其无shader访问。
独立进程原生typed packet重编码保留+4 offset/常量并得到122，但未接入UE
replay/GPU descriptor更新。当前库/app `6c0e5d6f8cfe…`；无遗留UE进程。
详见 [BATCH327](BATCH327_GPU_IDENTITY_RESOURCE_CLOSURE.md)。
**定向终端通过；UE API/CLI是GPU前预期拒绝；全量未跑；人工UI未验。
UE正常开启与replay目标仍未完成，已有帧可继续开发，不要求再次重截。**

## M10 / BATCH326：本地新UE帧缺GPU身份映射，诊断库已就绪，需用户重截

2026-09-30用户新帧 `UE58_capture.rdc` SHA `5d73323bafc3…` 仅CPU审计：
7912 chunks、181 draw、83 pass、10 MRT、59 commit；12 heap声明6 GiB。
用户实际新建NewMap Default模板，有Lumen/VSM/sky scope。buffer view创建
顺序正确，两个terminal Empty后无显式引用；资源表Buffer24保留886个GPU VA
和2106个texture ID，而捕获缺少原生身份映射，不能安全直接回放。
新增三个getter的诊断chunk1397与GPU前保守拒绝，不代表descriptor重定位。
极小原生/注入（两种时序×两份capture）/CPU metadata/API+CLI拒绝、旧
T01/T09/T35/T49/T52/T62 API/CLI及20次lifecycle、terminal Empty bytes/seek与
负例通过。最终库/app `c7cb40b786ff…`。原UE帧已另存同hash备份，.command
改为启动已存在的NewMap。**第一项待办：用户用同一.command的新库重截；
再检查descriptor原地址/ID归属，设计完整重定位。** 当前UE未启动。
详见 [BATCH326](BATCH326_GPU_IDENTITY_DIAGNOSTICS.md)。
**定向诊断/旧小帧终端通过；全量未跑；UI未验；新UE帧未GPU回放，目标未完成。**

## M10 / 本地 M2 Pro 接手：小夹具通过，Testproj 插件就绪，等待新帧

2026-09-30：干净工作树从 e0a26f7e6 快进到 GitHub 默认分支
`c4be68bb7fce662e4dd8498408981fa2e824c3b6`；未提交或推送。本机 M2 Pro /
16 GiB / macOS 26.1 / Xcode 26.0.1，UE 5.8.3 Testproj 插件、终端库及 viewer
均编译成功，库/app 内嵌库 `f4f4e9a4cae5…`。4 KiB 原生/注入/API/CLI、
GPU 字节/seek、一个 terminal Empty 新负例及 T35/T62 API/CLI、T62 十个
旧负例定向通过。增加 CPU audit 的 heap/CB/event 明细；修正 UE 启动脚本
的地图参数位置。Entry 重截会话 `20260930-175932` 已由日志确认 API 解析、
引擎初始化、Entry MAP LOAD 与 15 FPS/50%/Nanite off，待用户按钮截帧。
远端 UE 原件不在本机，未开完整大帧。详细命令、hash、
修改与下一步见 [本地接手记录](LOCAL_M2_SETUP_2026-09-30.md)。
**仅定向终端通过；全量回归未跑；人工 UI 未验收；UE 真帧 replay 仍未证明。**

本地 Mac 接手文字见 [2026-09-30 交接 prompt](LOCAL_MAC_AGENT_HANDOFF_2026-09-30.md)；
远程机停止 UE 大帧 GPU/GUI 回放。

## M9 / BATCH325：确认 23:15 WindowServer watchdog 重启，远程机继续停测

23:15:38 panic 明确为 WindowServer 连续 120 秒未 check in；其主线程
在 Metal/IOGPU/AGXG16X 提交路径，23:12 的快照里两个已终止的 UE 回放
探针仍列为 zombie，线程也停于 GPU 内核路径。此前 20:43 曾发生同类
watchdog，却没有这两个探针，所以本次与回放高度关联但不能判定唯一
根因或具体 Metal chunk。超时与杀进程不足以保证 GPU 内核工作结束。
本机停止 UE 大帧 GPU/GUI 回放；只读审计与编译可继续。见
[BATCH325](BATCH325_WINDOWSERVER_WATCHDOG_2026-09-29.md)。
**真帧未打开；无新定向 GPU 通过、全量回归或人工 UI 验收；累计成功
UI QA 增量 0。**

## M9 / BATCH324：UE 真帧停在 GPU 完成等待，本远程机停止大帧回放

用户提供的 API-only 日志未产生结果文件，不能判定成功或失败。助手在同一
旧库与 view 顺序诊断副本上做 12 秒和 18 秒两次有界 API 定位；后一次
栈采样落在 `FinishReplayCommands` → Metal `waitUntilCompleted`，物理
footprint 8.3 GiB，而脚本 RSS 仅采到 923.3 MiB。超时杀进程时系统拒绝
SIGKILL，进程处于 `?Es`；WindowServer 曾约 6.34 秒不就绪后恢复。
无新重启，但已达到本机停测门槛。XML 静态统计：12 heap/6.062 GiB、
64 次 commit、无编码 wait event；尚不能确定是 GPU 长任务还是驱动/回放
挂起。已修正两处扫描警告和脚本异常退出记录，终端库重建，**未用新库
再次跑 UE 真帧**。见 [BATCH324](BATCH324_UE58_GPU_WAIT_AND_REMOTE_HOST_STOP.md)。
**UE 真帧仍打不开；全量回归未跑；人工 UI 未验收；累计成功 UI QA 增量 0。**

## M9 / BATCH323：帧尾 buffer Empty 小夹具通过，UE 真帧待一次有界回放

在 D3D12/Vulkan 完成后回收与 UE 5.8 `DeferredDelete` 的横向对照后，
接通帧尾 buffer `Empty`：执行前拒绝后续引用，待已提交 GPU 工作完成后
执行，并在事件回跳前恢复。当前 Mac 的 4 KiB 原生 Validation、注入捕获、
API/CLI、GPU 字节/seek 及 1 例新负例通过；T62 的 10 例旧负例通过。
T62/T35 旧帧 API/CLI 通过，T319 专用 API/CLI 通过（通用 smoke helper
误用 T35 断言而失败）。原始 `UE58_capture.rdc` 的 15 个旧 view 顺序错误
仍在；已只读原件并生成 SHA 与 BATCH321 相同的诊断副本供有界验证。
旧真帧尚**未**用本批库做 GPU 回放，
也没有新人工 UI 验收；黑 RT 与 Shader Converter GPU VA 仍待核实。
见 [BATCH323](BATCH323_TERMINAL_PURGEABLE_REPLAY.md)。**仅定向终端通过；
全量回归未跑；人工 UI 通过 0，累计成功 UI QA 增量 0。**

## M9 / BATCH322：真帧 CPU 审计，GPU 完成边界仍未实现

在不执行 GPU 回放的条件下，`UE58_capture.rdc` 已可只读导出 XML：
11214 chunk、278 draw、89 render pass、10 个 MRT pass，确有 UE 场景与
Nanite scope。旧 `frame1770` 仍是单 RT 的 Slate 帧，不能解释真帧。
真帧的两个 buffer 经 command buffer blit 使用、提交后在 11174/11175
被置为 `Empty`，而捕获不含等待完成的 chunk；安全拒绝仍保留。原生
4 KB completion/Empty 探针仅编译，未在这台发生过 WindowServer watchdog
的远程 Mac 上运行。见 [BATCH322](BATCH322_UE58_CPU_AUDIT_AND_COMPLETION_PROBE.md)。
**仅 CPU 定向审计和编译通过；全量回归未运行；人工 UI 未验收；累计成功
UI QA 增量 0。**

## M9 / BATCH321：UE58_capture 原因定位；WindowServer watchdog 后暂停 GPU

用户 `UE58_capture.rdc`（SHA256 `a96e685f…`）确由受控 viewport 按钮保存，
但 15 个帧内创建的 buffer texture view 被错误写到父 placement buffer 之前。
捕获顺序代码已修，最小原生/注入/API/CLI/GPU 像素及事件回跳曾在重启前通过。
旧帧的诊断副本越过该处后，回放在帧内 `PurgeableStateEmpty` 触发 Metal
Validation 断言；随后本机发生 WindowServer watchdog 重启，因果未归属。
当前加了 GPU 执行前的安全拒绝，**旧帧仍不能正常开启**，等待在其他机器
验证完整 GPU 完成/资源回收功能族。重启后只完成无 GPU 的编译；定向回归、
全量回归、人工 UI 内容验收均未运行。详见 [BATCH321](BATCH321_UE58_CAPTURE_VIEW_ORDER_AND_WATCHDOG.md)。
累计成功 UI QA 增量 **0**。

> 新缺口的优先处理方法见 [跨 API 横向排查顺序](CROSS_API_TRIAGE.md)：
> 先查 RenderDoc 其他图形 API 与引擎源码，再处理 Metal 特有差异。

最后更新：2026-09-29（Asia/Shanghai）

## M8 / BATCH320：frame1770 人工 UI 内容失败，scope 已接通，黑屏仍在

用户已在 qrenderdoc 人工打开 `UE58_frame1770.rdc`：文件能打开，但无 UE
scope、唯一 RT 黑色，**内容验收失败**。XML 证实 UE 已发出 10 组
compute scope 和一个 `SlateUI` render scope；RDC 主体是 280 draw 的
Slate 合成帧，并非场景 GBuffer 帧。新 viewer 将 Metal debug group 映射到
事件树；旧 UE 帧终端显示 21 marker、280 draw。项目级按钮已按 UE 官方
RenderDoc 插件的受控 viewport 绘制顺序修改，UE 插件及库/viewer 构建通过，
但尚未点击新按钮验证真正场景截帧。旧帧整幅 RT 于首 draw 前后及帧尾均
全零；Shader Converter GPU VA 缺通用映射，尚未修复或证明唯一根因。
详见 [BATCH320](BATCH320.md)。**定向终端通过；全量回归未运行；人工
UI 已执行且内容失败；累计成功 UI QA 增量 0。**

## M7 / BATCH319：UE58 frame1770 定向终端打开通过，后续 UI 结果见 BATCH320

用户新截 v0x10 `UE58_frame1770.rdc`（29,633,881 字节，SHA256
`75dad9cecc5876c71db77de0e3a46b0f51371118e2d28ff3584113b148dda51d`），
注入库和插件 API 解析、保存均有日志；实际场景仍是 `Lvl_FirstPerson` PIE。
首次 API 打开通过，CLI 在帧内 Shared placement buffer `410638` 的帧首
重置失败后预览崩溃。按 D3D12/Vulkan 帧首资源边界修复后，同一帧一次
有界 Metal Validation API/CLI 均退出 0；T319 原生/注入、GPU 值、事件
回跳、两例负例，以及 T35/T59/T117/T313/T314/T317/T318 相关定向回归
通过（T318 使用专用探针）。最终库与 viewer 内嵌库 SHA256 均为
`c9bdc6bea56256140df635a65c30d5b56531c78a233b24a23568a5a372acfd6a`。
详见 [BATCH319](BATCH319.md)。**当批终端定向通过；全量回归未运行；
后续人工 UI 内容验收失败见 BATCH320，累计成功 UI QA 增量 0。**默认
bindless GPU VA 重定位及 Empty 最小场景仍待验证。

## M7 / BATCH315–318：BC placement 小夹具通过；UE v0xF 帧需重截

用户按钮生成 `UE58_frame5394.rdc`（SHA256 `fe304ecbd3392a4a4a35e45531e74e1b963bc266b558583ef5645d96c64f4081`），
确认 dyld 注入和截帧保存；缩略图是 `Lvl_FirstPerson`，不是 Empty。
一次有界 API 打开先拒绝 UE typed buffer view；定向修复后第二次拒绝
BC5 placement 纹理。BC1/BC1 sRGB/BC5 的原生 Metal Validation、
v0x10 帧首块快照、帧内拷贝、T317/T318 注入截帧、API/CLI、原始块
及 GPU 像素/事件回跳均通过；5 例负例安全拒绝，旧 T37/T117/T313/T59
定向通过。最终库和 viewer 内嵌库 SHA256 均为
`c13816cf6485ab945aa08ba476bc173613d5fbd609cb15518a6e65aabee49354`。
旧 v0xF UE 帧缺 BC 帧首初始内容，最终库有界 API 打开安全拒绝于 BC1
placement；不能继续用旧帧判断后续桥接。下一步用 v0x10 新库在 Empty、
非 Nanite 场景重截一次，再有界回放定位首个新阻塞。默认 bindless GPU VA
重定位仍未解决。详见 [BATCH315–318](BATCH315-318.md)。
**定向终端通过；全量回归和人工 UI 未运行；累计 UI QA 增量 0。**

## M7 新帧重截入口：前一阶段记录

本机 UE 5.8.3 项目与当时注入库预检通过；新增显式 `.rdc` 的有界 API/CLI
单次回放脚本，并以 T313 小帧验证其运行。磁盘上尚无采用 v0xF 新库的
UE 帧，旧 `frame4476` 不能验证 placement 修复。Shader Converter
bindless 表项是横向审计发现的后续风险，尚非新帧实测首阻塞。
18:26 新 UE 会话已确认 dyld 注入、插件 API 解析和引擎初始化；此前
`/tmp` 与 `/private/tmp` 路径字面比较导致的插件拒绝已通过启动脚本的
物理路径规范化修正。等待用户点击一次截帧。
见 [重截与回放入口](UE58_M7_RECAPTURE_GATE_2026-09-29.md)。
**全量回归和人工 UI 未运行；累计 UI QA 增量 0。**

## M7 / BATCH313–314：placement buffer 复用定向闭环，UE 旧帧仍拒绝

按 [跨 API 排查顺序](CROSS_API_TRIAGE.md)对照 D3D12/Vulkan 的独立资源身份与
UE `FreeBlock`，本机 Metal Validation 确认 placement buffer 原生 aliasability。
新 v0xF 截帧保留帧内 placement 创建顺序；T313 显式 alias、T314 释放复用的
原生/注入/API/CLI、GPU 字节及两轮事件回跳通过，受影响旧 T116/T117/T131/T132
定向回归及 39 例相关畸形输入通过。库和 viewer 内嵌库 SHA256 均为
`243043fb9dc1459afd3e1c2a6571dd7b880e1b76bf5b2122156a28a695fa1337`。
旧 v0xE `frame4476` 在最终库下 API/CLI 各单次仍安全拒绝于
`MTLHeap::newBuffer(offset)`；它缺帧内创建/释放时序，必须用新库重截。
详见 [BATCH313–314](BATCH313-314.md)。**没有全量回归或人工 UI 验收；
累计 UI QA 增量 0。**

## M6 / UE58 frame4476：首个回放阻塞为 placement heap 复用

用户完成默认 bindless 配置下的 UE 5.8.3 截帧后，编辑器已退出。
`UE58_frame4476.rdc` SHA256 `ab0e5a2918af5568a0257eb6f316140f68954f26d6c09e0b4f17606434b5bcbb`
（149,219,170 字节）。嵌入缩略图显示 `Lvl_FirstPerson` 编辑器视口，
不是 Empty 关卡。一次 Metal Validation API `OpenCapture` 在约 2 秒后
安全拒绝：placement heap `6047` 的 buffer `1374505` 与先前
`1373003` 占用区间重叠；无新 GPU Validation 报错。
详见[新帧首个阻塞证据](UE58_M6_FRAME4476_PLACEMENT_LIFETIME_2026-09-29.md)。
**此旧帧尚未打开**；本节首次诊断时 CLI 回放未运行，后续结果见上方 M7。
全量回归、人工 UI 验收均未运行，
累计 UI QA 增量 **0**。

## UE 5.8.3 `-BindlessOff` 启动隔离失败（16:22 session）

本机 `SocoTestProj` 的 `METAL_SM6` 编辑器在注入库
SHA256 `19d491f0…`、附加 `-BindlessOff` 后进入全局着色器编译，
未创建窗口。16:46 日志出现 3157 个全局着色器编译错误；
27 分钟时终止失败启动。没有新 `.rdc`，也没有回放或人工 UI 验收。
这证明该隔离命令在当前项目不可用，尚未单独证明编译失败归属 UE、
参数组合或注入。见[启动失败证据](UE58_M6_BINDLESSOFF_STARTUP_2026-09-29.md)。
去掉参数后的新 session `20260929-165339` 已由终端确认 UE 初始化和插件
解析 RenderDoc API；用户已截得 `UE58_frame4476.rdc`（149 MB，
SHA256 `ab0e5a29…`），缩略图是 `Lvl_FirstPerson` 编辑器视口。
截至记录时 UE 仍运行；新帧 GPU 回放尚未执行。
累计 UI QA 增量仍为 **0**。

## M5 / UE 5.8.3：Private 初始状态小夹具闭环，frame833 仍超时

本批接通 Private buffer 的帧首 GPU 上传和每次回放重置；原生/注入、
API/CLI 像素与回跳、未知资源负例及 T13/T21/T29 定向回归通过。
`frame833` 的 420 个 Private buffer（421,200,384 字节）上传成功，
但单次限时 75 秒 CLI 仍在 `ResourceId::503251` 等待超时；没有新
Validation 报错。UE SM6 bindless descriptor heap 中有捕获进程的
GPU VA，旧帧没有原地址映射，属于下一步待证明和处理的完整资源族。
详见[本批证据](UE58_M5_PRIVATE_INITIAL_BINDLESS_2026-09-29.md)。
**这张帧未打开**；全量回归和人工 UI 未运行，累计 UI QA 增量 **0**。

## M4 / UE 5.8.3：Private 间接绘制小夹具通过，frame833 仍未打开

现有 `UE58_frame833.rdc` 无需重截，viewer 已重建；本轮对 Private
普通/indexed 间接绘制及 buffer-only compute 的小夹具做了原生、注入、
API/CLI 定向验证，旧 T13/T21/T29 单次回归通过。UE 帧先越过原
`contents()` 断言及两个 compute 安全拒绝，但 14:21 的单次 CLI
60 秒超时，已终止；20 秒短 trace 进一步定位到等待
`ResourceId::503251`（XML 创建 chunk 3921、提交 chunk 10180）未完成。
暂停重复大帧回放，下一步静态审计其提交/依赖图并做小夹具。
库及 app 内嵌库 SHA256 `42ada08ec64c…`。
详见[本批证据](UE58_M4_PRIVATE_INDIRECT_2026-09-29.md)。
**新帧未通过**；全量回归及人工 UI 未运行，累计 UI QA 增量 **0**。

## M4 / UE 5.8.3 先前停点：frame833 回放仍阻塞，停止 GPU 复测

对 `UE58_frame833.rdc` 的定向诊断已越过 placement 纹理等早期拒绝，但
12:22 的 Metal Validation 在 `drawPrimitives(indirect)` 对 Private buffer
调用 `contents()` 时触发进程断言。已加入存储模式前置检查，同类 indexed
路径亦检查；未验证的 compute buffer dispatch 放行已撤回。viewer/CLI
重新编译通过，内嵌/CLI 回放库 SHA256 `319fd504dece…`；
**没有在新 GPU 错误后再运行回放**。因此本帧仍不可称为
可打开或画面正确；最后实测的阻塞是 GPU 可写、Private buffer 的
间接绘制参数与执行点元数据。撤回未验 compute 放行后，当前构建的
首个安全拒绝点尚未实测，可能更早。详见
[本批证据](UE58_M4_QRENDERDOC_3D_PLACEMENT_2026-09-29.md)。
当前构建没有全量回归或人工 UI 验收，累计 UI QA 增量 **0**。
请勿将当前 app 视为 `frame833` 的可用 viewer：Private 间接绘制现在会
明确拒绝，GPU 生成的参数仍需执行点读回和完整验证。

## M4 / UE 5.8.3 先前停点：新按钮截帧已保存，回放停在 3D placement 纹理

用户 `20260929-064750` 会话用 SHA256 `1ab4448a93b2…` 注入库成功保存
`UE58_frame833.rdc`（`472dfa48a56c…`，14,660,781 字节），说明上批
截帧结束的生命周期修复已在这次 UE 会话跨过原崩溃点。独立 worktree 的
`build-qrenderdoc/bin/qrenderdoc.app` 已编译，内嵌回放库与同次 CLI 构建均为
`8f79ed3345fd…`；**未启动 GUI**。该 CLI 在 Metal Validation 下单次打开
新帧退出 1，安全拒绝于 `MTLHeap::newTexture(offset)`：首个不支持的描述符
`MTLTextureType3D`（type 7），并非日志初看显示的资源选项或 hazard 值。
帧中共有 230 个 placement 纹理创建，含 2D、2D array、3D、cube；
后续格式、mip 和深度也超出现有 T117 的 2D 子集。需以完整功能族验证后
再扩展守卫。详见[本次构建与阻塞证据](UE58_M4_QRENDERDOC_3D_PLACEMENT_2026-09-29.md)。
**定向终端只确认了截帧保存和首个回放阻塞**；全量回归及人工 UI 未运行，
累计 UI QA 增量 **0**。
用户首次打开 viewer 于 11:50:01 在 `PythonContext::GlobalInit` 的
`PyDict_SetItemString` 闪退，第二次进程正常运行；同一栈在前一天也曾出现。
首次自动生成 Python stubs 写了版本标记却未生成文件，疑似 Python 3.14
兼容或失败后未清理错误状态；与新帧 Metal 回放阻塞是两个问题，详见上链证据。

## M4 / UE 5.8.3 先前停点：按钮截帧结束崩溃，生命周期修复待 UE 复测

用户 `20260929-024537` 会话确实在按钮请求后崩溃：RenderDoc
`EndFrameCapture` 向错误的原生对象发送 `waitUntilCompleted`，没有新 UE
`.rdc`；UE 随后在错误处理内卡住约两分钟。已按[跨 API 排查顺序](CROSS_API_TRIAGE.md)
核对并修复 command buffer record 跨 autorelease pool 的代理/原生保活。
本机原生及注入短 pool 夹具、按钮式 trigger/direct present、API/CLI 单次
回放通过；最终 dylib `1ab4448a93b2…`，详见[本批证据](UE58_M4_CAPTURE_LIFETIME_2026-09-29.md)。
当时尚未用新库复测 UE；后续 `20260929-064750` 会话已保存新帧，见上节。
全量回归和人工 UI 未跑；累计 UI QA 增量 **0**。

## M4 / UE 5.8.3：已有真帧定向终端回放通过

独立 worktree `renderdoc-metal-t312` 的 UE 真帧 `UE58_frame99.rdc` 现可在
Metal Validation 下单次 CLI 完整打开；API 对 165 个 draw 的首、四分之一、
中间、四分之三、末尾及回跳首事件检查 pipeline 身份通过。交错 command
buffer 的原生/注入截帧此前取得，本批 API GPU 数据/seek、CLI 和 2 例对象
身份负例通过；T35/T41/T56/T114 等旧帧定向回归通过。修复保留各 buffer
的 encoder 与状态，在捕获的 commit 点提交，并在 commit 前恢复 Shared CPU
写入；未活动的 Metal 反射参数不再强制要求 argument packet。细节见
[本批证据](UE58_M4_INTERLEAVED_REPLAY_2026-09-29.md)。

这是**定向终端通过**，尚无本批新 UE 截帧、原生 UE 画面/GPU 输出逐项对照、
全量压力回归或人工 UI 通过证据。当前项目亦未固定为严格非 Nanite 最小
场景；该里程碑仍待验证。累计 UI QA 增量 **0**，原 274 份待验不变。

## M4 / UE 5.8.3 先前停点：已截一帧，回放阻塞（历史）

2026-09-29 本机首次取得 UE 真帧 `UE58_frame99.rdc`（SHA256
`8a55f0e3738d8b10255dc855e13933ef6e9ab1055f7f59831fe3cfdba4a51022`）。
插件日志确认注入并保存。定向补齐 Shared Placement、4096 样本 counter buffer
和资源 `PurgeableStateEmpty` 后，单次 CLI 回放到达有效的**交错 command buffer**
时序：UE 先创建多个 buffer，再返回较早者编码。当前回放器在创建下一 buffer 时
提前提交上一 buffer，Metal Validation 于 `setCurrentCommandEncoder:` 触发
SIGABRT。现在各 encoder 入口及 commit 增加活动/未提交守卫；同一帧安全拒绝于
`MTLCommandBuffer::blitCommandEncoderWithDescriptor`，仍**不能打开 UE 帧**。
原生/注入的最小交错夹具重现该回放缺口；详见
[本机证据](UE58_M4_EVIDENCE_2026-09-29.md)。出现新 GPU Validation 错误后未再升级
UE 负载。全量回归、长时压力和人工 UI 均未运行；累计 UI QA 增量 **0**，
原 274 份待验不变。

## M4 / UE 5.8.3 先前接入检查点

见 [UE58_M4_INTEGRATION_2026-09-28.md](UE58_M4_INTEGRATION_2026-09-28.md)。
独立 worktree 从 `e0a26f7e6` 建立，保留旧目录 4 项改动。UE 5.8.3
项目级按钮插件已编译且注入库确实进入 UE。首次 blit pass 崩溃复现为
包装 counter buffer 传给原生 descriptor，以及跨 autorelease pool 的命令对象生命周期；
blit counter attachments 已接通为 Metal capture 版本 `0xD`。随后修复 compute
`useHeap`/`useHeaps` 包装对象被原生转发的问题，新 chunk 1384/1385。
本机两个功能族的原生 Validation、注入捕获、CLI/API 单次回放及 6+7 例
负例通过；T41/T101/T312 旧帧单次回放通过，详见批次记录。
最后一次 UE 启动 `20260929-001104` 已越过这两个崩溃点，随后在
`parallelRenderCommandEncoderWithDescriptor:` 的未接通守卫 SIGTRAP；
期间还有 AGX counter-buffer 类型错误及 heap identity 拒绝，须先定位。
**没有 UE `.rdc`，没有 UE API/CLI 回放、全量回归或人工 UI QA**；
累计 UI QA 增量 0，原 274 份待验不变。GPU 驱动调用链曾崩溃，停止继续增加 UE 负载。

## 当前安全检查点：BATCH311–312

公司 Codex API 临时接手请先读[2026-09-28 交接单](COMPANY_CODEX_API_HANDOFF_2026-09-28.md)；
本机暂停新增功能，等待额度重置后集中人工 QA。

- [BATCH311–312](BATCH311-312.md)：同步 tile/mesh pipeline 的 archive
  依赖与 archive-miss 选项在版本0xC接通。T311/T312 原生/捕获/API/CLI、
  本族315例畸形输入、32份跨族定向回归、T35/T305–T312各10次生命周期
  通过；旧版本截帧兼容。库/app `2f88058307f5…`，GUI待验**274份**；
  原始防御宏bridge/chunk **59/15**（含防御性拒绝与定义，不是未实现 API 数）。异步 tile/mesh archive 绑定仍拒绝；
  长时累计压力门禁与GUI未运行，旧panic未归因，本批无新增报告。

## 历史安全检查点：BATCH310

- [BATCH310](BATCH310.md)：Binary Archive mesh 函数添加接通为 chunk1383；
  T310原生/捕获/API/CLI与34例畸形输入、27份跨族定向回归、T35/T305–T310
  各10次生命周期通过。库/app `fecbaa45dd8a…`，GUI待验**272份**；
  原始防御宏bridge/chunk **57/15**。mesh pipeline archive 依赖尚未序列化；
  长时累计压力门禁与GUI未运行，旧panic未归因。

## 历史安全检查点：BATCH309

- [BATCH309](BATCH309.md)：Binary Archive tile 函数添加接通为 chunk1382；
  T309原生/捕获/API/CLI与25例畸形输入、22份跨族定向回归、T35/T305–T309
  各10次生命周期通过。库/app `13f127004ba0…`，GUI待验**271份**；
  原始防御宏bridge/chunk **58/15**。tile pipeline archive 依赖仍未序列化；
  mesh archive 变更仍拒绝。长时累计压力门禁与GUI未运行，旧panic未归因。

## 历史安全检查点：BATCH308

- [BATCH308](BATCH308.md)：Binary Archive 单节点 stitched library 添加
  接通为 chunk1381；T308原生/捕获/API/CLI与44例畸形输入通过，
  14份跨族定向回归、T35/T305–T308各10次生命周期通过。库/app
  `7c43ac13093f…`，GUI待验**270份**；原始防御宏bridge/chunk **59/15**。
  tile/mesh archive 变更与复杂 stitched graph 仍拒绝；长时累计压力门禁
  与GUI未运行。旧panic未归因，本批无新增报告。

## 历史安全检查点：BATCH307

- [BATCH307](BATCH307.md)：Binary Archive 的单个 visible 函数
  添加接通为 chunk1380；T307原生/捕获/API/CLI与43例畸形输入通过，
  13份跨族定向回归、T35/T305/T306/T307各10次生命周期通过。库/app
  `0a62ea89993e…`，GUI待验**269份**；原始防御宏bridge/chunk **60/15**。
  tile/mesh/library archive 变更仍拒绝；长时累计压力门禁与GUI未运行。
  旧panic未归因，本批无新增报告。

## 历史安全检查点：BATCH305–306

- [BATCH305–306](BATCH305-306.md)：空 Binary Archive + compute/render 函数
  变更两个新 chunk 1378/1379 闭环，同步/异步 pipeline 回放。修复旧版 capture
  条件字段误读；T301/T302 旧格式兼容。T305/T306 各30例畸形输入，T301/T302
  各19例旧格式畸形输入，12份跨族定向回归、T35/T305/T306各10次生命周期
  通过。库/app `1e0165e982e7…`，GUI待验**268份**。原始防御宏匹配
  bridge/chunk **61/15**；长时累计压力门禁与GUI未运行。旧panic未归因，
  本批无新增报告。

## 历史安全检查点：BATCH304

- [BATCH304](BATCH304.md)：异步 Stitched Library 新chunk1377闭环，
  T303/T304各17例畸形输入、12份跨族定向回归、T35+T303+T304各10次
  生命周期通过。库/app `a4547b2ebb75…`，GUI待验**266份**。
  原始防御宏匹配bridge/chunk **63/15**；复杂graph仍拒绝，旧panic未归因。
  长时累计压力回归与GUI未运行。

## 当前安全检查点：BATCH303

- [BATCH303](BATCH303.md)：单函数/单节点 Stitched Library 同步创建、
  函数表GPU调用及资源链已回放。17例畸形输入、11份跨族定向回归、
  T35+T303各10次生命周期打开通过。库/app `56bc82ca0211…`，GUI
  待验**265份**。bridge/chunk原始宏匹配**63/15**；异步 stitched
  后来在BATCH304接通，复杂graph仍明确拒绝。06:26 panic未归因，
  长时累计压力回归、UI未运行。

## 当前安全检查点：BATCH302

- [BATCH302](BATCH302.md)：同一 Binary Archive 的异步 compute/render
  pipeline 创建、依赖、GPU回放通过；19例畸形输入及T35+T302各10次生命
  周期通过。无新 bridge/chunk 数量变化；GUI待验**264份**。06:26 panic
  未归因，长时累计压力回归与 GUI 仍未执行。

## 当前安全检查点：BATCH301

- [BATCH301](BATCH301.md)：文件 URL Binary Archive 导入、payload 快照、
  compute/render pipeline archive 依赖与 archive-miss 选项已端到端接通。
  原生/捕获/API/CLI、原文件移走回放、19例畸形输入、8份跨族定向回归、
  T35+T301各10次生命周期通过。最终库/app `b297a988af63…`，GUI待验**263份**。
  旧chunk防御宏匹配 **17→16**；原 bridge 拒绝入口少1，但新增6个 archive
  变更方法显式拒绝，故原始 `METAL_NOT_HOOKED` 匹配 **59→64**，不可直接
  当成功能覆盖率。长时累计压力回归与 GUI 未执行，06:26 panic仍未归因。

## 当前安全检查点：BATCH299–300

- [BATCH299–300](BATCH299-300.md)：三/五实例复用两个不同 BLAS 的
  TLAS，新增可回放 chunk，保留旧截帧格式。两份原生/捕获/API/CLI、
  40例畸形输入、21份定向回归、2×10生命周期通过。库/app
  `ac97012d33b5…`，GUI待验**262份**；bridge/chunk宏匹配**59/17**。
  06:26 panic尚未归因，长时全量GPU回归及GUI未运行。下一阶段按
  [黑盒验收与稳定性门槛](BLACKBOX_GATE.md)转向崩溃隔离、M4/UE普通帧
  阻塞盘点，不再穷举光追描述符参数组合。
- [09-28稳定性复测](STABILITY_2026-09-28.md)：最终库对295份正常截帧
  分组API/CLI单次回放全过，全部295份分组各至少10次生命周期打开，
  另有3453例分组畸形输入按预期拒绝；未见新增panic。尚未跑完整累计
  脚本、295份×10同一进程或GUI，不归因旧panic。
- [Binary Archive资源链](BINARY_ARCHIVE_GATE.md)：T301已接通导入文件和两类
  pipeline；捕获期间修改 archive 的方法仍未支持。

## 历史安全检查点：BATCH298

- [BATCH298](BATCH298.md)：同一 BLAS 的 TLAS 实例上限由4扩到有界的
  65536；八实例原生/捕获/API/CLI、23例畸形输入、19份定向回归、
  10次生命周期打开通过。库/app `e67638f19213…`，GUI 待验 **260份**；
  bridge/chunk 原始宏匹配 **59/17**。仅八实例做 GPU 正例，不宣称
  65536实例已压测；长时全量 GPU 回归及 GUI 未运行。

## 历史安全检查点：BATCH294–297

- [BATCH294–297](BATCH294-297.md)：三/四个**不同** BLAS 构建 TLAS，
  补齐有序子资源数组、描述符快照、descriptor 分配与压缩复制路径。
  四份原生/捕获/API/CLI、72 例畸形输入、22 份去重定向回归、
  4×10 生命周期通过。库/app `bdc80ef81a6…`，GUI 待验 **259 份**；
  bridge/chunk 原始宏匹配 **59/17**。超过四个实例仍拒绝；长时全量
  GPU 回归及 GUI 未运行。系统只查到 06:26 的 `IOGPUResource` panic；
  快照含 `renderdoccmd`，panicked thread 属于 `kernel_task`/`IOGPUFamily`，
  测试可能是诱因，但没有足够证据判定根因，本批短测未复现。

## 历史安全检查点：BATCH290–293

- [BATCH290–293](BATCH290-293.md)：同一 BLAS 的 TLAS 从最多两个实例
  扩到三个/四个，含 descriptor 分配与压缩复制。四份原生/捕获/API/CLI、
  101 例畸形输入、29 份跨族哨兵、4×10 生命周期通过。库/app
  `986a0d98b8f8…`，GUI 待验 **255 份**；bridge/chunk 原始宏匹配
  **59/17**，但已有 bridge 守卫和 `buildInstances` chunk 的合法范围已
  实质扩大。超过四实例和两个以上不同 BLAS 仍明确拒绝；无新 panic，
  累计压力回归及 GUI 未执行。

## 历史安全检查点：BATCH284–289

- [BATCH284–289](BATCH284-289.md)：双三角形 indexed AS refit
  的 UInt16/UInt32、独立目标+压缩、换索引、换顶点及同时换两类资源。
  六份原生/捕获/API/CLI、300 例畸形输入、32 份跨族哨兵、6×10
  生命周期通过。库/app `2c2bc09e0e18…`，GUI 待验 **251 份**；
  bridge/chunk 原始宏匹配 **59/17**。无新 panic；累计压力回归和 GUI
  未执行。下一族优先选尚未接通的真实 bridge/chunk 资源链，
  不再仅以 indexed refit 参数组合扩大测试数；现有防御性拒绝不应删宏凑数。

## 历史安全检查点：BATCH281–283

- [BATCH281–283](BATCH281-283.md)：indexed refit 补足 UInt32
  indexOffset8、Float4 vertexOffset16 和两者加 descriptor 分配的组合。
  三份原生/捕获/API/CLI、150 例畸形输入、32 份跨族哨兵、3×10
  生命周期通过。库/app `2c2bc09e0e18…`，GUI 待验 **245 份**；
  bridge/chunk 原始宏匹配 **59/17**。无新 panic；累计压力回归和 GUI
  未执行。

## 历史安全检查点：BATCH272–280

- [BATCH272–280](BATCH272-280.md)：indexed triangle AS refit 扩展到
  Float4、noDuplicate、scratch/index/vertex 偏移、descriptor+vertexOffset、
  表槽1及 opaque 切换，再覆盖七参数组合。九份原生/捕获/API/CLI、
  450 例畸形输入、28 份哨兵、9×10 生命周期通过。库/app
  `2c2bc09e0e18…`，GUI 待验 **242 份**；bridge/chunk 原始宏匹配
  **59/17**。06:26 panic
  归因未明，本批短测无新 panic；累计压力回归和 GUI 未执行。

## 历史安全检查点：BATCH266–271

- [BATCH266–271](BATCH266-271.md)：indexed triangle AS refit 首批闭环，
  含 UInt16/UInt32、换 index buffer、独立目标、压缩复制和 descriptor
  分配。六份原生/捕获/API/CLI、300 例畸形输入、19 份哨兵、6×10
  生命周期通过。库/app `cf971d35cd45…`，GUI 待验 **233 份**；
  bridge/chunk 原始宏匹配 **59/17**。未测的 indexed refit 参数组合
  仍明确拒绝；06:26 panic 归因未明，累计压力回归和 GUI 未执行。

## 历史安全检查点：BATCH261–265

- [BATCH261–265](BATCH261-265.md)：修复 box/triangle refit 切换几何
  buffer 被错误拒绝，覆盖默认、格式化、noDuplicate 和独立目标。五份
  原生/捕获/API/CLI、155 例畸形输入、11 份哨兵、5×10 生命周期通过。
  库/app `c94ea0b15648…`，GUI 待验 **227 份**；bridge/chunk 原始
  宏匹配仍为 **57/17**。06:26 panic 与此前全量测试有时间关联但未能归因，
  本批短测未复现；累计压力回归和 GUI 未执行。

## 历史安全检查点：BATCH259–260

- [BATCH259–260](BATCH259-260.md)：refitted box AS 复制后射线以及复制
  目标再次 refit，GPU 射线 `1/1/0→0/1/0→0/1/0→1/1/0`。
  T259/T260 原生、捕获、API/CLI 通过，40/48 个畸形输入拒绝；九份
  定向哨兵和 T256–260 五份×10 生命周期通过。库/app `cbfb8ec0daff…`，
  GUI 待验 **222 份**。06:26 IOGPU panic 发生在此前全量测试期间，
  有时间相关性但未能归因；后续短测试未复现；
  累计压力回归和 GUI 仍未执行。

## 历史安全检查点：BATCH256–258

- [BATCH256–258](BATCH256-258.md)：bounding-box AS refit 已接通普通原位、
  组合偏移/表槽/禁止重复调用和独立目标。三份原生/捕获/API/CLI 的 GPU ray
  `1/1/0→0/1/0`，各 40 个畸形输入拒绝；5 份定向哨兵和 3×10 生命周期
  通过。库/app `cbfb8ec0daff…`，GUI 待验 **220 份**。bridge 原始调用
  **57 处**（新增长度为 2 的 box primitiveDataBuffer 显式拒绝）。06:26 IOGPU panic
  仍无法归因；后续短测试未出现新 panic，累计压力回归与 GUI 未执行。

## 历史安全检查点：BATCH251–255

- [BATCH251–255](BATCH251-255.md)：Float3/Float4 stride16 的 refittable
  triangle build/refit 已保真，覆盖原位、descriptor 分配加独立目标、
  禁止重复交点调用与 scratchOffset256，以及 refit 后压缩复制。五份
  原生/捕获/API/CLI ray `1→0`，各 34 个畸形输入安全拒绝；20 份
  哨兵和 5×10 生命周期打开通过。库/app `2668361301ba…`，GUI
  待验 **217 份**；原始宏匹配 **55 bridge 调用（另有定义 1）/
  17 未处理 chunk（含定义 1）**。仅有此前 06:26 一份 panic，因果
  未证实；全量压力回归与 GUI 未执行。

## 历史安全检查点：BATCH248–250

- [BATCH248–250](BATCH248-250.md)：indexed Float4/Float3 stride16
  接通 UInt16/UInt32、三偏移、descriptor 分配、禁止重复交点调用。
  三份原生/捕获/API/CLI ray `1,0,1`，各 25 个畸形输入拒绝；
  18 份跨族哨兵和 T244–250 的 7×10 生命周期打开通过。
  库/app `e9d06ac87d24…`，GUI 待验 **212 份**；原始宏匹配仍为
  **55 bridge 调用（另有定义 1）/17 未处理 chunk（含定义 1）**。
  只有此前 06:26 一份内核 panic，根因未证实；全量压力回归与 GUI
  未执行。

## 历史安全检查点：BATCH244–247

- [BATCH244–247](BATCH244-247.md)：静态非 indexed triangle 的
  Float3/stride16、Float4/stride16、vertexOffset16/scratchOffset256 及
  禁止重复交点调用已按新末尾 chunk 保真。四份原生/捕获/API/CLI ray
  `1,0,1`，各 20 个畸形输入安全拒绝，13 份跨族哨兵和 4×10 生命周期
  打开通过。库/app `21a036a49317…`，GUI 待验 **209 份**；原始宏匹配
  **55 bridge 调用（另有定义 1）/17 未处理 chunk（含定义 1）**。
  只有此前 06:26 一份内核 panic，因果未证实；全量压力回归和 GUI
  未执行。indexed/refit 的新顶点格式仍未接通。

## 历史安全检查点：BATCH240–243

- [BATCH240–243](BATCH240-243.md)：refittable triangle 的
  `allowDuplicateIntersectionFunctionInvocation=false` 已保真；原位、
  descriptor 分配加独立目标、scratchOffset256、refit 后压缩复制四份
  原生/捕获/API/CLI 均通过，GPU ray `1→0`。各 21 个畸形输入拒绝、
  最终库 21 份定向哨兵和 4×10 生命周期打开通过。错误 Encoder
  负例暴露并修复一次进程段错误；生命周期误报也已修正。库/app
  `61fe88a3591a…`，GUI 待验 **205 份**；**55 bridge 调用（另有定义 1）/
  17 未处理 chunk 宏匹配（含定义 1）**。未见新内核 panic，完整压力
  回归与 GUI 未执行。

## 历史安全检查点：BATCH237–239

- [BATCH237–239](BATCH237-239.md)：triangle 几何
  `allowDuplicateIntersectionFunctionInvocation=false` 已贯通分配、
  捕获与回放；非 indexed、UInt16 indexed、UInt32 indexed+三偏移/
  descriptor 三份均通过原生、API、CLI，各 16 个畸形输入拒绝、
  22 份跨族哨兵和 6×10 生命周期打开。库/app `103f584d33e5…`，
  GUI 待验 **201 份**；**55 bridge 调用（另有定义 1）/17 未处理
  chunk 宏匹配（含定义 1）**。06:26 panic 根因未知，未见新 panic；
  完整压力回归和 GUI 未执行。

## 历史安全检查点：BATCH234–236

- [BATCH234–236](BATCH234-236.md)：bounding-box 几何的
  `allowDuplicateIntersectionFunctionInvocation=false` 已贯通 descriptor 分配、
  build 捕获和回放；基本、box/scratch/table 偏移、opaque 三种组合的 GPU
  命中及交点函数计数均一致。三份各 22 个畸形输入拒绝、15 份旧捕获哨兵、
  3×10 生命周期打开通过。库/app 内嵌库 `0a4547ec4347…`，GUI 待验
  **198 份**；原始标记仍为 **55 bridge 调用（另有定义 1）/17 未处理
  chunk 宏匹配（含定义 1）**。06:26 panic 仍无法归因，未见新 panic；
  完整压力回归和 GUI 未执行。

## 历史安全检查点：BATCH231–233

- [BATCH231–233](BATCH231-233.md)：非 indexed、indexed 和 UInt32+
  三偏移/descriptor 分配的 triangle 表偏移1已保真；错槽原生对照
  render ray `0`，正例原生与回放为 `1`。三份各 17 个畸形输入拒绝、
  最终库 26 份跨族哨兵、8×10 生命周期打开通过。库/app 内嵌库
  `08d484d3b59f…`，GUI 待验 **195 份**；**55 bridge 调用（另有定义 1）/
  17 未处理 chunk 宏匹配（含定义 1）**。06:26 panic 根因未知，
  完整 GPU 压力回归与 GUI 未执行。

## 历史安全检查点：BATCH227–230

- [BATCH227–230](BATCH227-230.md)：indexed triangle 的顶点/索引/scratch
  三偏移、UInt16/UInt32、直接/descriptor 分配与单偏移分支已保真；
  四份原生/捕获/API/CLI 均为 compute ray `1/0/1`、render ray `1`。
  各 18 个畸形输入拒绝、30 份哨兵、9×10 生命周期打开通过。
  库/app 内嵌库 `96ad1c3c78a3…`，GUI 待验 **192 份**；
  **55 bridge 调用（另有定义 1）/17 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归与 GUI 未执行。

## 历史安全检查点：BATCH226

- [BATCH226](BATCH226.md)：UInt32 indexed triangle 的 indexOffset8 与
  descriptor 分配组合通过，原生/回放 ray `1/0/1`、render ray `1`；
  15 个畸形输入安全拒绝、19 份哨兵、7×10 生命周期打开通过。
  库/app 内嵌库 `99941fffb20b…`，GUI 待验 **188 份**；
  **55 bridge 调用（另有定义 1）/17 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归与 GUI 未执行。

## 历史安全检查点：BATCH225

- [BATCH225](BATCH225.md)：indexed triangle 的非零 indexBufferOffset
  已按独立末尾 chunk 保真；无效索引前缀+offset8 的 GPU ray `1/0/1`、
  render ray `1` 在原生和回放一致。15 个畸形输入拒绝、24 份跨族哨兵、
  6×10 生命周期打开通过。库/app 内嵌库 `99941fffb20b…`，GUI 待验
  **187 份**；**55 bridge 调用（另有定义 1）/17 未处理 chunk 宏匹配
  （含定义 1）**。06:26 panic 根因仍未知，完整 GPU 压力回归与 GUI 未执行。

## 历史安全检查点：BATCH223–224

- [BATCH223–224](BATCH223-224.md)：opaque box 几何及与 table/box/scratch
  偏移组合；两份原生/捕获/API/CLI 射线均 `0/0/0`，各 19 个畸形输入安全
  拒绝；21 份跨族哨兵、8×10 生命周期打开通过。库/app 内嵌库
  `1e37a4f471f9…`，GUI 待验 **186 份**；**55 bridge 调用（另有定义 1）/
  17 未处理 chunk 宏匹配（含定义 1）**。06:26 panic 根因仍未确定，
  完整 GPU 压力回归与 GUI/Computer Use 未执行。

## 历史安全检查点：BATCH221–222

- [BATCH221–222](BATCH221-222.md)：box 几何 intersection table offset1，
  再与 boxOffset48/scratchOffset256 组合，新增独立 chunk 保持旧捕获编号。
  两份原生/捕获/API/CLI、20/20 畸形输入、19 份跨族哨兵与 6×10
  生命周期打开通过；GPU 射线均 `1/1/0`。库/app SHA `44cc7a5efb3f…`，
  GUI 待验 **184 份**；**55 bridge 调用（另有定义 1）/17 未处理 chunk
  宏匹配（含定义 1）**。06:26 panic 根因未知，完整 GPU 压力回归暂停；
  无 GUI/Computer Use。

## 历史安全检查点：BATCH218–220

- [BATCH218–220](BATCH218-220.md)：compute 交点函数表创建与单表/range
  绑定已接通；stride32 双盒、boxOffset48/scratchOffset256 的 GPU 射线
  结果 `1/1/0` 由原生与 API 回放双重证实。三份捕获、18/18/20 畸形输入、
  17 份跨族哨兵与 4×10 生命周期打开通过。新 chunk 追加到枚举末尾，旧
  T137 捕获兼容已复验。库/app SHA `d26ccf06016a…`，GUI 待验 **182 份**；
  **55 bridge 调用（另有定义 1）/17 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归暂停；无 GUI/Computer Use。

## 历史安全检查点：BATCH216–217

- [BATCH216–217](BATCH216-217.md)：URL 动态库及与普通动态库的混合
  依赖已按捕获内字节重建，原始文件删除后回放仍通过。两份原生/捕获/
  API/CLI、11/13 个畸形输入、27 份跨族哨兵及 3 份×10 生命周期打开
  通过；私有临时目录无残留。仅支持可等长迁移 install name 的二进制，
  不可迁移时明确拒绝。库/app SHA `8d98d33c39c0…`，GUI 待验
  **179 份**；当前 **55 bridge 调用（另有定义 1）/17 未处理 chunk
  宏匹配（含定义 1）**。06:26 panic 根因未知，完整 GPU 压力回归
  暂停；无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH213–215

- [BATCH213–215](BATCH213-215.md)：box AS 非默认 stride32 的单盒、双盒、
  双盒加 box/scratch 偏移已接通。三份原生/捕获/API/CLI、28/29/29 个
  畸形输入、24 份跨族哨兵及 8 份×10 生命周期打开通过；第二盒内部几何
  的射线命中仍待程序化交点验证。库/app SHA `cffc4b0d117…`，GUI
  待验 **177 份**；当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。06:26 panic 根因未知，完整 GPU 压力回归暂停；
  无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH209–212

- [BATCH209–212](BATCH209-212.md)：box buffer offset48、scratch offset256、
  双偏移与偏移描述符分配已接通。四份原生/捕获/API/CLI、
  24/24/23/24 个畸形输入、21 份跨族哨兵及 5 份×10 生命周期打开通过。
  库/app SHA `920be51b84d8…`，GUI 待验 **174 份**；当前
  **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归暂停；无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH204–208

- [BATCH204–208](BATCH204-208.md)：refit scratch 偏移、独立目标、
  独立目标压缩后 ray 及紧凑 refit scratch 已接通。五份原生/捕获/
  API/CLI、33/33/33/52/34 个畸形输入、47 份跨族哨兵及 18 份×10
  生命周期打开通过。库/app SHA `56c3811f7405…`，GUI 待验 **170 份**；
  当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归暂停；无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH202–203

- [BATCH202–203](BATCH202-203.md)：refit 单/双三角形 descriptor 分配后
  build→refit→压缩→ray 通过。原生/捕获/API/CLI、45/47 个畸形输入、
  32 份跨族哨兵及 13 份×10 生命周期打开通过；库/app SHA
  `a7c43e2e37fa…`，GUI 待验 **165 份**。当前 **56 bridge 调用（另有
  定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。06:26 panic 根因未知，
  完整 GPU 压力回归暂停；无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH200–201

- [BATCH200–201](BATCH200-201.md)：单/双三角形 refit 后压缩复制的
  AS 目标继续用于 GPU ray。原生/捕获/API/CLI、45/47 个畸形输入、
  29 份跨族哨兵和 11 份×10 生命周期打开通过；库/app SHA
  `45b447817a59…`，GUI 待验 **163 份**。当前
  **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 panic 根因未知，完整 GPU 压力回归暂停；无 qrenderdoc/Computer Use。

## 历史安全检查点：BATCH196–199

- [BATCH196–199](BATCH196-199.md)：TLAS 普通复制及单/双实例 TLAS 压缩复制
  后 ray 通过。原生/捕获/API/CLI、26/29/31/32 个畸形输入、36 份跨族
  哨兵和 8 份×10 生命周期打开通过；库/app SHA `a08d2768c106…`，
  GUI 待验 **161 份**。当前 **56 bridge 调用（另有定义 1）/18 未处理
  chunk 宏匹配（含定义 1）**；06:26 panic 根因未知，完整 GPU 压力回归
  暂停，无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH194–195

- [BATCH194–195](BATCH194-195.md)：indexed 与多三角形的压缩 BLAS
  目标继续用于 TLAS GPU ray。原生/捕获/API/CLI、32/30 个畸形输入、
  62 份跨族哨兵及 14 份×10 生命周期打开通过。库/app 内嵌 SHA
  `2e33d8225d00…`，GUI 待验 **157 份**。
- box 压缩目标的后续 ray 需程序化交点路径；TLAS/refit 压缩来源
  未开放。当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。06:26 IOGPUFamily panic 根因未知，完整 GPU
  压力回归暂停；无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH192–193

- [BATCH192–193](BATCH192-193.md)：普通复制与跨 CB 压缩复制的 BLAS
  目标继续用于 TLAS GPU ray；同时修复全部 11 条 AS chunk 回放路径的活动
  encoder 身份校验。原生/捕获/API/CLI、25/29 个畸形输入、60 份跨族
  哨兵及 13 份×10 生命周期打开通过。库/app 内嵌 SHA
  `2e33d8225d00…`，GUI 待验 **155 份**。
- box/多三角形/indexed 压缩目标尚未逐类 ray 验证；TLAS/refit 压缩
  来源未开放。当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。06:26 IOGPUFamily panic 根因未知，完整 GPU
  压力回归暂停；无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH189–191

- [BATCH189–191](BATCH189-191.md)：box、多非 indexed 三角形、indexed
  三角形 AS 的压缩复制已能按 GPU 读回容量执行。原生/捕获/API/CLI、
  32/32/33 个畸形输入、36 份跨族哨兵及 10 份×10 生命周期打开通过。
  库/app 内嵌 SHA `f8fa07f7fd24…`，GUI 待验 **153 份**。
- 压缩目标的 build 元数据尚未传播与 ray 绑定验证；TLAS/refit 来源未开放。
  当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知，完整 GPU 压力回归暂停；无
  qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH186–188

- [BATCH186–188](BATCH186-188.md)：实例 descriptor 直接分配 TLAS，
  单实例、同BLAS双实例、异BLAS双实例均闭环。原生/捕获/API/CLI、
  20/22/23 个畸形输入、32 份跨族哨兵及 12 份×10 生命周期打开通过。
  后续追加 instance descriptor 的 heap size/align 查询，原生/注入通过，
  当前库 20 份跨族哨兵通过。库/app 内嵌 SHA `b9b786586f9d…`，
  GUI 待验 **150 份**。
- 当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**；
  现有 chunk 子集扩大，计数不变。06:26 IOGPUFamily panic 根因未知，
  完整 GPU 压力回归暂停；无 qrenderdoc/Computer Use，
  `supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH183–185

- [BATCH183–185](BATCH183-185.md)：计算/片元/顶点/tile 加速结构空绑定
  清空接通。T183–T185 的原生/捕获/API/CLI、每份 33 个畸形输入、25 份
  跨族哨兵及 9 份×10 生命周期打开通过。库/app 内嵌 SHA
  `54ed11b8cce6…`，GUI 待验 **147 份**。
- 当前 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**；
  新能力复用已有 chunk，计数不变。06:26 IOGPUFamily panic 根因未知，
  完整 GPU 压力回归暂停。无 qrenderdoc/Computer Use，
  `supportsRaytracing` 仍 false。

## 历史安全检查点：BATCH182

- [BATCH182](BATCH182.md)：多 indexed triangle descriptor 分配按索引
  buffer 容量有界开放，T182 双三角形/六 UInt16 索引的原生/捕获/API/CLI、
  20 个畸形输入、39 份跨族哨兵及 15 份×10 生命周期打开通过。
  库/app 内嵌 SHA `1a8c4816af5a…`，GUI 待验 **144 份**。
- 当前仍 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH181

- [BATCH181](BATCH181.md)：非 indexed 静态三角形 descriptor 分配按 count/
  buffer 长度有界开放，T181 双三角形的原生/捕获/API/CLI、18 个畸形输入、
  38 份跨族哨兵及 14 份×10 生命周期打开通过。库/app 内嵌 SHA
  `44a338d15489…`，GUI 待验 **143 份**。
- 当前仍 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH180

- [BATCH180](BATCH180.md)：box descriptor 分配改为有界 count/buffer 长度
  校验，三 box 正向闭环通过。原生/捕获/API/CLI、18 个畸形输入、37 份
  跨族哨兵及 13 份×10 生命周期打开通过。库/app 内嵌 SHA
  `ba68ff520274…`，GUI 待验 **142 份**。
- 当前仍 **56 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH179

- [BATCH179](BATCH179.md)：双 bounding-box descriptor 分配 AS 已接通，
  `boxCount=2` 的原生/捕获/API/CLI、18 个畸形输入、36 份跨族哨兵及
  12 份×10 生命周期打开通过。库/app 内嵌 SHA `cb8f903f05ff…`，GUI
  待验 **141 份**。当前仍 **56 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。
- 06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH178

- [BATCH178](BATCH178.md)：单 box 几何 descriptor 分配 AS 现可捕获/回放，
  GPU compacted size1280。原生/捕获/API/CLI、18 个畸形输入、35 份跨族
  哨兵及 11 份×10 生命周期打开通过。库/app 内嵌 SHA `9e5c56446ded…`，
  GUI 待验 **140 份**。
- 当前 **56 bridge 宏调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**；
  增加的 1 处为 box 描述符显式拒绝检查，并非功能回退。06:26 IOGPUFamily
  panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/Computer Use，
  `supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH176–177

- [BATCH176–177](BATCH176-177.md)：同一 AS build 中 vertexOffset16 与
  scratchOffset256 组合已由 size/descriptor 两种分配验证。原生/捕获/API/CLI、
  31 个畸形输入、33 份跨族哨兵及 13 份×10 生命周期打开通过。库/app
  内嵌 SHA 仍 `1bb6f06a68b5…`，GUI 待验 **139 份**。
- 当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH175

- [BATCH175](BATCH175.md)：非 indexed 静态三角形 AS 的 scratch buffer
  offset256 已接通；原生/捕获/API/CLI、13 个畸形输入、31 份跨族哨兵及
  11 份×10 生命周期打开通过。库/app 内嵌 SHA `1bb6f06a68b5…`，GUI
  待验 **137 份**。当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。
- 06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH174

- [BATCH174](BATCH174.md)：非 indexed 单三角形 vertexOffset16 的 descriptor
  AS 分配已接通，原生/捕获/API/CLI、5+12 个畸形输入、30 份跨族哨兵及
  10 份×10 生命周期打开通过。库/app 内嵌 SHA `8a9f713e6364…`，GUI
  待验 **136 份**。当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk
  宏匹配（含定义 1）**。
- 06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH173

- [BATCH173](BATCH173.md)：非 indexed 静态三角形 AS 顶点 buffer 的 16 字节
  偏移现可捕获/回放，前缀干扰数据验证实际 GPU 读取位置。原生/捕获/API/CLI、
  12 个畸形输入、29 份跨族哨兵及 9 份×10 生命周期打开通过。库/app
  内嵌 SHA `b5c8dff22b37…`，GUI 待验 **135 份**。
- 当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH171–172

- [BATCH171–172](BATCH171-172.md)：UInt32 indexed opaque descriptor 分配与
  非 opaque 交点拒绝形成双侧对照，GPU 值 1/0、绿色/红色。原生/捕获/API/CLI、
  29 个畸形输入、28 份跨族哨兵及 9 份×10 生命周期打开通过。驱动库/app
  内嵌 SHA 仍 `73a801b87e7b…`，GUI 待验 **134 份**。
- 当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH170

- [BATCH170](BATCH170.md)：UInt16 indexed 非 opaque 几何经自定义交点函数拒绝，
  原生/捕获/API/CLI 为 Shared0、中央红色；与 T167/T169 indexed opaque 的
  Shared1、绿色构成同族对照。12 个畸形输入、26 份跨族哨兵及 7 份×10
  生命周期打开通过。库/app 内嵌 SHA 仍 `73a801b87e7b…`，GUI 待验 **132 份**。
- 当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**。
  06:26 IOGPUFamily panic 根因未知；完整 GPU 压力回归暂停，无 qrenderdoc/
  Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH169

- [BATCH169](BATCH169.md)：indexed opaque 单三角形 descriptor 分配已接通，
  与独立 build chunk 组成 GPU 闭环。原生/捕获/API/CLI、5+12 个畸形输入、
  25 份跨族哨兵及 6 份×10 生命周期打开通过。当前 **55 bridge 调用
  （另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**；库/app 内嵌 SHA
  `73a801b87e7b…`，GUI 待验 **131 份**。
- 06:26 IOGPUFamily panic 的 panicked task 为 `kernel_task`，报告含
  renderdoccmd 进程快照，但具体诱因未证实；
  完整 GPU 压力回归继续暂停。无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH168

- [BATCH168](BATCH168.md)：UInt32 indexed opaque 三角形补齐 GPU 正向闭环；
  UInt16/UInt32 现在各有 capture/API/CLI 与 12 个畸形输入测试。24 份跨族哨兵
  API/CLI、5 份×10 生命周期打开通过。驱动库/app 内嵌 SHA 仍
  `9a7f700ace5c…`，GUI 待验 **130 份**；计数仍 **55 bridge 调用（另有定义 1）/
  18 未处理 chunk 宏匹配（含定义 1）**。
- 06:26 IOGPUFamily panic 根因仍未知；完整 GPU 压力回归继续暂停。
  无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH167

- [BATCH167](BATCH167.md)：indexed opaque 三角形 AS build 现可捕获/回放，
  新增独立 chunk 并保留旧 indexed 默认语义。原生与捕获 GPU 命中、
  12 个畸形输入、23 份跨族哨兵 API/CLI、4 份×10 生命周期打开通过。
  当前为 **55 处 bridge 调用（另有宏定义 1）/18 处未处理 chunk 宏匹配
  （含定义 1）**；库/app 内嵌 SHA 均 `9a7f700ace5c…`，GUI 待验 **129 份**。
- 06:26 IOGPUFamily panic 未复现，根因仍未知；完整 GPU 压力回归仍暂停。
  无 qrenderdoc/Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH166

- [BATCH166](BATCH166.md)：显式 opaque 三角形 AS descriptor 分配子集已接通，
  原生、捕获、API/CLI GPU 命中及 5+11 个畸形输入通过；22 份跨族哨兵及
  4 份×10 生命周期打开通过。当前为 **55 处 bridge 调用（另有宏定义 1）/
  18 处未处理 chunk 宏匹配（含定义 1）**；此前记录的 bridge 54 少算 1 处，
  并非功能回退。库/app 内嵌 SHA 均 `a0d4076aeb32…`，GUI 待验 **128 份**。
- 06:26 IOGPUFamily panic 根因仍未知，完整 GPU 压力回归仍暂停。
  无 GUI/Computer Use，`supportsRaytracing` 仍 false。

## 当前安全检查点：BATCH165

- [BATCH165](BATCH165.md)：AS三角形几何显式`opaque=true`现可捕获/回放，
  新增独立chunk并保留旧默认/nonopaque/refit语义。与T148自定义拒绝函数
  对照，T165 GPU结果由未命中0转为命中1；原生、注入捕获、API/CLI、11个
  畸形build输入、20份跨族哨兵及5份×10生命周期打开通过，resident growth
  1,556,480 bytes。原始计数仍**54 bridge / 18未处理chunk宏匹配**（含定义1），
  因为本批扩展旧bridge子集、新增chunk，不伪报标记清零。
  库/app内嵌SHA均`19ae3d8f9332…`，GUI待验**127份**。
- 06:26 IOGPUFamily panic没有复现，根因仍未知；完整GPU压力回归仍暂停。
  无GUI/Computer Use，`supportsRaytracing`仍false。

## 当前安全检查点：BATCH164

- [BATCH164](BATCH164.md)：带默认descriptor的AS encoder创建bridge接通，新增
  独立chunk；两次创建驱动BLAS/TLAS及compute射线`0,1`，原生/捕获/API/CLI
  通过，10个畸形descriptor/身份输入干净拒绝。19份跨族哨兵API/CLI、4份×10
  生命周期打开通过，resident growth 1,064,960 bytes。原始bridge **55→54**；
  原始未处理chunk宏匹配仍**18**（含宏定义1），本入口不是既有旧chunk。
  该批库/app内嵌SHA均`0ace9b0c4581…`，当时GUI待验**126份**。
- 06:26 IOGPUFamily panic根因仍未证实，完整GPU压力回归仍暂停；无GUI/Computer Use。

## 当前安全检查点：BATCH163

- [BATCH163](BATCH163.md)：IFT嵌套visible table的显式render `useResource`路径
  已在原生与回放中验证。负例发现旧residency回放缺encoder身份检查，错误ID会
  触发进程异常；render四种与compute两种residency入口已补类型/当前encoder
  校验，T163的8+6个负例干净拒绝。17份跨族哨兵API/CLI、3份×10生命周期
  打开通过，resident growth 622,592 bytes。标记仍**55 bridge / 18旧chunk**；
  该批库/app内嵌SHA均`917ed5804329…`，当时GUI待验**125份**。
- 没有再发生kernel panic，但其原因仍不能由目前证据确定。06:26后的完整GPU
  压力回归仍未启动；此处是定向安全检查点，不是全量验收。无GUI/Computer Use。

## 当前安全检查点：BATCH161–162

- [BATCH161–162](BATCH161-162.md)：IFT嵌套visible table单槽/range两个bridge已接通，
  新增两个chunk。原生与捕获回放中，visible函数读取IFT buffer参数的0/1值，
  GPU结果分别为未命中/命中；T161/T162 API/CLI及零值对照通过，6/10个畸形
  输入干净拒绝。13份相关capture定向API/CLI、7份×10生命周期打开通过，
  resident growth 2,605,056 bytes。原始bridge **57→55**，旧chunk **18**；
  该批库/app内嵌SHA均`1ad5d34e7b80…`，当时GUI待验**124份**。
- 没有重跑06:26 panic后的完整GPU压力回归，也没有GUI/Computer Use。
  当时IFT嵌套表的显式`useResource`路径尚未接通，本样本原生无需它；不能把本批
  视作完整ray tracing或任意资源residency支持。`supportsRaytracing`仍false。

## 当前安全检查点：BATCH159–160

- [BATCH159–160](BATCH159-160.md)：接通intersection function table的buffer单槽/数组
  两个bridge，新增两个chunk。GPU交点函数读取buffer，值1命中、值0拒绝；原生
  Metal Validation、注入捕获、T159/T160 API/CLI、零值反向对照均通过。T159/T160
  分别7/11个畸形绑定干净拒绝，相关11份capture定向API/CLI通过；5份capture×10
  生命周期打开通过，resident growth 1,064,960 bytes。原始bridge **59→57**，
  旧chunk **18**；该批库/app内嵌SHA均`d9498312a666…`，当时GUI待验**122份**。
- 旧T148–T153原生probe的`reject_triangle`缺少`instancing`签名，旧GPU值0不能
  严格证明交点函数被调用。已修正并重录六份capture，原生/注入捕获/定向API/CLI及
  T148/T153共42个表负例通过；此前的全量通过数字只适用于旧capture，不可移作新
  capture全量证据。T159实测带`instancing`函数可写入参数buffer，并用只读0/1
  对照证明真实调用。新T148–T153的人工项仍是红色/Shared0，但SHA已更新。
- 06:26 IOGPUFamily panic的根因仍未被证明。之后无第二次panic，但**未运行完整
  GPU压力回归**；仍只做分组/定向测试，无GUI/Computer Use。`supportsRaytracing`
  仍false，T61 ICB GPU执行范围问题保留在PLAN。

## 当前安全检查点：BATCH156–157 与 06:26 GPU kernel panic

- [BATCH158 T36兼容回归](BATCH158-T36-VALIDATION.md)：分组验证暴露T36延迟store
  action被回放提前定值、旧`textureBarrier`在当前设备Validation下被拒绝两处问题。
  已按实际setter时序修复；仅对pass内尚无GPU工作的旧barrier安全略过，之后
  明确拒绝。T36 API/CLI、begin→end seek、15个负例与26份跨族哨兵均通过；
  T35/T36/T43/T148/T156/T157共60次生命周期打开通过，resident growth 901,120 bytes。
  同一次会话另分组跑过T01–T147及T148–T153/T156–T157的定向API/CLI，
  但不是全部使用最新库，不可计为新版本全量通过；无第二次kernel panic。

- [BATCH156–157](BATCH156-157.md)：接通fragment intersection table的
  `setOpaqueTriangleIntersectionFunction`单槽与range两个bridge入口、新增两个独立chunk。
  原生空表GPU输出0，设置后T156/T157均输出1且中央绿色；Metal Validation、注入捕获、
  T156/T157/T153定向API/CLI、T156/T157分别16/18个表负例及各29个AS负例通过。
  原始bridge标记61→59，旧chunk仍18；`supportsRaytracing`仍false。
- 2026-09-28 06:26:38发生macOS kernel panic，Apple `IOGPUFamily`断言
  `IOGPUResource::free called for resource still owned by an IOGPUDevice`，panic快照有
  `renderdoccmd`。与全量回归时间相关，但无法由当前日志确定具体capture或把根因
  归于T156/T157；重启前日志在/tmp丢失。重启后全量第二次运行到T71时已主动停止，
  **不能声称本批全量通过**。在隔离前不要自动重启完整GPU压力回归或连续大量
  malformed replay；先做轻量定向与静态检查。没有GUI/Computer Use。集中UI待验120份。
- 针对“加载畸形capture提前返回、挂起命令尚未完成即析构资源”的可疑路径，
  已在初次加载失败时以及设备析构前结束/提交/等待剩余replay命令，并在新表桥接对超大单槽index
  做转换前拒绝。修补后T148–T153/T156–T157共8份定向API/CLI通过，两个编码中
  失败的负例在Metal Validation下通过；**这不是panic根因已被证明修复**。
- 此历史检查点的库/app内嵌库SHA均`e291aae57542…`；T156/T157 capture SHA
  `96ef002c3c03…`/`61dc07c81b38…`。下一步隔离panic的最小触发器并审核资源
  生命周期；若属系统驱动限制，记录并保留不运行高风险全量的门槛。

## 当前恢复检查点：BATCH148–153 Render Intersection Function Table

- [BATCH148–153](BATCH148-153.md)：接通render PSO创建intersection table及
  fragment/vertex/tile的单槽与range绑定共7个旧chunk，另新增表`setFunction`和
  显式非不透明三角形build两个独立chunk。六份T148–T153的自定义交点函数真实改变
  GPU ray-query结果：普通compute维持`1,0,1`，render三阶段均为未命中`0`；
  fragment/vertex中央红色，tile为dispatch写Shared。原生Metal Validation、
  注入捕获、定向API/CLI、每份29个AS负例及20/22个表负例通过。发现并修复
  默认非不透明三角形与旧refit路径的分支优先级，修复后另用临时新录T141确认
  native/capture/API/CLI兼容，未覆盖正式T141 capture。
- 最终库全量**152 captures、3337 malformed、1520 lifecycle opens**通过，
  resident growth **12,845,056 bytes**，日志`/tmp/metal-batch153-final.log`。
  库与app内嵌库SHA均`3fbc11f8a1e5…`，六份capture SHA见BATCH与集中QA表。
  GUI/Computer Use未运行，待人工累计**118份**；未提交/推送。
- 原始标记**61 bridge / 18旧chunk**。bridge从60升至61是因为新表wrapper对8个
  尚不支持的资源参数/opaque/curve变体明确拒绝，且本批消除了7个原有入口标记；
  不将新增拒绝伪报为功能完成。`supportsRaytracing`仍为false。下一族优先原生验证
  intersection table资源参数或其余旧chunk；T61 GPU执行点ICB
  range事件树限制仍在PLAN单独列明，未被本批解决。
- 后续原生能力探针：Apple M2 Pro在Metal Validation下报告
  `AtBlitBoundary=0`、`AtDrawBoundary=0`；因此blit/render
  `sampleCountersInBuffer`两个旧chunk未冒险接通，也没有T154/T155成功capture。
  已在`Metal_Counter_Stage`留下可重复环境开关，后续需支持该能力的GPU验证；
  本机下一族改评估其他可执行的intersection table变体或剩余旧chunk。

## 当前恢复检查点：BATCH146–147 Vertex/Tile阶段TLAS消费

- [BATCH146–147](BATCH146-147.md)：接通`setVertexAccelerationStructure`与
  `setTileAccelerationStructure`两个旧chunk。T146 vertex ray-query命中第二BLAS，
  Shared结果1、中央绿色；T147 tile kernel ray-query命中，Shared结果1。原生
  Metal Validation、注入捕获、T144–T147定向API/CLI、T146/T147各29个畸形
  输入通过；整批回归**146 captures、3037 malformed、1460 lifecycle opens**通过，
  resident growth **5,718,016 bytes**，日志`/tmp/metal-batch147-full.log`。库/app
  内嵌库SHA `2140c924a230…`，T146/T147 capture SHA `abeaf2f81566…` /
  `10073db853eb…`。
  GUI/Computer Use未运行，待人工累计112份；原始标记60 bridge/25旧chunk，
  `supportsRaytracing`仍false。下一段优先评估intersection function table的
  可执行链或更多实例映射，不能仅靠单槽AS绑定宣称光追已完整支持。

## 当前恢复检查点：BATCH145 Fragment阶段TLAS消费

- [BATCH145](BATCH145.md)：在T144的不同BLAS双实例TLAS上，接通旧chunk
  `setFragmentAccelerationStructure`。fragment shader真实ray-query命中第二BLAS，
  Shared输出1、中央像素绿色；注入回放API验证TLAS身份、slot0、事件seek
  `1→7→1`及像素。原生Metal Validation、T142–T145定向API/CLI、29个畸形
  输入通过。整批回归**144 captures、2979 malformed、1440 lifecycle opens**通过，
  resident growth **7,913,472 bytes**，日志`/tmp/metal-batch145-full.log`。
  库/app内嵌库 SHA `a624ff54d9b8…`，capture `78a3802c5057…`；GUI/Computer Use未运行。
  原始标记60 bridge/27旧chunk；`supportsRaytracing`仍false。下一段优先验证
  vertex/tile阶段AS消费或更多实例映射，先做原生GPU探针。

## 当前恢复检查点：BATCH144 两个不同BLAS的Top-Level实例

- [BATCH144](BATCH144.md)：T144为两个实例分别构建不同三角形BLAS，实例索引0/1，
  各平移x=-2/+2；三条GPU射线从x=-2/+2/+3返回`1,0,1`，可区分第二实例误指向
  第一个BLAS。原生Metal Validation、注入截帧、T142–T144定向API/CLI、24个
  T144畸形输入均通过；整批回归**143 captures、2950 malformed、1430 lifecycle
  opens**通过，resident growth **6,225,920 bytes**，日志`/tmp/metal-batch144-full.log`。
  库/app内嵌库 SHA `42099b733d9f…`，capture `3afa4d1f0c9a…`；未做GUI/Computer Use，
  集中清单新增至109份。原始标记60 bridge/28旧chunk；`supportsRaytracing`仍false。
  下一段评估更多实例数量与child映射、交叉函数表等完整GPU可观察链。

## 当前恢复检查点：BATCH142–143 Top-Level AS实例与GPU射线

- [BATCH142](BATCH142.md)、[BATCH143](BATCH143.md)：新增默认布局单实例及同一BLAS的
  双实例TLAS尺寸查询、真实build与独立chunk。T142平移x=+2，两个GPU ray返回
  miss/hit=`0,1`；T143双实例平移x=-2/+2，三条ray返回hit/miss/hit=`1,0,1`。
  注入回放均有API Shared输出与事件seek验证。T135–T143共9份定向API/CLI、
  T142/T143各21/23个畸形实例输入通过。T142当时完整基线141 captures、2903
  malformed、1410 lifecycle opens通过；当前最终一体化全量**142 captures、2926
  malformed、1420 lifecycle opens**通过，resident growth **7,012,352 bytes**，
  日志`/tmp/metal-batch143-full.log`。库/app内嵌库`c028455d3a89…`，T142/T143
  capture SHA `54d563c23e9b…` / `0a41991b814b…`；`git diff --check`通过。
  T70/T133最新原生/捕获及离线预期拒绝复验通过。GUI/Computer Use未运行，累计
  待人工108份；未提交/推送。原始标记**60 bridge / 28旧chunk**，比T141的
  59/28多一个实例descriptor显式fallback，不代表功能退回。
  `supportsRaytracing`仍false；下一步扩展不同child AS、更多实例及间接/motion、
  intersection table等可执行链，不能把一/两个同BLAS实例当作通用TLAS支持。

## 历史检查点：BATCH141 GPU可观察的底层AS原位Refit

- [BATCH141](BATCH141.md)：单无索引Float3三角形的refittable build、独立command buffer
  内GPU blit更新顶点、原位refit及compute单槽AS绑定已接通。真实ray-query在Metal Validation
  下build后命中1、refit后未命中0；注入回放API验证Shared输出首→末为`1,9`→`1,0`，
  末→首→末seek一致。T135–T141共7份定向API/CLI、28个T141畸形chunk通过。
  最终一体化全量**140 captures、2882 malformed、1400 lifecycle opens**通过，resident
  growth **0 bytes**，日志`/tmp/metal-batch141-final.log`。库/app内嵌库
  `9f1860586102…`，T141 capture `0ca68274fcea…`，`git diff --check`通过。
  GUI/Computer Use未运行，累计待人工106份；不提交/推送。
  原始标记**59 bridge / 28旧chunk**：新增refit描述符安全守卫使bridge文本计数从51
  增至59，不代表已接通的三个入口退回；不能用原始标记数替代可执行功能验证。
  `supportsRaytracing`仍false；下一步评估instance/top-level、render阶段AS绑定或更广
  refit形态的完整GPU可观察链，不把当前单三角形子集当作完整光追。

## 历史检查点：BATCH140 GPU尺寸约束的压缩AS Copy

- [BATCH140](BATCH140.md)：独立command buffer的静态三角形build→Shared GPU尺寸
  写回→1280-byte目标分配→压缩copy→目标尺寸写回，原生/注入Metal Validation、
  T140与T135–T139定向API/CLI、31个T140畸形捕获（含目标过小及GPU尺寸不一致）
  均通过；新增错误encoder负例后为32个。最终全量**139 captures、2854 malformed、
  1390 lifecycle opens**通过，resident growth 5,586,944 bytes，日志
  `/tmp/metal-batch140-final2.log`；库/app `2458b8caf470…`，UI未运行，
  累计待人工105份。
  原始标记51 bridge/28旧chunk，新增显式fallback使文本计数+1。T140需前序
  单无索引三角形build与GPU尺寸写回来自同一前序command buffer，且GPU尺寸可读；
  其它时序仍拒绝。T70/T133原生/捕获及离线预期拒绝复验通过；未提交/推送。
  下一功能族先评估AS refit的真实GPU变化与可观察验证，再选实例AS或shader消费链；
  不能仅凭压缩尺寸不变就声称refit几何正确。

## 历史检查点：BATCH139 等容量AS Copy

- [BATCH139](BATCH139.md)：两个1536-byte AS经真实GPU build→copy→目标压缩尺寸
  写回，原生/注入Metal Validation、14份定向API/CLI及T135–T139合计95个畸形AS
  输入通过。T139目标size并未缩至1280，`copyAndCompact`仍未实现。
  最终全量**138 captures、2822 malformed、1380 lifecycle opens**通过，resident
  growth 7,159,808 bytes，日志`/tmp/metal-batch139-full.log`。最新库/app
  `48c3c41062f2…`；原始标记50 bridge/28旧chunk，新增的显式资源类型fallback
  导致文本计数+1，不代表功能退步。T70/T133原生/捕获及离线预期拒绝复验通过。
  GUI未运行，累计待人工104份；未提交/推送。

## 历史检查点：BATCH136–138 AS几何扩展与描述符分配

- [BATCH136–137](BATCH136-137.md)、[BATCH138](BATCH138.md)：单静态三角形的
  UInt16/UInt32 indexed build、单 bounding-box build，及单无索引三角形 descriptor
  形式创建 AS，均形成真实 GPU 捕获回放链。T135–T138 原生/注入 Metal Validation、
  13份受影响捕获定向 API/CLI、73个畸形 AS 输入通过；最终全量为**137 captures、
  2800 malformed、1370 lifecycle opens**，resident growth 7,389,184 bytes，日志
  `/tmp/metal-batch138-full.log`。当时库/app `be071fb3fda6…`，GUI未运行，
  待人工103份；`supportsRaytracing`仍为false。原始标记49 bridge/28旧chunk：
  新增的描述符校验分支含显式 fallback，原始文本计数不能当作能力回退。
  T70/T133原生/捕获成功、离线预期拒绝也在最终库上复验通过；未提交/推送。

## 历史检查点：BATCH135 AS build与尺寸写回

- [BATCH135](BATCH135.md)：单个静态无索引三角形 AS 的 `newAccelerationStructureWithSize:`、
  命令 encoder、build/压缩尺寸写回/end 已形成真实 GPU 捕获回放链。T135 原生/注入
  size1536、GPU compacted1280；API/CLI、前后 seek、18个畸形输入及10份定向回归通过。
  其它AS形态仍显式拒绝，`supportsRaytracing`维持false。原始标记 **40 bridge /
  28旧chunk**：新增 AS encoder 对多种未支持描述符的显式 fallback，使 bridge
  文本计数上升，不能解读为功能退步或完整支持。当时库/app均为
  `55f87e6705d0…`，T135 capture `dff173e98bae…`；全量 **134 captures、2745
  畸形、1340 lifecycle** 通过，resident growth 5,554,176 bytes，日志
  `/tmp/metal-batch135-final.log`。T70/T133原生/捕获成功、离线预期拒绝再次通过。
  UI未运行，累计待人工100份（T134无独立UI项）；未提交/推送。

## 历史检查点：BATCH134 AS尺寸查询与BATCH131–132 Heap安全子集

- [BATCH134](BATCH134.md)：在光追完整资源链之前，先接通静态三角形/box 的
  `accelerationStructureSizesWithDescriptor:` 与
  `heapAccelerationStructureSizeAndAlignWithDescriptor:` 纯查询。原生/注入三种描述符
  数值一致、Metal Validation、T134捕获及6份定向API/CLI通过。其它描述符仍保留
  bridge fallback；原始标记仍为 **35/29**，`supportsRaytracing`仍为false。
  集中回归与UI未运行；无独立UI功能验收。

- [BATCH131–132](BATCH131-132.md)：buffer/texture `makeAliasable` bridge与旧chunk
  接通，限 heap 资源标记后不再使用、无后续重叠分配；原生 Metal Validation、
  捕获、CLI/API、6个畸形目标拒绝及旧 heap 定向通过。原始剩余 **35 bridge /
  29旧chunk**，T70另计36/30。全量 **132 captures、2727畸形、1320
  lifecycle** 通过，resident growth 5,521,408 bytes，日志
  `/tmp/metal-batch132-full.log`。库/app `d4f73865b86c…`；GUI/Computer Use
  未运行，待人工99份；未提交/推送。T133原生同offset重叠复用和捕获成功，离线
  明确拒绝，不计成功回放或UI待验；真正alias生命周期与事件seek仍须架构处理。

## 历史检查点：BATCH119–130 Visible Function Table 完整阶段链

- [BATCH119–120](BATCH119-120.md)、[BATCH121–123](BATCH121-123.md)、
  [BATCH124–125](BATCH124-125.md)、[BATCH126–130](BATCH126-130.md)：
  render fragment/vertex/tile 和 compute 的直接 visible function 链接、handle、
  table、单槽/范围绑定，以及 compute argument buffer 内的函数表编码已通过真实
  GPU输出、Metal Validation、捕获、CLI/API和畸形输入检验。Tile 同步及异步
  descriptor 均保留链接函数，schema v9 兼容旧捕获。原始剩余 **37 bridge /
  31旧chunk**，T70另计38/32。全量 **130 captures、2721畸形、1300
  lifecycle** 通过，resident growth 8,093,696 bytes，日志
  `/tmp/metal-batch130-full.log`。库/app `cf2a2ccc430e…`；GUI/Computer Use
  未运行，待人工97份；未提交/推送。Intersection function table、acceleration
  structure 和重叠 alias 生命周期仍未接通。

## 历史检查点：BATCH118 一层嵌套ArgumentEncoder

- [BATCH118](BATCH118.md)：接通`newArgumentEncoderForBufferAtIndex:` bridge/新chunk，
  父子encoder反射、只读内层texture/sampler、buffer指针尺寸与对齐、资源关系及GPU
  四象限采样均经原生Metal Validation、捕获、CLI/API、18个畸形输入验证。原始剩余
  **45 bridge / 39旧chunk**，T70另计46/40。全量 **118 captures、2625畸形、
  1180 lifecycle** 通过，resident growth 6,668,288 bytes，日志
  `/tmp/metal-batch118-full.log`。T118 capture SHA `e3fc631efe28…`，库/app
  `84df606e5379…`；GUI/Computer Use未运行，待人工85份；未提交/推送。
  Pipeline State公开结构暂不显示内层成员树，更深嵌套/数组/可写成员仍未支持。

## 历史检查点：BATCH116–117 Placement Heap 非重叠资源

- [BATCH116–117](BATCH116-117.md)：接通placement heap显式offset buffer/texture
  两个bridge和新chunk，以设备size/align校验偏移、边界及不重叠范围。GPU数据、三阶段
  texture画面和seek经原生Metal Validation及CLI/API验证；25个新增畸形输入拒绝。
  原始剩余 **46 bridge / 39旧chunk**，T70另计47/40。全量 **117 captures、
  2607畸形、1170 lifecycle** 通过，resident growth 5,603,328 bytes，日志
  `/tmp/metal-batch117-full.log`。T116/T117 capture SHA `9ab0c4160699…`/
  `3051cc3039d7…`，库/app `8dab14641eb8…`；GUI/Computer Use未运行，
  待人工84份；未提交/推送。重叠alias/makeAliasable仍明确不支持。

## 历史检查点：BATCH104–115 Dynamic library 链接及异步预加载

- [BATCH106–109](BATCH106-109.md)、[BATCH110–111](BATCH110-111.md)、
  [BATCH112–115](BATCH112-115.md)：计算/渲染（fragment 与 vertex）pipeline 的
  同步和异步 dynamic-library 预加载，以及异步 source-library 编译选项，均经原生
  Metal Validation、捕获、CLI/API 回放和畸形输入拒绝验证。原始剩余
  **48 bridge / 39旧chunk**，T70另计49/40。全量 **115 captures、2582畸形、
  1150 lifecycle** 通过，resident growth 7,094,272 bytes，日志
  `/tmp/metal-batch115-full.log`。库/app `5043305f5061…`；GUI/Computer Use 未运行，
  待人工82份；未提交/推送。URL 动态库导入仍不支持。

## 历史检查点：BATCH104–105 Dynamic library 链接执行

- [BATCH104–105](BATCH104-105.md)：接通`newDynamicLibrary:` bridge和旧chunk，
  source-library v6保存dynamic选项与依赖，回放私有installName路径并物化动态库。
  T104一依赖GPU输出3，T105两依赖GPU输出6；原生、capture、CLI/API、
  19+14畸形输入及旧捕获均通过。原始剩余 **48 bridge / 39旧chunk**，
  T70另计49/40。全量**105 captures、2502畸形、1050 lifecycle**通过，
  resident growth 3,932,160 bytes，日志`/tmp/metal-batch105-full.log`。
  T104/T105 capture SHA `22147d810218…`/`7520798eaddd…`，库/app
  `7d2400de2ed2…`。URL导入仍缺口；GUI/Computer Use未运行，待人工72份；
  未提交/推送。

## 历史检查点：BATCH103 Counter 子范围和偏移

- [BATCH103](BATCH103.md)：8槽counter sample indices2–5、resolve range(2,4)
  到目标buffer offset16，原生/捕获/CLI/API和22负例通过。原始剩余仍
  **49 bridge / 40旧chunk**，T70另计50/41。103 captures、2469畸形、
  1030 lifecycle全量通过，resident growth 622592 bytes；T103 capture
  `a38e39c760c4…`，库/app `30a8ca932d9f…`。GUI/Computer Use未运行，待人工
  70份；未提交/推送。全量日志`/tmp/metal-batch103-full.log`。

## 历史检查点：BATCH102 Reflection BufferBinding Argument Encoder

- [BATCH102](BATCH102.md)：接通`newArgumentEncoderWithBufferBinding` bridge/新chunk1316，
  从reflection重建简单只读texture2d/sampler布局并核对长度/对齐；T102左右两packet
  原生、capture、CLI/API、13个畸形输入拒绝及旧捕获定向通过。原始剩余
  **49 bridge / 40旧chunk**，T70另计50/41。102 captures、2447畸形、
  1020 lifecycle全量通过，resident growth 2375680 bytes；T102 capture
  `b9df0c466069…`，库/app `30a8ca932d9f…`。GUI/Computer Use未运行，待人工
  69份；未提交/推送。全量日志`/tmp/metal-batch102-full.log`。

## 历史检查点：BATCH101 阶段边界 Counter Sampling

- [BATCH101](BATCH101.md)：接通counter sample buffer创建与blit resolve两个旧chunk，
  render pass四阶段索引及资源身份使用v5 schema。原生probe与夹具验证四个非零递增
  timestamp；捕获/CLI/API/22个畸形输入拒绝和旧v1–v4定向通过。原始剩余
  **50 bridge / 40旧chunk**，T70另计51/41。101 captures、2434畸形、
  1010 lifecycle全量通过，resident growth 3538944 bytes；T101 capture
  `394b6a8f960b…`，库/app `77b615b94015…`。GUI/Computer Use未运行，待人工
  68份；未提交/推送。全量日志`/tmp/metal-batch101-full.log`。

## 历史检查点：BATCH100 异步Mesh + 双层Rate Map

- [BATCH100](BATCH100.md)：异步mesh pipeline调用时快照保存非默认mesh grid
  上限2，双层Rate Map第二层真实光栅和回拷已在原生/捕获/CLI/API验证；10个
  异步mesh加26个rate-map畸形输入拒绝。原始剩余仍**51 bridge / 41旧chunk**，
  T70另计52/42。100 captures、2412畸形、1000 lifecycle全量通过，resident
  growth 1949696 bytes；T100 capture `6997b8195353…`，库/app
  `d349808b9692…`。GUI/Computer Use未运行，待人工67份；未提交/推送。
  日志`/tmp/metal-batch100-full.log`。

## 历史检查点：BATCH99 双层Rate Map第二层真实光栅

- [BATCH99](BATCH99.md)：mesh primitive的`render_target_array_index`将两个
  threadgroup分别绘制到slice0/1，后者半水平速率物理宽208；原生、捕获、CLI/API
  slice1像素与回拷验证通过。mesh-only pipeline非默认网格上限2纳入v4 schema，
  v1/v2/v3兼容定向通过；19个mesh+26个rate-map畸形输入拒绝。原始剩余仍
  **51 bridge / 41旧chunk**，T70另计52/42。99 captures、2376畸形、990
  lifecycle全量通过，resident growth 3014656 bytes；T99 capture
  `161abbbbaeb2…`，库/app `d349808b9692…`。GUI/Computer Use未运行，当时待人工
  66份；未提交/推送。日志`/tmp/metal-batch99-full.log`。

## 历史检查点：BATCH98 双层Rate Map绑定数组渲染目标

- [BATCH98](BATCH98.md)：真实双层map绑定2-slice array render target，mesh绘制
  slice0并blit到drawable；原生Metal Validation、捕获、CLI/API与26个畸形输入
  拒绝通过。新增回放守卫要求map层数与render pass有效array length一致。
  原始剩余仍为**51 bridge / 41旧chunk**，T70另计52/42；slice1的真实光栅
  输出尚未验证。库/app `d5ddec74b502…`，T98 capture `1e6e084fd2fe…`。
  GUI/Computer Use未运行，当时待人工65份；未提交/推送。全量回归见
  `/tmp/metal-batch98-full.log`。

## 历史检查点：BATCH95–97 Rasterization rate map

- [BATCH95–97](BATCH95-97.md)：接通map创建旧chunk1030和`copyParameterDataToBuffer`
  bridge/新chunk1315，pass map引用；T96真实半水平速率在本机使物理宽度400→208，
  T97支持双层descriptor及参数数据，capture schema v3仍可读取v1/v2。原始剩余
  **51 bridge / 41旧chunk**；T70另计52/42。T95/T96/T97定向验证和各17/17/22
  个畸形输入拒绝通过；全量97 captures、2303畸形、970 lifecycle通过，resident
  growth 1589248 bytes，日志`/tmp/metal-batch97-full.log`。库/app
  `d93f1c816b54…`。GUI/Computer Use未运行，当时待人工64份；未提交/推送。
  双层array目标渲染尚未验证。

## 历史检查点：BATCH93–94 Object 输入网格

- [BATCH93–94](BATCH93-94.md)：修复object shader输入网格错误受mesh输出网格
  上限约束；T93两个object threadgroup、T94线程网格64×1×1原生及回放
  验证通过，联合16个畸形输入拒绝。原始剩余 **52 bridge / 42旧chunk**，
  T70另计53/43。捕获SHA `0b4476799c01…`/`28f1ccb2592a…`，库/app
  `3612c349a613…`；94 captures、2247畸形、940 lifecycle全量通过
  （growth 1327104 bytes，`/tmp/metal-batch94-full.log`）。GUI/Computer Use
  未运行，61份待人工；未提交/推送。

## 历史检查点：BATCH91–92 多线程 Object stage

- [BATCH91–92](BATCH91-92.md)：修复 direct mesh 两种 draw 对 object threadgroup
  必须为单线程的错误限制；T91/T92 各用 4 线程 object shader，经真实 payload
  绘制三角形，联合定向及16个畸形输入拒绝通过。原始剩余 **52 bridge / 42旧chunk**，
  T70另计53/43。捕获SHA `3ea72dbf14f6…`/`e18b6eb53bb5…`，库/app
  `dd543ab096a7…`；92 captures、2231畸形、920 lifecycle全量通过
  （growth 1884160 bytes，`/tmp/metal-batch92-full.log`）。GUI/Computer Use
  未运行，59份待人工；未提交/推送。

## 历史检查点：BATCH88–90 Mesh 间接绘制

- [BATCH88–90](BATCH88-90.md)：接通 mesh 间接 draw（chunk1314），Shared
  偏移16、GPU写 Private 参数、Object/Mesh 三种真实绘制路径；各11个畸形输入
  拒绝。原始剩余 **52 bridge / 42旧chunk**，T70另计53/43。捕获SHA
  T88 `81a4e8f91479…`、T89 `67f3eb8a6005…`、T90 `8ca736e3c39f…`；
  库/app `d74b63d4a264…`。定向及90 captures、2215畸形、900 lifecycle
  全量通过（growth 2424832 bytes），日志`/tmp/metal-batch90-full.log`。
  GUI/Computer Use未运行，57份待人工；
  未提交/推送。

## 历史检查点：BATCH87 异步 Object/Mesh pipeline

- [BATCH87](BATCH87.md)：异步object+mesh pipeline的独立调用时descriptor快照、
  回调包装与离线重建接通；8个畸形输入拒绝。原始剩余**53 bridge / 42旧chunk**，
  T70另计54/43。捕获`f6b5c2a71ac9…`、库/app`1e9b10c9196a…`；定向通过，
  全量日志`/tmp/metal-batch87-full.log`。GUI/Computer Use未运行，54份待人工；
  未提交/推送。全量87 captures、2182畸形、870 lifecycle通过，增长
  2015232 bytes。

## 历史检查点：BATCH86 Object threadgroup memory

- [BATCH86](BATCH86.md)：动态object threadgroup memory绑定接通，真实object
  shader以scratch驱动payload，16/32/48-byte三阶段与5个畸形输入验证通过。
  原始剩余**53 bridge / 42旧chunk**，T70另计54/43。捕获`6b70534fc597…`、
  库/app`fbc12b54dfb3…`；86 captures、2174畸形、860 lifecycle全量通过，
  growth 1933312 bytes，日志`/tmp/metal-batch86-full.log`。
  GUI/Computer Use未运行，53份待人工；未提交/推送。

## 历史检查点：BATCH85 Object texture/sampler 绑定

- [BATCH85](BATCH85.md)：六种object texture/sampler绑定接通，真实采样经payload
  改变三次mesh三角形位置，28个畸形输入拒绝。原始剩余**54 bridge / 42旧chunk**，
  T70另计55/43。捕获`e3bf1eee74e6…`、库/app`6091de8a8265…`；85 captures、
  2169畸形、850 lifecycle全量通过，growth 3096576 bytes，日志
  `/tmp/metal-batch85-full.log`。GUI/Computer Use未运行，52份待人工；
  未提交/推送。

## 历史检查点：BATCH84 Object buffer/bytes 绑定

- [BATCH84](BATCH84.md)：三个对象阶段buffer/bytes重载接通，真实payload让
  三次MeshDispatch三角形落在x≈80/220/360；13个畸形输入拒绝。原始剩余
  **60 bridge / 42旧chunk**，T70另计61/43。捕获`d29d5e86c1a9…`、
  库/app`aef27dba4f8d…`；84 captures、2141畸形、840 lifecycle全量通过，
  growth 1671168 bytes，日志`/tmp/metal-batch84-full.log`。
  GUI/Computer Use未运行，51份待人工；未提交/推送。

## 历史检查点：BATCH83 Object/Mesh pipeline 与对象buffer

- [BATCH83](BATCH83.md)：真实object shader的buffer→payload→mesh三角形链，
  同步object+mesh pipeline及object buffer绑定接通，16个畸形输入拒绝。
  原始剩余**63 bridge / 42旧chunk**，T70另计64/43。捕获`e7e52fa73e7d…`、
  库/app`77fd1d77b4cf…`；83 captures、2128畸形、830 lifecycle全量通过，
  growth 2179072 bytes，日志`/tmp/metal-batch83-full.log`。
  GUI/Computer Use未运行，50份待人工；未提交/推送。

## 历史检查点：BATCH82 异步 Mesh pipeline

- [BATCH82](BATCH82.md)：异步mesh pipeline的descriptor调用时快照与回调包装，
  原descriptor改动后仍捕获BGRA8/mesh函数/options；8个畸形输入拒绝。原始剩余
  **64 bridge / 42旧chunk**，T70另计65/43。捕获`5c89deaf9996…`、库/app
  `8d09e99f81c5…`；82 captures、2112畸形、820 lifecycle全量通过，
  growth 1982464 bytes，日志`/tmp/metal-batch82-full.log`。
  GUI/Computer Use未运行，49份待人工；未提交/推送。

## 历史检查点：BATCH81 Mesh thread-grid draw

- [BATCH81](BATCH81.md)：直接`drawMeshThreads`真实捕获/回放与MeshDispatch；
  32线程网格的绿色三角形、8个畸形输入拒绝。原始剩余**65 bridge / 42旧chunk**，
  T70另计66/43。捕获`e6126828325b…`、库/app`2eea4b68f328…`；定向通过，
  全量日志`/tmp/metal-batch81-full.log`，81 captures、2104畸形、810 lifecycle
  通过，growth 1212416 bytes。GUI/Computer Use未运行，48份待人工。
  未提交/推送。

## 历史检查点：BATCH80 Mesh texture/sampler 绑定

- [BATCH80](BATCH80.md)：六种mesh texture/sampler绑定、真实mesh shader采样
  使三次draw各右移约70像素，28个畸形输入拒绝。原始剩余**66 bridge / 42旧chunk**，
  T70另计67/43。捕获`28d188275e44…`、库/app`54f1bbde30f5…`；定向通过，
  全量日志`/tmp/metal-batch80-full.log`，80 captures、2096畸形、800 lifecycle
  通过，growth 1638400 bytes。GUI/Computer Use未运行，47份待人工。
  未提交/推送。

## 历史检查点：BATCH79 Mesh buffer/bytes 绑定

- [BATCH79](BATCH79.md)：四个mesh绑定bridge及新chunk，真实shader使用buffer
  offsets0/16/32与inline radius，三阶段左/中/右像素及seek已自动验证；16个
  畸形输入拒绝。原始剩余**72 bridge / 42旧chunk**，T70另计73/43。
  捕获`28f4ba76c665…`、库/app`4fc7b5c759ba…`；定向通过，全量日志
  `/tmp/metal-batch79-full.log`，79 captures、2068畸形、790 lifecycle全量通过，
  resident growth 1490944 bytes。GUI/Computer Use未运行，46份待集中人工。
  未提交/推送。

## 历史检查点：BATCH78 最小 Mesh pipeline 与直接 draw

- [BATCH78](BATCH78.md)：同步mesh pipeline、直接`drawMeshThreadgroups`真实
  capture/replay，MeshDispatch事件及三角形中心/背景像素验证；17个畸形输入安全拒绝。
  当前原始标记**76 bridge / 42旧chunk**；T70功能缺口另计77/43。
  T78 capture `160f902aef23…`，库/app `dfe3ec542c36…`；78 captures、2052畸形、
  780 lifecycle全量通过，growth 0 bytes，日志`/tmp/metal-batch78-full-final.log`。
  GUI/Computer Use未运行，
  T34–T69、T71–T78与T10 marker共**45份**待集中人工；T70不供UI打开。
  未提交/推送。

## 历史检查点：BATCH77 异步Tile pipeline

- [BATCH77](BATCH77.md)：异步Tile pipeline创建的调用时descriptor快照、
  原生回调包装与离线重建；8个畸形输入拒绝。T77 capture `0470cd9f7ecf…`，
  库/app `173fadcf7c8a…`。当前原始标记**78 bridge / 42旧chunk**；T70
  功能缺口另计为79/43。**77 captures API/CLI、2035畸形输入、770 lifecycle**
  全量通过，growth 2342912 bytes；日志`/tmp/metal-batch77-full-final.log`。
  GUI/Computer Use未运行，T34–T69、T71–T77与T10 marker共**44份**待集中人工；
  T70不供UI打开。未提交/推送。

## 历史检查点：BATCH75–76 Tile资源与动态内存

- [BATCH75](BATCH75.md)：六种Tile texture/sampler绑定真实capture/replay，
  采样纹理`2/255`驱动GPU计数；22个畸形输入拒绝。T75 capture
  `647f1bb524cb…`。T74夹具和捕获未变。
- [BATCH76](BATCH76.md)：Tile threadgroup memory长度、非零offset与零长度清除；
  tile shader实际使用动态内存，8个畸形输入拒绝。T76 capture
  `1ca3f463afa1…`。当前原始标记**79 bridge / 42旧chunk**；T70功能缺口
  另计则为80/43。
- T76全量通过：**76 captures API/CLI、2027畸形输入、760 lifecycle**，
  growth 1687552 bytes，日志`/tmp/metal-batch76-full.log`；T75全量亦通过
  （75/2019/750），库/app `8af6f90b645b…`。
  GUI/Computer Use未运行，T34–T69、T71–T76与T10 marker共**43份**待集中人工；
  T70不供UI打开。未提交/推送。

## 历史检查点：BATCH74 Tile pipeline与tile dispatch

- [BATCH74](BATCH74.md)：6 bridge / 6旧chunk真实capture/replay，剩余原始标记
  **86 bridge / 49旧chunk**；T70功能缺口另计则为87/50。三个tile dispatch真实
  GPU写buffer并驱动后续draw，事件网格和回退已自动核对。
- **74份capture API/CLI、1997畸形输入、740次lifecycle**全量通过，growth
  2064384 bytes；日志`/tmp/metal-batch74-full-final.log`，库/app`e74885bf9e09…`。
  GUI/Computer Use未运行，T34–T69、T71–T74与T10 marker共**41份**待集中人工；
  T70不供UI打开。未提交/推送。下一族优先Tile texture/sampler六重载，继续复用
  T74 pipeline并以真实texture采样验证，不做空绑定假测试。Heap alias/placement
  继续按[PLAN.md](PLAN.md)中的时序门槛处理。

## 当前恢复检查点：BATCH73 Render heap residency

- [BATCH73](BATCH73.md)：`useHeap/useHeaps`四种Render重载真实capture/replay，
  **92 bridge / 55旧chunk**原始标记；T70实际未接通范围另计则为93/56。
  **73份成功capture API/CLI、1969畸形输入、730次lifecycle**全量通过，
  growth 1015808 bytes；日志`/tmp/metal-batch73-full.log`，库/app `57efba093c53…`。
- GUI/Computer Use未运行，T34–T69、T71–T73与T10 marker共**40份**待集中人工；
  T70不供UI打开。未提交/推送。下一族优先继续评估Heap placement/alias生命周期或
  其他有真实应用阻塞的接口，不把原生透传当作回放支持。

## 当前恢复检查点：BATCH71–72 自动 Private Heap 的 Buffer/Texture

- [BATCH71–72](BATCH71-72.md)：`MTLDevice::newHeapWithDescriptor`及 Heap 子 buffer/texture
  形成真实 capture/replay 闭环；仅自动分配、Private、tracked hazard 的受控范围。
  **72份成功捕获 API/CLI、1947畸形输入、720 lifecycle** 全量通过，growth
  1556480 bytes；日志 `/tmp/metal-batch72-full.log`，库/app `c36b553dcd14…`，
  Max1286。原始标记 **96 bridge / 59旧chunk**；新增 Heap 包装显式暴露两个
  offset/placement 未接通入口，所以 bridge 原始数不等于本批净新增能力。
- T70 GPU indirect ICB range仍明确拒绝且单独复验通过；不计入成功回放。
  GUI/Computer Use未运行，**T34–T69、T71/T72与T10 marker共39份**待集中人工；
  T70不供UI打开。未提交/推送。下一族考虑 Render `useHeap(s)` 四重载。

## 当前恢复检查点：BATCH70 GPU 间接 ICB 范围安全边界

- [BATCH70](BATCH70.md)：原始标记 **95 bridge / 60旧chunk**；其中间接 ICB 范围
  是原生执行、可截帧、离线明确拒绝的**未解决功能**，所以按真正接通功能的口径仍为
  **96/61**。T70 GPU 写入范围的 Metal Validation 原生/捕获、结构检查、预期
  replay 拒绝通过；T53/T67 旧 capture 定向 API/CLI 通过。正确事件树仍需执行点
  范围方案，见 [PLAN.md](PLAN.md)。
- T01–T69 + T10 marker 联合终端回归通过：**70份 capture API/CLI、1917畸形输入、
  700次 lifecycle**，growth 1802240 bytes；日志 `/tmp/metal-batch70-full.log`，
  库/app `6ba534c557b2…`。T70 是单独的预期失败负例，不计入成功回放。
- GUI/Computer Use 未运行；T70 不可离线加载，不加入人工 QA；既有37份待验不变。
  未提交/推送。

## 当前恢复检查点：BATCH69 Intersection Function与SharedEvent handle

- [BATCH69](BATCH69.md)：接通3 bridge / 2旧chunk，剩余 **96 bridge / 61旧chunk**；
  T55同步/异步intersection function链接compute pipeline，T69同进程SharedEvent handle
  导入与跨queue时间线。**70 captures API/CLI、1917畸形输入、700 lifecycle**全量通过，
  growth 770048 bytes；日志`/tmp/metal-batch69-full.log`，库/app `22c244afc13a…`。
- GUI/Computer Use未运行；**T34–T69 + T10 marker共37份**待集中人工；未提交/推送。
- ICB GPU indirect执行范围仍优先于扩大ICB范围，见[PLAN.md](PLAN.md)。

## 当前恢复检查点：BATCH67–68 patch draw重载与同进程shared handle

- [BATCH67–68](BATCH67-68.md)：接通5 bridge / 5旧chunk，剩余 **99 bridge / 63旧chunk**，
  Max1284不变。T67三种patch draw及T68同进程shared-texture handle实录和定向回放通过。
  **69 captures API/CLI、1903畸形输入、690 lifecycle**通过，resident growth 786432 bytes；
  日志`/tmp/metal-batch68-host-final.log`，库/app `f8b8b15d458…`。
- GUI/Computer Use未运行；**T34–T68 + T10 marker共36份**待集中人工；未提交/推送。
- 下一开发波次优先：ICB GPU indirect执行范围。必须以GPU执行点范围构建事件树；
  未通过GPU生成range真实夹具前继续拒绝，见[PLAN.md](PLAN.md)。

## 历史检查点：BATCH66 直接 patch tessellation

- [BATCH66](BATCH66.md)：factor buffer、scale、直接 `drawPatches` 三个 bridge 与三个旧 chunk
  已接通；剩余 **104 bridge / 68旧chunk**，Max1284不变。**67 captures API/CLI、
  1883畸形输入、670 lifecycle**通过，resident growth 1376256 bytes；日志
  `/tmp/metal-batch66-host-final.log`，库/app `d098a04a1a7b…`。
- GUI/Computer Use未运行；**T34–T66 + T10 marker共34份**待集中人工；未提交/推送。
  GPU生成的ICB indirect执行范围仍未解决，不从编码时CPU值推断事件树。

## 历史检查点：BATCH65 SharedEvent初值与GPU同步集中自动通过

- [BATCH65](BATCH65.md)：SharedEvent创建及旧chunk1033接通，首次GPU使用前CPU初值可回放，
  T63动态stride补5个布局负例；剩余 **107 bridge / 71旧chunk**，Max1284。
- **66 captures API/CLI、1872畸形输入、660 lifecycle**通过，growth2162688 bytes；
  日志 `/tmp/metal-batch65-host-final.log`，库/app `d69fa7da1f8…`。
- GUI/Computer Use未运行，**T34–T65 + T10 marker共33份**待集中人工；未提交/推送。
  SharedEvent仅同进程GPU时间线与首次GPU使用前CPU初值；后续CPU赋值与handle导出
  会被明确标记为不可离线回放。
  以下为历史检查点。

## 历史检查点：BATCH63–64 集中自动通过

- [BATCH63–64](BATCH63-64.md)：动态顶点stride四重载、单视图amplification和descriptor-backed
  Private shared texture接通；净减 **6 bridge / 2旧chunk**，剩余 **108/72**，Max1282。
- **65 captures API/CLI、1855畸形输入、650 lifecycle**在最终代码上通过，resident
  growth2392064 bytes；日志 `/tmp/metal-batch64-final.log`，库/app `8772ed87e268…`。
- GUI/Computer Use未运行，**T34–T64 + T10 marker共32份**待集中人工；未提交/推送。
  单视图和Private descriptor是明确边界，不代表multiview或shared handle已支持。
  以下为历史检查点。

## 历史检查点：BATCH62 集中自动通过

- [BATCH62](BATCH62.md)：14 个 bridge 与 2 个旧 chunk 接通，剩余 **114/74**，
  Max1278。设备纯查询/宿主调度无需 replay chunk；buffer/texture purgeable 只保证
  KeepCurrent、NonVolatile 离线回放，Volatile/Empty 明确不支持。
- **63 captures API/CLI、1784 异常、630 lifecycle** 在最终代码上通过，growth606208bytes；
  日志 `/tmp/metal-batch62-final-replay.log`。库/app `e11191df7a77…`，GUI executable 未变。
- GUI/Computer Use未运行，**T34–T62 + T10 marker 共30份**仍待集中人工；未提交/推送。
  下方为历史检查点。

## 当前恢复检查点：BATCH61 no-copy Buffer集中自动通过

- [BATCH61](BATCH61.md)：真实no-copy创建及旧chunk1006接通，当前剩余**128 bridge /
  76旧chunk**，Max1278；只承诺Shared默认options。应用指针和deallocator保留在原生抓取，
  离线回放复制字节。抓取保留资源可能延后deallocator，详情见批次边界。
- 完整一键**62 captures API/CLI、1772异常、620 lifecycle**通过，growth1327104bytes；
  日志`/tmp/metal-batch61-final.log`，库/app `0f7f42e63b34…`，GUI executable未变。
- GUI/Computer Use未运行，**T34–T61 + T10 marker共29份**仍待集中人工；未提交/推送。
  更高级剩余族应围绕真实捕获阻塞挑选；无需为了清零标记透传无法回放的对象。
  以下为历史检查点。

## 当前恢复检查点：BATCH60 Device ArgumentEncoder集中自动通过

- [BATCH60](BATCH60.md)：Device descriptor创建的独立ArgumentEncoder接通1 bridge /1旧chunk，
  仅只读2D纹理和sampler；总剩余**129/77**，Max1278。真实shader消费两个packet，
  身份/布局29异常和回退通过，其他布局明确拒绝。
- 一键完整门禁**61 captures API/CLI、1759异常、610 lifecycle**通过，growth1130496bytes，
  日志`/tmp/metal-batch60-final.log`；库/app `ad4e54209464…`，GUI executable未变。
- GUI/Computer Use未运行，**T34–T60 + T10 marker共28份**仍待人工；未提交/推送。
  下一波应按真实捕获阻塞或剩余高级功能族选题，不将无测试的标记移除当作接通。
  以下是历史检查点。

## 当前恢复检查点：BATCH58–59 共享存储纹理族集中自动通过

- [BATCH58](BATCH58.md)：三种纹理view与buffer-backed纹理接通**6 bridge、4旧chunk**，
  剩余**130/78**，Max1278未变。父资源初值、提交间CPU更新及回退语义已补齐；支持范围见批次记录。
- 原生/capture、逐事件API/CLI、91个新增畸形变体及旧T定向通过。共享资源改动后立即
  集中回归：**60份capture API/CLI、1730畸形样本、600 lifecycle**，growth524288bytes；
  日志`/tmp/metal-batch58-59-final.log`。库/app SHA `474533dbd0be…`，GUI executable未变。
- GUI/Computer Use未运行，**T34–T59 + T10 marker共27份**仍待集中人工；未提交/推送，
  用户UE路线保留。下一个候选是Device独立ArgumentEncoder的descriptor创建族，需先确认
  其布局、成员验证和真实shader消费，不为减少标记而直接透传。以下均为历史检查点。

## 当前恢复检查点：BATCH57 Argument数据族集中自动通过

- [BATCH57](BATCH57.md)为本波唯一详细记录：buffer成员/批量、constants、arrayElement，
  多packet与GPU地址重定位、CPU提交更新/seek；**140→136 bridge，旧chunk仍82**，Max1278。
- 共享Shared初值恢复影响较广，本族立即集中回归：**58 captures API/CLI、1639异常、
  580 lifecycle**通过，30份API验证层，growth2555904bytes。另三份旧源码新录兼容、
  T35新录与更严格的首dispatch初值回退通过；无本批计划内自动回归欠项。
- 新增161异常、GPU地址清零后的合法重定位正例；真实帧内资源重绑native正常、离线拒绝。
  仅顶层Shared目的buffer/只读成员/FS packet；不承诺nested、GPU生成、VS/CS或任意重绑。
- 日志 `/tmp/metal-batch57-final.log`；库/app `8e84e249d684…`；GUI `fe8bcf852b68…`，
  只补buffer成员槽位映射并编译，未运行。UI待验 **T34–T57+T10 marker，共25份**，
  总单保留全部旧项并补T57/T35差异；未使用Computer Use、未提交，用户UE路线保留。
- 下一编号T58，候选texture views/共享存储别名，先审查初值及双向写入语义。
  以下均为历史检查点。

## 上一检查点：BATCH54–56 三族集中自动通过

- [BATCH54-56](BATCH54-56.md)为本波唯一详细记录：重载兼容1bridge/3旧chunk，Function
  创建4bridge/2旧chunk，Argument texture/sampler数组及reflection创建3bridge。
  **148→140 bridge，87→82旧chunk**；仅追加async1272/1273，Max1274。
- 三fixture各native/capture6帧；集中**57 captures API/CLI、1478异常、570 lifecycle**通过，
  29份API Metal验证层，growth1671168bytes；另3份旧源码新录兼容通过。无自动回归欠项。
  日志 `/tmp/metal-wave54-56-final.log`；库/app `71cf518d1e5b…`，GUI executable未改。
- C已明确收窄；下波优先单独处理argument buffer成员/constants/arrayElement及GPU地址
  重定位、CPU初值/提交更新/回退，不可仅移除bridge标记。下一编号T57。
- UI待验 **T34–T56 + T10 marker共24份**，最小差异并入QA_CONSOLIDATED，全部保持开放。
  未启动GUI/Computer Use、未提交；旧修改及用户UE路线保留。以下均为历史检查点。

## 上一检查点：推进节奏调整

- 执行规则集中于PLAN“当前推进节奏”；连续2–3族、定向检查、集中关口，最多3族验证间隔。
  以后此处只保留短检查点，不继续复制完整批次内容到多份文档。
- 本次仅改推进规则与新增 `test_metal_replay_targeted_macos.sh`，无Metal功能/格式变更。
  快速入口9份代表capture的API Metal验证层与CLI通过，重复T53已去重；参数/缺失capture
  错误非零退出、bash语法及diff检查通过。日志 `build-macos-debug/metal-targeted.IAT5mF/`。
- 最近集中基线仍是BATCH53：148 bridge / 87旧chunk，54 captures/1330异常/540 lifecycle。
  本波次新增功能族0，未产生新的驱动回归欠项；本次没有重跑1330异常/540 lifecycle。
- 下一步：在一份活跃BATCH表中确定首批重载兼容/Function创建/Argument encoder的边界与
  必跑旧T，再连续实现；编号从T54起。真实依赖或失败可调整顺序，不强凑bridge数量。
- GUI待验仍为T34–T53+T10 marker共21份，见QA_PENDING/QA_CONSOLIDATED；未启动GUI，
  未使用Computer Use、未提交。下方BATCH53及更早条目保留为历史证据。

## 2026-09-26 BATCH53 自动完成：ICB GPU 操作与可靠回退

- 单命令 reset 接通 1 bridge；GPU ICB reset/copy/optimize 接通旧 chunk1224–1226。
  剩余 **bridge 148 / 旧 chunk 87**。追加1270 reset、1271 初值不可用诊断，Max1272。
  后三项 bridge 原本已转发，不重复计算减少量。详见 BATCH53 / PHASE53。
- CPU 初始化的 Shared render ICB 支持跨 ICB/同 ICB 非重叠 copy、GPU reset/optimize、
  空命令与单命令 reset/re-encode；每个 replay epoch 恢复初值，保留 copied PSO/buffers/
  indexed 状态。捕获前 GPU 内容不可重建时明确拒绝，不声称通用 GPU-generated ICB 支持。
- T53 native/capture 各12帧、四阶段像素、9 draw / 5 empty、资源状态和往返均通过。
  新增194异常+3合法变体，纳入旧70 ICB异常+2空命令正例；旧缺失pipeline崩溃已修复。
  **54 captures API/CLI、1330 类异常、540 次 lifecycle**通过，growth **2,228,224 bytes**；
  26份 API Metal验证层。另帧前GPU写入负例native/capture各3帧正常，离线明确拒绝。
- 最终日志 `/tmp/metal-batch53-final.log`；一键 `test_metal_capture_batch53_macos.sh`，
  仅 replay 设 LAST_TEST=53。库/app `a5dcf57234590…`，T53 `6ad98408529a…`；GUI未改。
- 无GUI/Computer Use、无提交，保留旧修改和用户UE路线。**T34–T53 + T10 marker 共21份**
  待集中人工，最小差异已并入 QA_CONSOLIDATED；PHASE35–53 仍开放。下一编号 T54/PHASE54。
  以下为历史检查点，不代表当前剩余计数。

## 2026-09-26 BATCH51–52 自动完成：bridge 已降到 149

- 新增 9 bridge：T51 六个异步 library/render/compute 创建入口；T52 newEvent、signal、
  wait。剩余 bridge **158→149**，旧 chunk 未处理分支 **93→90**；新增可回放 async
  chunk1264–1269，Max1270。详见 BATCH51-52、PHASE51、PHASE52。
- T51 保留原生 callback/error/reflection、复制 descriptor、登记父依赖、处理借用结果
  所有权；五个 PSO 均实际执行。Source async 离线仅支持 options=nil，不运行应用 block。
  扩展 PSO/Event 独立 native 引用，补 source 资源校验及 Device 类型初始化。
- T52 原生编码 signal/wait，按 replay epoch 重建 Event 防止回退残留；验证两队列三
  提交的 GPU 数据链。仅支持已捕获先 signal 后 wait，外部/SharedEvent/future-signal
  wait 拒绝；不是任意并行调度重构。未支持路径不以空实现冒充成功。
- 最终 **53 captures API/CLI、1066 类异常、530 次 lifecycle** 通过，resident growth
  **1,736,704 bytes**；18 份 API Metal 验证层。新增 226 类异常（104+122）和四个合法
  变体；旧正式 captures 未重录，另三份 source 新录兼容 API/CLI 通过。
- 库/app副本 `8dbbe2d60146…`；T51/T52 `e0fc940f6ce0…` / `e0116d679d5f…`；GUI
  executable 未改。日志 `/tmp/metal-batch51-52-final.log`，一键入口
  `test_metal_capture_batch51_52_macos.sh`，仅 replay 设 LAST_TEST=52。
- 无 GUI/Computer Use、无提交，用户 UE 路线和原有 dirty 修改保留。**T34–T52 + T10
  marker 共 20 份**待人工，QA_CONSOLIDATED 已合并最小 UI 差异；PHASE35–52 仍开放。
  下一编号 T53/PHASE53。以下是历史检查点，不代表当前剩余计数。

## 2026-09-26 BATCH50 自动完成：两种纹理CPU读取与子资源同步

- T50接通2 bridge / 3旧chunk1072/1073/1205，剩余 **158/93**，Max1264不变。
  CPU getBytes保留原生透传，新增帧内元数据/资源引用；离线不写应用CPU指针。
  synchronizeTexture真实编码并检查Managed/当前encoder/子资源；Shared同步明确拒绝。
- 四次CPU读回覆盖局部2D/mip/array slice、3D两层、host行/图像padding和哨兵；native及
  capture各12帧48次读取通过。API核对独立读/同步纹理保留、2D子资源、CPU派生68-byte
  参数/回退/usage、RGBA43/79/113/255。3D viewer未扩展，边界见PHASE50。
- **51 captures API/CLI、840类异常、510次lifecycle**通过，growth **999,424bytes**；
  16份API Metal验证层。新增176异常+2合法变体。最终一键/日志/完整hash见BATCH50，
  只重放LAST_TEST=50；旧正式capture不重录。
- 库/app `a648ce05f576…`，T50 `f56406953e8d…`。无GUI/Computer Use、无提交。
  全部待人工 **T34–T50 + T10 marker，18份capture**，阶段和批次保持开放。

## 2026-09-26 BATCH49 自动完成：两回调入口与CPU提交/回退修复

- T49 scheduled/completed handlers：2 bridge、旧chunk1049/1054，剩余 **160/96**；
  Max1264不变。Native回调保持包装身份/生命周期，离线只呈现注册记录，不执行应用block。
- 修复commit后取CPU快照的竞态、submission更新在部分replay中太晚应用，以及CPU更新
  buffer回退残留；payload改为安全读取并校验范围/身份/实际长度。边界见PHASE49。
- 两command buffer、八回调、两个dispatch/三个fill/一个draw，callback前后12-byte参数、
  412-byte output及padding、反射/绑定、RGBA41/67/101/255和事件回退通过。
- **50 captures API/CLI、664类异常、500次lifecycle**通过，growth **3,178,496bytes**；
  15份Replay API Metal验证层。最终一键含三份源码新录兼容检查，全部使用同一库。
  入口`test_metal_capture_batch49_macos.sh`，日志和版本见BATCH49；仅重放LAST_TEST=49。
- 库/app `1cd92c21c774…`，T49 `82771996ea03…`。无GUI/Computer Use、无提交。
  全部待人工 **T34–T49 + T10 marker，17份capture**；阶段保持开放，集中总单保留全部旧项。

## 2026-09-26 BATCH48 自动完成：4个bridge、4个旧chunk与shader生命周期修复

- T48 file/URL/data/bundle binary library，旧chunk1015–1018接通；Max1264不变。
  剩余 **162 bridge / 98 chunk**。默认库读取、失败返回值以及library/function提前释放
  的heap corruption一并修复；其他类型wrapper所有权尚未重构，详见PHASE48。
- 五个库均实际执行，首帧前释放全部源对象；移走原metallib/app/bundle仍能回放。
  五份10765-byte嵌入payload逐字节核对、676-byte输出/padding、反射/依赖/回退/像素通过。
- **49 captures API/CLI、577类异常、490次lifecycle**通过，growth **1,228,800bytes**；
  14份Replay API Metal验证层。另三份源码路径新录兼容capture各API与3-loopCLI通过。
  主batch和补充检查分段完成且已整合脚本，日志/范围见BATCH48；只重放LAST_TEST=48。
- 库/app `6df633b629eb…`，T48 `f71eaf76384b…`。未运行GUI/Computer Use，未提交。
  全部待人工 **T34–T48 + T10 marker，16份capture**，阶段和批次不关闭。
  二进制库无MSL源文件属预期；动态库/跨OS-GPU/真实UE支持不在本批承诺。

## 2026-09-26 BATCH47 自动完成：再接通3个bridge、3个旧chunk

- 同步render options/reflection、compute function options/reflection、compute descriptor。
  旧chunk1022/1024/1025接通，Max1264不变；剩余 **166 bridge / 102 chunk**。
- T47六个pipeline，原生reflection及结构体成员、descriptor线程数/整倍数/mutability，
  三次dispatchThreadgroups加一次dispatchThreads、两个draw；532-byte数据、20-byte padding、
  pipeline身份/绑定/反射/usage和事件回退、最终RGBA17/41/73/255自动核对。
- 最终一键batch通过：**48 captures API/CLI、513类异常、480次lifecycle**，resident growth
  **491,520bytes**；13份Replay API Metal验证层。T47新增84负例及options1/2合法变体。
  `test_metal_capture_batch47_macos.sh` / `/tmp/metal-batch47-final.log`；只重放设LAST_TEST=47。
- 库/app `16c921313c2f…`，T47 `a28eef0f401b…`；完整hash/范围见BATCH47、PHASE47。
  未运行GUI/Computer Use，未提交。全部待人工 **T34–T47 + T10 marker，15份capture**。
  高级descriptor/异步pipeline不在支持范围，不宣称真实UE兼容；阶段继续开放。

## 2026-09-26 BATCH45–46 自动完成：再接通7个bridge、9个旧chunk

- T44新增Fence包装/创建和Blit/Compute/Render update/wait；T45/T46新增两种定时present、
  buffer添加/清空debug marker。合计11入口，Compute追加chunk1262/1263；剩余 **169/105**。
- Fence执行真实GPU同步，检查类型/active encoder/先行update与replay epoch，拒绝无效
  等待；marker保留frame引用并清理被后台reset覆盖的注释。timed present保留参数及输出
  action，离线replay不等待原机器时钟。不宣称跨队列/外部fence或高级stage已支持。
- 完整 `test_metal_capture_batch45_46_macos.sh` 通过：三模式native验证层/capture/XML/
  3-loopCLI、精确GPU数据/usage/seek，最终 **47份API/CLI、429类异常、470次lifecycle**；
  resident growth **1,638,400bytes**。12份replay API验证层；T00空帧另做CLI/40次定向生命周期。
- 库/app副本 `33c4399361f7…`；T44/T45/T46为 `caa028676f68…` / `9e71f6b21883…` /
  `7b1fbea45e1f…`。详情 `BATCH45-46.md`、PHASE45/46；仅重放入口设LAST_TEST=46。
- 全部待人工 **T34–T46 + T10 marker，14份capture**，总单QA_CONSOLIDATED保留全部旧项。
  GUI/Computer Use未执行，阶段/批次保持开放；没有提交或覆盖既有UE路线。

## 2026-09-26 BATCH43–44 自动完成，集中 GUI QA 扩展至十一份

- T42新增七个compute resource/barrier/marker入口；T43新增五个render分阶段声明/barrier
  入口，保留声明-only资源与初始内容。新chunk1255–1261追加。剩余bridge/chunk **176/114**。
- 修复失败render pipeline创建包装空对象；补齐vertex RW buffer的descriptor、storage
  分类和usage，以及VS Storage Buffers表入口。UI源码已构建，未做GUI验证。
- native验证层/capture/XML/3-loopCLI与逐事件buffer/pixel/descriptor通过；最终联合
  **44 captures API/CLI、342类异常、440次lifecycle**通过，resident growth **2,899,968 bytes**。
  共享脚本加入T12/T19/T42/T43验证层，目前9份replay验证层覆盖。修正旧harness样例识别
  和缩略图假设，不减去旧fixture断言；证据见 `BATCH43-44.md`。
- 正式库/app副本 `736925e6e195…`，app executable `3cc9c3b63507…`；
  T42/T43 captures `25c6e7349f22…` / `7139504e2357…`。新一键入口
  `test_metal_capture_batch43_44_macos.sh`，仅重放设LAST_TEST=43。PHASE43/44列边界。
- **T34–T43 + T10 marker十一份**仍待人工，统一总单 `QA_CONSOLIDATED.md`。
  全部未验阶段保持开放；无Computer Use、无GUI、无提交；原有UE路线和编辑保留。

## 2026-09-26 BATCH41–42 自动完成，继续累积集中 GUI QA

- T40接通compute inline bytes、buffer offset、动态threadgroup memory三个此前隐式转发
  的入口；新chunk1252–1254追加。五次reduction验证CPU数据拷贝、真实/inline切换、两个
  encoder状态重置、offset/size事件快照、结果与padding。dispatch校验使用reflection及
  真实pipeline/device限制，含static+dynamic memory总量；旧compute布局限制仍保留。
- T41接通blit descriptor创建和四种CPU/GPU优化提示，补frame引用、修复CPU slice/mip
  写错chunk ID。额外13×7纹理只由hint引用，捕获/回退内容正确。带counter sample buffer
  的descriptor明确拒绝。剩余bridge/chunk **181/119**；计数不包含无旧标记的compute入口。
- 新两项Metal API Validation native、capture/XML、Replay API、3-loop CLI通过。
  最终整批脚本通过 **42 captures API/CLI、239类异常、42×10 lifecycle**，resident growth
  **2,736,128 bytes**（最终显示布局修复后，门限64MiB）；脚本/源码格式检查通过。
  T40/T41 hashes `73a3398cec48…` / `35745dc5f7ab…`。
- 补跑replay验证层发现并修复共有display参数60/64-byte布局差异，显式padding+编译断言；
  T01/T02/T09/T40/T41验证层均通过，新增到共享脚本。当前库/app更新为`b30a0e4cb6ee…`，
  上述最终联合回归已在新库上再次通过；802,816bytes属padding修复前整批结果。
- 一键入口 `test_metal_capture_batch41_42_macos.sh`；仅重放设LAST_TEST=41，详见
  `BATCH41-42.md`、PHASE41/42。集中QA扩展为 **T34–T41 + T10 marker九份capture**，
  见 `QA_CONSOLIDATED.md`。全部GUI未执行，阶段/批次保持开放；没有Computer Use、没有
  启动qrenderdoc、没有提交。原有UE路线及所有未提交编辑保留。

## 2026-09-26 BATCH39–40 自动完成，已合并后续 GUI QA

- T38 接通 VS/FS/CS single/batch sampler LOD 六个入口；有效 descriptor 按事件保存，
  普通 rebind 恢复静态范围，nil/batch 空槽和各 stage 不串状态。新增两个 compute 尾部
  chunk，不重编号旧 ID。剩余 bridge/chunk **182/124**（本会话基线216/165）。
- T39 接通 private GPU buffer staging readback，验证非对齐字节区间、末尾截断、非法
  资源/范围与事件回退。修复 direct compute 原先只接受纹理滤镜布局的问题，新增
  texture→buffer/buffer-only 分支和 reflection 绑定/大小/对齐检查；旧路径约束仍保留。
- 新两项 native/capture/XML/API/3-loop CLI 通过。最终联合入口覆盖 T01–T39 + T10_debug
  **40 captures API/CLI、175类异常检查、40×10 lifecycle**；lifecycle 新增同进程重复
  Private/Shared 非对齐读回，最终 resident growth **557,056 bytes**（门限64MiB）。
  库与 app 内嵌库同为 `35150e09a506…`。批次说明 `BATCH39-40.md`，新样例 T38/T39 SHA 为
  `92d869806241…` / `0d552603e709…`。
- `QA_CONSOLIDATED.md` 已把 T34–T39 + T10 marker 七份待验 capture 合并为同一 app
  验收顺序；公共操作只做一次。用户将在重置后集中 QA，现在不催促、不做 Computer Use。
  当前库版本见该总单；旧单 hash 属于历史检查点。PHASE35–40、相关三批均保持开放。
  全部修改未提交，原有 UE 路线与其他编辑保留。

## 2026-09-26 BATCH38 自动完成，累计待人工 T34–T37/T10 marker

- 在 BATCH35–37 之后继续扩展：八项 visibility/store/barrier render 入口、四项 blit
  双向/whole/range copy 实际 replay、三项 blit debug chunk；剩余 bridge/chunk 为
  **186/128**（会话基线 216/165）。同时修复 visibility buffer 漏捕获导致的 native
  replay 崩溃和 Unknown store 在提前结束 pass 时的风险；新增 D32S8 raw readback/picking。
- T36/T37/T10_debug 原生、capture/XML、Replay API、逐份 3-loop CLI 通过；最终库上
  T01–T37 + T10_debug 共 38 captures API/CLI 回归通过，38 × 10 lifecycle 通过，
  最终脚本 resident growth 2,064,384 bytes（门限 64 MiB）。
  T34/T35 14、T36 14、T37 43，共 71 类异常输入被干净拒绝；详见 `BATCH38.md`。
  统一复跑入口 `test_metal_replay_batch35_38_macos.sh`；不重新录制历史 captures。
- 最终库与 app 内嵌库 SHA `47e0635e8736…`；T36 `436b3262fef5…`、T37
  `f87cbbc8ca00…`、T10_debug `2d4a8d609bae…`；T34/T35 不变。旧 T10 capture 保留。
- 没有启动 qrenderdoc 或使用 Computer Use，没有提交或发布；原有 UE 路线等修改保留。
  PHASE35–38、BATCH35–37/BATCH38 均保持开放。后续读 `QA_BATCH35-37.md` 与
  `QA_BATCH38.md`，一次指导用户验证。下方 BATCH35–37 是后续扩展前的历史检查点，
  当前版本/计数以上述条目为准。

## 2026-09-26 BATCH35-37 自动完成，T34/T35/T36 GUI L4 延后

- T34 接通 render vertex/fragment inline bytes、buffer arrays 与 vertex offset；
  T35 接通 limited queue、descriptor/unretained command buffer 和两种 compute
  encoder 创建及 `waitUntilScheduled`；T36 接通 viewport/scissor arrays、depth
  clip/bias、triangle fill、blend constant，以及 command buffer/render encoder 的
  debug group/signpost。bridge/chunk 未接通计数从 216/165 降为 194/143。
- 两个终端 batch 均通过：三项 native、capture/XML、Replay API、逐份 3-loop CLI，
  T34/T35 共 14 类和 T36 共 5 类异常捕获拒绝；最终 T34/T35/T36 × 10 轮
  lifecycle 最后一轮 resident growth 606,208 bytes。共享 render 路径的 T01/T02/T04/
  T07/T17 Replay API 与 CLI 通过；T00、这些旧场景及 T34/T36 的 8 captures × 10
  lifecycle 通过，growth 245,760 bytes。qrenderdoc app/renderdoccmd 仅从终端构建，
  未启动 GUI；`git diff --check` 通过。
- 最终 replay 库及 app 内嵌库 SHA-256 `ba380a0317b6…`；T34/T35/T36 captures
  分别为 `e6ee77deb50a…`、`7fbed8c95df6…`、`37d6c5cb1306…`。批次说明见
  `BATCH35-37.md`，自动测试入口与后续 GUI 步骤见 `QA_BATCH35-37.md`。
- 按用户要求本轮不使用 Computer Use。`QA_PENDING.md` 已登记 T34/T35/T36；
  PHASE35–37 与 BATCH35-37 保持开放。此前 UE 5.6.1 路线记录及其他未提交改动保留。

## 2026-09-26 已定位 UE 5.6.1 官方 Mac 构建

- 用户提供 `/Volumes/CauseUseMac/UE_5.6`。只读检查确认官方 promoted
  UE 5.6.1、arm64 `UnrealEditor` 存在，足够用作普通帧和后续 Nanite
  最小场景试截的固定目标。尚未启动 UE 或试截帧。
- 安装内 `RenderDocPlugin.uplugin` 的 `PlatformAllowList` 只有 Win64/Linux；
  Mac 首轮应从终端启动外部注入路径，不预期现有插件按钮可用。
  Epic UE 5.6 Mac 要求列 M2+ Nanite/VSM Beta。细节见
  `REAL_WORLD_CAPTURE_ROADMAP.md`。当前 `QA_PENDING.md` 无人工待验项。

## 2026-09-26 真实应用截帧与高级功能路线评估

- 用户目标扩展到 macOS 图形应用/游戏、UE/Unity 集成、Nanite，以及光追、
  mesh shading 和更多 Metal 特性。当前 T00–T33 受控 capture/replay 闭环
  不能直接推断真实引擎帧兼容；优先建议试 UE 普通场景首帧，再以实际阻塞
  API 推进 Nanite 最小场景。评估与更省重复人工 L4 的节奏见
  `REAL_WORLD_CAPTURE_ROADMAP.md`。
- 本机 M2 Pro/16 GB，原生设备报告 Apple8、Metal3、ray tracing API；Apple
  功能表允许在此开发基础 mesh shading 与 ray tracing API，Apple9+ 的
  indirect mesh draw/mesh ICB 及 M3/M4 硬件加速/性能须换目标机验证。
  当前 RenderDoc wrapper 显式将 ray tracing 能力返回 false，高级 capture
  路径尚未实现。Epic UE 5.8 Mac 文档列 Nanite/VSM 为 M2+ Beta。
- 本次仅做只读代码/设备/外部文档检查并记录路线，没有改实现、captures
  或 GUI；`QA_PENDING.md` 仍无人工待验项，全部未提交改动保留。

## 2026-09-26 T32/T33 GUI L4 通过，BATCH33-34 关闭；action 名称审查

- 用户确认 T32 EID 12 与 T33 EID 18 的新版间接摘要
  `dispatchThreadgroups(indirect, <2, 2, 1>)` 符合预期。结合此前已验的参数、
  CS 间接参数栏、资源、画面和状态栏，T32/T33 GUI L4 均通过。
  PHASE33、PHASE34、BATCH33-34 已关闭；`QA_PENDING.md` 当前无待验项。
- 审查 Event Browser 的 custom action names：T32/T33 间接 dispatch 与 Vulkan/
  D3D11/D3D12/GL 展示实际 group 数的惯例一致；部分 Metal 直接 draw/dispatch、
  单次 indirect draw、ICB 和 blit 摘要较其他后端冗长或格式不统一。
  具体对照、字段取舍与后续定向验证计划见 `ACTION_NAME_ALIGNMENT.md`。
- 此轮只做代码审查与文档归档，没有改代码、正式 capture 或 app；沿用下方
  最终联合验证证据，未重跑测试。全部未提交改动保留。

## 2026-09-26 T32/T33 间接 dispatch 摘要对齐其他 API

- 对照 Vulkan、D3D11、OpenGL 和 D3D12，间接 dispatch 的 Event Browser
  自定义摘要突出参数 buffer 中实际执行的 threadgroup 数量。Metal 现显示
  `dispatchThreadgroups(indirect, <2, 2, 1>)`；buffer、offset 和
  `{4,4,1}` 每组线程数仍由 API Inspector/CS 栏位提供。
- T33 的参数由 GPU writer 产生：载入阶段在 command buffer 完成后读取参数
  并更新对应 action 的摘要与 dispatchDimension，避免显示写入前零值。
  T32/T33 Replay API 断言均确认摘要及三项实际执行数；T01/T10/T11/T13/
  T21/T28/T29/T30/T31/T32/T33 共 11 份定向 Replay API 与逐份 CLI 通过，
  含 T00 的 12 份 capture × 10 轮 lifecycle 通过，resident growth
  360,448 bytes；T32/T33 的 5/6 类异常捕获继续被拒绝。
- 最新 GUI `f1ebea2ddb86…`、库 `aa21c9a6983d…`；正式捕获不变。
  用户已确认此前 CS 栏位及其他 GUI 项，仅新版 Event Browser 摘要待最短
  复验。T32/T33 仍在 `QA_PENDING.md`，两阶段和批次保持开放；全部未提交
  改动保留。

## 2026-09-26 T32/T33 GUI 部分通过与 compute indirect 栏位修正

- 用户确认 T32 EID 6/12 及后续、T33 EID 7/10/12/18 及后续的参数数值、
  offset、CS 绑定、纹理、最终画面和状态栏；指出 compute indirect buffer
  被错误放进 IA。Metal Pipeline State 已把 compute 间接参数移至 CS 的
  `Indirect Dispatch`，图形 draw 的 IA `Indirect Buffer` 保持原位。
- Event Browser 依通用设置显示自定义 action 名称；API Inspector 保留完整调用
  参数。Metal compute 自定义摘要现含 offset 16 和 4×4×1 threads/group。
  Vulkan 的间接 dispatch 也使用自定义摘要显示解析后的执行参数，而非照抄
  API 调用。用户仍须在新 GUI 上确认这两处可见行为。
- 最终 GUI `f1ebea2ddb86…`、库 `9c0b65bea9f5…`；正式 T32/T33 captures
  SHA 保持 `9aa1432c8d9a…` / `4478197f9994…`。T11/T13/T21/T32/T33
  Replay API 与逐份 CLI、含 T00 的 6 份 capture × 10 轮 lifecycle 通过，
  `git diff --check` 通过。新最短复验见 `QA_BATCH33-34.md` 顶部，
  `QA_PENDING.md` 分别保留 T32/T33 待验；PHASE33/34、BATCH33-34 不关闭。

## 2026-09-26 BATCH33-34 P33.1–P34.4 自动验证完成，T32/T33 L4 待验

- T32 CPU shared Buffer 21 byte offset 16 的 `2,2,1` 间接参数与 T33 GPU writer
  在同一 command buffer、跨 compute encoder 生成 Buffer 23 的相同参数均完成。
  T33 帧内 blit 先清零，Replay API 已核对写入前零值、writer EID 10 后
  `2,2,1`、EID 18 间接 dispatch 后 8×8 纹理与输出 buffer，以及前后 seek。
  T32 Begin/dispatch EID 6/12；T33 writer/第二 Begin/dispatch EID 10/12/18。
- 原生 T32/T33、正式 capture/XML、参数/descriptor/usage/readback/action、
  T32 五类及 T33 六类异常拒绝均通过。最终库上 T01/T10/T11/T12/T13/
  T16/T17/T18/T19/T21/T28/T29/T30/T31/T32/T33 共 16 份 Replay API 与
  逐份 CLI replay 通过；含 T00 的 17 份 capture × 10 轮 lifecycle 通过，
  resident growth 1,015,808 bytes。两份自动 DDS 完全相同且各 384 字节，
  参数 raw 各 44 字节。脚本语法、Python 编译、`git diff --check` 通过。
- 最终 app `build-macos-debug/bin/qrenderdoc.app` GUI SHA-256
  `5b2bc3aa95cf…`，内嵌库与 `build-macos-debug/lib/librenderdoc.dylib`
  同为 `8c05d8b03281…`；T32/T33 正式 capture SHA-256 分别为
  `9aa1432c8d9a…`、`4478197f9994…`。`QA_BATCH33-34.md` 给出准确路径和
  同轮 EID 验收步骤。L3 条件未触发：批内及定向旧场景已圈定间接 dispatch、
  GPU 参数与 descriptor 风险，且未进入发布/合并门槛。
- `QA_PENDING.md` 中 T32/T33 均为“自动通过，待用户 GUI L4”。
  PHASE33、PHASE34、BATCH33-34 保持开放；全部未提交改动保留。

## 2026-09-26 BATCH33-34 P33.1–P33.2 进行中

- 无需新对话或 compact，已从 `PHASE33.md` P33.1 开始。T32
  `Metal_Compute_Indirect_Dispatch` native 夹具已添加：CPU 在 shared buffer
  byte offset 16 放置 12 字节 threadgroup 参数 `2,2,1`，以 `4,4,1`
  threads-per-threadgroup 执行；原生纹理、buffer 写入和哨兵核对通过。
- compute indirect dispatch 的 bridge、chunk、replay/action/usage 已接通并编译。
  T32 capture/XML 已生成，XML 保留 buffer、offset 16 和 threadgroup size；
  CLI replay 一次通过。Replay API 的逐事件/数据断言、异常拒绝及 T33 尚未完成，
  不将 T32 记为自动通过。当前 `QA_PENDING.md` 已登记 T32 批末 L4。
- 已成功构建 demo 与最终 app/renderdoccmd。首次构建误用不存在的 `qrenderdoc`
  target，改为 `build-qrenderdoc`；新增 chunk 后同步 `metal_stringise.cpp`
  的 Max 静态断言，构建通过。下一步补 T32 Replay API 自动断言，随后进入
  P34.1。此前所有未提交改动保留。

## 2026-09-26 BATCH31-32 关闭，下一项 P33.1

- 用户已确认 T30 公共事件树最短复验符合预期：EID 4/13 Begin/End 顶层，
  `$action()` 保留边界并隐藏 EID 8/11 状态调用。T30 原功能 L4 此前已通过；
  T31 的 EID 7 分组、批量绑定/空槽、筛选、画面及状态栏也已通过。
  T31 GUI 导出入口由用户明确免除本轮复验，自动 DDS/raw 数据已核对，
  不记作本轮 GUI 实测。`QA_PENDING.md` 当前无待验项。
- 最终构建上的联合定向证据沿用 `/tmp/batch31-32-scope-fix-final.log`：
  T01/T06/T07/T08/T10/T11/T20/T22/T23/T24/T25/T26/T27/T28/T29/T30/T31
  共 17 份 Replay API 与逐份 CLI replay、含 T00 的 18 份 capture × 10 轮
  lifecycle 通过。L3 条件未触发。此次只记录用户验收并编写下一批计划，
  未改代码或 captures，故不重复构建或测试；`git diff --check` 通过。
- PHASE31、PHASE32 与 BATCH31-32 已关闭，全部未提交改动保留。下一批
  `BATCH33-34.md`：T32 CPU 参数的 compute indirect dispatch，T33 GPU 生成
  参数的 compute indirect dispatch；第一项 `PHASE33.md` P33.1 fixture/native。

## 2026-09-26 T31 GUI L4 通过，导出入口本轮免复验

- 用户在 EID 18 的独立 Pipeline State 确认 sampler 0/1/3/4、Read-Only
  Textures 0/2/4、Read-Only Buffers 0/1/2/3/5/7 为 Empty，覆盖要求的
  sampler 3、texture 2、buffer 5。`$action()` 隐藏状态调用，清空筛选后恢复；
  状态栏为 `No problems detected`。结合此前对 EID 7、12–14、18 及后续
  绑定和画面的明确反馈，T31 GUI L4 记为通过。
- 用户明确表示本轮无需重复 GUI 导出入口测试。此前自动 DDS/raw 数据已核对；
  GUI 导出入口记录为用户免复验，不记为本轮实测。无需为此重新构建或回归。
  当前仅 T30 公共事件树最短复验待确认；PHASE31 与 BATCH31-32 仍不关闭，
  PHASE32 的 T31 L4 已通过，但随批次保持开放。全部未提交改动保留。

## 2026-09-26 T31 部分 L4 反馈与验收步骤修订

- 用户确认 T31 EID 7 层级、Texture 零值、Buffer 哨兵，EID 12/13/14 的
  `range` 以 `location, length` 显示正确，EID 18 及后续绑定和画面符合预期。
  T31 的虚拟 `Copy/Clear Pass #1` 消失是正常结果：通用 `AddFakeMarkers()`
  只在足够多的连续可归组 action 上生成 marker；修复后 Begin Compute 是边界，
  前方 blit action 不足以独立成组。Vulkan 使用相同通用规则。
- 原验收单将 API Inspector 与独立 Pipeline State 混写，已改为在 EID 18
  经 **Window → Pipeline State → CS → Show Empty Items** 一次检查
  texture 2、sampler 3、buffer 5 的空槽。代码和正式 captures 未变。
  `QA_PENDING.md` 保留 T30 事件树最短复验，以及 T31 三空槽、筛选、导出/
  保存与状态栏等未明确确认项；阶段和批次不关闭。

## 2026-09-26 T31 反馈修复与 Vulkan render pass 行为对齐

- 用户确认 T30 全部通过。T31 用户指出 EID 7 `Begin Metal Compute Pass` 被 GUI
  自动 `1–7 Copy/Clear Pass #1` 错收，且原验收单把资源初值写成其 Output。
  根因是 `ReplayController::AddFakeMarkers()` 将 `PassBoundary` 当作普通零输出
  action 合并；已令所有显式 pass 边界保留在自动分组外。T31 的 EID 7 Begin、
  EID 23 End 均为顶层，原 EID 保持不变；EID 7 右侧 Output 本来就应为空，
  Texture 20/21 的零值须在 Texture Viewer 手动选资源查看。
- 对照 Vulkan：Begin/End render pass 是 `PassBoundary`，`$action()` 保留；
  Vulkan 名称概括 attachment load/store，并记录 Clear/Discard/Resolve usage。
  Metal render pass 现在显示各 color/depth/stencil attachment 的 Load/Clear/
  Don't Care 与 Store/Resolve，记录 begin/end 对应 usage；compute pass 因无
  attachment 仍只标 Begin/End。pass boundary 不消耗 action 编号，与 Vulkan 一致。
- 新断言先在旧库重现 T31 假 marker 吞入边界，修复后通过；T07 深度模板与 T08
  MSAA resolve 的名称/usage 定向断言通过。最终库 SHA-256 `7a494e202d1b…`，
  GUI 程序 SHA-256 `d80297e329c9…`；T30/T31 正式 capture 未变，SHA-256 仍为
  `8bfe33501fad…` / `f5ad1c86660b…`。T01/T06/T07/T08/T10/T11/T20/T22/T23/
  T24/T25/T26/T27/T28/T29/T30/T31 共 17 份 Replay API 与逐份 CLI replay
  通过；含 T00 的 18 份 capture × 10 次 lifecycle 通过，resident growth
  1,392,640 bytes。`git diff --check` 通过。新日志
  `/tmp/batch31-32-scope-fix-final.log`；L3 条件仍未触发。
- 修订后的 `QA_BATCH31-32.md` 给出 T31 最短复验。T30 原功能 L4 已通过，
  但公共事件树改变需用户仅复验 Begin/End 层级与 `$action()`；T31 其他 GUI
  项仍待明确确认。PHASE32/BATCH31-32 不关闭；所有未提交改动保留。

## 2026-09-26 BATCH31-32 P31.1–P32.4 自动验证完成，用户 L4 待验

- T30 compute sampler 直接绑定与 T31 compute texture/sampler/buffer 批量绑定已实现；
  T31 包含非零 range、null 清除、已声明未使用绑定和后续直接覆盖。
  正式 capture 为 `captures/metal-smoke/t30_capture.rdc`、`t31_capture.rdc`；
  SHA-256 分别为 `8bfe33501fad…`、`f5ad1c86660b…`。最终 qrenderdoc
  `build-macos-debug/bin/qrenderdoc.app` 的程序 SHA-256 `d80297e329c9…`，
  内嵌与构建库均为 `fa401fb7e602…`。
- 最终定向证据：`/tmp/batch31-32-targeted.log` 中 T01/T03/T11/T12/T16/T17/
  T18/T19/T28/T29/T30/T31 的 native、Replay API、逐份 CLI 均通过。
  T30/T31 的正式 XML 与 Point/Linear 像素、状态快照、descriptor/usage、seek、
  DDS 和 T31 raw bytes 已核对；异常捕获 2+9 例及 T28/T29 的 10 例均拒绝。
  13 份 capture × 10 轮 lifecycle 通过，resident growth 1,638,400 bytes，
  日志 `/tmp/batch31-32-targeted.log`。脚本语法、Python 编译及 `git diff --check`
  通过。批次脚本已扩展 T30/T31，未跑全量 L3。
- L3 条件未触发：T12 已覆盖通用 descriptor 改动；无 render pass、初始资源内容、
  ICB 或公共事件层级改动，也未进入发布/合并门槛。
- 合并 GUI 验收单 `QA_BATCH31-32.md` 待用户同轮 qrenderdoc 检查。
  `QA_PENDING.md` 保留 T30/T31；反馈前 PHASE31/PHASE32/BATCH31-32 不关闭。
  全部工作区改动保留，下一项是接收 L4 后仅对不符项修复或复验。

## 2026-09-25 BATCH31-32 P31.1 开始

- 从 T30 compute sampler fixture/native 开始；基于 T11 的确定性 8×8 输入，
  增加 compute sampler 直接绑定与可区分的采样输出。`metal_compute_sampler.cpp`
  已加入 demo target，功能实现和验证进行中。当前工作区的新改动保留。
- `QA_PENDING.md` 已登记 T30 批末用户 L4；T31 尚未开始。前批 T28/T29
  已关闭且无遗留人工 QA。下一安全操作是完成 T30 未注入 native 参考验证，
  随后接通 capture/replay，再继续 T31。

## 2026-09-24 BATCH29-30 关闭与下一批准备

- 用户在同一轮 qrenderdoc 确认 T28 右侧 Input/Output 缩略图正常，以及 T29
  `$action()` 筛选和右侧缩略图正常。此前 T28/T29 Event/API、Pipeline、
  Buffer 20、画面、导出、状态栏均已确认；T28 10×7 UI DDS 与自动参考完全一致。
  T22/T25 因事件树改动所需的最短复验也已通过。PHASE29、PHASE30 与
  BATCH29-30 现已关闭；`QA_PENDING.md` 当前无待验项。
- 自动证据：`/tmp/batch29-30-final.log` 覆盖 T28/T29 native/capture/XML/
  Replay API、10 类异常拒绝、联合 T11/T10/T01/T18/T19/T12/T16/T17、
  逐份 CLI replay 与 11×10 lifecycle。缩略图修复后追加 T01/T03/T09/T11/
  T12/T16/T17/T22/T25/T28/T29 Replay API 与逐份 CLI replay、12×10
  lifecycle，resident growth 1,376,256 bytes。最终 GUI binary SHA-256
  `b8259e2471ce…`、内嵌 replay 库 `81738a1bcbde…`；正式 T28/T29 capture
  SHA-256 分别为 `b54fb5128aaf…`、`99787263f8b9…`。
- 用户明确本次向 `crossous/renderdoc-metal` 的提交仅是备份，不是发布；
  因此本次不运行 T00–T29 L3。上述定向验证与 L4 作为提交依据。
  下一批 `BATCH31-32.md` 计划 T30 compute sampler 直接绑定与 T31 compute
  texture/sampler/buffer 批量绑定；第一项 P31.1，尚未开始实现。

## 2026-09-24 历史检查点：用户验收与下一批准备

- 用户重新保存的 `captures/metal-smoke/t28-output-ui.dds` 为 408 字节，SHA-256
  `ada9ab66b7c5…`，与自动参考 `t28_output.dds` 完全一致；T28 DDS 验收通过。
  用户确认 T22 父行展开为 EID 6/7，点父行绘制正常；T22 复验通过。
- Texture Viewer 右侧 Input/Output 列表有条目但缩略图为空：根因是通用
  `ReplayOutput::DrawThumbnail()` 使用 Headless output，Metal replay 原先仅接受
  macOS layer。现已为 Metal 增加 Headless output；最新 app 路径不变，GUI binary
  SHA-256 `b8259e2471ce…`、内嵌 replay 库 SHA-256 `81738a1bcbde…`。
  正式 T28/T29 captures 未变。T01/T03/T09/T11/T12/T16/T17/T22/T25/T28/T29
  的缩略图/Replay API 和逐份 CLI replay 通过；12 份 capture × 10 轮 lifecycle
  通过，resident growth 1,376,256 bytes；`git diff --check` 通过。T28/T29
  计算前黑色、计算后 Input/Output 有内容；T09 mip/array/cube 子资源也有内容。
  L3 条件未触发。用户仍需重启 app，在同轮 GUI 复验 T28/T29 右侧小图；
  T29 `$action()` 筛选亦待确认。详见 `QA_BATCH29-30.md` / `QA_PENDING.md`。
  T28/T29 与 BATCH29-30 不关闭，全部未提交改动保留。下一项等待用户 L4
  反馈；若反馈不符，先分析修复并给最短复验。

- 用户确认 T25 indexed instanced 的多个 instance、ICB 子 draw 位于 execute 内、
  点击 execute 画到最后子 draw；T27/T28 所见画面及 `setBuffer` 等状态 API 的
  EID 已确认。T25 本轮新 GUI 行为复验通过，T27 原验收保持通过。
- 本轮仍待人工确认：T28 从 10×7 Texture 20 保存的 GUI DDS（磁盘上旧文件仍
  是 400×300 Texture 30）；T29 `$action()` 隐藏 EID 10/11、保留 EID 12，
  清空筛选后恢复；T22 指定 EID 5 父行画出全部子项的画面。T28/T29 与
  BATCH29-30 继续未关闭。接手须看 `QA_PENDING.md`，不可因进入下一批遗漏。
- 已规划后续 `BATCH31-32.md`：T30 compute sampler 直接绑定、T31 compute
  texture/sampler/buffer 批量绑定；清单见两份 PHASE 文档。当前仅完成计划，
  未开始 T30 实现或宣称 T30/T31 自动/L4 通过。现有未提交改动全部保留。

## 2026-09-24 历史检查点：Metal 事件树与 ICB 复验

- 用户已确认 T29 Buffer 20 哨兵/写入变化，以及新版 API Inspector 可见 `setBuffer`
  等绑定。用户指出 Metal 无筛选 Event Browser 缺状态调用；ICB execute 子 draw
  未收在父行且点父行无绘制；范围 `0+1` 不清楚；indexed instanced 子行未显示实例数。
- 已为 Metal 帧内非 action 调用分配 EID；ICB execute 改为 MultiAction 父子层级，
  父行 seek 回放到范围末端；名称改为 `location=…, length=…`，子 draw 显示
  `instances=…`。API Inspector 的旧无 EID 补列路径已移除，现直接用正式 EID。
- 当前 qrenderdoc 路径仍为 `build-macos-debug/bin/qrenderdoc.app`，binary SHA-256
  `b8259e2471ce…`。正式 T28/T29 captures 已重生成；SHA-256 分别为
  `b54fb5128aaf…`、`99787263f8b9…`。T28 的 begin/dispatch/draw 是 EID 3/7/14；
  T29 的 fill/begin/setBuffer×2/dispatch/draw 是 EID 4/6/10/11/12/19。
- 自动结果：`/tmp/batch29-30-final.log` 的 T28/T29 native、capture/XML、Replay API、
  10 类异常、T11/T10/T01/T18/T19/T12/T16/T17、逐份 CLI 与 11×10 lifecycle
  再次通过，resident growth 1,343,488 bytes。事件语义受影响旧场景 T20–T27 的
  Replay API/CLI 逐份通过，T20–T27 异常拒绝按各自脚本通过，T00+T20–T27 的
  9×10 lifecycle 通过，growth 1,048,576 bytes。T29 两条 setBuffer EID 的逐项
  pipeline state、T22/T25 ICB 父行最终像素、层级、实例数均已定向断言。L0 构建与
  `git diff --check` 通过。事件影响已用编号定向覆盖，未触发 L3。
- **待人工 QA**：T28 Texture 20 的 10×7 UI DDS 仍待重存并由 agent 核对；T29
  Event Browser EID 10/11 与 `$action()` 筛选待验；T22/T25 因新 ICB 层级/父行
  seek/实例数显示改动需最短复验。新步骤与完整路径在 `QA_BATCH29-30.md` 顶部。
  T28/T29 与 BATCH29-30 不关闭；T22/T25 原已验功能保持关闭，只追踪新改动复验。
  全部未提交改动保留。第一项未完成工作是等待用户按当前验收单反馈；收到 DDS 后
  agent 须核对导出内容，并对任何不符先修复、重跑受影响自动验证、给最短复验步骤。

## 当前批次

最近关闭批次为 `BATCH33-34.md`；当前 `BATCH35-37.md` 的自动验证已完成，
T34/T35/T36 GUI L4 待后续 chat，因此批次保持开放。以本文件顶部 2026-09-26
检查点为准。BATCH29-32 也已关闭。
以下条目为 BATCH29-30 进行时的历史状态。

- 2026-09-24 已关闭 `BATCH27-28.md`：T26 ICB `inheritPipelineState` 与 T27
  `inheritBuffers` 的最终联合 L0/L1/L2、逐份 CLI replay、lifecycle 和同轮最新
  qrenderdoc L4 均通过；T00–T27 纵向切片已关闭，全部未提交改动保留。
- 当前批次：`BATCH29-30.md` / T28 `dispatchThreads` + T29 compute buffer binding；
  P29.1–P30.4 的功能与最终联合自动验证已通过。用户已反馈 T28 L4 部分通过，
  T29 L4 部分通过；当前以本文件顶部恢复检查点和 `QA_BATCH29-30.md` 的
  新 EID 验收步骤为准。下面旧 EID 与修复进程仅作历史记录。
  T28/T29 均不得标为关闭。
- 2026-09-24 T29 用户反馈与 API Inspector 修复：用户确认首次 ⌘O 重复弹窗已解决，
  T29 EID 4/5 的 Buffer 20 哨兵恢复和计算写入、画面及 `No problems detected`。
  `t29-pipeline-ui.html`、`t29-buffer-ui.csv` 和 Save Bytes 导出的 336-byte raw 已存在；
  raw 与自动 `t29_output.bin` 逐字节相同。UI 的实际菜单名是 Export to Bytes，
  对应原始字节；意外带反引号的原文件已复制为标准 `t29-buffer-ui.bin`，原文件保留。
  用户发现 API Inspector 看不到 `setBuffer`/`setTexture`。根因是这些状态 chunk
  存在于 XML，但 Metal 没为它们分配 EID，原 API Inspector 只列 action.events。
  已在 Metal action 的 API Inspector 中补列前一 EID 到该 action 之间的无 EID
  structured chunks，显示 `—` EID；事件树和 Replay API 语义未变。最新 GUI 构建
  `/tmp/batch29-30-api-inspector-build.log` 成功，binary SHA-256 `c1a773a05d56…`。
  用户须 ⌘Q 重开同一路径 app 后，在 T29 EID 5 最短复验两条 setBuffer 可见。
  EID 2/4 的 Buffer 20 值从 Resource Inspector 的 View Contents 打开 Buffer Viewer
  后切事件检查；EID 2 仍待用户明确确认。点击 API Inspector 中 Buffer 只到
  Resource Inspector 属预期。
  T28 的 10×7 Texture 20 UI DDS 仍待导出；批次未关闭。
- 2026-09-24 最新用户反馈与修复：T28 EID 2/5 等其余项目已由用户确认，
  `t28-pipeline-ui.html` 含正确的 `dispatchThreads`、`filter_main`、Texture 19/20。
  用户在 EID 1 看到的是默认 Texture 30 最终 backbuffer 渐变；此事件是 pass 边界，
  无 compute 绑定属预期，但仍需明确切到 10×7 Texture 20 检查全黑。
  用户保存的 `t28-output-ui.dds` 是 400×300 Texture 30，而目标 Texture 20 的自动
  DDS 为 10×7、408 bytes；需从 Texture 20 Viewer 重新保存并由 agent 核对。
  用户还报告每次冷启动首次 ⌘O 加载后文件选择窗口再次弹出。已将 ⌘O 的打开动作
  延后到按键事件结束，`/tmp/batch29-30-open-shortcut-build.log` 构建通过；最新 GUI
  binary SHA-256 `52c7e50b9928…`。正式 captures 未变，T28/T29 Replay API smoke、
  CLI replay 和自动 DDS/raw 对照再次通过。此 GUI 修复仍待用户首次打开实机复验。
  macOS 文稿/下载权限提示与当前位于 Developer 的 captures 路径不一致；本程序使用
  系统文件对话框，未见 app sandbox entitlement，具体权限触发路径尚无实机证据。
- 2026-09-24 首次打开问题二次修复：用户在相同 `.app` 上确认第一版延迟 ⌘O
  处理后仍可稳定复现：先开 T28，进度条和事件树出现后文件窗口再弹出，可继续开
  T29；反向顺序同样如此。源码中 Open Capture 同时注册为 Qt QAction 快捷键和
  `MainWindow::eventFilter` 全局快捷键；第二版只保留 Qt QAction 处理 ⌘O，
  第一版时序修改已撤回。`/tmp/batch29-30-open-shortcut-v2-build.log` 构建成功，
  GUI binary SHA-256 `969a565fe559…`。需用户 ⌘Q 退出旧进程后实机确认新版本。
  用户已找到 Texture List 并确认 T28 EID 1 Texture 20；UI DDS 仍为 400×300
  Texture 30，10×7 Texture 20 DDS 待导出。T29 未收到 GUI 反馈。
- 验收分工已更新：从 BATCH29-30 起，agent 完成全部终端可判定的 QA，并在最终
  captures 准备好后按 `QA_GUIDE.md` 给出一次性 T28/T29 GUI 验收单；用户反馈 L4
  通过后才关闭批次。`QA_PENDING.md` 跨批次保留所有未验 T；用户漏看或只反馈部分
  结果时其余项目持续“待人工 QA”，后续每次结果和交接都提示，且不得把对应批次
  标为关闭。当前 T28 的 GUI 已收到部分通过反馈，余项待最短复验；T29 尚待验。
  T26/T27 的 L4 已完成。
- 2026-09-24 T28/T29 已实现。最终构建为
  `build-macos-debug/bin/qrenderdoc.app`；正式 captures 为
  `captures/metal-smoke/t28_capture.rdc`、`t29_capture.rdc`。T28 7×5 thread grid/
  4×3 group 和 10×7 texture 未触及区；T29 buffer slot 2/4、offset 32/64、
  272-byte descriptor 范围与 64 项计算数据均由 Replay API 断言。
- 最终自动证据：脚本 `/tmp/run-metal-batch29-30.sh`，日志 `/tmp/batch29-30-final.log`。
  native 5 帧、正式 capture 8 帧/XML、T28 EID 1/2/5 与 T29 EID 4/5/8 的
  action/state/usage/readback/seek 和像素通过；T28 408-byte DDS、T29 336-byte raw
  内容已核对。T28/T29 合计 10 类非法 grid/binding/offset/resource 被拒绝。
  联合 T11/T10/T01/T18/T19/T12/T16/T17 Replay API/output 与 CLI 通过，
  11 份 capture × 10 轮 lifecycle resident growth 1,638,400 bytes；增量构建、
  脚本语法、Python 编译与 `git diff --check` 通过。随后仅调整 CS 页面空槽分类，
  `/tmp/batch29-30-ui-final-build.log` 重建 qrenderdoc 成功，正式 T28/T29 的
  Replay API smoke 再次通过；最终 GUI binary SHA-256 为 `15f84694baae…`。
- L3 决策：descriptor access 的改动已触发并通过 T12/T16/T17 条件验证；未改
  render-pass 或资源初始内容/所有权。定向验证未暴露无法圈定的跨场景风险，
  未到发布/合并门槛，故 T00–T29 L3 未触发。
- L4：`QA_BATCH29-30.md` 的合并验收单已更新。T28 仅 Texture 20 DDS 待导出；
  T29 的已确认 GUI 项及导出内容通过，API Inspector 无 EID 状态调用修复及
  EID 2 Buffer 20 值待最短实机复验。`QA_PENDING.md` 分别保留未验项，
  本批次保持未关闭。
- 用户指出原验收单没有讲清程序与两份 capture 的打开位置；已在
  `QA_BATCH29-30.md` 补上 Finder“前往文件夹”、qrenderdoc 打开顺序与三个完整路径。
  `QA_PENDING.md` 记录了此反馈；用户尚未执行 L4，T28/T29 继续待验。
- 最近自动证据：`/tmp/run-metal-batch27-28.sh`、`/tmp/batch27-28-final.log`，运行目录
  `/tmp/metal-batch27-28-final.XnLnvh`。T26/T27 native 5 帧、正式 capture/XML、Replay API
  action/state/usage/readback/seek、原始 buffer 字节、逐份 CLI replay 通过；联合
  T20/T22/T24/T25/T01/T05/T16/T19 定向 replay/output 与 CLI 通过。T20/T22/T24/T25
  的 6/11/8/15 类及 T26/T27 合计 14 类异常 RDC 被拒绝。11 份 capture × 10 轮
  lifecycle resident growth 507,904 bytes；最终增量构建、脚本语法、Python 编译、
  `git diff --check` 通过。
- 最近 L4 证据：最新 qrenderdoc 同一进程依次打开正式
  `captures/metal-smoke/t26_capture.rdc` 与 `t27_capture.rdc`。T26 两次 draw 的
  Pipeline State 17/18、Buffer 19、Mesh/Resource usage、红蓝输出；T27 的 Buffer 16/17、
  第二次 offset 16、IA/Mesh/Vertex Buffer usage、红蓝输出均正确。Event/API、资源跳转、
  HTML/CSV 导出和 DDS 保存通过。T26/T27 的 `*-pipeline-final.html`、
  `*-buffer-final.csv`、`*-output-final.dds` 位于 `captures/metal-smoke/`，导出内容已核对；
  两份 capture 均显示 `No problems detected`。
- 最近 L3 决策：indexed、fragment binding、render-pass 条件路径未改；联合定向验证未暴露
  无法圈定的跨场景风险，也未进入发布/合并门槛，故未运行 T00–T27 L3。最近完整
  L3 仍为 T00–T18 的 `/tmp/t18-final-regression.log`。
- 本批规则：终端可判定项已完成；仅待用户同轮 qrenderdoc L4。L3 条件未触发。

### 前批历史证据（BATCH25-26 关闭时）

- 2026-09-24 已关闭 `BATCH25-26.md`：T24 ICB reset 后重编码与 T25 混合命令 ICB 的最终
  联合 L0/L1/L2、逐份 CLI replay、lifecycle 和同一轮最新 qrenderdoc L4 全部通过；
  L3 未触发。T00–T25 纵向切片均已关闭，全部现有未提交改动保留。
- 最近自动证据：最终命令 `/tmp/run-metal-batch25-26.sh`，汇总
  `/tmp/batch25-26-final.log`，运行目录 `/tmp/metal-batch25-26-final.6n0AJS`。T24/T25 与
  T20/T22/T23/T01/T02/T13/T14/T21 的 native、capture/XML、Replay API/output、逐份 CLI
  replay 全部通过；T20/T22/T23/T24/T21/T25 的 6/11/18/8/9/15 类异常拒绝通过。
  含 T00 的 11 份 capture × 10 轮 lifecycle resident growth 1,638,400 bytes；脚本语法、
  Python 编译与 `git diff --check` 通过。
- 最近 L4 证据：最新 qrenderdoc 同一进程依次打开正式
  `captures/metal-smoke/t24_capture.rdc` 与 `t25_capture.rdc`。T24 的 `0+3` range、三个
  展开 draw、replacement 的 Pipeline/Mesh/Buffer/Resource、红/绿/蓝输出与 HTML/CSV 通过，
  旧紫色命令不可见。T25 的非索引/索引两个 action、无 index/UInt16 offset 4 size 6 的两套 IA、
  Mesh、Buffer/Resource、红/绿/蓝输出与 HTML/CSV/DDS 通过。两份均显示
  `No problems detected`。产物为 `t24-pipeline-final.html`、`t24-buffer-final.csv`、
  `t25-pipeline-final.html`、`t25-indexed-pipeline-final.html`、`t25-index-buffer-final.csv`、
  `t25-output-final.dds`。
- 最近 L3 决策：条件路径未触发；联合定向清单已覆盖 reset、混合 commandTypes、direct/
  indirect、indexed 和 ICB 公共路径，未见无法圈定的跨场景风险，也未进入发布/合并门槛，
  因此未运行 T00–T25 L3。最近完整 L3 仍为 T00–T18 的
  `/tmp/t18-final-regression.log`。
- 前一批 BATCH23-24 的自动证据：最终构建 `/tmp/t23-build.log`，联合命令
  `/tmp/run-metal-batch23-24.sh`，汇总 `/tmp/batch23-24-final.log`；T22/T23 与必跑
  T20/T21/T01/T02/T13/T14
  各自 5 帧 native、8 帧 capture、XML、Replay API、CLI replay 均通过。T20/T21/T22/T23
  的 6/9/11/18 类异常拒绝通过，9 份 capture × 10 轮 lifecycle resident growth
  1769472 bytes。T23 的 UInt16 offset 4、baseVertex/baseInstance 1、6-byte index raw 与
  左红右蓝输出通过。脚本语法检查与 `git diff --check` 通过；条件旧场景未触发，
  L3 未触发。
- 前一批 BATCH23-24 的 L4 证据：最新 qrenderdoc 同一进程依次打开与正式 capture SHA-256
  相同的 `/private/tmp/t22-batch-ui.rdc`、`/private/tmp/t23-batch-ui.rdc`。T22 的
  `1+2` range、EID 3/4 独立 Buffer 18/19、Mesh、ICB usage、红蓝输出和 HTML/CSV；
  T23 的 EID 3 indexed ICB、UInt16 index Buffer 18 offset 4/size 6、Buffer 16/17
  Vertex/Instance layout、Mesh 两实例、Index Buffer/ICB usage、红蓝输出和 HTML/CSV/DDS
  均通过。两份状态栏均为 `No problems detected`。T22 UI 产物
  `/private/tmp/t22-pipeline-final.html`、`/private/tmp/t22-buffer-final.csv`；T23 为
  `/private/tmp/t23-pipeline-final.html`、`/private/tmp/t23-index-buffer-final.csv`、
  `/private/tmp/t23-output-final.dds`，均已核对。
- 更早 BATCH21-22 自动证据：`/tmp/run-metal-batch21-22.sh` 对 T01/T02/T05/T13/T14/T20/T21 重跑
  native 5 帧、capture 8 帧、XML、Replay API smoke、逐份 CLI replay；T20 的 6 类、T21 的
  9 类异常 RDC 均被拒绝。含 T00 基线的 8 份 capture × 10 轮 lifecycle resident growth
  196608 bytes。汇总 `/tmp/batch21-22-final.log`，分项 `/tmp/batch21-22-*.log`。最终
  qrenderdoc Viewer 修复构建见 `/tmp/batch21-22-ui-fix-build.log`；修复只涉及 indirect 参数
  标签和 Buffer Viewer 格式，T13 Replay API smoke 复验通过，核心联合结果仍适用。
- 更早 BATCH21-22 L4 证据：最新 qrenderdoc 同一进程打开 SHA-256 与正式 capture 相同的
  `/private/tmp/t20-batch-ui.rdc`、`/private/tmp/t21-batch-ui.rdc`。T20 的 ICB marker/draw、
  IA/Mesh/Buffer/Resource、红色输出与 HTML/CSV 通过；T21 的 indexed action、6-byte index、
  20-byte 五字段参数、IA/Mesh/Buffer/Resource、红蓝输出与 HTML/CSV 通过。额外打开
  `/private/tmp/t13-batch-ui.rdc`，确认非索引 `Draw Primitives` 与四字段格式未回归。三份状态栏
  均为 `No problems detected`。产物 `/private/tmp/t20-batch-buffer-ui.csv`、
  `/private/tmp/t20-batch-pipeline-final.html`、`/private/tmp/t21-batch-arguments-final.csv`、
  `/private/tmp/t21-batch-pipeline-final.html`，内容已核对。
- 更早 BATCH21-22 L3 决策：条件触发的 T16/T19、T06/T07/T08 路径未变；通用 Viewer 修复的风险由
  T13/T21 UI 与 T13 自动状态界定，未见无法圈定的跨场景风险，也未进入发布/合并门槛，
  故未触发 T00–T21 L3。最近完整 L3 仍为 T00–T18 的 `/tmp/t18-final-regression.log`。

## 已完成

- RenderDoc 官方仓库已检出到工作区。
- 已核验标签 `v1.46`，基线提交为 `e4bd23b671d3d5a747ff5221dbe08a63eb6ca200`。
- 已创建本地开发分支 `metal-replay-v1.46`。
- 已确认仓库原有 Metal 骨架位于 `renderdoc/driver/metal`。
- 已完成 Metal 代码和本机工具链的第一轮静态盘点。
- 已建立计划、测试矩阵、决策和交接文档。
- 已安装 Qt 5.15.19、autoconf 2.73、automake 1.19、PCRE 8.45 和 bison 3.8.2。
- 已完成 `ENABLE_METAL=OFF` 的 qrenderdoc Debug 构建并验证主窗口。
- 已为 Xcode 26 / SDK 26 增加最小 Metal bridge 编译兼容补丁。
- 已完成 `ENABLE_METAL=ON` 的 qrenderdoc Debug 构建并重新验证主窗口。
- 已增加可重复构建/启动脚本 `util/buildscripts/scripts/build_metal_dev_macos.sh`。
- 已写好阶段 1 文件级计划 `PHASE1.md`。
- 已将 T00 `Metal_Empty_Frame` 和 T01 `Metal_Simple_Triangle` 接入 `util/test/demos`。
- 两个样例未注入 RenderDoc 时均能稳定运行；T01 的运行时 MSL 编译和三角形绘制通过。
- 修复 macOS 26 `CAMetalLayer` 内部 residency set 与代理 `MTLDevice` 不兼容导致的崩溃。
- 建立 drawable texture 的真实对象/RenderDoc wrapper 映射，render pass 可继续使用受跟踪纹理。
- 修复 app-controlled capture 在 present 时不记录 backbuffer、导致 `EndFrameCapture` 失败的问题。
- T00/T01 均能通过 `DYLD_INSERT_LIBRARIES` 和 in-app API 生成 Metal `.rdc`。
- T00 连续三次 capture 均成功，验证了最小链路稳定性。
- 注册 Metal structured processor；`renderdoccmd convert -c xml` 可解析 capture chunks。
- T01 structured XML 已核验 MSL、入口名、96 字节 vertex buffer、pipeline/binding、draw 和 present。
- 新增一键回归脚本 `util/buildscripts/scripts/test_metal_capture_macos.sh`。
- 已写好阶段 2 文件级计划 `PHASE2.md`。
- `MetalReplay` 已实现 `IReplayDriver` 并注册 `RDCDriver::Metal` replay provider。
- replay 初始化会创建真实 `MTLDevice`，并沿现有 structured 路径读取 capture。
- 已重建 T00/T01 所需的 device、queue、drawable 替代 texture、command buffer、render encoder、
  MSL library/function、pipeline、vertex buffer 和基础 render 命令。
- 已执行 T00/T01 的 clear、pipeline/buffer binding、`drawPrimitives`、commit/wait。
- 已建立 render pass、clear、draw、present 和 capture end 的最小 action/event。
- 已提供基础资源、buffer、texture 描述和 shared buffer 原始字节读取。
- `renderdoccmd replay --loops 1` 已分别对 T00/T01 成功运行并正常退出。
- qrenderdoc 已成功加载 `t01_capture.rdc`。此前的 degraded 弹窗来自 Metal replay 主动声明，
  并非 capture 加载失败或实际回退到软件渲染；完成 Texture Viewer 后已移除该标记。
- 已实现 macOS `CAMetalLayer` output window、尺寸/resize 跟踪、持久 BGRA8 output texture、clear、
  present 和 output readback。
- 已实现最小 fullscreen Metal texture display pipeline，支持 T00/T01 所需的 2D 单采样 color texture、
  fit/scale/pan、mip、flip、channel mask 和 range 映射。
- T00 output readback 得到预期 clear 色；T01 output readback 得到预期黑色背景和彩色三角形。
- qrenderdoc Texture Viewer 已实机显示 T01 三角形；窗口最大化后 resize/fit 仍正确，状态栏为
  `No problems detected`。
- Texture Viewer 闭环通过后已将 `APIProperties.degraded` 改为 `false`，新构建不再弹 degraded
  support 警告。
- `test_metal_capture_macos.sh` 现会编译离屏 output smoke、输出 T00/T01 PPM，并断言关键像素。
- 已保留 `CaptureBegin` 后的 frame stream，并为每个 API event 记录 frame-relative file offset。
- 已实现 T00/T01 所需的 `ReplayLog(Full/WithoutDraw/OnlyDraw)` 区间执行和未闭合 Metal
  encoder/command buffer 的安全收尾。
- 自动化会在同一个 replay controller 上连续 10 轮执行 T01 `clear -> draw -> clear`，中心像素依次为
  `#14141a -> #83645f -> #14141a`，验证前进和回退均会真实重放。
- qrenderdoc 实机切换 EID 1（clear）、EID 2（draw）、EID 1（clear）时画面分别为纯背景、彩色
  三角形、纯背景，状态栏始终为 `No problems detected`。
- 已实现 capture texture 的 `GetTextureData()`：通过 Metal blit 将单采样 2D RGBA8/BGRA8 mip
  复制到 shared buffer，并移除 Metal 行对齐 padding 后返回紧密排列的原始字节。
- 已基于同一 readback 路径实现 `PickPixel()`，按 RGBA/BGRA 通道顺序返回归一化浮点值；超出当前
  范围的 remap、resolve、MSAA、array/cube/3D、depth/stencil、压缩及其他格式均明确报 unsupported。
- T01 自动验证了 clear EID 的 400x300 BGRA backbuffer、背景字节 `1a1414ff`、背景拾取值约
  `(0.08, 0.08, 0.10, 1.0)`，以及 draw EID 中心像素不再是背景色。
- `ReplayController::SaveTexture()` 已使用同一路径成功保存 T01 为 400x300 ARGB8888 DDS，产物
  `captures/metal-smoke/t01_texture.dds` 为 480128 字节。
- 已把 `ShaderEncoding::MSL` 接入通用 replay/API 字符串与 qrenderdoc 源码高亮；source-created
  library 的源码会随 replay library 保存，并为 Metal function 生成真实 entry point、stage 和
  `ShaderReflection`，不为没有源码的 binary library 伪造 MSL。
- T01 自动断言恰有 `vs_main`/Vertex 与 `fs_main`/Fragment 两个 shader resource，两者均返回
  `captured.metal`、MSL encoding 和同一份包含真实入口声明的源码。
- qrenderdoc 已从 Resource Inspector 分别打开 Function 13/14；两个 Shader Viewer 均显示
  `captured.metal` 的真实 `metal_stdlib`、vertex/fragment 源码，状态栏保持 `No problems detected`。
- 修复 qrenderdoc 恢复到旧 D3D11 Pipeline State 子页面后加载 Metal capture 会错误调用 D3D11
  controller 并崩溃的问题：非 D3D/GL/Vulkan API 现在清空旧后端子页面，Metal capture 可稳定加载。
- 已新增独立 `MetalPipe::State`，并通过 replay controller、proxy serialization 和通用 `PipeState`
  暴露 T01 所需的 render pipeline、vertex/fragment shader、topology、vertex buffer 和 color target；
  不把 Metal 状态伪装成 D3D/GL/Vulkan。
- replay 会在 render pass、pipeline/buffer bind 和 draw 时更新 Metal snapshot；T01 自动断言
  `Pipeline State 15`、`Function 13/14`、`vs_main/fs_main`、`TriangleList`、96-byte `Buffer 16` 和
  swapbuffer `Texture 23` 均来自真实 replay 资源。
- qrenderdoc 已接入最小 Metal Pipeline State 页面。实机重新启动本次构建、加载最新 T01 并选择
  EID 2 后，页面显示上述 pipeline/shader/topology/VB/color target，状态栏为
  `No problems detected`；各资源行可进入 Resource Inspector。
- replay resource manager 现会确定性释放 wrapper 与真实 Metal 对象；wrapper 区分 `new*` 转移来的
  retained 对象和需要主动 retain 的 autoreleased command buffer/render encoder，并在重复 replay
  替换 live 对象时释放旧引用。ObjC embedded bridge 仅保留在 capture 路径。
- 新增 `metal_replay_lifecycle_smoke.mm`，在同一进程中打开/关闭 T00/T01 各 10 次并覆盖 action、
  texture readback 与 unsupported 接口；完整回归在两轮 warm-up 后 resident growth 为 491,520 字节，
  额外 50 轮压力检查增长 1,441,792 字节。
- shader debug 不再返回会被控制器解引用的空指针；四种 debug 调用返回可释放的空 trace。histogram、
  pixel history、post-VS 和 custom/target shader build 也返回稳定空结果或明确错误。
- 最新 qrenderdoc 同一进程完成 `T01 -> Close -> T00 -> Close -> T01`。重开后的 T01 EID 2 仍显示
  彩色三角形和完整最小 Pipeline State；T00/T01 状态栏均为 `No problems detected`，History/Debug
  按钮明确提示不支持并保持禁用。
- 已新增 `PHASE3.md`，将 T02 拆为 fixture、capture/resource、GPU replay/action、UI/data 和阶段关闭。
- 已新增确定性的 `Metal_Indexed_Cube`（T02）：一个 interleaved Float3 position/Float4 color vertex
  buffer、36 个 UInt16 index 和同内容的 UInt32 index、private `Depth32Float` attachment、less/write
  depth state，以及左右两个 viewport/scissor 的 indexed cube。
- 新增 `WrappedMTLDepthStencilState` 及 Objective-C bridge，序列化/重建
  `newDepthStencilStateWithDescriptor` 和 `setDepthStencilState`；descriptor 当前真实保存 label、depth
  compare 和 depth write，stencil face 明确保留为后续范围。
- 已接通 T02 所需的 `setScissorRect`、`setFrontFacingWinding`、`setCullMode` 和直接
  `drawIndexedPrimitives` capture/replay；indexed action 标记 `ActionFlags::Indexed` 并保存 count/offset。
- 修复 render pass 只引用 color attachment 的缺口：capture 现在统一追踪 color/depth/stencil 及
  resolve texture，因此 T02 private depth texture 的创建 chunk 会进入 `.rdc`，replay depth target 非空。
- 一键脚本现会原生运行并重新截取 T02，XML 断言 stride/attribute/depth/raster/scissor/index 参数，
  replay smoke 逐字节比较 72-byte UInt16 与 144-byte UInt32 index buffer，并检查左右半屏图像。
- P3.2 完整回归通过 T00/T01/T02 capture/replay 与三份 capture 各 10 次同进程开关；warm-up 后
  resident growth 为 1,196,032 字节，低于 64 MiB 阈值。
- 最新 qrenderdoc 已加载 T02，Event Browser 显示 EID 2/3 两个 `drawIndexedPrimitives(36)`；选中
  EID 3 后 API Inspector 标明 UInt32/Buffer 20，Texture Viewer 同时列出 color Texture 27 与 depth
  Texture 17，双立方体图像正确，状态栏为 `No problems detected`。
- `MetalPipe::State` 已扩展 vertex attributes/layout、index buffer、depth state 和 raster state，并通过
  通用 `PipeState` 暴露 index、viewport/scissor、depth/raster 查询；proxy serialization 同步覆盖。
- 修复 pipeline state 的 event 边界：加载 capture 时按 action event 保存 draw-time snapshot，避免
  `OnlyDraw` 区间继续执行到下一 event 前时把第一条 draw 的动态状态覆盖为第二条 draw 状态。
- T02 replay smoke 现逐 draw 断言 Float3/Float4、stride 28、UInt16/UInt32 binding、less/write、
  back/CCW、左右 viewport/scissor，并验证 clear -> draw1 -> draw2 -> draw1 回退的 BGRA 图像。
- 最新完整回归重新生成 T00/T01/T02 并全部通过；三份 capture 各 10 次 lifecycle 的 resident growth
  为 720,896 字节。最新 qrenderdoc 中 EID 2/3 Pipeline State 分别显示左/右 viewport 与
  `Buffer 19 / 72 / UInt16`、`Buffer 20 / 144 / UInt32`，其余 vertex/depth/raster/target 字段一致，
  状态栏为 `No problems detected`。
- `PipeState::GetVertexInputs()` 已映射 Metal attribute/layout，标准 Mesh Viewer 能按 T02 UInt16/UInt32
  index 展开 `attr0` Float3 与 `attr1` Float4；Metal output backend 可绘制当前 VS Input Float2/3/4
  点线/三角拓扑，自动 smoke 会检查 indexed cube 线框产生真实非背景像素。
- Pipeline State 的 vertex attribute 激活现进入 Mesh Viewer；vertex buffer 使用通用自动格式进入
  Buffer Viewer，index buffer 分别用 `ushort index`/`uint index` 查看精确子范围。Post-VS 输出表明确
  显示 Metal 不支持，不返回伪数据。
- P3.4/P3.5 最新完整脚本通过，三份 capture 各 10 次 lifecycle 的 resident growth 为 294,912 字节。
  最新 qrenderdoc 实机验证 EID 2 的 VS Input 表格/立方体线框、Buffer 18 interleaved 数据、Buffer 19
  UInt16 数据及 EID 3 Buffer 20 UInt32 数据，状态栏保持 `No problems detected`。
- 已新增确定性的 `Metal_Textured_Quad`（T03）：4x4 RGBA8 四色块纹理、Float2 position/UV、
  triangle strip 与 nearest/clamp sampler，原生运行、capture 和 structured XML 参数断言均通过。
- 已实现单层 2D `MTLTexture::replaceRegion` capture/replay，以及完整 wrapped sampler resource 的
  创建、序列化、重建和释放；`setFragmentTexture`/`setFragmentSamplerState` 会真实 replay 并更新状态。
- T03 replay smoke 精确验证 64-byte RGBA8 内容、四个 texel 的 `PickPixel()`、四象限 GPU 输出、
  `TriangleStrip`、Float2/Float2 input 和 fragment slot 0 的 texture/sampler。
- fragment texture/sampler 已经通过通用 descriptor API 暴露给 `PipeState`。Metal Pipeline 页面显示
  Fragment Textures/Samplers；texture 双击进入标准 Texture Viewer，sampler 进入 Resource Inspector。
- 最新 qrenderdoc 已加载 T03 EID 2，显示 Texture 17、Sampler 18（Point/Point/None、ClampEdge x3）和
  正确四色纹理；Texture Viewer/Resource Inspector 跳转均通过，状态栏保持 `No problems detected`。
- Metal Pipeline 页面已改用标准 Controls + `PipelineFlowChart` + 隐藏阶段页，形成 IA/VS/RS/FS/OM
  信息架构；`SelectPipelineStage()` 现在会切换对应页面，shader 行直接进入通用 Shader Viewer。
- `Show Empty Items` 会对已知空 binding/target/state 显示标准红色空槽；没有静态 shader resource
  reflection 的 pipeline 会禁用 `Show Unused Items` 并说明原因，避免将“已绑定”错误解释为“shader 使用”。
- 最终构建已分别用 T03/T02 实机核对新布局：T03 FS 显示 Texture 17/Sampler 18，IA/OM 空槽开关与
  shader 直接跳转正常；T02 IA、RS、OM 分别保持 UInt16 输入、左 viewport/scissor + Back/CCW、
  Texture 27 + Depth Texture 17 + Less/Write。两份 capture 状态栏均为 `No problems detected`。
- Metal Pipeline 的所有表已迁移到标准 `RDTreeWidget`/`RDHeaderView`，并通过父级
  `SetupResourceView()` 复用资源上下文菜单、usage、thumbnail/preview；Metal ResourceId 已补入三个
  通用分发点。最终 T03 FS 表双击 Texture 17 仍进入标准 Texture Viewer。
- 工具栏已加入标准 Export 控件；最终 qrenderdoc 在 T03 EID 2 实际写出
  `/tmp/metal-pipeline-t03.html`，文件包含 IA/VS/RS/FS/OM、Triangle Strip、Texture 17 和 Sampler 18。
  随后完整 T00-T03 回归通过，四份 capture 各 10 次 lifecycle resident growth 为 524,288 字节。
- replay 创建 Metal render pipeline 时会请求 argument reflection；T03 fragment shader 现枚举真实
  `colourTexture`/`colourSampler`、slot 0、类型、只读状态和 active 状态，并接入通用
  `ShaderReflection`/descriptor 查询。无反射证据的 pipeline 继续保守降级。
- T03 fixture 将同一 texture/sampler 额外绑定到 shader 未声明的 slot 1。自动回归验证 slot 0 映射
  reflection index 0 且为 used，slot 1 为 `NoShaderBinding + staticallyUnused`，`onlyUsed=true` 仅返回
  slot 0。最终 qrenderdoc 默认隐藏 slot 1；启用 `Show Unused Items` 后 texture/sampler 表均显示真实
  slot 1，状态栏为 `No problems detected`。最新完整 lifecycle resident growth 为 540,672 字节。
- 已新增 `Metal_Dynamic_Uniform`（T04）：单个 512-byte shared buffer 在 offset 0/256 保存两组固定
  float4 颜色，两条 fullscreen triangle draw 通过左右 viewport 输出红/绿半屏。
- 已完整 capture/replay `setFragmentBuffer` 与 `setFragmentBufferOffset`；structured XML 保存
  512-byte 初始内容、slot 0、offset 0/256 和两条 draw，事件回放验证 clear -> 左红 -> 左红右绿 ->
  左红往返。
- Metal pipeline state 新增 fragment buffer binding，并通过通用 constant-block descriptor/reflection
  枚举 `uniforms`、slot 0、16 bytes 和 active 状态；FS 页复用标准 RDTree 与 Buffer Viewer 跳转。
- 完整 T00-T04 一键回归通过；五份 capture 各 10 次 lifecycle resident growth 为 376,832 字节。
  最终 qrenderdoc 的 T04 EID 2/3 分别显示左红/右背景和左红/右绿，Constant Buffers 分别显示
  `Buffer 16 / 0 / 512` 与 `Buffer 16 / 256 / 256`。EID 3 双击进入 Buffer Viewer 的 offset 256、
  length 256 子范围，状态栏为 `No problems detected`；当前进程保持运行在 EID 3 FS 页。
- 已新增 `Metal_Instanced_Mesh`（T05）：24-byte Float2 position buffer 与 96-byte
  Float2 offset/Float4 colour instance buffer 分居 slot 0/1；直接 draw 使用
  `instanceCount=3/baseInstance=1` 输出红、绿、蓝三个固定实例。
- T05 structured XML、action 和 draw-time snapshot 均保存两个 vertex slot、stride 8/24、
  `PerVertex/PerInstance` step、instance count 3 与 base instance 1；自动回归逐字节验证两份 buffer，
  检查 clear/draw 往返、三处 `PickPixel()` 和最终 PPM `ff2010/10df30/1840ff`。
- 通用 `GetVertexInputs()` 与标准 Mesh Viewer 会应用 action 的 base instance：instance 0/1 分别显示
  offset `-0.55/0.00`，instance 1 colour 为 `.0625/.875/.1875/1.0`；VS Input 预览显示原始 attr0
  三角形，shader 变换后的 geometry 继续明确属于 post-VS unsupported。
- 完整 T00-T05 一键回归通过；六份 capture 各 10 次 lifecycle resident growth 为 573,440 字节，
  六份 capture 的 `renderdoccmd replay --loops 3` 与 `git diff --check` 均通过。最终 qrenderdoc 的
  T05 EID 2 正确显示三色实例，Pipeline IA 显示 Buffer 16/17 的 Vertex/Instance step，两个标准
  Buffer Viewer 与 Mesh Viewer 实例切换均正确，状态栏为 `No problems detected`；进程保持运行。
- 已新增 `Metal_MRT_Blend`（T06）：BGRA8 drawable 与 shared RGBA8 第二附件使用两条 draw；slot 0
  开启 `SourceAlpha/OneMinusSourceAlpha` RGB blending，slot 1 禁用 blending 并使用 RGB write mask。
- 修复 render pipeline color attachment capture 将 source RGB factor 错取为 source alpha factor 的
  字段错误；draw/clear/end-pass action 现保存全部 color outputs，Metal snapshot 与通用
  `GetColorBlends()` 返回每个 attachment 的 blend equation/write mask。
- Pipeline OM 页新增标准 RDTree Blend State 表，字段顺序对齐现有 GL/Vulkan 页面；Color Targets、
  Texture Viewer Outputs、资源激活与 HTML export 均复用公共路径，不增加 Metal 专用查看器。
- 完整 T00-T06 一键回归通过；七份 capture 各 10 次 lifecycle resident growth 为 1,015,808 字节，
  七份 capture 的 `renderdoccmd replay --loops 3` 与 `git diff --check` 均通过。最终 qrenderdoc 在 T06
  EID 3 显示 FB0 混合结果、FB1 RGB write-mask 结果以及两行正确 OM blend state；实际导出
  `captures/metal-smoke/t06_pipeline_state_standard.html`，状态栏为 `No problems detected`；进程保持运行。
- 已新增 `Metal_Depth_Stencil`（T07）：单一 `Depth32Float_Stencil8` attachment、两个 pipeline、两个
  depth-stencil state 和五条 draw，分别建立左右 stencil mask、通过 depth/stencil 的绿/蓝结果及一次
  可观察的 depth fail。
- 已完整保存/重建 front/back stencil compare、fail/depth-fail/pass operations、read/write masks 与
  single/dual dynamic reference；render-pass clear action 标记 `ClearDepthStencil`，draw action 保存真实
  `depthOut`，Metal snapshot/proxy serialization 与通用 `StencilFace` 同步更新。
- Pipeline OM 新增标准 `Stencil State` RDTree，并与 Depth Target/Depth State、Texture Viewer 和 HTML
  export 复用公共状态；共享 `PipelineFlowChart` 新增焦点及 Left/Right/Home/End 导航，继续向其他图形
  API 的标准交互收敛。
- 完整 T00-T07 一键回归通过；八份 capture 各 10 次 lifecycle resident growth 为 524,288 字节，八份
  capture 的 `renderdoccmd replay --loops 3` 均通过。最终 qrenderdoc 在 T07 EID 6 显示左绿右蓝输出、
  Texture 20、Less/Write Enabled 与正确 Front/Back stencil 状态；实际导出
  `captures/metal-smoke/t07_pipeline_state_standard.html`，状态栏为 `No problems detected`；进程保持运行。
- 已新增 `Metal_MSAA_Resolve`（T08）：4x BGRA8 multisample color attachment 显式 resolve 到 drawable，
  三条 draw 形成左红、右蓝与中央绿色叠加三角形；pipeline 同时覆盖 sample count 和
  alpha-to-coverage。
- replay texture description 现在区分 `Texture2DMS`；draw-time snapshot 保存 sample count、
  alpha-to-coverage/one、MSAA color target 与单采样 resolve target，action output 指向可显示的真实
  resolve 资源，不伪造 MSAA readback。
- Pipeline OM 新增标准 `Multisample State` 与 `Resolve Targets` RDTree，Color Targets 增加 Samples；
  Resolve Targets 复用标准 Texture Viewer 跳转，五阶段 HTML export 输出同一状态。
- 完整 T00-T08 一键回归通过；九份 capture 各 10 次 lifecycle resident growth 为 376,832 字节，九份
  capture 的 `renderdoccmd replay --loops 3` 均通过。最终 qrenderdoc 在 T08 EID 4 显示
  `Texture 17 / Texture 2D MS / 4 samples`、`Texture 24 / Texture 2D / 1 sample` 和红绿蓝 resolve
  图像，中心拾取为 `(0.06275, 0.87451, 0.18824, 1.00)`；实际导出
  `captures/metal-smoke/t08_pipeline_state_standard.html`，状态栏为 `No problems detected`；进程保持运行。
- 已新增 `Metal_Texture_Subresources`（T09）：3-mip RGBA8 2D、3-slice 2D array 和六面 cube；
  12 个固定色子资源通过 12 条屏幕色带采样，native 输出与 capture structured XML 均通过。
- 新增 slice-aware `replaceRegion` capture/replay chunk，纹理 descriptor 保留 cube/array 语义；
  `GetTextureData()`、`PickPixel()` 和标准 output renderer 按 mip/slice/face 读取、显示并拒绝越界组合。
  自动 smoke 逐一检查 12 个原始子资源、六面 cube pick、每个 display、12 条色带和 512-byte cube DDS。
- T09 EID 2 的 Pipeline FS 显示 Texture 17/18/19（2D/2D Array/Cube）与 Sampler 20；标准 Texture
  Viewer 实机显示 2D mip0/1/2 红/绿/蓝、array slice0/1/2 黄/品红/青与 cube face X+/X-/Z- 的
  不同固定色。Qt 5/macOS 26 的 combo popup 会在 Cocoa 插件崩溃，两个子资源选择控件改为点击
  循环、键盘方向键/Home/End 选择，实机切换与状态栏均通过。
- 完整 T00-T09 一键回归通过；十份 capture 各 10 次 lifecycle resident growth 为 475,136 字节。
  最新 qrenderdoc 从 T09 EID 2 的 cube Z- 保存全部 faces 到
  `captures/metal-smoke/t09_cube_ui.dds`，与自动保存产物均为 512-byte DDS 且 `cmp` 完全一致；状态栏为
  `No problems detected`。因 macOS `Documents` 目录下的 capture 直接打开偶发阻塞，L4 使用
  同一最新 `.rdc` 的字节拷贝 `/tmp/t09-ui-capture.rdc`；qrenderdoc 保持运行。
- 已新增 `Metal_Blit_Operations`（T10）：固定 64-byte shared buffer 执行 offset 8 到 0 的
  32-byte copy，并对 destination 16..31 fill `0x60`；8x8 RGBA8 四象限 texture 执行 texture copy，
  另一张 4-mip texture 生成 mip chain。最终 draw 分四条色带独立采样四种结果，native 自检通过。
- 已补齐 blit encoder 创建/end、buffer copy/fill、texture region copy 和 mipmap generation 的
  capture/replay；buffer offset/length 与 texture mip/slice/origin/size/format 均在执行前验证。每个操作
  生成独立 action/event，并通过 `CopySrc`/`CopyDst`/`Clear`/`GenMips` usage 接入通用资源路径。
- T10 定向 smoke 已核对 structured XML、event 顺序与资源链接、copy/fill 前进和回退、buffer/texture
  原始数据、mip1/mip3、最终四条色带和 468-byte 全 mip DDS。T03/T09 texture 定向 replay/output 与
  T00/T03/T09/T10 各 10 次定向 lifecycle 均通过。
- 最终代码的 T00-T10 一键回归复验通过；十一份 capture 各 10 次 lifecycle resident growth 为 1,277,952
  字节，逐份 `renderdoccmd replay --loops 1` 均通过。最新 qrenderdoc 的 Event Browser 显示
  `Copy/Clear Pass #1` 和 EID 1-6 blit 子事件；Resource Inspector 的 Buffer 18 为 EID 2
  `Copy - Dest`、EID 3 `Clear`，Texture 21 为 EID 5 `Generate Mips`。Buffer Viewer 显示
  copy/fill 前后字节，Texture Viewer 显示 Texture 20 四象限、Texture 21 mip3 拾取
  `(134,132,88,255)` 和 EID 8 四条结果色带；UI/自动两份 468-byte DDS 逐字节一致，
  状态栏为 `No problems detected`。D034 修正的组名与 usage 文案已用最新 app 包内库实机确认。
- 已新增 `Metal_Compute_Texture_Filter`（T11）：8×8 RGBA8 source/destination、`filter_main`、
  `2×2×1` threadgroups / `4×4×1` threads/group，B/R/G swizzle 后由 render draw 采样。
  compute pipeline/encoder wrapper、capture/replay chunk、dispatch action/usage、compute shader reflection、
  标准只读/读写 descriptor 与 Metal Pipeline CS 页均已接通。
- `/tmp/t11-final-regression.log` 记录 T00-T11 原生运行、重新 capture、XML、输出 smoke、逐份 CLI
  replay 和 12×10 lifecycle 全部通过，resident growth 999,424 字节。T11 smoke 验证 dispatch
  前全零、后全部 256 字节 CPU swizzle、回退再前进、最终画面及 384-byte DDS；T03/T09/T10 定向输出
  验证也通过。
- 最新 qrenderdoc 加载与正式 capture SHA-256 一致的 `/tmp/t11-ui-capture-final.rdc`；Event Browser
  显示 EID 1-3 Compute Pass、EID 2 dispatch 和 EID 5 final draw。CS 页显示 Compute Pipeline
  State 16 / Function 13 `filter_main`、slot 0 Texture 19 只读、slot 1 Texture 20 读写；资源跳转进入
  标准 Texture Viewer，Resource Inspector 分别显示 `CS - Texture`/`CS - Image/SSBO`。
  EID 1 目标纹理全黑，EID 2 的首像素拾取约 `(0.06275,0.12549,0.09412,1.0)`；EID 5 的
  final draw 采样 Texture 20，画面与参考一致。UI DDS `captures/metal-smoke/t11_filtered_ui.dds`
  与自动 DDS 均为 384 字节且 `cmp` 一致；`/tmp/t11_pipeline_state_standard.html` 包含 shader、
  Texture 19/20。状态栏为 `No problems detected`。
- 已完成 T12 单层 fragment argument buffer 与 T13 单次非索引 indirect draw。T13 使用 48-byte
  shared buffer 中 offset 16 的 16-byte `3/2/1/1` 参数，原生和 replay 左橙右蓝图像一致；
  action、usage、Pipeline `Indirect Buffer`、标准 Buffer Viewer、Resource Inspector 和 raw `.bin`
  均指向 Buffer 18。offset 17/36 派生 RDC 分别验证未对齐/越界拒绝。
- `/tmp/t13-final-regression.log` 记录 T00-T13 native/capture/XML/output/data/state、逐份 CLI replay
  与 14×10 lifecycle 通过，resident growth 1,294,336 bytes。最新 qrenderdoc 实机显示 EID 2
  indirect draw、16/16 参数区和 `3/2/1/1`；UI/自动 `.bin` 逐字节一致，Pipeline HTML 包含
  Indirect Buffer，状态栏为 `No problems detected`。
- T14 `Metal_Indexed_Instancing` 使用 UInt16 index buffer 的 byte offset 4、baseVertex 1、
  baseInstance 1、两个实例；位置/实例哨兵使三字段任一失效都无法得到左红右蓝输出。新 overload
  已完成 bridge、capture chunk、序列化、GPU replay、`Indexed|Instanced` action、精确 6-byte index
  binding、vertex/index usage、标准 Mesh/Buffer Viewer。XML、buffer 数据、clear/draw/回退像素和
  raw index export 自动断言通过；offset 3（未对齐）与 10（越界）的派生 RDC 均被 replay 拒绝。
- `/tmp/t14-final-regression.log` 记录 T00-T14 native/capture/XML/output/data/state、逐份 CLI replay
  和 15×10 lifecycle 全部通过，resident growth 1,015,808 bytes。最新 qrenderdoc 实机打开正式
  `t14_capture.rdc`，Event/API EID 2、IA Buffer 16/17/18、Index Buffer offset 4/length 6、
  Resource Inspector `Index Buffer` usage、Mesh instance 0/1、左右像素、标准 HTML/CSV 导出均正确；
  状态栏 `No problems detected`。产物 `captures/metal-smoke/t14_indices.bin`、
  `t14_indices_ui.csv`、`t14_pipeline_state_standard.html` 均已验证。
- T15 `Metal_Point_Line` 的 Point `(start=1,count=1)`、Line `(3,2)`、Line Strip `(6,3)`
  使用 264-byte interleaved Float2/Float4 buffer 和视口外哨兵。原生 BGRA 像素、RDC XML、
  action/topology、VS Input、Buffer 字节、Mesh preview、Vertex Buffer usage、clear/draw/回退和
  480128-byte DDS 均通过；非法 primitive 99 与零 vertexCount 派生 RDC 被 replay 拒绝。
- `/tmp/t15-final-regression.log` 记录 T00-T15 原生/capture/XML/output/data/state、逐份 CLI replay
  及 16×10 lifecycle 全部通过，resident growth 737280 bytes。正式
  `captures/metal-smoke/t15_capture.rdc` SHA-256 为
  `208b794cc2ab49cf88f970cdda787bfb016f3483284b389130e05a39bae1a614`。
  最新 qrenderdoc 完全重启后实机核对三种 action 与 EID 2/3/4 拓扑、Line Strip API 参数、
  Point/Line/Line Strip Mesh VS Input、标准 Buffer Viewer 全部顶点值、Resource Inspector
  `Vertex Buffer`、Texture Viewer 红点绿线蓝折线、Pipeline HTML Line Strip/Buffer 16，状态栏
  `No problems detected`。UI/自动 DDS 均为 480128 bytes，SHA-256 同为
  `4bedcfdcddf745e379a2f693eac75a5dedec53df32ffba7ddad9011f60de0a5c`；
  产物为 `t15_output_ui.dds`、`t15_pipeline_state_standard.html`。
- T16 `Metal_Vertex_Texture` 使用 384-byte Float2/Float2 buffer、2×2 RGBA8 texture 与
  Point/Clamp sampler，在 vertex shader 采样并输出四象限纯色。原生 readback、XML、直接
  vertex texture/sampler bridge/chunk/frame reference/GPU replay、event snapshot、VS reflection/
  descriptor、`VS_Resource` usage、资源字节、VS Input、clear/draw/回退及 DDS 自动断言均通过。
  T03/T12 fragment 定向 smoke 通过；slot 128 与空 texture/sampler 的四份派生 RDC 在对应
  chunk 被拒绝。
- `/tmp/t16-final-regression.log` 记录 T00-T16 原生/capture/XML/output/state、逐份 CLI replay
  和 17×10 lifecycle 全部通过，resident growth 114688 bytes。最新 qrenderdoc 实机使用与
  `captures/metal-smoke/t16_capture.rdc` SHA-256 同为
  `2f1b95128946d180d7f114ca9047bb2b90f28d43915e82ad9eca2925ff788ec5` 的
  `/tmp/t16-ui-capture.rdc`：Event/API EID 2、VS Texture 17/Sampler 18、Mesh VS Input、
  384-byte 标准 Buffer Viewer、Texture 17/Resource Inspector `VS - Texture`、四象限输出与
  状态栏 `No problems detected` 均通过。HTML 为
  `captures/metal-smoke/t16_pipeline_state_standard.html`；UI/自动 480128-byte DDS 的
  SHA-256 同为 `95597aa5bcb91a27f057df2f4f36cbeb49423ac7e5275d5ed17ca29b03ca21cf`。
  验证命令：`bin/demos_x64 Metal_Vertex_Texture --frames 5`；
  `build-macos-debug/metal_replay_output_smoke captures/metal-smoke/t16_capture.rdc /tmp/t16-directed.ppm /tmp/t16-directed.dds`
  （T03/T12 使用相同 smoke 的对应 capture）；
  `RENDERDOC_METAL_CAPTURE_DIR=/tmp/t16-final-regression-captures util/buildscripts/scripts/test_metal_capture_macos.sh > /tmp/t16-final-regression.log 2>&1`。
- T18 `Metal_Fragment_Storage_Buffer` 使用 640-byte shared buffer，在 byte offset 256 的
  fragment `device const float4 *` 读取四组颜色，物理 slot 3，未绑定 slot 5。native BGRA、
  capture/XML、storage reflection/descriptor、`PS_Resource` usage、事件 seek、640-byte raw export、
  哨兵和异常 offset 均经自动 smoke 验证；T04/T12/T10 定向通过。
- `/tmp/t18-final-regression.log` 记录 T00-T18 全量原生/capture/XML/output/state、逐份 CLI replay
  与 19×10 lifecycle 全部通过，resident growth 1,556,480 bytes；`git diff --check` 通过。
  最新 qrenderdoc 加载与正式 capture SHA-256 同为
  `4e8436d6c9859c9dcf8ba57273dd9b1515cfa3f641339bf6219ba2ffec247aa8` 的
  `/tmp/t18-ui-final.rdc`，EID 2 的 FS Storage Buffers slot 3/offset 256/size 384、
  标准 Buffer Viewer 四组值、Resource Inspector `FS - Resource`、HTML/CSV 保存与状态栏
  `No problems detected` 均通过。UI CSV 前四行与自动 raw export 对应字节一致。
  产物位于 `captures/metal-smoke/t18_capture.rdc`、`t18_storage.bin`、
  `t18_storage_ui.csv`、`t18_pipeline_state_standard.html`。UI 二进制菜单项未单独点击；
  raw 内容由自动 smoke 验证。
- T19 `Metal_Vertex_Storage_Buffer` 使用 768-byte shared buffer，在 byte offset 256 保存 24 个
  `float4` 位置；vertex shader 从 slot 4 读取，slot 6 绑定相同资源但静态未使用。自动 smoke 核对
  四象限 native/replay、XML、reflection/descriptor、`VS_Resource`、IA/storage 分类、
  clear/draw/回退、完整 raw 和异常 slot/offset。最终 T19/T18/T16/T02/T05 定向、CLI replay、
  T19 10 轮 lifecycle 和 `git diff --check` 通过；日志为 `/tmp/t19-final-t19.log`、
  `/tmp/t19-final-t18.log`、`/tmp/t19-final-t16.log`、`/tmp/t19-final-t02.log`、
  `/tmp/t19-final-t05.log`、`/tmp/t19-final-cli.log` 与 `/tmp/t19-final-lifecycle.log`。
  分类公共路径风险已由 T02/T05 覆盖，未触发 T00-T19 L3。最新 qrenderdoc L4 核对 EID 2、
  `drawPrimitives(24)`、IA 空表、VS Storage Buffers slot 4/6、256+512 Buffer 范围、
  Resource Inspector `VS - Resource`、四象限、HTML/CSV/bin 与 `No problems detected`；
  512-byte UI bin 与自动 raw 对应子范围一致。

## 已验证环境

| 项目 | 当前值 | 结论 |
| --- | --- | --- |
| macOS | 26.1 / Build 25B5042k | 目标主机 |
| 架构 | arm64 | Apple Silicon |
| Xcode | 26.0.1 / Build 17A400 | 高于仓库最低要求 12.2 |
| macOS SDK | 26.0 | 需要留意 v1.46 与新 SDK 的兼容差异 |
| CMake | 4.4.3 | 高于 Apple 构建最低要求 3.23 |
| Ninja | 1.13.2 | 可用 |
| Qt 5 qmake | 5.15.19 | 已验证 |
| autoconf | 2.73 | 已验证 |
| automake | 1.19 | 已验证 |
| PCRE | 8.45 | 已验证；CMake 当前仍选择本地构建副本 |
| bison | 3.8.2 | 已验证 |

Qt 5 会警告它只测试到 macOS SDK 14，当前 SDK 26 属于 Qt 未验证组合，但实际编译和窗口启动已
通过。SWIG 配置期间 macOS 打印过缺少 Java Runtime 的提示，后续 SWIG build/bindings 生成仍成功。

## 代码基线发现

- `renderdoc/driver/metal` 顶层源文件总计约 15,395 行。
- `METAL_NOT_HOOKED()` 约 237 处。
- TODO/FIXME/未实现/未处理类标记约 402 处。
- `MetalReplay` 已继承 `IReplayDriver`，但大量进阶接口仍明确返回 unsupported/空结果。
- Metal replay provider 已注册，T00/T01 可进入真实 GPU replay 初始化。
- `WrappedMTLDevice::ProcessChunk()` 只覆盖一小部分 device/resource/render encoder chunk。
- `WrappedMTLDevice::AddAction()` 和 `AddEvent()` 已接入最小 frame record。
- qrenderdoc 已有 macOS `CAMetalLayer` 输出窗口适配，能作为后续 replay output 的基础。

## 当前阻塞与风险

1. Qt 5.15 对 SDK 26 给出未验证警告；原生 combo popup 在 Cocoa 插件崩溃。T09 的 mip/slice/face
   控件已用 macOS 点击循环和键盘选择避开 popup，其他 combo 仍需后续 UI 回归关注。
2. SDK 26 新增的 Metal 协议方法目前由 Objective-C forwarding 转交真实对象，尚未被 capture。
3. Metal replay 不是局部补丁：接口注册、事件模型、资源读取、状态快照和输出都需要实现。
4. Texture Viewer 和 texture readback 当前支持单采样 RGBA8/BGRA8 2D，以及 T09 的 RGBA8
   mipmapped 2D、2D array 和 cube；T08 可显示单采样 resolve 结果。逐 sample/MSAA attachment、
   cube array/3D、depth/stencil、整数、浮点和压缩格式 readback 仍明确不支持。
5. 当前 drawable hook 只验证了本机 `CAMetalDrawable` 具体类；后续需要覆盖多屏/不同 GPU 可能出现
   的其他 drawable class。
6. 当前 event-range replay 已覆盖 T00-T13 的单 command buffer，并包含 T10 blit→render、
   T11 compute→render 与 dispatch 前后/回退纹理字节、
   T02 indexed、T04 dynamic offset、T05 instanced draw、T06 双附件 blend、T07 depth/stencil
   五 draw 与 T08 三次 MSAA resolve draw 的前进/回退专项断言；多 command buffer、多 render
   pass、嵌套 debug group 和
   load-action initial contents 仍需按后续样例扩展。
7. `OnlyDraw` 遵循 RenderDoc 控制器约定，依赖紧邻的 `WithoutDraw` 建好同一 encoder 的前置状态；
   当前不承诺把 `OnlyDraw` 当作独立入口调用。
8. T00-T13 的 replay wrapper/Metal object 释放已经完成并通过完整循环测试；后续 wrapper 必须继续
   遵守 D017 的 transferred/retained 所有权规则，避免重新引入双重释放或泄漏。
9. 当前 Metal Pipeline State 承诺 T01 基础字段、T02 的 Float3/Float4 vertex descriptor、UInt16/32
   index/depth/raster、T03 fragment texture/sampler、T04 fragment constant buffer/dynamic offset，
   T05 多 vertex buffer/per-instance layout、T06 多 color target/逐 attachment blend state、T07
   combined depth/stencil/front-back stencil state、T08 multisample/resolve/sample state 与 T09 的
   fragment 2D/array/cube texture bindings，以及 T11 的 compute pipeline/shader 和直接读写
   2D texture bindings，以及 T16 直接 vertex、T17 VS/FS 批量 texture/sampler、T18 fragment
   storage buffer 和 T19 vertex storage buffer bindings；其他 vertex format、writable/array buffer、
   更多 blend/depth-stencil 组合仍归后续范围。
10. 直接非索引 draw 已覆盖 instance count/base instance 与 T13 shared buffer 间接参数；indexed draw
    已支持 T14 直接 indexed instancing/base vertex/base instance。T20/T22/T23 已覆盖 CPU 编码
    单命令、多命令和 indexed ICB；T21 已覆盖 CPU/shared 五字段 indexed indirect；T24/T25 已覆盖
    reset 后重编码与混合命令 ICB；T26/T27 已覆盖 pipeline/buffer inheritance。GPU 生成、compute
    ICB、heap、blit ICB 管理和多 queue 仍不在当前支持承诺中。
11. 当前 Metal mesh renderer 只承诺 VS Input 的 Float2/Float3/Float4 和 Metal 可直接绘制的常见
    point/line/triangle topology；T05 的 per-instance 表格读取已支持，但 raw VS Input preview 不推导
    shader 中的 instance transform。post-VS、选点、高亮、solid/secondary/bbox 等仍待后续实现。
12. Metal Pipeline State 已接入标准 IA/VS/RS/FS/OM/CS、empty-slot、RDTree 资源操作/预览、HTML export、
    有反射证据的 used/unused 过滤、fragment constant buffer、逐附件 blend 与 depth/stencil；共享阶段
    导航已支持 Left/Right/Home/End。T16/T17 补齐 VS/FS texture/sampler 直接与批量表，T18/T19
    补齐 fragment/vertex storage buffer 表；剩余差异是更细的紧凑布局、更多状态字段及高级绑定类型。

## 下一步（按顺序）

1. 下一开发项为 `PHASE31.md` P31.1：T30 compute sampler fixture/native。
   完成 T30 必要自动验证后按 `BATCH31-32.md` 连续推进 T31；批末向用户交付
   合并 GUI 验收单。T30/T31 尚未实施，当前 `QA_PENDING.md` 无待验项。

## 恢复检查点

- 2026-09-24 T29 用户反馈检查点：首次 ⌘O 重复弹窗已由用户确认修复。
  T29 EID 4/5 Buffer 20 值回退/前进、画面、状态栏通过；HTML/CSV/336-byte Save Bytes
  均已导出，raw 与自动参考完全一致。API Inspector 看不到状态调用，原因是 Metal
  `setBuffer` 等 chunk 无 EID，原组件只显示 action.events。已补列该 action 前的
  structured chunks，标记无 EID；最新 qrenderdoc 构建成功，SHA-256
  `c1a773a05d56…`，`git diff --check` 通过。第一项未完成：用户 ⌘Q 后在新 GUI
  的 T29 EID 5 核对两条 setBuffer，并确认 EID 2 Buffer 20 全 `a5`；T28
  Texture 20 10×7 UI DDS 仍待导出。
  正式 captures 不变，全部未提交改动保留，两个阶段与批次均未关闭。
- 2026-09-24 首次打开二次修复检查点：用户确认第一版 ⌘O 延迟触发修复无效，
  仍在 T28/T29 任意先后顺序中打开第一份后再次弹文件窗口。修改
  `qrenderdoc/Windows/MainWindow.cpp`，让 Open Capture 只走 Qt QAction 快捷键，
  不再在 ShortcutOverride 全局表中重复触发；第一版修复已撤回。增量构建通过，
  最终 GUI SHA-256 `969a565fe559…`，正式 captures 未变。下一项：用户 ⌘Q 后
  重开同一路径 app，确认首次 ⌘O 只弹一次。用户已完成 T28 EID 1 Texture 20
  目视验收；10×7 UI DDS 仍待导出，T29 L4 待验。无失败构建命令；未提交改动
  全部保留，批次未关闭。
- 2026-09-24 T28 用户反馈检查点：用户确认 T28 其余 L4 项通过；EID 1 看到了
  Texture 30 而非目标 Texture 20，UI DDS 亦为 Texture 30。T29 未收到反馈。
  修复首次 ⌘O 可能因嵌套事件循环重入而重复弹窗的问题，仅修改
  `qrenderdoc/Windows/MainWindow.cpp`：延迟打开动作。增量构建、T28/T29 Replay API
  smoke 与 CLI replay、自动导出内容对照、`git diff --check` 通过。新 GUI SHA-256
  `52c7e50b9928…`；正式 capture SHA 未变。无失败命令。第一项未完成：用户使用
  新构建按 `QA_BATCH29-30.md` 最短复验首次打开、T28 Texture 20 EID 1/DDS；随后
  同轮验 T29。L3 未触发；全部历史未提交改动保留。不可将两阶段或批次标为关闭。
- 2026-09-24 BATCH29-30 自动验证完成检查点：最终代码/GUI 构建成功，正式 T28/T29
  capture 与自动证据见顶部和 `BATCH29-30.md`。T28/T29 自动通过，用户 L4 尚未反馈；
  `QA_PENDING.md` 保留两项待验。最后成功命令 `bash /tmp/run-metal-batch29-30.sh`，
  日志 `/tmp/batch29-30-final.log`；随后 DDS/raw 内容、正式 SHA-256、合并验收单核对。
  无当前失败命令。全部历史未提交改动保留。下一安全操作是交付
  `QA_BATCH29-30.md` 给用户；收到反馈前不关闭两阶段。本轮 L1/L2/CLI/lifecycle 已完成，
  L3 未触发，L4 待用户同轮完成。

- 2026-09-24 BATCH27-28 关闭检查点：最终脚本 `/tmp/run-metal-batch27-28.sh` 与汇总
  `/tmp/batch27-28-final.log` 通过。T26/T27 及联合 T20/T22/T24/T25/T01/T05/T16/T19
  的 native/capture 或定向 replay、逐份 CLI、异常拒绝和 11×10 lifecycle 均通过；
  resident growth 507,904 bytes。同一最新 qrenderdoc 进程验收正式 T26/T27 capture，
  逐 draw Pipeline/Buffer/offset、Mesh/Resource、红蓝输出、HTML/CSV/DDS 保存均通过，
  两份均 `No problems detected`。L3 条件未触发。`BATCH27-28.md`、`PHASE27.md`、
  `PHASE28.md` 与索引文档已同步，全部 dirty 改动保留。下一批 `BATCH29-30.md`
  分为 `PHASE29.md` T28 dispatchThreads 与 `PHASE30.md` T29 compute buffer binding；
  第一项未完成 P29.1。最近完整 L3 仍是 T00–T18。

- 2026-09-24 BATCH25-26 关闭检查点：最终脚本 `/tmp/run-metal-batch25-26.sh` 与汇总
  `/tmp/batch25-26-final.log` 通过。T24/T25 及联合 T01/T02/T13/T14/T20/T21/T22/T23
  native/capture/XML/Replay API/逐份 CLI replay、6/11/18/8/9/15 类对应异常拒绝及
  11×10 lifecycle 全部通过，resident growth 1,638,400 bytes。同一最新 qrenderdoc 进程
  依次验收正式 T24/T25 capture：T24 reset/reencode 的三个展开 draw、旧命令失效、
  IA/Mesh/Buffer/Resource、HTML/CSV、红绿蓝输出；T25 两类 action、direct 无 index、indexed
  UInt16 4/6 与 Vertex/Instance 输入、Mesh/Buffer/Resource、HTML/CSV/DDS、红绿蓝输出；
  两份均 `No problems detected`。条件旧场景与 L3 均未触发。`BATCH25-26.md`、
  `PHASE25.md`、`PHASE26.md` 与索引文档已同步，全部未提交改动保留。下一批已拆为
  `BATCH27-28.md`、`PHASE27.md` T26 pipeline inheritance、`PHASE28.md` T27 buffers
  inheritance；第一项未完成为 P27.1。最近完整 L3 仍是 T00–T18。

- 2026-09-24 BATCH23-24 关闭检查点：T22/T23 最终联合 T01/T02/T13/T14/T20/T21/T22/T23
  native/capture/XML/Replay API/逐份 CLI replay、T20/T21/T22/T23 的 6/9/11/18 类
  异常拒绝及 9×10 lifecycle 全部通过，汇总 `/tmp/batch23-24-final.log`。同一最新
  qrenderdoc 进程依次验收哈希与正式文件相同的 T22/T23 capture：T22 `1+2` range、
  两个展开 draw/IA/Mesh/Buffer/Resource、HTML/CSV、红蓝输出；T23 indexed action、
  UInt16 index 4/6、两实例 Mesh、Buffer/ICB usage、HTML/CSV/DDS、红蓝输出；两份均
  `No problems detected`。L3 未触发，条件旧场景未触发。`BATCH23-24.md`、
  `PHASE23.md`、`PHASE24.md` 与索引文档已同步，全部未提交改动保留。
  下一批 `BATCH25-26.md` 已拆为 `PHASE25.md` T24 reset 后重编码、`PHASE26.md`
  T25 混合命令 ICB；第一项未完成为 P25.1。最近完整 L3 仍是 T00–T18。

- 2026-09-24 T23 P24.4 批末 UI 中断检查点（已由上方关闭检查点取代）：最终联合 L0/L1/L2、逐份 CLI replay、
  T20/T21/T22/T23 异常拒绝与 9 份 capture × 10 轮 lifecycle 全部通过，汇总
  `/tmp/batch23-24-final.log`。T22/T23 均标“自动验证通过，批末 UI 待验”；L3 条件未触发。
  最新 qrenderdoc 已重启并在同一进程打开与正式 capture SHA-256 相同的
  `/tmp/t22-batch-ui.rdc`，验证 Event range 1+2、EID 3/4 各自 Buffer 18/19、Mesh、
  ICB `Indirect argument` usage、红蓝输出、HTML 导出及 `No problems detected`。
  T22 Buffer CSV 保存确认前 Mac 再次锁屏；T23 尚未在本轮 UI 打开。第一项未完成：
  用户解锁后完成 T22 CSV 保存并在同一进程打开 `/tmp/t23-batch-ui.rdc`，完成 T23 全部
  L4，随后同步阶段/索引与下一批。临时两份 RDC 与正式文件哈希一致；全部 dirty 改动保留。

- 2026-09-24 T22 P23.4 批内转交检查点：T22 未注入 native、capture/XML、Replay API
  action/state/usage/readback/seek、逐份 CLI replay、三份顶点包原始字节、11 类异常拒绝、
  T20/T01/T13 定向与本场景 10 轮 lifecycle 通过；`PHASE23.md` 已标“批末 UI 待验”。
  样例 `util/test/demos/metal/metal_multi_command_icb.cpp`，正式 capture
  `captures/metal-smoke/t22_capture.rdc`。最终联合 L1/L2/CLI/lifecycle 与 T22/T23 同轮 L4
  尚待批末；L3 未触发。全部未提交改动保留；第一项未完成是 `PHASE24.md` P24.1 T23
  indexed ICB fixture/native。

- 2026-09-24 BATCH21-22 关闭检查点：T20/T21 联合 native/capture/XML/Replay API/逐份 CLI
  replay、T20 六类/T21 九类异常拒绝及 8 份 capture × 10 轮 lifecycle 通过，日志
  `/tmp/batch21-22-final.log`。最终 Viewer 标签/五字段格式修复后，最新 qrenderdoc 同一进程
  验收 T20/T21，并额外用 T13 确认非索引四字段路径；T20 的最终 IA/Mesh、ICB Resource
  与红色输出已再次复核。全部状态栏 `No problems detected`。`HANDOFF.md` 接手路径与
  `TEST_MATRIX.md` 批次说明已同步到 BATCH23-24，`git diff --check` 通过。
  L3 未触发。全部现有未提交改动必须保留。已新增 `BATCH23-24.md`、`PHASE23.md`、
  `PHASE24.md`；第一项未完成是 P23.1 T22 多命令 ICB fixture 未注入 native 验证。

- 2026-09-24 批末 UI 阻塞检查点：T20/T21 的最终联合自动验收已通过，日志和产物见顶部；
  第一项未完成是解锁 Mac 后用最新 `build-macos-debug/bin/qrenderdoc.app` 在同一轮依次验收
  `captures/metal-smoke/t20_capture.rdc` 与 `t21_capture.rdc` 的 Event/API、Pipeline、
  Mesh/Buffer/Resource、跳转/保存/export、输出与 `No problems detected`。若 L4 发现问题，
  修复后只重跑受影响自动项与 UI；之后同步阶段/索引文档并规划下一批。全部 dirty 改动保留，
  当前 L3 未触发，不得把阶段写成关闭。

- 2026-09-24 T21 P22.1 接手检查点：T20 自动验证通过、批末 UI 待验；T21 fixture 的原生
  5 帧及 driver 增量构建已通过。当前第一项未完成为 T21 capture/XML、CLI replay 和 Replay API
  smoke。此前全部未提交改动与 T20 capture 保留；联合 L1/L2 和双 capture L4 仍待批末执行。

- 2026-09-24 批次编排检查点：用户要求减少重复 QA，并要求新对话可批量执行 phase、批末统一
  验收。已新增 `BATCH21-22.md` 和 `PHASE22.md`，将 T20 单命令 ICB 与 T21 indexed indirect
  固定为两阶段一批；`PHASE21.md`、`HANDOFF.md`、`PLAN.md`、README、TEST_MATRIX 与 STATUS
  同步。T20/T21 尚未开发或验证，第一项未完成仍为 P21.1。每条切片的 native/capture/replay
  自动验证须立即完成；批末在最终构建上去重跑联合定向清单，qrenderdoc 同一轮验收两份
  capture，未通过 L4 不可标记任一阶段关闭。L3 默认不跑，触发后范围至 T00–T21。
  本次仅修改文档，全部既有未提交改动保留；最近代码证据仍是 T19 阶段关闭检查点。

- 2026-09-24 T19 阶段关闭检查点：P20.1-P20.4 已完成。新增 768-byte vertex storage fixture，
  slot 4/offset 256 used、slot 6/offset 320 unused；reflection/descriptor/`VS_Resource`、IA/storage
  分类、异常 slot/offset、标准 VS Storage Buffers 与 UI 导出均通过。必跑 T19/T18/T16 以及因
  `setVertexBuffer` 分类改动触发的 T02/T05 全部通过；T19 CLI 与 10 轮 lifecycle 通过，最终构建
  `/tmp/t19-final-build.log`。最新 qrenderdoc 已核对 Event/API、IA 空表、VS slot 4/6、标准
  Buffer/Resource、四象限和 `No problems detected`；UI HTML/CSV/bin 位于 `/tmp/t19_*`。
  定向结果没有跨场景未界定风险，故未触发 T00-T19 L3；最近完整基线仍是 T00-T18。
  全部既有未提交改动保留。第一项未完成为 `PHASE21.md` P21.1。

- 2026-09-24 验证清单细化：`HANDOFF.md` 和 `PLAN.md` 规定每份新阶段文档须列出必跑旧 T、
  条件触发旧 T、L4 检查和 L3 决策/完整范围。`PHASE20.md` 已明确 T19、必跑 T18/T16、
  条件触发 T02/T05；L3 默认不执行，触发时覆盖 T00-T19。T19 仍待从 P20.1 开始；
  本次只改文档，未运行代码测试，既有未提交改动保留。

- 2026-09-24 验证规则调整检查点：用户指出每个 Txx 阶段全量回归随场景数增长，要求小范围验证
  只覆盖本次修改。已同步 `HANDOFF.md`、`PLAN.md`、`PHASE20.md`：T19 从 P20.1 开始，先做
  T19 和实际受影响的 T18/T02/T05/T16 定向验证；T19 UI 用最新 qrenderdoc 做一次定向验收。
  仅在较大里程碑、发布/合并前或确有定向测试无法覆盖的跨场景风险时运行 L3。此前
  T00-T18 L3/L4 结果保留为历史证据，不重跑；全部未提交改动保留。本次只改文档，未运行代码测试。

- 2026-09-24 T18 阶段关闭检查点：P19.1-P19.4 完成。T00-T18 L3 日志
  `/tmp/t18-final-regression.log`（19×10 lifecycle，resident growth 1,556,480 bytes）；
  正式 capture、640-byte raw、UI CSV/HTML 产物位于 `captures/metal-smoke/`。最新构建 qrenderdoc
  用与正式 capture SHA-256 相同的 `/tmp/t18-ui-final.rdc` 完成 Event/API、FS Storage Buffers
  slot 3/offset 256/size 384、标准 Buffer Viewer、Resource Inspector `FS - Resource`、
  HTML/CSV 保存与 `No problems detected` 验收。UI CSV 前四行与自动 raw export 一致；
  UI 二进制菜单项未单独点击，raw 由自动 smoke 验证。T18 fixture、reflection/descriptor/usage、
  Metal FS Pipeline UI、usage 标签、自动 smoke 与阶段文档已修改；全部前序 dirty worktree 改动
  保留。第一项未完成为 `PHASE20.md` P20.1；不要重跑 T18 L3/L4。

- 2026-09-24 P19 L4 待验收检查点：T18 fixture、反射/descriptor/usage 和 FS Storage Buffers
  页面已实现；原生/capture/XML/replay、T04/T12/T10 定向均通过。T00-T18 完整 L3 日志
  `/tmp/t18-final-regression.log`：19×10 lifecycle、resident growth 737280 bytes，CLI replay 通过。
  完整回归无需重跑。qrenderdoc 旧进程打开 T18 后显示旧页面；已退出并启动最新进程，二进制含
  `Storage Buffers`，但 Mac 随即锁屏，Computer Use 报需用户解锁。L4 **未完成**，阶段尚未关闭。
  解锁后只需用最新 qrenderdoc 打开 `captures/metal-smoke/t18_capture.rdc`，核对 Event/API、
  EID 2 的 FS Storage Buffers slot 3/offset 256/size 384、未绑定 slot 5、标准 Buffer Viewer、
  Resource Inspector、HTML/raw save 和 `No problems detected`；然后同步阶段关闭文档。
  全部前序 dirty worktree 改动保留。最后成功命令为全量回归；没有待修复的代码失败。

- 2026-09-24 P19 L4 续接检查点：Mac 曾短暂解锁，最新 qrenderdoc 已显示 EID 2 的 FS
  Storage Buffers slot 3/offset 256/size 384、标准 Buffer Viewer 的四组 `float4` 和
  Resource Inspector 的 EID 2 usage。UI 发现 `PS_Resource` 文案误报 `FS - Texture`，已在
  `qrenderdoc/Code/QRDUtils.cpp` 为 Metal 改成 `FS - Resource`；修改后 T00-T18 全量回归
  `/tmp/t18-final-regression.log` 再次通过，19×10 lifecycle，resident growth 1556480 bytes。
  最新 qrenderdoc 可执行文件已重建并启动，但 Computer Use 随后确认 Mac 已锁屏且无法自动解锁。
  已请求用户手动解锁；第一项未完成
  是在新进程中核对更正后的 Resource Inspector 标签、HTML/raw save 与状态栏，**无需再跑 L3**。

- 2026-09-23 P19 开始检查点：已按最短接手路径读取 README、STATUS 当前阶段/最近检查点、PHASE19
  和 HANDOFF 验证规则；`git status` 确认前序未提交改动均保留，`git diff --check` 已通过。T18 尚未
  修改或验证。第一项未完成为 P19.1 原生 fixture；开发中仅跑 T18 与受影响 T04/T12，P19.4 才运行
  一次 T00-T18 L3 和一次最新 qrenderdoc L4。

- 2026-09-23 T17 阶段关闭检查点：P18.1-P18.4 全部通过。全量 L3 日志
  `/tmp/t17-final-regression.log`（18×10 lifecycle，resident growth 1,015,808 bytes）；正式 T17
  capture/XML/PPM/DDS、UI DDS/HTML 位于 `captures/metal-smoke/`。修改 T17 fixture、四个 render
  encoder 批量入口、replay fragment texture usage、自动 smoke/回归脚本和当前阶段文档；此前所有 dirty
  worktree 改动均须保留。最新 L4 已验证 Event/API、VS/FS Pipeline、空槽/未使用项、Mesh/Buffer/
  Texture/Resource、DDS/HTML 与状态栏。第一项未完成为 `PHASE19.md` P19.1；无需重跑 T17 L3/L4。

- 2026-09-23 T16 阶段关闭检查点：P17.1-P17.4 全部通过。全量 L3 日志
  `/tmp/t16-final-regression.log`（17×10 lifecycle，resident growth 114688 bytes）；正式
  T16 capture、XML、PPM、DDS 与 UI DDS/HTML 位于 `captures/metal-smoke/`。本轮修改 T16 fixture、
  render encoder bridge/wrapper/chunk、replay snapshot/descriptor/usage、Metal Pipeline VS UI、
  自动 smoke/脚本和文档；T00-T15 累计 dirty worktree 均不可回退。最新 L4 已验证 Event/API、
  VS Pipeline、Texture/Buffer/Mesh/Resource、DDS/HTML 与状态栏。第一项未完成为 `PHASE18.md`
  P18.1；不要重跑 T16 L3/L4，不要清理/回退工作区。

- 2026-09-23 T15 阶段关闭检查点：P16.1-P16.4 均已完成。T00-T15 L3 日志
  `/tmp/t15-final-regression.log`（16×10 lifecycle resident growth 737280 bytes）；正式 capture
  `captures/metal-smoke/t15_capture.rdc` SHA-256 为
  `208b794cc2ab49cf88f970cdda787bfb016f3483284b389130e05a39bae1a614`，自动 DDS
  `captures/metal-smoke/t15_output.dds` 480128 bytes。T01/T02/T05/T14 定向 smoke 均通过。
  非法 primitive 99 与零 vertexCount 的 `/tmp/t15-*.rdc` 派生 capture 均在 replay 拒绝。
- 本轮新增/修改：`util/test/demos/metal/metal_point_line.cpp`、demos CMake 列表、
  `metal_render_command_encoder.cpp`、`metal_replay.cpp`、`metal_replay_output_smoke.mm`、
  `test_metal_capture_macos.sh` 及阶段文档；既有 T00-T14 dirty worktree 均不可回退。
- 最新 L4：完全重启最新 qrenderdoc 后打开正式 T15，Event Browser 三个 action、EID 2/3/4
  的 Point List/Line List/Line Strip、API Inspector 的 Line Strip/6/3、标准 Buffer Viewer 的
  11 行顶点、Resource Inspector `Vertex Buffer` usage、三个 Mesh VS Input、Texture Viewer
  红点绿线蓝折线及 UI DDS/HTML export 均已验证，状态栏 `No problems detected`。
- 第一项未完成：进入 `PHASE17.md` P17.1；不要重跑 T15 L3/L4，不要清理/回退工作区。

- 历史检查点（T14 关闭时）：当时第一项未完成工作是 `PHASE16.md` P16.1 / T15 fixture。
- 工作区：T00-T14 累计未提交有效改动仍在。本轮新增 `metal_indexed_instancing.cpp`、indexed
  instanced-base render encoder capture/replay、精确 index binding/usage 和 T14 回归。
  不得清理、覆盖或回退；完整 dirty 列表以 `git status --short` 为准。
- 最近成功：`/tmp/t14-final-regression.log` 记录 T00-T14 native/capture/XML/output/data/state、
  15×10 lifecycle（resident growth 1,015,808 bytes）和逐份 CLI replay 全部通过。T02/T05/T13
  定向 output smoke 通过；offset 3/10 派生 RDC 均在 indexed-instanced chunk 拒绝。
- 最新 L4：完全重启最新 qrenderdoc 后，正式 `t14_capture.rdc` 的 Event Browser EID 2、API
  Buffer 18/offset 4、IA Pipeline Buffer 16/17/18、Resource Inspector `Index Buffer`、
  标准 Buffer Viewer 范围 4/6 与 `0/1/2`、Mesh instance 0/1、红蓝输出及状态栏
  `No problems detected` 均通过。`t14_indices_ui.csv` 内容为 0/1/2，
  `t14_pipeline_state_standard.html` 包含 Triangle List 与 Buffer 16/17/18。
- 当时的下一条安全操作：进入 `PHASE16.md` P16.1；T15 现已关闭，不要重跑 T14/T15 L3/L4，
  也不要清理累计 dirty worktree。

## Agent 工作节奏

2026-09-24 起采用更新后的 `PLAN.md` / `HANDOFF.md` 验证规则：一个 agent 默认连续负责完整 Txx
阶段，阶段内及收口只做当前与实际受影响旧路径的定向验证；涉及 UI 时完成一次最新 qrenderdoc
定向验收。L3 全量回归仅在较大里程碑、发布/合并前或确有定向测试无法覆盖的跨场景风险时执行。
阶段完成后可在当前对话继续；阶段中途只有先写好上述恢复检查点后才建议 compact。agent 不应
等待用户反复发送“继续”。

## 构建与启动

```sh
./util/buildscripts/scripts/build_metal_dev_macos.sh
./util/buildscripts/scripts/build_metal_dev_macos.sh --run
./util/buildscripts/scripts/test_metal_capture_macos.sh
```

- Build 目录：`build-macos-debug`
- App：`build-macos-debug/bin/qrenderdoc.app`
- 核心库：`build-macos-debug/lib/librenderdoc.dylib`
- App 内嵌核心库：`build-macos-debug/bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib`；
  `build_metal_dev_macos.sh` 在增量构建后核对并同步，防止 UI 加载旧 replay 代码。
- CLI：`build-macos-debug/bin/renderdoccmd`
- Metal demos：`bin/demos_x64`
- smoke capture/XML：`captures/metal-smoke/`（生成目录，不提交）
- 清理：删除 `build-macos-debug` 后重新运行脚本；该目录已被 `.gitignore` 忽略。

## 最近验证

2026-09-23 T10 收口：`./util/buildscripts/scripts/test_metal_capture_macos.sh` 首次完整运行通过；
通用事件分组/UI 修正后按阶段门槛以最终代码复验，日志 `/tmp/t10-final-regression-post-ui.log`；
十一份 capture 各 10 次 lifecycle resident growth 1,277,952 字节。T10 自动断言 buffer
copy/fill 的 EID 2→3→2 数据往返、texture copy 四象限、
mip1/mip3、资源 usage、最终四条色带和 468-byte DDS，T03/T09 路径与 T00-T08 均通过。
`/tmp/t10-final-cli-replay-post-ui.log` 记录 T00-T10 逐份 `--loops 1` 成功。最新 qrenderdoc app
包内库与构建库逐字节一致。
正式 capture 的 `/tmp` SHA-256 相同副本在 Event Browser 显示 `Copy/Clear Pass #1`、EID 1-6
blit 操作；API Inspector 可跳到 Buffer 18 和 Texture 20，Resource Inspector usage 为
`Copy - Dest`/`Clear`/`Generate Mips`。标准 Buffer Viewer 的 EID 2/3、Texture Viewer 的四象限
与 mip3 拾取 `(0.52549,0.51765,0.34510,1.00)`、EID 8 四条色带均与自动结果一致。
`captures/metal-smoke/t10_mips_ui_final.dds` 与自动 `t10_mips.dds` 各 468 字节、`cmp` 相同；
状态栏 `No problems detected`。

2026-09-23 T09 收口：`./util/buildscripts/scripts/test_metal_capture_macos.sh` 最新运行通过，日志
`/tmp/t09-final-regression-v3.log`；十份 capture 各 10 次 lifecycle resident growth 为 475,136 字节。
T09 自动断言 12 子资源 readback/display、cube face pick、越界拒绝、12 条色带、fragment bindings 和
cube DDS；T03 texture 路径与 T00-T08 均通过。`renderdoccmd replay --loops 1` 对 T00-T09 逐一通过，
`git diff --check` 通过。最终 qrenderdoc 打开同一最新 T09 capture 的 `/tmp` 字节拷贝，EID 2 FS
绑定为 Texture 17/18/19 + Sampler 20；Texture Viewer 的 mip1/2、array slice1/2、cube X-/Z-
切换颜色正确，Z- 时“Save selected Texture”写出 512-byte
`captures/metal-smoke/t09_cube_ui.dds`，状态栏为 `No problems detected`。

```sh
cmake --build build-macos-debug --target renderdoc renderdoccmd build-qrenderdoc -j 12
./util/buildscripts/scripts/test_metal_capture_macos.sh
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t00_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t01_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t02_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t03_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t04_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t05_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t06_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t07_capture.rdc
build-macos-debug/bin/renderdoccmd replay --loops 3 captures/metal-smoke/t08_capture.rdc
git diff --check
```

2026-09-22 T08 收口的最新一轮均通过。T08 自动回归检查 216-byte vertex buffer、4x MSAA descriptor、
显式 resolve/store action、三条 draw action、sample/resolve pipeline state、clear/draw/回退 resolve 图像，
最终 PPM 为左红、中绿、右蓝；完整 lifecycle resident growth 为 376,832 字节。qrenderdoc EID 4 的
OM 页显示 4x MSAA Color Target 与 1x Resolve Target；从 resolve 行进入 Texture Viewer 后中心拾取为
`(0.06275, 0.87451, 0.18824, 1.00)`，实际 HTML export 已核对，状态栏无错误。
较早的 T01 qrenderdoc 验证还覆盖 Texture Viewer 和窗口最大化 resize，画面正确，
状态栏显示 `No problems detected`，未再出现 degraded support 弹窗。事件回放自动化与 qrenderdoc
手工切换 EID 1 -> 2 -> 1 也通过，画面按 clear -> draw -> clear 正确变化。
`test_metal_capture_macos.sh` 还自动验证 capture texture 的紧密 BGRA 字节、clear/draw 像素拾取和
DDS 保存；`t01_texture.dds` 经 `file` 识别为 400x300、32-bit ARGB8888。
qrenderdoc 中也已手工验证 EID 2 的 Texture Viewer：右键拾取纹理坐标 `(202, 128)` 得到
`(0.61176, 0.34118, 0.32157, 1.00)`，状态栏为 `No problems detected`；“Save selected Texture”
成功写出 `t01_texture_ui.dds`，同样识别为 400x300、32-bit ARGB8888、480128 字节。
最终收口时将 `GetTextureData()`/`PickPixel()` 的资源类型校验改为 replay texture 描述表，避免因
live wrapper 不保留 capture record 而误拒绝合法纹理；修正后重新执行完整 capture smoke、T00/T01
单轮 CLI replay 和 `git diff --check`，均通过。
2026-09-21 的 Shader Viewer 收口中，自动回归验证了 `vs_main`/Vertex、`fs_main`/Fragment 和真实
MSL source。随后启动新构建 qrenderdoc，分别从 Function 13/14 打开 `captured.metal`，源码显示正确、
状态栏无错误。测试期间还复现并修复了持久化 D3D11 Pipeline State 子页面误用于 Metal capture 的
崩溃；修复后的同一路径已重新实机加载通过。
最终重新执行 `test_metal_capture_macos.sh`，生成新 T00/T01 capture，并通过 shader reflection、
texture data/pick/save 和 10 轮 event replay；随后 T00/T01 各自执行 `renderdoccmd replay --loops 3`
也均以状态 0 退出。
2026-09-21 的 Pipeline State 收口中，`renderdoc`、`renderdoccmd`、SWIG 与 qrenderdoc 均成功构建；
完整 smoke 重新生成 T00/T01，并自动验证 Metal pipeline/shader/topology/VB/color target 与原有
shader/texture/event 路径。随后关闭旧进程、启动新 qrenderdoc，在最新 T01 EID 2 的 Metal Pipeline
State 页面核对 `Pipeline State 15`、`Triangle List`、`Function 13/14`、`vs_main/fs_main`、
`Buffer 16`（0/96）和 `Texture 23`（mip/slice 0），状态栏为 `No problems detected`。
2026-09-21 的 M2.6 阶段关闭中，完整一键 smoke 再次通过并自动执行 T00/T01 各 10 次同进程
打开/关闭，warm-up 后 resident growth 为 491,520 字节；两份 capture 的 `renderdoccmd replay
--loops 3` 均以状态 0 退出。最新 qrenderdoc 同一进程完成 `T01 -> Close -> T00 -> Close -> T01`，
重开 T01 EID 2 后图像和 Pipeline State 仍正确，状态栏无错误；History/Debug 明确禁用。工作区路径
通过 Recent Captures 打开时曾被 macOS 26 阻塞在文件 `open()`，使用 SHA-256 相同的 `/private/tmp`
副本完成验证，因此该现象记录为宿主文件访问问题，不计为 replay 失败。
随后额外运行 T00/T01 各 50 次 lifecycle 压力检查，resident growth 为 1,441,792 字节并通过。
2026-09-21 的 T02 P3.1/P3.2 回归中，一键脚本重新构建并原生运行 T00/T01/T02，生成三份 capture
与 XML；T02 的 vertex descriptor、private depth texture/state、CCW/back cull、两组 viewport/scissor、
36 个 UInt16/UInt32 index draw 全部通过 structured 断言。Replay smoke 验证两个 indexed action、
真实 depth target、已知 index 字节与左右半屏输出；三份 capture 各 10 次生命周期循环通过，resident
growth 为 1,196,032 字节。
同一份 T02 capture 随后由最新 qrenderdoc 实机打开；EID 3 的 UInt32 indexed action、color/depth
outputs 和双立方体图像均正确，状态栏无错误。因 macOS 26 对工作区 Documents 路径弹出文件访问
授权，仍按既有方式使用 SHA-256 相同的 `/private/tmp` 副本完成验证。
2026-09-21 的 T02 P3.3 收口中，Metal pipeline snapshot 增加 vertex descriptor、index buffer、depth
与 raster 字段，并改为按 action event 保存 draw-time 状态。完整脚本重新截取三份 capture，验证
T02 clear/draw1/draw2/回退图像和两条 draw 的精确状态，lifecycle resident growth 为 720,896 字节。
随后关闭旧 qrenderdoc、启动最新构建，加载 SHA-256 与工作区一致的 T02 副本：EID 2 显示左半屏、
UInt16 Buffer 19/72，EID 3 显示右半屏、UInt32 Buffer 20/144；Float3/Float4、stride 28、less/write、
back/CCW 与 color/depth target 均正确，状态栏为 `No problems detected`。
2026-09-21 的 T02 P3.4/P3.5 收口中，Metal vertex inputs 接入通用 `PipeState`，Pipeline 资源激活
复用标准 Mesh/Buffer Viewer，并新增最小 VS Input wireframe renderer。完整脚本重新截取三份 capture，
验证 T02 generic vertex input 与 mesh output，lifecycle resident growth 为 294,912 字节。最新 qrenderdoc
重启后，EID 2 的 Mesh Viewer 显示 UInt16 展开的 `attr0/attr1` 和正确立方体线框；Buffer 18 自动显示
Float3/Float4 interleaved 数据，Buffer 19 显示 72-byte `ushort index`；EID 3 的 Buffer 20 显示
144-byte `uint index`，状态栏始终为 `No problems detected`。收尾时 T00/T01/T02 又分别执行
`renderdoccmd replay --loops 3`，并通过 `git diff --check`。

2026-09-21 的 T03 P4.1-P4.3/P4.5 验证中，新增 4x4 RGBA8 纹理四边形、`replaceRegion` upload、
一级 sampler resource、fragment texture/sampler replay 与通用 descriptor 查询。完整脚本重新构建并
生成 T00-T03 capture，T03 的 64-byte texel、四点 `PickPixel()`、四象限输出和 binding 均通过；
四份 capture 各 10 次 lifecycle resident growth 为 1,245,184 字节，随后四份 capture 的
`renderdoccmd replay --loops 3` 均以状态 0 退出。最终构建的 qrenderdoc 在 T03 EID 2 显示
Texture 17、Sampler 18、Float2/Float2、Triangle Strip 和正确四象限；标准 Texture Viewer 与
Resource Inspector 跳转均通过，状态栏为 `No problems detected`。阶段 4 下一项保持为 P4.4 布局收敛。

2026-09-22 的 P4.4 首个收敛切片中，Metal Pipeline 页面复用标准 Controls、`PipelineFlowChart` 和
隐藏 stage tabs，按 IA/VS/RS/FS/OM 重排现有真实状态。`Show Empty Items` 使用红色空槽，静态
binding reflection 尚未实现时 `Show Unused Items` 明确禁用；shader 行可直接进入 Shader Viewer。
完整 T00-T03 回归通过，四份 capture 各 10 次 lifecycle resident growth 为 294,912 字节。最终
qrenderdoc 先用 T03 验证 FS texture/sampler、empty index/depth 和 shader 跳转，再用 T02 验证 IA
UInt16 binding、RS viewport/scissor/raster 及 OM color/depth/depth-state，状态栏均为
`No problems detected`。该首个切片完成后，P4.4 转入资源树、上下文操作、预览/export 与 used
reflection 收敛。

2026-09-22 的 P4.4 第二个收敛切片将 Metal 表迁移到 `RDTreeWidget`/`RDHeaderView`，并把 Metal
ResourceId 接入通用 context/usage、thumbnail 和 preview 分发；资源双击路径保持不变。工具栏新增标准
Export 控件，T03 EID 2 实际导出的 HTML 含 IA/VS/RS/FS/OM、Triangle Strip、Texture 17 和
Sampler 18。完整 T00-T03 capture/replay 回归通过，四份 capture 各 10 次 lifecycle resident growth
为 524,288 字节；最终 qrenderdoc 保持运行且状态栏为 `No problems detected`。下一项转为 shader
resource binding reflection 与 used/unused 过滤。

2026-09-22 的 P4.4 第三个收敛切片让 replay 创建 pipeline 时请求 Metal argument reflection，并将
T03 的 `colourTexture`/`colourSampler` 映射到通用 shader reflection。fixture 将同一资源额外绑定到
未声明的 slot 1；自动测试验证 slot 0 为 used、slot 1 为 `NoShaderBinding + staticallyUnused`，且
used-only 查询只返回 slot 0。完整 T00-T03 capture/replay 回归通过，四份 capture 各 10 次 lifecycle
resident growth 为 540,672 字节。最终构建的 qrenderdoc 在 T03 EID 2 FS 页默认只显示 slot 0，勾选
`Show Unused Items` 后 texture/sampler 表各显示 slot 1，状态栏为 `No problems detected`。

2026-09-22 的 T04 P5.1-P5.4 中新增动态 uniform fixture，并补齐 `setFragmentBufferOffset` 的
capture/replay、fragment constant-block state/reflection/descriptor 与 FS Constant Buffers 表。完整
T00-T04 脚本通过，最终 PPM 左右像素为 `ff2010`/`10df30`，五份 capture 各 10 次 lifecycle resident
growth 为 376,832 字节。最终构建的 qrenderdoc 实机切换 EID 2/3，图像由左红/右背景变为左红/右绿，
binding 从 `Buffer 16 / 0 / 512` 变为 `Buffer 16 / 256 / 256`；双击进入标准 Buffer Viewer 的
offset 256、length 256 子范围，四个 float 原始值正确，状态栏为 `No problems detected`。

2026-09-22 的 T05 P6.1-P6.4 中新增两物理 vertex buffer 的 instanced fixture，并以
`instanceCount=3/baseInstance=1` 验证直接 draw、action/state、通用 VS input 和事件回放。完整脚本
通过，最终三处 PPM 像素为 `ff2010/10df30/1840ff`，六份 capture 各 10 次 lifecycle resident growth
为 573,440 字节。最终 qrenderdoc 的 EID 2 Texture Viewer 显示三色实例；IA 显示
`Buffer 16 / 24 / stride 8 / Vertex / 1` 与 `Buffer 17 / 96 / stride 24 / Instance / 1`；Mesh Viewer
instance 0/1 正确显示 baseInstance 后的 offset/colour，两个 Buffer Viewer 展示完整原始数组，状态栏
保持 `No problems detected`。

## 工作日志

| 日期 | 任务 | 结果 |
| --- | --- | --- |
| 2026-09-20 | M0.1 | 从官方 GitHub 检出 `v1.46`，成功 |
| 2026-09-20 | M0.2 | 创建 `metal-replay-v1.46` 分支，成功 |
| 2026-09-20 | M0.3 | 完成源码/工具链预检，发现 Qt 5 等依赖缺失 |
| 2026-09-20 | 计划初始化 | 建立 `docs/metal-replay/` 文档集 |
| 2026-09-20 | M0.4 | 通过 Homebrew 安装并核验 macOS/Qt 构建依赖 |
| 2026-09-20 | M0.5 | `ENABLE_METAL=OFF` 构建成功，qrenderdoc 主窗口启动成功 |
| 2026-09-20 | M0.6 | 修复 SDK 26 bridge 编译问题；`ENABLE_METAL=ON` 构建和启动成功 |
| 2026-09-20 | M0.7-M0.8 | 增加一键脚本并记录产物、启动和清理方法 |
| 2026-09-20 | M1.1-M1.2 | 增加 T00/T01 Metal fixtures；原生构建与运行成功 |
| 2026-09-20 | M1.3-M1.4 | 修复 macOS 26 drawable/residency 边界和主动 capture backbuffer；成功写出 `.rdc` |
| 2026-09-20 | M1.5-M1.6 | T00/T01 chunks、MSL、96-byte VB、draw/present 经 XML structured export 验证 |
| 2026-09-20 | M1.7 | 增加并通过 `test_metal_capture_macos.sh` 一键回归；阶段 1 完成 |
| 2026-09-20 | M2 预备 | 注册 Metal structured processor；真实 replay provider 待实现 |
| 2026-09-20 | M2.1 | 注册真实 Metal replay provider；T00/T01 可由 `renderdoccmd replay` 加载 |
| 2026-09-20 | M2.2 | 重建 T00/T01 基础 Metal 对象、资源和命令并在加载期间真实执行 |
| 2026-09-20 | M2.3 | 增加 render pass、clear、draw、present、capture end 的最小 action/event |
| 2026-09-20 | M2.4 开始 | qrenderdoc 成功加载 T01；确认 degraded 弹窗是输出未实现的主动能力标记 |
| 2026-09-20 | M2.4 output | 实现 Metal output/`RenderTexture()`/readback；T00/T01 像素回归和 qrenderdoc UI 验证通过 |
| 2026-09-20 | M2.4/M2.8 event replay | 保存 frame stream/event offset，实现 Full/WithoutDraw/OnlyDraw；自动和 UI 的 clear -> draw -> clear 验证通过 |
| 2026-09-20 | M2.5 texture data | 实现 capture texture readback/`PickPixel()`，自动验证 clear/draw BGRA 数据并成功保存 DDS |
| 2026-09-20 | M2.5 texture data 收口 | 修正 live texture 类型校验；重跑完整 capture smoke、T00/T01 CLI replay 和 DDS 格式检查，全部通过 |
| 2026-09-21 | M2.5 Shader Viewer 开始 | 开始汇合 source library MSL、function 入口和 Metal function stage；完成后需自动回归并启动 qrenderdoc 实机验收 |
| 2026-09-21 | M2.5 Shader Viewer 收口 | `ShaderEncoding::MSL`、source/entry/stage reflection、自动测试和 Function 13/14 实机查看通过；下一项为最小 Pipeline State |
| 2026-09-21 | M2.5 Pipeline State 开始 | 建立 Metal 专用最小 pipeline state，目标为 T01 draw 的 pipeline、vertex/fragment shader、vertex buffer、topology 和 color target |
| 2026-09-21 | M2.5 Pipeline State 收口 | Metal snapshot、通用 `PipeState`、proxy serialization、qrenderdoc 专用页、自动状态断言与最新 T01 EID 2 实机验证全部通过；M2.5 完成，转入 M2.6 |
| 2026-09-21 | M2.6 生命周期 | 修复 replay Metal 对象所有权和确定性 shutdown；T00/T01 各 10 次循环通过，resident growth 491,520 字节 |
| 2026-09-21 | M2.6 unsupported | shader debug 返回安全空 trace；histogram/pixel history/post-VS/custom/target shader 稳定降级并纳入自动回归 |
| 2026-09-21 | M2.6 UI/文档收口 | 最新 qrenderdoc 完成 T01/T00 关闭重开和禁用能力验证；新增 `PHASE3.md`，下一项为 T02 P3.1 |
| 2026-09-21 | P3.1 T02 fixture | 新增确定性 indexed cube、显式 vertex descriptor、UInt16/UInt32 index、Depth32Float 与固定 raster state；原生运行通过 |
| 2026-09-21 | P3.2 capture/replay | 新增 depth-state wrapper，接通 scissor/front-face/cull/直接 indexed draw，并修复 depth attachment 帧引用；structured capture 与 GPU replay 通过 |
| 2026-09-21 | P3.1-P3.2 自动回归 | T00/T01/T02 全量脚本通过；T02 index 字节/depth target/双视口图像和三份 capture 各 10 次 lifecycle 通过 |
| 2026-09-21 | P3.2 T02 UI | 最新 qrenderdoc 的 Event/Texture Viewer 显示 EID 2/3、UInt32 Buffer 20、FB0/DS 与双立方体，状态栏无错误；完整 Pipeline/Mesh 留给 P3.3/P3.4 |
| 2026-09-21 | P3.3 T02 state/event | 增加 draw-time event snapshot、vertex/index/depth/raster 状态和 clear/draw1/draw2/回退断言；全量回归及 qrenderdoc Pipeline State 实机验证通过，转入 P3.4 Mesh/Buffer UI |
| 2026-09-21 | P3.4-P3.5 T02 Mesh/UI 收口 | 通用 VS input、标准 Mesh/Buffer 跳转、Float3 indexed wireframe、自动输出断言及 qrenderdoc UInt16/UInt32 实机验证通过；阶段 3 完成，转入 T03 |
| 2026-09-21 | P4.1-P4.3 T03 texture/sampler | 完成确定性纹理四边形、upload/sampler/binding capture/replay、通用 descriptor 与标准资源跳转 |
| 2026-09-21 | P4.5 T03 自动化/UI | T00-T03 完整回归、四份 capture 三轮 CLI replay、生命周期及 qrenderdoc T03 实机验证通过；转入 P4.4 标准布局收敛 |
| 2026-09-22 | P4.4 标准阶段布局 | Metal Pipeline 接入 Controls + PipelineFlowChart + IA/VS/RS/FS/OM，empty-slot 与 shader 直接跳转通过 T02/T03 实机验证；继续资源树/反射收敛 |
| 2026-09-22 | P4.4 标准资源表/export | Metal 表迁移到 RDTree/RDHeader，接入通用资源操作/预览与五阶段 HTML export；T03 实际导出、完整回归和最终 qrenderdoc 验证通过；继续 shader reflection |
| 2026-09-22 | P4.4 shader reflection/过滤 | Metal argument reflection、slot 0/1 used-unused 自动断言、完整回归及 qrenderdoc 过滤实机验证通过；T03 阶段关闭，下一项为 T04 动态 uniform |
| 2026-09-22 | P5.1-P5.4 T04 动态 uniform | 512-byte uniform、fragment buffer offset、constant-block reflection/descriptor、事件回放、完整 T00-T04 回归与 qrenderdoc Buffer Viewer 实机验证通过；下一项为 T05 instancing |
| 2026-09-22 | P6.1-P6.4 T05 instanced mesh | 两个 vertex buffer、base instance、per-instance VS input、三色输出、完整 T00-T05 回归与 qrenderdoc Pipeline/Mesh/Buffer 实机验证通过；下一项为 T06 MRT + blending |
| 2026-09-22 | P7.1-P7.4 T06 MRT/blending | 双 color attachment、多输出 action、逐附件 blend/write mask、两张 texture 事件回放、标准 OM/Texture/export、完整 T00-T06 回归与 qrenderdoc 实机验证通过；下一项为 T07 depth/stencil |
| 2026-09-22 | P8.1-P8.4 T07 depth/stencil | combined attachment、front/back stencil、dynamic reference、五 draw 事件回放、标准 OM/Texture/export、共享阶段键盘导航、完整 T00-T07 回归与 qrenderdoc 实机验证通过；下一项为 T08 MSAA resolve |
| 2026-09-22 | P9.1-P9.4 T08 MSAA resolve | 4x MSAA attachment、显式 resolve、sample/resolve snapshot、三 draw 事件回放、标准 OM/Texture/export、完整 T00-T08 回归与 qrenderdoc 实机验证通过；下一项为 T09 mip/cube/array |
| 2026-09-23 | Agent 节奏重编排 | 固化一个 agent 完成一个 Txx 阶段、L0-L4 分层验证、阶段末单次完整回归/qrenderdoc、compact 安全检查点和下一任务提示模板；T09 仍待开始 |
| 2026-09-23 | P10.1-P10.4 T09 mip/cube/array | 12 子资源 fixture、slice-aware upload/readback/pick/display、通用绑定、标准 Texture Viewer 和 cube DDS 保存；完整 T00-T09 回归、CLI replay、lifecycle 与最终 qrenderdoc 验收通过；下一项 T10 blit |
| 2026-09-23 | P11.1-P11.4 T10 blit | buffer/texture copy、fill、mipgen 的 action/usage/seek/readback、标准 Buffer/Texture UI 与 DDS；完整 T00-T10 回归、CLI replay、11×10 lifecycle 和最终 qrenderdoc 验收通过；下一项 T11 compute |
| 2026-09-23 | P12.1-P12.4 T11 compute | 8×8 RGBA8 compute filter、dispatch action/usage/seek、读写 descriptor、CS Pipeline/Texture/Resource UI 与 DDS/HTML；完整 T00-T11 回归、逐份 CLI replay、12×10 lifecycle 和最新 qrenderdoc 验收通过；下一项 T12 argument buffer |
| 2026-09-23 | P13.1-P13.4 T12 argument buffer | 单层 fragment argument buffer、texture id(0)/sampler id(1)、capture/replay、间接 usage、通用 descriptor/reflection、标准 Viewer 跳转、DDS/HTML；T00-T12 L3、13×10 lifecycle 与最新 qrenderdoc L4 全部通过，下一项 T13 indirect draw |
| 2026-09-23 | P14.1-P14.4 T13 indirect draw | shared buffer offset 16 的 `3/2/1/1` 参数、indirect action/usage、标准 Buffer/Resource/Pipeline、raw `.bin`/HTML、异常 offset 拒绝；T00-T13 L3、14×10 lifecycle 与最新 qrenderdoc L4 均通过，下一项 T14 indexed instancing/base vertex |
| 2026-09-23 | P15.1-P15.4 T14 indexed instancing | UInt16 byte offset 4、baseVertex/baseInstance 1、双实例、精确 IA/Buffer/Mesh/usage/seek、异常 offset 拒绝；T00-T14 L3、15×10 lifecycle 与最新 qrenderdoc L4 均通过，下一项 T15 point/line |
| 2026-09-23 | P16.1-P16.4 T15 point/line | Point/Line/Line Strip 非零 vertexStart、三种 action/topology/Mesh VS Input、264-byte Buffer/usage/seek、非法参数拒绝、UI DDS/HTML；T00-T15 L3、16×10 lifecycle（resident growth 737280 bytes）与最新 qrenderdoc L4 均通过，下一项 T16 vertex texture/sampler |
| 2026-09-23 | P17.1-P17.4 T16 vertex texture/sampler | 四象限 native/replay、直接 vertex binding、VS reflection/descriptor/usage、标准 Viewer、非法 slot/资源拒绝、UI DDS/HTML；T00-T16 L3、17×10 lifecycle（resident growth 114688 bytes）与最新 qrenderdoc L4 均通过，下一项 T17 batch binding |
| 2026-09-23 | P18.1-P18.4 T17 texture/sampler batch binding | VS/FS 批量 range、空槽清除、used/unused、四象限、异常 RDC；T00-T17 L3、18×10 lifecycle（resident growth 1015808 bytes）与最新 qrenderdoc L4 均通过，下一项 T18 storage buffer |
| 2026-09-24 | P19.1-P19.4 T18 fragment storage buffer | slot 3/offset 256/size 384、reflection/descriptor/PS usage、FS Pipeline/Buffer/Resource、HTML/CSV/raw；T00-T18 L3、19×10 lifecycle（resident growth 1556480 bytes）与最新 qrenderdoc L4 均通过，下一项 T19 vertex storage buffer |
| 2026-09-24 | P20.1-P20.4 T19 vertex storage buffer | slot 4/offset 256 used、slot 6/offset 320 unused、IA/storage 分类、reflection/descriptor/VS usage、VS Pipeline/Buffer/Resource、HTML/CSV/bin；T19/T18/T16/T02/T05 定向、本场景 10× lifecycle 与最新 qrenderdoc L4 通过，L3 未触发，下一项 T20 ICB |
| 2026-09-24 | BATCH21-22 编排 | 固定 T20 ICB + T21 indexed indirect 两阶段；每条立即做功能自动验证，批末最终构建上去重跑联合定向/CLI/lifecycle，并同一轮 qrenderdoc 验收两份 capture；本次仅文档变更，第一项 P21.1 |
| 2026-09-24 | BATCH21-22 关闭 | T20 单命令 ICB、T21 indexed indirect 的联合自动、8×10 lifecycle 与同轮 qrenderdoc L4 通过；L3 未触发，下一批 BATCH23-24 |
| 2026-09-24 | BATCH23-24 关闭 | T22 多命令/non-zero range、T23 indexed ICB 的联合自动、9×10 lifecycle 与同轮 qrenderdoc L4 通过；L3 未触发，下一批 BATCH25-26 |
| 2026-09-24 | BATCH25-26 关闭 | T24 reset/reencode、T25 mixed draw/indexed ICB 的联合自动、11×10 lifecycle 与同轮 qrenderdoc L4 通过；L3 未触发，下一批 BATCH27-28 P27.1 |
| 2026-09-24 | BATCH27-28 关闭 | T26 pipeline inheritance、T27 buffers inheritance 的联合自动、11×10 lifecycle 与同轮 qrenderdoc L4 通过；L3 未触发，下一批 BATCH29-30 P29.1 |

| 2026-09-24 | BATCH29-30 自动完成，L4 待用户 | T28/T29 native/capture/XML/Replay API、10 类异常拒绝、联合 T11/T10/T01/T18/T19/T12/T16/T17、11×10 lifecycle 全部通过；合并 GUI 验收单 `QA_BATCH29-30.md`，T28/T29 均待人工 QA，批次未关闭 |
| 2026-09-24 | BATCH29-30 关闭 | T28/T29 的缩略图、T29 `$action()` 及先前 GUI 项由用户同轮确认；T28 DDS、T22/T25 新事件树复验通过。自动定向、逐份 CLI 与 lifecycle 通过，L3 未触发；下一批 BATCH31-32 P31.1 |

2026-10-02 B381最终库150e4d323ec1343139960cf7f7f98ce7355a40ae87da58e6877010e54d9860e6，
大frame buffer定向6 captures/24 seeks/67 negative groups通过，详见BATCH381。
组合全量958d133d…308/7786/3080（resident growth12632064B）仅包含更早v40候选。
provider临时堆Count误用rounded Native容量，逻辑槽位数修复/隔离构建/实际UE重截进行中。
人工UI仍锁屏未验；实际UE未整帧GPU提交。持续目标active，无提交或推送。

2026-10-02 B382大typed table定向库4322af8c…：2 captures/16 dispatch seeks+8 EID0/21 API+CLI negatives通过；18MiB主表与98304B sampler表、逐空槽普通metadata/bias校验通过。UE0b2dbec9临时heap42→10，仍存在4 unknown非零，按UE精确三handle范围修正后模块7b160b…重新截帧中。精确全量/UI未验，实际UE未整帧GPU提交；详见BATCH382。

- B395 精确5b329da9全量通过：308/7786/3080，growth10403840B；B396正在修复逐调用compute indirect参数（参考Vulkan FetchIndirectData），未验证整帧UE。

- B396 精确5473713a定向4 captures/28 reset seeks通过，原v52复现EID11错误0/1/1、新版1/3/2正确；serial/concurrent、inline/batch-offset绑定恢复正常。精确全量进行中，实际UE仍未整帧GPU回放。

- B396 精确5473713a全量通过：308/7786/3080，growth11026432B，端点哈希不变。B397深度附件候选开始定向，未验实际UE/UI。

- B397 精确b38753a6：14captures/56cycles/236API+CLI negatives深度/MRT通过；间接4/28和late prefix11copies4/16/+16/144交叉通过。精确全量进行中，UE整帧/UI未验。

- B397 精确b38753a6全量通过：308/7786/3080，growth11321344B，端点哈希不变。B398 render/compute counter共用验证器候选开始定向；实际UE和人工UI未验。

B409候选62新CPU payload槽位：c4c60375定向6/24/108+24通过；真实UE CPU预检推进到dispatch streamOffset182336。最终全量/UI/完整UE GPU仍待验证，持续推进。

B410/411：候选63/64定向各6/24，保留producer/CPU源反例；真实UE预检越过两次descriptor dispatch与mixed CPU新槽位，推进到frame buffer texture view stream240320。完整UE GPU/UI仍未通过，目标active。


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
