# BATCH88–90 — Mesh 间接绘制与 GPU 参数

T88 接通 `drawMeshThreadgroupsWithIndirectBuffer` bridge，新增 chunk 1314。
回放校验 Mesh pipeline、参数 buffer 身份/4-byte 对齐/12-byte 范围，以及 object、
mesh threadgroup 限制；保留参数 buffer 引用、Indirect usage、pipeline 输出与
MeshDispatch|Indirect action。GPU 在执行时读取网格尺寸，因此事件树显示
`indirect, <?, ?, ?>`，不从编码时 CPU 值推断尺寸。

- T88：Shared buffer 偏移 16 的间接网格参数，真实绿色三角形。
- T89：compute shader 在 GPU 上写 Private buffer 的偏移 16；mesh 间接绘制
  在同一个 command buffer 消费该参数。Replay API 验证写入事件先于绘制、
  绘制事件的 buffer 数据、Indirect/CS_RWResource usage 和中心像素。
- T90：Object→payload→Mesh pipeline 复用同一间接入口，覆盖 object
  threadgroup 上限分支与参数资源、像素。
- 每个 fixture 五次原生 Metal Validation、捕获/XML、CLI 三次、Replay API
  与 11 个畸形输入拒绝。定向命令为
  `bash util/buildscripts/scripts/test_metal_capture_batch88_macos.sh`，
  `bash util/buildscripts/scripts/test_metal_capture_batch89_macos.sh`，
  `bash util/buildscripts/scripts/test_metal_capture_batch90_macos.sh`。
- 全量命令：
  `RENDERDOC_METAL_LAST_TEST=90 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
  90 captures、2215 malformed、900 lifecycle通过，resident growth 2424832 bytes；
  日志 `/tmp/metal-batch90-full.log`。
- Captures SHA 前缀：T88 `81a4e8f91479`，T89 `67f3eb8a6005`，
  T90 `8ca736e3c39f`；库及 app 内嵌库 `d74b63d4a264`。
- 原始剩余 **52 bridge / 42 旧 chunk**；T70 GPU 生成 ICB range 仍单独
  不支持，功能缺口计 53/43。该族不应与 T70 的精确 ICB 子事件问题混淆。
  GUI/Computer Use 未运行；T34–T69、T71–T90 与 T10 marker 共 57 份
  待用户集中 QA，见[总单](QA_CONSOLIDATED.md)；未提交/推送。
