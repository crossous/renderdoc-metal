# UE 5.8 Metal 首帧接入

**当前入口（2026-09-29 BATCH320）：** 项目按钮已改为受控抓取当前 scene
viewport：渲染线程开始捕获、主动绘制 viewport、等待编辑器 Slate present，
再结束。不要用旧 `TriggerCapture` 版本判断是否截到场景 pass。本机插件已编译，
库和 qrenderdoc 内嵌库 SHA256 均为
`ca90af4c95ab69858145eadf1e3023de16cbbb424d565eff864e3b91c7432872`。
用户旧 `frame1770` 在 qrenderdoc 内容验收失败：只有 Slate pass、黑色 RT；
新按钮尚待用户点一次验证。见 [BATCH320](../../docs/metal-replay/BATCH320.md)。
以下 BATCH315–318 入口与 `TriggerCapture` 说明为历史记录。

**历史入口（2026-09-29 BATCH315–318）：** 新 v0x10 库 SHA256
`c13816cf6485ab945aa08ba476bc173613d5fbd609cb15518a6e65aabee49354`；
使用 `RENDERDOC_METAL_BUILD_DIR=/tmp/rdm-t312-build bash
util/ue/run_ue_metal_capture_macos.sh --run` 启动 UE，在 Empty、非 Nanite
视口点一次截帧。新 `.rdc` 生成后，可用 `RENDERDOC_METAL_BUILD_DIR=/tmp/rdm-t312-build
bash util/ue/replay_ue_metal_once_macos.sh <capture.rdc>` 单次有界打开。
旧 v0xF `UE58_frame5394.rdc` 缺 BC 纹理帧首初始内容，最终库安全拒绝；
需用此库重截，才能定位下一个真实阻塞。详见
[BATCH315–318](../../docs/metal-replay/BATCH315-318.md)。
下方 M5 与更早记录为历史证据，以本段和 STATUS 顶部为准。

本目录提供项目级 Mac Editor 插件和启动脚本，不修改 UE Engine。插件向 Level
Editor 视口工具栏及 **Tools** 菜单添加 **Capture Metal Frame**。按钮调用本仓库
`RENDERDOC_GetAPI` 的 `TriggerCapture()`，只请求下一帧；它不能让已经运行的
进程事后加载 Metal hook。必须由启动脚本从进程创建时注入 dylib。

本机项目 `SocoTestProj` 的 UE `Build.version` 为 **5.8.3**，初始地图是
`/Game/FirstPerson/Lvl_FirstPerson`。这是用于定位首个真实阻塞点的入口，
尚未完成 UE 普通场景与人工 UI 验收。下一次请在编辑器中使用一个简单的非 Nanite
场景；项目现有默认渲染设置包含 ray tracing、VSM 和 Lumen，不代表最小配置。

**当前停点（09-29 M5）：** `UE58_frame833.rdc` 在 Private buffer 初始状态
恢复后仍于 `ResourceId::503251` 等待超时。UE SM6 bindless heap 内有旧进程
GPU VA；旧帧缺捕获地址映射，不能宣称 viewer 已能打开。定向结果见
`docs/metal-replay/UE58_M5_PRIVATE_INITIAL_BINDLESS_2026-09-29.md`。新编译
viewer 备份在 `build-private-initial-viewer/bin/qrenderdoc.app`，未做人工 UI。
原 `build-qrenderdoc/bin/qrenderdoc.app` 也已同步为相同构建。
启动脚本目前默认读取同目录 `lib/librenderdoc.dylib`；`--check` 会打印它的
SHA256，必须等于本批证据里的 `19d491f0...` 才是这一版。旧
`build-ue-debug` 保留作历史构建，仍可用 `RENDERDOC_METAL_BUILD_DIR` 显式选择。

## 准备（终端）

当前这台机器的 `SocoTestProj` 已安装并编译插件；以下安装与构建命令用于
另一个项目或重建，不要在同一项目重复运行安装器（它会拒绝覆盖现有插件）。

```bash
cd "/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312"
python3 util/ue/install_renderdoc_metal_plugin.py \
  "$HOME/Documents/Unreal Projects/SocoTestProj/SocoTestProj.uproject"

# 源码和构建目录带空格时，CMake 在 -force_load 处会拆分路径。用无空格别名：
test -L /tmp/renderdoc-metal-t312 || ln -s "$PWD" /tmp/renderdoc-metal-t312
cmake -S /tmp/renderdoc-metal-t312 -B /tmp/renderdoc-metal-t312/build-ue-debug \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-Wno-nontrivial-memcall \
  -DENABLE_METAL=ON -DENABLE_GL=OFF -DENABLE_GLES=OFF -DENABLE_EGL=OFF \
  -DENABLE_VULKAN=OFF -DENABLE_QRENDERDOC=OFF -DENABLE_PYRENDERDOC=OFF \
  -DENABLE_RENDERDOCCMD=ON
cmake --build /tmp/renderdoc-metal-t312/build-ue-debug --target renderdoc renderdoccmd -j 12

"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
  SocoTestProjEditor Mac Development \
  -Project="$HOME/Documents/Unreal Projects/SocoTestProj/SocoTestProj.uproject"

bash util/ue/run_ue_metal_capture_macos.sh --check
```

