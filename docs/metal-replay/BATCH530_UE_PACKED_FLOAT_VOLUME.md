# B530：UE packed-float 三维纹理

完成。真实B528 CPU65在10×8×26 RG11B10Float3D/mip1/RW3拒绝。参照VK image3D和DX12 typed Texture3D/UAV、逻辑初态和native heap收费，coverage65补Private/Tracked、各维≤64、array1/sample1单mip RW3。复用既有11/11/10 codec、placement范围/alias lifetime和总预算，旧coverage保持。

10×8×26、64³、3×5×2三shape各两个生命周期，六native→capture→API/CLI3通过；72事件往返/EID0，每z不同red (1+z)/128→(64-z)/128，完整packed字节、首末z三位置PickPixel、两write/独立read usage及texture ID变化验证通过。20坏组40 API4/CLI1预提交拒绝，包含维度、mip、array/sample、深度3D不支持、usage/atomic/storage/hazard/legacy64、heap越界/alignment/重复birth。

首轮final-volume六有效capture验收通过，但wrong_format用92，与原RG11B10Float格式相同，因此原有效capture能合法重放，OpenCapture0且有GPUwait，属于反例预期错误，保留FAIL与完整日志，不计为拒绝。将反例改为Depth32Float(252)3D，verified-volume全部重新通过；没有删生产检查或新增对合法格式的禁止。

相关旧60texture/view/heap/volume capture API/CLI3、八B526query与四B512 TraceRay API/CLI3通过；fresh整数array/2Datomic两native/四capture/API/CLI3通过（对应MSL编译metallib），官方固定MIT两scene64²/frames4/seed1、270事件/EID0/10查询/CLI3，六能力和CPU预算组件通过。544完成最终日志无断言、overrun、前向stream seek、未知资源、资源表非空或缺AS快照诊断。

原B528实际UE capture仅mandatory-no-GPU CPU65，越过packed3D；下一RG11B10Float2D 4096×4096/mip1/usage3 frame texture拒绝。没有新UE截帧、UE GPU输出/事件/绑定/EID0验收或总预算通过证据。完整78IR/75RT/308、32CPU/AIR、旧84bad、Qt/ARC、官方13bad未重跑，旧hash结果不继承。原用户Qt/AS ForceCrash及系统冻结根因未闭环。

产物build-macos-debug/metal-ray-b530：manifest.json、verified-volume/manifest.json及六capture/MSL/AIR/metallib、regressions/manifest.json及official/gate-results和UE-CPU.log；final-volume/保留首轮反例失败。最终librenderdoc-final.dylib及源码/工具hash见manifest。

backend/bundle SHA256 99df52e439193cebda9ae8889b17425460602f76334aa7694e02d2287cf26cc1；GUI3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。构建/GPU共享锁串行，有限超时，syntax/diff通过。两flagsfalse；下一B531实际4096² packed二维sample与相关范围反例，再动态Shared header/typed producer/预算闭包。持续active，无提交推送。
