# B508：初始资源列表解码与 owner device 关闭

接续 PHASE59/B507 的关闭诊断。补齐 CPU 结构导出和真实 replay device 的关闭所有权，以及初始资源列表解码；生产 compute/render RT 仍 false，实际 UE 本批未运行。所有构建、native/capture/replay 步骤串行，未提交或推送。

## 实现与依据

`WrappedMTLDevice` constructor 把 owner `this` 注册到资源管理器，destructor 又负责销毁该 manager 并最后释放 native device。过去 manager Shutdown 把 owner 当 child 送进 `ResourceTypeRelease`，产生 eResDevice/type4 错误；structured-export 状态又未调用 Shutdown，留下资源表断言。现在在 replay 或 structured-export 状态先 `ReleaseResource(GetResID(this))` 解除 owner 注册，再 Shutdown child resources，最后按原所有权释放 manager/native device。没有把 eResDevice 错误分支改成静默成功，也没有递归删除 owner。

先查 DX12 `d3d12_device.cpp:5375`、Vulkan `vk_core.cpp:5095` 的 InitialContentsList 分支及公共 `resource_manager.h:1322` 的 `NeededInitials`/`WrittenRecord` 格式。Metal 现在解码同一 `rdcarray<ResourceManagerInternal::WrittenRecord>`，保留资源 ID 与 written 标记，检查读取错误。已有 typed buffer/texture/AS 初态仍独立注册和恢复。Metal generic `Create_InitialState` 尚未实现，不能为了消除日志调用它；本批没有承诺补齐通用默认初态语义。

## 关闭和元数据证据

B507 库保存在 `build-macos-debug/metal-ray-b508/baseline/librenderdoc-b507.dylib`。原 CPU 导出在 LLDB 中暴露 InitialContentsList 未处理及 `m_ResourceMap.empty()` 断言，日志 `baseline/owner-release-lldb.log`、`baseline/owner-release-lldb-noinit.log` 保留。前者还受用户 `.lldbinit` 的 UE formatter 路径和默认 SIGTRAP 停止影响；第二次关闭该初始化后继续到正常退出，同时保留两项真实诊断。

修复后的 `export-cleanup-lldb.log` 和 `replay-cleanup-lldb.log` 在相同 eResDevice release 错误行设断点，CPU 导出与真实 Private 间接 TLAS 重放均正常 exit0，断点没有命中，无旧列表/资源表/device诊断。CPU 是 dummy 路径，native 是实际 Metal 设备路径，两者分别验证。

新增 `metal_initial_list_close_gate.py` 完成四次原 capture CPU 导出、一次重新导入后的导出、完整 API 输出/三方向事件/EID0 与 CLI3loops。9项初始资源 ID 和 written 标记逐项保持一致（7true、2false）。把数组计数改为 uint64 最大值，验证读取器在分配前拒绝，CLI正常exit1，不把 signal/timeout 当通过。

新增 `metal_ray_failed_close_smoke.mm` 在同一进程每轮先打开三份合法容器但无法 replay 的 capture（超大初始列表、未知 AS kind、未知 AS header identity），再 CPU 导出并打开正常 Private 间接 TLAS；10轮共30失败/10成功/10CPU结构导出/30次EID0→dispatch→EID0。每次命中输出146/11，重置7/7，resident growth1081344bytes、exit0。原 lifecycle smoke 的普通t35、global+local+heap Private indirect及官方两scene 4×10重开，growth409600bytes、exit0。仅证明这些有限成功/失败关闭场景；不把它当作任意 ARC 提前释放、UE 或无界运行验收。

两个新增入口共用 IR 测试锁。本机路径以 Python tempfile/TMPDIR 为准，实际是 `/var/folders/zx/9sny0k7d6jvg6wrgcnjjpbqm0000gn/T/renderdoc-metal-ir-gpu-tests.lock`；C++ helper 遵循 TMPDIR/TEMP/TMP 目录选择。CPU 持锁时 Python --help 和无参数 C++ helper 都阻塞，释放后分别exit0/1，没有创建 replay/device 或运行GPU。见两个 lock-check.json。

最终审计2326份有效日志，检查 `Unexpected Metal resource type`、`Assertion failed`、`m_ResourceMap.empty` 与旧 InitialContentsList 未处理标记，没有命中。明确排除下列保留的测试构造失败和修正前反例，不能把排除范围计成通过。

