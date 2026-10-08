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

2026-10-07 最新任务整改（覆盖历史场景许可策略）：先完成一组通用资源恢复与重定位机制，再继续 UE/Lumen 新案例。实际修改纹理/frame-view/coverage/runtime AIR preflight，区分 API/设备、恢复缺口、统一预算、历史场景限制与展示分析；coverage 不积累场景等级，Native 动态计算与 partial 展示独立。具体执行规则、逐函数 DX12/Vulkan 对照及剩余边界见 [GENERIC_API_RECOVERY.md](GENERIC_API_RECOVERY.md) 与根 AGENTS.md；光追开启门槛不变。持续任务 metal 已同步。

2026-10-07 [B544 buffer-texture/当前copy状态集成](BATCH544_API_RT_INTEGRATION.md)：固定backend/bundle4163d391。按VK/DX12对象/offset恢复帧view metadata，逻辑像素范围与根参数可共用backing；当前copy字节有效性独立历史opaque标记，像素restoration intervals独立CPU数值分析。两fresh view/两同跨提交atomics合计240事件、77坏组154 GPU前拒绝，shared-root真实先读像素且缺像素上传保留根上传仍拒绝；fresh RayQuery4253/4253/0/0与528uint/48事件、29旧capture58 API/CLI通过。UE已越过3853，到3854/278528 background GPU descriptor初态/namespace与Private producer链，仍API4，无initial/frameGPU/GPUwait，peak1,421,934,592bytes。未知writer动态heap资格仍需恢复，不将CPU展示未知永久拒绝；flagsfalse/目标active、整帧/full/final未验；8manifest/65源码四产品九文档全外盘，旧oracle/MSL/API失败保留。

2026-10-07 [B544 Native SIMD/纹理原子与读回集成](BATCH544_API_RT_INTEGRATION.md)：固定backend/bundle8d6ec636。Native lane不模拟CPU结果，phi保留来源，atomic store/load/RMW及真实producer/提交独立；texture→buffer共享布局/生命周期/alias/预算校验。两fresh同/跨提交144事件/EID0/64pixel/2112-byte读回padding、38坏组76拒绝；metadata-only48事件/15坏组30拒绝；fresh RayQuery4253/4253/0/0与528uint、6fresh无fragment174事件/4坏组8拒绝、29旧capture58API/CLI通过。UE越过3836，下一3853/277440 frame buffer-texture对象/写资格，unknownBuffer0/unknownCall0，commit3517632仍API4，peak1,275,609,088bytes，无initial/frameGPU/GPUwait；下一诊断实际view恢复，不扩CPU数值语法。整帧/最终官方/full未验，flagsfalse、目标active；7manifest/65源码四产品九文档检查点全外盘，中间FAIL保留。

2026-10-07 [B544 条件来源/重定位与纹理对象信息集成](BATCH544_API_RT_INTEGRATION.md)：backend/bundle529d0a7b。已恢复buffer/null动态读及字面量连续写保持Native来源；base/derived/spanning捕获VA独立要求重定位，不用CPU predicate绕过。尺寸查询用恢复对象/factory信息，pixel read仍验证初态/producer，Native内部readonly调用不生成假返回来源。fresh集成48事件/20查询点/40descriptor-location与15坏组30拒绝、fresh RayQuery4253/4253/0/0和528uint/48事件、6 fresh无fragment174事件/4坏组8拒绝、29旧capture58 API/CLI通过。实际UE越过1624/1625/3830，下一3836/274240 SIMD/texture atomics副作用及读写初态，commit3517632仍API4，无initial/frame GPU，peak1,419,739,136bytes；下一不模拟lane算法/不只删unknownCall，补恢复/提交闭包。standalone frame texture仍未实现，不能以placement成功替代。flags false、目标active，9文档/64源码四产品检查点全外盘；final/full/official/UE整帧未验。

2026-10-07 [B544 可选shader阶段与Native同步集成](BATCH544_API_RT_INTEGRATION.md)：backend/bundlef9718c71。按实际Native PSO允许普通vertex管线无fragment/有颜色和深度及合法残留fragment绑定，公开access不报告不存在shader的阶段；Native线程组barrier/fence与buffer原子效果分开，不模拟线程组算法。6fresh无fragmentcapture/174事件与4坏组8拒绝、fresh线程组同步/原子48事件/20查询及10坏组20拒绝、fresh RayQuery4253/4253/0/0与528uint/48事件、29旧capture58 API/CLI通过。实际UE无GPU越过draw3330304，推进commit3517632；PSO1624 unknownCall0，剩4 derived-null/GEP资格拒绝，未证明缺地址。下一独立来源/地址恢复，不增加数值/共享数组模拟。peak1,163,837,440bytes，flagsfalse、目标active；full/official/UE整帧未验，证据/64源码四产品九文档检查点全外盘。

