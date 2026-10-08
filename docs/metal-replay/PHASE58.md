B493开发检查点：native inactive探针已证实count1、ASID0/mask0可构建并ray miss；产物build-macos-debug/metal-ray-b492/inactive-native-probe.log。新增kind11/schema9全禁用槽位配方、Private执行点冻结及完整72-byte packet oracle，五场景与反例脚本已编辑/语法检查；尚未构建或GPU验收。当前仍ae81e24a，公开能力false。实际UE的零候选输入需新诊断，不推定语义。

# PHASE58：真实 UE 光追调度适配

官方 Metal2 两scene已通过样例门槛；本阶段以本机 Testproj/UE5.8 实际调度
驱动修复。诊断进程仅严格 env probe=1 放行原生 compute 能力；生产false。

[B481](BATCH481_RAY_HEAP_AS.md) 已修复原生heap AS逃逸，六种时机/分配模式、
坏捕获、重放、生命周期和官方两scene回归通过。[B482](BATCH482_RAY_MULTIPLE_INDEXED_GEOMETRIES.md) 已补共享VB/IB多indexed
geometry、初态及Shared TLAS物化；十场景和官方回归通过。真实UE越过该构建，
当前失败于无输入TLAS尺寸查询，B483继续。随后根据实际日志
处理 heap 输入冻结、GPU resource ID/indirect TLAS、绑定和真实RT dispatch。

仍需设备父对象ARC提前释放闭环、UE新截帧/离线输出/事件往返/EID0/生命周期、
最终哈希下相关集中回归及原生能力判断。未达到启用门槛，目标保持active。

[B483](BATCH483_RAY_UNBOUND_SIZE_AND_UI_LIFETIME.md) 已补无输入/indirect TLAS查询，
10组native一致、官方两scene及坏输入/能力查询通过；实际UE下一失败gpuResourceID。
三族固定32333bb6集中回归308/7784/3080 PASS、growth0bytes、起止哈希一致。顺带修复并独立复现Qt header旧模型回调问题，
原用户crash场景仍待补充，未将独立widget测试当成原崩溃验收。

[B484](BATCH484_RAY_AS_GPU_IDENTITY.md) AS标识getter与资源关联通过六例/738事件、
坏输入及生命周期、官方两scene。实际UE越过getter，compute VFT count>32成为
下一已证实失败；B485补compute表容量，不扩大AS IFT offset或render能力。

[B485](BATCH485_RAY_COMPUTE_VFT_CAPACITY.md) 已补compute VFT容量，33/256/65536
末槽真实ray调用、坏输入和生命周期通过；官方两scene通过，UE复验随后固定库
20份RT与全量串行进行。继续从实际UE下一失败开发，公开能力false。

B485实际UE已越过VFT容量，下一fatal为AS实例构建直接描述guard；实际使用
Private/offset/Indirect实例。80c2d706下20份RT复验2619事件通过，集中全量运行中。
下一实现需按DX12/VK执行点冻结与typed实例AS ID重定位，保留userID/flags/mask/
IFT offset；不得把任意整数替换为地址或只放开descriptor guard。

80c2d706集中旧回归已结束：308/7784/3080、growth0bytes、exit0及工作/冻结库
起止hash一致；RT20captures/2619事件也PASS。此证据不覆盖UE RT/ARC/UI原crash。

[B486](BATCH486_RAY_ENCODER_INPUT_SNAPSHOTS.md) 已补Tracked独立Private多indexed输入执行点冻结，同CB上传/覆盖四例与Shared拒绝边界、408事件/120坏输入及生命周期通过。官方两scene PASS；a2e87831固定库24份RT/3027事件及集中308/7784/3080 PASS，growth0/hash一致。间接实例原生userID73基线通过，capture仍被guard拒绝；继续typed实例语义与GPU ID重定位，公开能力false。

[B487](BATCH487_RAY_USER_ID_INSTANCES.md) 直接UserID实例及偏移/填充步长、schema6初态已通过七例/732事件、284坏输入与80生命周期，保留完整uint32编号。官方两scene通过，26303b3c库官方/旧RT/Managed与schema2定向复验PASS，本库全量/UE未跑。下一步Shared间接实例typed关联，然后Private/GPU实例与UE复验。

[B488](BATCH488_RAY_TYPED_INDIRECT_INSTANCES.md) Shared间接实例typed关联、原始72-byte输入与schema7初态、重放UserID/live child规范化已通过七例/732事件、318坏输入与80生命周期。Private/GPU实例仍拒绝；c924730b官方/38份RT4491事件/集中308/7784/3080 PASS，growth4653056bytes/hash一致。下一Private执行点输入冻结后再复验UE。

