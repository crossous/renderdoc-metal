# BATCH77 — Asynchronous tile pipeline creation

`newRenderPipelineStateWithTileDescriptor:options:completionHandler:`现在保留调用时的
descriptor快照，向Metal传入解包后的tile function，在完成回调中包装pipeline并记录
独立的异步创建chunk。回放复用已验证的tile pipeline序列化契约；原生回调、错误和
reflection仍在应用运行时传回，离线回放不重新执行应用block。

T77夹具在发起异步创建后立即把原descriptor的BGRA8改为RGBA8、把tile function
设为nil并释放descriptor；成功的回调返回原始pipeline。捕获XML验证保存的是
BGRA8/function/options=ArgumentInfo，回放三次tile dispatch的GPU计数和后续draw
仍正确。

- 定向入口：`bash util/buildscripts/scripts/test_metal_capture_batch77_macos.sh`。
  五次原生运行、异步捕获/XML快照、三次CLI replay、Replay API、8个畸形创建chunk
  拒绝、T74–T76回归。
- 完整入口：`RENDERDOC_METAL_LAST_TEST=77 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
  最终全量通过77 captures、2035 malformed、770 lifecycle；resident growth
  2342912 bytes，日志`/tmp/metal-batch77-full-final.log`。
- T77 capture `0470cd9f7ecf…`，当前库/app `173fadcf7c8a…`。GUI未运行；集中QA
  只新增一个异步创建入口与三条tile dispatch、buffer/像素回退，不要求离线触发回调。

原始剩余标记78 bridge / 42旧chunk；T70 GPU indirect ICB range另计则79/43。
