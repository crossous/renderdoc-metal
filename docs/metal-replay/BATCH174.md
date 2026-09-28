# BATCH174：非零顶点偏移的 descriptor AS 分配

在 T173 静态三角形 build 的非零顶点偏移闭环基础上，T174 让第二 BLAS 由
`newAccelerationStructureWithDescriptor` 分配。该入口现在接受 4 字节对齐、
buffer 内足够 36 字节的非 indexed 单三角形顶点偏移；indexed descriptor
仍要求 offset0。T174 使用 16 字节干扰前缀、vertexOffset16、显式 opaque
几何；原生 Metal Validation、注入捕获、API/CLI 回放均得到 Shared 值 1、
中央绿色。分配 chunk 继续只保存容量，实际偏移由 build chunk 记录。

5 个畸形分配 chunk 与 12 个畸形 build chunk 输入干净拒绝；30 份跨族哨兵
API/CLI、10 份×10 生命周期打开通过，resident growth 1,884,160 bytes。
驱动库/app 内嵌库 SHA-256 `8a9f713e6364…`，T174 capture SHA-256
`ed91b6c2ad4b…`。当前仍 **55 bridge 调用（另有定义 1）/18 未处理 chunk
宏匹配（含定义 1）**，因为本批扩展既有 descriptor 子集。

集中 GUI 待验 136 份；未启动 qrenderdoc/Computer Use。06:26 IOGPUFamily
panic 根因未证实，完整 GPU 压力回归继续暂停。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167 t168 t169 t170 t171 t172 t173 t174
python3 util/test/metal/metal_opaque_descriptor_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t174_capture.rdc
python3 util/test/metal/metal_opaque_triangle_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t174_capture.rdc
```

完整累计回归入口已纳入 T174，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=174 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
