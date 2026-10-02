# B396：逐调用保存 compute indirect 参数（实施中）

实际 UE 的 31 个 compute indirect 分布在 12 个 compute encoder，其中存在同一 encoder
重复调用并将参数 buffer 声明为可写的情况。帧末读取源 buffer 不能代表各调用处的内容。
官方 RenderDoc Vulkan `FetchIndirectData` / `ExecuteIndirectReadback` 在调用处保存
dispatch 参数；D3D12 ExecuteIndirect 同样保存独立参数供事件修补。这是通用问题。

Metal compute encoder 内不能插入 blit。候选仅在 loading 阶段向原 GPU command stream
插入一个三 uint 的复制 kernel，用 buffer barrier 排序，保存独立 Shared readback。
然后恢复原 compute pipeline、slot 0/1 buffer/offset 或原重定位后的 inline bytes，
继续原 Native indirect dispatch。无 CPU 中途等待、无 encoder 分拆、无 CPU 替换调度。
已有全提交完成等待后才读取保存的参数并修补事件。每个 readback 有明确释放路径，
source 在调用后被覆盖或 retired/purge 不影响已保存内容。

最小夹具 `metal_compute_indirect_reuse_capture.mm`：同一 Private 参数 buffer offset16
在同一 encoder 内依次 GPU 写入 1/3/2，再分别 indirect dispatch；最后 GPU 写入0。
验证事件树三个调用分别为1/3/2，Native full output 与14次 reset/seek输出、哨兵、
pipeline binding；覆盖 inline slot1 与 batch buffers + setBufferOffset。
运行：`bash util/buildscripts/scripts/test_metal_compute_indirect_reuse_macos.sh`。

当前只构建对象，v52全量运行期间不替换库、不并行提交GPU。
尚未验证候选 GPU 结果；当前实际 UE indirect preflight 仍拒绝，不能据此宣称整帧通过。
捕获时逐调用参数证据、render indirect、实际深度/stencil及counter pass还需继续适配。

旧精确v52 5b329da9复现成立：原生输出130/count6正确；replay第一调用EID11
事件显示groups=0/1/1，预期1/1/1，helper退出6。日志
`local-m2-descriptor-replay/indirect-reuse-old-replay.log`。测试改由调用处pipeline state
定位ResourceId（创建前标签未保存），不依赖标签或资源分配顺序。

候选精确5473713adb247b202689efde2834a27fdae295d988dd18e45445ccb778860dbc：
`metal-indirect-reuse.siTtSc`两captured/14 reset seeks通过；增强
`metal-indirect-reuse.h4IoOA`四captured/28 reset seeks通过，含serial/concurrent encoder、
inline bytes以及batch-buffer/offset恢复，均开启Metal API Validation。
逐调用元数据1/3/2正确且frame末Native参数为0/1/1，output及全buffer哨兵正确。
该精确候选全量回归已启动（per-use-indirect-combined-regression.log），尚未完成。

2026-10-02 精确5473713a全量通过：308 captures / 7786 malformed cases /
3080 lifecycle opens，resident growth11026432B；运行结束哈希不变。
日志 `local-m2-descriptor-replay/per-use-indirect-combined-regression.log`。
深度候选在此期间只编译对象，待本轮结束后才链接，结果不能归给深度候选。
人工UI未验，实际UE整帧未提交，持续目标active。
