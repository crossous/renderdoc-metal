# BATCH298：同一 BLAS 的更多 TLAS 实例

同源 BLAS 的 `buildInstances` 原有限制为四实例。本批把有界上限扩至
65536：bridge、descriptor 分配、捕获与回放共用范围；回放前先验证
`count × 64` 快照字节数、Shared buffer 容量及每项 AS 索引0、mask、
options、表偏移和有限变换。三个/四个不同 BLAS 的上限仍为四，
不能把本批误读为不同子结构数量也已放宽。超过65536仍明确拒绝。

T298 构建同一 BLAS 的八实例 TLAS；原生 Metal Validation、注入截帧、
逐事件 API readback 和 CLI replay 均通过，八个 GPU ray 输出从
`7/9/11/13/15/17/19/21` 依次变为1。捕获 SHA-256 前缀
`00ed80a5d7cc`。23 例畸形输入被干净拒绝，含超出上限计数；
T291 原有四实例负例23例再验通过。九份跨族哨兵、T142/T143/T144、
T290/T291、T294–297 加 T298 共19份定向 API/CLI 回放通过。
T35 基准加 T298 的 2×10 生命周期打开通过，resident 增长688128 bytes。

库与 app 内嵌库 SHA-256 均为 `e67638f19213…`，GUI executable
`fe8bcf852b68…`，只构建未启动。GUI 待验累计 **260 份**。
bridge/chunk 原始防御宏匹配仍 **59/17**；本批扩大已有 bridge 守卫及
`buildInstances` chunk 的真实能力。上限处只做负例，没有对65536实例
进行 GPU 压测；此前来源未明的 IOGPU panic 后仍不运行长时全量压力
回归。累计脚本登记 `LAST_TEST=298`，本轮未执行其全量模式。
