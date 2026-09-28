# BATCH167：indexed opaque 三角形 AS build

将单个 UInt16/UInt32 indexed triangle build 的显式 `opaque=true` 子集接入 bridge，
新增 `buildIndexedOpaqueTriangle` chunk；旧 `buildIndexedTriangle` chunk 继续表示默认
非 opaque 几何，不改变已有 capture 语义。T167 用 UInt16 索引 `{0,1,2}`、第二
BLAS 和自定义 `reject_triangle` 形成 GPU 对照：原生 Metal Validation 和注入捕获
均命中，render Shared 值 1、中央绿色；T148 的非 opaque 路径仍拒绝为 0。

T167 API/CLI 回放通过，12 个畸形 indexed opaque 输入干净拒绝；23 份跨族哨兵
API/CLI 通过，T35/T136/T165/T167 共 40 次生命周期打开通过，resident growth
2,342,912 bytes。bridge 仍 55 处调用（另有宏定义 1），旧未处理 chunk 宏匹配
仍 18（含定义 1）；本批扩展既有 bridge 子集并新增 chunk，不将它误算为旧标记
清零。`supportsRaytracing` 仍 false。

当前库/app 内嵌库 SHA-256 `9a7f700ace5c…`，T167 capture SHA-256
`198c95ce8dbf…`。集中 GUI 待验 129 份；未启动 qrenderdoc/Computer Use。
06:26 IOGPUFamily panic 后未再见第二份 panic 报告；根因不确定，完整 GPU
压力回归仍暂停。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167
python3 util/test/metal/metal_indexed_opaque_triangle_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t167_capture.rdc
```

完整累计回归入口已纳入 T167，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=167 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
