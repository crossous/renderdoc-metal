# B513：真实 UE Lumen inline 查询截帧与顺序证据扫描

B512 已在 ad0a0248 固定库完成验收，按用户要求回到 UE。能力仍 false，持续目标 active，不提交或推送。构建、UE 与 GPU 回放串行，复用统一互斥锁。没有重跑旧 B494/B495 长 UE/full followup。

## 实际 UE 截帧及光追证据

UE5.8.3 CL58210709，`/Users/crossous/Documents/Unreal Projects/Testproj/Testproj.uproject`；本次 isolated MetalRHI provider SHA256 `9d6349a655a5fb93c1fbeb1dd657b4c3eccce1167f07e8220e6a877e96a7e225`，未替换安装引擎。本次 native/capture 库仍为 B512 `ad0a0248f5a1f60df425d81e188aaec44a37814a56053f2596e8b071ec514bf2`，起止一致。

命令 `python3 util/ue/run_testproj_metal_ray_macos.py --run --lumen-only --capture-delay 6 --startup-timeout 120 --work-dir build-macos-debug/metal-ray-b513/UE-capture`。隔离 INI 和运行时查询确认 r.RayTracing=1、Lumen.HardwareRayTracing=1、Inline=1、GI/ReflectionMethod=1；独立 RT shadows=0，320x240 viewport，窗口请求640x480（UE实际最小900x640）。保留旧 capture 和原 settings；原 settings SHA256 `2a2c5e49dd2068cd54bc3f18283769ef4ffc51beaf1a4e66686794d0f687004c` 未变。诊断进程按 native device 值放行 compute，未提前打开 production 支持。

session `/Users/crossous/Documents/Unreal Projects/Testproj/Saved/RenderDocMetalSessions/20261006-124032`：完成初始化、受控一帧 capture，6次间接TLAS构建、643 compute dispatches。监督器捕获保存后清理 owned process group，editor_exit=-9；launch_exit0表示保存和清理成功，**不是编辑器自行正常关闭**。280 native compute compilation完成、pending0；这只证明编译完成，不当成 GPU ray query 输出正确。

原始捕获 `build-macos-debug/metal-ray-b513/UE-capture/original.rdc`，SHA256 `401db9a1fd10b972491a641b720153f99130ad2292990b5f2b0c6a5b4a5d7068`，546MiB左右；XML/ZIP已导出并保留。首次 API inventory 没有 RaygenIndirection/direct AS binding，不能当成“不运行光追”。UE使用 converted inline query，从资源包访问 AS。修 inventory 对 descriptor async 的小写 functionName、typed AS residency、PSO→function→library 和逐encoder/ordinal/command/buffer/offset 间接证据关联；重复/帧内或错owner记录不作为查询参数证明。

新增 `audit_ue_metal_ray_air.py` 从该 capture 的原始 library bytes 提取58个实际 Lumen-bound shader module，58全部离线反汇编。只将真实 air.allocate/reset/next_intersection_query 调用、实际 PSO/function/library、非零原始调度组关联，未从 label 推断光追。两条 shader 包含原生硬件查询：

| shader | 捕获 chunk index | 原 per-use 间接组 |
| --- | --- | --- |
| Main_0000a274_46403057 | 34717 / 47794 | 2×938×1 / 2×957×1 |
| Main_0000fe04_3ac302c4 | 36556 / 49556 | 634×1×1 / 635×1×1 |

证据 `UE-capture/AIR-proof/manifest.json` 保留 library hash、selected entry AIR、PSO/function/library/encoder、debug groups、参数 provenance。**确实是非零硬件查询 shader 调度，不是普通光栅帧；不保证每 invocation 都执行查询分支，不计输出或重放 PASS。** 没有 RT shader 单步/AS内部/RT Pixel History扩展。

## 按真实捕获修复

原库正常打开原 capture 在资源声明检查拒绝，但此前扫描同时产生前向压缩流 SetOffset 错误。对照 DX12 `d3d12_device.cpp:5467` 与 Vulkan `vk_core.cpp:3680` ReadSection/ReadChunk/EndChunk 顺序读取、DX12 SetComputeRootShaderResourceView typed地址与Vulkan AS descriptor处理；Metal 的 capture indirect evidence 仍保持 command/encoder/pass/source/offset/ordinal 关联和真实执行点比较。

