# B435：真实 UE 局部回放的晚编码 signal 提交前缀

2026-10-02，原始 UE 截帧 `ef026832e5f8c3591e6234b86298b8452d1c53b0c6e9bba9a45652ebf21a6415` 完整画面已经匹配捕获缩略图，但 UI 定位第一处64层 MRT 绘制 EID3718 时拒绝缺失 Resource16374 的提交前缀。该 command buffer 在目标绘制编码之后创建，却在目标提交之前提交，仅包含原始 Event12/value2512 signal，没有渲染或计算 encoder。这不是新的 GPU 数值偏差，是阻断实际局部回放的队列顺序问题。

参照本地官方 RenderDoc Vulkan `wrappers/vk_queue_funcs.cpp` 中命令编码与队列提交顺序分离的处理。coverage65 的既有缺失 copy 提交计划现在可保留已经通过事件身份、同队列、单调值和时间线预检的 signal-only 前缀。按原始 create/signal/commit 重建；未放宽 enqueue、wait、活跃 encoder 或一般 GPU 工作的限制。旧 coverage52..64 行为不变，资源快照、原生完成等待、CPU 更新恢复保持原路径。

当前库 SHA256：`f52cc7256899ba1fbc13b5c4f7cf3fb6c3c481b6835a0643c4e47b097d2e778e`。未提交或推送。

## 定向终端

`util/buildscripts/scripts/test_metal_late_signal_prefix_macos.sh`：真实 Metal 两份 capture，各四次 first/last draw 定位及 EID0 重置，索引上传和 GPU 颜色通过。修复前同一 capture 在 EID18 拒绝 Resource43 前缀；修复后以原始 signal-only 提交恢复正确顺序。`metal_late_signal_prefix_gate.py` 的未知/零身份、零值、重复 signal、commit 后 signal、资源出生前 signal 等8组 API/CLI反例在帧提交之前拒绝。日志：`build-macos-debug/local-m2-descriptor-replay/late-signal-prefix-targeted.log`，产物 `build-macos-debug/metal-late-signal-prefix.5wTzKu`。

## 同一真实 UE

使用原截帧的 metadata-only coverage65 副本 bd34d0b7。修复后立即回到该文件：EID1954、3718、3766、9669 各两次 EID0 重置/定位通过。3718 的16061/16065、3766 的16067/16069，每个64³ RGBA16Float目标读回2,097,152B，重复定位逐字节一致，无NaN/Inf。ambient SHA256 `2443e3edf3017b8762c012f94404c31fb8402112c59cad065f61e4d651e10e32`；directional `29e28aa3de9e925c263212e1844acfdb33ab0ee5dcf97b5f5d1a104cbdcbd34e`。一致性证明重放稳定，不等同于具备捕获时这四个 MRT 的独立全精度 golden。

日志和原生数据：`build-macos-debug/local-m2-descriptor-replay/ue-candidate65-event-images/`。此前完整帧31 compute indirect、17 render indirect 均与原始捕获执行点证据匹配；官方整数缩放/jpge90后 JPEG 与原捕获缩略图逐字节匹配，详见 B434。

## UI 操作验证

f52 embedded library 的 qrenderdoc 正常打开该候选，EID3718 Pipeline/Texture Viewer 成功，Outputs 正确显示两个64³目标，16061/16065最后层 Slice63可显示。切换 EID3766 显示16067/16069，16069切换Slice1成功。切回EID9669，Texture List打开16466，显示900×640 UE中文菜单、Lvl_FirstPerson、天空、黄色平台和编辑器控件。未出现先前缺失提交前缀的 fatal error。随后机器锁屏，额外UI覆盖待解锁；未声称遍历全部切片或完成全部UI功能。

## 全量独立记录

本局部修复先做定向、立即回同一UE，尚未重复全量。0d0725ff之前的全量在308份正常捕获通过后，T49 malformed Private CPU update 反例触发拒绝日志无条件调用 Private contents() 的Metal断言，未完成全量。仅Shared才打印contents的日志修复，其87个 malformed 定向验证通过。最新f52的验收全量尚待进行，不能用先前5366完整通过结果替代。

2026-10-02 19:16 验收全量最终通过：冻结f52库hash起止一致，308正常capture、7786畸形反例、3080重复开启/回收均通过，resident growth11,190,272B。日志/JSON `late-signal-frozen-combined-regression.log/json`。这是全量独立结论；随后立即回同一原始UE审计副本63c55db1检查更多局部事件，不以回归数量代替真实UE回放证据。
