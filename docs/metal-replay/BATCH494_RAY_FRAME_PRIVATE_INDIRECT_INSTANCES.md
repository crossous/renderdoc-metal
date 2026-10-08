# B494：帧内 Private 间接实例输入

B494开发检查点：08028e4c实际UE session20261005-225938越过零候选并初始化，截帧54-instance Private/Tracked placement输入在background=0被bridge拒绝。新增帧内encoder-end快照、保持原chunk顺序/API metadata的延后序列化、typed snapshot重放，六场景脚本已编辑，未构建/未GPU验收；B493集中仍串行运行，不重叠构建。公开能力false，UE RT输出/离线未证明。

参照Vulkan CopyInputBuffers与DX12 CopyBuildInputs在构建执行位置冻结输入/typed地址重定位。Metal使用AS encoder结束后的tracked blit；仅源输入与该encoder scratch/AS输出物理范围不重叠时有效。保留逐build snapshot、原API chunk位置及时间/线程/调用栈，提交完成后写入完整packet。重放用已验证typed child的live handles建UserID staging，全禁用packet保留原Indirect语义；不依赖不可读原Private字节，不扫描任意地址。

补七个fixture：独立Private帧内、同CB上传/高userID/重复三实例、placement帧内、distinct alias擦零、两种全禁用帧内，以及同一TLAS两次build分别ray userID73/74与两个原始packet oracle。另补payload完全缺失拒绝。语法/diff检查PASS，尚未编译/GPU，不计验收。

首候选870a909f构建PASS，前两个frame Private场景native/capture/事件/CLI PASS。repeated/high反例children-limit已被序列化器干净拒绝，但错误为Reading invalid array or byte buffer ... larger than total stream size，脚本仅识别Reading off the end，故测试FAIL。仅children-limit新增精确bounded corruption消息识别；原日志/manifest保留before-serializer-bound-message-fix。其它反例仍必须Metal特定拒绝。新候选整体尚未验收。

第二FAIL：870a909f原生alias覆盖正确，离线MTLHeap::newBuffer(offset)因普通capture无descriptor coverage而拒绝重叠。保留before-frame-alias-proof-fix。新补仅已验证frame Private AS独立packet staging之后、AS encoder已结束、同Private/Tracked heap精确范围重叠的别名；不退休旧buffer，不绕过descriptor backing ABI，不扩大任意partial overlap；每次AS初态恢复/EID0清空资格。等待新构建/GPU及负例。

111c4787已越过heap alias拒绝；API gate仍FAIL于EID1源byte oracle，诊断为初始Private未初始化字节非零。旧BG擦零oracle误用于帧内：改为EID0实际初始bytes、GPU上传后的Shared源完整bytes、fill执行后全零三阶段，涵盖CPU物理位置和GPU事件前缀。Backend未改，旧FAIL保留before-frame-source-oracle-fix及source-oracle-diagnostic*.log。

别名负例审查：before-build拒绝；before-recorded-EndEncoding合法（CPU创建不访问range，已验证build有独立staging，replay在CPU边界关闭native encoder）。此变体改为API全事件及CLI3-loop合法控制，不计坏输入。另验证无build与partial-overlap拒绝；每份alias capture三坏例+一合法事件控制。原断言FAIL日志保留，不扩大未知packet/descriptor backing或build之前的别名资格。

2026-10-05 B494定向验收：帧内Private/Tracked（独立或placement）间接实例输入按AS encoder消费点冻结，提交完成后保持原API chunk位置/metadata写入；重放typed live child/UserID staging或全禁用Indirect。精确范围别名仅在已验证冻结build、native encoder关闭且无descriptor backing冲突时允许，EID0清空资格。最终backend/bundle 111c4787c2a7fddf59bdd7503e2ea89f8227c23d72962f584942cb7e2e65a14d，GUI3ba30e36。七例native/capture/API954事件/EID0/CLI3 PASS，含同TLAS两build原始userID73/74、两种inactive完整bytes oracle及两份合法CPU别名提前创建控制；5×43+2×37+6=295坏输入/alias反例、20旧检查/T12416及8×10生命周期PASS（growth1081344bytes/起止hash一致）。产物captures/metal-ray-b494、metal-ray-b494/indirect-manifest.json。两次实现/判定FAIL日志保留，不计通过。官方/UE/75份RT和本库集中尚未跑；08028e4c集中308/7784/3080/growth0属于旧库。公开能力false，ARC/原Qt crash/UE RT未闭环，未提交/推送。

2026-10-06 [B495](BATCH495_UE_RT_FREEZE_DIAGNOSTICS.md)：系统重启取证与UE诊断监督。23:50:44启动的111c4787 UE session在frame127停滞；WindowServer连续40s未checkin，UE线程等待高CPU的MTLCompilerService，forceReset为btn_rst/force_off。未证实GPU内核panic或具体根因。新增12s帧进展检测（同帧后台日志不续期）、1s栈取证/2s采样上限、owned group清理及outer timeout收尾，原子状态记录；修复RT隔离启动INI沿用1728×1020最大化窗口，固定640×480且原设置hash不变。18项CPU失败注入/INI测试、preflight/语法/diff PASS；重启后未启动UE/GPU、未改后端二进制。B494官方2scene/10查询/41坏sample/6能力查询已PASS；UE截帧未完成，75份RT/本库集中未运行，manifest已纠正INTERRUPTED。backend/bundle111c4787c2a7fddf59bdd7503e2ea89f8227c23d72962f584942cb7e2e65a14d，GUI3ba30e36。公开能力false。接续先CPU定位待编译管线/RT ABI及补取证；不要直接重启旧run_followup.py、全量或长UE运行。监督器不能保证阻止系统级死锁，根因未修复，持续任务未完成。
