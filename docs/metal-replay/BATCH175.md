# BATCH175：静态三角形 AS scratch buffer 非零偏移

单个非 indexed 静态三角形 build 现在接受 256 字节对齐、剩余容量足够的
`scratchBufferOffset`，仍拒绝 indexed/box/refit 的该变体。T175 原生 Metal
Validation 先验证了 scratch buffer 前缀 256 字节与实际 build；bridge 和回放
随后复现第二 BLAS 的显式 opaque 构建，render Shared 值 1、中央绿色。
已有 `buildOpaqueTriangle` chunk 原本记录 scratchOffset，本批未新增 chunk。

第一次注入捕获被 bridge 遗漏的零偏移 guard 明确拒绝，退出码 133；修正后
原生/捕获/API/CLI 通过，没有 kernel panic。13 个畸形 build 输入干净拒绝，
包括未对齐与越界 scratch 偏移；旧 T165/T174 的负例也随之扩至 13 个。
31 份跨族哨兵 API/CLI、11 份×10 生命周期打开通过，resident growth
3,653,632 bytes。驱动库/app 内嵌库 SHA-256 `1bb6f06a68b5…`，T175
capture SHA-256 `22fd0ff433ef…`。当前仍 **55 bridge 调用（另有定义 1）/
18 未处理 chunk 宏匹配（含定义 1）**。

集中 GUI 待验 137 份；未启动 qrenderdoc/Computer Use。06:26 IOGPUFamily
panic 根因仍未证实，完整 GPU 压力回归继续暂停。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167 t168 t169 t170 t171 t172 t173 t174 t175
python3 util/test/metal/metal_opaque_triangle_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t175_capture.rdc
```

完整累计回归入口已纳入 T175，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=175 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
