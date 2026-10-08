2026-10-08 Release/UE 插件交付：[应用与自动附加唯一记录](RELEASE_2026-10-08.md)。`v1.46-metal.1` 已发布优化 arm64 app（source127f，backend270b）；构建/打包/搬迁/CPU运行时通过，GPU/输出本候选NOT_RUN、整体INCOMPLETE。插件0.2提供Project Settings的app/dylib路径与PostConfigInit一次re-exec附加，最终UE5.8.3 CPU正例/路径优先级/默认跳过/保护实跑通过；新UI按钮GPU截帧尚未验。英文/简中README与只含qrenderdoc内容例图替换入口；历史1845/f1图像及矩阵不继承新库。

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

B544 续作入口：当前 backend/bundle完整SHA `466a6485d46eab93478646347d29f443fb705931b555e545c14db3e60c3b132b`，源码/库校验备份在外盘 `metal-ray-b544/checkpoint-submission-output-466a`，哈希清单 `development/checkpoint-submission-output-hashes.json`。当前库 `private-output-frame-CBV-2D-closed` / `private-placement-output-large-frame-CBV`、`private-output-closure-bad`、`legacy-submission-output-current` 均已终结PASS开发定向检查；没有活跃build/GPU/UE/qrenderdoc。下一在同一B544补typed texture UAV/多输出通用资源闭包、实际query PSO根布局及执行依赖的capture元数据，再固定最终库集中验收→实际UE。未声明GPU计算产生的CBV仍拒绝；帧内standalone Private birth仅≤64KiB，Tracked placement沿用既有allocation限额，Shared/WriteCombined帧内backing沿用8MiB与总proof64MiB；初态backing沿用128MiB总资源预算且每根read≤64KiB。根count/role/地址唯一性及sampler逐slot核验保持。B542实际4318是2MiB options1背景buffer：creation initialData为空，但另有完整2MiB InitialContents，五个帧内部分CPU snapshot；不能把空creation字段误报成缺初态。当前没有新UE结果，勿重复旧同一guard失败或提前开flag。旧e8f续作说明是历史记录，以本段为准。

2026-10-07 [B544通用API集成继续开发](BATCH544_API_RT_INTEGRATION.md)：保持大批次合并/开发定向验证/固定最终库集中验收，Lumen仅作实际应用验收，接口与初态/重放继续参照VK/DX12。新增共用IR compute混合根类型与texture SRV依赖，static sampler逐slot来源/身份重定位，Private（含Tracked placement）初态CBV范围读取；不按shader/pass/效果特判。当前开发backend/bundle e8f709bf、GUI3ba30e36：40-byte四Private CBV+六sampler、48-byte五Private placement CBV+六sampler/二维[2,66,1]/64几何实例/unretained，两fresh Native/capture/API/CLI PASS（528uint各、48/56事件及EID0），六真实texture sample AIR调用/所有sampler ID变化/完整padding与read-only usage；27坏组54无GPUwait拒绝+合法对照、五旧capture API/CLI十检查PASS。mixed5早期初态拒绝/验收工具悬空SD引用崩溃已保留并修正，不等同此前UE/Qt崩溃修复。B544未关闭、未集中验收；下一帧内CBV producer-consumer与texture/Private输出通用闭包，再一次集中相关回归→短UE实际光追截帧/输出/事件/预算。两flagsfalse，无新UE或本库官方/full/lifecycle/Qt-ARC验收，不继承旧库。219目录17.3GiB外盘校验迁移与58 cache清理已完成，原路径链接、活动B544产物继续外盘；内盘约32GiB可用。未提交推送/重置，持续active。

B544续作入口：活动目录是外盘链接，backend/bundle e8f709bf、GUI3ba30e36；build及所有GPU/helper子进程均已终结，无后台UE/qrenderdoc。当前库development/mixed5-private-40及mixed6-private-placement-2D完整gate PASS，mixed6-closure-bad 27组/54拒绝+1合法，legacy-e8f三旧/legacy-CBV-e8f两纯CBV共十API/CLI PASS；development/checkpoint-e8f-hashes.json记录库/GUI哈希。旧mixed5-closed为9f2bb2ec，其他21b6/64ad失败或开发结果仍保留，不能把它们当e8f最终集中验收。共用RayIRLocalRoot/RayIRHeapEntry用于IRComputeRoot/HeapEntry两新末尾chunk，Max1442；目前只支持CBV/static sampler根与readonly RGBA8小texture SRV、完整初态CBV≤64KiB，Private允许standalone/Tracked placement，sampler表保持Shared immutable且逐slot type7/kind2完整声明。下一在同一个B544补帧内/Private CBV生产依赖、buffer/texture输出公共闭包，接真实PSO metadata后固定最终库一次集中相关验收→短UE。不要重复旧UE同一guard失败，不要启用能力，不新增每接口独立BATCH。验收helper现在关闭capture前缓存sampler offset，禁止再次使用file->Shutdown后的SD引用。详见B544开发记录。

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

下一official replay→run_followup.py（坏sample/能力→UE→冻结63份RT/old身份/schema2/indirect旧反例）；构建/GPU串行，下一按UE新失败补。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B491](BATCH491_RAY_MANY_INDIRECT_CHILDREN.md)：间接TLAS的typed子AS预算1024，直接仍4；修复描述工具固定4项栈数组越界。backend/bundle e6f18f11，GUI3ba30e36。5/33/frame33/Private33/placement alias128五例native/capture/API588事件/CLI、226坏输入、20旧检查/T12416及6×10生命周期PASS，growth327680bytes/hash一致；实际最多128子已验，不把1024预算称为GPU验收。官方两scene/10查询PASS；41坏sample/6能力查询、UE、58份RT及本库集中串行运行中。f991栈溢出与测试错误判定失败均保留，不计验收。生产能力false，ARC/原Qt crash/GUI未闭环，持续推进、未提交/推送。

当前运行metal-ray-b491/run_followup.py：官方反例/能力查询→UE→冻结58份RT→集中回归，构建/GPU串行。下一按真实UE新失败补功能。

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

2026-10-03 接手最新先看 [B452](BATCH452_VULKAN_PASS_AND_VIEWPORT_OVERLAY.md)。当前库 `0776deb7` 的七种 Native 定向、pass oracle、同一 UE 六覆盖层两周期及原始字节恢复通过；本库无全量，ac2a9748 全量是前批历史。助手 UI 的 Viewport/Scissor→Depth→None 切换和原图恢复通过，界面停在 EID3300 SceneColor。GPU 串行，持续目标 active，保留改动、不提交/推送。后续仍需 Clear Before、Quad/Triangle 与 shader depth export 原 FS 路径。

2026-10-03 当前接手先看 [B449](BATCH449_TEXTURE_VIEW_INSPECTION.md)、[B450–451](BATCH450_451_NATIVE_DRAW_OVERLAYS.md)。当前库 `ac2a9748` 已通过 Texture Viewer 定向、临时 DepthStencil/Sampler 捕获与同一 UE 五绘制覆盖层两周期及原始字节恢复检查；固定库全量308/7786/3080通过（growth8716288B/hash一致），全量后同一 UE 五覆盖层及 GBuffer Range/Histogram 两周期通过，完整 UE 两次 reset 呈现图像保持基线；实际助手 UI 的五绘制覆盖层、Range/自动范围/Histogram/数值覆盖层/Pixel Context 均通过，界面停在3300 SceneColor。保持 GPU 串行，不提交/推送；以下 ff444e68 验收只作历史。后续还需 shader depth export 的原 FS/stencil mask、其他绘制覆盖层及 pass 归属。 持续对齐目标保持 active；五项通过不代表所有 Overlay 已与 Vulkan 平齐，后续优先 pass 归属和其余 N/A 项。

2026-10-03 最终解锁验收：当前 ff444e68 定向、固定库全量及全量后新旧 UE 均通过；实际助手 UI 现已完成（新 UE 六个命名输入、GBuffer 场景、usage Escape/鼠标外点击/跳转4057、公共 Shader Viewer 的 AIR 与 Captured MSL）。界面已重新打开新副本，停在3300 Fragment Shader。当前已报告问题与支持范围内的检查流程通过，用户画面反馈仍待；不代表任意 Metal API 或动态 bindless 全量反馈已经平齐。未提交/推送。详见 [B448](BATCH448_SUBMISSION_DESCRIPTOR_SCOPE.md)。

2026-10-03 [B446–B447](BATCH446_447_FRAME_FLOAT_AND_VISIBILITY.md)、[B448](BATCH448_SUBMISSION_DESCRIPTOR_SCOPE.md)：ff444e68定向、固定库全量308/7786/3080（growth5488640B/hash一致）及全量后新旧UE通过。新截帧光照3300的GBufferA/C与Nanite后4349、旧基线逐字节一致；三份呈现图像及原始JPEG精确一致，真实GBufferA名称与三类usage正确。旧3371/3551/188及整帧两次重置保持原图像。最终UI仍待再次解锁（终端完成后CUA重查仍锁定），无GPU进程存活；持续目标blocked（本次解锁恢复后连续三轮复查仍锁定，必须再次手动解锁完成最终UI），未提交/推送。

