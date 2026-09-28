# BATCH81 — 直接 Mesh thread-grid draw

接通`drawMeshThreads` bridge和新chunk1299。它独立于`drawMeshThreadgroups`，
捕获真实线程网格、object/mesh threadgroup尺寸，回放检查mesh pipeline、网格
非零与GPU上限后调用原生API，并产生MeshDispatch事件。对象线程组仍限1×1×1
（BATCH78 pipeline无object function）。当前未接间接mesh draw。

T81原生fixture使用32×1×1线程网格、1×1×1 object组、32×1×1 mesh组，
产生T78同一三角形。最初用1线程网格仅执行一个shader线程，API像素测试
正确检出三角形缺失，已更正为32线程；最终定向全通过。8个畸形网格/组
参数安全拒绝。

- 定向：`bash util/buildscripts/scripts/test_metal_capture_batch81_macos.sh`；五次原生
  Metal Validation、捕获/XML、CLI3、API像素、负例、T78–T80回归已通过。
- 全量：`RENDERDOC_METAL_LAST_TEST=81 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`；
  81 captures、2104 malformed、810 lifecycle已通过，resident growth 1212416 bytes，
  日志`/tmp/metal-batch81-full.log`。
- 捕获SHA`e6126828325b…`、库/app SHA`2eea4b68f328…`。
- 原始剩余**65 bridge / 42旧chunk**，T70实际缺口另计66/43。
  GUI/Computer Use未运行，48份capture待集中人工。
