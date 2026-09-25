# 从受控 Metal replay 走向 UE、Unity 和高级 GPU 功能

评估日期：2026-09-26。本文是路线评估，不改变当前已验收范围或实现代码。

## 已有能力与边界

T00–T33 的受控小样例已覆盖基础图形、资源/Viewer、compute、单次间接
draw/dispatch 和一组 render ICB。每项都有 native、capture、structured data、
replay/API 和用户 GUI 证据。现有自动脚本通过 `DYLD_INSERT_LIBRARIES` 注入
自建 demo，也有应用内 capture API 的早期验证。基础 viewer 和 seek 已可用。

这证明了“受支持的 Metal 调用可完整进入 `.rdc` 并在本机重放”，尚未证明
“任意图形应用或游戏的一帧都可截取和分析”。当前 `README.md` 的首版范围
明确排除了任意应用注入和完整 capture。代码审查还发现约 216 处 bridge
`METAL_NOT_HOOKED()` 标记，以及 `metal_core.cpp` 中约 165 个未处理 chunk
分支；它们含大量可选接口，不能用数量直接计算完成百分比，却说明真实
引擎兼容仍有显著缺口。`newHeapWithDescriptor`、texture view、预编译
library/data、compute pipeline descriptor、fence/event 等是需要通过真实
帧确认优先级的缺口。当前没有 UE/Unity 的正式 capture 和回放证据。
此外，当前 Shader Viewer 对源码创建的 library 可展示真实 MSL；对预编译
`.metallib` 不能凭 capture 还原原始源码。若目标是理解 Nanite 的 shader
实现，还需与 UE 的 shader 名称、生成 MSL、调试符号或引擎源码建立映射；
仅有 Event Browser 和纹理画面不足以完成源码级分析。shader 单步调试与
pixel history 也尚不在当前支持范围。

## 达到目标的里程碑

| 里程碑 | 现状 | 验收目标 |
| --- | --- | --- |
| 受控 Metal 场景 | T00–T33 已完成 | 保留为底层回归库 |
| 自有复杂多 pass 场景 | 部分组合能力已验证 | 多 queue/encoder、heap、texture view、同步、预编译 shader 等在同一帧闭环 |
| 第三方可控应用 | 未验证 | 明确注入或应用内接入方式，可靠触发截帧、生成 `.rdc`，错误能定位到调用 |
| UE 普通场景 | 未验证 | 选定引擎版本、RHI/渲染设置，稳定打开一帧，事件树/资源/画面可信 |
| UE Nanite/VSM 场景 | 未验证 | 在 UE 普通场景之后，用 Nanite 原生 Mac 路径逐项补缺，能分析关键 compute、indirect、资源与输出 |
| Unity | 未验证 | 另选 Unity 版本、渲染管线和最小项目，重走真实应用接入与首帧 triage |
| 光追、mesh shading、Metal 4 特性 | 未实现 | 按独立 fixture + 真实场景逐类建立 capture、replay、资源/状态和 UI 语义 |

优先做 **UE 普通场景试截帧**，不应等全部 Metal API 清单完成。先在
同一 UE 版本的非 Nanite 小场景上验证加载、触发和 capture；用首个真实
失败调用生成缺口清单，再推进到启用 Nanite 的最小场景。Nanite 的具体
Metal 命令、资源格式、同步和 GPU 驱动方式必须以该帧为准，不能仅从 UE
的跨平台架构推断。Unity 的接入与首帧可并行规划，但它是独立兼容目标。

### UE 构建选择

首个真实应用试截帧**不要求源码版 UE**：可以先用 Epic Launcher 官方
macOS 构建与一个最小项目，从终端启动编辑器或项目进程，验证本项目的
Metal 动态库能否进入进程、拦截设备创建并生成可检查的 `.rdc`。
当前受控样例使用 `DYLD_INSERT_LIBRARIES`；官方 UE app 的签名、hardened
runtime、library validation、子进程启动方式或 Metal 接口差异可能阻挡
此路径，须以实测为准。先检查日志与签名/进程状态，不把启动失败当成
UE 必须使用源码版的证明。

