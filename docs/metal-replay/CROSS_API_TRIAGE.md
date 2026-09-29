# Metal 缺口处理顺序：先横向对照，再实现

本规则适用于后续 agent 遇到 UE 或其他真实应用的 Metal 崩溃、回放失败、
资源身份与 GPU 时序问题。先确定已存在的方案，再改 Metal driver。

1. **定位实测行为。**保存最早失败的 API/chunk、调用对象身份、创建/编码/
   提交顺序、原生 Metal Validation 结果和可重复的最小捕获。把进程崩溃、
   安全拒绝、画面或数据错误分别记录。
2. **横向查 RenderDoc 其他图形 API。**优先查 D3D12、Vulkan 中同类资源的
   capture record、提交时序、回放状态、逐事件 seek、CPU/GPU 可见性及异常
   输入检查。复用它们已有的概念和边界，不凭单个 Metal 守卫从零设计。
3. **查目标引擎是否真的特殊适配。**以当前机器上的 UE/Unity 源码及截帧为准；
   区分引擎正常 API 使用、针对原生 Metal 的平台处理和专门针对 RenderDoc 的
   适配。不得为让工具通过而要求引擎改变合法调用顺序。
4. **只对 Metal 特有约束另行推理。**核对 Apple API 生命周期、encoder 独占、
   command buffer 提交与同步、resource/heap alias 等实际限制；用原生
   Validation 夹具证明。若其他 API 的方案无法直接迁移，记录差异后设计
   Metal 映射，不能以原生透传或删除守卫冒充支持。
5. **闭环验收。**先跑原生正例、注入捕获、API/CLI 单次回放和 GPU 输出或
   资源身份检查，再测必要负例与受影响旧帧。遇到新的 GPU 错误按
   [BLACKBOX_GATE](BLACKBOX_GATE.md)停止升级负载。定向通过、全量回归和
   人工 UI QA 分开报告。

当前例子：UE 5.8 的交错 command buffer 先创建多个 buffer、再回到较早者
编码，是原生 Metal 允许的用法。D3D12 在 `ExecuteCommandLists`，Vulkan 在
`vkQueueSubmit` 按各自 command object 身份保留工作并于提交点回放；本项目先前
的 Metal driver 在创建下一 buffer 时调用 `FinishReplayCommands()` 提前提交上一
buffer。这是回放状态缺口，并非 UE 为 RenderDoc 特意适配，也非 Metal 禁止
多个未提交 buffer。现已按捕获的 commit 顺序保留各 buffer 的 encoder、
Shared CPU 写入及逐事件状态；定向证据见
[UE 5.8.3 本批记录](UE58_M4_INTERLEAVED_REPLAY_2026-09-29.md)。
