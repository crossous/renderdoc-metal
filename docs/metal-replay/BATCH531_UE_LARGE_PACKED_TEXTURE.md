# B531：UE 大 packed 二维纹理

完成。B530实际B528 CPU65下一4096² RG11B10Float2D/mip1/RW3。参照VK typed image2D和DX12普通typed UAV/subresource，coverage65 Private/Tracked单mip array1/sample1、各维≤4096、exact RW3；RT usage/旧coverage保持原边界。完整native heap、logical initial及CPU/GPU预算不变。

4096²、2048×1376、3×5三shape各两生命周期，六native→capture→API/CLI3，72事件/EID0、完整11/11/10 packed字节与PickPixel、两write/独立read usage及GPU ID变化通过。fixture每线程最多8×8像素，每像素限界，max恰262144线程/dispatch，不增加生产预算。fixture wholeheap≤576MiB与现有policy一致，max四texture合计256MiB。

20坏组40 API4/CLI1 GPU前拒绝：width/height4097，mip/depth/array/sample/format，usage缺write/RT7/atomic19/unknown35，storage/hazard/legacy64、heap范围/对齐/重复birth。旧66texture/view/heap/volume capture、八B526query与四B512 TraceRay API/CLI3通过；fresh整数array/2Datomic两native/四capture通过。固定官方MIT两scene/270事件/EID0/10查询/CLI3、六能力/CPU budget通过。

实际B528原UE capture仅mandatory-no-GPU CPU65，越过4096²；下一RGB10A2Unorm2D 320×240/mip1/usage7 frame拒绝。没有新UE运行/截帧/GPU输出/事件/绑定或总预算通过证据。full78IR/75RT/308、32CPU/AIR、旧84bad、Qt/ARC、官方13bad未跑、不继承旧hash；原用户Qt/AS ForceCrash与系统冻结根因未闭环。

产物build-macos-debug/metal-ray-b531：manifest.json、final-packed/manifest.json和六capture/MSL/AIR/metallib、regressions/manifest.json、UE-CPU.log及官方gate-results。最终librenderdoc-final.dylib和源码/工具hash见manifest。

backend/bundle SHA256 3f7e0854f7a1f645f6a0cf1b784522bb4045b71a34282ef859a8277420b0b4d3；GUI3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。568完成日志无严格诊断；构建/GPU共享锁串行、有限超时，syntax/diff通过。两flagsfalse，下一B532 packed二维RT用途与clear/sample，再小Lumen缓存/typed动态header/producer/预算闭包。持续active，无提交推送。
