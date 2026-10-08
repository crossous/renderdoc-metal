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

# Metal Replay 决策记录

## D061：AS 尺寸查询按原生描述符转发，与实际构建存储限制分开

- 2026-10-05，参照 DX12 GetRaytracingAccelerationStructurePrebuildInfo 和 Vulkan vkGetAccelerationStructureBuildSizesKHR。
- B476 的 AS sizes/heap layout 查询解包实例描述符，允许 Managed/Private 输入；不读取 CPU 实例字节，也不序列化查询 chunk。实际分配/构建/初态恢复保留现有校验。
- 官方 sample 与独立 5 组查询测试驱动修复；固定源码/ZIP 哈希，native/capture/replay 分项记录。sample 捕获仍失败和生命周期断言不能计为通过，能力门槛见 RAYTRACING_ENABLEMENT.md。

## D057：描述符生命周期沿用明确shadow元数据，UE私有执行点用隔离模块

- 2026-10-01，先对照D3D12 CopyDescriptors/GetRefIDs与Vulkan DescriptorSetSlot；
  裸内存旧非零值不能证明槽位活跃，记录allocator generation与真正deferred free。
- B332仅重编码producer声明的compute inline VA字段，普通常量保留；小GPU/seek
  与56负例通过。B333 slot记录仅作CPU诊断，缺完整契约时GPU前拒绝，不加coverage。
- 官方局部源码核对后直接隔离编译MetalRHI，保持类布局/98导出，进程级加载副本。
  UBT WriteOutdatedActions会先删除产物，禁止再用于安装Engine只读探测；事故123库
  已按官方manifest恢复复核。持续真实UE目标active。见BATCH332/BATCH333。

## D056：帧内descriptor资源先验证出生时序，再复用原有完成/重建边界

- 2026-10-01，采用v3受限compute契约。CPU preflight只登记已发生创建的候选，
  不创建未来native资源；唯一VA range/ID明确重编码。非法late update也先拒绝。
- 回跳仍等GPU完成，先释放view再parent，保留wrapper identity并按事件重建。
  Shared/Private与buffer-backed view实际GPU/seek、48负例通过。见BATCH331。
- v3不是UE完整coverage：仍缺有效slots/临时slice/inline来源/render与alias，
  不给primary插件添加未经证明的coverage声明。完整UE仍拒绝。

## D055：捕获 thumbnail 持有被选中的 drawable acquisition 至完成后

- 2026-09-30，采用。UE真实thumbnail copy应用崩溃表明completed command buffer
  保留不足以证明drawable/surface存活。recording持有native drawable/texture/proxy；
  selected backbuffer原子发布，结束/丢弃后平衡释放。提交后不额外持有非selected
  drawable acquisition，避免CAMetalLayer pool耗尽。六次present+autorelease测试通过。
- 保留原有GPU完成等待；成功捕获不代表UE replay通过。见BATCH329。

## D054：GPU写入表必须具有显式CPU写入来源

- 2026-09-30，采用v2小用例契约。GPU目的表禁止自动Shared diff；CPU完整entry
  annotation按执行点记录，仅更新该范围，防止捕获GPU结果或抹掉其它GPU写入。
- coverage声明是应用完整schema/来源承诺，UEprimary布局插件不能作此声明。
  GPU copy+CPU entry真实输出41/121/160与seek通过。见BATCH328。

## D053：descriptor relocation 仅针对明确typed字段

- 2026-09-30，采用受限小用例。资源身份/range+offset重编码，普通常量不扫描；
  初始与全部CPU写入先验证，再执行GPU帧。缺项/歧义和未支持执行路径提前拒绝。
- v1/v2实际replay已实现，覆盖小compute帧；UE帧内身份/临时布局/有效槽位与render
  尚未支持，不删D051守卫求打开。见BATCH328–329。

## D052：raw GPU身份查询形成保守live资源capture依赖

- 日期：2026-09-30；状态：捕获遗漏修复采用，replay族仍未完成。
- useHeap只声明驻留，sampler没有heap；编码器显式setter列表不足以表达raw
  descriptor依赖。对照D3D12 RefBuffers，在capture开始保守引用查询过GPU
  身份的live资源；在active capture中缓存getter仍须标记引用，只去重chunk。
- 通过资源管理器锁内live map遍历，不保留所有历史对象，不额外retain native
  资源，不把已销毁对象写进本次capture；可能增加实际未使用live资源的捕获量。
- 原生间接/useHeap小夹具证实旧文件丢掉全部三类资源，而修复后两次capture
  保留。UE自动重截sampler表全部有候选；同ID+相同描述符可记录为等价别名，
  但不得随意选择不一致资源或把未知/stale槽置零求打开。
- 不改变D051的GPU前拒绝。typed schema、临时payload与GPU/CPU更新执行点
  必须真实实现；原生跨进程122证明仅是算法验证。见BATCH327。

## D051：原生GPU身份先捕获诊断，未重定位前拒绝整帧GPU执行

- 日期：2026-09-30；状态：采用诊断边界，完整replay未实现。
- 新UE帧确实保留Shader Converter原进程VA/texture ID，但旧格式缺身份映射。
  对照D3D12地址range tracker和UE三字段descriptor，记录应用首次查询的
  buffer VA、texture/sampler ID及资源身份；追加chunk1397，不改变旧字段。
- metadata保存在资源record，活跃捕获另存frame stream，原子去重并验证
  跨capture持久性。新帧必须先CPU核对mapping覆盖与歧义；不扫描任意64-bit
  内容并盲目替换，也不把getter查询当已实现descriptor解析。
- 在整个stream预扫描发现该族时，于资源分配/initial GPU upload/frame
  commit前拒绝；即使只是无实际解引用的查询也会保守拒绝。CPU structured
  export持续可用。旧帧没有metadata，不能把没有marker当安全证明。
- 后续完整族需要识别表类型、offset/typed views/samplers与帧内GPU更新/
  拷贝的执行点语义。新库重截只获取必要数据；小诊断原生通过不等于UE
  replay或人工UI通过。验证与当前阻塞见BATCH326。

## D001：固定在 RenderDoc v1.46 上开发

- 日期：2026-09-20
- 状态：已采用
- 决策：以官方 `v1.46` 标签、提交 `e4bd23b671d3d5a747ff5221dbe08a63eb6ca200` 为基线，
  在 `metal-replay-v1.46` 分支开发。
- 原因：用户明确要求 1.46；固定提交可以避免上游 Metal 骨架持续变化导致计划和 capture 格式漂移。

## D002：首版只承诺离线 replay 产品能力

- 日期：2026-09-20
- 状态：已采用
- 决策：首版不承诺任意应用注入和完整 capture 支持，但允许补齐测试程序生成 `.rdc` 所需的最小
  capture/序列化路径。
- 原因：没有可靠 Metal capture 输入无法验证 replay；限定输入生成范围可保持目标聚焦。

## D003：优先使用 RenderDoc 通用 UI 框架

- 日期：2026-09-20
- 状态：已采用
- 决策：通过 `IReplayDriver`、`PipeState`、资源描述和现有 qrenderdoc viewer 框架接入 Metal；
  Pipeline State 的后端专用内容页由 D016 进一步细化。
- 原因：用户需要的是 RenderDoc 能力；复用通用 UI 能让 Buffer、Texture、Pipeline、Shader、Mesh
  Viewer 的行为与其他后端一致。

## D004：以小型确定性测试为主、外部样例为辅

- 日期：2026-09-20
- 状态：已采用
- 决策：仓库内建立最小 Metal 用例覆盖单一特性；Apple 和 GitHub 样例用于组合场景回归。
- 原因：大型样例难以定位 replay 差异，而只测自建三角形又不足以证明常见接口覆盖。

## D005：MSL 展示遵循“只展示真实可获得信息”

- 日期：2026-09-20
- 状态：已采用
- 决策：对 source-created library 保存和展示 MSL；对预编译 `.metallib` 展示函数、入口和反射，
  若源码不可恢复则明确标记，不把反编译或 shader debugging 作为首版门槛。
- 原因：Metal binary 不保证包含可恢复的原始 MSL，伪造源码会误导分析。

## D006：SDK 26 新增协议方法通过 Objective-C forwarding 保持兼容

- 日期：2026-09-20
- 状态：已采用
- 决策：对 RenderDoc 尚未包装的新增 Metal 协议方法，保留现有 `forwardInvocation` 行为，并仅在
  `rdoc_metal` target 上关闭协议完整性和相关 property synthesis 的 error 诊断。
- 原因：这些方法本来就应转交真实 Metal 对象；为了通过编译而生成大量无 capture 语义的伪包装
  会造成错误的支持承诺。未来纳入 replay 范围时应逐个实现并移除对应缺口。

## D007：Capture 是 Replay 的前置条件，采用逐 feature 纵向闭环

- 日期：2026-09-20
- 状态：已采用
- 决策：先建立可原生运行的 Metal fixture，再补足其 capture 并生成 `.rdc`，随后立即实现同一
  feature 的 replay/UI 验证；不先批量截取所有高级样例。
- 原因：RenderDoc `.rdc` 依赖 API 专用序列化和资源初始状态，不存在可直接替代的通用 Metal
  trace。只验证文件生成无法发现资源和命令语义缺失，逐 feature 闭环能更早暴露格式设计问题。

## D008：CAMetalLayer 内部始终使用真实 MTLDevice

- 日期：2026-09-20
- 状态：已采用
- 决策：在 hook 的 `nextDrawable()` 调用期间临时把 layer device 切换为真实 device，让系统创建
  drawable/IOSurface/residency 资源；返回后恢复代理 device，并把 drawable 的 `texture` getter 映射
  为对应的 RenderDoc wrapper。
- 原因：macOS 26 的 Metal 驱动会把 drawable texture 加入内部 `MTLResidencySet`。若 layer 持有代理
  device，系统私有路径会混入 wrapper，最终在 `AGXFamilyResidencySet` 中崩溃。系统对象必须看到
  真实 Metal 对象，而应用和序列化层仍需看到 wrapper。

