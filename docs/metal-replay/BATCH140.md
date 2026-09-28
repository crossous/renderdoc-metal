# BATCH140：GPU尺寸约束的AS压缩复制

`copyAndCompactAccelerationStructure:toAccelerationStructure:` 现支持一个明确有序的底层AS路径：先在独立command buffer构建单个静态无索引三角形源AS、写出Shared buffer中的GPU compacted size；在目标容量按该真实值分配后，再由另一command buffer压缩复制。捕获保存源/目标AS、GPU尺寸buffer及期望值。回放在提交压缩复制前检查资源类型、该次build与尺寸写回属于同一前序command buffer、Shared读回范围与类型、实际GPU值和目标容量；不满足即明确拒绝。不同设备算出的压缩尺寸不同时也安全拒绝，不声称跨GPU可移植。

T140 原生与注入 Metal Validation：源1536 bytes、GPU compacted size1280、目标确实分配1280 bytes，复制后目标再次报告1280。API/CLI回放检查 build→源尺寸写回→compact copy→目标尺寸写回、资源ID和末→compact→末seek；32个畸形捕获（含目标过小、错误GPU尺寸值/缓冲区/类型与错误encoder时序）被非崩溃拒绝。它仍不是完整光追：实例结构、refit、shader消费及intersection table缺失，`supportsRaytracing`保持false。

终端复验：

```sh
bash util/buildscripts/scripts/test_metal_as_geometry_macos.sh
RENDERDOC_METAL_LAST_TEST=140 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

GUI/Computer Use未运行；新增最小人工检查已并入`QA_CONSOLIDATED.md`。旧chunk及schema v9未改。
最终全量回归通过：139份捕获、2854个畸形用例、1390次生命周期打开，resident
growth 5,586,944 bytes；日志 `/tmp/metal-batch140-final2.log`。
