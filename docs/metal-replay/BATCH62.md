# BATCH62：设备查询与资源 purgeable 状态

承接 BATCH61 的 128 bridge / 76 旧 chunk。本批接通 14 个 bridge：九个设备只读
heap/sample/sparse/timestamp 查询；一个加速结构纯 size 查询；设备并行编译调度开关与旧版
command-queue capture boundary；buffer、texture 的 `setPurgeableState`。后两者接通
原有两个 chunk。当前剩余 **114 bridge / 74 旧 chunk**；未新增 chunk ID，Max1278。

设备尺寸、采样与稀疏区域转换只询问真实设备，不产生 GPU replay 命令；加速结构
`heapAccelerationStructureSizeAndAlignWithSize` 也仅接通纯数字查询，**不代表加速结构
创建/编码/回放可用**。描述符型加速结构查询仍保留未接通标记，因为其中可能含 wrapped
buffer，直接转发不可靠。包装设备仍将 `supportsRaytracing` 报为 false。编译调度开关仅影响本机编译；
旧 capture boundary 是原生调试提示。`metal_device_queries_smoke` 比较原生、注入和
Metal API Validation 下的结果及状态，三种运行均通过；原生设备支持 ray tracing 时
测试强制调用注入层的纯 size 查询，日志 `/tmp/metal-device-queries-final.log`。

T62 用 Shared buffer 和 Shared 纹理经 `KeepCurrent`、`NonVolatile` 两种状态，在
抓取前和帧内形成真实 chunk；shader 同时消费两资源，原生 BGRA `37/26/15/255`，
回放 API 像素/资源身份通过。离线只接受这两种**不主动丢弃内容**的状态；
`Volatile`、`Empty` 仍可在抓取时转发原生 API，但离线明确拒绝，不能把它们视为已支持。
如真实应用依赖此生命周期，下一阶段需建模内容丢弃与重新填充。

一键 `bash util/buildscripts/scripts/test_metal_capture_batch62_macos.sh` 完成初轮；
收窄描述符型查询后在最终代码上重跑完整 replay 门禁，日志
`/tmp/metal-batch62-final-replay.log`，共 **63 份 capture API/CLI、1784 个畸形输入、630 次
lifecycle 打开**，resident growth 606208 bytes。新增的 12 个畸形样本覆盖两类
资源身份及状态范围。T62 capture SHA-256 `88d2fbc35494fd7b571b54cd39da6265cb3e074dc3bb58b17d31f586fe593ddd`；
库与 app 内库 `e11191df7a7734e59ee50ba137521241388a9ad445d6c559d34557a6c4ed457e`，
GUI executable `fe8bcf852b683bc463a3be883e6c54208b7bd45054a24f5dbace58c912346d74`。
未执行 GUI/Computer Use，未提交/推送；T62 加入集中人工清单，总数 30 份。

下一轮若目标 bridge <100，应按完整 heap/mesh 等功能族设计资源身份与回放；
不把只转发原生对象、但无法离线重建的 marker 当作完成。