## D009：先注册 structured processor，再实现完整 replay provider

- 日期：2026-09-20
- 状态：已采用
- 决策：阶段 1 增加 Metal structured processor，使 `renderdoccmd convert -c xml` 能解码 capture，但
  不把它当作 replay 完成。
- 原因：它能自动验证 chunk 名称、字段、MSL 和 buffer 数据，且与未来 replay 共用 `ProcessChunk`
  序列化入口；真实 GPU replay 和 UI 仍由阶段 2 的 `IReplayDriver` provider 单独验收。

## D010：首版 frame stream 在加载时执行，显示闭环后再引入 seekable replay

- 日期：2026-09-20
- 状态：已完成并由 D013 取代
- 决策：M2.1-M2.3 先复用 `ReadLogInitialisation()` 顺序执行 capture command stream，以尽快验证
  对象重建、命令执行和最终 render target；M2.4 显示闭环后必须实现按 event 的 full、without-draw、
  only-draw replay。
- 原因：这能尽早暴露 Metal 序列化与真实 API 执行错误，但只执行一次无法满足事件跳转，不能作为
  最终 replay 架构。
- 结果：显示闭环后已按计划引入 frame reader 和 event offset；加载时执行仅用于建立结构化 action，
  交互式查看由 D013 的 seekable replay 负责。

## D011：Texture Viewer 未接通前保留 degraded 能力标记

- 日期：2026-09-20
- 状态：已完成
- 决策：在 output window 和 `RenderTexture()` 仍为占位实现期间，`GetAPIProperties()` 返回
  `degraded=true`；T00/T01 的 Texture Viewer 真实显示通过后再取消。
- 原因：qrenderdoc 的 degraded 弹窗文案偏向硬件回退，但对当前实现来说仍比宣称完整支持更诚实。
  文档和日志必须明确它不是 capture 加载失败。
- 结果：T00/T01 Texture Viewer、resize 和 output readback 验证通过后已设置 `degraded=false`；
  尚未支持的高级纹理类型和 replay 功能继续通过具体接口结果表达，而不再使用全局 degraded 标记。

## D012：output window 使用持久纹理，再复制绘制到每个 drawable

- 日期：2026-09-20
- 状态：已采用
- 决策：每个 output window 保存一张私有 BGRA8 render-target/shader-read texture；viewer 操作先写入
  该纹理，`FlipOutputWindow()` 再用内部 fullscreen pipeline 绘制到当前 drawable。
- 原因：`ReplayOutput` 在非 dirty 帧仍会请求新的 drawable 并调用 flip。直接只绘制当前 drawable 会在
  后续刷新丢失画面；持久纹理同时简化 output readback 和 resize 生命周期。

## D013：以 frame-relative event offset 驱动 Metal 两段式 replay

- 日期：2026-09-20
- 状态：已采用
- 决策：在 `CaptureScope` 后保存独立 `StreamReader`，加载 action 时为每个 event 记录 frame-relative
  file offset。`WithoutDraw` 从帧头执行到目标 offset 前并保留 encoder；`OnlyDraw` 从目标 offset
  执行到下一 event offset；`Full` 从帧头执行到目标 event，并在结束时补齐 encoder/command buffer。
- 原因：这与 `ReplayController::SetFrameEvent()` 的调用顺序一致，既能恢复目标 draw 的绑定状态，又能
  对 clear/pass boundary 等非 draw action 给出确定结果。以 offset 而非硬编码 EID/chunk 名切分，也能
  保持 action ID 调整后的 replay 稳定性。
- 当前边界：只验证了 T00/T01 的单 command buffer、单 render pass。多 command buffer、多 pass 和
  load-action initial contents 必须随对应测试样例扩展；`OnlyDraw` 仍按控制器约定依赖先行的
  `WithoutDraw`。

## D014：首个 capture texture readback 只承诺紧密 RGBA8/BGRA8 2D 数据

- 日期：2026-09-20
- 状态：已采用
- 决策：`GetTextureData()` 首版只支持 sample 0、slice 0 的单采样 2D
  `RGBA8Unorm(_sRGB)`/`BGRA8Unorm(_sRGB)`。使用 Metal blit 复制到 shared buffer，并按设备要求
  对齐 GPU row pitch，返回时逐行去除 padding。`PickPixel()` 复用同一数据路径并做真实通道重排。
- 原因：当前 T00/T01 backbuffer 正好覆盖这条确定性路径；先确保 qrenderdoc 读取、拾取和保存拿到
  真实数据，再按新 fixture 扩展 MSAA、array/cube/3D、depth/stencil、压缩、整数和浮点格式。
  未支持情况必须明确记录错误，不能用空成功或伪造像素掩盖能力缺口。

## D015：source-created library 按 function 复用真实 MSL reflection

- 日期：2026-09-21
- 状态：已采用
- 决策：replay 时按 library resource 保存 `newLibraryWithSource` 的原始 MSL；每个重建的
  `MTLFunction` 根据 `functionType` 映射 RenderDoc shader stage，并生成引用同一真实 source file 的
  `ShaderReflection`。新增 `ShaderEncoding::MSL`，让通用 Shader Viewer 识别并高亮 MSL。
- 原因：Metal 的 source 属于 library，而 RenderDoc Shader Viewer 从 function resource 查询 reflection；
  在两层间显式关联既能保持入口/stage 正确，也不会复制或改写源码。binary/default library 没有原始
  source 时只暴露真实可得信息，继续遵守 D005，不生成伪 MSL。

## D016：Metal 使用独立 pipeline state 类型和现有后端页面模式

- 日期：2026-09-21
- 状态：已采用
- 决策：新增 `MetalPipe::State`，通过 replay controller、proxy serialization 和通用 `PipeState`
  提供跨 API 查询；qrenderdoc 在现有 Pipeline State 容器内增加 Metal 页面。首个 snapshot 只保存
  T01 已有证据支持的 render pipeline、vertex/fragment shader、primitive topology、vertex buffer、
  color/depth target，不借用 D3D/GL/Vulkan state，也不填充尚未 capture 的字段。
- 原因：RenderDoc v1.46 的 Pipeline State 顶层是通用容器，但实际内容页按后端区分。给 Metal 独立
  数据模型和页面符合现有结构，并避免为了复用 UI 而伪装成其他图形 API；后续状态字段可随 fixture
  和验证一起扩展。该决定细化 D003：继续复用通用 viewer 框架和交互，而不是复用错误的后端数据。

## D017：Replay wrapper 显式记录 Metal 对象所有权

- 日期：2026-09-21
- 状态：已采用
- 决策：replay 资源 wrapper 统一持有一个真实 Metal 对象引用，并记录该引用是否由 wrapper 负责
  释放。`new*` API 的 retained 返回值直接转移所有权；`commandBuffer()`、render encoder 等
  autoreleased 返回值先 retain；同一 capture resource 在重复 replay 中替换 live 对象时先持有新值，
  再释放旧值。Objective-C embedded bridge 只用于 capture 跟踪，不参与 replay 析构。
- 原因：无差别释放会让 autorelease pool 二次释放并崩溃，完全不释放又会在线程内循环打开 capture
  时线性泄漏。显式所有权既保留 Cocoa/Metal 返回约定，又让 resource manager 可以确定性 shutdown。
- 验证：T00/T01 各 10 次同进程打开/关闭通过，完整回归 warm-up 后 resident growth 为 491,520 字节。

## D018：Unsupported 能力返回安全空对象或明确错误

- 日期：2026-09-21
- 状态：已采用
- 决策：能力标志继续声明 shader debugging/pixel history 不支持；直接调用时 histogram、pixel
  history、post-VS 返回空数据，custom/target shader build 返回空 ResourceId 与错误文本，四个
  `Debug*()` 返回 stage 正确、`debugger == NULL` 且可释放的空 `ShaderDebugTrace`，不返回空指针。
- 原因：`ReplayController` 会在 debug 调用后访问 trace；返回 `NULL` 会把“未支持”升级成崩溃。
  对 histogram 伪造 256 个零值同样会误导 UI，应使用真正的空结果表达 unsupported。
- 验证：lifecycle smoke 直接覆盖全部上述调用；qrenderdoc 的 History/Debug 控件显示明确的不支持
  提示并保持禁用。

## D019：T02 先固定直接 indexed draw，并显式追踪全部 render-pass attachment

- 日期：2026-09-21
- 状态：已采用
- 决策：首个索引绘制切片只包装直接的 `drawIndexedPrimitives(indexCount:indexType:indexBuffer:
  indexBufferOffset:)`，同时覆盖 UInt16/UInt32；instance/base/indirect 重载继续明确 unsupported，直到
  对应 fixture 到位。render encoder capture 对 color/depth/stencil attachment 及各自 resolve texture
  统一标记帧引用，不能只序列化 ResourceId。
- 原因：直接重载足以闭合 T02 且不会虚假承诺 base/instance 语义。仅把 depth texture ID 写入 render
  pass chunk 而不标帧引用，会使资源创建 chunk 缺失，structured XML 看似正确但 replay 得到空 depth
  attachment；统一 attachment 引用规则消除了这一隐蔽失配。
- 验证：T02 `.rdc` 包含独立 `Depth32Float` texture 创建 chunk；replay pipeline depth target 非空，
  UInt16/UInt32 index 字节、两个 indexed action 和最终双视口图像均由一键回归断言。

## D020：Pipeline snapshot 按 action event 保存 draw-time 状态

- 日期：2026-09-21
- 状态：已采用
- 决策：Metal 在初始加载建立 action 时保存一份对应 event 的 pipeline snapshot；控制器请求状态时
  返回目标 event 或其最近前序 action 的快照，而不是复制 replay 区间结束时的最后动态状态。
- 原因：RenderDoc 的 `OnlyDraw` 区间从目标 draw chunk 延伸到下一 event 前，因此会执行下一 draw 前
  的 viewport/scissor 等状态设置。图像仍只包含目标 draw，但若在区间末尾取状态，EID 2 会错误显示
  EID 3 的右半屏 viewport。draw-time snapshot 让 UI 状态与实际执行该 action 时完全一致。
