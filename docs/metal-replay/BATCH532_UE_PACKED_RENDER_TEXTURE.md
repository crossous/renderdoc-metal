# B532：UE packed 二维读写与渲染目标

完成。实际B528 CPU65在RGB10A2Unorm2D 320×240/mip1/usage7拒绝。参照DX12 typed RTV+UAV及VK color attachment+storage/read用途，coverage65既有Private/Tracked、≤512二维单mip/array1/sample1 RW3允许带RenderTarget7。wholeheap、alias lifetime和总预算不变；旧3D/旧coverage边界不变，现有frame render-pass预检复用。

320×240、512²、3×5各两生命周期，六native→capture→API/CLI3通过。原生真实render pass clear(0.125/0.375/0.625/1)，随后独立compute读取32/96/160和保护字，commit/wait后再write/read/overwrite。重放核验独立clear usage/EID、所有packed字128/384/639、角落PickPixel；计算前后全部bits/PickPixel、两个CS_RW/独立read及不同GPU ID通过。每cycle增加clear选择，六capture共96事件往返/EID0；helper原static计数改显式递归参数，支持额外clear-read dispatch而保持老second-dispatch语义。

20坏组40 API4/CLI1 GPU前拒绝，usage_render5缺declared writer、atomic19/unknown35，width/height、mips/depth/array/sample/format/storage/hazard/legacy64及heap范围/对齐/重复birth。已支持RW+RT7不再作为packed2d坏输入；原RW3也由同时出生rwImage和旧六capture验证。

旧72texture/view/heap/volume capture、八B526query/四B512 TraceRay API/CLI3，fresh整数array/2Datomic两native/四capture，固定MIT官方两scene/270事件/EID0/10查询/CLI3、六能力和CPU budget通过。592完成日志无断言/overrun/前向stream seek/未知资源/资源表非空/AS快照诊断。

原B528实际UE仅mandatory-no-GPU CPU65，越过RGB10A2 usage7；下一BGRA8Unorm_sRGB2D 320×240/mip1/usage7 frame拒绝，无新UE/GPU输出/事件/绑定或总预算通过证据。full78IR/75RT/308、32CPU/AIR、旧84bad、Qt/ARC、官方13bad未跑、不继承旧hash，原用户Qt/AS ForceCrash和系统冻结根因未闭环。

产物build-macos-debug/metal-ray-b532：manifest.json、final-packed/manifest.json和六capture/MSL/AIR/metallib、regressions/manifest.json、UE-CPU.log及官方gate-results，librenderdoc-final.dylib/source hashes。

backend/bundle SHA256 dba15ac3b254f91afa4f999dfc0d1b1df61a894ef0723bb9b7f86241d929fc7c；GUI3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。构建/GPU共享锁串行、有限超时，syntax/diff通过。两flagsfalse；下一B533小Lumen cache真实新UEcapture，核实实际HW query/成本，再sRGB/typed动态header/producer。持续active，无提交推送。
