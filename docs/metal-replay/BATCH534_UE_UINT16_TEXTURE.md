# B534：UE R16Uint 二维纹理

完成。B533原UE新capture CPU65在4096×16 R16Uint2D/mip1/RW3拒绝。参照VK/DX12 ordinary typed unsigned16 UAV，coverage65 Private/Tracked、二维单mip/array1/sample1、width≤4096/height≤512、exact RW3；复用logical size、完整native heap/lifetime/CPU-GPU预算。其他format/旧coverage保持，atomic/RT/unknown usage不支持。

4096×16、4096×512、3×5三shape各两个生命周期，六native→capture→API/CLI3、72事件/EID0通过。每y unsigned16 before32768+y→after65535-y，证明高位非signed/truncation；全部2-byte rawbits、首末/中间PickPixel、两write/独立read usage及GPU ID变化通过。每线程最多八相邻像素，逐像素边界，max恰262144invocations，工作和wholeheap总预算不放宽。oracle integer raw读取按compByteWidth(2/4)而非固定4bytes。

20坏组40 API4/CLI1 GPU前拒绝，width4097/height513、mips/depth/array/sample/format、usage/RT7/atomic19/unknown35/storage/hazard/legacy64及heap范围/对齐/重复birth。旧78texture/view/heap/volume capture、八B526query/四B512 TraceRay API/CLI3，fresh整数array/2Datomic两native/四capture，固定MIT官方两scene/270事件/EID0/10查询/CLI3、六能力和CPU budget通过。616最终完成日志无断言/overrun/streamseek/未知资源/资源表非空/AS快照诊断。

B533实际原RDC仅mandatory-no-GPU CPU65，越过所有frame texture创建，随后静态allocation GPU预算拒绝：native3,593,011,456、initial1,170,276,095、snapshots12,957,904、recommended12,713,115,648、alreadyAllocated393,216bytes。原RDC不改，没有实际GPUreplay/output/events/bindings/EID0验收；旧B528 BGRA8_sRGB frame birth仍未支持，不能将这个scene通过创建盘点视作全UE功能。full78IR/75RT/308、32CPU/AIR、旧84bad、Qt/ARC、官方13bad未跑，不继承旧hash；原用户Qt/AS ForceCrash与系统冻结根因未闭环。

产物build-macos-debug/metal-ray-b534：manifest.json、final-short/manifest.json和六capture/MSL/AIR/metallib、regressions/manifest.json、UE-CPU.log及官方gate-results，librenderdoc-final.dylib/source hashes。

backend/bundle SHA256 f4f8b745e688af052822885281e5f004af500729b08c600284741b68532ae66d；GUI3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。构建/GPU共享锁串行、有限超时，syntax/diff通过。两flagsfalse；下一B535 fresh isolated provider16MiB block与有界UEcapture，核验wholeheap预算，再typed动态header/producer及sRGB等实际缺口。持续active，无提交推送。