- 验证：自动回归在 EID 2/3 分别断言左/右 viewport/scissor 与 UInt16/UInt32 index binding，并执行
  clear -> draw1 -> draw2 -> draw1 图像回退；qrenderdoc 实机切换两条 draw 的 Pipeline State 结果一致。

## D021：Metal Pipeline UI 以通用数据和标准交互为收敛边界

- 日期：2026-09-21
- 状态：已采用
- 决策：Metal 继续保留真实的 `MetalPipe::State`，但优先通过通用 `PipeState` 查询和 RenderDoc 既有
  Buffer/Texture/Shader/Mesh Viewer 消费数据；Pipeline State 中的属性、buffer、texture、shader
  激活行为与其他后端保持一致。当前代码生成的 Metal 页面允许作为开发期过渡布局，后续按其他后端
  的 IA/Shader/Raster/Output 分组、可见性过滤、空槽和资源操作逐步重构，不建立 Metal 专用查看器分叉。
- 原因：后端原生状态模型必须保持语义正确，但用户使用路径和信息层级应最终符合 RenderDoc 标准，
  否则每增加一个 Metal 字段都会扩大 UI 差异并重复维护 viewer 逻辑。
- 验证：T02 vertex descriptor 经 `GetVertexInputs()` 直接被标准 Mesh Viewer/自动 buffer formatter
  消费；Pipeline attribute、VB、IB 激活分别进入 Mesh Viewer 和带格式 Buffer Viewer。页面视觉排版
  尚未宣称完成，将在 T03 绑定扩展时继续收敛。

## D022：T03 texture upload 保留调用顺序，sampler 作为一级资源进入通用 descriptor 模型

- 日期：2026-09-21
- 状态：已采用
- 决策：当前支持的 `MTLTexture::replaceRegion` 作为 texture resource record 中的有序初始化 chunk
  保存，在创建 texture 后按原调用参数 replay；不提前伪装成覆盖所有子资源的通用 initial-state
  snapshot。sampler 使用独立 wrapped resource、ResourceId、descriptor 和生命周期，并与 fragment
  texture binding 一起通过通用 `DescriptorAccess`/`Descriptor`/`SamplerDescriptor` 暴露给 UI。
- 原因：T03 的 CPU upload 发生在 frame 前，保留 API 顺序可在最小范围内忠实恢复内容；若直接抽象成
  全量 initial state，会错误暗示 mip/slice/private/压缩路径已经支持。sampler 若只存进 pipeline 快照，
  Resource Inspector、资源身份和后续 argument buffer 扩展都会失去统一基础。
- 首版边界：T03 只覆盖单层、非压缩 RGBA8 2D 的非 slice `replaceRegion`，紧密源数据按真实
  `bytesPerRow * height` 保存。T09 已按 D031 扩展 slice/bytesPerImage 与 mip 链；其他格式和
  storage mode 仍待后续 fixture。
- 验证：T03 XML 保存 64-byte upload 与 sampler 参数；GPU replay 四象限像素、原始 texture data、
  `PickPixel()`、通用 fragment texture/sampler 查询和 qrenderdoc Texture/Resource 跳转全部一致。

## D023：Metal Pipeline 复用标准阶段导航，used 状态必须以静态反射为依据

- 日期：2026-09-22
- 状态：已采用
- 决策：Metal Pipeline 页面复用 qrenderdoc 的 `PipelineFlowChart` 和隐藏 stage tabs，固定映射为
  IA/VS/RS/FS/OM；数据继续来自通用 `PipeState`/`MetalPipe::State`。`Show Empty Items` 只显示状态
  模型能证明存在且未绑定的槽；`Show Unused Items` 在 shader resource binding reflection 可用前保持
  禁用并向用户说明原因。共享 `PipelineFlowChart` 接受焦点，并以 Left/Right/Home/End 切换同一组
  stage，不为 Metal 建立私有导航逻辑。
- 原因：阶段导航、信息层级和操作路径应与 D3D/Vulkan 一致，但“unused”是 shader 静态引用语义，
  不能从当前仅记录实际绑定的 descriptor access 反推。把按钮禁用比将所有绑定误标为 used 更准确。
- 验证：T03 IA/FS/OM 和 T02 IA/RS/OM 在最终 qrenderdoc 中逐页核对，empty index/depth 槽、shader
  直接跳转、texture/sampler/depth/raster 数据均正确；T07 又实机用 Home/End 从 IA 往返 OM，状态栏
  保持 `No problems detected`。

## D024：Metal 资源表复用 RDTree 公共操作，HTML export 按标准阶段输出

- 日期：2026-09-22
- 状态：已采用
- 决策：Metal Pipeline 的状态表统一使用 `RDTreeWidget`/`RDHeaderView`，ResourceId 存入 item tag，
  并由 `PipelineStateViewer` 的 `SetupResourceView()`、thumbnail/preview 分发消费；不复制 Metal 专用
  右键菜单。HTML export 复用公共 writer/table helper，按 IA/VS/RS/FS/OM 输出现有真实状态。
- 原因：资源复制、usage、Resource Inspector、hover preview 与 HTML 样式属于 qrenderdoc 通用交互，
  独立实现会扩大和 D3D/Vulkan 的差异。按当前真实表导出还能避免为追求完整报告而伪造未捕获字段。
- 验证：最终 qrenderdoc 的 T03 FS 表保持标准选择和 Texture Viewer 双击跳转；Export 在 EID 2 写出
  `/tmp/metal-pipeline-t03.html`，核对标题为 Metal Pipeline export，且包含五阶段、Triangle Strip、
  Texture 17 与 Sampler 18。完整 T00-T03 回归通过，resident growth 为 524,288 字节。

## D025：used/unused 以 Metal pipeline argument reflection 为准，物理槽与反射索引分离

- 日期：2026-09-22
- 状态：已采用
- 决策：source-created render pipeline 在 replay 创建时请求 `MTLPipelineOptionArgumentInfo`，直接使用
  Metal 返回的 vertex/fragment argument reflection 建立 shader resource binding；不解析 MSL 源码，
  也不从“当前已绑定资源”反推 shader 声明。`DescriptorAccess.index` 保持通用 API 所需的 shader
  reflection 数组索引，Metal 物理 slot 继续保存在 descriptor store offset 中并由 UI 换算显示。
  已绑定但 shader 未声明的 slot 使用 `NoShaderBinding + staticallyUnused`；没有 argument reflection 的
  pipeline 保持原兼容映射并禁用过滤，不伪造 unused 证据。
- 原因：shader 接口索引和 API 物理 binding slot 并不保证相同；混用二者会让未声明 slot 在 UI 显示为
  `65535`，也会破坏通用 descriptor API。编译器 reflection 是静态使用语义的权威来源，并能为后续
  argument buffer 和更多 binding 类型保留正确模型。
- 验证：T03 的 slot 0 texture/sampler 分别映射到 `colourTexture`/`colourSampler` reflection index 0，
  同一资源额外绑定的 slot 1 被标记为 statically unused；自动回归验证 `onlyUsed=true` 只返回 slot 0。
  最终 qrenderdoc 默认仅显示 slot 0，勾选 `Show Unused Items` 后显示真实 slot 1，状态栏为
  `No problems detected`。完整 T00-T03 回归通过，resident growth 为 540,672 字节。

## D026：直接 Metal buffer argument 映射为 constant block，动态 offset 保留事件语义

- 日期：2026-09-22
- 状态：已采用
- 决策：当前 source-created MSL 中以 `constant T& [[buffer(n)]]` 声明的直接 buffer argument 映射到
  `ShaderReflection::constantBlocks` 和 `DescriptorType::ConstantBuffer`，而不是伪装成 texture-style
  read-only resource。Metal fragment buffer 使用独立 descriptor offset 空间，物理 slot 与 shader
  reflection index 继续分离。`setFragmentBufferOffset` 作为独立 chunk replay，在保留已绑定 ResourceId
  的同时更新当前 offset/available size，并由 draw-time snapshot 固化到各 event。
- 原因：constant block 能被通用 `PipeState::GetConstantBlocks()`、标准 Pipeline UI 和后续常量查看路径
  正确消费；动态 offset 若只修改真实 encoder 而不更新 snapshot，会让 EID 2/3 显示同一个范围。独立
  descriptor 空间避免 buffer slot 0 与 texture/sampler slot 0 冲突。
- 当前边界：只承诺 T04 使用的单个直接 fragment constant buffer 与单-slot offset 更新；storage
  buffer、批量 binding、argument buffer、vertex-stage texture/sampler 和结构成员反射继续留待 fixture。
- 验证：T04 XML 保存 512-byte 初始数据、slot 0、offset 0/256 和两条 draw；自动回归断言 buffer 字节、
  `uniforms`/16-byte reflection、descriptor 和事件图像。最终 qrenderdoc 在 EID 2/3 分别显示
  `Buffer 16 / 0 / 512` 与 `Buffer 16 / 256 / 256`，Buffer Viewer 精确打开第二个范围；完整 T00-T04
  回归通过，五份 capture 各 10 次 resident growth 为 376,832 字节。

## D027：实例化输入继续复用通用 VS Input，raw preview 不伪造 shader 输出

- 日期：2026-09-22
- 状态：已采用
- 决策：Metal vertex descriptor 的 per-instance layout 直接映射到通用
  `VertexInputAttribute::perInstance/instanceRate`，action 的 base instance 由标准 Buffer Viewer 数据窗口
  应用；Pipeline IA 与 Mesh Viewer 不增加 Metal 专用实例解释。VS Input preview 只绘制被选作 position
  的原始 attribute，不尝试猜测其他 instance attribute 如何参与 vertex shader 变换；变换后的实例
  geometry 必须来自未来真实 post-VS 数据。
- 原因：任意 MSL vertex shader 都可能以非平移方式使用 per-instance 数据，UI 无法仅凭 attr 名称或
  buffer layout 推导输出位置。复用标准路径能正确展示实例记录、base instance 和 raw input，同时避免
  把 T05 特定的 offset 语义硬编码进 replay backend。
