# BATCH87 — 异步 Object/Mesh pipeline

异步mesh pipeline入口现区分有无object function。带object stage时在原生回调
包装pipeline，记录独立chunk1313和完整object/mesh/fragment函数、payload、
线程上限、grid、颜色格式与options快照；离线复用BATCH83严格重建逻辑。
原生夹具发起异步调用后把原descriptor的BGRA8改为RGBA8并清除object function，
验证回调一次且捕获仍保留调用时配置，随后GPU产生object payload驱动三角形。

- 定向：`bash util/buildscripts/scripts/test_metal_capture_batch87_macos.sh`；五次
  原生Metal Validation、捕获/XML、CLI3、Replay API中心像素、8个畸形快照拒绝
  与T82–T86回归已通过。
- 全量：`RENDERDOC_METAL_LAST_TEST=87 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`；
  87 captures、2182 malformed、870 lifecycle通过，resident growth 2015232 bytes；
  日志`/tmp/metal-batch87-full.log`。
- 捕获SHA`f6b5c2a71ac9…`、库/app SHA`1e9b10c9196a…`。
- 原始剩余**53 bridge / 42旧chunk**，T70实际缺口另计54/43。
  此批补真实语义，不声称额外清除bridge标记。GUI/Computer Use未运行，54份
  capture待集中人工。
