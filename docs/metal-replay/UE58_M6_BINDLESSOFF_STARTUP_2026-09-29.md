# UE 5.8.3 `-BindlessOff` 启动失败（2026-09-29）

## 本次终端证据

- 独立工作树：`renderdoc-metal-t312`，HEAD
  `e0a26f7e65e22a3890aa38a318bde51d24babf6b`；原目录未动。
- 命令：`UE_METAL_EDITOR_ARGS=-BindlessOff bash util/ue/run_ue_metal_capture_macos.sh --run`。
  session：`SocoTestProj/Saved/RenderDocMetalSessions/20260929-162217`。
  注入库 SHA256 `19d491f029980e7b40f2f5f88c1fedcf833b8ad46fb78c60f3684bdc1843ed8f`。
- `renderdoc.log` 16:22:22 记录库进入 UnrealEditor、注册 Metal hooks；
  UE 5.8.3 / macOS 26.6 / Apple M4 Max。程序坞有图标，但窗口未创建。
- 两次短时采样显示 UE GameThread 在
  `CompileGlobalShaderMap` / `BlockOnShaderMapCompletion` 等待；8 个
  `ShaderCompileWorker` 持续运行，短采样落在 UE 的 HLSL→SPIR-V 编译。
- 16:46:50 `ue-editor.log` 记录
  `3157 Shader compiler errors compiling global shaders for platform METAL_SM6`。
  首个可见错误是 `PathTracingAtmosphere.ush:136` 的 `VolumeFlags` 未定义；
  大量后续错误涉及 Metal atomic 指针地址空间和 16 位类型。
  16:49:34 请求结束进程，最终强制终止；没有 `.rdc`。
- 保存的 `ue-editor.log` SHA256
  `f25c92d129259a13c4bf229bbefbf716ffb555b316ded9d23178aa566e8f4818`；
  `renderdoc.log` SHA256
  `b7fc81d96ab03b2c49a506a07e0777e96344f320f40fabe1763e3df39ed261dc`。

## 解释和后续

UE 源码 `RHI.cpp` 明确解析 `-BindlessOff`，而 macOS
`BaseMacEngine.ini` 的 `METAL_SM6` 默认 `BindlessConfiguration=All`。
因此这是实际的 shader 配置切换。早前不加该参数的 session
`20260929-064750` 曾达到 `Engine is initialized`，但当前没有进行
不注入、相同参数的对照，**不能断言 3157 个编译错误的归属**。
本次没有走到截图/回放；终端定向截帧结果为失败，全量回归未运行，
人工 UI 未运行，累计 UI QA 增量 **0**。

启动脚本已改为记录超时、每分钟提示，并在明确的全局 shader 编译错误时
停止失败进程，避免无窗口长时间等待。当前 session 使用的是编辑前的脚本，
因此不体现这些新行为。随后以
`env -u UE_METAL_EDITOR_ARGS bash util/ue/run_ue_metal_capture_macos.sh --run`
启动默认配置，session `20260929-165339`：16:53:58 UE 日志报告
`Engine is initialized`，插件解析到了 RenderDoc API，注入日志持续记录
Metal command buffer。尚未得到用户对窗口的人工确认，也没有截帧。用户可在
Empty、非 Nanite 关卡点一次截帧按钮；bindless 的完整支持仍按 M5 的
资源重定位门槛推进。

## 默认配置首次新截帧

用户随后在 session `20260929-165339` 点击了一次 **Capture Metal Frame**。
UE 日志 17:03:11 请求下一帧，17:03:13 确认保存
`SocoTestProj/Saved/RenderDocMetalCaptures/UE58_frame4476.rdc`；文件大小
149,219,170 字节，SHA256
`ab0e5a2918af5568a0257eb6f316140f68954f26d6c09e0b4f17606434b5bcbb`。
`renderdoccmd thumb` 可以读取嵌入缩略图；图中为 `Lvl_FirstPerson`
编辑器视口，**不是 Empty 关卡**。UE 运行日志也显示载入
`/Game/FirstPerson/Lvl_FirstPerson`；本次是有用的真实应用截帧，
不能当作非 Nanite 最小场景验收。

编辑器随后由用户正常关闭。新帧的一次 API 打开在 placement heap
区间复用处安全拒绝，详见
[frame4476 首个阻塞证据](UE58_M6_FRAME4476_PLACEMENT_LIFETIME_2026-09-29.md)。
嵌入缩略图证明捕获写入了画面，但不证明事件、资源或回放像素正确。
全量回归与人工 UI 验收未运行，累计 UI QA 增量 **0**。
