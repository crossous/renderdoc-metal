# BATCH351：UE 绘制常量与静态索引来源

2026-10-01，持续目标 active；未经提交或推送。

对照当前 UE 5.8.3 的 Apple metal_irconverter_runtime.h：direct draw 的
IRRuntimeDrawParams 为20字节，vertex slot4；slot5为ushort索引类型0/1/2，
UE间接调用还先写入4字节零值。其结构均不含GPU地址；包含indexBuffer VA的
完整IRRuntimeDrawInfo不能混用。按D3D12 SetGraphicsRoot32BitConstants / Vulkan
vkCmdPushConstants保留普通常量字节，沿现有InlineLayout chunk使用count=0、
stride=固定字节长度；只接受vertex 4/5及这些确切结构。其他GPU VA字段仍逐来源
重编码，没有泛化为任意setBytes原样通过。

coverage14的索引路径沿现有Serialise_drawIndexedPrimitives提交：仅static Shared
≤64KiB、UInt16/UInt32、3个索引0/1/2、Triangle、instance1、base0、range/alignment
合法，匹配IR scalar count/instance/byte-offset/index-kind，已有PSO/阶段资源/targets
检查仍执行。这个限制用于当前极小GPU证据，不是完整UE的draw范围。

首个正例发现索引buffer只有创建字节及首个commit时的冗余CPU快照，没有独立的
Initial Contents chunk。记录哪些background创建提供了完整字节；预检仅从有此
证据的已知Shared wrapper或显式Initial Contents取得索引。帧内每个相关CPU快照
必须与其完全一致；真正修改、GPU writable绑定、退役、未来帧内buffer仍拒绝。
没有读取捕获VA所指的内存，也没有用新分配buffer的任意内容冒充捕获来源。

原生与注入的UInt16/UInt32两捕获各四次seek已验证GPU结果122/186交替和2×2像素。
阶段、root、绑定、draw/order、标量布局/参数、index type/range/bytes、CPU快照改变、
创建内容缺失、GPU索引写入等每种索引57组API+CLI反例通过。连同旧19类，
21类/674组suite全部通过，日志metal-descriptors.VwvNQb。中途发现并修复诊断slot
夹具的临时queue包装生命周期错误，另见B352。真实UE更新shader的小图形链路
两捕获/四次seek/27反例及旧八帧定向通过。库0f68436555b02175c896ccdcc783503c5662c61fbf346bcf323c88ea5b5fd802。

prepare_ue_metal_provider.py在隔离的MetalCommands.cpp的四种IR helper调用点以及
NullBuffer写入点添加诊断布局，其他引擎调用不变，不声明完整coverage。
隔离MetalRHI编译SHA256 feeed2284852266f8a43e52c428a2cec9c491056675b7a0b46bfeebd16d0e2ff，
98个导出与原模块一致；原模块ace97360…保持原样。CPU audit支持count0和普通常量
计数；真实UE新捕获待本轮queue修复验证后运行。完整UE GPU画面/MRT/pass及UI
未验收，不以这些自定义消费者结果替代。
