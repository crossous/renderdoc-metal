# BATCH74：Tile pipeline与GPU tile dispatch

2026-09-27。接通 `MTLDevice::newRenderPipelineStateWithTileDescriptor`、Render encoder
`setTileBytes`、`setTileBuffer`、`setTileBufferOffset`、`setTileBuffers` 与
`dispatchThreadsPerTile`：合计6 bridge / 6旧chunk，原始剩余 **86/49**。tile descriptor
记录函数、单色附件格式、sample count、最大线程数、tile-size匹配及options；含额外
binary/preloaded/linked function、非常规tile buffer mutability或调用栈深度的描述符
在capture保留原生行为，但离线明确拒绝。当前仅验证单色RGBA/BGRA8、单采样；
tile texture/sampler、imageblock、function table等其它入口尚未接通。

T74在本机M2 Pro上用tile kernel对12-byte Shared buffer作真实原子累加，三个提交
依次使用单buffer、offset变体、batch变体，并用三份`setTileBytes`传入1/2/3；
后续fragment draw读取第三段计数驱动画面。另在dispatch后测试单/batch空绑定。
五次Metal Validation原生运行、截帧、XML结构、三轮CLI、API逐事件计数与
末→首→中→末回退、最终像素、28种畸形输入、T34/T43/T73定向回放通过。
入口`bash util/buildscripts/scripts/test_metal_capture_batch74_macos.sh`，日志
`/tmp/metal-batch74-targeted.log`。GUI/Computer Use未运行；T74留待集中人工QA。

最终集中终端回归：**74份成功capture API/CLI、1997畸形输入、740次lifecycle**
通过，resident growth 2064384 bytes；日志`/tmp/metal-batch74-full-final.log`。
库及app内嵌库SHA-256为`e74885bf9e097d5864d198aa1e914bc590517ba90ad8888279dd2e6dbabee597`，
T74 capture为`921596de7c369175645bc3f7bba9fb3f3b4bafee97bf1468589e44d94770cb50`。
最终代码上`git diff --check`与脚本bash语法检查通过；未提交/推送。