- 验证：T05 action 保存 `instanceCount=3/baseInstance=1`；自动回归逐字节检查 24/96-byte buffer、
  per-vertex/per-instance 映射、raw mesh preview 与三色 GPU 输出。qrenderdoc Mesh Viewer instance 0/1
  分别显示 record 1/2 的 offset/colour，Pipeline IA 和两个标准 Buffer Viewer 使用相同 ResourceId、
  offset、size 与 stride，状态栏为 `No problems detected`。

## D028：MRT action 与 OM 状态以完整 attachment 数组为准

- 日期：2026-09-22
- 状态：已采用
- 决策：Metal render pass 的 clear/draw/end-pass action 从当前 pipeline snapshot 填充全部 color
  outputs，不再把 slot 0 当作唯一 framebuffer。render pipeline descriptor 中每个有效 color
  attachment 的 blend enable、RGB/alpha factor、operation 和 write mask 转成通用 `ColorBlend` 数组，
  由 `PipeState::GetColorBlends()`、标准 OM RDTree 和 HTML export 共同消费。Texture Viewer 继续使用
  action outputs 自动建立 FB0/FB1 列表，不建立 Metal 专用 MRT 查看器。
- 原因：多附件是 RenderDoc action、Texture Viewer 和 Pipeline State 共用的数据关系；若各 UI 从
  Metal 私有 descriptor 单独推导，会造成事件输出、缩略图与 OM 表不一致。通用 `ColorBlend` 也能让
  字段顺序和命名自然收敛到 GL/Vulkan/D3D 页面。
- 当前边界：只承诺 T06 覆盖的两个单采样 2D RGBA8/BGRA8 attachment、Add operation、
  SourceAlpha/OneMinusSourceAlpha 和 disabled blend/RGB mask；logic op、更多 factor/operation、MSAA、
  memoryless 与 programmable blending 仍需独立 fixture。
- 验证：T06 action slot 0/1、两行 `ColorBlend`、240-byte vertex 数据和两张 attachment 的 clear/draw/
  回退像素均由 smoke 自动断言。七份 capture 各 10 次 lifecycle 与三轮 CLI replay 通过；qrenderdoc
  EID 3 的 Outputs 可切换两张图，OM 表与实际 HTML export 显示相同 blend/write-mask 状态，状态栏为
  `No problems detected`。

## D029：Depth/stencil action、通用状态与标准 OM 必须共享 draw-time snapshot

- 日期：2026-09-22
- 状态：已采用
- 决策：combined depth/stencil attachment 继续使用同一真实 texture ResourceId；render-pass action 的
  `depthOut`、Metal depth-stencil state、通用 `DepthTestState`/`StencilFace` 与 qrenderdoc OM 表都从
  draw-time snapshot 读取。front/back compare、三类 stencil operation、read/write masks 与 dynamic
  reference 保留为独立字段；绑定新的 depth-stencil state 不覆盖 encoder 上已有的 dynamic reference。
  当前不支持的 depth/stencil texel readback 不从 color 结果反推，也不返回伪数据。
- 原因：Metal 把 descriptor state 与 encoder dynamic reference 分开，且 combined attachment 同时承担
  depth/stencil；若在 bind state 时重置 reference，或由 UI 重新解释 descriptor，会让 event seek、GPU
  结果与 Pipeline State 相互矛盾。复用通用 OM 数据结构也能保持与 D3D/Vulkan 的字段顺序和操作路径
  收敛。
- 当前边界：只承诺 T07 的单采样 `Depth32Float_Stencil8`、单 render pass、front/back descriptor、
  single/dual reference 与直接 draw；depth/stencil texture readback、MSAA、depth bounds 和 memoryless
  attachment 留待后续 fixture。
- 验证：T07 structured XML、五个 action/depth output、事件 state 与左绿右蓝 GPU 结果由 smoke 自动
  断言；八份 capture 各 10 次 lifecycle 与三轮 CLI replay 通过。qrenderdoc EID 6 的 OM 表和 UI
  导出的 `t07_pipeline_state_standard.html` 均显示 Texture 20、Less/Write Enabled、Front/Back
  reference 5、`000000FF/00000000` masks、Equal 与 `Inc Sat/Dec Sat`，状态栏无错误。

## D030：MSAA attachment 与 resolve output 必须保持为两个真实资源

- 日期：2026-09-22
- 状态：已采用
- 决策：Metal pipeline snapshot 分别保存 multisample color attachment 与 resolve target，并保留各自
  texture type/sample count。Pipeline OM 使用独立的 Multisample State、Color Targets 和 Resolve
  Targets 标准资源表；clear/draw action 的可显示 output 在存在显式 resolve 时指向单采样 resolve
  resource。Texture Viewer、像素拾取和保存继续读取该真实 resolve texture，不把 multisample attachment
  降格成普通 2D texture，也不声称支持逐 sample readback。
- 原因：Metal 的 render attachment 是实际写入对象，resolve texture 才是 pass store 后可采样/展示的
  结果；若用一个 ResourceId 同时代表两者，会丢失 4x/1x 关系，并让 Pipeline State、action output 与
  Texture Viewer 产生矛盾。分离模型也与 Vulkan/D3D 的 attachment/resolve 语义一致，便于 UI 最终
  收敛到 RenderDoc 标准布局。
- 当前边界：只承诺 T08 的单 render pass、4x BGRA8 2D multisample attachment、单采样 2D resolve、
  `MultisampleResolve` store action 与 alpha-to-coverage。逐 sample 查看、MSAA array、custom sample
  positions、memoryless 和 depth/stencil resolve 留待后续 fixture。
- 验证：T08 structured XML、三条 draw/action output、sample/resolve snapshot 与 clear/draw/rewind 像素
  由 smoke 自动断言；九份 capture 各 10 次 lifecycle resident growth 为 376,832 字节，三轮 CLI
  replay 均通过。qrenderdoc EID 4 显示 Texture 17 为 4x Texture 2D MS、Texture 24 为 1x Texture 2D，
  resolve 行可进入正确红绿蓝图像，HTML export 与页面一致，状态栏无错误。

## D031：T09 子资源索引统一采用 mip 与线性 slice

- 日期：2026-09-23
- 状态：已采用
- 决策：保留真实 Metal texture type 与 `mipmapLevelCount`；RenderDoc 对外的 array size 对
  `Texture2DArray` 等于层数，对 cube 等于六面，对 cube array 等于六倍 cube 数。上传 chunk 明确记录
  mip/slice/bytesPerRow/bytesPerImage；`GetTextureData()`、`PickPixel()`、标准 output renderer 和
  DDS 保存使用相同的线性 slice 语义，越界及当前不支持的组合明确拒绝。
- 原因：只保存 mip 而丢失 slice 会让 array/cube 的不同内容被错误合并；把 cube 误报为单层也会使
  Texture Viewer face 选择、Pipeline 类型和导出结果互相矛盾。
- 当前边界：T09 验证单采样 RGBA8 的 2D mip、2D array 与 cube；cube array descriptor 可枚举，
  但 display/readback 与更多格式、3D、texture view 仍按后续 fixture 扩展。
- 验证：structured XML 覆盖全部 12 个 `replaceRegion`；smoke 检查 12 份原始字节/display、cube
  face pick、越界拒绝与 512-byte DDS；qrenderdoc EID 2 的 FS/Texture Viewer/保存路径一致。

## D032：macOS 26 的 Qt 5 子资源组合框避开 Cocoa popup

- 日期：2026-09-23
- 状态：已采用
- 决策：仅在 macOS 的 Texture Viewer mip 与 slice/face 两个 `QComboBox` 上拦截左键
  press/double-click/release，按当前模型顺序循环选项；原有方向键、Home/End 和通用 index-change
  更新保持不变。其他平台的组合框不变；控件 tooltip 说明点击和键盘用法。
- 原因：本机 Qt 5.15.19/macOS 26.1 的 `QComboBox::showPopup()` 会在 `libqcocoa.dylib` 中访问
  无效地址 0x28 并使 qrenderdoc 崩溃；仅改为非原生 `QListView` 仍会崩溃。问题在 UI popup 路径，
  与 Metal replay 数据无关。限制修复范围比改动全局 Qt style 更稳妥。
- 验证：最新 qrenderdoc 通过连续点击切换 mip0/1/2、array slice0/1/2、cube X+/X-，键盘 End
  选到 Z-；各颜色正确，标准 DDS 保存和 `No problems detected` 均通过。

## D033：T10 blit 逐操作记录 action、usage 与可 seek 资源结果

- 日期：2026-09-23
- 状态：已采用
- 决策：T10 的 blit encoder 创建/end 形成 pass boundary；buffer copy、texture copy、buffer
  fill 与 mip generation 各生成一个 action/event，并分别使用通用 `Copy`、`Clear`、`GenMips`
  flag。copy action 保存真实源/目标 resource ID 与 texture mip/slice；`GetUsage()` 按 event 记录
  `CopySrc`、`CopyDst`、`Clear` 和 `GenMips`，让标准 Resource Inspector/Timeline 使用同一数据。
  replay 在提交 GPU 命令前验证 buffer offset/length、texture mip/slice/origin/size 和 pixel format。
- 原因：只执行 GPU blit 而缺少 action/usage 会让事件切换、资源跳转和读写关系与实际内容不一致；
  复用现有标准 Viewer 能直接观察 blit 前后字节及子资源，不需要新的 Metal 专用查看器。
- 当前边界：只承诺 T10 覆盖的 shared buffer→buffer、单采样 RGBA8 2D texture→texture origin
  重载、buffer fill、2D mipmap generation 和同一 command buffer 内的 blit→render 可见性。
  buffer↔texture、更多重载/格式、跨 command buffer/queue 与 managed-resource 同步待后续 fixture。
- 验证：T10 XML、EID 2→3→2 buffer 数据、Texture 20 四象限、Texture 21 mip3、四条输出
  色带、468-byte DDS、11 份 capture 各 10 次 lifecycle，以及 qrenderdoc 的标准资源跳转通过。