2026-10-07 [B544 指针来源/原生调用效果与附件范围集成](BATCH544_API_RT_INTEGRATION.md)：backend/bundle2133a76f。分支、结构投影和原子保留资源来源；数值调用按实际声明记录资源效果，GPU仍计算，展示partial不拒绝，旧writer事实失效；真正嵌入捕获VA/未知GPU指针仍需恢复。8 fresh六/八MRT、200事件/EID0和5坏组10拒绝；fresh原子分支48事件/20公共查询与10坏组20拒绝；fresh RayQuery4253/4253/0/0、528uint/48事件，21旧capture42 API/CLI通过。实际UE无GPU越过首commit557952与六附件3233472，当前draw3330304的fragment阶段身份/可选性待核实，peak1,180,876,800bytes。未完成UE整帧/最终启用验收，生产flagsfalse、目标active；9文档/外盘检查点继续保存最新四层准则，不以CPU数值语法扩展代替API恢复。

2026-10-07 [B544 恢复与展示首段解耦](BATCH544_API_RT_INTEGRATION.md)：backend/bundle d17d244e。已恢复对象的动态 buffer 偏移及分支候选范围可保留 Native 重放、展示部分；未知写使旧scalar事实失效，真正来源/类型/初态/提交约束保持。三fresh普通Native/capture/API/CLI、144事件/EID0/60公共查询及35坏组70拒绝；fresh converted RayQuery4253/4253/0/0、528uint/48事件；21旧capture42 API/CLI通过。UE仍在248128 select丢失pointer identity处无GPU拒绝，不证明实际地址未恢复，下一补独立 pointer provenance/namespace资格。尚未完成全路径解耦/UE整帧/最终启用验收；flagsfalse、目标active。证据和检查点在外盘。

2026-10-07 用户最新方向（覆盖历史 unknown 一律拒绝策略）：将资源恢复、地址重定位、提交依赖与 shader 访问展示独立处理。正确重放的动态访问不能仅因 CPU 展示分析不完整而永久拒绝；未恢复 GPU 指针、失效初态、缺失资源/生产者必须补恢复机制。下一 B544 优先拆恢复资格与访问展示，不继续逐个扩充 CPU shader 表达式作为完整 API 适配。执行规则见 [RT_API_INTEGRATION_WORKFLOW.md](RT_API_INTEGRATION_WORKFLOW.md)。方向已调整，当前 backend 的既有展示 hard gate 尚未全部拆除，不能冒称已经实现解耦。

2026-10-07 [B544 通用附件 Load/颜色格式族与 Header 指针发布集成](BATCH544_API_RT_INTEGRATION.md)：仍为同一大批次，不增加微批。按本地 DX12 Preserve/Vulkan Load 保留 frame-born Private placement MRT/depth/stencil 的 Load/Discard，未定义像素不生成 CPU 地址事实；既有完整初态的普通 uncompressed UNorm/SNorm/float 颜色格式按 API 格式族支持，补 R8/R16 Snorm 尺寸查询，保留出生/alias/范围/usage/PSO/store 与预算校验。Header 的贡献指针发布不再要求被指缓冲从未被 GPU 写入，实际读取/已知内容和提交闭包仍单独验证。当前 backend/bundle `08677fd7`：12 fresh Native/capture/API/CLI 场景、384事件选择/EID0、每 plane 256像素；38附件坏组76 GPU前拒绝；Header 派生两合法控制及10坏组20拒绝；fresh converted RayQuery 4253/4253/0/0、528uint与公共查询/事件通过；21旧capture42 API/CLI通过。实际 UE 无GPU预检越过三 Header，当前首提交557952于 compute PSO1707/dispatch247360的3 unknownBuffer拒绝，peak1,637,203,968bytes；graphics commit消费证明、UE整帧输出/EID0/绑定仍未到达。新增GPU contribution复制 Native/capture成功，但实际 query consumer 及派生 Header-only 均拒绝，分别保留 FAIL/预期拒绝证据，不算支持；保留编译/旧helper范围计数及第二capture oracle失败和修复复验。全量official/IR/RT/lifecycle/Qt-ARC最终集中验收未跑，生产flagsfalse、任务active，历史崩溃/重启未证明修复。全部产物外盘，内盘32GiB，无提交推送/重置。

