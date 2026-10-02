# BATCH348：异步同队列提交与 Shared 快照同步

2026-10-01，持续目标 active，未提交或推送。

v12 接通同一队列先 commit 后继续编码下一提交的路径；捕获不要求应用调用
waitUntilCompleted。Replay 的 CPU Shared 快照恢复先等待此前已提交的原生
command buffer，避免 CPU memcpy 与前一 GPU 写入竞争。Loading 和 active seek
均执行该同步。借鉴 Vulkan coherent-memory 快照与等待、D3D12 GPU sync 的
现有机制；当前有限路径明确拒绝跨队列、Event/Fence 依赖。

小用例两份捕获各含两个 commit、零 waitUntilCompleted。第一提交与第二提交
累加输出严格为 308；描述符 GPU 更新后的四轮 seek 图像为 186/122，并检查
普通 metadata、heap 帧内重建。27 组异步提交反例及 27 组图形反例通过。

描述符整套十八类/507 组 API+CLI 反例通过，日志 metal-descriptors.Zo6hVz。
实际 UE 更新 shader 接入小图形消费者重跑 metal-ue-graphics.nmWxNn 通过，
旧六帧 t01/t02/t09/t11/t12/t35 定向通过（async-targeted.log）。库
b2edab43e7482bbcc6a827622105471a240bcd87770dcd0346f68764ae87045d。
全量回归未跑，最新人工 UI 未验，完整 UE 帧 GPU replay 尚未完成。
下一项 frame heap alias 的资源退役、同地址重新分配与多次 seek。
