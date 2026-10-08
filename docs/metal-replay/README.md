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

当前光追进展：[PHASE56](PHASE56.md)，完成 [B474 refit/copy](BATCH474_RAY_REFIT_AND_COPY_INITIALS.md)、[B475 compact/版本校验](BATCH475_RAY_COMPACT_INITIALS.md)。最终定向218正例/5,199事件选择/281损坏输入/4边界/490生命周期通过；Shared基础AS生命周期可用，尚非通用光追。

# RenderDoc Metal Replay 项目入口

当前光追阶段：[PHASE55：AS 初态恢复](PHASE55.md)，结果见 [B473](BATCH473_RAY_INDEXED_INITIALS.md)。

本目录是 RenderDoc v1.46 Metal replay 适配工作的唯一计划与交接入口。实现代码仍放在
RenderDoc 原有目录中，项目状态、阶段计划、测试覆盖和关键决策统一记录在这里。

## 项目目标

在 macOS 上为 RenderDoc v1.46 建立一套可用的 Metal 离线 replay 能力。首版完成后，
用户应当能够在 qrenderdoc 中打开受支持的 Metal `.rdc`，浏览事件，并查看：

- Buffer 列表、元数据和原始/格式化内容。
- Texture 列表、子资源、像素和常见格式。
- Render pipeline、render pass、资源绑定和固定功能状态。
- 顶点/索引输入以及 Mesh Viewer 中的网格。
- Metal shader 的入口点、阶段、可获得的 MSL 源码和反射信息。
- 选定 draw call 的 replay 输出。

这一阶段不以 shader debugging、pixel history、性能计数器、ray tracing、mesh shader、
MetalFX、任意第三方应用注入或完整 capture 产品化为目标。为了产生可重复测试输入，允许
实现最小范围的 capture/序列化补全和测试夹具，但它们只服务于 replay 验证。

## 固定基线

- 上游：`https://github.com/baldurk/renderdoc.git`
- 标签：`v1.46`
- 基线提交：`e4bd23b671d3d5a747ff5221dbe08a63eb6ca200`
- 开发分支：`metal-replay-v1.46`
- 工作平台：Apple Silicon macOS

## 文档导航

