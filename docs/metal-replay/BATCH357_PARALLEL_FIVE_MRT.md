# BATCH357：parallel pass 与五个颜色附件

2026-10-01，持续目标 active，未提交或推送。

真实 warm UE 帧 f744c413…包含六个 parallel parent/child，两个 pass 使用五个
颜色附件；颜色 store action 先为 Unknown，结束前逐项改为 Store。v20 sourced
preflight 复用已有 Native parallel encoder，校验 parent/child command buffer
归属、唯一 ID、child 关闭次序和最终 store action。五附件检查逐项匹配 pipeline
格式，旧 coverage 仍限制两个附件。当前小资源、draw 和 dispatch 限额保持。

精确库 SHA256 4deb75f8cc6a30522ce19e4530db7586da3a0deaefd4ce7028bd728933cb0d8a。
test_metal_descriptor_parallel_mrt_macos.sh 在 metal-parallel-mrt.ZcrKAA 通过：
serial/parallel × two/five 共八捕获、32 seek，每个附件的像素 186/122、普通
metadata、GPU 描述符写入、下一 pass 采样、parent/child/pass scope 正确。
223 API+CLI 反例验证未知 child、归属冲突、未关闭 child、缺失/重复/越界/晚到
store action 等拒绝路径。日志 local-m2-descriptor-replay/parallel-mrt-suite.log。

B356 精确 bfe86da…全量 308 捕获/7786 畸形输入/3080 生命周期打开已通过，
常驻内存增长 12042240 B。该全量对应 v19；上述新 parallel/MRT 定向对应 v20。
人工 UI 仍受 Mac 锁屏阻挡，qrenderdoc 内置库仍是 v19。

完整 UE 尚未 replay。实际帧还有 depth/stencil、Private 描述符资源、异步 heap
alias、间接绘制等未接通路径。仅查询 Native heap layout 的 CPU 辅助检查发现
buffer/texture 重叠；texture 创建时间与当前帧 chunk 位置不符，继续检查帧内
texture birth 记录和 seek 生命周期，不把这种重叠直接当作合法 alias 放行。