## D034：Metal usage 文案与 mipgen 的通用 pass 分组

- 日期：2026-09-23
- 状态：已采用
- 决策：qrenderdoc 的 `ResourceUsage` 文案将 Metal 纳入现有通用分支，使 copy/fill/mipgen
  显示真实 `Copy - Source/Dest`、`Clear`、`Generate Mips`，不落到 `Unknown`。通用自动 marker
  的 copy/clear 分组把 `GenMips` 视作该组操作，避免 blit 组因没有 color output 被误写成
  `Depth-only Pass`。
- 原因：L4 实机发现自动 smoke 的 usage 语义正确，但 UI 文案和分组标题误导用户；保持 action
  flags 不变，仅修正通用显示与分组规则。
- 验证：T10 定向 output smoke/CLI replay 通过；最新 app 包内 `librenderdoc.dylib` 同步后，
  Event Browser 显示 `Copy/Clear Pass #1` 及 EID 1-6，Resource Inspector 显示 Buffer 18 的
  `Copy - Dest`/`Clear` 和 Texture 21 的 `Generate Mips`，状态栏为 `No problems detected`。

## D035：T11 compute 写入以 frame 内 reset 和标准 descriptor 表达

- 日期：2026-09-23
- 状态：已采用；自动 L3 与最新 qrenderdoc L4 均通过
- 决策：fixture 的 destination `replaceRegion(0)` 放在 `StartFrameCapture` 之后，成为可重放的
  frame chunk；compute encoder 的 begin/dispatch/end 使用标准 action/event，dispatch 标记
  `CS_Resource/CS_RWResource` usage。compute pipeline/shader/texture binding 写入 `MetalPipe::State`，
  由标准只读 `Image` 和读写 `ReadWriteImage` descriptor 暴露，在 Metal Pipeline 的独立 CS 阶段
  展示并可直接打开标准 Texture Viewer；不增加 compute 专用资源查看器。
- 原因：若 reset 在 capture 前，初次 replay 后写纹理会保留最终 texel，seek 回 dispatch 前无法恢复
  清零状态。将 reset 纳入 frame stream 后可复用现有 replay 区间逻辑，且通用 descriptor/usage 让
  UI、Resource Inspector 和自动断言使用同一来源。
- 当前边界：只承诺单 command buffer、直接绑定的同尺寸 2D RGBA8 texture read/write 与
  `dispatchThreadgroups`；不包含 argument buffer、indirect dispatch、跨 queue 同步及更多格式。
- 验证：T11 前/后/回退 readback、全部 256 字节、最终 draw、384-byte DDS、T03/T09/T10 定向、
  T00-T11 全量回归和 12×10 lifecycle 已通过。最新 qrenderdoc 的 Event/CS Pipeline/Texture/
  Resource Inspector、UI DDS/HTML 导出与 `No problems detected` 均通过。

## D036：argument buffer 保存编码语义，不保存不可移植的 GPU 地址字节

- 日期：2026-09-23
- 状态：已采用；自动 L3 与最新 qrenderdoc L4 均通过
- 决策：T12 的 `MTLFunction::newArgumentEncoderWithBufferIndex` 创建真实 wrapper；
  `setArgumentBuffer`、`setTexture`、`setSamplerState` 作为目标 argument buffer resource record 的有序
  chunk 保存，并把 encoder/function、texture、sampler 建成依赖。replay 在真实 device 上重建 encoder
  后重新编码资源句柄，不把 capture 进程中的 argument buffer 原始 GPU 地址字节当作初始内容复制。
  单层成员写入 `MetalPipe::ArgumentBuffer`，同时展开为 shader struct-member reflection 和通用 descriptor；
  `useResource` 保持真实 Metal residency 声明并产生间接 texture frame reference。
- 原因：argument buffer 内的资源表示由 Metal 驱动编码，跨进程复制 raw bytes 不能保证句柄有效；只在
  Metal 专用 UI 保存旁路映射又会让通用 descriptor、资源跳转和自动化看到不同事实来源。以 API 编码
  序列重建并复用通用 descriptor，能同时保持 GPU 正确性和 Viewer 一致性。
- 当前边界：只承诺 fragment buffer slot 0、单层直接 `texture2d<float> id(0)` 和 sampler `id(1)`；
  nested/array argument buffer、buffer member、compute/vertex argument buffer、device descriptor 路径、
  heap/bindless/function table/ICB 均留待独立 fixture。
- 验证：T12 XML、resource ID、`PS_Constants/PS_Resource` usage、reflection/descriptor、64-byte texel、
  clear/draw seek、四象限输出、192-byte DDS、T03/T04/T11 定向以及 T00-T12 全量和 13×10 lifecycle
  已通过；最新 qrenderdoc 的 Event/FS Pipeline、Buffer/Texture/Resource 跳转、UI DDS/HTML export 与
  `No problems detected` 也通过。

## D037：单次 indirect draw 使用真实 buffer 参数与标准状态路径

- 日期：2026-09-23
- 状态：已采用；自动 L3 与最新 qrenderdoc L4 均通过
- 决策：T13 仅接通非索引 `drawPrimitives(primitiveType, indirectBuffer, offset)`。capture 保存
  buffer 资源引用与 offset，并延用 shared buffer 初始内容；replay 从真实 buffer 读取四个
  `uint32` 参数填充 action，再调用真实 Metal indirect overload。用 `Drawcall|Indirect` 与
  `ResourceUsage::Indirect` 表达事件和资源用途，将精确 16-byte 参数区放入 `MetalPipe::State`，
  由 IA Pipeline 打开标准 Buffer Viewer。
- 原因：参数字节、action、GPU draw 和 UI 应当来自同一个 buffer/offset；独立 Metal 专用面板会
  让资源身份和通用导出分叉。replay 对未对齐、越界或不可 CPU 读取的参数显式失败，不把未知参数
  伪装成确定 action。
- 边界：只承诺 CPU 写入的 shared buffer、单次非索引 indirect draw；indexed indirect、GPU 生成
  参数、ICB、heap、多 queue 留待独立 fixture。首版 P1 缺口先处理 T14 indexed instancing/base
  vertex，ICB 保留为 P2。
- 验证：正常 offset 16 的四字段 `3/2/1/1`、左右实例图像、16-byte UI/自动 raw export、
  Pipeline HTML、14×10 lifecycle 与 T00-T13 全量通过；派生 offset 17/36 capture 在 indirect
  chunk 返回明确 replay 失败，最新 qrenderdoc 状态栏为 `No problems detected`。

## D038：indexed instancing 的 index binding 使用 draw-time 精确子范围

- 日期：2026-09-23
- 状态：已采用；T14 L3/L4 均通过
- 决策：只接通 `drawIndexedPrimitives(... instanceCount, baseVertex, baseInstance)` 直接重载，
  保留 T02 基础 indexed draw 与 T13 非索引 indirect 路径。replay 将 Metal 的
  `indexBufferOffset` 放入 `MetalPipe::indexBuffer.byteOffset`，把 `byteSize` 限为
  `indexCount × indexStride`；`ActionDescription::indexOffset` 相对于该绑定为 0。
  `baseVertex`、`instanceOffset` 和 `Indexed|Instanced` 标记来自同一 chunk。draw action 为绑定的
  vertex/instance/index buffer 写入通用 resource usage。
- 原因：标准 Mesh Viewer/Buffer Viewer 会将 index binding offset 与 action indexOffset 相加；
  若两处重复保存绝对 offset，会读取错误的 index。精确子范围还让 IA、Buffer Viewer 和导出
  与实际 GPU draw 对齐，不把 buffer 尾部哨兵误当作当前 draw 数据。
- 边界：当前只覆盖直接 UInt16/UInt32 indexed instancing/base vertex overload；indirect indexed、
  ICB、GPU 生成参数与任意应用 capture 仍需独立 fixture。replay 对无效 index 类型、未对齐或越界
  index 范围、超出 action 字段宽度的参数显式失败。
- 验证：T14 原生/重放左红右蓝、XML 参数、`0/1/2` index 数据、4/6 Buffer Viewer、两实例
  VS Input、usage/seek/raw 保存、offset 3/10 异常拒绝、T02/T05/T13 定向、T00-T14 全量及
  15×10 lifecycle 均通过；最新 qrenderdoc 的 Event/API/IA/Resource/Mesh/Buffer/HTML/CSV 和状态栏
  均已核对。

## D039：直接 point/line draw 延用标准 action 与 VS Input 路径

- 日期：2026-09-23
- 状态：已采用；T15 L3/L4 均通过
- 决策：Point、Line、Line Strip 复用已支持的直接 `drawPrimitives` chunk 和通用
  `ActionDescription::vertexOffset`，按 primitiveType 设置事件级拓扑。Event 名称显示明确的 primitive
  类型，`PipeState`、Mesh preview 和标准 Buffer Viewer 从同一 vertex buffer/descriptor 读取数据。
  replay 对未知 primitive 或零顶点/实例数明确失败。仅 Mesh preview 的 vertex shader 输出
  固定 9px point size，以使 point 可见；不会修改被捕获 draw 的大小。
- 原因：非零 vertexStart 应只写入 action，标准 Mesh Viewer 在读取顶点时加上这个偏移；把它再次
  写到 buffer binding 会导致双重偏移。细线的精确覆盖受光栅化边界影响，像素验证检查端点附近小范围。
- 边界：仅覆盖直接 point/line draw；indexed/indirect line、可变 point size、line width 扩展、
  几何 shader 和 post-VS 留待独立 fixture。
- 验证：T15 native/capture/XML/action/seek/VS Input/Buffer/Resource/Mesh/DDS、非法参数拒绝、
  T01/T02/T05/T14 定向以及 T00-T15 L3（16×10 lifecycle，resident growth 737280 bytes）通过；
  最新 qrenderdoc 的 Point/Line/Line Strip Event/API/Pipeline、Mesh/Buffer/Resource、UI DDS/HTML
  与状态栏均已核对。

## D040：vertex texture/sampler 使用独立 stage snapshot 和 descriptor 地址

