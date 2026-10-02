# BATCH338：计算 producer 来源关联与真实 UE 重截

2026-10-01；持续目标 active，未提交、推送或完整 UE GPU replay。

## 计算更新复用已有 shadow

新增chunk1407 `MTLBuffer::DescriptorSlotProducer`，包含destination+offset、
compute encoder、typed payload+offset。capture只解析已知wrapper，不解引用未知指针。
coverage v6在B337基础上验证producer已执行一次1×1×1dispatch，source/destination
live slot及类型一致，后续GPUExpected的24字节与来源一致，Binding也一致；
元数据不CPU重写GPU目标。仍一个CB、≤4dispatch、≤2更新、2×2clear和静态小预算。

小例为实际compute读取source packet、写destination，再由下一shader消费。
两捕获41→80和80→41（第二捕获以GPU结果为初始状态），GPU字节/地址/普通
常量/DEADBEEF/四轮seek/clear像素全部通过。24个计算路径API+CLI负例在GPU前
拒绝。九类tiny共123负例及slot冻结诊断通过，日志metal-descriptors.gDYRaT；
旧t01/t02/t09/t11/t12/t35 API+CLI通过，targeted-sourced-compute-final.log。
当前库SHA256 `78130e17647d1a8fa6ed36adf56534890d5c638b1dee3fdbcb1112c81844e0ef`。

## UE局部provider与CPU证据

依据官方5.8.3安装源码的FlushPendingDescriptorUpdates及
Engine/Shaders/Private/UpdateDescriptorHandle.usf：1线程按Indices循环复制24-byte
IRDescriptorTableEntry。Inline hook保存当前compute encoder，GPUValues在dispatch后
关联每条实际upload suballocation+Idx*24与主表destination slot。主表在背景
声明GPU写入，使普通Shared CPU diff不误覆盖GPU输出；诊断声明不授予coverage。

新隔离module `provider-build-producers/libUnrealEditor-MetalRHI.dylib` SHA256
`ff6e624bd2ddafb121137a00dd3dd80794a21ea07e578396627dc3e2b3b0fa4c`，98导出不变，
仅当前UE进程DYLD加载，不替换Engine文件。会话20261001-063500，End=1；
SIGTERM此次owned PID8025，launcher exit0。原有13条AutomationTest启动错误
与旧基线相同，未见新provider ensure/Warning。

新capture 16,238,323bytes，SHA256
`5039ca0f18a3a812047892c22f1bee780aae71f57a5622f0425338595a35169b`，保护副本
Saved/RenderDocMetalCaptures/UE58_NewMap_producers_5039ca0f.rdc；本地文件
local-m2-descriptor-replay/ue-producers.rdc、zip.xml、audit.json。

- 46,942chunks，scope25,177；1,477/1,477活槽与Initial Contents完全匹配。
- **118 producer /118 GPUExpected raw payload和source binding匹配，0问题**。
- lifetime/frame value epoch/inline issues均0；880帧内slot fields、3,569非零
  inline fields有显式来源；408compute+780vertex+376fragment声明setter。
- 4,075历史已退役来源身份不保留，不能充当执行时liveness/alias或GPU输出证明。

这里只证明真实UE记录的输入/provenance闭合，尚未证明UE shader GPU目的字节，
更未证明完整图像/MRT/pass scope。provider仍不声明任何完整coverage。

## 对齐真实UE类型，继续适配

复核RHIDefinitions.h后确认UE descriptor枚举：Buffer0/1、typed buffer2/3、
Texture4/5、CBV6、Sampler7。**v4–v6 tiny夹具的4/5只是旧sourced buffer契约标记，
并未接入真实UE；不能直接用其标记解释UE的4/5 texture条目。** 后续新契约将
按真实类型及显式source kind同时处理buffer VA、texture ID、sampler ID，保留
旧小例兼容。还需frame births、alias生命周期、render stage及UE实际GPU验收。

全量未跑；本批人工UI未验，旧成功UI不升级；完整UE目标尚未完成。