`ScanDescriptorMetadata` 的 compute/render evidence 分支只消费自己的不同 chunk，结束交给 EndChunk 跳过余量，删除两个回退 SetOffset。不把整份546MiB捕获复制进可 seek 缓冲，不修改捕获、不跳过证据、不放宽 descriptor guard。修后原 UE capture 正常重放仍以“frame-born or invalid GPU identity”拒绝；candidate65 CPU-only 进一步定位 `ResourceId::11841 / heap3635` 的 R16Float 256×128 / mip8 / usage3 frame texture，**候选未获准重放**。

强化日志检查发现旧 compute/render 反例生成器把所有chunk length设0，转换虽exit0但记录嵌套chunk断言。三个生成器保留非零可跳过预算，加128bytes允许变长反例。保留 `before-invalid-budget-fix/`，不能计为通过。两个 fixture `undeclared` 和 `write_alias` 本来是拒绝控制，验证 orchestrator 首次误当正例，已改期望4，失败日志/manifest分别保留；没有为通过测试改变产品 guard。

## 修后固定库验证

后端及app bundle SHA256均为 `77a7fa990601c460168339fdefd8a82e34ecff911f418b8644aa37729de5a4f3`，GUI仍 `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。证据根目录 `build-macos-debug/metal-ray-b513/`。

| 范围 | 实际结果 |
| --- | --- |
| 实际UE capture | PASS保存及owned cleanup；native/capture使用ad0a，非77新capture；原帧完整replay FAIL，输出/事件/EID0未验 |
| actual ray-query shader audit | PASS CPU关联，两shader/四次非零组，58 library module；无GPU提交，不把静态分支核验计逐ray输出 |
| 原capture正常打开/CPU-only candidate65 | EXPECTED REJECTION；77修后无压缩流回退，声明/多mip边界保持；无完整重放 |
| compute indirect旧8 captures | PASS最终输出/guard words/1→3→2原参数、每份7 seek/reset，56次 |
| render indirect旧8合法captures | PASS serial/indexed/parallel/unretained，输出及每份7 seek/reset，56次；另undeclared/write_alias两拒绝控制 |
| 间接证据反例 | PASS compute22 + render31×2 =84组，各API/CLI拒绝共168次；另2真实执行点参数不一致，API/CLI4次拒绝 |
| IR64几何 | PASS B512已捕获frame multi64的API/三方向EID0与CLI3，当前77库；未重跑全部78 native场景 |
| 官方MIT Apple sample | PASS修后77两scene native→capture→replay字节/事件三方向270选择/CLI；本批未跑13官方坏capture |
| device能力 | PASS6入口默认false/native判断；未启用生产开关 |
| CPU | inventory8 + AIR3 + supervisor21 =32 tests；Python语法、diff和hash检查 |
| 完整78 IR反例/75 RT/308/原Qt崩溃/ARC早释/UE最终图像和事件 | NOT RUN或NOT VALIDATED；B512 ad0a验收不能转计77全量通过 |

源代码/最终binary hash、validation/manifest、官方gate、CPU日志、AIR-proof与system-report-comparison保留。本次系统相关报告 filename/stat 相对B512变化0；未证明旧重启的GPU具体根因，不能说冻结已修复。

## 接续

优先实际UE暴露的R16Float多mip frame texture与完整typed inline-query AS header/root/资源包 producer关联，再核验真实GPU输出、前后事件/EID0及损坏拒绝。捕获中AS构建通过不等于raw AS header重定位已支持；不得靠添加coverage声明或放宽失败条件推进。必要时用匹配DX12/VK的最小converted RayQuery fixture补齐并验证，再回同一有限UE流程。IR refit/copy/异buffer和更广资源布局也仍缺。只有sample、UE实际所需范围、native判断和最终库受影响回归通过，才考虑对应supportsRaytracing，并重新验收；持续任务不停止。
