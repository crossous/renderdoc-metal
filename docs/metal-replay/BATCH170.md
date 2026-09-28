# BATCH170：indexed 非 opaque 交点函数反向对照

T167–T169 的 indexed opaque 几何命中需要同类几何的反向证据。T170 使用相同
UInt16 索引 `{0,1,2}`、第二 BLAS 和 `reject_triangle` 交点函数，但保留
`opaque=false`；原生 Metal Validation 与注入捕获都得到 render Shared 值 0、
中央红色。回放仍使用原有 `buildIndexedTriangle` chunk，T167/T169 则用独立
`buildIndexedOpaqueTriangle` chunk，GPU 结果为 1、绿色，证明 opaque 语义未串线。

T170 API/CLI、12 个畸形 indexed build 输入通过；26 份跨族哨兵 API/CLI、
T35/T136/T148/T165/T167/T169/T170 共 70 次生命周期打开通过，resident growth
1,277,952 bytes。驱动库/app 内嵌库 SHA-256 仍 `73a801b87e7b…`，T170 capture
SHA-256 `d843c33ff562…`。当前 **55 bridge 调用（另有定义 1）/18 未处理
chunk 宏匹配（含定义 1）**；本批只增加验证，不宣称新旧标记减少。

集中 GUI 待验 132 份；未启动 qrenderdoc/Computer Use。06:26 IOGPUFamily
panic 根因未证实，完整 GPU 压力回归仍暂停。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167 t168 t169 t170
python3 util/test/metal/metal_indexed_opaque_triangle_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t170_capture.rdc
```

完整累计回归入口已纳入 T170，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=170 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
