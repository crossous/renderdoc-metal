# BATCH180：有界多 bounding-box 描述符分配

T179 的双 box 描述符分配不再硬编码最多两个。该入口现在对单个静态 box
几何按 `1 ≤ boundingBoxCount ≤ 1,000,000`、`count ≤ buffer.length/24`
和默认 stride/选项校验；build wrapper 的容量检查仍独立生效。T180 用同一
描述符中的三个 box、72 字节 buffer 实测，原生 Metal Validation 与注入
捕获均得到 AS size1536、GPU compacted size1280；API/CLI 回放确认
`buildBoundingBox.boxCount=3`。18 个畸形 AS chunk 输入干净拒绝。

37 份跨族哨兵 API/CLI、13 份×10 生命周期打开通过，resident growth
2,129,920 bytes。驱动库/app 内嵌库 SHA-256 `ba68ff520274…`，T180
capture SHA-256 `dcb164b33c4e…`。当前仍 **56 bridge 宏调用（另有定义 1）/
18 未处理 chunk 宏匹配（含定义 1）**；本批扩展已有 descriptor 的数量
边界，不减少旧 chunk。`supportsRaytracing` 仍 false。

集中 GUI 待验 142 份；未启动 qrenderdoc/Computer Use。06:26 IOGPUFamily
panic 根因未证实，完整 GPU 压力回归继续暂停。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t137 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167 t168 t169 t170 t171 t172 t173 t174 t175 t176 t177 t178 t179 t180
python3 util/test/metal/metal_acceleration_structure_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t180_capture.rdc
```

完整累计回归入口已纳入 T180，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=180 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
