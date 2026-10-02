# B402：sourced compute indirect工作量与生命周期

B400真实自动捕获1deff873bbb017e85a99ba744f6bb6a71eb5635375420e2f93ba316de522a829，库7930a5b3、隔离UE heap64 module 0d648a90。31条逐使用记录均Native completed、GPU marker正确、原encoder/command/ordinal/source/offset一致；22次零工作量，总39616threads，最大单次22400threads。保存原始RDC，CPU审计ue-indirect.arguments-audit.json。尚无sourced coverage，不直接运行该UE帧。

v57候选按Vulkan per-use readback证据校验，保留原Native间接dispatch；groups允许零（实际UE需要），threadgroup仍按原有Native compiler/device校验，每维及单次总线程262144、帧总8Mi上限。证据缺失、不匹配、参数offset/length、buffer未出生/已alias均在frame GPU提交前拒绝。参数buffer不得是descriptor table，不得超过1MiB，标记其资源使用并复用既有生命周期追踪。

定向夹具复用descriptor producer与MRT：首次indirect groups1，GPU producer原生写参数2，第二次indirect groups2；consumer仅thread0输出，避免并发非原子写。Shared/Private参数与depth-only组合；每次捕获前恢复原始参数，Private恢复用capture之前的Native blit。加载阶段B396 Native逐次读取事件元数据继续复用。

当前实现与定向候选测试中；未执行当前候选全量/人工UI/实际UE整帧。Render indirect、完全无附件UAV、真实heap预算与历史别名仍待闭合。

2026-10-02 精确库26a25b7f214a2577355b69fecc1833042e1bbf9b63f4defef30a1d47afdc3f2f，metal-sourced-indirect.md7JM6：6组合/12 captures/48 reset cycles/150 API+CLI负例组通过。Native逐次元数据1/2以及同encoderGPU尾写0→零indirect正确；thread0结果/descriptor ordinary bytes/所有MRT和DS pixels通过。加载indirect路径复用direct已验证sourced inline-only dispatch与usage闭包，保持原Native调用和NoteDescriptorDispatch。
真实1deff873用当前库CPU打开提前拒绝frame-born/invalid GPU identity（provider未声明coverage），无frame GPU wait。全量准备中。

2026-10-02 后续v57精确26a25b7f…组合全量包含以上实现：308 captures/7786 malformed/3080 lifecycle passed，growth7520256B；结束库hash一致。日志sourced-indirect-depth-only-combined-regression.log。人工UI与真实UE整帧尚未验收，目标active。