上述编译标志仅为本机 Xcode 26 的旧代码警告兼容设置；未调整 Metal 功能守卫。
`--check` 只记录版本、签名和 hash，不启动 UE。

## 当前验证状态

用户 `20260929-064750` 会话已用 SHA256 `1ab4448a93b2…` 的新注入库点击按钮，
成功保存 `Saved/RenderDocMetalCaptures/UE58_frame833.rdc`（SHA256
`472dfa48a56cbaa43b6cc97d5f82d0471263de0c0fdd47049dde166e1df6f163`）。
这次 UE 截帧结束没有重现旧的生命周期崩溃。viewer 位于
`/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312/build-qrenderdoc/bin/qrenderdoc.app`；
CLI 单次回放该帧在 `MTLHeap::newTexture(offset)` 安全拒绝，首个未接通纹理类型
是 3D placement。viewer 可以由用户手动启动，但目前不能据此确认该 UE 帧的
画面回放正确。见 `docs/metal-replay/UE58_M4_QRENDERDOC_3D_PLACEMENT_2026-09-29.md`。

用户 `20260929-024537` 再次点击按钮时，旧库在截帧结束阶段因 command buffer
生命周期错误崩溃，没有新 UE `.rdc`。当前 worktree 已修复该路径并通过短
autorelease pool 的原生、注入、API/CLI 定向测试；后续用户截帧已跨过该崩溃点。
证据见 `docs/metal-replay/UE58_M4_CAPTURE_LIFETIME_2026-09-29.md`。
重试前可用启动脚本输出的 library SHA256 确认当前 dylib 是
`1ab4448a93b2c8f08211087bb40f2afbf72a9231d98c978918ea5425fe3b6ef9`。

注入、编辑器启动和按钮触发已经由终端验证；`20260929-010300` 会话取得
`Saved/RenderDocMetalCaptures/UE58_frame99.rdc`。在当前 worktree 构建的
`librenderdoc.dylib` 下，这一帧已通过 Metal Validation 的单次 CLI 回放，
API 探针对 165 个 draw 进行六次前进与回跳的 pipeline 身份检查也通过。
交错 command buffer 回放修复及命令见
`docs/metal-replay/UE58_M4_INTERLEAVED_REPLAY_2026-09-29.md`。这是定向终端
验证；该项目还不是固定的非 Nanite 最小场景，尚未将原生画面与回放 GPU
输出逐项对照，也没有人工 UI 验收。

本机 ZenServer 未安装，脚本默认使用 `-ddc=InstalledNoZenLocalFallback`，
避免在 Zen 检查处反复等待。可用 `UE_METAL_DDC_MODE` 覆盖。
可用 `UE_METAL_EDITOR_ARGS` 传递额外编辑器参数；脚本将其写入 session manifest，
并在 UE stdout 中记录完整启动命令。当前项目的 `METAL_SM6` 在
`-BindlessOff` 下报告 3157 个全局 shader 编译错误，窗口未创建；
本轮不要以该参数启动。详见
`docs/metal-replay/UE58_M6_BINDLESSOFF_STARTUP_2026-09-29.md`。

## 后续单次验证命令

```bash
bash util/ue/run_ue_metal_capture_macos.sh --run
```

若已用旧版脚本启动编辑器，请先正常关闭该 UE 进程，再运行上面命令。
旧版脚本经 `/usr/bin/arch` 启动，而 macOS 会移除传给这个受保护工具的
`DYLD_*` 变量，导致按钮可见但 `RENDERDOC_GetAPI` 缺席。新版直接启动
UE 的 arm64 可执行文件；不可在已经运行的编辑器中补注入。

确认编辑器日志中有 `RenderDoc API resolved from ...librenderdoc.dylib`，再点
**Capture Metal Frame** 一次。截帧写入项目 `Saved/RenderDocMetalCaptures`。
每次启动的 manifest、UE stdout 和 RenderDoc 日志存于
`Saved/RenderDocMetalSessions/<时间>`。按钮会显示请求已发出或已保存的路径；
若 60 秒内没有文件，会明确提示超时。启动脚本最长运行 1800 秒，可用
`UE_METAL_TIMEOUT_SECONDS` 调整；超时会停止本次编辑器进程组。脚本不自动点击、不循环截帧。

如启动失败，先检查 session 中是否有 `DYLD_INSERT_LIBRARIES` 被忽略、
`Loading into ...UnrealEditor`、`RenderDoc API resolved`，以及最早的
`METAL_NOT_HOOKED`/`METAL_CHUNK_NOT_HANDLED`/`RDCERR`。没有这三类注入证据
时，不能把失败归因于具体 Metal API。仅在生成有效 `.rdc` 后，对该文件单次
运行 `build-ue-debug/bin/renderdoccmd replay --loops 1 <capture.rdc>`。

`qrenderdoc` 的现有目标控制功能可在连接到运行中的目标后请求截帧，但本项目
尚未在 UE 5.8 上验证该路径。本插件是本轮固定的一按钮入口。
