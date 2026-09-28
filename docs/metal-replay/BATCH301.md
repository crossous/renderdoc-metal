# BATCH301：导入 Binary Archive 与两类 pipeline 依赖

功能：`newBinaryArchiveWithDescriptor` 不再落入旧 chunk 拒绝分支；捕获 file URL
archive 二进制并给资源独立身份，回放从有界临时文件重建。compute/render descriptor
保存有序 archive 依赖，保留 `FailOnBinaryArchiveMiss` 选项。新增字段以版本
0xA 门控，旧 0x9 截帧仍可读。变更 archive 的 `add*` 方法仍显式拒绝。

终端证据：M2 Pro 原生探针与 T301 demo 均在 Metal Validation 下通过；注入
生成 `t301_capture.rdc`（最终 SHA-256 `2780f4d81854…`），archive 数据 32960 字节，
compute/render 同引 ResourceId 12。原始 archive 文件移走后 CLI ×3 与 API
通过，API 断言 32 个 compute `i+17` 值及中心像素；19 例畸形截帧被拒绝。
T01/T48/T104/T115/T135/T144/T300/T301 的 API+CLI 定向回归全过；
T35+T301 各 10 次生命周期打开通过，resident 增长 393216 字节。没有运行
完整累计压力回归。仅有此前 06:26 那份 IOGPU panic，不能归因，也无新 panic。

构建：`renderdoccmd`、demos、`build-qrenderdoc` 均构建成功；replay 库与 app
内嵌库 SHA-256 同为 `b297a988af63…`，GUI 只编译未启动。GUI 待验累计
263 份。原有 bridge 拒绝入口少 1、旧 chunk 拒绝分支少 1；由于新 wrapper
对 6 个 archive 变更方法加入显式拒绝，未经归一化的 `METAL_NOT_HOOKED`
文本匹配从 59 增至 64，不能把这个数字解释为功能倒退。旧 chunk 匹配 16。

下一步：如需支持“捕获期间构建/修改 archive”，须逐项序列化其输入和时序；
当前真实应用若走这些方法会明确失败。UI 的资源跳转、API Inspector 依赖及
事件 seek 已登记待集中 QA。
