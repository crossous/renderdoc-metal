# B500：TLAS 参数与转换 DXR 实际 TraceRay 样例

2026-10-06，PHASE58。B499 的 instance-AS 参数成员此前只允许布局，未验证 GPU 路径；本批完成 BLAS/TLAS 成对的真实 native/capture/replay 验收，并新增使用本机 UE 转换器和 IR runtime 的 HLSL TraceRay 小样例。转换样例 native/capture 命中73、未命中11，但离线重定位仍未实现，预期拒绝。**未改产品后端库或生产能力开关**。

## 对照与样例实现

继续参照 VK `vk_descriptor_funcs.cpp::Serialise_vkUpdateDescriptorSets` 的 typed AS descriptor 和 DX12 `d3d12_device_wrap.cpp::CreateShaderResourceView` 的 AS 资源关联；B499 的 native argument encoding 不复刻 Metal handle 布局。本批为其补真实 TLAS 子AS/实例构建及消费证据、未构建AS/缺失成员反例，未增加无测试的支持声明。

转换 DXR 路径按 DX12 `d3d12_command_list4_wrap.cpp::Serialise_DispatchRays` / `D3D12RTManager::PatchRayDispatch` 的明确 SBT/record 依赖、VK `vk_draw_funcs.cpp::Serialise_vkCmdTraceRaysKHR` 的 typed SBT区域审查；不得只修补两个函数表字段便声称完整 IR 支持。

新 MIT fixture `metal_ir_ray_shaders.hlsl` 含 raygen/closest-hit/miss：两条固定 ray，hit payload73、miss11，结果写入 UAV。`metal_ir_ray_convert.cpp` 使用本机 UE 的 DXC(lib_6_3)与 Apple MetalShaderConverter，不复制外部实现，root signature1.1 两个直接 root descriptor（AS header SRV、结果 UAV）。raygen/交叉函数均采用 VisibleFunction 编译模式，与 UE `MetalRayTracing.cpp` 相同；生成五份 metallib，包含原生 `RaygenIndirection` 和 triangle intersection wrapper。

`metal_ir_ray_native.mm` 使用实际 Apache-2.0 runtime 头文件：一个 triangle BLAS、一个 TLAS、VFT slots1/2/3 对应 raygen/miss/closest-hit、slot0空，IFT一个 wrapper；maxCallStackDepth2、linked functions4。固定152-byte dispatch packet、32-byte shader ID、64-byte AS header；GRS两个指针，SBT三个32-byte records，hit stride0合法、callable为空、shader indices1/2/3保留不重定位。以一个32-thread group执行实际 TraceRay。设备与库保留至样例结束；不宣称任意 ARC 父对象提前销毁安全。

## 参数路径实际结果

串行 `test_metal_ray_instance_packet_macos.sh` 调用扩展 packet gate。支持场景为 B499 的六种组合分别采用 BLAS 与 TLAS，共12例：function/device、Shared/Managed、scalar/IFT与VFT数组 setter。所有 native/capture 输出 **3/0/0/9** 一致，Metal validation无错误；API每例22事件×3/EID0，共 **792** 次选择，CLI每例3 loops PASS。参数 offset、AS/IFT/VFT CS usage、显式 null packet、48-byte填充均通过。

坏输入：六份 BLAS 各24（B49920 + 未构建AS1 + 缺少AS/IFT/VFT成员3）；六份 TLAS 各25，再加 BLAS冒充TLAS1。共 **294** PASS。缺成员、未构建AS、错误AS种类及 foreign PSO table 在 dispatch 前拒绝，其他反例失败于具体setter chunk；不接受信号/超时或无关错误作为通过。

四份帧内编码控制（BLAS/TLAS各function、device-managed）native/capture输出正确，离线均明确拒绝 `MTLArgumentEncoder::unsupportedEncoding`，不计成功 replay。旧18份targeted API/CLI、T60的27反例和t126/t127各7反例合计41 PASS。t35 + 新12份capture共 **13×10** 生命周期 PASS，resident growth **327680 bytes**、exit0、backend起止hash一致。

## 转换 IR 样例实际结果

`metal_ir_ray_sample_gate.py` 完整最终运行 PASS native/capture，**replay为预期unsupported**，不是成功离线验收。CPU转换30s、native/capture各20s、离线拒绝30s上限，子进程独立group、超时只终止该group；没有启动UE/Qt，也未重跑旧长UE/full followup。

