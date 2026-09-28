# BATCH138：描述符形式创建底层加速结构

`newAccelerationStructureWithDescriptor:` 现在对单个静态、无索引、Float3/stride12、默认几何选项与usage、零偏移三角形创建真实包装的 AS。捕获写入独立的 descriptor-create chunk 和实际分配大小；回放用同样大小重建并保留资源 ID。若捕获/回放设备所需大小不同，目前仍只做容量安全校验，不承诺跨 GPU 的 AS 可移植性。其他描述符明确拒绝，`supportsRaytracing` 不变。

T138 原生和注入运行均通过 Metal API Validation：descriptor 分配1536 bytes，后续真实 GPU build 写回 compacted size1280。`t138_capture.rdc` 的 API/CLI 定向回放验证创建 chunk、AS 资源类型与 build/write 事件链，write→build→write 的 Shared buffer 读回为1280→0→1280；18个畸形捕获被非崩溃拒绝。T135–T138 的原生三次、重捕获、13份定向回放与各自畸形测试均通过：

```sh
bash util/buildscripts/scripts/test_metal_as_geometry_macos.sh
RENDERDOC_METAL_LAST_TEST=138 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

GUI/Computer Use 未执行；新增待验项在 `QA_CONSOLIDATED.md`。旧 T135–T137 chunk 格式及 schema v9 未改。
最终全量回归通过：137份捕获、2800个畸形用例、1370次生命周期打开，resident
growth 7,389,184 bytes；日志 `/tmp/metal-batch138-full.log`。