## 当前固定库回归

backend 与 bundle SHA256：`5f4db1c727c0bde74dcf73aa48a8de9f1176dc1175c52ce77134b6151517d66b`。GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。cmd/app先后构建，所有有效结果使用相同库，起止hash一致。

- 51个真实转换 TraceRay 场景 native/capture/ABI/API3147次事件选择/三方向/EID0/CLI每份3loops通过。覆盖 B507 的 UserID、Private/GPU 构建输入清零、空/屏蔽 TLAS，及 direct/heap AS、全局/局部根、texture/sampler、any-hit/null shader 等既有范围。
- 706坏IR正常拒绝/53合法控制；间接五组156/14在修正测试构造后重新运行，其余550/39不重复跑、不重复计数。另1坏初始列表与1合法完整往返控制独立记录。
- 18旧targeted API/CLI、41旧argument反例、6旧B500 packet、旧B501/B502 IR、无声明raw IR预期拒绝、6能力查询通过。
- 官方Apple MIT两个scene：native重复/capture/逐字节输出/API270事件/CLI各3loops、10尺寸查询、13坏官方capture通过。固定ZIP SHA256 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`；41是旧argument数量，不能混入官方反例。
- 有限正常/失败生命周期、两项CPU互斥检查、Python/Bash语法与diff检查通过。未运行整个包装器；上述实际子步骤已整合到包装器。

产物：`captures/metal-ray-b508/final/<mode>/ir-runtime_capture.rdc`；`build-macos-debug/metal-ray-b508/final/manifest.json`、`validated-modes.json`、`remaining-manifest.json`、`initial-list-close/manifest.json`、`shutdown-log-audit.json`、`indirect-fixture-rerun.json`、官方`apple-sample/gate-results/`、两份生命周期日志、两份CPU锁检查与`source-binary-hashes.json`。LLDB与构建证据在B508根目录。

## 保留的测试失败与修正

新gate首次把 RenderDoc debug日志和重定向日志指向同一个未加锁文件，关闭时文件被 logger 删除，脚本读日志失败。补与现有反例脚本一致的 shared logfile lock；失败记录 `initial-list-close-first-script-failure/`，不是后端捕获失败。

第二次尝试通过零长度 chunk 构造缺失列表，在通用 structured writer 中触发“Beginning a chunk inside another chunk”断言；未把此产物送入 native replay、未计正常拒绝。保留 `initial-list-close-payload-construction-failure/`，换成完整8-byte最大数组计数构造，只检验当前列表读取器。通用零长度写入路径没有在本批修改。

最终日志审计发现 B507 就存在的 `empty-has-vertices` 反例序列化断言：把空实例 buffer 增至72字节却沿用原272-byte chunk估计长度，实际读取305字节导致错位和chunk overrun，原 CLI虽exit1但没有隔离 typed recipe 校验。修正 `replace_vertices` 为增长与 buffer alignment 留足chunk空间；5组156/14全部重新运行，空TLAS非空实例现在被AS初态语义拒绝，无序列化断言。旧五组完整保留 `indirect-invalid-before-serialization-fixture-fix/`。本批审计不能忽略或把旧断言归成正常语义拒绝。

## 下一项与未运行范围

IR仍是静态 ABI 契约，所有帧内AS encoder操作在preflight提前拒绝。下一批实现按事件更新的 AS 当前配方、build输入验证与EID0恢复，再用GPU/Private帧内重建→真实TraceRay证明；不能简单移除guard或拿初态代替当前状态。

仍需可靠UE producer shader/header/library/export/function/PSO与typed descriptor factory关联、heap CBV/UAV/typed view、nested UB、大heap/工作量、Private/GPU IR与动态heap、必要SBT/callable/render RT。显式fixture注解不代表生产UE支持。AS内部查看、RT shader单步和RT Pixel History不在承诺内。

实际UE RT/离线、75旧RT全覆盖、集中308、原Qt sizeHint复现、ARC任意提前释放与独立旧descriptor-table十反例未运行，不能继承旧库PASS。用户系统冻结仍只有既有WindowServer watchdog/强制重启与UE compiler等待证据，具体根因未修；不重跑旧长UE/full followup。用户UE AS栈崩溃不因本批关闭修复被宣称解决。能力保持false，持续目标active。