- 日期：2026-09-23
- 状态：已采用；T16 L3/L4 均通过
- 决策：直接 `setVertexTexture`/`setVertexSamplerState` 与 fragment 对应接口使用各自的
  `MetalPipe::State` 绑定数组。通用 descriptor store 为 vertex texture/sampler 分配独立的
  `0xD00`/`0xE00` 地址区，stage、reflection index 与物理 slot 在 `DescriptorAccess` 中分别
  保存；VS Pipeline 从通用 `PipeState` 查询资源。draw action 为 vertex texture 写入
  `VS_Resource` usage。
- 原因：vertex 与 fragment 可以同时使用相同物理 slot，若共享 snapshot 或 descriptor 地址，
  标准 Viewer 会跳到错误资源。source-created MSL 的 pipeline argument reflection 已给出
  vertex texture/sampler 名称和 active 状态，可复用 T03 的 used/unused 语义。
- 边界：本轮只覆盖直接绑定、非空 texture/sampler 和受控 source-created MSL；批量绑定在 T17，
  vertex argument buffer、storage buffer、function constants 与预编译 metallib 分开验证。
  replay 对 slot 128 与缺失资源在对应 chunk 明确失败。
- 验证：T16 四象限 native/capture/replay、VS descriptor/reflection/usage、T03/T12 定向、
  四份异常 RDC、T00-T16 L3（17×10 lifecycle，resident growth 114688 bytes）及最新
  qrenderdoc Event/API/VS Pipeline/Texture/Buffer/Mesh/Resource/DDS/HTML 与状态栏均通过。
## D041：批量 texture/sampler 绑定逐槽保留空槽语义

- 日期：2026-09-23
- 状态：已采用；T17 L3/L4 均通过
- 决策：四个 vertex/fragment 批量入口分别记录 range、逐槽是否绑定以及资源数组；replay 先验证
  range/数量/缺失资源，再一次性调用 Metal 批量 API，并逐槽更新事件 snapshot。空槽明确清除先前
  的绑定；每个非空资源保留 frame reference。fragment texture draw 同时记录 `PS_Resource` usage。
- 原因：单一数组中的 null 既是合法清空操作，也可能来自不存在的 captured resource；独立的
  bound 标志让 replay 区分两者。逐槽 snapshot 与标准 descriptor 的物理 slot、used/unused 语义一致。
- 边界：本轮只覆盖非 LOD clamp 的 vertex/fragment texture/sampler 批量入口；storage buffer、
  LOD clamp、function constants 和任意应用注入独立验证。
- 验证：T17 native/capture/XML/replay、四份异常 RDC、T03/T12/T16 定向、T00-T17 全量
  （18×10 lifecycle）与最新 qrenderdoc VS/FS Pipeline、空槽、Viewer/Resource、DDS/HTML 通过。

## D042：vertex buffer 物理槽按 shader 反射区分 IA 与 storage 语义

- 日期：2026-09-24
- 状态：已采用；T19 L1/L2/L4 通过，L3 未触发
- 决策：`setVertexBuffer` 仍保存唯一物理绑定，但 draw-time snapshot 根据当前 vertex function
  reflection 分类：被 vertex descriptor attribute 引用的槽进入 IA `vertexBuffers`，被 vertex shader
  read-only buffer argument 引用的槽进入 `vertexStorageBuffers`；同一槽若同时满足两类语义可同时出现。
  pipeline 后绑定或切换时重新分类已有物理绑定。vertex storage 使用独立 descriptor 地址区
  `0xF00 + slot`，action 记录 `VS_Resource`，qrenderdoc 通过标准 VS Storage Buffers/Buffer Viewer/
  Resource Inspector 展示精确 offset 和剩余范围。
- 原因：Metal 用同一 `setVertexBuffer` API 同时承载 vertex fetch 和 vertex shader buffer argument，
  仅按 API 名称归入 IA 会把 storage buffer 伪装成顶点流；仅按 reflection 归入 storage 又会破坏普通
  vertex input。以 vertex descriptor 与 shader reflection 两份证据分类，可让 replay、descriptor、
  Mesh/IA 和标准 Viewer 使用一致事实来源。
- 边界：当前只承诺 source-created MSL 的只读 vertex buffer argument、受控 reflection 和非零 offset；
  writable buffer、vertex argument buffer、无反射 metallib、buffer arrays 及跨 pipeline 的复杂别名需
  独立 fixture。非法 slot 和越界 offset 在 replay 调用真实 Metal 前明确失败。
- 验证：T19 的 slot 4 used、slot 6 unused、256+512/320+448 descriptor、四象限、raw/seek/usage，
  T18/T16 必跑及因分类改动触发的 T02/T05 均通过；最新 qrenderdoc 的 IA 空表、VS Storage Buffers、
  Buffer/Resource、HTML/CSV/bin 与 `No problems detected` 已核对。定向证据界定了风险，未执行 L3。

## D043：单命令 ICB 的 execute marker 与展开 draw 各占真实事件

- 日期：2026-09-24
- 状态：已采用；T20 联合 L1/L2/L4 通过，批次已关闭
- 决策：T20 保存 ICB descriptor、index 0 的 pipeline/vertex buffer/draw 编码及
  `executeCommandsInBuffer` 的 range `0+1`。捕获时为 execute marker 和实际执行各写一个 chunk，
  replay 中分别形成连续事件；展开 draw 使用真实 Metal ICB 执行，并把 pipeline、offset 16 的
  vertex binding、拓扑与资源 usage 写入标准 action/pipeline 来源。
- 原因：独立的 marker 和 draw 事件使 Event/API 能显示执行边界，同时事件 seek 可在两者之间区分
  clear 与 draw 输出；把两者压在同一事件会让标准 Viewer 的 draw-time 状态和导航不稳定。
- 边界：仅支持 CPU 编码、单 render pass、容量 2 中 index 0 的一条非索引 draw；不承诺 GPU
  命令生成、inherit pipeline/buffers、indexed ICB、reset、多命令或多 queue。无效 descriptor、
  缺失资源和越界 range/index 在 replay 明确拒绝。
- 验证：T20 native/capture/XML、Replay API action/state/usage/readback/seek、原始字节、6 类
  异常 RDC、联合清单与 lifecycle 通过；最新 qrenderdoc 的 Event/API、IA/Mesh/Buffer/
  Resource、红色输出及 HTML/CSV 验收通过。

## D044：indexed indirect 用真实五字段确定 action 和精确 IA 子范围

- 日期：2026-09-24
- 状态：已采用；T21 联合 L1/L2/L4 通过，批次已关闭
- 决策：T21 保存 indexed indirect overload 的 index/参数 buffer 与各自 offset。replay 从
  20-byte shared 参数包读取 indexCount、instanceCount、indexStart、baseVertex、baseInstance；
  IA index binding 指向 `indexBufferOffset + indexStart * stride`，长度为
  `indexCount * stride`，action 的 indexOffset 相对该 binding 为 0。indirect 参数 binding
  精确指向 20-byte 子范围；通用 index buffer description 标为 `Index`，resource usage 分别
  标为 `IndexBuffer` 与 `Indirect`。
- 原因：indexStart 属于参数包而非 API offset；若 IA 仍从 API offset 展示，会让 Mesh/Buffer
  Viewer 看到被跳过的哨兵，并与实际 GPU 绘制及 action 不一致。
- 边界：仅支持单次 CPU 写入 shared 参数的 UInt16/UInt32 indexed indirect；不承诺 GPU 生成
  参数、indexed ICB、heap 或多 queue。缺失资源、错位/越界 offset、越界或零 index 数在
  replay 明确拒绝。
- 验证：T21 native/capture/XML、Replay API 五字段/IA/Mesh/usage/readback/seek、9 类异常
  RDC、T02/T14 分类路径与联合清单通过；最新 qrenderdoc 的 Event/API、IA/Mesh/Buffer/
  Resource、红蓝输出及 HTML/CSV 验收通过。

## D045：Metal Pipeline 的 indirect 参数格式按 action 区分

- 日期：2026-09-24
- 状态：已采用；T13/T21 最新 qrenderdoc L4 通过
- 决策：Metal Pipeline 的 Indirect Buffer 行根据当前 action 的 `Indexed` 标志展示
  `Draw Indexed Primitives` 或 `Draw Primitives`；跳转标准 Buffer Viewer 时分别使用
  五字段 `indexCount/instanceCount/indexStart/baseVertex/baseInstance` 或四字段
  `vertexCount/instanceCount/vertexStart/baseInstance` 格式。
- 原因：T21 的参数包是 20-byte indexed 布局。固定四字段格式会把 `baseVertex` 误读为
  `baseInstance`，并隐藏真正的第五字段，尽管底层 replay 数据正确。
- 边界：仅修正通用 Viewer 显示与跳转格式，capture/replay 文件及 action/state 不变。
- 验证：最终 `build-qrenderdoc` 成功；T13 Replay API smoke 通过。相同进程中 T21 显示
  20-byte 五字段 `3/2/1/1/1` 和 `Draw Indexed Primitives`，T13 显示 16-byte 四字段
  `3/2/1/1` 和 `Draw Primitives`；T20/T21 同一轮 L4 与导出通过。

## D046：多命令 ICB 的 execute 保留原始 marker，各 draw 各占一条回放 chunk

- 日期：2026-09-24
- 状态：已采用；T22 与 T23 最终联合自动及同轮 qrenderdoc L4 通过
- 决策：一次真实 Metal `executeCommandsInBuffer(icb, range)` 在 capture 中保留完整
  `range` 的 marker，并为范围内每个 command 写独立的 draw 回放 chunk。回放以单 command
  子范围调用真实 ICB，使每个 draw 有不同文件偏移和 EID、可独立 seek 与保存 draw-time
  pipeline/vertex state。
- 原因：单个 chunk 内连续生成多个事件会共享文件偏移，无法通过现有 ReplayLog 事件范围
  精确回放第一条与第二条 draw。拆为逐命令 chunk 后，Event/API、回退和标准 Viewer 均使用
  原有事件语义；捕获进程仍只执行一次原始 range。
