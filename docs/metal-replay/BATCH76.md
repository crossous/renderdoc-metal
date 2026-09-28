# BATCH76 — Tile threadgroup memory

`MTLRenderCommandEncoder::setThreadgroupMemoryLength:offset:atIndex:`现在按原调用
顺序捕获并在回放时验证 encoder、slot、16-byte 对齐、render pass 的总内存边界和
设备容量。零长度清除允许使用 offset 0。该操作没有资源 ID，不伪造资源引用或
GPU 数据。

T76 的 tile shader 在动态 `threadgroup` 内存写入并读回 delta，再将结果原子累加
到12-byte buffer。render pass 预留32 bytes，三阶段依次绑定16 bytes于 offset
0、16、0；第一阶段 dispatch 后清除绑定。原生和捕获回放均在 Metal API Validation
下运行，自动检查三个 GPU 计数、最终 draw 像素与事件回退。

- 定向入口：`bash util/buildscripts/scripts/test_metal_capture_batch76_macos.sh`。
  五次原生运行、捕获/XML 偏移与清除断言、三次 CLI replay、Replay API、
  8 个畸形 chunk 拒绝、T74/T75 回归。
- 完整入口：`RENDERDOC_METAL_LAST_TEST=76 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
  全量通过76 captures、2027 malformed、760 lifecycle；resident growth
  1687552 bytes，日志`/tmp/metal-batch76-full.log`。
- UI 未测试。集中 QA 只需额外打开 T76，核对四次内存绑定与三条 tile dispatch，
  以及 buffer 的 `tileCount×1/×2/×3` 和末→首→中→末 seek。

原始剩余标记预计79 bridge / 42旧chunk；T70功能缺口另计为80/43。
接下来的缺口主要是 mesh/object pipeline、函数表、ray tracing、sparse/alias 生命周期等
完整系统族；不能用单纯移除透传标记替代捕获回放。
