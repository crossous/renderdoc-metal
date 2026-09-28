# BATCH179：双 bounding-box 描述符分配 AS

T178 的 descriptor AS 分配扩至同一几何描述符中两个静态 bounding box：
包装的 48 字节 box buffer、默认 stride/几何选项、零偏移、count2。
原生 Metal Validation 与注入捕获都得到 AS size1536、GPU compacted
size1280；API/CLI 回放检查 `buildBoundingBox` 的 `boxCount=2`、资源身份、
write→build→write 事件链。18 个畸形 AS chunk 输入干净拒绝。容量和
compacted size 与单 box 在本设备恰好相同，不能单靠两个数值判断 boxCount；
因此 API 检查明确验证 chunk 字段为 2。

当前库上 36 份跨族哨兵 API/CLI、12 份×10 生命周期打开通过，resident
growth 1,785,856 bytes。驱动库/app 内嵌库 SHA-256 `cb8f903f05ff…`，
T179 capture SHA-256 `c65cfc84faf9…`。当前仍 **56 处 bridge 宏调用
（另有定义 1）/18 处未处理 chunk 宏匹配（含定义 1）**；本批扩展既有
box descriptor 子集，不减少旧 chunk。`supportsRaytracing` 仍 false。

集中 GUI 待验 141 份；未启动 qrenderdoc/Computer Use。06:26 IOGPUFamily
panic 根因未证实，完整 GPU 压力回归继续暂停。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t137 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167 t168 t169 t170 t171 t172 t173 t174 t175 t176 t177 t178 t179
python3 util/test/metal/metal_acceleration_structure_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t179_capture.rdc
```

完整累计回归入口已纳入 T179，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=179 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
