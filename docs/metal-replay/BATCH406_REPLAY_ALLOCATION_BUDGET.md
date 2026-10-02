# B406：按 backing allocation 计费与提交前诊断

最新真实 UE eb9386b7 逐项 CPU 查询：34 heaps 2357460992B，5 standalone buffers 和8 drawable/standalone textures Native36241408B；Native 总2393702400B。1257个 heap buffer 的664615760B共享 heap backing，不能再次当作 Native 分配。保留 initial1282587160B，最大 texture staging75759616B，Private buffer staging复用16MiB，serial uploader峰值取最大值。captured CPU update10776825B，含2份initial和192MiB余量的保守成本5170980137B。报告 `build-macos-debug/local-m2-descriptor-replay/ue-render-indirect.budget-audit.json`；查询创建device但不创建heap/resource/queue，不提交GPU。

通用来源复用本地官方RenderDoc D3D12 `Serialise_CreateHeap` backing与placement关系、Vulkan `Serialise_vkAllocateMemory` 和buffer/device-memory地址范围；Metal独有尺寸查询调用device heapBuffer/TextureSizeAndAlign。Apple对recommendedMaxWorkingSetSize定义为性能不受影响的分配估算，不能当作free memory：[官方文档](https://developer.apple.com/documentation/metal/mtldevice/recommendedmaxworkingsetsize)。新预算上限是本项目保守策略，不是Apple保证。

候选 coverage60：heap单次<=128MiB；Native backing累计一次，standalone使用Native size/alignment，heap自身按64KiB保守取整；Native成本及现有device分配+128MiB预留不得超过 min(3GiB,recommended/4)。Native+当前分配+2份initial+captured CPU snapshots+未有initial的buffer fallback snapshots+192MiB余量不得超过min(6GiB,recommended/2)。检查加法overflow、duplicate heap/background buffer、frame heap、无效size/options。旧coverage<=59保持原预算和行为。

39913195/Dt7nPN微型定向6captures/24reset cycles/110indirect反例/15allocation反例通过。真实64/128MiB placement heap，128B Private原始indirect3/6，indexed、MRT每像素/ordinary metadata、末尾source0正确，端点hash不变。component无GPU验证device/headroom/CPU/snapshot/overflow/boundary/hardcap。后续pre-submit诊断版本85d2c505，尚未重新验收该最终库全量。

`RENDERDOC_METAL_CPU_METADATA_COVERAGE=60`仅在ReadLogInitialisation mandatory-exit路径提供candidate版本。ScanDescriptorMetadata完成后无论成功失败都会返回APIReplayFailed，不进入Native资源加载/帧GPU；不修改RDC或给provider声明完整coverage。真实UE原文件已通过全部metadata/per-use evidence/budget，Native2393702400B、initial1282587160B、snapshots含fallback10907897B，device当前allocated393216B；log `ue-render-indirect.metadata-only60.log`，GPU wait begin不存在。返回code19是诊断出口，不能报告为正常OpenCapture成功。

`RENDERDOC_METAL_PRE_SUBMIT_COVERAGE=60`进一步允许背景Native资源和原生PSO/reflection加载，验证descriptor/source/lifetime/encoder/work closure；强制拒绝背景command encoding/submission，CaptureIndirectArguments CPU证据例外。CPU replaceRegion保留原Shared/Managed storage/range/pitch验证，不创建replay CB。到CaptureScope经过PrepareDescriptorTables/ValidateDescriptorFrame后必然退出，位于ResetReplayCPUUpdatedBuffers/Private buffer和texture GPU upload之前。微型pre-submit已通过，并在日志证明没有replay wait；真实UE原文件诊断进行中。

最新实际整帧UE GPU/replay和人工UI尚未验收；持续目标active，不提交或推送。

真实UE pre-submit85d2c505背景资源加载在MTLBuffer::newTextureWithDescriptor拒绝，尚未GPU上传或执行frame。CPU定位6个TextureBuffer宽度大于旧1114112限制：2145/2146 R32Float2621440texels、14057/14059 R32Uint4587520、14532/14537 R32Uint4538368；全部背靠<=128MiB已完整初始化的Private buffer，offset0，实际10/17.5/17.3125MiB范围。继续按Vulkan buffer view allocation extent而非旧夹具width修正，先Native极小shader采样3点验证，不回放整帧。
