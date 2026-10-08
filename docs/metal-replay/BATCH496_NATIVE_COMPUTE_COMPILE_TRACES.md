# B496：原生 compute 管线编译取证

2026-10-06。承接B495系统死机取证：watchdog只能证明UE线程等待Metal编译服务，不能给出具体待编译函数，本批填补诊断信息。没有增加RT调试内部能力或扩大图形接口支持范围。

## 参照及边界

仓库DX12 `d3d12_device_wrap5.cpp` 的CreateStateObject在native创建后包装/记录；Vulkan `vk_shader_funcs.cpp` 的vkCreateRayTracingPipelinesKHR保留native结果，当前deferred operation不包装并以NULL调用原生。沿用native创建/包装的边界：本批仅记录原有compute native调用开始与返回，不强制异步变同步、不添加第二次编译、不改变描述、回调顺序或成功/失败语义。Metal异步回调的诊断token为无资源所有权的POD，未新增device/function/table保活。ARC未因此解决。

## 修改

`WrappedMTLDevice::BeginComputeCompileTrace`只在capture mode且`RENDERDOC_METAL_PIPELINE_COMPILE_TRACE=1`精确匹配时生成记录；生产能力查询及replay不受影响。RT隔离runner请求此开关，普通运行默认关闭。

覆盖六入口：function、function+options、descriptor各自的同步与异步创建。每个native调用前begin；同步返回或异步callback第一行end（先于wrapper记录与用户回调）；异步提交调用返回后submitted。即便native同步触发callback、end先于submitted，也按合法顺序记录。process/token唯一关联，线程ID可映射sample栈；记录function name、descriptor label的最多1024 UTF-8 bytes、options、普通/binary/private linked functions数量、maxCallStackDepth、native success和elapsed_ms。字符串逐字节JSON转义避免控制字符断行；名称截断时审计UTF-8按replacement显示。Timing频率是ticks/ms，elapsed不额外乘1000。

`metal_pipeline_compile_audit.py`区分native同步未返回、native提交尚未返回和已提交等待callback；没有记录是NOT OBSERVED，损坏/重复/无begin结果是INVALID TRACE，均不把记录完整当成RT调度或像素验收。不同process相同token分开；出错仍保留此前未完成调用。监督程序退出时从renderdoc.log流式读记录并写compile-trace.json，supervisor.json引用pending/completed数量；损坏诊断不能遮蔽owned group清理。

## 实际验证

- 串行构建renderdoccmd与qrenderdoc app，exit0；日志`build-macos-debug/metal-ray-b496/build.log`。Qt SDK26兼容警告保留，与本批代码错误不同。
- 协议审计9项CPU测试PASS：sync成功/失败、线程交错、callback早于submit返回、三种pending、跨process重号、控制/Unicode bytes、截断UTF-8、无trace和重复/损坏记录。
- 监督/INI 20项CPU测试PASS：B495原18项复验，另两项验证异常退出保存未完成函数/线程、损坏compile trace仍清理owned进程。仅使用模拟Python进程，不调用Metal。
- Python/shell语法、preflight和git diff --check PASS。旧UE81276 renderdoc.log离线审计为NOT OBSERVED：旧111库没有这些记录，无法逆推出函数名，不能把新协议模拟当作旧死机根因证明。

产物：`compile-audit-tests.log`、`supervisor-tests.log`、`preflight.log`、`prior-session-compile-trace.json`、`binary-hashes.json`和`manifest.json`，均位于忽略的`build-macos-debug/metal-ray-b496/`。

最终backend/bundle SHA256：acd0c0d697b41c83390419b972e0a508358226371f4ced65e111123519369c14。
GUI SHA256：3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。
上个111库已复制到`metal-ray-b496/baseline/lib/librenderdoc.dylib`并核验hash。保存最终工具/source hash于manifest。

**未运行**新库GPU、官方sample、UE重截/实际trace、75份RT、集中回归、GUI和ARC验收。旧111 sample、B494定向与08028全量仍只属于各自旧库。此次没有失败的编译/CPU测试，不重跑已成功无变化检查。

## 接续

先审查UE IR dispatch的typed GPU地址/函数表gpuResourceID与仓库VK/DX12对应关联；可推进独立CPU实现/测试与诊断。长UE/全量仍不能直接复用旧followup。新trace尚未在真实native compile验证，下一有限GPU诊断必须明确验证入口/输出和停止条件，不能凭本批构建或模拟成功声称根因修复。生产supportsRaytracing/FromRender继续false，持续目标未达标，不提交/推送。

B497更正：旧thread字段为pthread指针，并非sample numeric线程号；新增native_thread，四个真实descriptor-sync begin/end与非零native ID一致、耗时0.364–0.470ms。另用parent shared logfile lock解决正常退出日志被unlink；具体见[B497](BATCH497_FUNCTION_TABLE_GPU_IDENTITY.md)。其余五入口真实调用仍未验，不能用本批证明旧UE死机根因。
