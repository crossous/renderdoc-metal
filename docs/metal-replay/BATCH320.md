# BATCH320：UE frame1770 的 scope、目标 viewport 与黑屏定位

2026-09-29，本机 Apple M4 Max、macOS 26.6，独立工作树
`/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312`，基础 HEAD
`e0a26f7e65e22a3890aa38a318bde51d24babf6b`。原 `renderdoc-metal`
目录及其四项 Git 改动未动。本批未提交、推送、启动 GUI 或使用 Computer Use。

## 用户 UI 反馈与真帧

用户已在 qrenderdoc 人工打开 `UE58_frame1770.rdc`：打开成功，但 Event Browser
没有 UE scope，唯一 RT 呈黑色，**内容验收失败**。capture SHA256 为
`75dad9cecc5876c71db77de0e3a46b0f51371118e2d28ff3584113b148dda51d`；
同目录 `save.txt` 是用户导出的事件树。不能再把这份 UI 验收写成“未运行”。

`renderdoccmd convert -f <capture> -o /tmp/ue1770.zip.xml -c zip.xml`
显示 10 个 compute push 和 10 个 pop，包括
`RenderGraphExecute - UpdateAllPrimitiveSceneInfos`、`FRDGBuilder::SubmitBufferUploads`；
另有一个 render push `SlateUI Title = SocoTestProj - 虚幻编辑器`。帧内仅一个
2992×1766 的 clear-to-black 色附件 `410708`，后随 280 个 Slate indexed draw。
缩略图是捕获进程原画面，不能替代 replay 结果。以 `MTL_DEBUG_LAYER=1`
和 45 秒超时运行 `/tmp/ue1770_bytes`，`GetTextureData(410708)` 在
EID 232、261、3101 各返回 21,135,488 字节，非零数均为 0；无新 GPU
Validation 报错。故这张 RDC 主要是编辑器 Slate 合成帧，且 replay 确有黑屏。

## 横向对照与改动

UE 5.8 MetalRHI 的 `RHIBeginBreadcrumbGPU` / `PushDebugGroup` 会调用原生
Metal `pushDebugGroup`；D3D12/Vulkan 在回放事件树使用 PushMarker、PopMarker、
SetMarker。Metal 原来只序列化 debug chunk，没有转成 action。本批在 render、
compute、blit encoder 的加载阶段生成真实 marker action，以捕获字符串命名，
保留 EID 和原始 chunk；按 command buffer 保存 marker path，pass 结束时关闭
未平衡的 encoder scope。新库直接打开旧 UE 帧可见 21 个 marker，其中
`SlateUI` 下为原来 280 个 draw。旧 T10 blit 与 T36 render scope 的名称和
子节点也经 API 探针核实。

UE 官方 RenderDoc 插件通过渲染线程 StartFrameCapture、主动
`Viewport->Draw(true)`、GPU 提交后 EndFrameCapture 定位目标 viewport。
当前项目按钮只调用 `TriggerCapture`，在编辑器多窗口 present 中落到 Slate。
项目级插件已按官方次序修改，优先选择有焦点的 game viewport，否则选
`GEditor->GetActiveViewport()`；无有效 viewport 时明确拒绝。受控 capture
跨一个 editor tick 等 Slate present；没有 drawable 时 Metal capture 安全丢弃，
避免留下 active capture。**新按钮尚未由用户点击，目标场景帧未实测。**

## 黑屏仍未修复

UE Shader Converter 会将 GPU VA 写入 bindless `IRDescriptorTableEntry`、
CBV table 和 vertex buffer VA 数组。旧帧的 buffer 24 前 8192 字节包含
非零 VA；首 draw 前 `setVertexBytes(index=2)` 和
`setFragmentBytes(index=2)` 含 `0x100212b5200`、`0x10060e00000`。
诊断构建中回放资源表有些 VA 恰好相等（例如 descriptor 的
`0x10040090400` 对应 buffer 48），但找不到覆盖
`0x10060e00000` 的已登记 buffer。旧 v0x10 capture 没有足够的原始
GPU VA 到资源 ID 映射，不能安全猜测重定位或宣称这是唯一根因。
下一步先用新按钮截到真正的场景 pass，再以原生 Metal Validation、注入
最小夹具及 API/CLI GPU 输出验证完整 Shader Converter 地址族。

## 命令与结果

- `cmake --build /tmp/rdm-t312-build --target renderdoc renderdoccmd -j 8`：通过。
- `cmake --build /tmp/rdm-t312-build --target build-qrenderdoc -j 8`：通过，
  仅有 Qt 对 macOS 26 SDK 的兼容警告。
- `/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh
  SocoTestProjEditor Mac Development
  '-Project=/Users/kurogames/Documents/Unreal Projects/SocoTestProj/SocoTestProj.uproject'`：
  通过；项目插件二进制 SHA256
  `567c593a72358d35795285017e94c3ffebc7085c3d3d56875f99d6c93f670c13`。
- `RENDERDOC_METAL_BUILD_DIR=/tmp/rdm-t312-build bash
  util/buildscripts/scripts/test_metal_replay_targeted_macos.sh t10_debug t35`：
  API/CLI 各两例通过，日志 `metal-targeted.vT2mRQ`。
- 同脚本 `t36`：API/CLI 通过，日志 `metal-targeted.jqauqI`。
- 同脚本 `t59`：API/CLI 通过，日志 `metal-targeted.rWWepm`。
- 最终库改动后合并重跑 `t10_debug t35 t36 t59`：四份 API/CLI 均通过，
  日志 `metal-targeted.esgqA9`。
- `MTL_DEBUG_LAYER=1 python3 /tmp/ue_marker_negative.py`：将 T36 的 render
  push encoder ID 改为不存在资源，CLI 在 30 秒界内退出 1 并明确拒绝
  `MTLRenderCommandEncoder::pushDebugGroup`；日志 `/tmp/ue-marker-negative.log`。
- 有界 `MTL_DEBUG_LAYER=1 /tmp/ue1770_actions <UE58_frame1770.rdc>`：
  280 draw、21 marker、55 root actions；`/tmp/ue1770-actions-final.log`。
- 有界 `MTL_DEBUG_LAYER=1 /tmp/ue1770_bytes <UE58_frame1770.rdc>`：
  EID 232/261/3101 的 RT 均全零；`/tmp/ue1770-bytes-final.log`。
- `RENDERDOC_METAL_BUILD_DIR=/tmp/rdm-t312-build bash
  util/ue/run_ue_metal_capture_macos.sh --check`：通过；未启动 UE。
- `git diff --check`：通过。库与 viewer 内嵌库 SHA256 均为
  `ca90af4c95ab69858145eadf1e3023de16cbbb424d565eff864e3b91c7432872`。

本批仅**定向终端通过**。未运行新按钮截帧、完整 Shader Converter GPU VA
功能族、全量回归、长时压力或新版本人工 UI 验收。累计成功 UI QA 仍为 0；
`frame1770` 的人工 UI 检查已发生但结果为失败。
