# B479：程序化包围盒初态与绑定顺序

2026-10-05，PHASE57 持续进行，公开光追能力仍关闭。

## 实现

Vulkan/DX12 支持 AABB 几何输入保存与重建。Metal boxes 使用同一 portable AS
recipe（kind3）保存 offset/stride/count/IFT offset/opaque/duplicate/usage 与冻结输入。
Managed GPU readback 和 Shared 提交快照沿用 B477 producer/别名/容量限制；重建时
使用独立 Shared staging，primitive boxes 先于 TLAS，支持实际 sample 的 compact
boxes 和 compact TLAS 依赖。校验 count、stride、完整末尾 padding、64 MiB 上限、
6 个 finite min/max 值及 min<=max。未知 kind、额外 index/children 和损坏 payload
拒绝；现有 schema1..4 字段不变。Managed refit 和同 CB GPU producer 仍未支持。

官方 Renderer 先 setIntersectionFunctionTable 再 setComputePipelineState，Metal
允许此顺序。参照 VK descriptor set/DX12 root table 的 pipeline 独立绑定方式，
setter 保留 identity/range/stage 检查，最终 pipeline association 在所有 compute
执行入口的 dispatch 前证明。记录状态归实际 encoder，修复 dummy serializer
全局 pipeline 状态可能串到其它 encoder 的问题。nil 清空仍合法；单 table 的未知
非零 ID 保留并拒绝，不能反序列化成 nil 来绕过校验。

## 已完成测试

backend 与 app 内库 SHA256：
a35028ba687133e45983174a51404ea8572c637144c0fabe00aa0ff9802bea71。

| 检查 | 结果 |
| --- | --- |
| 正式 --stage replay 官方 triangle/procedural | native 各2次、capture、离线像素逐字节一致；CLI 各3 loops PASS |
| 官方事件往返/EID0 | triangle44、procedural46 个事件，各3方向 PASS |
| B477 10 例 + B475 12 例、独立 Managed pointer fixture | API/CLI PASS |
| T60 device texture/sampler packet | CLI3 loops +27坏输入 PASS |
| 新官方 sample 坏输入 | triangle13 + procedural28 拒绝，无崩溃 |
| 命名正例事件选择 | 2,574（官方两场景 +22旧例 +pointer），另 triangle 重复132次 PASS |
| 固定库全量 | 308 captures /7,784坏输入 /3,080 lifecycle，PASS；growth3,784,704bytes，exit0、起止哈希一致 |
| 新 RT 生命周期 | 14 captures ×10，PASS，resident growth0bytes |
| GUI、UE 实际光追、ARC capture 提前释放 | PENDING |

坏输入含 Pointer 类型/重叠/array/access/alignment、成员 offset/index/ID、nil/wrong
AS type、residency、IFT 非零未知 ID 与错类型，以及 boxes 参数、NaN、倒置 bounds。
负例脚本初版仅查找非索引 triangle kind1，实际 sample 为 kind2，修正选择逻辑；
未把脚本选择错误记为 backend 缺陷或丢弃负例。

结果入口 build-macos-debug/ray-samples/apple-basic/b479-manifest.json、
verify-b479.log、gate-results-b479-a35028ba/replay-manifest.json；两种 output log 含每个 EID 结果。
B479 gate-results 已完整存档，避免后续新版本测试覆盖证据。
全量固定库目录 build-macos-debug/frozen-validation-a35028ba/。
首轮 full-regression.log 停在旧 T102 scalar arrayLength0 负例；原生 Metal 标量合法，
改为3-loop正例，并以数组越界替换负例（13坏输入仍通过）。第二轮
full-regression-second.log 停在 T163 将 AS residency 视为 wrong-type 的旧负例；
B478 已支持合法 AS 驻留，保留3-loop正例并改用 encoder 作为错类型（8坏输入通过）。
两轮失败没有计 PASS，测试语义修正后 full-regression-third.log 同一固定库完整通过。
摘要 manifest.json 保存库哈希、范围和常驻增长。
新 RT 生命周期结果：ray-samples/apple-basic/lifecycle-b479.log 和
lifecycle-b479-manifest.json，140次完成，常驻增长0bytes。

## 接续

sample 两场景和全量闭环已通过；B480 诊断能力/compute 表身份正在构建验证，仍需审计
ARC device/子对象释放风险。之后进入本机 Testproj 真实 UE RT，使用进程级隔离
诊断能力开关才可触发调度，不改变默认生产 capability。确认实际 AS/RT 工作、
修复 UE 触发的 IndirectInstance 等缺口，再验收 flags。未提交/推送。
