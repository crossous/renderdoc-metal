# BATCH139：等容量 AS Copy

在 T135–T138 底层 AS build 链上接通 `copyAccelerationStructure:toAccelerationStructure:`：两个不同的包装 AS，目标容量不少于源容量，capture 保存两端资源依赖；回放前校验资源身份、类型和容量，生成 Copy action。T139 同一命令 encoder 里执行 build → copy → 对目标写 compacted size，原生与注入 Metal Validation 的 GPU 写回均为 1280。API/CLI 回放验证两个 AS ID、事件顺序、目标输出 buffer 的 write→copy→write seek 为1280→0→1280；22个畸形捕获均非崩溃拒绝。

本批**不是**压缩复制：目标仍分配1536 bytes，不声称 `copyAndCompactAccelerationStructure:` 已接通。AS 内部几何尚无 shader 射线查询的像素级验证，`supportsRaytracing` 保持 false。UI/Computer Use 未执行，新增项在 `QA_CONSOLIDATED.md`。

终端复验入口：

```sh
bash util/buildscripts/scripts/test_metal_as_geometry_macos.sh
RENDERDOC_METAL_LAST_TEST=139 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

最终全量通过：138份捕获、2822个畸形用例、1380次生命周期打开；resident
growth 7,159,808 bytes，日志 `/tmp/metal-batch139-full.log`。