2026-10-03 [B444–B445](BATCH444_445_RESOURCE_USAGE_AND_UI.md)：补齐真实资源label、graphics附件usage和texture view parent usage；同一UE GBufferA已有3147 Clear/3310 FB Color/3371 FS Resource，GBuffer字节不变。开始对齐DX12/Vulkan shader紧凑头部、descriptor跟随标题及pass摘要；当前dafb37a9定向及同一UE两次重置通过，固定库全量308/7786/3080通过（growth9453568B），全量后同一UE3371/3551/188及整帧两次重置通过，GBuffer A/C与呈现图像SHA保持一致；UI已有部分通过，菜单外点击及最终界面待Mac解锁后验证。目标blocked（连续三轮确认Mac锁定，等待用户手动解锁），未提交/推送。

当前 shader/Inputs 工作见 [B443](BATCH443_UE_SHADER_INSPECTION.md)，在 B440–B442 基础上，继续使用同一 UE capture；分项验收状态以该报告为准。

最新本地未提交改动：先看 [B440–B442](BATCH440_442_UE_EVENT_NAVIGATION.md)。用户 DX12/Metal 对比反馈已定位到真实光照部分回放漏掉先提交的 Nanite producer；c7bb6da1 定向及同一 UE 数据验证通过，最终全量/UI 状态以该记录及 STATUS 顶部为准。不要把此前 b814 整帧通过当成每个中间 draw 都正确。

# Agent 交接规范

2026-10-02 当前基线见 [UE replay checkpoint](CHECKPOINT_2026-10-02_UE_REPLAY.md)、
[STATUS 顶部](STATUS.md)和[本机运行入口](../../util/ue/README.md)。用户已要求将
B326–B439 当前改动提交并推送，随后提供画面差异继续改进；终端、真实UE和助手UI
验证已有记录，用户最终画面验收仍待反馈。下方为早期交接历史。

临时交给公司 Codex API 时，请先看[2026-09-28 交接单](COMPANY_CODEX_API_HANDOFF_2026-09-28.md)。
当前接手请先看 [STATUS 顶部](STATUS.md)、[BATCH301](BATCH301.md)、
[BATCH302](BATCH302.md)、[BATCH303](BATCH303.md)、[BATCH304](BATCH304.md)、
[BATCH305–306](BATCH305-306.md)、[BATCH307](BATCH307.md)、[BATCH308](BATCH308.md)、
[BATCH309](BATCH309.md)、[BATCH310](BATCH310.md)、
[BATCH311–312](BATCH311-312.md)及
[黑盒验收门槛](BLACKBOX_GATE.md)。最新已接通空 Binary Archive 的
compute/render/单个visible函数/单节点stitched library/tile/mesh函数添加，
并修正旧格式兼容读取；T311/T312 进一步接通同步tile/mesh pipeline
archive-miss依赖。T301–T312 原生/捕获/API/CLI、本族315例畸形输入和32份
定向回归通过。当前库/app `2f88058307f5…`，GUI待验274份，原始
bridge/chunk 宏匹配59/15（含新功能的防御性拒绝及定义，不等于未实现 API 数）。06:26 IOGPU panic
仍未归因，本轮没有新增；继续避免长时全量GPU压力回归及GUI/Computer Use。
下一项优先依据真实应用普通帧阻塞，或为当前硬件可验证的剩余资源族
先做原生能力探针；不要仅为降低宏数字删除守卫。

## 当前执行规则：2026-09-26 节奏调整

