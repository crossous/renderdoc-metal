# B535：UE 隔离 placement block 粒度

2026-10-06 [B535](BATCH535_UE_SMALL_PLACEMENT_BLOCKS.md)：隔离provider placement block64→16MiB，唯一源码变更MetalBuffer.cpp、98exports完整，新模块ee945869；--rhi明确选择并记录hash。f4f8b745 backend/bundle不变，新UE capture fbe1b7ac/73,738,545bytes、3间接TLAS/96direct+56indirectcompute、60 AIR全部审查/2shader两非零HW query，owned cleanup -9。80heap共1,731,791,872bytes（旧3.557GB），初态1,175,476,527反略增；CPU65首次接受metadata/预算(native1,765,703,936/snapshot12,835,615)，未执行GPU重放。pre-submit65 API4于RGBA16Uint TextureBuffer8192/row65536创建，无GPUwait/初态上传；10保存日志无严格诊断。生产flagsfalse，UE输出/事件/EID0/typed动态header未验，原引擎/用户工程及guard保持；下一B536 typed RGBA16Uint view真实读写sample与旧回归，再当前UE提交前预检。持续active、未提交推送。

## 实现与参照

沿用VK typed VkBufferView/offset/range 和DX12 typed SRV/资源依赖的资源初态原则，Metal placement heap依旧捕获、收费和恢复整个backing。没有缩小截帧的heap描述或放宽RenderDoc预算。

逐文件核对旧provider-source-render-indirect与9d6349a6模块manifest，fresh copy至 `build-macos-debug/metal-ray-b535/provider-source`。唯一变更 `Private/MetalBuffer.cpp:1580` METALHEAP_DEFAULT_BLOCKSIZE64→16MiB，分配仍max(request,size)，native placement/alignment/lifetime保持。原安装引擎512MiB常量及原用户工程未改。

新增工具--rhi明确选择模块/记录SHA，prepare provider支持16/32MiB选项。共享锁内三个unity单元串行编译，provider ee945869c45fce3de0e124043990c569881c190e7f0e623856f8613a9aafaf24；安装与baseline98exports均无缺失。provider-preparation.json/provider-ABI.json/build-manifest.json保留。

## 实际UE与测试

固定640x480、隔离3mesh两light、small-shadows/small-lumen-caches，UE5.8.3 CL58210709。命令：

    python3 util/ue/run_testproj_metal_ray_macos.py --run --lumen-only --small-shadows --small-lumen-caches --capture-delay 6 --startup-timeout 120 --rhi build-macos-debug/metal-ray-b535/provider-build/libUnrealEditor-MetalRHI.dylib --project build-macos-debug/metal-ray-b527/scene-minimal/project/MetalRayScene.uproject --work-dir build-macos-debug/metal-ray-b535/UE-capture

session20261006-201426保存新帧、owned cleanup -9，不能计editor正常退出。captureSHA fbe1b7accd6d99e9535b8fb4dbc16410aabc0a9e184fc7340480ea4006bd9231，73,738,545bytes。3typed TLAS、7heap AS/identity、96direct+56indirectcompute。60原Lumen AIR全部审查0unvalidated：Main_0000a274_46403057/chunk32023/groups[2,64,1]、Main_0000c358_ed865861/chunk34035/groups[132,1,1] 两非零实际shader/PSO/function/library匹配，库e330d501/18b9c6bc同前。证明bound query shader实际调度，不证明每invocation走分支、typed AS header正确或图像结果。

80heap1,731,791,872bytes，比B53352heap3,556,769,792减少48.7%；初态blob1,175,476,527（旧1,170,276,095）及capture体积略增，不称所有成本单调下降。

| 检查 | 实际结果 |
| --- | --- |
| provider source/ABI build | PASS，一文件常量变更，98exports无缺失 |
| finite native UE capture、AIR/cvar来源 | PASS保存，owned -9清理 |
| normal-open | API4背景/帧内identity限制，无GPUwait |
| mandatory-no-GPU CPU65 | 接受metadata与预算后故意API4终止，不加载/重放GPU |
| pre-submit65 | API4格式113缓冲区视图创建不支持；无GPUwait/初态上传/frame replay |
| 保存10顶层/导出日志严格扫描、diff | PASS，未扫描完整编译cache日志 |

CPU预算native1,765,703,936、initial1,175,476,527、snapshots12,835,615、recommended12,713,115,648、alreadyAllocated393216。后续加载遇Buffer2986→Texture3287/chunk2634，RGBA16Uint(113)/TextureBuffer8192、row65536、Private/default、allowGPUOptimized=false；尚缺typed格式8bytes认可，数据范围66560合法。原RDC不改。

## 哈希、范围与下一步

`build-macos-debug/metal-ray-b535/final-manifest.json`、保存librenderdoc-final.dylib与上述日志是结果入口。backend及app bundle f4f8b745e688af052822885281e5f004af500729b08c600284741b68532ae66d；GUI3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。同B534后端，本批未重跑样例/旧GPU回归，引用B534同hash已验范围；不新增通过数量。UE output/离线整帧/seek/EID0/typed query binding，full78IR/75RT/308/QtARC未跑。原ForceCrash/重启根因未修。

两production能力false；下一B536按VK/DX12 typed view补RGBA16Uint8bytes，真实native/capture读写、raw/PickPixel/事件与坏输入/相关旧回归，然后当前新UE提交前检查，接续动态header/producer闭包。持续active、不提交推送。