2026-10-07 [B544 通用 GPU 拷贝输入链继续集成](BATCH544_API_RT_INTEGRATION.md)：仍在同一大批次开发，按DX12/VK原生copy及提交顺序补齐Shared→Private→Private CBV输入，支持背景/帧内Private暂存、非零offset/分段拷贝、同提交/跨提交；真实shader writer使旧字节失效，未知GPU输出不回退旧初态。当前backend/bundle `f1988b67`：direct及三种copy-chain四fresh Native/capture/API/CLI通过（192事件/EID0、80公共access/descriptor/location检查、完整输出/padding）；45坏组90无GPUwait拒绝，另fresh合法native/capture中的opaque shader writer→copy链两次GPU前拒绝。fresh converted RayQuery4253/4253/0/0、528uint/48事件及四旧capture8 API/CLI通过；404日志无严格诊断，43源码/四产品与七文档外盘检查点。实际UE CPU审计确定首consumer提交参数[4,1,0,2]，四scatter目标索引860–863、96-byte typed descriptor payload；不是GPU验收。下一通用有界循环/data-indexed typed descriptor GPU producer及来源/重定位闭包，无UE/Lumen/shader名分支；未重复未变化的UE loop阻塞，当前UE initial/frame GPU/full集中验收未跑，两生产flagsfalse，目标active。新产物全外盘，内盘32GiB，清理19,066-byte可重建cache、没有本地Metal测试产物目录，无提交推送/重置。

2026-10-07 [B544 通用 runtime PSO 实际消费集成](BATCH544_API_RT_INTEGRATION.md)：同一大批次按immutable根ABI、实际AIR/当前typed descriptor来源和提交时已发布CBV字节支持普通runtime buffer访问，不再将绑定全局表中的未消费AS槽当作query许可；candidate必须在整帧GPU前完成提交闭包证明。write residency与实际shader writer分开，真实writer使parent/物理alias scalar事实失效，未知索引/范围/调用与runtime RT consumer仍拒绝。当前backend/bundle `f68bb0be`：一fresh原生compiled MSL准确ABI场景Native/capture/API/CLI通过，Private CBV GPU上传驱动A[3]=123→B[7]=456，完整输出/未写padding、48事件/EID0和20公共access/descriptor/location身份查询；9坏组18无GPUwait拒绝，含换入真实RayQuery metallib保留runtime元数据的反例（AS=1/unknownAS=1/unknownCall=8）。CPU icmp/select/动态struct GEP/未知调用测试、fresh converted RayQuery4253/4253/0/0及四旧capture8 API/CLI通过。43源码/四产品检查点/108日志外盘，严格诊断为空。实际UE启用新通用消费验证后在首个PSO437的循环/data-indexed scatter buffer访问拒绝（7 unknownBuffer），此前该普通PSO尚无完整消费证明；无initial uploads/frame GPU/GPUwait，peak RSS1,580,875,776bytes。下一按通用loop phi/实际输入索引及producer/alias闭包补齐，不能跳过验证或按shader名称适配；最终集中验收和UE GPU输出未完成，两生产flagsfalse，任务active。

