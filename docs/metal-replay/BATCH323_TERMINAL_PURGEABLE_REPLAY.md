# BATCH323：帧尾 buffer Empty 的有界回放

2026-09-29，工作树 `/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312`，
基础提交 `a7dd2bdae28adaa5a0a623663ea2557d4af0a600`。本批未提交或推送；
原 `renderdoc-metal` 的四项本地改动未触碰。承接 [BATCH322](BATCH322_UE58_CPU_AUDIT_AND_COMPLETION_PROBE.md)。

## 横向证据与设计边界

RenderDoc D3D12/Vulkan 在 GPU 提交和完成边界之后回收资源。UE 5.8
`MetalSubmission.cpp` 检查工作与 signal command buffer 的 Completed 状态，
`MetalTempAllocator.cpp` 的 `DeferredDelete` 才对临时 buffer 调用 `Empty`。
真帧在 chunk 11174/11175 对 `251031/251032` 调用 `Empty`，它们仅在
9248/9249 由 blit encoder `251045` 读取，所属 command buffer `251044`
于 11163 提交；之后无显式引用。捕获不含 UE 的 callback 完成时间，
因此回放不能在 11174 直接设置 `Empty`。

本批只接通**帧尾 buffer Empty**：帧执行前扫描整个 chunk 流；若 buffer
在 `Empty` 后被再次引用、重复设置 purgeable state，或状态非法，则在
GPU 帧执行前拒绝。安全扫描按 64 KiB 分块，不随 chunk 大小增长内存。
回放先记录该 `Empty`，待 `FinishReplayCommands` 提交并等待所有 frame
command buffers 成功完成后执行；下一次 seek 前恢复 `NonVolatile`，再由
原有帧首快照恢复内容。纹理 `Empty`、`Volatile`、帧中复用和无法证明
后续无引用的情形仍拒绝。这个方案不重现 UE callback，也不声称支持
任意异步回收。

## 当前 Mac 的定向证据

- `bash util/test/metal/run_purgeable_completion_probe_macos.sh --run`：
  4 KiB 原生 Metal Validation 的 blit → 后继完成信号/callback → `Empty`
  → `NonVolatile` → 再次 blit 与 GPU 字节通过，10 秒超时；日志
  `/var/folders/cm/wrylyc0x78vbwmff8py06ktm0000gn/T/metal-purgeable-completion/run.log`。
- 新库注入同一夹具的帧尾 `Empty` 变体，10 秒超时退出 0，生成
  `/private/tmp/rdm-t323/t323_terminal_capture.rdc`，SHA256
  `5910b24d0249a15522e36aee9c6af1515492c0376fdac0b607a8ebd7842b8423`。
  注入日志 `/private/tmp/rdm-t323/terminal-injected.log`；XML 确认
  commit、completed handler 注册、signal、`Empty` 顺序。
- `cmake --build /private/tmp/rdm-t312-build --target renderdoc renderdoccmd -j 4`：
  通过。随后 `cmake --build /private/tmp/rdm-t312-build --target build-qrenderdoc -j 4`
  也通过；最终终端库与 app 内嵌库 SHA256 均为
  `27bdde246ee87f426920d004a1b5c1503bf71671f44bcfa2382a760b5de97e50`，
  GUI 可执行文件 SHA256
  `fd140d1a3cfe25e051299595cef859f2d834d03ecd929aae169b0671781ce9b4`。
  App 位于 `/private/tmp/rdm-t312-build/bin/qrenderdoc.app`，**未启动**。
- `RENDERDOC_METAL_BUILD_DIR=/private/tmp/rdm-t312-build
  UE_METAL_REPLAY_TIMEOUT_SECONDS=10 UE_METAL_REPLAY_MAX_RSS_MIB=1024
  bash util/ue/replay_ue_metal_once_macos.sh
  /private/tmp/rdm-t323/t323_terminal_capture.rdc`：Metal Validation
  API/CLI 各一次退出 0；日志 `rdm-ue-replay.icXHA4`。
