# BATCH169：indexed opaque 描述符分配 AS

`newAccelerationStructureWithDescriptor` 原先只允许无索引单三角形。本批放开
已验证的单三角形 index buffer 子集：包装的 buffer、零 index offset、UInt16/
UInt32、至少三个索引；保留其它 descriptor 限制。T169 以 UInt16 indexed opaque
三角形经 descriptor 分配第二 BLAS，随后使用 T167 的独立 build chunk。
原生 Metal Validation、注入捕获、API/CLI 均得到 render Shared 值 1、中央绿色。
分配 chunk 仍只序列化容量，几何语义由 build chunk 保留。

分配 chunk 的 5 个畸形输入和 indexed opaque build 的 12 个畸形输入干净拒绝；
25 份跨族哨兵 API/CLI 通过，T35/T136/T165/T167/T168/T169 共 60 次生命周期
打开通过，resident growth 1,064,960 bytes。驱动库/app 内嵌库 SHA-256
`73a801b87e7b…`，T169 capture SHA-256 `ea73a885eaaa…`。当前 55 处 bridge
调用（另有宏定义 1）/18 处未处理 chunk 宏匹配（含定义 1）；本批仅扩展既有
bridge 子集。`supportsRaytracing` 仍 false。

集中 GUI 待验 131 份；未启动 qrenderdoc/Computer Use。06:26 的 kernel panic
报告明确为 `IOGPUResource::free` 的 IOGPUFamily 断言，panicked task 是
`kernel_task`；快照中有 renderdoccmd，但不能从快照判定具体测试或资源是触发因。
完整 GPU 压力回归仍暂停。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167 t168 t169
python3 util/test/metal/metal_opaque_descriptor_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t169_capture.rdc
python3 util/test/metal/metal_indexed_opaque_triangle_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t169_capture.rdc
```

完整累计回归入口已纳入 T169，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=169 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
