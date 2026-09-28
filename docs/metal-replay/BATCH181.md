# BATCH181：有界多非 indexed 三角形描述符分配

`newAccelerationStructureWithDescriptor` 的单个静态非 indexed 三角形几何
不再固定 count1，现按 `1 ≤ triangleCount ≤ 1,000,000` 与剩余顶点 buffer
长度/36 校验；indexed descriptor 暂仍限定 count1。T181 用两个 Float3
三角形、72 字节顶点 buffer，经原生 Metal Validation 与注入捕获得到 AS
size1536、GPU compacted size1280。捕获使用
`buildNonOpaqueTriangle.triangleCount=2`，API/CLI 回放检查该字段、动作名、
资源身份及 write→build→write 事件链。18 个畸形 AS chunk 输入干净拒绝。

测试器最初只识别旧普通 triangle build 名称，导致第一次 API 检查失败；
CLI 回放已成功。补充识别正确的 `buildNonOpaqueTriangle` 后，T181 及旧
T135/T138/T179/T180 定向通过。最新库上 38 份跨族哨兵 API/CLI、
14 份×10 生命周期打开通过，resident growth 3,571,712 bytes。
驱动库/app 内嵌库 SHA-256 `44a338d15489…`，T181 capture SHA-256
`1ffbca9bdc9b…`。当前仍 **56 bridge 宏调用（另有定义 1）/
18 未处理 chunk 宏匹配（含定义 1）**；本批扩展既有 descriptor 子集。

集中 GUI 待验 143 份；未启动 qrenderdoc/Computer Use。06:26 IOGPUFamily
panic 根因未证实，完整 GPU 压力回归继续暂停。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t137 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167 t168 t169 t170 t171 t172 t173 t174 t175 t176 t177 t178 t179 t180 t181
python3 util/test/metal/metal_acceleration_structure_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t181_capture.rdc
```

完整累计回归入口已纳入 T181，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=181 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
