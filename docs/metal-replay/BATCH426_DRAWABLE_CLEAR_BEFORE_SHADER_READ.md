# B426：current drawable 在 Clear 后成为 sourced shader 来源

真实 UE chunk4119 获得 current drawable17885，BGRA8 900×640/Managed/Tracked，frame38421将其绑定SRV、38422原生RenderPass Clear。当前 drawable 无frame-start initialpixels，valid Native handle不应因为绑定发生在Clear之前被拒绝；读取前必须初始化。沿用Vulkan swapchain image与vkCmdBeginRenderPass的Clear-before-shader-read顺序，不伪造initialpixels。

coverage65 从原始nextDrawable chunk记录明确ResourceId，Native identity relocation仅接受既有2D BGRA8≤2048、mip/sample/array/depth1非framebufferOnly drawable。缺initial的drawable必须完整原NativeClear（显式pass尺寸0或等于图像）、或经相同CB/已提交同队列Clear再Load；每次dispatch/draw对encoder typed-table/source closure验证clear predecessor，跨unsubmittedCB或先读后clear拒绝。旧tiny fallback也显式要求该完整initialization，避免2×2图像绕过。Nativeoperations/pixels未替换。

精确d6c890d4、metal-drawable-clear.d1jDEu：2captures/8resetseeks，先获得drawable、完整typed SRV CPU slot/binding，再原NativeClear(.25,.5,.75,1)，原Nativecompute读取，所有4像素rawBGRA=bf8040ff、GPU sum638/DEADBEEF，Native与replay一致。18 API+CLI rejectiongroups：legacy64/missingclear/Load或DontCare无initial/先读后clear/otherunsubmittedclear/partialextent/非drawablebirth/错误format。第二帧含历史drawable，gate按实际attachmentResourceId选择currentbirth。测试中曾发现旧tiny fallback对partialClear误放行，已修复并全部复验；第二帧最初gate错误修改历史drawable，已定位修复，Native/replay始终通过。

真实UE后续pre-submit进行中。固定d6c890d4库副本准备独立全量回归，整帧GPU/UI未验收，无提交推送。

固定d6c890d4库副本全量通过：308captures/7786malformed/3080lifecycle opens、residentgrowth0B，start/endhash一致。日志drawable-clear-frozen-combined-regression.log。该结果不替代最新B427/B428库01db4c65的全量验收。