[B489](BATCH489_RAY_PRIVATE_INDIRECT_SNAPSHOTS.md) 扩AS encoder-end输入冻结到帧前Tracked独立Private间接实例，完成typed候选筛选与版本依赖。八例/816事件、368坏输入、90生命周期（growth327680bytes）、官方两scene/41坏sample/6查询PASS。349df416实际UE新诊断FAIL于间接实例输入guard（session20261005-203243），下一补精确诊断；本库集中全量未跑，帧内Private/heap/untracked/大候选集仍需证据，能力false。

[B490](BATCH490_RAY_PLACEMENT_INSTANCE_INPUTS.md) 根据真实UE输入扩kind9到帧前Tracked placement heap，区分birth aliasability与显式退休并检查scratch/AS写入范围。七例/714事件/322坏/80生命周期PASS，distinct alias同CB/后来CB覆盖保持编号；0495cfde官方/53份旧新RT6021事件PASS，UE越过heap guard后FAIL于候选检查，本库集中308/7784/3080 PASS，growth13074432bytes/hash一致，能力false。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B491](BATCH491_RAY_MANY_INDIRECT_CHILDREN.md)：间接TLAS的typed子AS预算1024，直接仍4；修复描述工具固定4项栈数组越界。backend/bundle e6f18f11，GUI3ba30e36。5/33/frame33/Private33/placement alias128五例native/capture/API588事件/CLI、226坏输入、20旧检查/T12416及6×10生命周期PASS，growth327680bytes/hash一致；实际最多128子已验，不把1024预算称为GPU验收。官方两scene/10查询PASS；41坏sample/6能力查询、UE、58份RT及本库集中串行运行中。f991栈溢出与测试错误判定失败均保留，不计验收。生产能力false，ARC/原Qt crash/GUI未闭环，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B491](BATCH491_RAY_MANY_INDIRECT_CHILDREN.md)：间接TLAS typed子AS预算1024（GPU实际最多128已验），直接仍4；固定native数组越界已修。backend/bundle e6f18f11，GUI3ba30e36。五例native/capture/API588事件/CLI、226坏输入、20旧检查/T12416及60生命周期PASS（growth327680bytes）；官方两scene/10查询/41坏sample/6能力查询、58份RT6609事件、身份旧反例/schema2和集中308/7784/3080 PASS，集中growth0bytes/exit0/起止hash一致。UE session20261005-215022已越过多候选guard，下一FAIL为Private placement间接count=0；B492源码已编辑，等待native基线及新构建验证，不计本证据。ARC/原Qt crash/GUI未闭环，两能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B492](BATCH492_RAY_EMPTY_INDIRECT_TLAS.md)：补Indirect零count尺寸/heap查询及kind10/schema8空TLAS配方，不读输入、不猜子AS，严格offset<length。backend/bundle ae81e24a，GUI3ba30e36。五例Shared帧前/帧内、Private/placement帧前及同CB alias：native/capture尺寸一致、ray miss0/0/0/0、API588事件/EID0/CLI、169坏输入、20旧检查/T12416反例与60生命周期PASS（growth589824bytes/hash一致）。官方/UE/63份RT接续中，本库集中未跑，e6固定集中308/7784/3080/growth0 PASS为旧库。ARC/原Qt crash/GUI未闭环，两能力false，持续推进、未提交/推送。

B492 ae81e24a接续结果：官方两scene/10尺寸查询/41坏sample/6能力查询、63份B482–492 RT7197事件、AS身份旧反例/schema2/旧Indirect46反例PASS，冻结/工作库起止hash一致。UE session20261005-222357已越过zero count，下一FAIL为private间接primitive候选count0（limit1024），instanceCount尚需补诊断，不能推定为null AS。当前库集中未跑；B491 e6集中308/7784/3080/growth0 PASS仅旧库。下一Native inactive instance探针及UE精确候选/参数诊断；生产能力false。

2026-10-05 B493定向验收：全禁用Indirect槽位kind11/schema9，仅接受每packet ASID0/mask0，完整transform/flags/IFT offset/userID冻结。最终backend/bundle 08028e4c7e2048c3ac37d0de00d712de139c3026b4960d8c836d4cc12f3b255f，GUI3ba30e36。五例Shared帧前/帧内、Private/placement帧前、同CB distinct alias擦零：native/capture/API588事件/EID0/CLI及完整72-byte snapshot oracle PASS；4×43+36=208坏输入、20旧检查/T12416反例、6×10生命周期PASS（growth425984bytes/hash一致）。产物captures/metal-ray-b493、build-macos-debug/metal-ray-b493/indirect-manifest.json。官方/UE/68份RT与本库集中接续中；ARC/原Qt crash/GUI/UE RT未闭环，公开能力false，未提交/推送。