- [PLAN.md](PLAN.md)：总体路线、阶段门槛和验收条件。
- [PHASE1.md](PHASE1.md)：已完成的 Metal 样例与 capture 阶段记录。
- [PHASE2.md](PHASE2.md)：已完成的 T00/T01 replay 后端纵向闭环。
- [PHASE3.md](PHASE3.md)：已完成的 T02 索引立方体、depth 与 Mesh Viewer 纵向闭环。
- [PHASE4.md](PHASE4.md)：已完成的 T03 纹理采样、资源绑定与 Pipeline UI 收敛计划。
- [PHASE5.md](PHASE5.md)：已完成的 T04 动态 uniform 与 fragment buffer binding 纵向切片。
- [PHASE6.md](PHASE6.md)：已完成的 T05 多 vertex buffer 与实例化网格纵向切片。
- [PHASE7.md](PHASE7.md)：已完成的 T06 MRT 与 blending 纵向切片。
- [PHASE8.md](PHASE8.md)：已完成的 T07 depth/stencil 纵向切片。
- [PHASE9.md](PHASE9.md)：已完成的 T08 MSAA resolve 纵向切片。
- [PHASE10.md](PHASE10.md)：已完成的 T09 mip/cube/array 子资源纵向切片。
- [PHASE11.md](PHASE11.md)：已完成的 T10 buffer/texture blit 纵向切片。
- [PHASE12.md](PHASE12.md)：已完成的 T11 compute texture filter 纵向切片。
- [PHASE13.md](PHASE13.md)：已完成的 T12 单层直接 argument buffer 资源引用纵向切片。
- [PHASE14.md](PHASE14.md)：已完成的 T13 单次间接 draw 参数纵向切片。
- [PHASE15.md](PHASE15.md)：已完成的 T14 indexed instancing/base vertex 纵向切片。
- [PHASE16.md](PHASE16.md)：已完成的 T15 point/line 基础拓扑纵向切片。
- [PHASE17.md](PHASE17.md)：已完成的 T16 vertex texture/sampler binding 纵向切片。
- [PHASE18.md](PHASE18.md)：已完成的 T17 texture/sampler 批量绑定纵向切片。
- [PHASE19.md](PHASE19.md)：已完成的 T18 fragment storage buffer 纵向切片。
- [PHASE20.md](PHASE20.md)：已完成的 T19 vertex storage buffer 纵向切片。
- [BATCH21-22.md](BATCH21-22.md)：已关闭的 T20/T21 联合验收记录。
- [PHASE21.md](PHASE21.md)：已完成的 T20 单命令 indirect command buffer 纵向切片。
- [PHASE22.md](PHASE22.md)：已完成的 T21 indexed indirect draw 纵向切片。
- [BATCH23-24.md](BATCH23-24.md)：已关闭的 T22/T23 多命令与 indexed ICB 联合验收记录。
- [PHASE23.md](PHASE23.md)：已完成的 T22 多命令 ICB 与非零 execute range 纵向切片。
- [PHASE24.md](PHASE24.md)：已完成的 T23 indexed ICB command 纵向切片。
- [BATCH25-26.md](BATCH25-26.md)：已关闭的 T24/T25 ICB reset 与混合命令联合验收记录。
- [PHASE25.md](PHASE25.md)：已完成的 T24 ICB resetWithRange 与重编码纵向切片。
- [PHASE26.md](PHASE26.md)：已完成的 T25 混合非索引与 indexed ICB 纵向切片。
- [BATCH27-28.md](BATCH27-28.md)：已关闭的 T26/T27 ICB pipeline/buffers inheritance 联合验收记录。
- [PHASE27.md](PHASE27.md)：已完成的 T26 ICB 继承 render pipeline 纵向切片。
- [PHASE28.md](PHASE28.md)：已完成的 T27 ICB 继承 vertex buffers 纵向切片。
- [BATCH29-30.md](BATCH29-30.md)：已关闭的 T28/T29 compute dispatch 与 buffer binding 联合验收记录。
- [PHASE29.md](PHASE29.md)：已完成的 T28 compute dispatchThreads 纵向切片。
- [PHASE30.md](PHASE30.md)：已完成的 T29 compute buffer binding 纵向切片。
- [BATCH31-32.md](BATCH31-32.md)：已关闭的 T30/T31 compute sampler 与批量资源绑定联合验收记录。
- [PHASE31.md](PHASE31.md)：已完成的 T30 compute sampler 直接绑定纵向切片。
- [PHASE32.md](PHASE32.md)：已完成的 T31 compute texture/sampler/buffer 批量绑定纵向切片。
- [BATCH33-34.md](BATCH33-34.md)：已关闭的 T32/T33 compute 间接 dispatch 批次。
- [PHASE33.md](PHASE33.md)：已完成的 T32 CPU 参数 compute 间接 dispatch。
- [PHASE34.md](PHASE34.md)：已完成的 T33 GPU 生成参数 compute 间接 dispatch。
- [BATCH35-37.md](BATCH35-37.md)：T34–T36 render 绑定、命令创建与动态状态批次；自动通过，GUI L4 待验。
- [PHASE35.md](PHASE35.md)：T34 render inline bytes 与 buffer batch binding。
- [PHASE36.md](PHASE36.md)：T35 command queue/buffer/compute encoder 创建变体。
- [PHASE37.md](PHASE37.md)：T36 render 动态状态。
- [BATCH38.md](BATCH38.md)：T36 visibility/store/barrier、T37 blit transfer、T10 marker 与 D32S8 API。
- [PHASE38.md](PHASE38.md)：T37 pitched buffer/texture 和 whole/ranged texture copy。
- [BATCH39-40.md](BATCH39-40.md)：T38 sampler LOD、T39 Private buffer readback 和 compute buffer-output。
- [PHASE39.md](PHASE39.md)：六项 VS/FS/CS sampler LOD 入口与有效事件状态。
- [PHASE40.md](PHASE40.md)：Private GPU buffer staging readback。
- [BATCH41-42.md](BATCH41-42.md)：T40 compute inline/offset/threadgroup，T41 blit descriptor/optimization。
- [PHASE41.md](PHASE41.md)：Compute inline bytes、buffer offset 与动态threadgroup memory。
- [PHASE42.md](PHASE42.md)：Blit descriptor 与texture optimization、hint-only资源保留。
- [BATCH43-44.md](BATCH43-44.md)：Compute/Render资源声明、barrier与vertex写buffer，终端联合验证。
- [PHASE43.md](PHASE43.md)：T42 Compute resource/barrier/marker七个入口。
- [PHASE44.md](PHASE44.md)：T43 Render staged声明/barrier及vertex RW descriptors。
- [BATCH45-46.md](BATCH45-46.md)：Fence同步、定时present、buffer debug marker及终端联合验证。
- [PHASE45.md](PHASE45.md)：T44 Fence创建、Blit/Compute/Render update/wait。
- [PHASE46.md](PHASE46.md)：T45/T46 两种present调度参数和buffer annotations。
- [BATCH47.md](BATCH47.md)：T47 三个同步pipeline入口及完整终端回归。
- [PHASE47.md](PHASE47.md)：Pipeline options/reflection、compute descriptor与线程组约束。
- [BATCH48.md](BATCH48.md)：四种预编译library加载、离线回放与源码路径兼容检查。
- [PHASE48.md](PHASE48.md)：Binary library嵌入、失败返回值及library/function提前释放。
- [BATCH49.md](BATCH49.md)：两种command buffer回调、CPU更新回退修复与联合自动回归。
- [PHASE49.md](PHASE49.md)：回调包装/生命周期、提交前快照及shared CPU更新部分replay。
- [BATCH50.md](BATCH50.md)：纹理CPU读取/Managed同步、176类异常与51份联合回归。
- [PHASE50.md](PHASE50.md)：两种getBytes元数据、pitch/子资源验证与读回后GPU依赖。
- [BATCH51-52.md](BATCH51-52.md)：六种异步创建与Event同步，bridge降至149，53份联合回归。
- [PHASE51.md](PHASE51.md)：异步Library/PSO回调包装、descriptor快照与资源生命周期。
- [PHASE52.md](PHASE52.md)：Event创建/signal/wait、epoch重建、跨提交数据依赖与回退。
- [BATCH53.md](BATCH53.md)：ICB GPU操作/单命令reset、初值恢复、194新异常与54份联合回归。
- [PHASE54.md](PHASE54.md)：Vulkan/DX12对照的基础光追回放、AS markers/fences、函数表清空恢复及后续初态门槛。
- [PHASE53.md](PHASE53.md)：Shared render ICB范围、copy/reset/optimize、空命令及未知初值拒绝。
- [QA_CONSOLIDATED.md](QA_CONSOLIDATED.md)：当前所有未验功能的集中 GUI QA 总入口。
- [ACTION_NAME_ALIGNMENT.md](ACTION_NAME_ALIGNMENT.md)：跨 API action 名称审查与后续对齐计划。
- [REAL_WORLD_CAPTURE_ROADMAP.md](REAL_WORLD_CAPTURE_ROADMAP.md)：UE/Unity、Nanite、光追、mesh shading 与本机/M4 的路线评估。
- [STATUS.md](STATUS.md)：当前状态、最近验证结果、阻塞项和下一步。
- [TEST_MATRIX.md](TEST_MATRIX.md)：Metal API/资源/UI 覆盖矩阵与测试样例来源。
- [HANDOFF.md](HANDOFF.md)：新 agent 的接手规则、省额度验证节奏、compact/新任务边界和可复制提示。
- [QA_GUIDE.md](QA_GUIDE.md)：从 BATCH29-30 起的命令行 QA 与用户 GUI L4 分工、一次性验收单格式。
- [QA_PENDING.md](QA_PENDING.md)：跨批次保留每个 T 的人工 L4 待验状态与用户反馈。
- [QA_BATCH29-30.md](QA_BATCH29-30.md)：最终 T28/T29 正式 captures 的合并 GUI 验收单。
- [QA_BATCH31-32.md](QA_BATCH31-32.md)：T30/T31 正式 captures 的合并 GUI 验收单。
- [QA_BATCH33-34.md](QA_BATCH33-34.md)：T32/T33 正式 captures 的合并 GUI 验收单。
- [QA_BATCH35-37.md](QA_BATCH35-37.md)：T34/T35/T36 的后续一次性 GUI 验收单。
- [QA_BATCH38.md](QA_BATCH38.md)：T36 增量、T37 和 T10 marker 的后续 GUI 验收单。
- [HANDOFF_HISTORY.md](HANDOFF_HISTORY.md)：按需追查的历史阶段交接证据。
- [DECISIONS.md](DECISIONS.md)：关键架构与范围决策。

