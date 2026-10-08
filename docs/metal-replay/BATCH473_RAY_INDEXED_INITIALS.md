# B473：索引 BLAS 初态与格式边界

2026-10-05，接续 [B472](BATCH472_RAY_INITIAL_INSTANCES.md)。参照 Vulkan/DX12
保存原顶点/索引构建输入、资源依赖及重建的实现。保留原索引拓扑，不转换成
另一种几何表示；支持 TLAS 引用这些底层结构，仍不增加光追内部调试。

新增 UInt16/UInt32 索引初态、非零索引偏移。索引与顶点分别在提交时冻结，
按冻结的索引安全计算最大顶点范围；校验格式、对齐、payload 长度、源缓冲区
长度及 64 MiB 完整跨度限制，再构建。Shared 写入范围重叠、Private、同 CB GPU 写入
等未证明情形继续拒绝。独立 Shared 输入已有前序完成 GPU 写入时可以读取。

schema3 增加 indexSource/indices，保留 schema1 BLAS、schema2 BLAS/TLAS 读取。
TLAS 初态允许非索引或索引 BLAS，先重建所有 primitive AS，再重建 TLAS。
每次 seek 完整重置均恢复；当前源缓冲区可以与冻结构建数据不同。

## 格式检查发现及修复

首轮索引 gate 在 `11f7d54b...` 库通过（日志另存
`build-macos-debug/metal-ray-b473-indexed-before-formatted-gate.log`）。补充 Float4、
stride=20、vertex offset=16、duplicate=false 时，原生/capture 正确，但 56-byte
紧凑顶点重建触发 Metal Validation 的完整 stride 断言，未计 PASS。

修复为上传缓冲区补零到完整末尾 stride；只补不参与几何运算的 padding。
捕获与加载预先验证源的完整 stride 范围。尺寸查询也改用冻结 staging 输入，
避免向 Metal 提交应用后来改写的顶点/索引/实例数据。旧 AS 数据无需重新捕获。
原失败帧在修复后反复事件选择通过。最终复核又补上 Shared 写入范围检查：
不同 NoCopy 资源 ID 可以指向同一内存，不能只按 ID 排除 scratch/compacted-size
写入。按 CPU 地址范围检查重叠；写资源已无法解析时保守拒绝快照。新增 alias
夹具先 build、经另一 buffer ID 向输入写 compacted size、再 build；原生/capture
结果正确，离线明确拒绝缺初态。修复后最终 gate 全部重跑。

## 最终候选与验收

backend/bundle SHA256：
`8594954e82c608b182542b170e32cf0fd68aada97795564fab0ca5e2ae3e3cb3`。
一键入口 `util/buildscripts/scripts/test_metal_ray_basics_macos.sh`，默认本批目录；
记录 `build-macos-debug/metal-ray-b473/`，总日志 `build-macos-debug/metal-ray-b473-gate.log`。

| 检查 | 结果 |
| --- | --- |
| 16 份新正例 native/capture/API/CLI | 基础、fence、帧前 BLAS/TLAS、帧内 TLAS 使用帧前 BLAS、索引 UInt16/UInt32/非零偏移、索引改写、索引 TLAS、Float4/offset/stride/padding：PASS |
| GPU 结果与源数据 | 原构建/构建后改写为 0/1/0/1；提交前改写为 0/0/0/0；当前顶点/索引字节及 AS 绑定逐事件正确 |
| 三方向 API event 选择 | 合计 1,785 次，每次先回 EID0 再选择，输出前缀、marker/函数表/AS 绑定 PASS |
| 异常输入 | 基础55 + 旧57 + BLAS26 + TLAS23 + 索引15 + Float4完整stride源范围1 = 177项 gate PASS |
| 语义边界 | 同 CB GPU producer、TLAS 子 BLAS 后续重建、不同buffer ID的Shared写入别名三份捕获结果原生正确；离线明确拒绝，无 signal/hang |
| 格式兼容 | schema1 BLAS、schema2 BLAS/TLAS 各3轮 CLI PASS；Float4 schema1 另3轮 PASS |
| 旧帧回归 | 30 份旧 API/CLI PASS；合计46份正例，非全量 |
| 生命周期 | 18 captures × 10 = 180 次；resident 增长360,448 bytes，PASS |
| 构建与静态检查 | renderdoccmd/app 构建PASS；backend/bundle一致；shell/Python语法及diff whitespace检查PASS |

最终两份基础捕获 hash：

- `table_capture.rdc`：`02df997a7cb9ceb2a31bb869d09479f26eec80a15dcd1aa1909a13aa8f197c4d`
- `fences_capture.rdc`：`98266669a27df9cf2dcf99efb6e1dc4b074edf8caf9bd6c4ca60ad77853e4787`

Float4 范围/旧 schema 检查见 `formatted-initial-invalid.log`；写入别名边界见
`background-alias-native.log`、`background-alias-capture.log`、`background-alias-reject.log`。
未跑完整308帧/累计负例/3080生命周期、GUI或UE真光追。受控 Shared 静态场景的
基本捕获与回放链可用；refittable/refit/copy/compact 初态、Private/同 CB GPU输入、
多 geometry/motion等仍有缺口，设备两项光追能力保持false。接续见 [PHASE55](PHASE55.md)。
保留工作区修改，未提交/推送。
