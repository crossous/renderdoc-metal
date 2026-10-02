# BATCH326：本地 UE 帧 CPU 审计与原生 GPU 身份诊断

后续：[BATCH327](BATCH327_GPU_IDENTITY_RESOURCE_CLOSURE.md)已收到用户重截，
发现并修复间接资源capture过滤遗漏，且由助手自动再截。下文待用户重截是
本批结束时的历史检查点，目前不再要求重复操作；完整UE replay仍未通过。

2026-09-30，本地 M2 Pro/16 GiB，基础 HEAD c4be68bb7；保留接手时已有
STATUS/QA_PENDING、CPU audit 与启动地图顺序修改。未提交或推送。

用户新帧 Testproj `UE58_capture.rdc` 25,493,035 bytes，SHA256
`5d73323bafc3217c6c5e4f8b8ed12d5ebca6e36eb03875c6d23e66b9eda6a5e5`。
会话 `20260930-182302` 的 controlled viewport 捕获返回 1，UE 正常退出。
用户在 Entry 启动后新建并保存 Default 模板为 NewMap，截帧视口 2744×1690。
CPU XML：7912 chunks，181 draw，83 render pass，10 MRT pass，59 CB/commit，
12×512 MiB heap（声明容量 6 GiB）；有 Lumen/VSM/sky capture scope，不能将它
当纯 Empty 小帧。两次 buffer Empty 后没有显式 wait chunk，属于已有回收边界。
原件未修改，完整帧 GPU/GUI 尚未运行。

横向对照 UE `MetalBindlessDescriptors.cpp`、Shader Converter runtime header
与 D3D12 `WrappedID3D12Resource::m_Addresses`。UE 标准资源表 Buffer 24 的
18,874,368-byte 初始状态，按 UE 的 24-byte IRDescriptorTableEntry 布局解码，
有 886 非零 gpuVA（708 种值）与 2106 非零 textureViewID。Buffer 25 是 sampler
表，不能把它的首字段误当 buffer VA。旧格式没有原地址/ID→资源身份映射；
这是已确认的数据缺口，尚不能证明它是所有远端 watchdog 的唯一原因。

本批范围是**捕获诊断与保守拒绝**，不是 bindless replay 功能族完成：
三个 native getter（buffer gpuAddress、texture/sampler gpuResourceID）首次被
应用查询时，追加 chunk 1397 `MTLResource::CaptureGPUIdentity`，含资源身份、
kind 0/1/2、原值。记录保存在资源 record，活跃捕获时另放入帧流；每对象原子
去重，供后续捕获复用。新 Max1398，旧 chunk 编号/字段不变。元数据可 CPU 导出。
实际 GPU 回放先扫描整个 stream，发现此族在分配捕获资源、初始 GPU 上传或
帧提交之前拒绝；仅查询而未实际解引用的应用也会被保守拒绝。旧帧没有该诊断
chunk，不能据此认为它们安全。未实现 descriptor 重编码、frame GPU 写入/复制
执行点、sampler/typed view/offset 解析或跨 GPU 兼容，不做任意 64-bit 替换。

日志/本地产物目录：`build-macos-debug/local-m2-ue-capture`。

## 验证清单（运行前范围）

- L0：renderdoc/renderdoccmd/viewer 构建、diff check。
- L1：单线程、一像素、4/8-byte buffer 原生 Validation，GPU pointer 读41+
  texture采样64=105；同源注入两次捕获，核对三种原生身份与 CPU table bytes；
  两次捕获均保留 metadata；API/CLI 对元数据初始区和移至帧尾的变体在 GPU前
  拒绝，CPU export可用。另测首次 texture/sampler query 在活跃帧内：第一份
  保留 resource record+frame metadata，第二份复用 record 的原值。
- L2：旧 T01/T09/T35/T49/T52/T62 的定向 API/CLI；旧 terminal Empty 小帧的
  GPU bytes/seek 与负例；T35/T01各10次生命周期（base wrapper新增原子字段）。