## 当前状态

T00–T33 的 Native/Capture/RDC inspect/Replay/UI 纵向切片已关闭。此后按功能族持续
扩展；实时进度、原始 bridge/旧 chunk 计数、最终终端回归和明确未支持项只维护在
[`STATUS.md`](STATUS.md)顶部，避免在本索引重复易过期的数字。T34以后尚未人工验收的
功能继续保留在[`QA_PENDING.md`](QA_PENDING.md)，集中 GUI 操作入口为
[`QA_CONSOLIDATED.md`](QA_CONSOLIDATED.md)。按用户要求，当前开发阶段不运行
GUI/Computer Use，不把终端通过误记为 UI 关闭。
跨 API action 名称审查见
`ACTION_NAME_ALIGNMENT.md`，部分 Metal 名称待后续显示一致性工作处理。
接手时先读本入口、`STATUS.md` 顶部、当前 BATCH、`QA_PENDING.md`，再按需读
`HANDOFF.md`、`QA_GUIDE.md` 验证规则和`PLAN.md`；历史文档
按需查阅。正式 captures 与本机构建仍保留在各自的忽略目录。
从 BATCH29-30 起，agent 完成终端可判定的 QA；最终 GUI 交互由用户按合并的验收单
一次完成。未收到用户明确反馈的功能会持续列在 `QA_PENDING.md`，以后每次结果都会提示；
下一功能可以继续开发，未验批次不会被标为关闭。

