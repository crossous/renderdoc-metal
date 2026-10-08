# B495：UE 光追诊断死机取证与测试监督

2026-10-06。用户报告主机死机并强制重启。本批补诊断测试的边界与取证，未修改Metal捕获格式、RT支持范围或能力开关，未宣称死机根因已修复。

## 已确认的日志

| 时间（本地 UTC+8） | 实际证据 |
| --- | --- |
| 10-05 23:50:44 | 串行接续中的UE进程81276启动，session20261005-235044；库111c4787 |
| 23:51:04 | UE初始化完成；此后provider日志帧号正常前进 |
| 23:51:08—23:52:40 | MTLCompilerService81395替UE编译，90 CPU秒/92墙钟秒，约占一个核心98%；AGX/LLVM优化和寄存器分配栈仍执行 |
| 23:51:50 | UE最后观察到前进的帧号127；后续音频/网络/DDC日志继续出现，未生成新捕获 |
| 23:52:25—23:52:31 | 第一份WindowServer watchdog采样，报告已经40s没有成功checkin；随后两份报告继续显示停滞 |
| 23:54:40 | 本机重启；forceReset报告panic原因btn_rst，AppleM68Buttons栈，ResetCounter显示force_off |

第一份watchdog中，UE Background Worker #3的Metal同步调用等待UE work queue，该队列通过XPC等待MTLCompilerService81395/thread0x1154573。编译服务有实际CPU进展，不能据此认定编译器死锁。WindowServer自身栈截断且无直接链接到这条等待链；相关性明确，导致全机停滞的具体调用/驱动原因尚未证实。forceReset是强制复位记录，不能当成GPU kernel panic。所查日志未发现GPU reset/timeout或OOM判定；16GB设备上的UE约7.9GB、编译服务约1.28GB占用仅作为负载证据。

原始系统报告复制到忽略的`build-macos-debug/metal-ray-b495/`，每份来源、大小与SHA256记录于`system-diagnostics-manifest.json`。另外提取WindowServer/UE/MTLCompiler/ANECompiler进程栈。内核与watchdog查询输出保留`metal-ray-b494/reboot-kernel-watchdog.log`。未修改系统配置或结束系统编译服务/WindowServer。

## 实际修改

- 将原shell内编辑器监管提取为`util/ue/ue_metal_capture_supervisor.py`，shell以exec调用监督程序，使外层TERM可进入编辑器收尾。只操作自己创建的编辑器进程组。
- RT runner初始化后12s无UE帧号进展就终止诊断；同帧号后台日志不能延长时限。自动捕获总窗口为初始化后45+20s。保留通用手动入口旧行为：默认没有帧停滞限制，手动捕获成功也不自动关闭。
- 异常终止前对该编辑器采样1s，采样最多2s；TERM宽限2s后KILL，即使group leader先退出也清理子进程。外层400s超时发送TERM并给监督器6s收尾，避免旧subprocess.run只结束shell留下独立编辑器group。RT与已有Testproj光栅capture runner共用此收尾方式；通用手动入口和光栅默认监视时限不变。
- 原子写入`supervisor.json`，包括拥有者/编辑器PID、UTC时间、最后帧号、限制、退出原因；RT manifest启动前写RUNNING、最后记录监督结果。断电仍可能留RUNNING，需要用boot/session证据标记中断，不能标PASS。
- RT runner之前仅覆盖LoadLevelAtStartup，遗漏已有光栅runner使用的RootWindow启动限制。用户原INI实际为1728×1020/InitiallyMaximized=True。独立INI现在固定640×480/False，同时保留其它key并记录原设置SHA256。实际启动尺寸和负载改善仍需后续UE验证。

此次不扩大图形接口。B494 AS输入冻结继续参照VK `VulkanAccelerationStructureManager::CopyInputBuffers`（vk_acceleration_structure.cpp）与DX12 `D3D12RTManager::CopyBuildInputs`（d3d12_manager.cpp）的执行位置复制与typed关联。对本次编译等待，审查Metal同步/异步compute descriptor路径：捕获调用一次native编译，后续仅包装/记录；未发现由本次捕获额外重复编译的证据。不能通过跳过编译、改shader或提前开启能力掩盖未验收状态。

## 验证与保留失败

`python3 util/ue/test_ue_metal_capture_supervisor.py`：18项CPU测试PASS，覆盖初始化/分段日志/初始化不续期、后台心跳不能掩盖frame stall、counter wrap、capture-success优先、manual保留、shader error、EPERM真/假退出、startup kill、非零退出与无关进程存活、leader先退出的子进程清理、外层timeout跨group清理、saved收尾、INI幂等与其它key保留。实际运行模拟Python进程，不加载Metal或启动UE。

首次增加descendant测试后一次Darwin退出竞态FAIL：group leader刚退出，killpg(...,0)返回EPERM而不是ESRCH。日志保留`supervisor-before-darwin-exit-race-fix.log`；现在EPERM只有经ps证明无活成员才按退出处理，有活成员仍报错，追加两个失败注入测试。最终18项通过；未把旧失败删除或计PASS。

preflight仅检查项目/库/隔离RHI/provider hash，exit0，实际不会启动编辑器；Python语法、shell语法、git diff --check PASS。`settings-preservation.json`证明本机原设置修改前后hash相同；只写隔离副本。旧日志离线应用新时限，frame127约在23:52:02会达到阈值，记录`prior-session-progress-audit.json`；这不能证明当时监督器可及时调度或避免已经发生的系统停滞。

**本批未运行：** UE重截、任何GPU测试/回放、75份RT复验、111c4787集中、GUI复现、根因修复验证。监督措施是取证/缩短受控运行，不保证OS或驱动死锁能被用户态清理。

## 中断验收更正与二进制

B494定向七例/954事件/295坏输入/80生命周期此前PASS，仍有效；随后的官方两scene/10尺寸查询/41坏sample/6能力查询已在111c4787通过。旧followup manifest原RUNNING已按重启证据改INTERRUPTED，保留四项实际成功检查；UE没有新RT capture，75份RT与全量排在其后，根本未执行。UE diagnostic manifest为重启后重建，明确reconstructed_after_reboot，不冒充终止时正常写入。

backend及bundle：`111c4787c2a7fddf59bdd7503e2ea89f8227c23d72962f584942cb7e2e65a14d`。
GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。
本批未重建二进制，复核结果见`binary-hashes.json`。源码/日志hash及本批结果见`manifest.json`。B493全量308/7784/3080/growth0只属于08028e4c，不能挪作当前库验收。

## 接续

先CPU审查并补充待编译PSO的函数名/参数与调用栈取证，检查函数表gpuResourceID与UE IR dispatch typed ABI缺口；保留VK/DX12支持边界。不要直接运行旧B494 followup或重新发起长UE/全量GPU测试。新的CPU监督已通过，但尚未真实验证，也未解决系统停滞原因；下一次UE诊断需能回答具体编译调用和负载问题，不能重复原条件。能力继续false，UE真实RT dispatch/输出/离线与ARC/UI原crash仍未闭环，不提交/推送，持续目标未完成。