Epic 的 RenderDoc 插件文档称插件随引擎提供，但其已列的 RenderDoc 平台
和 API 不包括 macOS/Metal。因此不能假设 Launcher 版 UE 的现有插件按钮
可直接控制本项目的 Metal 截帧。若目标是编辑器内按钮、自动 attach、
RenderDoc API 集成或 shader/渲染 pass 名称映射，可先评估项目级插件与
预编译引擎是否足够；需要修改 UE 自身 Mac RHI、引擎插件源码或构建/签名
选项时，再使用 UE 源码版。源码版方便开发，但不会自动补齐 RenderDoc
Metal driver 的 capture/replay 缺口。

## 测试节奏

- 保留每个新增 API 路径的 native/capture/replay 自动断言；这些测试负责保证
  截到的语义真实，不能仅靠大引擎最终画面替代。
- 以功能簇和实际引擎阻塞点安排开发，批内跑受影响旧场景；在里程碑上跑
  大范围回归。GUI L4 只验收新增或实质改变的可见行为，合并成少量同轮
  代表帧，不为每个内部小调用重复打开 app。
- 给第三方帧增加自动化 triage：记录第一条未包装/未处理调用、资源缺失、
  replay 停止位置、前后事件输出，并保存可重现的引擎版本/设置和最小项目。

## 本机 M2 Pro 与公司 M4

本机为 M2 Pro、16 GB RAM、macOS 26.1，运行时 Metal 设备报告
`supportsFamily(Apple8)=true`、`supportsRaytracing=true`、
`supportsRaytracingFromRender=true`。Apple 2026 Metal 功能表列明 mesh
shading 自 Apple7、compute/render ray tracing 自 Apple6 可用；因此 M2 Pro
可开发和验证对应 **Metal API 功能正确性**。但 Apple 称 M3 才首次带来
硬件加速的 ray tracing 和 mesh shading；M2 上的 API 可用不等于有这些
专用硬件加速，也不等于可验证其性能或 M3/M4 专属路径。

Epic 当前 UE 5.8 Mac 要求将 Nanite/VSM 列为 M2+ Beta、Lumen 软件光追列为
M1+、Lumen 硬件光追/MegaLights 列为 M2+ Experimental；推荐 32 GB，
最低 16 GB。因此 M2 Pro 可以试 UE Nanite 和 Lumen 软件路径，但本机
内存处于最低线，大工程/大 capture 的失败可能是容量问题。公司 M4 适合
做性能、硬件加速和 Apple9 及以上能力验证；先确认具体 M4 型号、RAM、
macOS/Xcode/UE 版本。Apple 表中 indirect mesh draw arguments 与 mesh
draw ICB 自 Apple9 起，M2 Pro 的 Apple8 无法原生验证这两类特性。

“软件 fallback”有两种不同含义：UE 的 Lumen 软件光追是引擎自己选择的
另一条渲染路径，本机可实际测试；RenderDoc 不能把缺少的 GPU 特性
普遍软件模拟成同一 Metal API 路径。软件 fallback 的截帧不能替代目标
硬件路径的正确性验收。当前 RenderDoc wrapper 还显式把
`supportsRaytracing()` 与 `supportsRaytracingFromRender()` 返回 `false`，
加速结构和 mesh draw 相关 bridge 尚未接通；即使物理设备支持，当前
RenderDoc 构建也不能宣称已支持这些截帧路径。

## 粗略距离判断

没有真实 UE/Unity 首帧之前，无法给可信完成百分比或固定工期。受控
replay 核心已建立；“可靠截取一个 UE 普通帧”仍是一个独立的集成里程碑，
“可分析 Nanite 帧”还需跨越更多资源、同步和 GPU 驱动路径。以当前缺口看，
它们应按**多个开发批次、数月量级的工程风险**规划，而非再补一两个 API
即可完成；首次试截帧后的缺口清单会显著收窄估计。光追和 mesh shading
各自也是大功能簇，不应与 UE Nanite 首帧混作同一验收目标。

## 来源

- 本仓库 `README.md`、`TEST_MATRIX.md`、`PLAN.md` 与 Metal driver 当前代码。
- Epic：<https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine>（页面标为 UE 5.8）。
- Epic RenderDoc 插件文档：<https://dev.epicgames.com/documentation/en-us/unreal-engine/using-renderdoc-with-unreal-engine>（平台/API 清单未列 macOS/Metal）。
- Apple Metal feature tables：<https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf>（2026-05-21）。
- Apple M3 GPU 公告：<https://www.apple.com/newsroom/2023/10/apple-unveils-m3-m3-pro-and-m3-max-the-most-advanced-chips-for-a-personal-computer/>。