- 边界：T22 支持 CPU 编码的非索引多命令 ICB；indexed command 由 T23 接入。无效 range、
  未编码命令、缺失资源和不支持 inheritance 在 replay 明确拒绝。
- 验证：T22 native/capture/XML、逐命令 Replay API action/state/usage/readback/seek、11 类
  异常拒绝及最终联合定向、CLI、9×10 lifecycle 通过；与 T23 同轮 qrenderdoc L4 核对
  `1+2` range、两个 draw、IA/Mesh/Buffer/Resource、HTML/CSV 和无错误状态栏。

## D047：indexed ICB command 保留独立 index 子范围与 draw-time 状态

- 日期：2026-09-24
- 状态：已采用；T23 最终联合自动及同轮 qrenderdoc L4 通过
- 决策：T23 在 CPU 编码的 render ICB command 中保存 index type/buffer/byte offset、
  indexCount、instanceCount、baseVertex、baseInstance；replay 重建真实 ICB indexed command，
  展开 draw 使用 `Drawcall|Indexed|Indirect|Instanced` 和精确 index 子范围。Metal 的 ICB
  indexed 方法没有独立 `indexStart`，本场景用非零 `indexBufferOffset` 表示索引起点。
- 原因：T21 的 indexed indirect 参数 buffer 与 T23 的 ICB command 是不同 API 路径。
  index 子范围、两条 vertex binding 和实例布局需在同一 draw-time state 中一致，才能让
  IA/Mesh/Buffer/Resource Viewer 显示真实执行的输入。
- 边界：本场景仅承诺 CPU 编码、UInt16 fixture 与非继承 descriptor；实现可接收经检查的
  UInt16/UInt32，GPU 生成命令、reset、继承绑定、heap 和多 queue 留待独立场景。
- 验证：T23 native/capture/XML/Replay API、6-byte index raw、18 类异常拒绝、联合定向、
  CLI、9×10 lifecycle 通过。最新 qrenderdoc 显示 command index 1、UInt16 Buffer 18
  offset 4/size 6、Buffer 16/17 与 Mesh 两实例、Index Buffer/ICB usage、红蓝输出、
  HTML/CSV/DDS 和无错误状态栏。

## D048：ICB reset chunk 追加编号并同步清空真实命令与 capture snapshot

- 日期：2026-09-24
- 状态：已采用；T24 最终联合 L1/L2/L4 通过，批次已关闭
- 决策：把 `MTLIndirectCommandBuffer_reset` 作为新的 Metal chunk 追加在既有 indexed ICB
  chunk 之后，避免重编号已产生 capture 的 chunk id。capture 记录 `resetWithRange`；replay 先
  严格验证非空、无溢出且在容量内的 range，再对真实 ICB 调用 reset，并把同范围的
  `MetalIndirectDraw` snapshot 清空。后续对相同 command index 的编码从空 snapshot 重建。
- 原因：只重置真实 Metal 对象会让旧 pipeline/vertex/index 资源继续出现在 action/state/usage；
  只清 snapshot 又会让 GPU 执行旧命令。追加 chunk id 同时保持现有测试 capture 的枚举稳定。
- 边界：当前承诺 ICB buffer 的 `resetWithRange`；单个 `MTLIndirectRenderCommand::reset`、blit
  encoder 的 reset/copy/optimize、GPU 生成命令另立场景。重置后未重编码的 execute 明确失败。
- 验证：T24 native/capture/XML、三个展开 draw、4×104-byte 原始包、旧资源无 VertexBuffer
  usage、8 类异常、T20/T22/T23 定向、CLI/lifecycle 与最新 qrenderdoc replacement/邻居/
  HTML/CSV/红绿蓝输出通过。

## D049：混合 ICB descriptor 保留 commandTypes 位掩码并按每条命令验证类型

- 日期：2026-09-24
- 状态：已采用；T25 最终联合 L1/L2/L4 通过，批次已关闭
- 决策：`WrappedMTLIndirectCommandBuffer` 保存原始 `MTLIndirectCommandType` 位掩码；当前
  允许 Draw、DrawIndexed，以及二者组合且最多两个 vertex buffer 的非继承 descriptor。
  `drawPrimitives`/`drawIndexedPrimitives` 分别检查对应 bit，再为每个 command snapshot 保存
  独立类型与资源。execute 展开时非索引 action 明确清空 index state，indexed action 保留精确
  UInt16/UInt32 子范围。
- 原因：把组合 descriptor 折叠成单一类型会错误拒绝其中一条命令，或让非索引 draw 继承前一条
  indexed binding。保留位掩码并在命令入口验证，能让真实 Metal、capture 诊断和 Viewer 状态一致。
- 边界：当前组合场景只承诺 CPU 编码、非继承 pipeline/buffers、零 fragment bind count；
  inheritance 由 T26/T27 覆盖，compute ICB、ray tracing、heap、GPU 生成与多 queue 不在本决策内。
- 验证：T25 native/capture/XML、两个 action、252-byte 原始资源、15 类异常、联合定向、CLI、
  11×10 lifecycle 通过；最新 qrenderdoc 显示 direct 无 index、indexed UInt16 4/6 与两实例、
  独立 IA/Mesh/Buffer/Resource、红绿蓝输出、HTML/CSV/DDS 和无错误状态栏。

## D050：compute thread grid 与 buffer descriptor 沿标准 replay action/state/Viewer 接入

- 日期：2026-09-24
- 状态：已采用；T28/T29 自动验证通过，用户 L4 待验
- 决策：`dispatchThreads` 与 compute `setBuffer` 各追加 Metal chunk id，保持旧 capture
  编号稳定。T28 action 保存 total grid 与 threadsPerThreadgroup，并验证非零、尺寸和资源。
  T29 将 buffer slot/offset/剩余范围写入 `MetalPipe::State::computeBuffers`；descriptor access
  以独立偏移段区分 compute 只读和读写 buffer，CS Pipeline 使用标准 Buffer Viewer 跳转。
  output buffer 在 frame 内经 blit fill 为哨兵，支持 event seek 的前后对比。
- 原因：仅转发真实 Metal 调用无法在 RDC 中重建绑定和 dispatch，也无法让标准 CS
  Viewer、Resource usage 与逐事件数据保持一致。
- 边界：本批 fixture 限 2D RGBA8 texture 和 shared uint buffer；未覆盖 compute sampler、
  间接 dispatch、跨 queue、heap 或 compute ICB。非法 slot/offset/resource/grid 稳定拒绝。
- 验证：T28/T29 native/capture/XML/Replay API、10 类异常 RDC、联合定向、CLI、
  11×10 lifecycle 与 DDS/raw 内容自动通过；合并 GUI L4 见 `QA_BATCH29-30.md`。

## D058：按 Vulkan/DX12 的实际支持边界补齐 Metal

- 2026-10-05，用户再次明确，采用。每项图形/光追能力先查对应后端实现，
  支持的优先复用 RenderDoc 捕获、资源依赖、提交、事件与恢复模型；
  对应后端不支持的功能不在 Metal 独自扩展。Metal 独有功能先证明必要性。
- `docs/behind_scenes/raytracing.rst` 确认光追为黑盒：保存并正确执行 GPU 工作，
  输出供后续普通图形调试使用，不提供光追内部调试。AS marker 使用统一
  ActionFlags 和事件树；IFT 空槽更新只为重建必要执行状态，不增新 viewer。
- 受控射线样例不等于任意应用支持；在执行期实例数据、帧前 AS 初态和函数表
  提交快照等缺口闭环前，保持两项 raytracing 能力查询为 false。
- 实施与后续门槛见 [PHASE54](PHASE54.md)。

## D059：AS 初态保存不可变构建输入，按已证明的提交与依赖重建

- 2026-10-05，采用。参照 Vulkan `vk_acceleration_structure.cpp`、DX12
  `d3d12_initstate.cpp`，保存输入与资源身份，在回放设备重建，不复制 Metal
  opaque AS 存储，也不扩展光追内部查看/调试。
- 描述符参数在调用时固定；顶点、索引和实例字节在 commit 前固定。Shared
  可读性不代表 producer 已完成；需要提交完成、无其它 reservation、构建 CB
  无输入写入。保留原生提交读 status，不在捕获切换锁内等待应用完成回调。
- TLAS 保留子 build-record 身份。底层结构被重新构建后，不能拿新输入替换旧
  TLAS 的依赖而宣称恢复正确。每轮先验证子初态完整，再先 BLAS、后 TLAS。
- 尺寸查询和实际重建均使用冻结输入。完整 vertex stride 的尾 padding 补零；
  padding 不属于几何输入。源缓冲区后来改写时保留其当前初态，AS 仍使用旧输入。
- System InitialContents 的 schema1/2 可读，schema3 增加索引输入；旧 buffer/
  texture 字段和 driver chunk ID 不变。未证明的输入明确拒绝，设备能力仍 false。
- 结果与接续见 [PHASE55](PHASE55.md)。

## D060：refit/copy初态保留输入版本，compact先重建再验证容量

- 2026-10-05，采用。按Vulkan/DX12的AS输入记录和重建方法恢复refit/copy；
  Metal nil目标归一到源ID，沿用现有refit chunk，不新增内部调试能力。
- copy持有独立不可变recipe；commit检验编码时源版本未被其它提交替换。
  原地refit的占位记录不能参与此版本比较；先固定提交前源版本集合。
- 完整回放清除全部AS的逻辑构建状态，再应用初态，避免异地refit目标沿用
  上轮执行状态。静态索引和refittable索引沿用不同的既有count元数据约定。
- compact初态在全尺寸临时AS中重建，查询本机实际压缩尺寸，完成且容量足够
  才copy到原分配；恢复初态尺寸查询producer身份，保留公共尺寸缓冲帧首字节。
- schema4补compact和尺寸查询证明，保持schema1/2/3读取；未证明执行点的输入
  明确拒绝，设备能力仍false。结果和接续见 [PHASE56](PHASE56.md)。

## B486–B487：执行点输入冻结与实例编号格式

