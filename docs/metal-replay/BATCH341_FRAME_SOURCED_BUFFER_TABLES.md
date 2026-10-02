# BATCH341：帧内普通 buffer / 临时表与每次 seek 重建

2026-10-01；持续目标 active，未提交、推送或完整 UE GPU replay。

对照已有 heap placement 的帧时序，普通 MTLDevice newBuffer 在 active capture
也保存帧创建事件并登记 frame resource；资源 record 继续用于下一捕获的背景
状态。创建与归档处于 capture transition read lock 内。之前这些创建只在资源
记录里，错误地落在 scope 前。descriptorTable/GPUWrites 支持 active diagnostic
归档，不授予 UE replay coverage。

新增 coverage v8：v7 来源 shadow + 小型 Shared 普通 buffer 帧创建与 CPU 临时
表。CPU preflight 在 native 分配前检查≤64KiB/总≤1MiB、创建/声明/slot/来源顺序、
实际 capture VA+offset、typed range，未来对象不通过地址搜索代替。仅消费前
创建，仍一个CB/≤4单线程dispatch/2×2clear；frame GPU-owned新表、heap/alias、
render consumption、多提交均仍拒绝。v1–7保持现有约束。

加载后把帧 buffer 加入已有 frame allocation 重建路径；每次 seek 先完成 GPU，
释放 native objects，再由创建 chunk重建。普通分配不重置 heap；logical ResourceId
与 capture source shadow保留。帧内CPU initializer更新为提交所属快照，重放时
用当前新native VA再次patch，而非沿用loading pass中的VA。

实际小例每帧新建 input buffer和CPU payload table，再执行真实 compute packet
复制、纹理采样。两份捕获为122→186和186→122；四轮seek后占用已释放的小
分配，强迫source VA改变：第一份20164/20676/21188/21700（共同高位90195400000），
第二份20420/20932/21444/21956；GPU结果、普通常量、DEADBEEF与clear像素不变。
19个创建/声明/来源错误组及24个GPU producer错误组 API+CLI均在GPU前拒绝。

十二类tiny及214 API+CLI错误组全部通过，日志metal-descriptors.lbl9lS；
旧t01/t02/t09/t11/t12/t35 API+CLI再次通过，targeted-sourced-frame-final.log。
库SHA256 37063340c32d572e208e7fd7122e85747d179e0cf9ccaa9cab3f0eb2e79b5c80。

UE会话20261001-075136，沿用B339隔离retained MetalRHI，End=1，owned PID20109
SIGTERM/launcher exit0。捕获15,957,652bytes，SHA256
 a5907a666a4138aba0111d1e930e146b99aef6ec87513b28ef0fee48c08ba1c4，
保护副本Saved/RenderDocMetalCaptures/UE58_NewMap_frame_births_a5907a66.rdc。
CPU审计45,807chunks/scope26,811，1,479/1,479帧首槽匹配、106 producer/
106 expected/source匹配；lifetime/frame epoch/inline/producer问题均0。
帧内两次普通buffer创建、五条临时表声明正确入流；4,649历史来源身份未保留。
没有把CPU证据当作实际UE GPU执行或执行时alias证明。

下一项从真实UE帧提取UpdateDescriptorHandle shader与精确小输入，先验证单线程
scatter pass；然后继续frame placement/views、alias生命周期与render stage消费。
全量回归未跑，新增人工UI未验；完整UE正确图像、MRT、pass scope仍未验收。