用户要求加快连续开发。以 [PLAN.md 的当前推进节奏](PLAN.md#当前推进节奏功能族连续开发定向验证集中回归)
为准，取代本文件下方旧的逐PHASE/逐小批完整收尾与全索引同步要求；历史证据不变。
先读STATUS顶部短检查点、PLAN当前节奏、活跃BATCH和QA_PENDING当前表；不通读本文件历史。
每波连续2–3功能族，本族闭环+受影响旧T及时测；最迟第3族后、QA交付前集中回归，
风险无法局部界定则提前。快速入口 `test_metal_replay_targeted_macos.sh` 不代表全量通过。
日常只更新短STATUS/一份BATCH增量及必要QA短行，详细QA在集中关口合并；不自动启动GUI。
中断时必须保留族数、已跑/欠跑检查及日志、首个未完成项，不能因chat结束把全量欠项抹掉。
“定向通过”“集中通过”“用户L4通过”分别记录；未验阶段不关闭。以下为历史交接证据。

## 2026-09-26 最新：BATCH53 自动通过，148 bridge / 87 chunk 剩余

继续终端开发、禁止 Computer Use/GUI 的要求不变。本批单命令 reset 减少1 bridge，
blit ICB reset/copy/optimize 消除3旧chunk1224–1226；后三项bridge原本已转发。
追加1270 CPU命令reset、1271不可重建初值诊断，Max1272，资源类型枚举未改。

ICB replay 每个epoch首次使用时原地恢复CPU初值，不能替换native ICB造成command wrapper
悬空；OnlyDraw延续WithoutDraw。GPU copy/reset更新shadow，保留pipeline/两个vertex
buffers/indexed参数；空命令有SetMarker子项，不伪造draw。优化范围按CB/epoch记录，
同提交重叠在Metal前拒绝，零长度不编码。所有范围采用减法检查以防溢出。

边界：CPU初始化Shared render ICB；非Shared在创建阶段拒绝。capture epoch/GPU改写标记
检测跨capture边界不可恢复的内容，插入1271让离线明确失败。完整CPU reset可清理未知
标志；保守地不允许仅靠帧内GPU覆盖绕过。不能删诊断chunk来静默接受错误初值。
现有CPU编辑仍按初始化record保存，不支持任意帧内CPU重编码/提交交错、GPU shader
生成或compute ICB。通用initial contents和其他wrapper所有权缺口仍在。

最终一键 `test_metal_capture_batch53_macos.sh`：54 API/CLI、1330异常、540 lifecycle，
growth2,228,224bytes；26份API验证层。新194异常+3合法变体，旧70 ICB异常加入总脚本；
T22/T24两个空命令从负例改为三轮正例。六旧脚本加30秒超时/禁止信号退出，修复旧T26
缺失render pipeline调用native时崩溃。T53 native/capture各12帧；另真实帧前GPU内容
变体各3帧正确、离线明确拒绝（不计入54/1330）。正式T01–T52未重录。

仅replay设LAST_TEST=53；日志 `/tmp/metal-batch53-final.log`；库/app `a5dcf57234590…`，
T53 `6ad98408529a…`，GUI executable未改。完整hash/功能和测试范围见BATCH53/PHASE53。
无GUI/Computer Use、无提交，用户UE路线与旧修改保留。下一chat先读QA_PENDING及
QA_CONSOLIDATED：**T34–T53与T10 marker共21份**待人工；PHASE35–53和对应batch仍开放。
下一编号T54/PHASE54。以下为历史检查点，不代表当前剩余计数。

## 2026-09-26 最新：BATCH51–52 自动通过，149 bridge / 90 chunk 剩余

用户本轮要求 bridge 降到 150 内，已从 158 降至 149。T51 六异步创建入口追加
chunk1264–1269（Max1270）；T52 newEvent/signal/wait 接通旧1032/1062/1063，旧未处理
chunk93→90。资源类型 eResEvent 追加在 Fence 后，eResMax20。

异步桥接继续调用原生 API，完成时包装结果并登记父依赖，再传 error/reflection 给应用。
source/descriptor 调用时复制、native function 解包，快照不受后续修改/释放影响。结果
借用引用需 retain，回调后释放临时 proxy；PSO/Event 使用独立 native 所有权。Library/
Function 沿用 BATCH48 策略，其他 wrapper 生命周期并未普遍修复。Async source 非 nil
compile options 标为 unsupported；离线不执行 blocks、不还原 callback 调度。
共享 source serializer 增加 device/ID 验证，同时补 WrappedMTLDevice eResDevice 类型。

Event 每个新 replay epoch 懒创建新 native 对象，避免信号值从后来事件泄漏到回退；
先完成旧提交，OnlyDraw 延续 WithoutDraw epoch。只接受当前未提交/无 encoder 的 CB、
递增 signal、wait≤此前捕获 signal。双队列三提交已测，但 replay 仍串行，future-signal
wait、外部/帧前状态及 SharedEvent 未支持；不要直接放宽检查而引入 GPU 死锁。

最终一键 `test_metal_capture_batch51_52_macos.sh`：53 API/CLI、1066 异常、530 lifecycle，
growth1,736,704bytes；18 份 API 验证层。两 fixture 各 native/capture12 帧，T51 七个
completion（含失败）及五 PSO，T52 每进程72次同步。新增104+122异常，四个合法变体。
正式 T01–T50 未重录；另三份 source 新录兼容全部通过。仅 replay 设 LAST_TEST=52。
日志 `/tmp/metal-batch51-52-final.log`；库/app `8dbbe2d60146…`，T51/T52
`e0fc940f6ce0…` / `e0116d679d5f…`。完整 hash/测试范围见 BATCH51-52。

无 GUI/Computer Use、无提交，保留用户 UE 路线和全部旧修改。下一 chat 先读 QA_PENDING/
QA_CONSOLIDATED，**T34–T52 及 T10 marker 共20份**待集中人工；PHASE35–52 与相应 batch
仍开放。下一编号 T53/PHASE53。后续继续选择可终端验证的完整功能切片，不仅去掉标记；
通用 initial contents、其他 wrapper 所有权、views/高级资源等仍有缺口。以下为历史检查点。

## 2026-09-26 最新：BATCH50 自动通过，158 bridge / 93 chunk剩余

T50新增两getBytes捕获元数据/帧资源引用，旧chunk1072/1073；原生CPU内容仍透传，离线
不保存/重放CPU指针和输出payload。synchronizeTexture旧chunk1205真正编码回放，校验
Managed、当前blit encoder和mip/slice；共享纹理同步原生验证层会拒绝，已补负例。
无新chunk，Max1264。synchronizeResource旧空回放尚未处理。

Fixture包含Managed二维/mip/数组、Shared二维及3D两层、仅同步引用纹理；四次CPU读
逐字节检查padding/哨兵，结果写入68-byte Shared参数由第二command buffer绘制。
API核对2D像素、资源保留、参数43/79/113/151/152和回退零值、RGBA43/79/113/255。
3D原生/capture读回已验证，但Replay API/GUI 3D显示未扩展；格式/pitch支持边界见PHASE50。

最终一键`test_metal_capture_batch50_macos.sh`通过：51 API/CLI、840异常、510lifecycle，
growth999,424bytes；16份API验证层。T50新增176异常+2合法metadata变体。日志
`/tmp/metal-batch50-final.log`；仅重放LAST_TEST=50。正式T01–T49未重录。
库/app `a648ce05f576…`，T50 `f56406953e8d…`，GUI executable未改，完整hash见BATCH50。

无GUI/Computer Use、无提交，保留全部原有dirty修改与用户UE路线。下次先读QA_PENDING/
QA_CONSOLIDATED，**T34–T50及T10 marker共18份**待集中人工；PHASE35–50开放。
下一编号T51/PHASE51。普通wrapper所有权、通用InitialContents、views/高级资源仍有缺口；
继续选择能终端形成闭环的切片，不以清除标记代替支持。以下为历史检查点。

## 2026-09-26 最新：BATCH49 自动通过，160 bridge / 96 chunk剩余

新增T49 scheduled/completed handler两入口、旧chunk1049/1054。Native block正常复制/调用，
回传包装command buffer，桥接闭包保留proxy；序列化仅注册身份，离线不执行应用block。
fixture每帧八回调，验证身份/状态/次数/闭包释放，以及两个GPU提交之间的CPU参数依赖。

发现并修复旧CPU快照竞态/回退：快照移到native commit前；shared更新用bytebuf安全读取，
校验range/type/payload/owner；首次加载预扫描CPU更新buffer并恢复非零初值，再按command
buffer缓存更新，部分replay在该提交开始时预先应用，每次非OnlyDraw恢复被CPU更新的
buffer初值，完成前一提交再CPU写。不是通用
initial contents或多队列调度重构。无新chunk，Max1264；详情PHASE49/BATCH49。

最终50 API/CLI、664类异常、500次lifecycle通过，growth3,178,496bytes；15份API Metal验证层。
另三份源码triangle/argument buffer/pipeline variants新录兼容检查通过，不覆盖正式capture。
最终一键`test_metal_capture_batch49_macos.sh`包含全部主回归和源码兼容复验，
统一日志`/tmp/metal-batch49-final.log`。仅重放LAST_TEST=49。
库/app `1cd92c21c774…`，T49 `82771996ea03…`，GUI executable未改，完整hash见BATCH49。

无GUI/Computer Use、无提交。下次先读QA_CONSOLIDATED/QA_PENDING，**T34–T49及T10 marker
共17份**仍待人工，PHASE35–49均开放。下一T50/PHASE50。其他wrapper association生命周期、
全部InitialContents、更多高级资源仍有缺口，不将本批计数当真实引擎已全面支持。
以下为历史检查点，不覆盖本节。

## 2026-09-26 最新：BATCH48 自动通过，162 bridge / 98 chunk剩余

新增T48预编译library四入口file/URL/data/bundle，接通旧chunk1015–1018；默认库1014
修复NSData读取和autorelease误用，旧格式不变。无新chunk，Max1264。失败library/function
返回NULL并保留NSError，不产生空wrapper；replay只使用capture字节，origin不参与加载。

捕获时发现Library/Function提前release堆损坏，改为proxy独立拥有native +1，不再挂
native→proxy retaining association；先销毁ObjC实例，再释放记录/native。T48在首帧前
释放全部库/函数仍保留5库7shader父依赖。其他wrapper旧association策略未重构，后续应独立审计。

正式主batch49 API/CLI、577类异常、490次lifecycle通过，growth1,228,800bytes；14份API
Metal验证层。另源码triangle/argument buffer/pipeline variants新录API+3-loopCLI通过，
不覆盖旧正式capture。主日志`/tmp/metal-batch48-final.log`，补充`/tmp/metal-batch48-source-compat.log`。
两段使用同一库，补充脚本已并入一键`test_metal_capture_batch48_macos.sh`。只重放LAST_TEST=48。
库/app `6df633b629eb…`，T48 `f71eaf76384b…`；完整hash/边界见BATCH48/PHASE48。

未运行GUI/Computer Use，未提交。下一chat读QA_CONSOLIDATED/QA_PENDING：**T34–T48及
T10 marker共16份**仍待人工，阶段35–48开放；下一T49/PHASE49。Binary shader有反射但
无原始MSL展示，非library损坏；不宣称跨GPU/OS兼容、动态库或真实引擎全面支持。
原metallib生成目录已恢复，`.offline`不残留。下方为历史检查点。

## 2026-09-26 最新：BATCH47 自动通过，166 bridge / 102 chunk剩余

新增同步pipeline三个入口：render options/reflection、compute function options/reflection、
compute descriptor，复用旧chunk1022/1024/1025，Max仍1264。T47实际使用六个pipeline，
覆盖native reflection/Params结构体、options3/0、descriptor maxThreads64/mutability/
线程执行宽度整倍数。Replay记录shader父依赖/反射，验证整倍数及dispatchThreads尾组。
native复制descriptor不改调用方对象；高级未支持状态明确拒绝，异步创建仍未接通。

`test_metal_capture_batch47_macos.sh` 最终完整通过：48 API/CLI、513类异常（新增84），
480次lifecycle，resident growth491,520bytes；13份Replay API Metal验证层。
日志`/tmp/metal-batch47-final.log`。库/app `16c921313c2f…`，T47 `a28eef0f401b…`，
完整hash与范围见BATCH47/PHASE47。只重放设LAST_TEST=47。未运行GUI、未提交。

下一chat先读QA_CONSOLIDATED/QA_PENDING，**T34–T47和T10 marker共15份**待集中人工QA。
旧待验项全部保留；PHASE35–47及对应batch未关闭。下一编号T48/PHASE48。
可以继续挑终端可测功能；本批不宣称任意pipeline/真实引擎已兼容。下方均为历史检查点。

## 2026-09-26 最新：BATCH45–46 自动通过，169 bridge / 105 chunk剩余

本轮按用户要求多推进bridge/chunk。Fence创建/包装/生命周期及三类encoder的update/wait，
两种timed present、buffer add/remove marker，共11入口，移除7bridge/接通9旧chunk。
追加Compute1262/1263、Max1264，资源类型尾部追加eResFence。T44为fence链，T45/T46复用
该链并分别覆盖atTime/minimum-duration与marker；原T34–T43不重录、不丢待验状态。

完整batch脚本 `test_metal_capture_batch45_46_macos.sh` 通过，最终47 API/CLI、429类异常、
470次lifecycle（growth1,638,400bytes）；12份Replay API验证层，T00另CLI+40次生命周期。
库/app `33c4399361f7…`，T44/T45/T46 `caa028676f68…` / `9e71f6b21883…` / `7b1fbea45e1f…`。
仅重放设LAST_TEST=46；范围与完整hash见BATCH45–46、PHASE45/46。没有运行GUI或提交。

下一chat先读QA_CONSOLIDATED/QA_PENDING，**T34–T46和T10 marker共14份**待集中人工QA。
T45/T46只查差异，别把fence数值重复三遍；未收到用户反馈不可关闭PHASE35–46及相关批次。
下一编号T47/PHASE47。Fence限当前帧先update后wait，不支持捕获前/外部状态或跨队列保证；
拒绝same/ended encoder无效等待。标记为structured metadata，无范围高亮UI；present不重现
时钟等待。可以继续终端开发高级对象前置或常用资源路径，但不把计数下降当真实引擎兼容。
下方旧“最新”标题均为历史检查点。

## 2026-09-26 最新：BATCH43–44 自动通过，十一份 capture 待集中 GUI QA

用户继续要求快速终端开发，不用Computer Use。新增T42七个compute资源/barrier/marker、
T43五个render声明/barrier入口；修复失败pipeline空包装和vertex RW buffer descriptor/
usage/VS Storage Buffers表入口。剩余176/114，追加chunk1255–1261，旧capture IDs不变。
范围/边界见BATCH43–44、PHASE43/44；支持直接buffer/texture，非通用heap/AS/高级stage。

最终联合 **44份API/CLI、342类畸形、440次lifecycle**通过，resident growth2,899,968bytes；
9份replay Metal验证层通过。native/capture/XML阶段与最终共享回归分段证据见批次文档。
正式库/app副本 `736925e6e195…`，GUI executable `3cc9c3b63507…`，
T42/T43 `25c6e7349f22…` / `7139504e2357…`；全部未提交，未运行GUI。
一键入口 `test_metal_capture_batch43_44_macos.sh`，只重放可用
`RENDERDOC_METAL_LAST_TEST=43 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。

下一chat先读 QA_CONSOLIDATED/QA_PENDING：**T34–T43 和T10 marker**十一份仍待人工。
同一app集中检查，旧未验项全部保留；未收到反馈不得关闭PHASE35–44或相关batch。
下一编号T44/PHASE45。可继续终端开发；texture barrier依赖链/RenderTargets scope尚无
独立fixture，vertex RW texture、高级资源/同步对象依赖需另设范围。不得称真实UE已支持。
下面所有旧“最新”标题均为历史检查点，不覆盖本段。

## 2026-09-26 最新：BATCH41–42 自动通过，九份 capture 待集中 GUI QA

用户继续要求额度重置前快速终端开发，不做Computer Use。本次新增T40三个compute参数/
共享内存入口，以及T41 blit descriptor/四种optimization，修复CPU subresource hint错误
chunk与资源漏引用。八个入口全部有native/capture/replay证据；剩余marker181/119。
新compute chunk IDs1252–1254，旧编号不变。限制与异常边界见BATCH41–42、PHASE41/42。

正式一键脚本 `test_metal_capture_batch41_42_macos.sh` 已跑通；最终42份API/CLI、239类
畸形输入、420次lifecycle。后补display padding修复后，最终联合再次通过，resident growth
2,736,128bytes。库/app副本更新为`b30a0e4cb6ee…`；
T40/T41为`73a3398cec48…` / `35745dc5f7ab…`。未提交，未启动GUI，未改变旧T37capture。
只重放可用 `RENDERDOC_METAL_LAST_TEST=41 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。

接手先读 `QA_CONSOLIDATED.md` / `QA_PENDING.md`：当前 **T34–T41和T10 marker** 共九份
capture待人工；全部旧未验项保留。若用户继续开发，可推进新的终端可测功能；若请求QA，
按总单在同一app中合并检查，不逐批重复。不要自动关闭PHASE35–42或任何未验batch。
下一编号T42/PHASE43。原生验证层确认compute offset接口不能接inline bytes；未实现inline
常量解码UI、threadgroup专用UI、counter sample buffers、通用3D compute/真实UE兼容。

## 2026-09-26 最新：BATCH39–40 自动通过，集中 QA 总单已建立

用户希望额度重置前持续做终端可验证开发，重置后再统一 QA；不使用 Computer Use。
最新完成 T38 六项 sampler LOD（含事件有效 descriptor）和 T39 Private buffer readback，
并打通两个 buffer-output compute 布局。剩余 bridge/chunk 为182/124。新 native/capture/
XML/API/3-loop CLI 通过；最终40 captures、175类异常、400次lifecycle，含反复private
staging读回。测试入口 `test_metal_capture_batch39_40_macos.sh` 和设置 LAST_TEST=39
的共享回归脚本；详情见 BATCH39–40、PHASE39/40、STATUS。

下一 chat 优先读 `QA_CONSOLIDATED.md` 和 `QA_PENDING.md`。当前待验 T34–T39 与
T10 marker，共七份capture，在一个app进程中合并验收；用户未要求前继续开发即可。
不得把前面批次未验项丢掉或自动关闭。版本hash以集中总单为准，旧表保留历史证据。

## 2026-09-26 最新：BATCH38 接通，GUI 全部留待后续 chat

用户当前要求连续快速开发、只做终端可判定验证，不做 Computer Use。已进一步完成
T36 visibility/store/barrier、T37 四种 blit transfer、T10 marker 路由及 D32S8 API
读回/拾取。bridge/chunk 当前 **186/128**；38 份旧/新 capture API/CLI 和 10 轮
lifecycle 通过，71 类异常输入干净拒绝。脚本入口和功能限制见 `BATCH38.md`、
`PHASE38.md`；完整联合重放脚本 `test_metal_replay_batch35_38_macos.sh`。

最终库 `47e0635e8736…`（app 内嵌副本相同），T36/T37/T10_debug 为
`436b3262fef5…` / `f87cbbc8ca00…` / `2d4a8d609bae…`；T34/T35 与旧 T10 不变。
待验 **T34/T35/T36/T37 + T10 marker 增量**，见 `QA_PENDING.md`、`QA_BATCH35-37.md`
和 `QA_BATCH38.md`。用户请求 QA 后再指导同一进程验收；目前任何新批次均不能关闭。
Debug marker 仅结构化 API 事件；D32S8 API 支持不代表 depth Texture Viewer shader 已做。
不把终端跑通推断为真实 UE/Nanite 支持。原有 UE 5.6.1 记录和所有未提交改动保留。

以下为此前历史检查点，其旧版计数、hash 与“当前批”表述不覆盖本节。

## 2026-09-26 BATCH35-37 自动完成，GUI 留待后续 chat

用户明确要求本轮优先连续开发与终端可验证操作，不使用 Computer Use。T34/T35/T36
已完成实现与自动闭环：bridge/chunk 计数 216/165 → 194/143（最后补齐
`waitUntilScheduled` 和五项 command/render debug marker）；两个新增 batch 脚本
覆盖 native、capture/XML、Replay API、3-loop CLI、19 类畸形 capture 拒绝和最终
3 captures × 10 lifecycle（最后一轮 resident growth 606,208 bytes）。最终库
`ba380a0317b6…`，captures 为 `e6ee77deb50a…` / `7fbed8c95df6…` /
`37d6c5cb1306…`。没有启动 qrenderdoc，不得把 T34–T36 写成 GUI 已通过。

接手先读 `BATCH35-37.md`、PHASE35–37、`QA_BATCH35-37.md` 和 `QA_PENDING.md`。
若用户要求继续开发，可继续审计剩余 194/143，优先可用受控 fixture 验证的常用资源/
render 命令簇；高级 heap/event/ray tracing/function-table 需按对象依赖独立规划。若用户
要求 QA，则在同一 qrenderdoc 进程按验收单检查 T34/T35/T36，收到明确反馈后再关闭批次。

共享 render 路径还定向复验了 T01/T02/T04/T07/T17 的 Replay API 与 CLI；T00 加这些
旧场景及 T34/T36 的 8 captures × 10 lifecycle 通过，growth 245,760 bytes。T35 是无 draw
fixture，受 lifecycle harness 只能把首项视为无 draw 的约束，另与 T34/T36 组成三项轮次。

## 2026-09-26 UE 5.6.1 本机目标

用户已有 `/Volumes/CauseUseMac/UE_5.6`。只读检查确认这是官方 UE 5.6.1
promoted build，`Engine/Binaries/Mac/UnrealEditor` 含 arm64。首轮普通帧
试截可使用此安装，不需先编译源码版；当前尚未实际启动或截帧。
该安装的 RenderDoc 插件模块只允许 Win64/Linux，Mac 首轮先走外部注入。
路径与验收层次见 `REAL_WORLD_CAPTURE_ROADMAP.md`。`QA_PENDING.md` 无待验项。

## 2026-09-26 真实应用目标与本机能力评估

用户希望本项目最终能截取 macOS 图形应用/游戏并接入 UE/Unity，重点是 UE
Nanite、光追、mesh shading。当前 T00–T33 是受控场景，不等于真实引擎
兼容。`REAL_WORLD_CAPTURE_ROADMAP.md` 给出缺口、建议从 UE 普通场景
首帧开始的真实应用里程碑、自动化 triage 与 M2 Pro/M4 分工。本轮未实施
代码或新 capture，`QA_PENDING.md` 仍无待验项；全部未提交改动保留。

## 2026-09-26 BATCH33-34 关闭与 action 名称审查

用户确认 T32 EID 12 与 T33 EID 18 新版
`dispatchThreadgroups(indirect, <2, 2, 1>)` 摘要符合预期；此前参数、
CS 间接栏、资源、画面和状态栏已验。T32/T33 GUI L4 通过，PHASE33、
PHASE34、BATCH33-34 已关闭，`QA_PENDING.md` 当前无待验项。

已对照 Vulkan/D3D11/D3D12/GL 审查 Metal custom action names：间接
compute 摘要一致，部分直接 draw/dispatch、单次 indirect draw、ICB、blit
名称存在详细程度或格式差异。后续工作和定向验证见
`ACTION_NAME_ALIGNMENT.md`。此次只改文档，不改 app、代码或 captures，
沿用 `STATUS.md` 上一检查点的最终自动证据；全部未提交改动保留。

## 2026-09-26 T32/T33 新间接摘要待最短 GUI 复验

与 Vulkan/D3D11/GL/D3D12 对齐，Metal compute indirect 的 Event Browser
自定义摘要现显示实际 threadgroup 数量：
`dispatchThreadgroups(indirect, <2, 2, 1>)`。T33 在 command buffer 完成后
解析 GPU writer 的参数，避免误显示零值。用户此前已验 CS 间接参数栏、
资源、画面及状态栏；只需按 `QA_BATCH33-34.md` 顶部复验 T32 EID 12 与
T33 EID 18 的新摘要。最终库 `aa21c9a6983d…`，GUI `f1ebea2ddb86…`；
11 份定向 Replay API/CLI、12×10 lifecycle 与异常拒绝通过。
T32/T33 在 `QA_PENDING.md` 待确认，PHASE33/34 与 BATCH33-34 不关闭。
全部未提交改动保留。

## 2026-09-26 T32/T33 用户部分验收，最短复验待完成

用户确认两份 capture 原验收单中除 IA 栏位以外的参数、资源、画面和状态栏。
compute indirect buffer 已移至 CS 的 `Indirect Dispatch`，图形 draw 的 IA
位置不变；Event Browser 自定义 action 摘要新增 offset 与 threads/group。
最终 GUI `f1ebea2ddb86…`、库 `9c0b65bea9f5…`，T32/T33 capture 不变。
只请用户按 `QA_BATCH33-34.md` 顶部两步，在同一 app 中分别看 EID 12/18；
T32/T33 仍在 `QA_PENDING.md` 待人工确认，两阶段和批次不得关闭。
全部未提交改动保留。

## 2026-09-26 BATCH33-34 自动完成，T32/T33 GUI L4 待用户

无需新对话或 compact。T32 CPU 参数和 T33 GPU 生成参数的 compute 间接
dispatch 已完成 P33.1–P34.4 功能与最终联合自动验证。正式 capture、最终
app 和按 EID 排序的同轮 GUI 验收步骤见 `QA_BATCH33-34.md`；T32/T33
均在 `QA_PENDING.md` 标为待人工 L4，PHASE33、PHASE34 和 BATCH33-34
不得关闭。用户反馈不符时先分析/修复并给最短复验；仅沟通仍无法确认或用户
明确要求时用 Computer Use。可继续后续功能，但每次结果和交接须提醒这两项。

最终库 `8c05d8b03281…`、GUI `5b2bc3aa95cf…`，T32/T33 capture 分别为
`9aa1432c8d9a…`、`4478197f9994…`。16 份定向 Replay API 和逐份 CLI、
17 份 capture × 10 轮 lifecycle、T32/T33 的 5/6 类异常拒绝通过；
自动 DDS 完全相同，raw 参数各 44 字节。L3 条件未触发。
全部未提交改动保留。

## 2026-09-26 BATCH31-32 关闭，下一批 BATCH33-34

用户确认 T30 本次公共事件树改动后的 EID 4/13 顶层与 `$action()` 筛选均符合
预期。T30 原功能与 T31 GUI L4 此前已通过；T31 GUI 导出入口由用户明确
免除本轮复验，自动 DDS/raw 数据已核对，不记作 GUI 实测。最终联合定向的
17 份 Replay API/CLI、18×10 lifecycle 与 L3 决策见 `STATUS.md` 顶部。
PHASE31、PHASE32、BATCH31-32 已关闭，`QA_PENDING.md` 无待验项，
全部未提交改动保留。

下一批 `BATCH33-34.md` 已拆成 `PHASE33.md` T32 CPU 参数 compute indirect
dispatch 与 `PHASE34.md` T33 GPU 生成参数 compute indirect dispatch。第一项
P33.1：建立固定参数/输出的 native fixture，先证明未注入运行正确；之后再接
capture/XML、replay/state 与按编号列明的定向验证。下一批尚未实施。

## 2026-09-26 T31 L4 收口，T30 事件树待复验

用户确认 T31 EID 18 Show Empty Items 中 sampler 0/1/3/4、只读 texture
0/2/4、只读 buffer 0/1/2/3/5/7 为空；`$action()` 隐藏状态调用、清空后恢复，
状态栏为 `No problems detected`。合并此前 EID 7、12–14、18 及后续反馈，
T31 GUI L4 已通过。用户明确免除本轮 GUI 导出入口复验；自动 DDS/raw 已核对，
不要把 GUI 导出写成实测。唯一待验是 T30 本次公共事件树修改后的 EID 4/13
顶层与 `$action()` 最短复验，见 `QA_PENDING.md`。PHASE31、PHASE32 和
BATCH31-32 随批次保持开放，全部未提交改动保留。

## 2026-09-26 T31 部分验收补充

用户确认 T31 EID 7 层级/Texture 零值/Buffer 哨兵，EID 12/13/14 的
`location, length` 参数正确，EID 18 及后续绑定和画面符合预期。
`Copy/Clear Pass #1` 消失符合通用自动分组规则。原验收单把 API Inspector
和 Pipeline State 混写，已改为 EID 18 在 **Window → Pipeline State → CS**
勾选 **Show Empty Items** 检查 texture 2、sampler 3、buffer 5。
T30 公共事件树最短复验、T31 其他未确认 L4 继续见 `QA_PENDING.md`；
PHASE31/PHASE32/BATCH31-32 不关闭。最新代码与 capture 标识不变。

## 2026-09-26 T31 分组修复与 Vulkan pass 标注对齐

用户确认 T30 全部通过；T31 EID 7 Begin Compute 被 `Copy/Clear Pass` 错收。
`AddFakeMarkers()` 已修复，显式 pass 边界保持顶层；T31 EID 7/23 为 compute
Begin/End，EID 7 无 Output attachment。Metal render pass 已参考 Vulkan
标注 load/store 并登记 Clear/Discard/Resolve usage；`$action()` 保留 pass
边界，边界不消耗 action 编号。最终 app/库与 T30/T31 capture SHA、17 份
定向 Replay API+CLI、18×10 lifecycle 见 `STATUS.md` 顶部。
`QA_BATCH31-32.md` 已更新最短复验。T30 原 L4 通过，但本次公共事件树需
复验其 Begin/End 与 `$action()`；T31 其他 UI 项仍待明确确认。
`QA_PENDING.md` 是当前待验清单。PHASE32/BATCH31-32 未关闭，工作区改动保留。

## 2026-09-26 BATCH31-32 自动验证完成，L4 待验

T30 compute sampler 直接绑定和 T31 compute texture/sampler/buffer 批量绑定已完成
最终定向自动验证，正式 captures 与准确 EID 见 `QA_BATCH31-32.md`。T01/T03/
T11/T12/T16/T17/T18/T19/T28/T29/T30/T31 的 native、Replay API、逐份 CLI
通过；T30/T31 异常拒绝、13 份 capture × 10 轮 lifecycle 通过。
L3 条件未触发。`QA_PENDING.md` 保留 T30/T31；用户尚未完成 L4，
PHASE31/PHASE32/BATCH31-32 不关闭。全部未提交改动保留。
下一安全操作是收集用户同轮 qrenderdoc 反馈；若不符先分析修复并给最短复验。

## 2026-09-24 BATCH29-30 关闭交接

用户确认 T28/T29 右侧缩略图和 T29 `$action()` 筛选正常；此前其余 L4
及 T28 DDS 已确认，T22/T25 事件树改动的最短复验也已完成。T28/T29
自动与用户 L4 均通过，BATCH29-30 已关闭；`QA_PENDING.md` 当前无待验项。
本次提交是向 `crossous/renderdoc-metal` 备份，并非发布，用户明确要求
不运行 T00–T29 L3。最终定向证据与 app/capture 标识见 `STATUS.md` 顶部。
下一批为 `BATCH31-32.md`：从 `PHASE31.md` P31.1 开始 T30 compute
sampler，再推进 `PHASE32.md` T31 批量绑定。T30/T31 尚未实现。

## 2026-09-24 历史交接补充

用户最新确认 T25 的 indexed instanced 多实例文字、ICB 子 draw 收在 execute 下，
点 execute 到最后子 draw；并确认 `setBuffer` 等状态 API 有 EID，T27/T28 可见结果。
因此 T25 新行为复验已通过。余项缩为 T28 10×7 UI DDS、T29 `$action()`
筛选、T22 指定父行画面。`BATCH31-32.md`、`PHASE31.md`、`PHASE32.md` 已写
下一批 T30/T31 计划与定向验证清单，尚未开始代码；当前 BATCH29-30 仍未关闭。

本批 T28/T29 自动通过，用户确认 T29 Buffer 20 变化与 API Inspector 绑定可见。
最新事件树修复使非 action 调用有 EID，ICB execute 可展开并在父行回放整个范围；
范围显示 location/length，indexed instanced 子 draw 显示实例数。正式 T28/T29
captures 已重生成。当前 GUI 和 EID 见 `STATUS.md` 顶部及 `QA_BATCH29-30.md`
顶部。待人工 QA 共四项：T28 的 10×7 Texture 20 DDS、T29 的 EID 10/11 与
`$action()`、T22 ICB 父子/父行输出、T25 ICB 父子/父行输出/实例数。
旧已验 T22/T25 仅新显示行为待复验；T28/T29 与 BATCH29-30 未关闭。
全部未提交改动保留。L3 条件未触发。

## 默认工作周期

一个 agent 默认连续负责一个已定边界的相邻阶段批次；最近完成的批次为
`BATCH33-34.md`。下一批须先写明边界、PHASE 与验证清单；批次以 2–3 个相关 T 为宜，改变边界前先更新
批次计划与验证清单，不因单条切片的自动验证通过就停下。只有以下情况可以在批次关闭前停下：

1. 需要用户选择会显著改变范围、兼容策略或用户可见行为。
2. 外部条件不可用，且安全的替代检查已穷尽。
3. 上下文已明显膨胀，需要先写恢复检查点再 compact。
4. 最终联合自动验证已完成，按 [QA_GUIDE.md](QA_GUIDE.md) 交付一次性 GUI 验收单，
   正等待用户完成 L4 并反馈。此时批次保持未关闭，待验功能持续登记在
   [QA_PENDING.md](QA_PENDING.md)；可继续后续功能开发，但不能把未验批次写成关闭。

每条切片按以下顺序推进：

1. **Fixture/native**：建立最小、确定性输入，先证明未注入运行正确。
2. **Capture/data**：补齐 capture/chunk/initial contents，并用 XML 或结构化检查锁定参数。
3. **Replay/state**：完成 GPU replay、event seek、readback 与 pipeline/descriptor 状态。
4. **自动验证**：立即跑当前 fixture 与实际受影响旧路径的必要 L0/L1/L2，证明数据与功能闭环；
   将 UI 操作记为批末待验，不把未验 UI 标为通过。标准 Viewer、跳转和 export 仍在实现范围内。
5. **批次收口**：最终构建上执行去重后的联合自动清单与 CLI/lifecycle，准备正式
   captures 和 [QA_GUIDE.md](QA_GUIDE.md) 规定的一次性 GUI 验收单。用户在同一轮
   qrenderdoc 中依次验收批内 captures；收到结果后完成文档同步。全量回归按下述触发条件执行。

## 新 agent 的最短接手路径

1. 阅读 `README.md` 入口、`STATUS.md` 顶部当前批次与最近恢复检查点、当前 BATCH 文档
   （当前为 `BATCH39-40.md`，前批为 `BATCH35-37.md` / `BATCH38.md`）、当前PHASE及集中QA总单，以及本文件、`QA_GUIDE.md` 和
   `QA_PENDING.md` 的接手/验证规则与待验项；`PLAN.md` 和历史阶段文档按需追查。
2. 执行 `git status --short --branch`，把工作区视为可能包含前任未提交的有效修改，不得清理、覆盖或
   回退未知改动。
3. 读取 `STATUS.md` 的“当前任务”“下一步”和“恢复检查点”；从第一项未完成工作继续，不重做已有
   明确验证证据的步骤。
4. 用增量构建或当前 fixture 的最短定向测试确认环境可用；新 agent 接手时默认**不**先跑全部历史
   capture 的完整回归。
5. 连续完成当前批次。开始时只需在 `STATUS.md` 留一条简短状态；切片转接时记录自动验证结果、
   UI 待验与下一项。每个微小修复不要求逐次改文档，架构决策、影响范围和中断检查点除外。

## 分层验证与额度控制

验证按风险从低到高执行，避免每个修改都重复读取长日志：

- L0：增量编译受影响 target、`git diff --check`、必要的静态检查。
- L1：每条切片立即运行 native/capture/XML/replay/readback/state 的必要断言；批末在最终代码上
  核对两条 T 的 L1、CLI replay 与本场景 lifecycle。已通过且未受后续修改影响的结果可复用。
- L2：每条切片按清单运行实际受影响旧 fixture；批末取各清单的并集，相同 T 只跑一次。
- L3：只在较大里程碑、发布/合并前，或有具体跨场景风险且 L1/L2 无法充分覆盖时，运行一次
  当前全部 T00-Txx 的一键回归、lifecycle 和 CLI replay。当前两阶段批次本身不构成 L3 触发条件。
- L4：批末由用户用最新成功构建，在同一轮 qrenderdoc 中依次验收批内 T 的 Event、
  Viewer、Pipeline、资源跳转/保存/export 和 `No problems detected`。agent 先完成命令行
  可判定项并交付准确步骤与预期，按 [QA_GUIDE.md](QA_GUIDE.md) 收集反馈；L3 不是前置条件。
  反馈不符时先分析、修复并给最短复验步骤；仅在沟通仍无法确认或用户明确要求时，
  agent 才使用 Computer Use 定向诊断。
  未收到用户针对相应 T/步骤的明确通过反馈时，L4 持续为待验证；部分反馈只关闭
  已确认的项。每轮结果和交接都列出 `QA_PENDING.md` 中仍待人工 QA 的功能。

关闭当前批次并编写下一份 `PHASEx.md` 时，必须在新阶段文档中列出验证清单：L1 的当前 Txx、
L2 必跑的旧 T 编号、仅在特定代码路径变动时才跑的旧 T 编号及触发条件、L4 的 UI 检查项，
以及 L3 是“计划执行”还是“默认不执行”。若计划执行 L3，写明覆盖的完整 T 编号范围、
CLI/lifecycle 范围和原因；若默认不执行，写明升级触发条件及触发后的完整范围。
批次文档还须列出去重后的 L1/L2/L4 联合清单。开发时发现新的受影响路径，先在相关 PHASE
及批次文档补上对应 T 编号和原因，再运行测试；批次收口在 `STATUS.md` 只记录实际执行的清单
和结果。不要把“受影响旧路径”留作无编号的验收要求。

优先用命令行、脚本和 Replay API 完成可直接判定的截帧、载入、事件切换、资源/像素
断言、导出内容和日志检查。需要 qrenderdoc 可见交互的部分预先合并成一次用户 L4；
验收单必须列明步骤、预期和简短反馈格式，不能仅给宽泛的检查项目。用户的观察
不能由自动测试冒充；“验收单已交付”也不算 L4 通过。具体分工见 [QA_GUIDE.md](QA_GUIDE.md)。

命令输出应优先重定向到日志文件；成功时只读取摘要，失败时只读取相关错误和末尾日志。修复后只重跑
受影响的 L1/L2 和必要的 L4。后一个 T 修改了前一个 T 的公共路径时，重跑受影响的前一个 T
自动断言。修改公共 replay、序列化、资源所有权、事件语义或通用 UI 路径时，
先列出可能受影响的旧 fixture 并做定向验证；只有具体风险跨越这些 fixture、无法合理界定范围时
才升级到 L3。不要仅凭“改了公共文件”就运行全量回归。批内仍须实现和测试必要功能，不能
为省 QA 删掉功能或测试；确实不必要的功能要说明依据并更新范围。UI 批末实机验收不可省略，
默认由用户按验收单完成。

完整回归命令（仅在上述 L3 条件成立时运行）：

```sh
./util/buildscripts/scripts/test_metal_capture_macos.sh
```

该脚本构建 qrenderdoc/renderdoccmd 与 Metal demos，并自动运行当前全部 T00-Txx 的 native、
capture/XML、replay/output、CLI replay 和 lifecycle。产物位于 `captures/metal-smoke/`；历史阶段
证据按需查看对应 PHASE 文档，不在本文件重复列出逐场景命令。

以下为 BATCH29-30 进行时的历史交接记录，当前状态以本文件顶部为准。
当时最新关闭的是 T00-T27。BATCH29-30 的 T28/T29 功能与最终联合自动均通过，
`/tmp/batch29-30-final.log` 记录 11×10 lifecycle、定向场景与 CLI；L3 未触发。两份正式
capture 已准备，`QA_BATCH29-30.md` 是合并用户 L4 验收单；`QA_PENDING.md` 中 T28/T29
均部分待复验。用户确认 T28 其余 GUI 项和 EID 1 Texture 20 通过，首次 ⌘O
重复弹窗也已解决；T28 当前 EID 7 重新保存的 10×7 DDS 已与自动参考核对通过。T29 的 Buffer Viewer
回退/前进、画面与状态栏已通过，HTML/CSV/Save Bytes raw 已生成，agent 核对 raw
与自动参考完全一致。T29 API Inspector 原未显示无 EID 状态调用，已补列该 action
前的 structured chunks；最新 GUI SHA-256 `c1a773a05d56…`，需用户 ⌘Q 重开后在
EID 5 确认两条 setBuffer 可见，并在完整 Buffer 20 Viewer 确认 EID 2 全 `a5`。
用户还确认 T22 父行 EID 5 可展开为 EID 6/7，绘制正常。此后 Texture Viewer
右侧缩略图空白已定位为 Metal 缺 Headless output；修复后 T01/T03/T09/T11/
T12/T16/T17/T22/T25/T28/T29 的 Replay API 和逐份 CLI replay、12×10 lifecycle
通过，库 SHA-256 `81738a1bcbde…`。当前待用户最短 GUI 复验：T28/T29 右侧
Input/Output 小图，以及 T29 `$action()` 筛选。未收到余项通过反馈前两阶段和
批次不关闭；可继续开发后续功能，但每次结果和交接保留两项待验提醒。
不清理工作区；最近完整 L3 仍为 T00-T18 的 `/tmp/t18-final-regression.log`。

## Compact、继续与新任务边界

### 继续当前任务

只要当前批次尚未完成、上下文仍清晰且没有用户决策阻塞，agent 应自行继续下一项 Pxx.y；T28
自动验证通过后直接进入 T29，不等待用户再次发送“继续”。一次失败或修复循环不是切换任务的理由。

### 建议 compact

出现自动上下文压缩提示、关键输出反复截断、已经历多轮大范围排查，或 agent 难以可靠保留早期实现
细节时，应在安全检查点建议 compact。建议前必须先在 `STATUS.md` 的“恢复检查点”写明：

- 当前批次、各 T 的自动/UI 状态和第一项未完成任务；
- 已修改文件及不可回退的已有改动；
- 最后成功命令与结果；
- 当前失败命令、最短关键日志和已排除原因；
- 下一条安全操作；
- L1/L2/L4 哪些已完成或待批末验收；L3 是否有触发条件、若有是否已执行。

compact 只是压缩当前任务的聊天历史，不改变批次目标。compact 后先读上述文档和 `git diff`，直接从
检查点继续；不重新做未受后续修改影响的测试，也不假定 dirty worktree 可以清理。

### 建议新建任务

完整批次关闭后先判断当前对话能否可靠承接下一批：上下文清楚、工具可用且下一批边界明确时
继续当前对话；上下文已膨胀或需要独立的新目标时建议新建对话，并交付可直接复制的提示词。
当前批内两条切片的自动验证、受影响旧路径联合清单、两份 capture 的用户 L4、文档同步和下一批拆分全部
完成，才可说“批次完成”；若触发 L3，也须完成 L3。等待用户 L4 时，在 `STATUS.md`
及 `QA_PENDING.md` 记录最终构建、captures、自动结果、验收单和第一项待办，
回复中交付准确步骤并列出全部跨批次待验 T；未回复或漏看结果时状态原样保留。
不把自动通过写成阶段关闭。若中途因其他原因切换任务，也留下同等完整的恢复检查点。
agent 的最终回复说明批次是否关闭、下一项是什么，以及继续当前对话还是建议新对话；如建议
新对话，附可直接复制的提示词。上下文已明显变长而批次未关闭时，先写 STATUS 检查点再建议
compact，而非提前声称完成。

下一任务通用提示模板：

```text
继续 RenderDoc Metal replay 当前批次。保留全部未提交改动。接手只读 README 入口、
STATUS 当前批次/最近恢复检查点、QA_PENDING 全部待验项、当前 BATCH 与其中 PHASE 文档、
HANDOFF/QA_GUIDE 验证规则；
PLAN 与历史阶段按需查阅。从 STATUS 第一项未完成工作连续推进到批次关闭。每个 T 完成
必要的 native/capture/replay 自动验证后记录“批末 UI 待验”，继续下一 T；最终代码上按
批次清单去重运行定向验证、CLI/lifecycle。命令行可判定项由你完成；对 GUI 剩余项
按 QA_GUIDE 准备合并两份 capture 的一次性用户验收单，写明步骤、预期与反馈格式，
等待用户 L4 结果。未收到明确反馈或只有部分反馈时，未验功能持续登记在 QA_PENDING，
每次结果都提示全部待人工 QA 的 T；可以继续后续开发，不关闭未验批次。若用户反馈
不符，先修复并给最短复验；仅在沟通仍无法确认或用户明确要求时使用 Computer Use。
收到对应 T 全部步骤通过反馈后再关闭该批次。
影响范围变化时先更新 PHASE 与 BATCH 清单和原因。全量 L3 仅在列明条件触发时执行。
同步阶段与索引文档；完成后判断能否在当前对话继续，若建议新对话则给可复制提示词。
若上下文先变长，先写 STATUS 检查点再建议 compact。
```

T19 已关闭后的下一批提示：

```text
继续 RenderDoc Metal replay 的 BATCH21-22：PHASE21/T20 单命令 ICB 和 PHASE22/T21 indexed
indirect，从 P21.1 连续推进到 P22.4。保留全部未提交改动。接手只读 README、STATUS 当前批次
与最新检查点、BATCH21-22、PHASE21、PHASE22 及 HANDOFF 验证规则；历史按需追查。每个 T
立即完成必要的 native/capture/replay 自动断言，T20 记“批末 UI 待验”后继续 T21。最终构建
按批次联合清单一次性去重执行定向、CLI/lifecycle，并用最新 qrenderdoc 同一轮验收两份 capture。
影响范围变化先更新清单和原因；L3 仅在清单条件触发时覆盖 T00–T21。全部通过再关闭两阶段、
同步文档，并判断是否需要新对话；若需要，给下一批可复制提示词。
```

T21 已关闭后的下一批提示：

```text
继续 RenderDoc Metal replay 的 BATCH23-24：PHASE23/T22 多命令 ICB 与非零 execute range，
PHASE24/T23 indexed ICB，从 P23.1 连续推进到 P24.4。保留全部未提交改动。接手只读
README、STATUS 当前批次与最新检查点、BATCH23-24、PHASE23、PHASE24 和 HANDOFF 验证规则；
历史按需查阅。每个 T 立即完成必要 native/capture/XML/replay/readback/state 自动断言，
T22 自动通过后标记“批末 UI 待验”，继续 T23。最终代码按批次联合清单去重执行旧场景、
CLI replay 与 lifecycle，并用最新 qrenderdoc 同一轮验收两份 capture。影响范围变化先更新
PHASE 与 BATCH 清单和原因；L3 默认不跑，仅在清单条件触发时覆盖 T00–T23。全部通过后
同步阶段和索引文档，再判断是否需要新对话；若需要，留下 STATUS 检查点。
```

T23 已关闭后的下一批提示：

```text
继续 RenderDoc Metal replay 的 BATCH25-26：PHASE25/T24 ICB reset 后重编码，
PHASE26/T25 同一 ICB 中混合非索引与 indexed command，从 P25.1 连续推进到 P26.4。
保留全部未提交改动。接手只读 README、STATUS 当前批次与最新检查点、
BATCH25-26、PHASE25、PHASE26 和 HANDOFF 验证规则；历史按需查阅。每个 T 立即完成
必要 native/capture/XML/replay/readback/state 自动断言；T24 自动通过后标记“批末 UI 待验”，
继续 T25。最终代码按批次联合清单去重执行旧场景、逐份 CLI replay 与 lifecycle，
并用最新 qrenderdoc 同一轮验收两份 capture。影响范围变化先更新 PHASE 与 BATCH
清单和原因；L3 默认不跑，仅在清单条件触发时覆盖 T00–T25。全部通过后同步阶段和
索引文档，再判断是否需要新对话；若需要，留下 STATUS 检查点。
```

T25 已关闭后的下一批提示：

```text
继续 RenderDoc Metal replay 的 BATCH27-28：PHASE27/T26 ICB `inheritPipelineState`，
PHASE28/T27 ICB `inheritBuffers`，从 P27.1 连续推进到 P28.4。保留全部未提交改动。
接手只读 README、STATUS 当前批次与最新检查点、BATCH27-28、PHASE27、PHASE28 和
HANDOFF 验证规则；历史按需查阅。每个 T 立即完成必要 native/capture/XML/replay/readback/state
自动断言；T26 自动通过后标记“批末 UI 待验”，继续 T27。最终代码按批次联合清单去重执行
旧场景、逐份 CLI replay 与 lifecycle，并用最新 qrenderdoc 同一轮验收两份 capture。
影响范围变化先更新 PHASE 与 BATCH 清单和原因；L3 默认不跑，仅在清单条件触发时覆盖
T00–T27。全部通过后同步阶段和索引文档，再判断是否需要新对话。
```

T27 已关闭后的下一批提示：

```text
继续 RenderDoc Metal replay 的 BATCH29-30：PHASE29/T28 compute dispatchThreads，
PHASE30/T29 compute buffer binding，从 P29.1 连续推进到 P30.4。保留全部未提交改动。
接手只读 README、STATUS 当前批次与最新检查点、BATCH29-30、PHASE29、PHASE30 和
HANDOFF/QA_GUIDE/QA_PENDING 验证规则及待验清单；历史按需查阅。每个 T 立即完成必要 native/capture/XML/replay/readback/state
自动断言；T28 自动通过后标记“批末 UI 待验”，继续 T29。最终代码按批次联合清单去重执行
旧场景、逐份 CLI replay 与 lifecycle。终端可判定的 QA 均由你完成；根据最终正式
captures 按 QA_GUIDE 输出一次性 T28/T29 GUI 验收单，列明打开路径、按 EID 合并的
步骤、预期和反馈格式，由用户在同一轮 qrenderdoc 完成 L4。若反馈不符，先指导
最短复验或修复；仅在沟通仍无法确认或用户明确要求时用 Computer Use。等待反馈时
标为“自动验证通过，等待用户 L4”，登记 QA_PENDING，每次结果列出未验 T；
用户漏掉结果或仅部分反馈时持续待验，可以继续后续开发，但不要关闭未验批次。
影响范围变化先更新 PHASE 与 BATCH
清单和原因；L3 默认不跑，仅在清单条件触发时覆盖 T00–T29。L4 通过后同步文档并关闭。
```

历史详细验收记录已移至 [HANDOFF_HISTORY.md](HANDOFF_HISTORY.md)，仅按需追查。

## 每次工作必须更新的内容

- `STATUS.md`：当前批次、各 T 自动/UI 状态、实际验证命令/结果、阻塞项、下一步。
- `QA_PENDING.md`：每个 T 的人工 L4 状态、验收单、用户反馈及跨批次未验项。
- `TEST_MATRIX.md`：只要 API 覆盖或样例状态变化，就同步修改对应行。
- `DECISIONS.md`：出现影响架构、capture 格式、兼容范围或用户可见行为的选择时新增记录。
- `PLAN.md`：阶段范围发生变化时更新；禁止只在聊天中改变计划。
- 当前 `BATCH` 与 `PHASEx.md`：影响范围变化时更新清单；批次关闭时写明下一批及每个 T 的
  编号级验证清单、L3 决策和触发条件。

更新节奏默认是“批次开始一次、切片转接/架构决策/中断时一次、批次收口一次”。不要仅为了记录
每个小修复而反复重写长文档；测试覆盖或用户可见能力实际变化时，仍必须在收口前完整同步。

## 批次与阶段关闭规则

批内前一个阶段在功能自动验证通过后只能标为“批末 UI 待验”。批次中的阶段只有在以下事项全部
完成后才能一起标为完成：

1. 阶段验收条件全部通过，或未通过项得到用户明确接受并记录。
2. 最终构建的联合 L1/L2/CLI/lifecycle、批末两份 capture 的用户 L4，以及实际触发的 L3
   均已完成；没有回复或仅完成部分步骤时，相应 T 在 `QA_PENDING.md` 持续待验证；
   验证命令、产物路径和结果已写入 `STATUS.md`。
3. 新增/变更能力已反映到 `TEST_MATRIX.md`。
4. 已知限制和遗留问题有明确任务 ID。
5. 下一批已拆成文件级或接口级任务，并在 `STATUS.md` 中指定第一项。

## 修改与验证约定

- 实现优先放在 RenderDoc 原有架构位置；不要另建绕开 replay API 的独立查看器。
- 构建产物统一放在 `build-*` 目录，不提交二进制和本机绝对配置。
- 第三方测试样例固定 commit 和许可证；未经核验不复制源码。
- 每个 Metal chunk 的支持应包含 capture/序列化、replay、状态更新和测试四方面检查。
- 暂不支持的接口应稳定返回错误/unsupported，并写清楚日志，不留下 silent success。
- 遇到新 SDK 兼容补丁时，将纯兼容修改与 replay 功能修改尽量分开。

## 建议的任务记录格式

在 `STATUS.md` 工作日志中追加：

```text
| YYYY-MM-DD HH:mm | Mx.y | owner | 做了什么 | 验证命令与结果 | 下一步/阻塞 |
```

若任务中断，必须留下：已修改文件、最后成功命令、当前失败命令、关键日志摘要和安全的下一操作。


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

## B544 Native query 与 AS/Header 恢复（2026-10-07，候选 25632fab）

仍属于通用资源恢复与重定位能力批次。最终 backend/bundle `25632fabc7d45be667f48871f5ca14c4572ce45d728470d096bdeb706916b4c8`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。生产 supportsRaytracing 和 FromRender 保持 false；目标 active。本次确实修改后端与 Native sample，不仅整理清单；没有增加 coverage 等级、按 UE entry/PSO/EID 放行或提高资源预算。

`validateRuntimeDispatch` 增加 AS/Header 专用 typed 来源链：已恢复 kind3 descriptor 槽位产生 Header pointer，公开字段0的 Native AS handle、字段8的贡献缓冲 pointer 均从显式对象+offset恢复。dispatch 编码点保存 Header、验证过的 AS recipe 图和 build owner 快照；消费时验证 live AS/primitive、初态或真正 build、同 command/已提交 producer，并登记实际资源依赖。后来同对象的构建不能补早期缺失 producer。没有照搬旧 `ValidateRayQueryStructure` 的64实例场景限制；使用已验证 typed recipe/冻结输入及既有预算。贡献缓冲仍要有真实恢复的初态/producer，不能把 GPU 计算的 instance id 当 CPU 结果。

`UniformResourceAccess` 区分私有 query handle 和捕获资源。已知 instancing/triangle Native allocate/reset/next/commit/get/deallocate 方法保留私有句柄来源，返回数值 unknown；reset 仍记录并验证真实 AS 来源。getter 的 nocapture/readonly 属性不改变资源语义。store 按目标最外层存储空间分类：device pointer 存入 private slot不是 device buffer write，真正 device 目标/missing AS/missing query handle/未知 Native 外部指针调用仍保守拒绝。独立 CPU tests验证 private store、真实设备指针目标、未知 AS/handle以及 query 返回值不能证明动态写偏移。

Default direct instance构建族不再要求 child 与 parent 分属不同 command；child 的 build kind/owner 必须由真实在前 build 发布，额外验证 Native device一致。`CaptureASInitialBuilds` 顺序访问 candidates，只允许同 native submission的已冻结子 recipe，或之前已完成 submission；未来/缺失子构建不能提供配方。子 recipe被保留并在 TLAS之前恢复。本组 Native sample将不相关的初始化 blit 放在独立提交，AS build仍同一个提交；未取消 mixed-command/Shared-input未知 writer 的保守恢复检查。UserID/copy/refit的旧独立提交限制及 broader mixed-command snapshot机制未全部整改，不能宣称所有AS构建路径支持。

具体仓库参照（实改前读取）：DX12 `WrappedID3D12GraphicsCommandList::Serialise_BuildRaytracingAccelerationStructure`（d3d12_command_list4_wrap.cpp:990）保存完整 build descriptor、目的AS对象+offset，TLAS通过 `PatchAccStructBlasAddress` 重定位，再执行原生 Build；VK `WrappedVulkan::Serialise_vkCmdBuildAccelerationStructuresKHR`（vk_cmd_funcs.cpp:8688）保存 pInfos/rangeInfos、UnwrapStructAndChain实际handle，再原生 CmdBuild与事件/rerecord。Metal复用 typed对象、冻结输入、顺序构建图；必要差异是公开Header同时含不可平移 AS gpuResourceID和buffer VA，需要双字段重定位。DX12 `D3D12RTManager::PatchRayDispatch`（d3d12_manager.cpp:1630）恢复实际 shader-table/root descriptor区域，不CPU执行query；VK `Serialise_vkCmdTraceRaysKHR`（vk_draw_funcs.cpp:5018）恢复实际SBT区域/调度，Metal query无SBT时仍直接执行Native shader。初态参照 `D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp:1914）及 `WrappedVulkan::Apply_InitialState`（vk_initstate.cpp:1835）；Metal现有 `RecordReplayASContents` 保存验证的recipe、先原生恢复primitive再TLAS，本组不将allocation/identity当作已构建。提交参照 `Serialise_ExecuteCommandLists`（d3d12_command_queue_wrap.cpp:440/904）DataUploadSync/跨queue依赖，以及 `Serialise_vkQueueSubmit`（vk_queue_funcs.cpp:1283）原command/waits；Metal使用dispatch点的build owner、同command与committed条件，驻留不等于producer。展示参照两后端 `GetDescriptorAccess` 与 Native Dispatch 分离；Metal partial保留实际typed访问和unknown范围，未模拟交点。

限制分类：已移除 queryReset全拒绝、direct child不同提交要求、指针值内层addrspace造成私有store误报（历史/展示错误）。API/device仍检查真实对象、支持的TLAS/primitive recipe种类、设备及格式/指针类型；恢复限制仍拒绝缺Header/AS/child/贡献初态、未提交producer、opaque Header、未恢复GPU pointer。统一资源、初态、snapshot、Native工作量预算保持。本组新 runtime AS closure不以实例64为门槛；仍有其他coverage族、legacy query64、UserID/copy/refit、standalone frame texture/更广view等旧限制待按协议和真实机制整改。剩余私有pointer spill是地址来源传递缺口，不能永久当展示问题拒绝，也不能直接删unknownBuffer检查。

外盘development实际结果：

| 证据 | 真实范围 |
|---|---|
| `runtime-query-final-native-zero` / `runtime-query-native-variation-fixed` | 自有MIT独立MSL真Native intersection query：命中+distance+instance贡献读取、未命中参与完整输出oracle。1→3 TLAS实例；Header0→64、贡献0→16、输出index0→32，无后端二次修改。fresh Native/capture/API/3-loop CLI、两个完整64uint输出/CBV padding、每份48事件/EID0和20公共访问点（AS+UAV各descriptor/location）通过；各12缺namespace/Header/AS/child/贡献初态等坏组24 API4/CLI1在GPU前拒绝。无显式DeclareRayQuery旁路。 |
| `runtime-query-final-zero` / `runtime-query-final-partial-texel` | 当前库fresh可变输入、threadgroup allocator/local原子及动态GPU descriptor/部分texel初态通过，每份48事件/EID0和typed公共查询；13+26坏组78次GPU前拒绝。 |
| `runtime-query-final-RT` | 当前native源码构建，复用有provenance的converted shader/reflection；fresh真RayQuery4253/4253/0/0、528uint、64几何Private TLAS/帧GPU输入、48事件/EID0及typed公共查询。不是官方新下载或full RT验收。 |
| `runtime-query-final-related` | 29已有capture58 API/CLI（texel/multi-UAV/TraceRay/Compute→Load/linked/depth/deferred/parallel/六八MRT）通过；只计定向旧capture重放。 |
| `runtime-query-final-UE-pre-submit` | 403f90ea实际UEcapture，禁初态上传/frameGPU，45s/3GiB监督。API4/无GPUwait/strict，peak1284096000bytes；3246400/4065变为AS1/unknownAS0/unknownCall0，剩unknownBuffer1。AIR贡献buffer pointer经private alloca存取丢来源；不是UE重放/RT输出PASS。 |

失败证据完整保留：`runtime-query-native-baseline` Native0但capture7，实查TLAS同提交被拒绝；`runtime-query-ordered-build-baseline` 修复后Native/capture0、API3旧AS/calls闭包；首query build因局部变量shadow编译FAIL并保留日志，修名后构建通过；`runtime-query-native-closure-first` 剩五getter属性/deallocate未知calls，补实际Native API签名；`closure-second`/`closure-diagnostics`实际输出无差异，但test以未序列化AS label查身份失败，改用捕获声明真实structure；`runtime-query-native-variation`漏改descriptor source Header偏移而初态拒绝，修fixture后`variation-fixed`全通过。没有把这些失败计通过。CPU首次缺-I导致编译失败不是后端FAIL，随后正确构建和当前gate CPU通过；所有中间产物留外盘。

原始/派生shader捕获地址、只读资源、缺初态、未知producer拒绝保留；动态数值越界/division坏程序不执行、不冒充恢复拒绝。完整官方/IR/RT/frame-family、生命周期/Qt-ARC、原生设备集中门槛与UE整帧RT输出/重放/事件/EID0/绑定仍未完成。历史崩溃/系统重启未证明已修复。新产物外盘、内盘31GiB/外盘1.6TiB；构建/GPU串行共享锁，没有提交/推送/重置或改用户RDHeaderView。

当前证据已核验：`development/runtime-query-evidence.json` 保存7结果manifest及585完成日志SHA，严格诊断0；`checkpoint-runtime-query-25632fab/manifest.json` 保存83源码/执行文档、3产品SHA和不可变快照。最终构建0、diff-check0。Native query两份sample和同库相关回归范围见本节；不把UE诊断driver0当作UE replay PASS。目标active，下一是贡献缓冲pointer经过private alloca的typed来源传递，先独立Native复现，不能删除unknownBuffer检查。
