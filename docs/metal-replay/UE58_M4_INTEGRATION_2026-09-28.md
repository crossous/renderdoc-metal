# UE 5.8.3 / M4 Max 单帧接入（历史与续测）

最新状态见 [交错 command buffer 定向回放证据](UE58_M4_INTERLEAVED_REPLAY_2026-09-29.md)：
已有 UE 真帧 CLI/API 定向回放通过。下方是早期检查点，其中“没有 UE `.rdc`”
等说法仅对当时有效。

## 2026-09-29 终端续测：首个 UE 阻塞点链（历史停点）

本段记录当时结论；上方链接的交错回放证据是最新结果。下方 2026-09-28 的
“未启动 UE”描述也是当时检查点。工作树仍为独立 `renderdoc-metal-t312`，基础 HEAD
`e0a26f7e65e22a3890aa38a318bde51d24babf6b`。旧 `renderdoc-metal` 的
4 项 Git 改动未重置、暂存或改写。本轮未提交、未推送，也未使用 Computer Use。

用户首次注入 UE 的 `20260928-233651` 会话在
`WrappedMTLCommandBuffer::blitCommandEncoderWithDescriptor` 的原生调用处 SIGSEGV。
小型原生 Metal Validation 探针证明带 counter sample 的 blit pass 在本机可执行；
注入旧库时 AGX 报 counter buffer 不是 `AGXMTLCounterSampleBuffer`。
新增 blit pass counter attachment 的真实资源解包、捕获、回放及索引/身份校验，
Metal capture 版本升为 `0xD`，旧版无 attachment 的 chunk 仍能回放。
`20260928-235547` UE 会话暴露另一半问题：UE 跨 autorelease pool 保存 command
buffer，旧代理未保有对应原生对象；小探针可复现。修复 command buffer 和 encoder
代理的原生引用生命周期后，UE 越过 RHI 初始化。`20260928-235827` 随后因
原生 blit encoder 被 pool 提前释放而触发 `released without endEncoding`；
encoder 生命周期修复后不再停在此处。

本机 Zen 工具独立运行 `zen service status` 返回 `Service 'ZenServer' is not
installed`，UE 默认 DDC 会反复等待。启动脚本现默认加
`-ddc=InstalledNoZenLocalFallback`；这是本机 UE 启动条件，不是 Metal 修复。
`20260929-000237` UE 会话继续到 compute `useHeaps:count:`，包装 heap 数组被
Objective-C 的未知消息转发交给 AGX，驱动调用链 SIGSEGV。现新增 compute
`useHeap`/`useHeaps` 双 chunk（1384/1385），原生数组只传真实 heap、捕获
依赖、回放身份/数量校验。最后一次 UE 会话 `20260929-001104` 已越过该点，
在 `ObjCBridgeMTLCommandBuffer parallelRenderCommandEncoderWithDescriptor:` 的
`METAL_NOT_HOOKED()` 守卫 SIGTRAP。该功能族与 parallel 子 encoder 尚未实现，
不能用原生透传冒充支持。此会话还记录 AGX counter-buffer 类型错误和 compute/
render heap identity 拒绝；这些可能是另一个 descriptor/资源来源，尚未归因。
当前**第一个仍未解决的明确阻塞点**为 parallel render encoder；在其之前的
AGX 错误也需先用定向探针定位。因已有 GPU 驱动调用链崩溃，本轮停止后续 UE 启动。

本机实际验证：

- `cmake --build build-ue-debug --target renderdoc renderdoccmd -j 8`：通过。
  当前 dylib SHA256 `63e96dd83e971c0926015f9bd3456789155763d7b915e85ad83c861692be4396`。
- `MTL_DEBUG_LAYER=1 bin/demos_x64 Metal_UE_Blit_Counter --frames 5` 及
  `Metal_UE_Compute_Heaps --frames 5`：原生各 5 帧通过；同命令加
  `RENDERDOC_METAL_CAPTURE_PATH=<本工作树 captures/metal-smoke 前缀>` 和
  `DYLD_INSERT_LIBRARIES=<本工作树 build-ue-debug/lib/librenderdoc.dylib>`：注入各
  5 帧通过，生成 `ue_blit_counter_capture.rdc` 和
  `ue_compute_heap_final_capture.rdc`，SHA256 分别为
  `104de70ba3b56c894af9a1ab38acfa1bb57ac92e9272c5d28dda86377d114d6b`、
  `1601b4efd3fe460c33e0fe27b75a9e2bfa95a88b54465df00261b0d040029d94`。
- 两份新帧分别以 `MTL_DEBUG_LAYER=1 build-ue-debug/bin/renderdoccmd replay
  --loops 1 <capture>`、`build-ue-debug/metal_replay_output_smoke <capture>
  /tmp/<output>.ppm` 单次 CLI/API 打开通过；API 检查 blit counter 身份、索引和
  GPU 时间戳，以及 compute heap 身份与 GPU 写回。
- `python3 util/test/metal/metal_ue_blit_counter_invalid.py ...` 的 6 例和
  `metal_ue_compute_heap_invalid.py ...` 的 7 例畸形截帧均拒绝且未崩溃。
  旧 `t41_capture.rdc`、`t101_capture.rdc`、`t312_capture.rdc` 均单次 CLI 回放通过。
- `UE_METAL_TIMEOUT_SECONDS=60 bash util/ue/run_ue_metal_capture_macos.sh --run`
  的最后会话 manifest/RenderDoc/UE 日志在项目
  `Saved/RenderDocMetalSessions/20260929-001104/`。`renderdoc.log` 和 dyld
  证实 dylib 进入 UE；未生成 UE `.rdc`，也未到按钮截帧阶段。