- L3：不执行全量压力；本批 preflight 与 metadata 不放行新 GPU路径，出现GPU
  错误、异常增长或 WindowServer卡顿即停止，不扩大负载。
- L4：未启动 qrenderdoc，用户新UE帧正确内容/资源/scope/MRT仍待验。

## 最终定向证据与检查点

- `cmake --build build-macos-debug --target renderdoc renderdoccmd -j 4`，及
  `--target build-qrenderdoc -j 4`：成功。viewer's embedded library 已同步。
  初次 preflight 只 SkipCurrentChunk 未 EndChunk，导致下一 chunk 未对齐、旧
  T01 API 失败；已改为 EndChunk，最终全清单通过。这是用户态读流错误，
  没有在这些失败中提交 frame GPU 工作。
- `metal_gpu_identity_capture_probe` 原生 Validation：结果105；注入 pre-frame
  query 变体两份 capture 与 active-query 变体两份 capture均退出0。
  日志 `identity-native.log`、`identity-injected.log`、`identity-active-injected.log`。
- `metal_gpu_identity_capture_gate.py`：两种变体各两份 metadata与原生
  bufferVA/textureID/samplerID及8-byte pointer table核对通过；每份原帧与把
  所有 metadata移至帧尾的变体，API/CLI均在执行frame GPU前明确拒绝。
  `identity-gate.log`、`identity-active-gate.log`。用同一资源跨两次capture验证
  metadata持续保留。拒绝日志现在打印 ResultDetails 的具体错误信息。
- `test_metal_replay_targeted_macos.sh t01 t09 t35 t49 t52 t62`：六帧 API/CLI
  成功；`identity-targeted.log`，详细 `metal-targeted.BH3buA`。
- T35/T01各10次 lifecycle：20次打开成功，resident growth622592 bytes，
  `lifecycle.log`。旧 terminal Empty GPU字节/seek和post-reference负例成功：
  `old-terminal-bytes.log`、`old-terminal-invalid.log`。
- 新UE帧静态检查：所有97个 buffer texture view 的父 buffer 均先出现；
  两个 Empty 后没有显式 ResourceId 引用。`static-binding-evidence.json`。
  显式引用检查不能证明没有 shader间接引用，不据此放行GPU执行。
- CPU audit 增加 `gpu_identity_queries`；原帧为0。diff check、Python AST、
  `.command` shell语法与新库 UE `--check`通过。未发现本轮新panic报告。

最终终端库与 viewer内嵌库 SHA256均为
`c7cb40b786ff508d7e008279a909dc4d78bab0345784e940671358502f728745`；
CLI `ffcebc3677ce0a39b00fc31fccd96b3e9324c6b02d801f9eade6e131719c4b72`。
identity capture两个pre-frame变体和两个active变体仅本地build目录保存，
不将诊断拒绝写为成功replay。原UE文件复制到
`Saved/RenderDocMetalCaptures/UE58_NewMap_before_identity_5d73323b.rdc`，
SHA256与原件相同，以防下一会话使用同一捕获名覆盖。

项目 `Saved/RenderDocMetalLocalM2.command` 已更新启动地图为已存在的
`/Game/NewMap`，保持15FPS/50%/Nanite off；没有改项目Config、Content或
Engine源代码。新库预检记录在会话 `20260930-185530`。当前UE未启动；
**下一项需要用户运行同一.command并点击一次捕获按钮**，取得带原生身份
映射的新UE帧。之后CPU检查metadata覆盖、offset/typed texture/sampler及
GPU descriptor更新链，再按D3D12/Vulkan设计完整重定位；不能用此次元数据
补丁或关闭bindless把该族记为支持。

**定向终端（诊断捕获、GPU前拒绝、旧小帧回归）通过；全量未跑；人工UI
未验；用户UE新帧仅CPU审计，未GPU回放。UE正常画面与replay目标仍未达到。**
