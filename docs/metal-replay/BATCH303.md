# BATCH303：单节点 Stitched Library

接通同步 `MTLDevice::newLibraryWithStitchedDescriptor`（旧 chunk 1020）及 bridge。
当前安全子集：一个已包装的 stitchable visible 函数、一个 graph、一个输出
function node、一个索引0的 input node，无 attributes/control dependencies。
捕获函数资源身份、graph/function 名称和输入索引；回放重建同构 descriptor，
建立源函数→stitched library→输出函数→compute pipeline 资源链。复杂 graph
仍显式拒绝；异步入口随后在 BATCH304 接通，不以 native 透传冒充回放支持。

本机原生 Metal Validation 探针使用 `scale(x)=2x`，visible function table 在
GPU 将输入3.0变为6.0；T303 demo 进一步执行32线程并绘制画面。注入截帧
探针最初把 input node 错放进 graph.nodes，Metal Validation 在用户进程
内触发断言退出（exit 134）；按 SDK 约束改为仅把 function node 放入该数组后
探针稳定通过。这不是新的系统 panic。
`t303_capture.rdc` 最终 SHA-256 `a4df65b1fb4b…`，API 检查资源图和32个
float值 `2,4,…,64`、中心像素约 `(0.2,0.7,0.3)`，CLI 回放通过。
17例畸形身份、名称、输入索引、函数类型和重复chunk被干净拒绝；
T01/T48/T104/T115/T119/T135/T144/T300/T301/T302/T303 的11份 API+CLI
定向回归通过，T35+T303各10次生命周期打开通过，resident增长1622016字节。
完整累计压力回归、GUI/Computer Use未运行。

本批当时库与app内嵌库 SHA-256均为 `56bc82ca0211…`；当前最终版本见
[BATCH304](BATCH304.md)，GUI executable仅构建未启动。
待集中GUI QA 累计265份。原始防御宏匹配 bridge **63**、chunk **15**
（chunk数字含宏定义本身，实际未接通case为14）；与BATCH301后的64/16相比
各减少1。异步 stitched 已在 BATCH304 接通。06:26 IOGPU panic仍无法归因；
本批短测未见新增。