2026-10-07 [B544 通用 placement buffer、无窗口捕获与设备生命周期集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，参照DX12/VK将buffer物理alias范围、逻辑descriptor retirement、资源保持与shader消费分开；buffer alias预算1MiB，texture预算不变，重叠writer使旧scalar事实失效。无窗口capture保存以及child代理持有device/内部frame record清理已补齐。当前backend/bundle `63ef3240`：224KiB/1MiB两fresh Native/capture/API/CLI、各48事件/EID0/完整物理重叠和padding；1MiB提前释放device引用fresh控制通过；8坏组16无GPUwait拒绝；20失败加载→10完整controller生命周期PASS（增长9,027,584bytes）；一fresh真实converted RayQuery Native/capture/API/CLI 4253/4253/0/0、528uint/48事件及公共typed查询通过，四旧capture8 API/CLI通过。118日志无严格诊断，39源码/四产品检查点外盘。实际UE已越过229376-byte alias阻塞，当前no-GPU在普通compute PSO3731/encoder14984的AS submission closure拒绝：绑定全局表中未消费AS槽被误作query权限；实际AIR含动态索引buffer store，下一按底层实际消费/当前typed根/descriptor/writer提交闭包实现，不能仅凭无ray调用放行未知地址。UE GPU输出/最终集中回归未验，生产flags仍false，历史UE/Qt崩溃和系统重启未证明修复。内盘32GiB/外盘1.6TiB，无提交推送，任务active。

2026-10-07 B544续作：aca98095增加实际sampler消费/public query，当前两旧真实capture扩展API/CLI、41坏组/3合法、20旧回归通过，详见[B544](BATCH544_API_RT_INTEGRATION.md)。下一优先generic AIR buffer pointer来源/读写及其他资源intrinsic（实际UE含get_width_texture_buffer_1d和gather），保持unknown拒绝；按真正shader消费连接runtime根/当前slot/producer提交闭包，不能因AS/texture/sampler子集已解析自动授权PSO。同一B544继续开发，固定最终库集中验收后回UE整帧。当前无fresh Native/capture/官方/full/lifecycle/Qt-ARC/UE整帧GPU验收，flagsfalse，目标active。

2026-10-07 B544当前开发更新：执行点已发布scalar/typed heap/Header快照和实际AIR消费链已实现，45c2a584当前库两fresh复合sample、35坏组/3合法、20旧API/CLI及真实值追踪通过，详见[B544](BATCH544_API_RT_INTEGRATION.md)。继续同一大批次，下一补实际runtime PSO的完整buffer/texture/sampler/GPU producer与动态索引消费闭包；仅部分AS/texture报告不授权UE。固定最终库集中受影响回归后短UE真实光追截帧/整帧重放/输出/事件/EID0/预算，再判断flags；当前flagsfalse。当前库官方/full/lifecycle/Qt-ARC/UE整帧未跑，失败与外盘检查点保留，目标active。

2026-10-07 [B544 通用 AS descriptor 与运行时绑定集成](BATCH544_API_RT_INTEGRATION.md)：同一大批次继续开发，参照DX12当前root table/AS descriptor和VK执行点参数/usage；AIR uniform解析支持匿名AS Header及实际AS调用，公共GetDescriptorAccess/GetDescriptors返回所绑定AS而非Header buffer。严格解析runtime binding ABI，核验根布局/绑定点/线程组与实际dispatch；runtime事实不能声明shader访问或开启消费资格，setBuffer替代setBytes会清空旧inline数据。当前backend/bundle `8caaecc0`、GUI `3ba30e36`、provider `2496f103`：两已有复合capture当前API/CLI PASS，每项48事件/EID0、144身份、48公共descriptor检查（含12 AS），五纹理全部528texels/CBV padding/六sampler；10 ABI坏组20无GPUwait拒绝+合法控制，10旧capture20 API/CLI PASS。中间71c9库两fresh Native/capture/API/CLI、21坏组42拒绝/两控制/旧20检查分别保留其hash，不冒充当前库fresh capture或最终验收。CPU解析119实际UE runtime payload+1 compiler payload及六真实query AIR形状通过；AIR使用合成loader，仅证明解析覆盖8 AS reset/93纹理调用，不证明真实资源依赖/UE输出。当前110完成日志无严格诊断，27源码/四产品校验备份及产物全外盘，内盘约32GiB。B544仍开发中，实际UE动态AS Header/自动PSO-heap消费闭包和整帧GPU/事件/EID0/预算未验，未重复已知失败UE；当前库官方/full/lifecycle/Qt-ARC未跑，两flagsfalse，历史崩溃/重启修复未证明。无提交推送/重置，持续active。

2026-10-07 [B544 通用多 UAV 与执行点依赖集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，PSO输出角色与资源身份解耦，多个texture UAV按当前heap slot解析；已验证槽位换绑、query纹理GPU输出→后续SRV消费的提交依赖及零调度。未证明的GPU buffer writer保持opaque，不能把旧初态用作后续消费证明。实现参照VK/DX12，无UE/Lumen/shader名分支。当前开发backend/bundle `105986c1`、GUI `3ba30e36`、provider `2496f103`；同一当前库两fresh Native/capture/API/CLI PASS：Private placement/五帧内2MiB CBV与Shared UAV/四帧内Private CBV，每项五纹理各528texels、48事件选择/EID0、144身份检查、36公共descriptor查询、完整CBV/padding/六sampler身份及只读usage，结果4248→4560和2375→2687。12坏组24无GPUwait拒绝+合法副本、10旧capture20 API/CLI定向检查通过，两真实compiler/AIR证明各两非零query；158完成日志无严格诊断。新19源码/四产品检查点及全部产物外盘，内盘约32GiB。B544未关闭，实际UE的通用动态AS Header/heap消费闭包及整帧GPU输出/定位/EID0/预算仍未验，本轮未重复启动已知失败UE；当前库官方/full/lifecycle/Qt-ARC未跑，两生产flagsfalse，历史崩溃/重启修复未证明。无提交推送/重置，持续active。

2026-10-07 [B544 通用纹理与 PSO 绑定 ABI 集成开发](BATCH544_API_RT_INTEGRATION.md)：同一大批次补齐类型化 texture SRV/UAV、Private/Tracked placement、原始创建 usage 核验及原生小堆范围；新增不可变 PSO 反射/运行时 ABI 捕获，保持 compiler JSON 与 runtime binding facts 的来源区别。当前开发 backend/bundle `96cf2591`、GUI `3ba30e36`、provider `2496f103`：新复合 Native/capture/API/CLI PASS（528 texels、56事件/EID0、48-byte帧内根/二维调度）；16坏组32无GPUwait拒绝+合法控制，10旧capture20 API/CLI定向回归PASS。实际新UE `403f90ea`/75,234,193bytes：119 PSO ABI/AIR核验、6光追PSO/5非零调度、40–72-byte根及原生线程组/绑定payload长度逐次一致，未依赖Lumen分组筛选；旧Lumen范围60 AIR/两非零query也通过。UE CPU65仍因动态AS Header/heap消费契约拒绝、无GPUwait/初态上传/frameGPU；UE输出/事件/EID0及总预算未验。B544未关闭，下一合并当前执行点动态/多输出和GPU生产依赖闭包，再固定库集中相关回归与短UE完整验收。两生产flagsfalse，无本库官方/full/lifecycle/Qt-ARC验收，不继承旧库；未证明历史Qt/UE崩溃或重启已修复。新产物继续外盘，未提交/推送/重置，持续active。

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

当前持续光追目标与验收门槛见 [RAYTRACING_ENABLEMENT](RAYTRACING_ENABLEMENT.md)，接续 [PHASE57](PHASE57.md)；[B476](BATCH476_APPLE_RAY_SAMPLE_AND_SIZE_QUERIES.md) 已接入官方 sample 并补尺寸查询。原生通过，sample 捕获仍失败，先补 Managed/GPU 构建输入及 TLAS，再进入本机 UE 实际光追；能力尚未开启。下方保留早期阶段历史。

当前接续优先级以 [PHASE56](PHASE56.md) 为准：Shared AS更新/复制/压缩初态已闭环，下一阶段是Private与GPU执行点输入、boxes/多geometry等。当前定向结果见 [B475](BATCH475_RAY_COMPACT_INITIALS.md)，设备光追能力仍false。

# Metal Replay 总体计划

2026-10-05 当前光追回放阶段见 [PHASE55](PHASE55.md)：B471–473 帧前非索引/索引 BLAS 和 TLAS 初态已闭环；下一项 refittable/refit/copy/compact 初态和 GPU 构建输入。按 Vulkan/DX12 实际支持边界推进，不扩展光追内部调试。

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


10-01 [BATCH334](BATCH334_UE_SOURCES_AND_FROZEN_SNAPSHOT.md)–
[BATCH336](BATCH336_CAPTURE_QUEUE_COMPLETION.md)：真实来源/inline provider、
冻结 metadata、sourced slot 小帧 replay 与应用队列完成边界通过。新 UE 帧
1,476 活槽与实际初始字节全部一致；下一项 sourced GPU 更新、同帧多提交与
alias/render 生命周期。完整 UE 仍未 GPU验收，持续任务 active。


10-01 [BATCH332](BATCH332_INLINE_DESCRIPTOR_BYTES.md)/[BATCH333](BATCH333_UE_PARTIAL_PROVIDER.md)：
compute inline typed bytes已通过GPU小例，56负例通过；官方源码与隔离局部MetalRHI
编译路线已建立，诊断slot provider正常Metal启动通过。持续自动重截→CPU审计→
适配driver的slot generation/动态slice，再render stages/alias/预算与实际UE replay。
当前目标active，完整UE未放行；全量未跑，新增Viewer未验。

10-01 [BATCH331](BATCH331_FRAME_DESCRIPTOR_RESOURCES.md)：frame Shared/Private placement和
buffer-backed view的小帧身份/时序/回跳已实现，48负例通过。接下来driver有效slot/
动态slice/inline typed bytes契约与对应Engine provider，再render/alias和预算。
已询问可重编译UE5.8.3源码环境路径；完整UE仍拒绝，当前无需重截。

09-30 [BATCH330](BATCH330_DUPLICATE_IDENTITIES_AND_VIEWER.md)：164条重复身份
误拒绝已修；344条新身份精确分类为278placement buffer/66buffer-backed view。
两类小帧人工Viewer已验，继续极小帧内placement身份/创建时序/回跳恢复，随后
临时表/有效槽位/CPU来源/render；UE整帧仍未放行，用户无需再次重截。

09-30 [BATCH328](BATCH328_EXPLICIT_DESCRIPTOR_REPLAY.md)/[BATCH329](BATCH329_UE_DESCRIPTOR_LAYOUTS_AND_DRAWABLE_LIFETIME.md)：
显式小表重定位及GPU/CPU混合更新已实际replay通过；UEprimary布局和截帧结束/
drawable生命周期修复已自动重截成功。下一阶段：极小帧内资源/身份生命周期、
临时table/payload slice、有效槽位与CPU写入来源，然后render/packed uniform；
现有UE仍拒绝整帧提交。人工小帧Viewer待用户解锁，本机3.5GiB真帧尚未放行。

> **实施门槛：**所有新的真实应用失败先按
> [跨 API 横向排查顺序](CROSS_API_TRIAGE.md)检查 D3D12/Vulkan 的现成方案、
> UE/Unity 是否专门适配，再只对 Metal 特有约束设计实现；不能直接删守卫。

## 当前优先级：黑盒回放、稳定性、真实普通帧

09-30 [BATCH327](BATCH327_GPU_IDENTITY_RESOURCE_CLOSURE.md)：用户新帧已取得，
间接资源capture过滤遗漏已修并自动重截，无需用户再次按钮操作。新原生跨进程
typed packet重编码验证了buffer+offset/texture/sampler与普通常量保留。
下一步将显式schema及资源+offset关联接入replay，处理UE临时descriptor payload、
GPU写入/复制、CPU快照执行点及stale/alias生命周期，再扩大GPU负载；现有
完整UE仍明确拒绝，不以诊断或原生算法证明冒充UE回放支持。

09-30本地 [BATCH326](BATCH326_GPU_IDENTITY_DIAGNOSTICS.md)：新UE帧仍含6 GiB
heap和原进程GPU VA/texture ID，先补查询身份diagnostic capture与GPU前拒绝。
下一项是用户用新诊断库重截，CPU核对mapping覆盖，再实现完整Shader Converter
descriptor重定位（offset/typed view/sampler及帧内GPU写入/复制）。此前不直接
在16 GiB本机打开完整UE帧，不把diagnostic chunk视作bindless支持。

09-30 [BATCH325](BATCH325_WINDOWSERVER_WATCHDOG_2026-09-29.md)：23:15
WindowServer watchdog 已导致第二次本机重启；前一轮超时的两个 UE 回放
探针仍见于重启前 stackshot 的 GPU 内核路径。**停止此远程机 UE 大帧
GPU/GUI 回放**；超时或 SIGKILL 不构成安全保证。可做 CPU 静态审计、
编译和不涉及 GPU 的日志工具；GPU 功能族实证转到可本地恢复的 Mac。

09-29 [BATCH324](BATCH324_UE58_GPU_WAIT_AND_REMOTE_HOST_STOP.md)：本远程机
的 UE 真帧诊断副本已到 GPU 完成等待，采样见 8.3 GiB footprint 与短暂
WindowServer 不就绪。暂停本机 UE 大帧 GPU/GUI 测试；先静态排查
command buffer 完成与资源规模，并使超时/物理 footprint 监测可靠。
不得跳过等待或删除 `Empty` 守卫求打开。旧帧仍未正常开启；重启测试须
在稳定环境且一次性验证 API/CLI、像素、MRT、scope 和 GPU VA。

09-29 [BATCH323](BATCH323_TERMINAL_PURGEABLE_REPLAY.md)：当前 Mac 的
4 KiB 原生/注入/API/CLI/GPU 字节及 seek 小夹具已通过，帧尾 buffer
`Empty` 接通。原 `UE58_capture.rdc` 仍有 15 个旧 view 顺序错误；已生成
哈希与 BATCH321 相同的诊断副本。下一步由用户对**诊断副本**做**一次**有日志、
超时与 RSS 上限的 API-only 回放，确认是否越过首阻塞；再检查场景
scope、MRT、像素与 GPU VA。不要将小夹具结果当作 UE 真帧或 UI 通过。

09-29 [BATCH322](BATCH322_UE58_CPU_AUDIT_AND_COMPLETION_PROBE.md)：
先在另一台可承担 GPU 测试的 Mac 显式运行 4 KB 原生 completion/Empty
探针，再据实测决定 Metal 回放的完成边界方案；当前远程机继续只做
CPU 审计与构建。真帧含 Nanite/10 个 MRT pass，不能把旧 Slate 帧的
单 RT 黑屏结论套用到它。实际回放仍安全拒绝，不能删守卫求打开。

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

2026-10-05 B493定向验收：全禁用Indirect槽位kind11/schema9，仅接受每packet ASID0/mask0，完整transform/flags/IFT offset/userID冻结。最终backend/bundle 08028e4c7e2048c3ac37d0de00d712de139c3026b4960d8c836d4cc12f3b255f，GUI3ba30e36。五例Shared帧前/帧内、Private/placement帧前、同CB distinct alias擦零：native/capture/API588事件/EID0/CLI及完整72-byte snapshot oracle PASS；4×43+36=208坏输入、20旧检查/T12416反例、6×10生命周期PASS（growth425984bytes/hash一致）。产物captures/metal-ray-b493、build-macos-debug/metal-ray-b493/indirect-manifest.json。官方/UE/68份RT与本库集中接续中；ARC/原Qt crash/GUI/UE RT未闭环，公开能力false，未提交/推送。

B493 08028e4c接续验收：官方两scene/10尺寸查询/41坏sample/6能力查询、68份B482–493 RT7785事件、AS身份旧反例/schema2/旧Indirect与空TLAS反例PASS；固定库集中308/7784/3080 PASS，growth0bytes/exit0/工作和冻结库起止hash一致。产物frozen-validation-08028e4c/full-regression.log与metal-ray-b493/followup-manifest.json。UE session20261005-225938进入截帧后Private/Tracked placement、54实例、background0被bridge主动ForceCrash拒绝；仍未证明RT dispatch/输出/离线。B494已编辑，待串行构建/GPU，不能计此证据；生产能力false。

2026-10-05 B494定向验收：帧内Private/Tracked（独立或placement）间接实例输入按AS encoder消费点冻结，提交完成后保持原API chunk位置/metadata写入；重放typed live child/UserID staging或全禁用Indirect。精确范围别名仅在已验证冻结build、native encoder关闭且无descriptor backing冲突时允许，EID0清空资格。最终backend/bundle 111c4787c2a7fddf59bdd7503e2ea89f8227c23d72962f584942cb7e2e65a14d，GUI3ba30e36。七例native/capture/API954事件/EID0/CLI3 PASS，含同TLAS两build原始userID73/74、两种inactive完整bytes oracle及两份合法CPU别名提前创建控制；5×43+2×37+6=295坏输入/alias反例、20旧检查/T12416及8×10生命周期PASS（growth1081344bytes/起止hash一致）。产物captures/metal-ray-b494、metal-ray-b494/indirect-manifest.json。两次实现/判定FAIL日志保留，不计通过。官方/UE/75份RT和本库集中尚未跑；08028e4c集中308/7784/3080/growth0属于旧库。公开能力false，ARC/原Qt crash/UE RT未闭环，未提交/推送。


2026-10-07 [B544 buffer 实际消费与纹理尺寸查询](BATCH544_API_RT_INTEGRATION.md)：保持同一大批次，新增 AIR 全局 buffer load/store/atomic 依赖报告、实际 thread-grid 与 CBV scalar 的保守地址范围、当前 typed buffer descriptor 查询及尺寸 intrinsic 资源依赖；参照DX12/VK资源+offset/当前descriptor，未加UE/Lumen特判。当前 backend/bundle `ec73a2f1`：两 fresh native/capture 后的当前 API/CLI 通过，Private placement buffer 528uint/12公共buffer查询、五纹理各528texels/144身份/48公共descriptor/72sampler查询，均48事件往返/EID0；真实三消费点各14buffer读，raw另1写，9/11texture和6sampler且无unknown。58坏组116无GPUwait拒绝+3合法控制、10旧capture20API/CLI通过；456日志无严格诊断，28源码/四产品外盘检查点。首次构建/工具编译/80-byte工具重建/证明脚本旧假设FAIL保留并修正，未计PASS。B544/PHASE59和RT启用验收仍开放，两flagsfalse；当前库官方/full/lifecycle/Qt-ARC/UE整帧GPU未跑，不能证明历史崩溃/重启修复。优先完成通用runtime PSO/实际资源依赖/动态Header闭包再固定库集中验收及UE；新产物外盘，内盘约32GiB，无提交推送/重置。

推进重点：下一阶段优先让真实runtime PSO依靠当前绑定、完整实际AIR资源消费、已证明GPU producer与动态Header提交顺序完成通用闭包；避免把覆盖更多夹具变成关闭任务的替代条件。能力开关继续以官方sample和UE整帧固定版本集中证据为终点。


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

### 2026-10-07 B544 接续（acc8b703，开发中）

2026-10-07 [B544 通用有界循环和描述符角色继续集成](BATCH544_API_RT_INTEGRATION.md)：同一大批次，按提交时已发布输入证明零起点、单步有界循环和间接索引范围，保留typed pointer来源；读写重叠（含物理alias）、未知/越界输入拒绝。统一buffer SRV/UAV、typed buffer与CBV角色，公共buffer查询保留逻辑view长度，无UE/Lumen/shader名分支。backend/bundle `acc8b703`：两fresh循环Native/capture/API/CLI通过（直接和跨提交Private拷贝，96事件/EID0、40 access及120 descriptor检查、完整输出/只读/padding）；29坏组58无GPUwait拒绝，含实际触发的索引自写重叠和逻辑view越界。fresh converted RayQuery4253/4253/0/0、528uint/48事件及四旧capture8 API/CLI通过；244完成日志无严格诊断，45源码/四产品外盘检查点。UE实际首scatter和后续同类compute闭包通过，当前no-GPU预检在depth-only graphics draw的全局AS槽资格拒绝（峰值1,601,683,456bytes，无initial/frame GPU/GPUwait）；CPU原始AIR/当前输入和vertex linked stage-in调用已留证，均非UE输出验收。下一通用graphics linked function/vertex-instance输入及描述符实际消费，再补RT runtime consumer闭包；最终集中验收未完成，两生产flagsfalse、任务active。新产物外盘，内盘32GiB，无提交推送/重置。

详细当前范围/通过/失败/未运行见[B544有界循环开发证据](BATCH544_API_RT_INTEGRATION.md)。下一先完成通用graphics linked function及vertex/instance数据依赖、实际heap消费，再接runtime RT AS/Header闭包；不按Lumen效果适配。相关功能固定后一次集中最终验收，再验证真正UE光追截帧输出/事件/EID0/绑定和原生能力，才开启对应flag并复验。

### 2026-10-07 B544 graphics接续（39ff0128，持续active）

2026-10-07 [B544 通用 graphics linked stage 与 AS 初态依赖继续集成](BATCH544_API_RT_INTEGRATION.md)：仍为同一大批次。按真实 Native linkedFunctions 身份、vertex/instance/base/index 参数、当前 typed inline/heap 来源和提交输入证明普通 graphics buffer 消费，保留 AS 黑盒；完整创建数据可作初态但 writer/alias 后不回退，CPU/GPU 发布与当前逻辑 view 继续核验。修复 Header 快照在 background 状态的帧引用被忽略而遗漏 BLAS 初态的问题；公共查询按已证明事件/阶段/管线呈现实际 heap buffer access，不扫描闲置 AS，不按 UE/Lumen/shader 名授予支持。当前 backend/bundle `39ff0128`：fresh linked graphics Native/capture/API/CLI PASS，32事件/EID0、每次256深度像素、16 access/descriptor/location检查；6坏组12 GPU前拒绝。真实 converted RayQuery fresh PASS（4253/4253/0/0、528uint/48事件）；跨提交Private循环链 fresh 48事件/20 access/60 descriptor及17坏组34拒绝，四旧capture8 API/CLI通过。AIR入口直接参数/嵌套ordinal、converted附加metadata、linked返回值/缺失调用/instance下溢CPU通过。实际UE无GPU预检已越过旧depth-only draw资格guard，但其graphics提交闭包仍未到达；当前停在frame MRT renderpass303616的Load/initial/alias初始化语义，peak RSS1,612,087,296bytes，无initial/frameGPU/GPUwait，不计UE重放PASS。中间metadata回归及修复后复验分别留证。本批未关闭，生产flags仍false，最终official/full/lifecycle/Qt-ARC/UE整帧未验，历史崩溃/重启未证明修复。新产物全外盘，内盘32GiB，无提交推送/重置，目标active。

下一通用frame attachments Load/DontCare与初始化/alias顺序，随后actual UE graphics提交证明和runtime RT AS/Header闭包；固定集成范围后集中最终相关回归与真正UE光追输出验收，才启用对应flag。保持较大API批次，不新建每个接口的小批次；详见B544本次证据。
