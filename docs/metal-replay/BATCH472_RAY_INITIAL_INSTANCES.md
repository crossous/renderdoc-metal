# B472：帧前 TLAS 与底层结构依赖恢复

2026-10-05，接续 [B471](BATCH471_RAY_INITIAL_BUILD_INPUTS.md)。参照 Vulkan
AS 重建与 DX12 TLAS 初态恢复：保留实例描述符和 BLAS 资源身份，在回放设备
重建依赖；没有增加内部遍历或交点 shader 调试。

新增 default instance descriptor 的初态保存，支持 1–65,536 个实例、1–4 个
唯一非索引三角形 BLAS、重复子结构引用、有限仿射变换；沿用既有 options=0、
mask=0xff、instance table offset=0 和独立 Shared 数据边界。参数在编码时
固定，实例字节在提交时固定。需要底层结构已经完成，且 TLAS 构建至截帧之间
没有重建底层结构；用不可变 build-record 身份检查后者。被改写的实例缓冲区
与顶点缓冲区各自保留当前初态，AS 从原构建输入重建。

初态 schema2 增加 kind/children，schema1 BLAS 仍可读。每轮先验证所有子初态
存在，再重建 BLAS、最后 TLAS；不依赖资源 ID 或初态 chunk 的排列顺序。
AS 引用登记补齐底层 AS 与构建缓冲区；帧内 TLAS 可消费帧前 BLAS。

## 最终候选与验收

backend/bundle SHA256：
`ad0ee840c1c6b37adec0eb4bdde558b995f8120b3f4b3d8b00de02faabddbea2`。
入口 `util/buildscripts/scripts/test_metal_ray_basics_macos.sh`；结果目录
`build-macos-debug/metal-ray-b472`，总日志 `build-macos-debug/metal-ray-b472-gate.log`。

| 检查 | 结果 |
| --- | --- |
| 五种新 TLAS native/capture/API/CLI | 单实例、2 BLAS/3 重复实例、构建后同时改写实例/顶点、提交前改写实例、帧内 TLAS 使用帧前 BLAS：PASS |
| GPU 结果 | 普通/构建后改写 0/1/0/1；提交前改写 0/0/0/0 |
| 三方向 API event 选择 | 四份帧前 TLAS 各 34×3；帧内 TLAS 41×3，PASS |
| 初态异常输入 | BLAS 26 + TLAS 23；含无效子 ID/type、自引用/重复/数量、缺子初态、NaN、options/mask/table/index、payload 大小，PASS |
| 子 BLAS 重建边界 | 原生与捕获仍正确；离线缺 TLAS 初态时明确拒绝，不静默换成新构建版本 |
| schema1 | 兼容转换后 CLI 3 轮 PASS |
| B469–471、旧帧 | 既有五份新正例、30 份旧 API/CLI、55+57 项旧异常输入 PASS |
| 生命周期 | 12 captures × 10 = 120 次；resident 增长 425,984 bytes，PASS |
| 构建 | renderdoccmd/app PASS，backend/bundle 一致 |

初轮负例测试将“缺子初态”限定为 chunk 错误文本，但正确实现是在初态整体校验
时返回 `Invalid Metal initial CPU buffer data`；修正测试期望后，完整 gate 重跑通过。
未跑全量 308 帧、GUI 或真实 UE 光追；设备两项光追能力仍 false。
后续继续索引 BLAS、refittable/copy/compact 初态与 GPU/Private 输入。
