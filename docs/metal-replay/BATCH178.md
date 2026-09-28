# BATCH178：bounding-box 描述符分配 AS

`newAccelerationStructureWithDescriptor` 现接通单个静态 bounding-box 几何的
分配子集：包装的 box buffer、零偏移、默认 stride/几何选项、count1 和至少
24 字节。后续仍走已接通的 `buildBoundingBox` chunk；分配 chunk 只记录所需
容量。T178 原生 Metal Validation 与注入捕获均得到 AS size1536、GPU
compacted size1280；API/CLI 回放验证 write→build→write 事件链及资源身份。
18 个畸形 AS chunk 输入干净拒绝。

最新库上 35 份跨族哨兵 API/CLI、11 份×10 生命周期打开通过，resident
growth 2,080,768 bytes。驱动库/app 内嵌库 SHA-256 `9e5c56446ded…`，
T178 capture SHA-256 `b8c37cb4c5a7…`。当前 **56 处 bridge 宏调用
（另有定义 1）/18 处未处理 chunk 宏匹配（含定义 1）**：比 T177 多的
一处是 box 描述符边界的显式拒绝检查，不是功能回退；本批仍未清除任何
旧未处理 chunk。`supportsRaytracing` 仍 false。

集中 GUI 待验 140 份；未启动 qrenderdoc/Computer Use。06:26 IOGPUFamily
panic 根因未证实，完整 GPU 压力回归继续暂停。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t137 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167 t168 t169 t170 t171 t172 t173 t174 t175 t176 t177 t178
python3 util/test/metal/metal_acceleration_structure_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t178_capture.rdc
```

完整累计回归入口已纳入 T178，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=178 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