B493 08028e4c接续验收：官方两scene/10尺寸查询/41坏sample/6能力查询、68份B482–493 RT7785事件、AS身份旧反例/schema2/旧Indirect与空TLAS反例PASS；固定库集中308/7784/3080 PASS，growth0bytes/exit0/工作和冻结库起止hash一致。产物frozen-validation-08028e4c/full-regression.log与metal-ray-b493/followup-manifest.json。UE session20261005-225938进入截帧后Private/Tracked placement、54实例、background0被bridge主动ForceCrash拒绝；仍未证明RT dispatch/输出/离线。B494已编辑，待串行构建/GPU，不能计此证据；生产能力false。

2026-10-05 B494定向验收：帧内Private/Tracked（独立或placement）间接实例输入按AS encoder消费点冻结，提交完成后保持原API chunk位置/metadata写入；重放typed live child/UserID staging或全禁用Indirect。精确范围别名仅在已验证冻结build、native encoder关闭且无descriptor backing冲突时允许，EID0清空资格。最终backend/bundle 111c4787c2a7fddf59bdd7503e2ea89f8227c23d72962f584942cb7e2e65a14d，GUI3ba30e36。七例native/capture/API954事件/EID0/CLI3 PASS，含同TLAS两build原始userID73/74、两种inactive完整bytes oracle及两份合法CPU别名提前创建控制；5×43+2×37+6=295坏输入/alias反例、20旧检查/T12416及8×10生命周期PASS（growth1081344bytes/起止hash一致）。产物captures/metal-ray-b494、metal-ray-b494/indirect-manifest.json。两次实现/判定FAIL日志保留，不计通过。官方/UE/75份RT和本库集中尚未跑；08028e4c集中308/7784/3080/growth0属于旧库。公开能力false，ARC/原Qt crash/UE RT未闭环，未提交/推送。

2026-10-06 [B495](BATCH495_UE_RT_FREEZE_DIAGNOSTICS.md)：系统重启取证与UE诊断监督。23:50:44启动的111c4787 UE session在frame127停滞；WindowServer连续40s未checkin，UE线程等待高CPU的MTLCompilerService，forceReset为btn_rst/force_off。未证实GPU内核panic或具体根因。新增12s帧进展检测（同帧后台日志不续期）、1s栈取证/2s采样上限、owned group清理及outer timeout收尾，原子状态记录；修复RT隔离启动INI沿用1728×1020最大化窗口，固定640×480且原设置hash不变。18项CPU失败注入/INI测试、preflight/语法/diff PASS；重启后未启动UE/GPU、未改后端二进制。B494官方2scene/10查询/41坏sample/6能力查询已PASS；UE截帧未完成，75份RT/本库集中未运行，manifest已纠正INTERRUPTED。backend/bundle111c4787c2a7fddf59bdd7503e2ea89f8227c23d72962f584942cb7e2e65a14d，GUI3ba30e36。公开能力false。接续先CPU定位待编译管线/RT ABI及补取证；不要直接重启旧run_followup.py、全量或长UE运行。监督器不能保证阻止系统级死锁，根因未修复，持续任务未完成。


2026-10-06 [B496](BATCH496_NATIVE_COMPUTE_COMPILE_TRACES.md)：新增仅capture且精确env=1的native compute编译取证，覆盖function/function-options/descriptor三类同步及三类异步。begin/submitted/native end按process/token关联，记录线程、函数名/label bytes、linked表数量、options/stackDepth、native结果与耗时；async可识别调用未返回/等待callback，不改变native调用次数、ABI、回放格式或支持范围。监督器退出时保存compile-trace.json；9项协议审计+20项监督/INI CPU测试PASS，串行renderdoccmd/app构建及preflight/语法/diff PASS。最终backend/bundle acd0c0d697b41c83390419b972e0a508358226371f4ced65e111123519369c14，GUI3ba30e36；111c4787已保留baseline。旧UE日志没有新trace，结果NOT OBSERVED，不能定位旧待编译函数。未运行GPU/UE/sample/75 RT/集中，新库不能继承111或08028旧库验收。两能力false；死机根因未修，先CPU推进UE IR/函数表typed ABI与编译证据，不直接重跑旧长UE/全量。持续任务未完成、未提交/推送。


