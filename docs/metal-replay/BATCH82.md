# BATCH82 — 异步 Mesh pipeline

接通异步`newRenderPipelineStateWithMeshDescriptor:options:completionHandler:` bridge
及新chunk1300。调用时复制descriptor、在原生调用中使用解包后的函数，回调中包装
pipeline并记录完整快照；离线回放复用BATCH78同步创建的严格descriptor校验。
原生夹具在发起调用后把原descriptor的BGRA8改为RGBA8并清除mesh function，
验证捕获仍保留调用时BGRA8/函数/options；回调只发生一次。

- 定向：`bash util/buildscripts/scripts/test_metal_capture_batch82_macos.sh`；五次原生
  Metal Validation、捕获/XML、CLI3、API三角形像素、8个畸形pipeline快照拒绝及
  T78–T81回归已通过。
- 全量：`RENDERDOC_METAL_LAST_TEST=82 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`；
  82 captures、2112 malformed、820 lifecycle已通过，resident growth 1982464 bytes，
  日志`/tmp/metal-batch82-full.log`。
- 捕获SHA`5c89deaf9996…`、库/app SHA`8d09e99f81c5…`。
- 原始剩余**64 bridge / 42旧chunk**；T70实际缺口另计65/43。
  GUI/Computer Use未运行，49份capture待集中人工。
