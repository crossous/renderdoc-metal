# B520：UE 大 placement heap 的有界描述表预检

同一UE capture的heap3496为512MiB Private/Tracked/Placement。Metal Serialise_newHeap已支持4096–576MiB，描述表coverage60–65却仍硬限128MiB。参考DX12 Serialise_CreateHeap按原HeapDesc恢复、Vulkan Serialise_vkAllocateMemory按原allocation及设备兼容native size恢复的处理，本批仅令coverage65描述表预检与既有原生heap范围一致，不压缩offset或猜测未用区域；旧coverage<=64边界保持。native backing只计heap一次，placement children仍通过真实size/alignment/range/alias检查。

预检新增读完storage/cache/hazard/type并匹配原生Serialise_newHeap约束，拒绝无效ID、重复、frame内heap、Shared automatic、memoryless、缓存/跟踪/类型越界。完整heap descriptor拒绝诊断输出实际字段。仍保留MetalReplayAllocationBudget的设备四分之一/3GiB GPU上限、二分之一/6GiB总上限、driver/uploader余量和初态两份费用；没有更改Fits，不能为了UE通过扩大预算。

最终backend/bundle SHA256 `da2234f12fd2136624cdd6bd61386dabea9c8f636fe1ffe33ecfb522297f0a88`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，manifest及最终库在 `build-macos-debug/metal-ray-b520/`。

| 范围 | 实际结果 |
| --- | --- |
| 129/192MiB placement heap | 2native/4capture/API/CLI3 PASS，48事件/EID0、全部R16 mip像素/usage/不同GPU ID |
| 31损坏capture | 62 API/CLI正确拒绝、无GPU wait；13新heap descriptor/出生/总预算组与18旧texture组 |
| 六个576MiB heap的总费用 | native3623878852，明确拒绝，未分配这些大heap/未提交GPU工作 |
| B514–519原capture | 36旧texture+10view API/CLI3 PASS，未重跑native |
| 旧RGBA8/R16 view | 2native/4capture/API/CLI3 PASS |
| IR multi64 | 原B512capture当前库API/CLI3 PASS，非78组全量 |
| 官方Apple MIT sample | pin/license核验、2scene native/capture/API270事件/CLI3和10查询 PASS；13坏sample未跑 |
| 六能力/CPU预算组件/语法/diff | PASS，两公开能力false |
| 实际UE | CPU65 mandatory-exit，total预算拒绝；GPU输出/重放/事件未验 |
| 完整78IR/75RT/308、旧84间接坏证据、32CPU/AIR、Qt/ARC | 新库 NOT RUN，旧库结果不继承 |

`UE-memory.json`流式读取原XML：70heap共5266571264 bytes，最大512MiB；最大的初态包括134217728/89473024/75759616及多份67108864 bytes buffer。新CPU预检按真实512MiB费用（此前对无效heap截断到128MiB）得native5321785600、initial4362319907、snapshots24864323，确实超过现有总预算；没有启动UE GPU回放、没有改capture/coverage。应先缩减实际UE工作集/场景，而不能放开安全上限。引擎源码有RHI.TransientAllocator.MinimumHeapSize等配置，但本批没有试运行或宣称其能缩减这些heap。

构建/GPU全串行互斥，当前所有进程终止。第一次bundle构建目标名误写qrenderdoc，已改build-qrenderdoc成功；原失败日志`app-build-wrong-target.log`保留。严格扫描482完成日志无断言/overrun/streamseek/未知资源/资源表未空诊断。576MiB单heap的真实GPU场景未跑，512MiB UE heap仅CPU核验，不扩大该测试结论。系统冻结和用户原Qt/AS崩溃根因仍未闭环；持续任务未完成。