2026-10-06 [B497](BATCH497_FUNCTION_TABLE_GPU_IDENTITY.md)：按DX12 shader identifier关联/VK group handle原生转发补VFT/IFT gpuResourceID及typed依赖，查询未绑定表也捕获；没有实现UE raw IR packet重定位。最终backend/bundle eea2539fb069dae9d59d2c32ad962c562ee83aecb09d57be195e50538f123c57，GUI3ba30e36。四例native/capture/API714次事件/EID0/CLI PASS，77坏capture/8合法副本、3份旧RT408事件、18份旧函数表targeted、50生命周期(growth589824bytes) PASS；官方2scene/270事件/10尺寸/41坏sample/6能力查询PASS。四次真实descriptor-sync取证完整，新增系统native thread ID和日志shared-lock保留；10+21项CPU、preflight/语法/diff PASS。早期编译/日志/测试oracle失败均保留。UE/75 RT/集中/原Qt/ARC未验，死机根因未修，两能力false。下一UE IR typed ABI及六入口有限诊断，继续禁止直接旧长UE/full followup；未提交/推送，持续任务未完成。


2026-10-06 [B498](BATCH498_SIX_NATIVE_RAY_COMPILE_APIS.md)：六种sync/async compute创建入口均用真实opaque triangle ray query验证1/0；exact1 trace有6begin/6end/3submitted、callback native线程可关联，unset/0/true/空关闭。enabled capture32事件×3/EID0/PSO+AS绑定、23坏creation、19旧targeted、30生命周期(growth589824bytes) PASS。修正生命周期检查漏认帧前AS无IFT inline ray；失败日志保留。UE IR实际header CPU布局152/104/32bytes，记录完整SBT/GRS/函数表/嵌套record依赖，未实现IR重定位。backend/bundle未改仍eea2539f，GUI3ba30e36，B497同hash官方证据有效；本批UE/75 RT/集中/原Qt/ARC未跑，死机根因未修，能力false。下一完整typed IR声明与重定位，禁止直接旧长UE/full；未提交/推送。


2026-10-06 [B499](BATCH499_TYPED_RAY_ARGUMENT_RESOURCES.md)：补MTLArgumentEncoder AS/IFT typed编码和AS/IFT/VFT初态重新编码，compute调度前验证成员、已构建AS种类与函数表PSO，补CS usage。六例Shared/Managed、function/device、标量/函数表数组native/capture/API396事件/EID0/CLI PASS，120坏输入及2帧内明确拒绝PASS；18旧targeted/41旧反例/70生命周期(growth409600bytes) PASS。新backend/bundle03fc7b32、GUI3ba30e36；当前库官方2scene/270事件/10查询/41坏sample/6能力查询PASS。UE raw IR/SBT尚未实现，instance参数GPU/嵌套/render/帧内重编码未验或不支持；UE/75 RT/集中/原Qt/ARC未跑，死机根因未修，能力false。下一typed TLAS packet和缺失/错误AS种类反例→完整UE IR声明重定位，禁止直接旧长UE/full。未提交/推送，持续任务未完成。


2026-10-06 [B500](BATCH500_TLAS_PACKETS_AND_CONVERTED_DXR_SAMPLE.md)：补TLAS参数GPU路径；12例BLAS/TLAS native/capture/API792事件/EID0/CLI、294坏输入、4帧内明确拒绝、18旧targeted/41旧反例/130生命周期(growth327680bytes) PASS。新增UE5.8.3 DXC→Apple IR真实TraceRay样例，RaygenIndirection+VFT/IFT、152-byte参数/32-byteSBT ID/64-byteAS header，native与精确probe capture命中73/未命中11；CPU完整ABI/来源及1次native编译审计PASS。IR离线明确拒绝无声明raw地址，未修重定位、不计replay成功。backend/bundle仍03fc7b32、GUI3ba30e36，B499同hash官方证据有效，本批未重跑。UE/75 RT/集中/原Qt/ARC未验，死机根因未修、能力false。下一直接用converted fixture补完整typed IR packet/SBT/GRS/AS header声明重定位，禁止绕过guard/直接旧长UE/full。失败日志保留，未提交/推送，目标active。



B501已进入[PHASE59](PHASE59.md)：完整IR声明、参数重定位与TLAS residency初态依赖首次闭环，最终10c28bf1的真实转换73/11与相关验收通过。UE布局扩展继续，生产能力false，详见[B501](BATCH501_TYPED_IR_RAY_DISPATCH.md)。
