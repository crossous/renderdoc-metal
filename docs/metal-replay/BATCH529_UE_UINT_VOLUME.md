# B529：UE frame R32Uint 三维纹理

完成。真实B528 CPU65缺口192x48x48 R32Uint RW3D。参照DX12 Texture3D subresource/typed UAV与VK VK_IMAGE_TYPE_3D，coverage65新增Private Tracked单mip array1/sample1、width≤256/height-depth≤64整数volume；RW3，无RT/atomic用途，旧coverage与其他格式不变。复用完整native heap extent/lifetime/CPU和GPU总预算。

三shape192x48x48、256x64x64、3x5x2各两个captured生命周期：六native/capture/API/CLI3，72 event选择/EID0；各z不同uint(64+z→192-z)、全部体素rawbits、首末/中间PickPixel、两个write及独立read usage、textureID变化及EID0零表。每dispatch最多262144个线程：UE和max样例每线程写最多四个相邻体素，边缘不足四个严格限界；没有扩大生产工作预算。20坏组40 API4/CLI1 GPU前拒绝，含范围、mip、array、sample、format、usage/atomic、storage/hazard、legacy64、heap范围/alignment、重复birth；无wait/断言。

相关旧50 R16/packed2D/R8/R11/half-array/mipviews/heap和四float volume capture API/CLI3通过（54 old captures），八B526 query与四B512 TraceRay API/CLI3通过。fresh integer array/2D atomic两native/四capture/API/CLI3通过。固定官方Apple MIT两scene/270事件/EID0/10查询/CLI3、六能力及CPU budget组件通过；不重复native旧capture。

首次final-volume sample native/capture成功，API预检拒绝单dispatch线程数442368超既有262144；改fixture四体素/线程后verified-volume完整验收。regressions旧54/八query/四IR均通过后，fresh uint-array raw-source无AIR导致usage=0、oracle14；保留FAIL manifest，不把这个失败算通过。remaining-regressions.py导出对应MSL、编译metallib，与新volume gate一致后fresh四capture及余下官方/能力/CPU通过；生产未删usage检查。已经通过的54 old没有再次重跑。严格扫描只计完成PASS条目和最终目录，失败保留。

原B528 UE capture仅CPU65 mandatory-no-GPU再次检查，越过R32Uint3D，下一RG11B10Float3D10x8x26/mip1/usage3 frame texture拒绝。无新UE运行/截帧/GPU重放，UE输出/事件/总预算未验。完整78IR/75RT/308、32CPU/AIR、旧84bad、Qt/ARC、官方13bad未跑，不继承旧hash。原用户Qt/AS ForceCrash和系统冻结根因未闭环。

产物build-macos-debug/metal-ray-b529：manifest.json、verified-volume/manifest.json及六capture/MSL/AIR/metallib、regressions/manifest.json（保留部分FAIL和之前PASS条目）、regressions-final/manifest.json/official/gate-results、UE-CPU.log；final-volume/失败和diagnose.*保留。最终librenderdoc-final.dylib与源码/工具hash见manifest。

backend/bundle SHA256 aee8c20d4b557fa92eb3c6ac5b5dc27899c0daa469c648f72f8ba853c2ec6a99；GUI3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。构建/GPU共享锁串行、有限超时，syntax/diff通过，两flagsfalse。下一B530实际packed-float3D sample，再动态Shared header/producer及预算闭包。持续active，无提交推送。

最终524完成PASS日志严格无断言/overrun/streamseek/资源表/快照诊断。
