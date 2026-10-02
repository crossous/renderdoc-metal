# UE5.8.3 完整 descriptor provider 的缺口

2026-10-01。完整coverage仍未实现；BATCH333已实现并局部编译诊断provider，
正常Metal启动通过，自动重截/CPU审计进行中。**诊断provider不等同于replay支持**。
现有插件只记录primary布局，不能添加coverage v3来放行真实UE帧。

## 已核实的来源

引擎路径 `/Users/Shared/Epic Games/UE_5.8/Engine`，5.8.3 CL58210709。

| 代码位置（相对Engine/Source） | 必须记录的信息 | 当前缺口 |
| --- | --- | --- |
| Runtime/Apple/MetalRHI/Private/MetalBindlessDescriptors.cpp FMetalDescriptorHeap::Init | 所有primary和临时heap的实际native buffer、offset、容量、schema | 插件只取primary两张，FlushPending另建256-byte override heap |
| 同文件 AllocateDescriptor / UpdateDescriptorImmediately | slot出生、完整CPU entry写入及其时点 | B333记录generation/完整24-byte CPU事件；driver语义重放尚未接通 |
| 同文件 FreeDescriptor | deferred delete真正执行后的slot失效 | B333在实际deferred callback交回pool前记录free；旧bytes不清，driver待接 |
| 同文件 FlushPendingDescriptorUpdates | EntriesBuffer的实际suballocation offset/count、schema，Indices普通数据；primary为GPU目的表 | 临时allocator普通buffer里混有其它payload；不能按buffer尺寸/label猜布局 |
| Runtime/Apple/MetalRHI/Private/MetalStateCache.cpp IRBindResourcesToEncoder | CBVTable经SetShaderBytes传入的VA字段布局、stage、binding与长度；static sampler table | B332极小compute setBytes显式契约已验；UE hook与render stages仍缺 |
| 同文件 IRBindPackedUniforms / IRBindUniformBuffer | uniform allocation VA与slice、嵌套typed来源 | 根地址有效不足以证明全部间接payload布局 |
| Runtime/RHICore/Public/RHIDescriptorAllocator.h / Private/RHIDescriptorAllocator.cpp | 帧首活跃slot集合与alloc/free变化（锁保护） | GetAllocatedRange只返回包围区间，不排除其中free holes；Ranges是private |

FreeDescriptor的失效时点必须落在deferred delete实际执行时，而非排队时，避免
把仍被已提交GPU工作引用的slot提前作废。还需保留Metal command buffer资源引用
到完成边界，以及view→parent/heap引用关系。

## 接入顺序

1. 为driver设计并验证有效slot、执行点动态slice与inline typed bytes记录；保持
   旧chunk编号/section version兼容，旧capture无新增记录仍保守拒绝。
2. 用B333的匹配官方源码局部MetalRHI编译路线加按需provider；仅当RenderDoc
   API存在、所有hook覆盖且版本匹配时启用，禁止未完整覆盖便作coverage承诺。
3. 帧首先记录活跃slots与全部实际layout；GPU目的表不取自动Shared CPUdiff，
   即时CPU写调用显式完整entry；临时GPU update source按实际slice记录layout。
4. 每个新契约先最小Metal正例/负例、GPU字节与反复seek，然后小UE场景新capture
   的CPU审计；完成render/alias/inline支持与本地预算后再考虑整帧GPU。

私有执行点不能仅靠原项目插件取得。B333已从官方5.8.3 tag局部核对源码，并用
缓存compile/link response直接构建隔离MetalRHI，三个unity去PCH依赖顺序编译，
保留98/98原导出和全部类布局。通过进程级DYLD路径选择副本；安装MetalRHI和
其余原Engine库保持已校验原件。无需用户提供整套源码版或启动全量Engine build。
早期UBT导出误删除123个库的事故及官方manifest恢复证据见
[BATCH333](BATCH333_UE_PARTIAL_PROVIDER.md)；后续禁止该导出方式。

参照RenderDoc D3D12对buffer VA和descriptor对象的明确元数据、Vulkan
vk_descriptor_funcs对binding/descriptor类型和资源的明确记录；Metal裸内存
没有这样的API，不能照搬Vulkan忽略未引用descriptor的条件来删掉未知字段。
