# BATCH319：UE 帧内 Shared placement buffer 的回放重置

2026-09-29，独立工作树 `/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312`，
基础 HEAD `e0a26f7e65e22a3890aa38a318bde51d24babf6b`；原
`renderdoc-metal` 目录未改动。本批没有提交、推送或运行 GUI/Computer Use。
本机 Apple M4 Max、64 GB、macOS 26.6；UE 5.8.3，项目 `SocoTestProj`。

## 真帧和首个失败点

用户用库 SHA256 `c13816cf6485ab945aa08ba476bc173613d5fbd609cb15518a6e65aabee49354`
截取 `UE58_frame1770.rdc`，29,633,881 字节，SHA256
`75dad9cecc5876c71db77de0e3a46b0f51371118e2d28ff3584113b148dda51d`。
会话 `Saved/RenderDocMetalSessions/20260929-192015` 的 manifest 与 UE 日志
确认该库注入、插件解析 `RENDERDOC_GetAPI`、19:20:52 保存帧。Editor 主进程
已退出。日志场景为 `Lvl_FirstPerson` PIE，仍不是 Empty 最小场景。

首次命令：

```bash
RENDERDOC_METAL_BUILD_DIR=/tmp/rdm-t312-build UE_METAL_REPLAY_TIMEOUT_SECONDS=75 \
  bash util/ue/replay_ue_metal_once_macos.sh \
  '/Users/kurogames/Documents/Unreal Projects/SocoTestProj/Saved/RenderDocMetalCaptures/UE58_frame1770.rdc'
```

日志在 `rdm-ue-replay.uH7IQn`：Metal Validation API `OpenCapture` 退出 0，
341 root actions、10 textures、545 buffers；CLI 首次事件预览退出 -11。
RenderDoc 日志显示先失败在 `ReplayLog` 的 `Invalid Metal CPU buffer reset`，
崩溃报告 `renderdoccmd-2026-09-29-192230.ips` 显示随后在
`ReplayOutput::RefreshOverlay` 空指针崩溃。诊断构建把失败限定为 Shared reset，
具体资源 `ResourceId::410638`：有加载期快照、属于帧内创建的 Shared placement
buffer，但回放开始前已按 placement 生命周期释放，其原生对象为 null。
这次没有 Metal Validation GPU 错误；两次诊断回放峰值 RSS 约 324 MiB。

## 横向对照与修复

D3D12 `ApplyInitialContents` 和 Vulkan `ApplyInitialContents` 在帧重放前恢复
帧首资源；帧内创建资源由命令流重新创建。UE 5.8.3 MetalRHI 的 placement
buffer 创建和释放是普通 heap allocator 路径，未发现专门针对 RenderDoc
的处理。Metal 特有之处是同一 heap offset 的旧原生对象必须在 seek 前释放，
随后按创建 chunk 重建。因此 `ResetReplayCPUUpdatedBuffers` 与
`RestoreReplayPrivateBufferInitialContents` 现在跳过已登记的**帧内 placement**
资源；其帧内 CPU 写入仍按已缓存的提交所属更新，在新对象创建后的 command
buffer 提交时执行。其他帧首 buffer 的恢复与验证守卫保持原样。
总括错误也拆成 Shared、Private、BC texture、texture view 四个阶段，
Shared reset 记录具体资源/大小/模式，便于今后定位。

## 定向证据

- 修复后同一有界命令日志 `rdm-ue-replay.SjzQiF`：API/CLI 均退出 0，
  API/CLI 峰值 RSS 分别 226.5/320.8 MiB；CLI 单次事件预览通过。
- 新 T319 原生 `MTL_DEBUG_LAYER=1 /tmp/rdm-ue-frame-shared-placement` 退出 0，
  帧内 Shared placement buffer 初值 7/11 经两个 GPU dispatch 变成 9/13。
  同一程序加 `DYLD_INSERT_LIBRARIES=/tmp/rdm-t312-build/lib/librenderdoc.dylib`
  和 `RENDERDOC_METAL_CAPTURE_PATH=/tmp/rdm-ue-frame-shared-placement-t319`
  截帧通过。`t319_capture.rdc` SHA256
  `6f1a9c951de371703135d78df337dca5531ef235fb54a3d381e12fd37b674e39`。
  编译与注入命令：

  ```bash
  clang++ -std=c++17 -arch arm64 -mmacosx-version-min=12.0 -I. -x objective-c++ \
    util/test/metal/metal_ue_frame_shared_placement.mm \
    -framework Foundation -framework Metal -framework QuartzCore \
    -o /tmp/rdm-ue-frame-shared-placement
  MTL_DEBUG_LAYER=1 /tmp/rdm-ue-frame-shared-placement
  MTL_DEBUG_LAYER=1 \
    RENDERDOC_METAL_CAPTURE_PATH=/tmp/rdm-ue-frame-shared-placement-t319 \
    DYLD_INSERT_LIBRARIES=/tmp/rdm-t312-build/lib/librenderdoc.dylib \
    /tmp/rdm-ue-frame-shared-placement
  ```
- `/tmp/rdm-ue-frame-shared-placement-replay t319_capture.rdc` 在
  Metal Validation 下验证 GPU 值 8/12、9/13、8/12、9/13 的事件回跳；
  有界脚本的 API/CLI 各一次也退出 0，日志 `rdm-ue-replay.KqykNt`。
  专用探针用 `clang++ -std=c++17 -arch arm64 -mmacosx-version-min=12.0
  -DRENDERDOC_PLATFORM_APPLE -I. util/test/metal/metal_ue_frame_shared_placement_replay.cpp
  -L/tmp/rdm-t312-build/lib -lrenderdoc -Wl,-rpath,/tmp/rdm-t312-build/lib
  -o /tmp/rdm-ue-frame-shared-placement-replay` 编译。
- `python3 util/test/metal/metal_ue_frame_shared_placement_invalid.py
  /tmp/rdm-t312-build/bin/renderdoccmd captures/metal-smoke/t319_capture.rdc`：
  缺 heap、未对齐 offset 两例均安全拒绝。
- `test_metal_replay_targeted_macos.sh t35 t117 t313 t314 t317 t318`：
  T35/T117/T313/T314/T317 的 API/CLI 通过；T318 的通用 helper 被旧 T37
  拷贝断言误判（`metal-targeted.aTVOj9`）。T318 随后以专用
  `/tmp/rdm-ue-bc-placement-replay captures/metal-smoke/t318_capture.rdc`
  验证三纹理原始块、GPU 像素、正反向 seek，且有界 API/CLI 通过
  （`rdm-ue-replay.3qR0QJ`）。另跑 T59 API/CLI 通过（`metal-targeted.HRz68V`）。
- `cmake --build /tmp/rdm-t312-build --target renderdoc renderdoccmd -j 8` 与
  `cmake --build /tmp/rdm-t312-build --target build-qrenderdoc -j 8` 通过；
  最终库与 app 内嵌库 SHA256 同为
  `c9bdc6bea56256140df635a65c30d5b56531c78a233b24a23568a5a372acfd6a`。
  `git diff --check` 通过。Qt 提示 macOS 26 SDK 兼容性警告。

当前结论是**这张 UE 帧的定向终端 API/CLI 打开通过**，尚未人工确认画面、
资源和事件树，也未核对原生 UE GPU 像素。全量回归、长时压力、人工 UI
均未运行，累计 UI QA 增量 **0**。终端打开暂无首个阻塞；默认 bindless
GPU VA 重定位及 Empty 非 Nanite 最小场景仍待独立验证。
