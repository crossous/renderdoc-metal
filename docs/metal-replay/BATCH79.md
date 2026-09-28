# BATCH79 — Mesh buffer/bytes 绑定

接通 `setMeshBuffer`、`setMeshBufferOffset`、`setMeshBuffers`、`setMeshBytes`
四个 bridge 与新chunk（1289–1292）。回放严格校验encoder、资源类型与设备、槽位、
数组形状、offset在buffer内及已有绑定，然后调用原生API；buffer按读前写引用标记。
`setMeshBytes`覆盖此前同槽buffer状态。未把mesh buffer伪装为vertex buffer UI状态。

T79真实mesh shader从48-byte buffer的0/16/32 offset读取位移，并从inline bytes
读取半径；三次MeshDispatch分别在左、中、右形成绿色三角形。Replay API逐事件
检查三个中心及黑色背景、mesh pipeline和fragment shader身份、1×1×1 grid与
32×1×1 mesh线程。16个畸形chunk拒绝测试覆盖类型/越界/形状/encoder。

- 定向：`bash util/buildscripts/scripts/test_metal_capture_batch79_macos.sh`，五次
  原生Metal Validation、捕获/XML、CLI3、API像素、负例和T78回归已通过。
- 全量：`RENDERDOC_METAL_LAST_TEST=79 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`；
  79 captures、2068 malformed、790 lifecycle已通过，resident growth 1490944 bytes，
  日志`/tmp/metal-batch79-full.log`。
- 捕获SHA `28f4ba76c665…`、库/app SHA `4fc7b5c759ba…`。
- 当前原始剩余**72 bridge / 42旧chunk**；T70实际缺口另计73/43。
  GUI/Computer Use尚未运行，T34–T69、T71–T79与T10 marker共46份待集中人工。
