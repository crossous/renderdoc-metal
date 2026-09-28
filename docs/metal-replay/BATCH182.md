# BATCH182：有界多 indexed 三角形描述符分配

`newAccelerationStructureWithDescriptor` 的静态 UInt16/UInt32 indexed
triangle 子集不再固定 count1，现按三索引/三角形验证 index buffer 的
实际字节容量，并保留 triangleCount 上限 1,000,000、零顶点/index offset
等边界。T182 使用两组三角形、72 字节顶点 buffer 和六个 UInt16 索引，
原生 Metal Validation 与注入捕获均得到 AS size1536、GPU compacted
size1280；API/CLI 回放检查 `buildIndexedTriangle.triangleCount=2`、索引
类型和资源身份。20 个畸形 AS chunk 输入干净拒绝，其中新增把 count 从 2
篡改为 3，以明确覆盖 index buffer 字节边界。

最新库上 39 份跨族哨兵 API/CLI、15 份×10 生命周期打开通过，resident
growth 2,293,760 bytes。驱动库/app 内嵌库 SHA-256 `1a8c4816af5a…`，
T182 capture SHA-256 `014076f1de47…`。当前仍 **56 bridge 宏调用
（另有定义 1）/18 未处理 chunk 宏匹配（含定义 1）**；本批扩展已有
descriptor 子集。`supportsRaytracing` 仍 false。

集中 GUI 待验 144 份；未启动 qrenderdoc/Computer Use。06:26 IOGPUFamily
panic 根因未证实，完整 GPU 压力回归继续暂停。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t137 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167 t168 t169 t170 t171 t172 t173 t174 t175 t176 t177 t178 t179 t180 t181 t182
python3 util/test/metal/metal_acceleration_structure_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t182_capture.rdc
```

完整累计回归入口已纳入 T182，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=182 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