- 专用 `metal_purgeable_completion_replay` 对小截帧检查 4 KiB GPU 字节
  均为 `0x5a`，并向同一 copy event 回跳两次：退出 0，日志
  `/private/tmp/rdm-t323/targeted-replay-viewerlib.log`。最终 app 内嵌库
  同哈希下，小帧 API/CLI 又各一次退出 0，日志 `rdm-ue-replay.8ZOBNh`。
  其他旧帧定向日志对应 app 构建前的同源库版本，未扩大到全量回归。
- `python3 util/test/metal/metal_purgeable_completion_invalid.py
  /private/tmp/rdm-t312-build/bin/renderdoccmd
  /private/tmp/rdm-t323/metal_purgeable_completion_replay
  /private/tmp/rdm-t323/t323_terminal_capture.rdc`：将 `NonVolatile`
  插入 `Empty` 后的畸形帧，OpenCapture 在 GPU 帧执行前拒绝。
  `metal_purgeable_state_invalid.py` 的既有 10 例也全部拒绝。
- `test_metal_replay_targeted_macos.sh t62 t35 t319`：T62、T35 的
  API/CLI 均通过。T319 的通用 smoke helper 误套 T35 的 dispatch
  断言而失败；T319 专用 `metal_ue_frame_shared_placement_replay` 的 GPU
  字节与 seek、CLI 均通过，不能把通用 helper 的失败记成该帧回放失败。
- `git diff --check`：通过。

如果被推迟 `Empty` 的 buffer 同时仍是待解析的 compute indirect
argument，加载 pass 会在 GPU 读回前拒绝该帧；避免把这个后处理读取误当成
chunk 流中无引用。此条件尚未有专用正反夹具。

## UE 真帧和稳定性

`UE58_capture.rdc` SHA256
`a96e685f726608bb30a84f2ae0c7485d2edaaad82553b8e95da1df44e91915f0`。
CPU XML 审计确认两次 `Empty` 后无显式 ResourceId 引用。**尚未用本批库
对这张 UE 真帧做 API/CLI GPU 回放，也未在 qrenderdoc 人工打开。**
原始 RDC 仍有 BATCH321 的 15 个 buffer texture view 早于父 buffer 的
旧捕获顺序，不能直接用于检验本批回放。`util/ue/repair_ue_metal_buffer_view_order.py`
仅重排这 15 个创建 chunk，并保持总计 11214 个 chunk 和其余顺序；
在 `/private/tmp/rdm-t323/UE58_capture_view_ordered.rdc` 生成**诊断副本**，
SHA256 `45db270790407660288878daaace741f0209bb08183a2b0221324c2d813deb2d`，
与 BATCH321 重启前生成的诊断副本哈希完全相同。原始文件 SHA 未变。
生成命令：

```bash
python3 util/ue/repair_ue_metal_buffer_view_order.py \
  /private/tmp/rdm-t312-build/bin/renderdoccmd \
  "/Users/kurogames/Documents/Unreal Projects/SocoTestProj/Saved/RenderDocMetalCaptures/UE58_capture.rdc" \
  /private/tmp/rdm-t323/UE58_capture_view_ordered.rdc
```

本机曾有两次 WindowServer watchdog 重启，和 RenderDoc 的因果未归属；
这次只运行数 KB 夹具，没有升级到 UE/GUI 负载。下一步由用户运行
`util/ue/replay_ue_metal_once_macos.sh` 对**诊断副本**先做一次 API-only 的
有日志、超时和 RSS 上限的回放。脚本新增
`UE_METAL_REPLAY_API_ONLY=1` 入口，API 成功时不会再运行 CLI，避免在
本机重复大帧 GPU 负载。只有成功后才检查 GPU 像素、scope、MRT 与 Shader Converter
GPU VA；仍可能遇到下一阻塞点。
该 API-only 入口已用 4 KiB 小帧验证退出 0，日志 `rdm-ue-replay.bueHPr`；
**未**在 UE 真帧上运行。

**本批仅小夹具与相关旧帧定向终端通过；全量回归未跑；UE 真帧回放未跑；
人工 UI 未验收；累计成功 UI QA 增量 0。**