原生与精确进程probe=1注入捕获均输出 **TraceRay 73/11**，不是普通光栅或仅ray capability查询。生产默认未变。实际 compile trace为COMPLETE：descriptor-sync、RaygenIndirection、1begin/1end、linked4、stack2，native thread238709、耗时0.488ms；无pending/失败。该小管线不证明旧UE编译停滞已修复。

CPU structured export核验1个dispatch、5个buffer GPU identity、1个AS identity、2个函数表identity。新增 `metal_ir_ray_audit.py` 核验真实ZIP raw bytes中的完整 SBT地址/size/stride、dimensions2/1/1、null heaps/callable/额外IFT pointer、GRS到AS header/结果、AS header到TLAS/contributions，以及shader indices与null static sampler。该审计只证明fixture的ABI和来源，**不给任何 replay coverage 权限**。AS与IFT的GPU ID实际都为2，表明必须按类型/字段关联，不能按任意整数扫描猜资源。

离线以正常正退出码明确失败：`Metal capture uses raw GPU addresses/resource IDs; descriptor relocation is unsupported without explicit table layouts and coverage. CPU XML export remains available.` 没有绕过guard、没有把失败记为成功query replay。下一批可直接用 `ir-runtime_capture.rdc` / 可重建fixture驱动完整声明与重定位，覆盖SBT/GRS/AS header，而不是再次只盘点UE布局。

## 来源、失败与产物

外部工具/头文件来自 `/Users/Shared/Epic Games/UE_5.8/Engine`，具体版本、dylib/头文件及自产HLSL/DXIL/metallib/helper/capture SHA在 `ir-native/manifest.json`。Apple runtime/converter头文件注明Apache-2.0；DXC头文件注明UIUC许可。本批没有重新下载第三方包或改上游文件。Apple官方MIT sample的B499当前03fc库验收仍有效，**本批没有重复运行官方sample**，不能把自编fixture称作官方sample。

- packet gate第一次在十二例/四拒绝均完成后，旧targeted脚本继承B500 capture目录，找不到t01而exit2。修正子脚本使用 `captures/metal-smoke`；只接续旧回归/反例/生命周期，未重复已通过的GPU场景。失败manifest/日志独立保留为 `before-capture-dir-fix-manifest.json` / `old-arguments-before-capture-dir-fix.log`，最终manifest保留该已修正失败条目及80个成功step。
- 新转换工具的参数数组只有5项却传count6，CPU子进程SIGSEGV；LLDB回溯为DXC读取越界argv到wcslen。改为数组实际长度，最终转换通过。两份crash回溯保留；这不是RenderDoc/UE/GPU死机。
- root signature1.0被转换器拒绝（error2），按实际UE1.1格式修正，失败日志保留。runtime实现要求ARC及MTLResourceID结构赋值，初次helper构建失败日志保留，修正后通过；另一个StartFrameCapture误用返回值的编译错误已修正，其工具输出保留在对话记录。

backend/bundle不变：`03fc7b32c31d0c31c07655df462d2814c984086a646e14982d924b17333832b1`；GUI不变：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。最终hash/语法/diff检查PASS。产物 `captures/metal-ray-b500/`（12成功packet、4预期拒绝packet、1转换IRcapture），`build-macos-debug/metal-ray-b500/manifest.json`、`source-binary-hashes.json`、packet/旧回归/lifecycle日志，`ir-native/manifest.json`、ABI/compile审计、DXIL/五metallib、native/capture/export/拒绝日志及gate。

## 接续

下一优先给该真实converted fixture建立完整 IR packet/SBT/GRS/AS-header typed来源声明，按帧内快照顺序恢复、保持shader index与stride/null语义，加入损坏输入与事件/EID0验证。不能为读这份capture放宽无声明raw GPU identity guard。随后扩到UE的local root/static sampler/descriptor heaps、实际UE有限调度，再最终完整回归及能力验收。

UE实际RT、旧75份RT、本库集中308份、原Qt sizeHint crash、ARC父对象提前销毁未跑；nested/render/Private或GPU写入ray参数未验，帧内argument重编码仍不支持。系统死机根因未修，B495取证和有限诊断约束继续有效。能力仍false，持续目标active，未提交/推送。
