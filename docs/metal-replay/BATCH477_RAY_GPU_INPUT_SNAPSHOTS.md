# B477：光追 GPU 输入快照与官方 sample 捕获

2026-10-05，PHASE57 进行中；两项公开光追能力仍 false。

## 实现与跨 API 依据

参照 Vulkan vk_acceleration_structure.cpp 的执行点输入复制，以及
D3D12 d3d12_initstate.cpp 的 AS 输入保存/依赖重建，保留 portable build recipe，
不复制不透明 AS 分配。Managed/Private 顶点与索引通过同一 AS-only command buffer
末尾的 Shared readback 保存 GPU 实际输入。允许同 queue 的先前已提交 GPU producer，
按完成状态证明快照；禁止其它 queue 的未完成 producer、未提交保留、同提交输入写入、
heap/NoCopy 别名和超过 64 MiB 的输入。前台同提交 GPU producer、Managed refit
及 Private TLAS 构建仍未支持。本轮没有扩大序列化 schema。

后台 Managed 默认实例 TLAS 构建与压缩可执行。按 Vulkan/DX12 等价实例字段支持
低 8 位 mask、cull/winding/opaque/nonopaque 标志和现有 0..31 IFT offset；未知标志、
冲突 opaque/nonopaque、越界子 AS 与非有限变换仍拒绝。

GPU 改写回归暴露了普通 Managed buffer 初态未恢复的旧缺口：加载和每次 seek
现在恢复已捕获 Managed 初态，并在地址重定位后 didModifyRange 发布 GPU 副本。
AS 使用历史冻结输入，普通缓冲恢复捕获开始时的实际值，两者分别验证。

官方 sample 探针第四帧提供有限离屏 drawable，实际走原 Renderer 呈现，解决
EndCapture 没有 backbuffer 的拒绝；未修改固定版本 Apple sample 源码。

## 测试结果

被测 backend SHA256：582c068b8928eb5c2bfb270427bf10d81ac338970fe2fd8e3382008fd5ed419b。
CLI 和 app 构建成功；本批没有保存 app 内库独立 SHA，因此不声称该项 hash 验收。

| 检查 | 结果 |
| --- | --- |
| 10 新例 native/capture/API + CLI 各 3 loops | PASS |
| 12 B475 旧 background compact/copy/index/TLAS 例 API + CLI | PASS |
| 前后往返/EID0 选择 | 2,262 PASS |
| 损坏 TLAS 初态 | 24 拒绝，无崩溃 |
| schema2 BLAS/TLAS | 3 loops PASS |
| 官方 triangle/procedural 两场景 native 各 2 次、capture-probe | PASS；输出逐字节一致 |
| 官方离线回放 | FAIL 于 device-created pointer argument encoder，下一批处理中 |
| 全量 308/7786/3080、GUI、生命周期、UE 实际光追 | NOT RUN |

新例包含实例 flags、forceOpaque、mask0、实例 IFT offset1；Managed 三角形/索引、
重复多子 TLAS、索引重复 TLAS、Managed TLAS compact；以及 AS 完成后 GPU 改写
顶点/索引为零但历史 AS 仍命中的例子。原生与捕获成功日志和最终库重复回放独立保存。
测试入口第一次将旧 compact_capture 误送入 table oracle 返回 2，已按 fixture 类型
改为只使用匹配的 12 个 background table 旧例；没有将该调用记为产品回归通过。

产物：build-macos-debug/metal-ray-b477/final-manifest.json、final-gate.log、各项日志；
captures/metal-ray-b477；官方 gate-results/capture-probe-manifest.json。
官方输出 SHA256：triangle e64da5d75d7a53f5c93843a58d796f55dbedcbecc97f063e43eb3cb93ce0bebb；
procedural 1a2c5f07731f303b9c4cc7607be3654ae716214b9bcf4e1a722019b7464ca3c4。

## 接续

补只读指针参数描述与 Managed argument packet 的资源地址重编码，运行正式 sample
离线像素 oracle；随后补程序化 boxes 初态、compact TLAS 依赖。ARC 析构风险仍待。
全量与 UE 未验收之前不开启 capability。保留现有未提交修改，未提交/推送。
