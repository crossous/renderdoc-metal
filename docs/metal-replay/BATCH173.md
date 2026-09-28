# BATCH173：静态三角形 AS 顶点 buffer 非零偏移

单个非 indexed 静态三角形 build 现在接受 4 字节对齐、落在 vertex buffer
内的非零 `vertexBufferOffset`，仍拒绝 indexed/refit 的该变体。T173 在第二
BLAS 的顶点 buffer 前放 16 字节干扰数据，描述符 offset16 指向真实三角形；
自定义交点函数被 opaque 几何绕过，原生 Metal Validation、注入捕获、API/CLI
回放均得到 Shared 值 1、中央绿色。已有 `buildOpaqueTriangle` chunk 原本就含
`vertexOffset` 字段，本批扩展校验与 bridge 子集，不新增 chunk。

12 个畸形 build 输入干净拒绝，包括未对齐偏移与越界偏移；旧 T165/T166
的负例也各扩至 12 个并复验。29 份跨族哨兵 API/CLI、9 份×10 生命周期
打开通过，resident growth 1,851,392 bytes。驱动库/app 内嵌库 SHA-256
`b5c8dff22b37…`，T173 capture SHA-256 `a52f95b7871e…`。仍为 55 处
bridge 调用（另有定义 1）/18 处未处理 chunk 宏匹配（含定义 1）。

集中 GUI 待验 135 份；未启动 qrenderdoc/Computer Use。06:26 IOGPUFamily
panic 根因未证实，完整 GPU 压力回归继续暂停。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167 t168 t169 t170 t171 t172 t173
python3 util/test/metal/metal_opaque_triangle_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t173_capture.rdc
```

完整累计回归入口已纳入 T173，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=173 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