尚未运行：parallel encoder 功能族的原生正例、注入截帧、API/CLI 回放及负例；
UE 真正单帧截帧和回放；长时压力、全量回归、人工 UI QA。以上只是**定向终端
验证**，不是全量或人工验收。累计 UI QA 增量 0，原 274 份待验不变。

本批从 `crossous/renderdoc-metal` 的 `metal-replay-v1.46` 提交
`e0a26f7e65e22a3890aa38a318bde51d24babf6b` 创建独立 worktree
`/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312`。原
`renderdoc-metal` 目录及其 4 项改动保持不变。新 worktree 初始干净；
`captures/metal-smoke/t312_capture.rdc` SHA256 为
`2beb183fccd609cad533a1e126e6c3444d91c33b305076d83076e5193c801d07`。

## 本机与构建

- macOS 26.6 / arm64 / Apple M4 Max（40 GPU 核）/ 64 GB RAM；UE 官方
  `/Users/Shared/Epic Games/UE_5.8/Engine` 为 5.8.3、CL 58210709。
- 项目为 `/Users/kurogames/Documents/Unreal Projects/SocoTestProj/SocoTestProj.uproject`；
  有 FirstPerson 地图。项目原默认设置含 ray tracing、VSM 和 Lumen，
  首次应手动使用非 Nanite 最小场景，不能把现有默认场景视为最小配置。
- UE 可执行文件 SHA256：
  `92a29c07430a88ee50ca6d343a81d83e92ec433619dff99ac12995dac26c2612`。
  arm64 存在；签名为 ad-hoc，未见 hardened runtime 标志。
- RenderDoc dylib SHA256：
  `90ce0856425b9aa8383a183d67d706ec0cc1ae7b5310a89f37e7ff1f56054787`。
  `cmake -S /tmp/renderdoc-metal-t312 -B /tmp/renderdoc-metal-t312/build-ue-debug
  -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-Wno-nontrivial-memcall
  -DENABLE_METAL=ON -DENABLE_GL=OFF -DENABLE_GLES=OFF -DENABLE_EGL=OFF
  -DENABLE_VULKAN=OFF -DENABLE_QRENDERDOC=OFF -DENABLE_PYRENDERDOC=OFF
  -DENABLE_RENDERDOCCMD=ON` 后，`cmake --build ... --target renderdoc
  renderdoccmd -j 12` 通过。旧 `MetalInitialContents` 的 Clang 21 警告
  只在本机构建命令中抑制，没有改动该功能族。
- 项目级插件经 `UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh SocoTestProjEditor
  Mac Development -Project=<上述uproject>` 编译通过；UE 工具报告
  `Result: Succeeded`。插件 dylib SHA256：
  `2af3618d9abac61a2e0300523fd98c209649abb82b68db6cb31aa7ed4bf1a23e`。
- `bash util/ue/run_ue_metal_capture_macos.sh --check` 通过，仅做只读预检；
  manifest 位于项目 `Saved/RenderDocMetalSessions/20260928-231331/manifest.txt`。
  `bash -n`、安装器 `py_compile` 和 `git diff --check` 通过。

## 接入与验证边界

新增 `util/ue/RenderDocMetalCapture` 项目级 Mac Editor 插件、安装器、启动脚本
和说明。启动脚本在进程创建时使用 `DYLD_INSERT_LIBRARIES`，记录注入日志、
版本与 hash，设置超时。插件通过 `dlsym` 查找 `RENDERDOC_GetAPI`，核对
来源库路径，并在视口工具栏和 Tools 菜单提供一次请求一帧的按钮。
`TriggerCapture()` 之后最多观察 60 秒，报告 `.rdc` 路径或超时。
这只是接入机制；它不绕过任何 Metal bridge 守卫，也不保证 UE 帧可捕获。

按用户要求，**未启动 UE/GUI，未注入，未生成 UE `.rdc`，未跑 UE API/CLI
回放、畸形输入、旧帧定向回归、长时压力或人工 UI QA**。之前 M2 Pro 上的
32 份定向测试不能当作本机 M4 的运行结果。第一个待定位的真实阻塞点仍是
**UE 进程是否加载了这份 dylib，随后最早不支持的 Metal 调用/bridge/chunk**。
由用户单次运行 `bash util/ue/run_ue_metal_capture_macos.sh --run`，按
`util/ue/README.md` 收集 session 日志与捕获后，再决定是否具备某一功能族
的原生、注入、API/CLI 与负例验证条件；在此之前不实施 Metal 语义修复。

累计 UI QA 增量：0。BATCH311–312 的 274 份人工待验 capture 状态不变。

## 用户首次启动的首个阻塞点（同日补记）

用户从旧版脚本启动 UE 并点击按钮，插件报告
`RENDERDOC_GetAPI is absent`。本机 session
`Saved/RenderDocMetalSessions/20260928-232337` 的 UE 进程 PID 40510
有 `RENDERDOC_METAL_LIBRARY`/`RENDERDOC_CAPFILE`，但没有
`DYLD_INSERT_LIBRARIES`/`DYLD_PRINT_LIBRARIES`；`vmmap`/`lsof`
只见项目插件，不见 RenderDoc dylib，也没有 `renderdoc.log` 或 `.rdc`。
这定位为注入前的启动问题，尚不能归因到 Metal API/bridge/chunk。

非 GPU 探针复现：通过 `/usr/bin/arch -arm64` 运行 arm64 小程序时，
`DYLD_*` 从目标进程环境消失；直接运行同一程序可由 dyld 加载
`90ce0856…` 的 RenderDoc dylib。启动脚本已去掉 `arch` 包装；
UE 自身为 universal binary，本机默认 arm64。**尚未在新版脚本下重启 UE
或取得截帧**，须由用户正常关闭旧进程后再做一次单帧尝试。
