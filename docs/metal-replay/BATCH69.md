# BATCH69：Intersection Function 与同进程 SharedEvent Handle

承接 BATCH67–68 的99 bridge / 63旧chunk。本批接通 `MTLLibrary` 同步/异步
`newIntersectionFunctionWithDescriptor`（共2个bridge、1个旧chunk），以及
`MTLDevice::newSharedEventWithHandle`（1个bridge、1个旧chunk）。当前剩余
**96 bridge / 61旧chunk**，Max1284不变。没有把 ray tracing 或跨进程同步宣称为已支持。

T55扩展：两个真实 intersection function 分别由同步/异步API创建，链接到两个compute
pipeline。函数快照、依赖记录、pipeline linked-functions回放在原生Metal Validation、
捕获、API/CLI回放与前后seek中通过；增加7个损坏的函数快照负例。仅允许最多32个
intersection函数链接，binary/private/groups等更广泛链接形式仍明确拒绝；
`supportsRaytracing`仍返回false，完整ray tracing功能尚未接通。

T69：同一进程从SharedEvent导出handle后导入为独立资源，跨两queue的第一条signal、
导入别名wait/signal、源event后续wait使用同一GPU时间线。回放每个epoch先重建源event，
再从该源导出并导入别名，信号值的验证以源时间线为准。两个draw灰度10→80、反向seek、
Metal Validation、CLI三次循环、API和7个损坏身份负例通过。导出但不导入的handle无GPU
副作用；跨进程/反序列化后失去同进程来源关联的导入会记录Source=0并被离线回放拒绝。
GPU使用后的CPU改值仍拒绝。T65旧负例脚本因此只保留CPU改值负例；另对
导出但不导入handle的capture做了成功回放，CPU改值capture仍在
`MTLSharedEvent::unsupportedHostMutation`明确失败。

最终联合终端回归：`RENDERDOC_METAL_LAST_TEST=69 bash
util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`通过 **70份capture
API/CLI、1917个畸形输入、700次lifecycle打开**，resident growth 770048 bytes；
日志`/tmp/metal-batch69-full.log`。`git diff --check`、批次脚本语法检查通过；
GUI/Computer Use未执行，T55扩展与T69并入集中人工QA。T55 capture SHA-256
`6be4751717496e61325751167ba4722114a7b5a16706015fd2c7d724a5f788f7`；T69
`3ac0d738542d6447268618cf30bb297dbdfb33f85d0e63b5b3a0b77c3acb3aed`。
库与app内嵌库SHA-256均为
`22c244afc13af3fb7d53f5fa367779b335410d4f573c393cc13c8a6a71108fdc`。

ICB GPU间接range仍未解决，下一波优先；见[PLAN.md](PLAN.md)不可遗忘项。
