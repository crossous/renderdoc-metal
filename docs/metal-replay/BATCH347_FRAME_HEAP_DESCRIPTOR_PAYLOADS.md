# BATCH347：帧内 heap placement 描述符 payload

2026-10-01，持续目标 active，未提交或推送。

v11 在 v10 两提交/完成等待基础上支持帧内 Shared placement buffer/table。
复用 WrappedMTLHeap 的 native 创建与回跳释放/范围复位机制，预检记录未来资源的
heap/length/options/offset，查询原生 heapBufferSizeAndAlign 校验边界与对齐；拒绝
初始 placement overlap 及帧内 overlap，保持 64KiB buffer /1MiB 总预算。静态
检查只读取已建立的 heap 元数据，不提前建立帧内 native buffer。来源与 table
必须在创建后声明，inline 与 GPU producer 仍使用明确 ResourceId。

小用例第一次 compute/commit/wait 后新建 heap payload，GPU 更新 Texture4 目标，
然后 compute 与 vertex/fragment 消费。两捕获 offset 各异，四轮 seek 释放/重建
原生对象，GPU 字节/像素保持 122→186、186→122。21 组 heap/range/birth/order
API+CLI 反例，加已有图形和提交反例均在 frame GPU 提交前拒绝。

描述符整套十七类/453 组通过，日志 metal-descriptors.f877RU，库
52b42a31d3642ac2c01fac3ff677fce0a3d556c91264a9bbb4acd66a993e8e04。
完整 UE 仍未验收；实际 frame 有 726 个 heap buffer 出生，而捕获 heap 总声明
4672MiB，不可直接放宽 tiny 预算执行。下一项无捕获端 CPU wait 的同队列提交
及 Shared 快照/GPU写入的同步顺序，然后 alias/索引与间接绘制。
全量未跑，新增人工 UI 未验，持续推进。