AS encoder结束后隐藏blit只扩独立Tracked Private多indexed输入，拒绝Shared/NoCopy别名和heap/untracked等未经证明路径；延续kind8/schema5。直接UserID实例使用kind5/schema6并严格schema/type配对，按原始offset/stride提取68-byte规范化输入，完整uint32编号保留；默认实例schema1–4与multi-indexed schema5不改。间接GPU ID只能经typed资源关联重定位，不能把buffer中任意整数猜成地址。此批未扩Private/Indirect实例或公开能力。

## B488：间接实例 typed AS 标识

kind9/schema7保存72-byte公开Indirect实例与typed child ResourceIds；GPU标识只取自AS getter记录的关联。重放规范化为68-byte UserID实例和live child handles，避免原始捕获ID提交或任意整数扫描，保留完整userID/flags/mask/IFT。帧内先限定Shared CPU静态输入与此前提交的1–4个primitive子AS；Private/GPU输入另行验收。身份metadata跨活AS占有冲突必须拒绝。

## 2026-10-07 用户执行准则：重放恢复与访问展示分离（优先于历史 guard 策略）

不能把不断扩充 CPU 上的 shader 表达式分析当作通用 API 适配的全部。按仓库 DX12/Vulkan 的实现，分别推进以下四层：

| 层次 | 应完成的机制 | 是否可以阻止重放 |
|---|---|---|
| 资源恢复 | 捕获真实初态/创建数据；恢复 AS recipe、Private/GPU 输入、帧内创建及生命周期；按原生 API 重做生产者 | 缺资源、失效/遗漏初态或 AS 恢复失败必须修复并拒绝错误重放 |
| 地址重定位 | 通过已知对象和真实 ABI/typed factory 来源，重建 root、descriptor、AS Header、GPU 生成指针及复制的地址字段 | 未恢复的 GPU 指针、错误来源/类型/范围必须修复；禁止猜测地址位型或简单忽略错误 |
| 提交依赖 | 保留原生队列、encoder、生产者→消费者和物理别名顺序，正确恢复事件前后及 EID0 | 缺生产者、未提交依赖或生命周期错误必须修复 |
| shader 访问展示 | 静态访问和可获得的动态反馈只用于报告真实/部分访问；声明的绑定/驻留资源不是实际访问列表 | 动态索引/条件/循环/数值表达式没有被 CPU 分析完整，不能仅因此永久拒绝已可正确重放的 API 调用 |

资源恢复证明与访问展示结果必须使用独立状态，不能继续用同一个 `UniformResourceAccess` 报告同时决定“能否重放”和“能否精确展示”。CPU 分析可以作为可选诊断/已知访问证据，不是必须执行所有 shader 表达式的 CPU 模拟器。不存在完整展示证据时，明确报告部分/未知展示，不把整个 heap 伪装成 shader 实际访问。GPU 生产者仍由真实 Native shader/API 重做；未恢复的地址字段要补真实重定位机制，不能用旧初态或 CPU 算出的输出替代。

本地参照：`renderdoc/driver/d3d12/d3d12_command_list_wrap.cpp::Serialise_Dispatch`、`renderdoc/driver/vulkan/wrappers/vk_draw_funcs.cpp::Serialise_vkCmdDispatch` 重放原生调度；两者 `GetDescriptorAccess` 使用静态访问与可用的 shader feedback 展示。DX12 在 `d3d12_replay.cpp:1995` 仅追加 valid 的动态反馈，不把 CPU 数值分析作为普通 Dispatch 的必要条件；Vulkan 对应 `vk_replay.cpp:2990` 和 `vk_shader_feedback.cpp::FetchShaderFeedback`。资源、地址及 descriptor 支持范围继续优先参考 VK/DX12；Metal 独有且必要的地址语义才补机制。RT 保持黑盒，Lumen 只作实际应用验收。

历史文档中“unknownBuffer/unknownCall 一律拒绝”及只以扩充表达式语法解决所有新 feature 的下一步，均被本准则取代。历史失败/通过日志仍保留，不更改已经发生的结果。每个新拒绝必须区分资源恢复、地址重定位、提交依赖、非法 API/capture 和纯展示不足，记录具体依据；纯展示不足不作为新支持范围的永久否决条件。坏 capture 验收继续覆盖缺失资源/来源、错误身份和地址字段、生命周期与提交破坏；只修改普通 shader 索引/数值的旧“拒绝 oracle”要审查其真实语义，不能为了维持旧预期重新绑定展示与重放。

下一项仍为同一 B544/PHASE59 大批次：先拆 `metal_descriptor_tables.cpp::validateRuntimeDispatch` 的恢复/重定位/提交资格与 `metal_shader_inspect.cpp`/`GetDescriptorAccess` 展示路径。用 Native 正确但静态分析不完整的 GPU 索引/分支 sample 验证“重放通过、展示部分”，再验证未恢复 GPU 指针/缺生产者等确实失败的控制。恢复机制完成并独立验证后，移除仅由展示不足造成的 hard gate；不能直接把所有 unknown 放行。随后回当前 UE 实际 capture，检查整体输出、真正 RT 调度、事件、EID0 和绑定，再集中验收生产能力开关。

2026-10-07 实现进展：B544 已验证首段“已知对象/恢复指针 + 动态偏移或分支候选范围 → Native 重放通过、展示部分”。下一处理独立 pointer provenance/namespace资格；当前 UE pointer select 丢失身份不能自动归类成实际地址缺失，不以继续扩充 CPU 分支/数值表达式作为主要修复。完整证据见 BATCH544 当前 continuation。

2026-10-07 B544当前进展：指针分支、结构投影与Native原子/调用资源效果保留已有恢复来源，数值展示不完整可Native重放/展示部分；嵌入原捕获VA独立拒绝并要求实际重定位。真实UE预检已越过旧首commit557952与六MRT；下一核实draw3330304的fragment阶段可选性/身份，不继续按数值表达式或引擎名字逐个准入。scope、失败和当前2133a76f验证见BATCH544；完整UE/最终能力开关仍未验，目标active。

2026-10-07 B544推进到f9718c71：可选Native阶段与barrier/fence资源效果独立，空shader阶段不产生访问列表，GPU共享内容保持未知但原生执行。UE越过fragment旧guard，下一1624的known-buffer/null选择后动态GEP来源资格；当前四unknown不证明实际地址恢复失败，先核实来源/字段语义，不将模拟共享数组或数值表达式作为主修复。当前scope/FAIL/未验在BATCH544，任务active、生产flagsfalse。

2026-10-07 B544当前529d0a7b：分支来源/相对索引、固定shader地址重定位与对象metadata/像素初态独立；GPU执行原shader，CPU展示partial不永久拒绝。已知predicate也不省略literal捕获VA/span校验，pixel读保留初态/producer。下一实际3836是Native SIMD效果与texture atomic读写/初态/提交闭包，再runtime AS/Header，不模拟lane算法、不只删unknownCall。standalone frame texture仍未实现，不能以placement fixture替代。PASS/FAIL/未跑在BATCH544，目标active/flags false。

2026-10-07 B544当前8d6ec636：Native SIMD/atomic返回保持unknown；恢复source/null phi与动态offset可原GPU重放/展示partial，texture store/load/RMW、初态/producer和提交独立。Native与帧factory共用texture→buffer布局/overflow/生命周期/alias/预算，真实写不恢复CPU旧事实。两同/跨提交fresh全输出/各72事件通过，UE越过3836，下一3853 frame buffer-texture对象/写资格拒绝待诊断，不以扩CPU数值表达式代替恢复。PASS/FAIL/未跑见B544/PHASE59，flagsfalse、目标active，用户四层规则持续有效。

2026-10-07 B544当前4163d391：frame buffer-texture对象/逻辑范围、当前copy字节有效性、像素restoration intervals独立CPU数值展示；root与texel backing合法不重叠可Native重放，缺像素恢复/真实overlap仍GPU前拒绝。UE越过3853，下一3854 background GPU descriptor初态/namespace与Private producer链；已有gpuExpected/opaque历史标记不能直接等于当前恢复失败，不以新增CPU数值表达式代替资源/地址/提交恢复。PASS/FAIL/未跑见B544/PHASE59，flagsfalse/目标active，用户四层准则持续有效。

2026-10-07 B544通用资源恢复首组：已实际移除frame纹理/view组合许可、coverage容量与command/dispatch数量许可，统一布局/投影/预算；Native stride/metadata/真实producer恢复和JIT无AIR展示unknown已由独立sample变体及相关回归验证。后续按通用资源/地址/提交机制继续清理剩余coverage族与未知namespace/call，不按UE feature/pass/name/hash/EID准入；不将supervised预检完成当UE成功。当前UE仍API4无GPU，整帧/最终启用未验，flagsfalse。详见GENERIC_API_RECOVERY/B544最新段与根AGENTS，覆盖历史场景逐项许可策略。

2026-10-07 B544 Native register API effect compatibility与按resource索引的依赖恢复：不计算CPU bit值、不按shader/EID/hash准入；完整prototype只提供资源效果，typed来源/初态/生命周期/提交校验独立。短CPU sample定位全局descriptor逐resource扫描，lower_bound保留本resource全部live字段/来源/依赖。UE同45s/3GiB限制内约25s到真实API4，下一同dispatch scalar事实/读写资源交叠；不拿监督exit0当UE PASS，不直接删除检查或用失效CPU数据选GPU资源。详见B544/PHASE59/GENERIC最新段，flagsfalse/目标active。


2026-10-08：f1ba1862对应compute RT门槛在公开能力路径、官方/UE完整Native输出、事件/reset、独立合法组合与同候选固定矩阵完成后通过；supportsRaytracing直接Native委托，FromRender独立未验保持false。按DX12 OPTIONS5原生tier边界与VK物理RT/capture-replay恢复条件区分能力和恢复，不按UE名称/PSO/EID授予资格。最终范围/未跑/历史FAIL见HEAP_BACKING_RECOVERY最终节；NaN等旧实现拒绝不反推Native非法。持续任务此目标达成后停止。
