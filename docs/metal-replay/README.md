# RenderDoc Metal Replay 项目入口

本目录是 RenderDoc v1.46 Metal replay 适配工作的唯一计划与交接入口。实现代码仍放在
RenderDoc 原有目录中，项目状态、阶段计划、测试覆盖和关键决策统一记录在这里。

## 项目目标

在 macOS 上为 RenderDoc v1.46 建立一套可用的 Metal 离线 replay 能力。首版完成后，
用户应当能够在 qrenderdoc 中打开受支持的 Metal `.rdc`，浏览事件，并查看：

- Buffer 列表、元数据和原始/格式化内容。
- Texture 列表、子资源、像素和常见格式。
- Render pipeline、render pass、资源绑定和固定功能状态。
- 顶点/索引输入以及 Mesh Viewer 中的网格。
- Metal shader 的入口点、阶段、可获得的 MSL 源码和反射信息。
- 选定 draw call 的 replay 输出。

这一阶段不以 shader debugging、pixel history、性能计数器、ray tracing、mesh shader、
MetalFX、任意第三方应用注入或完整 capture 产品化为目标。为了产生可重复测试输入，允许
实现最小范围的 capture/序列化补全和测试夹具，但它们只服务于 replay 验证。

## 固定基线

- 上游：`https://github.com/baldurk/renderdoc.git`
- 标签：`v1.46`
- 基线提交：`e4bd23b671d3d5a747ff5221dbe08a63eb6ca200`
- 开发分支：`metal-replay-v1.46`
- 工作平台：Apple Silicon macOS

## 文档导航

- [PLAN.md](PLAN.md)：总体路线、阶段门槛和验收条件。
- [PHASE1.md](PHASE1.md)：已完成的 Metal 样例与 capture 阶段记录。
- [PHASE2.md](PHASE2.md)：已完成的 T00/T01 replay 后端纵向闭环。
- [PHASE3.md](PHASE3.md)：已完成的 T02 索引立方体、depth 与 Mesh Viewer 纵向闭环。
- [PHASE4.md](PHASE4.md)：已完成的 T03 纹理采样、资源绑定与 Pipeline UI 收敛计划。
- [PHASE5.md](PHASE5.md)：已完成的 T04 动态 uniform 与 fragment buffer binding 纵向切片。
- [PHASE6.md](PHASE6.md)：已完成的 T05 多 vertex buffer 与实例化网格纵向切片。
- [PHASE7.md](PHASE7.md)：已完成的 T06 MRT 与 blending 纵向切片。
- [PHASE8.md](PHASE8.md)：已完成的 T07 depth/stencil 纵向切片。
- [PHASE9.md](PHASE9.md)：已完成的 T08 MSAA resolve 纵向切片。
- [PHASE10.md](PHASE10.md)：已完成的 T09 mip/cube/array 子资源纵向切片。
- [PHASE11.md](PHASE11.md)：已完成的 T10 buffer/texture blit 纵向切片。
- [PHASE12.md](PHASE12.md)：已完成的 T11 compute texture filter 纵向切片。
- [PHASE13.md](PHASE13.md)：已完成的 T12 单层直接 argument buffer 资源引用纵向切片。
- [PHASE14.md](PHASE14.md)：已完成的 T13 单次间接 draw 参数纵向切片。
- [PHASE15.md](PHASE15.md)：已完成的 T14 indexed instancing/base vertex 纵向切片。
- [PHASE16.md](PHASE16.md)：已完成的 T15 point/line 基础拓扑纵向切片。
- [PHASE17.md](PHASE17.md)：已完成的 T16 vertex texture/sampler binding 纵向切片。
- [PHASE18.md](PHASE18.md)：已完成的 T17 texture/sampler 批量绑定纵向切片。
- [PHASE19.md](PHASE19.md)：已完成的 T18 fragment storage buffer 纵向切片。
- [PHASE20.md](PHASE20.md)：已完成的 T19 vertex storage buffer 纵向切片。
- [BATCH21-22.md](BATCH21-22.md)：下一批 T20/T21 的联合验收与批内状态规则。
- [PHASE21.md](PHASE21.md)：批内第一条 T20 单命令 indirect command buffer 纵向切片。
- [PHASE22.md](PHASE22.md)：批内第二条 T21 indexed indirect draw 纵向切片。
- [STATUS.md](STATUS.md)：当前状态、最近验证结果、阻塞项和下一步。
- [TEST_MATRIX.md](TEST_MATRIX.md)：Metal API/资源/UI 覆盖矩阵与测试样例来源。
- [HANDOFF.md](HANDOFF.md)：新 agent 的接手规则、省额度验证节奏、compact/新任务边界和可复制提示。
- [HANDOFF_HISTORY.md](HANDOFF_HISTORY.md)：按需追查的历史阶段交接证据。
- [DECISIONS.md](DECISIONS.md)：关键架构与范围决策。

## 当前状态

T00-T19 的 Native/Capture/RDC inspect/Replay/UI 纵向切片已关闭。最新 T19 完成 vertex
storage buffer 的非零 slot/offset、reflection/descriptor/usage、VS Storage Buffers、标准 Buffer
Viewer 与资源跳转；T19/T18/T16/T02/T05 定向回归和最新 qrenderdoc 实机验收通过，未触发
T00-T19 全量 L3。下一批按 `BATCH21-22.md` 连续实现 `PHASE21.md` 的 T20 单命令 ICB 与
`PHASE22.md` 的 T21 indexed indirect。每条切片立即完成必要自动验证；批末在最终构建上做一次
去重的联合定向测试和同一轮 qrenderdoc 实机验收，两条阶段一起关闭。接手时只需阅读本入口、
`STATUS.md` 当前批次与恢复检查点、BATCH/两份 PHASE 和 `HANDOFF.md` 的验证规则；`PLAN.md` 与
历史阶段文档按需查阅。

## 初始基线结论（2026-09-20）

RenderDoc v1.46 已有约 1.5 万行 Metal 驱动骨架，包含对象包装、部分 capture 序列化、
初始资源内容和 Objective-C bridge。然而它并不是可用的 replay 后端：

- `MetalReplay` 尚未实现 `IReplayDriver`。
- 尚未注册 Metal replay provider，qrenderdoc 不能把 Metal `.rdc` 作为可 replay capture 打开。
- `MetalReplay` 当前只有资源描述索引辅助函数。
- bridge 中约有 237 个 `METAL_NOT_HOOKED()`；Metal 目录中约有 402 个未实现/未处理标记。
- `WrappedMTLDevice::AddAction()` 和 `AddEvent()` 仍未实现，事件树尚未形成。
- `ProcessChunk()` 只处理少量资源和 draw 相关 chunk，大量常见状态仍直接报未处理。

因此首个工程里程碑不是扩充所有 Metal API，而是先建立可编译、可启动、可截取、可打开并可
replay 的最小纵向链路，再按测试矩阵逐项扩大支持面。每个新 feature 都先验证样例原生运行，
随后补 capture，再立即补同一 feature 的 replay 和 UI 检查，避免积累一批无法验证语义的 `.rdc`。
