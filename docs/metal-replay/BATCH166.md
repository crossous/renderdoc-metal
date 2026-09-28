# BATCH166：显式 opaque 描述符分配 AS

T166 将 T165 的第二个 BLAS 改用 `newAccelerationStructureWithDescriptor` 分配，
描述符中的单个无索引三角形显式设置 `opaque=true`。bridge 现在接受此分配子集；
wrapper 仍按已验证的分配容量序列化，实际几何语义由后续
`buildOpaqueTriangle` chunk 保留。自定义 `reject_triangle` 仍在 fragment
intersection function table 中，但 opaque 几何绕过它；原生 Metal Validation、
注入捕获、API/CLI 回放均得到 Shared 值 1 和中央绿色。此批不表示任意 AS 描述符、
索引几何或 refit+opaque 组合已受支持。

5 个畸形 descriptor 分配输入及 11 个畸形 opaque build 输入均干净拒绝；
22 份跨族 capture 定向 API/CLI 回放通过，T35/T138/T165/T166 共 40 次
生命周期打开通过，resident growth 819,200 bytes。原始 bridge 宏匹配仍为
55 处调用（另有宏定义 1），旧未处理 chunk 宏匹配仍 18（含宏定义 1）：
本批扩展既有 bridge 子集，不计为整条 bridge 或旧 chunk 消除。
此前 BATCH164/165 的 54 是漏算 1 处的记录误差，并非本批新增拒绝分支。
`supportsRaytracing` 仍为 false。

当前库及 app 内嵌库 SHA-256 `a0d4076aeb32…`，T166 capture SHA-256
`a540260cce00…`。集中 GUI 待验增加至 128 份；未启动 GUI/Computer Use。
06:26 的 IOGPUFamily kernel panic 根因仍未证实，此后未重跑完整 GPU 压力回归。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166
python3 util/test/metal/metal_opaque_descriptor_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t166_capture.rdc
python3 util/test/metal/metal_opaque_triangle_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t166_capture.rdc
```

完整累计回归入口已纳入 T166，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=166 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