## 初始基线结论（2026-09-20）

RenderDoc v1.46 已有约 1.5 万行 Metal 驱动骨架，包含对象包装、部分 capture 序列化、
初始资源内容和 Objective-C bridge。然而它并不是可用的 replay 后端：

- `MetalReplay` 尚未实现 `IReplayDriver`。
- 尚未注册 Metal replay provider，qrenderdoc 不能把 Metal `.rdc` 作为可 replay capture 打开。
- `MetalReplay` 当前只有资源描述索引辅助函数。
- bridge 中约有 237 个 `METAL_NOT_HOOKED()`；Metal 目录中约有 402 个未实现/未处理标记。
- `WrappedMTLDevice::AddAction()` 和 `AddEvent()` 仍未实现，事件树尚未形成。
- `ProcessChunk()` 只处理少量资源和 draw 相关 chunk，大量常见状态仍直接报未处理。

因此首个工程里程碑不是扩充所有 Metal API，而是先建立可编译、可启动、可截取、可打开并可
replay 的最小纵向链路，再按测试矩阵逐项扩大支持面。每个新 feature 都先验证样例原生运行，
随后补 capture，再立即补同一 feature 的 replay 和 UI 检查，避免积累一批无法验证语义的 `.rdc`。

2026-10-05 B493定向验收：全禁用Indirect槽位kind11/schema9，仅接受每packet ASID0/mask0，完整transform/flags/IFT offset/userID冻结。最终backend/bundle 08028e4c7e2048c3ac37d0de00d712de139c3026b4960d8c836d4cc12f3b255f，GUI3ba30e36。五例Shared帧前/帧内、Private/placement帧前、同CB distinct alias擦零：native/capture/API588事件/EID0/CLI及完整72-byte snapshot oracle PASS；4×43+36=208坏输入、20旧检查/T12416反例、6×10生命周期PASS（growth425984bytes/hash一致）。产物captures/metal-ray-b493、build-macos-debug/metal-ray-b493/indirect-manifest.json。官方/UE/68份RT与本库集中接续中；ARC/原Qt crash/GUI/UE RT未闭环，公开能力false，未提交/推送。

B493 08028e4c接续验收：官方两scene/10尺寸查询/41坏sample/6能力查询、68份B482–493 RT7785事件、AS身份旧反例/schema2/旧Indirect与空TLAS反例PASS；固定库集中308/7784/3080 PASS，growth0bytes/exit0/工作和冻结库起止hash一致。产物frozen-validation-08028e4c/full-regression.log与metal-ray-b493/followup-manifest.json。UE session20261005-225938进入截帧后Private/Tracked placement、54实例、background0被bridge主动ForceCrash拒绝；仍未证明RT dispatch/输出/离线。B494已编辑，待串行构建/GPU，不能计此证据；生产能力false。

2026-10-05 B494定向验收：帧内Private/Tracked（独立或placement）间接实例输入按AS encoder消费点冻结，提交完成后保持原API chunk位置/metadata写入；重放typed live child/UserID staging或全禁用Indirect。精确范围别名仅在已验证冻结build、native encoder关闭且无descriptor backing冲突时允许，EID0清空资格。最终backend/bundle 111c4787c2a7fddf59bdd7503e2ea89f8227c23d72962f584942cb7e2e65a14d，GUI3ba30e36。七例native/capture/API954事件/EID0/CLI3 PASS，含同TLAS两build原始userID73/74、两种inactive完整bytes oracle及两份合法CPU别名提前创建控制；5×43+2×37+6=295坏输入/alias反例、20旧检查/T12416及8×10生命周期PASS（growth1081344bytes/起止hash一致）。产物captures/metal-ray-b494、metal-ray-b494/indirect-manifest.json。两次实现/判定FAIL日志保留，不计通过。官方/UE/75份RT和本库集中尚未跑；08028e4c集中308/7784/3080/growth0属于旧库。公开能力false，ARC/原Qt crash/UE RT未闭环，未提交/推送。
