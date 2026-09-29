# UE 5.8 新帧重截与单次回放入口（2026-09-29）

> 本文下方为当时的启动前记录。后续已取得 v0xF `UE58_frame5394.rdc`，
> 并定位 typed buffer view → BC placement；BC 帧首初始内容需新 v0x10
> 格式，见 [BATCH315–318](BATCH315-318.md) 和 [STATUS](STATUS.md)。
> 旧帧在最终库下安全拒绝，尚未取得 v0x10 UE 帧。

独立工作树 `/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312`，
基础 HEAD `e0a26f7e65e22a3890aa38a318bde51d24babf6b`。保留原
`renderdoc-metal` 目录及其 4 项本地 Git 改动；未提交、未推送、未操作 GUI。

## 当前输入和横向结论

- 本机 UE Engine `UE_5.8`、`SocoTestProj.uproject`、项目插件均存在。
  `RENDERDOC_METAL_BUILD_DIR=/tmp/rdm-t312-build bash
  util/ue/run_ue_metal_capture_macos.sh --check` 退出 0，确认本轮注入库 SHA256
  `243043fb9dc1459afd3e1c2a6571dd7b880e1b76bf5b2122156a28a695fa1337`。
- 项目目前只有 `Lvl_FirstPerson`、`Lvl_Horror`、`Lvl_Shooter` 三张地图，
  默认启动 `Lvl_FirstPerson`；尚无保存的 Empty 地图。截帧应由用户在编辑器
  中切到 Empty、非 Nanite 视口。现有最新 UE 文件仍是 17:03 的旧 v0xE
  `UE58_frame4476.rdc`；它缺帧内 placement 创建时序，不能复用来验证
  BATCH313–314 的修复。没有新 UE 截帧或新的 UE 回放结论。
- 首次 `20260929-182336` 启动确实将新 dylib 注入 UE，编辑器初始化，
  但插件按字面比较 `dladdr()` 的 `/private/tmp/...` 与脚本环境变量的
  `/tmp/...`，报 `Wrong RenderDoc library`。这是同一文件的路径别名，
  不属于 Metal replay。修正启动脚本以物理路径传入库后，向首次进程发送
  SIGTERM 并重启；修改正在执行的 shell 文件使旧会话的脚本在 Python
  退出后报告一次 shell 语法错误，旧会话没有截帧。`20260929-182605`
  会话的 dyld 与 UE 日志已确认新库加载、
  `RenderDoc API resolved from /private/tmp/rdm-t312-build/lib/librenderdoc.dylib`
  和 `Engine is initialized`。截至本记录，尚未点击按钮或生成新帧。
- 按 [跨 API 排查顺序](CROSS_API_TRIAGE.md)复查可能的下一族：Vulkan 的
  buffer device address capture/replay 特性可保留原地址，D3D12 跟踪原
  GPU VA 并按具体用途映射。UE 5.8 `MetalBindlessDescriptors.cpp` 使用
  Shader Converter 的 24 字节 `IRDescriptorTableEntry`，含 buffer `gpuVA`、
  texture `gpuResourceID` 和 metadata；延迟更新经 compute 写入 descriptor
  heap。Metal replay 目前只重编码已知 argument encoder packet。若新帧
  确实命中此族，需依据表项类型及提交时序建立资源映射；不能扫描所有
  Shared buffer 盲替换 64 位值。此项目前是静态风险，**未确认是新帧首阻塞**。

## 新增可复现入口

新增 `util/ue/ue_capture_open_probe.cpp`、
`util/ue/replay_ue_metal_once_macos.sh`，并修正
`util/ue/run_ue_metal_capture_macos.sh` 的注入库路径规范化。回放脚本要求
显式 `.rdc` 文件，编译
API 探针，写 manifest（截帧、库、CLI、探针 SHA256），以
`MTL_DEBUG_LAYER=1` 进行一次 API `OpenCapture`；仅 API 成功才执行一次
`renderdoccmd replay --loops 1`。每一步默认上限 75 秒、进程 RSS 4 GiB，
分别保存日志和退出码；没有循环回放或 UI 操作。

验证命令：

```bash
bash -n util/ue/replay_ue_metal_once_macos.sh
RENDERDOC_METAL_BUILD_DIR=/tmp/rdm-t312-build \
  UE_METAL_REPLAY_TIMEOUT_SECONDS=30 \
  bash util/ue/replay_ue_metal_once_macos.sh captures/metal-smoke/t313_capture.rdc
```

两步均退出 0。小帧 API/CLI 各一次退出 0；日志目录
`/var/folders/cm/wrylyc0x78vbwmff8py06ktm0000gn/T/rdm-ue-replay.Yij5Op`，
API 峰值 RSS 18.5 MiB、CLI 87.4 MiB。T313 截帧 SHA256
`12964bf3e559c30ea251ceeca53dfd743d6c9ee9bac8bdc6aa0acca08d6ed2c1`；
测试库 SHA256 如上。此脚本尚未用于新 UE 帧。

下一步由用户运行带 `--run` 的 UE 启动脚本并点击一次截帧；收到新 `.rdc`
后用此脚本做一次有界回放，记录最早失败 API/chunk。若出现 GPU Validation
错误、内存异常增长或系统崩溃，停止升级负载并保留 session 与回放日志。
全量回归、长时压力和人工 UI QA 均未运行；累计 UI QA 增量 **0**。
