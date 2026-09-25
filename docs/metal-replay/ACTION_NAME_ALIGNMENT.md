# Metal Event Browser action 名称对齐检查（2026-09-26）

状态：已审查，列为后续显示一致性工作；本文件不改变 replay、capture 或 GUI。
T32/T33 的间接 compute 摘要已由用户复验通过，BATCH33-34 可关闭。

## 通用显示规则

`qrenderdoc/Windows/EventBrowser.cpp` 的 `GetCachedEIDName()` 在启用
**Show custom action names** 时优先显示 `ActionDescription.customName`；没有
自定义名称时，从 structured chunk 名称与标记为 `Important` 的参数生成行名。
关闭该选项后一般显示 chunk/参数；普通用户 debug marker 仍显示用户给的文字。
API Inspector 则保留完整调用参数。因此 custom name 是后端为事件摘要写的
选择性文本，RenderDoc 没有一份强制所有 API 使用同一字符串的规范。

跨 Vulkan、D3D11、D3D12 和 OpenGL 的实际惯例是：直接 draw/dispatch/copy
通常依赖 chunk 与重要参数；**indirect** action 解析实际执行参数，用尖括号
摘要显示 dispatch 三维数量，或 draw 的顶点/索引数与实例数；多 draw 的父行
标命令数量，子行标每条实际数量。Buffer、offset、base vertex/instance、
threadgroup size 等详情留给 API Inspector 和 Pipeline State。Vulkan render
pass 的 Begin/End 是例外：自定义名称显示 attachment load/store 概况。

## Metal 审查结果

| 类别 | 当前 Metal | 对照与结论 | 后续处理 |
| --- | --- | --- | --- |
| T32/T33 compute indirect | `dispatchThreadgroups(indirect, <2, 2, 1>)` | 与 Vulkan `vkCmdDispatchIndirect(<x, y, z>)`、D3D11 `DispatchIndirect(<x, y, z>)`、GL 和 D3D12 显示实际 group 数的惯例一致；Metal API 名称保留 | 无；本次验收关闭 |
| 直接 compute | `dispatchThreadgroups(2x2x1, 4x4x1)`、`dispatchThreads(7x5x1, 4x3x1)` | 其他后端的直接 dispatch 通常走 chunk/重要参数；Metal 同时把 group/grid 和 threads-per-group 硬编码在摘要中，格式与间接摘要不一致。`dispatchThreads` 的 thread grid 是 Metal 特有且应可见 | 优先改用 chunk/Important 参数；group/grid 与 threads-per-group 均可在展开参数和 CS 看，Event Browser 的精简显示须保留实际 group/grid 数 |
| 直接 draw | 基础调用自定义 `drawPrimitives(4)` 等；indexed instanced 行还列 baseVertex/baseInstance | 其他后端直接 draw 主要用 chunk/Important 参数；后者详情显示过多，且实例数字在 Metal 各重载间不统一 | 统一顶点/索引数和实例数的摘要规则；base、起始位置、buffer offset 留给 API Inspector/IA/Mesh；核对 Point/Line 拓扑在 IA 仍可见 |
| 单次 indirect draw | 非 indexed 为 `drawPrimitives(indirect, 3 vertices, 2 instances)`；indexed 还列 indexStart/baseVertex/baseInstance | GL、D3D11、Vulkan 和 D3D12 的解析后摘要核心是 `<count, instances>`，详细字段由 action/state 提供 | 两种 Metal 路径统一为保留 API 名称和 `<顶点或索引数, 实例数>` 的形式，其他参数留在 Inspector/IA；验证 T13/T21 |
| ICB execute 与子 draw | 父行 `executeCommandsInBuffer(location=…, length=…)`，子行 `ICB[n] draw…(count) instances=count` | 父子层级和 seek 已验收，与 Vulkan multi indirect、D3D12 ExecuteIndirect 的结构一致；父行的 location/length 是 Metal range 的必要区别，且此前用户已要求清楚标明起点与长度 | 保留父行的 `location`/`length` 与已验收层级/seek；子行改为索引及 `<count, instances>`，覆盖 T20–T27 |
| render/blit/compute pass | render Begin/End 标 load/store，blit/compute Begin/End 标边界 | render pass 对齐 Vulkan；blit/compute 是 Metal encoder 的真实边界 | 保留；后续仅核对标注词与 attachment usage 一致 |
| blit copy/fill/mipmap | 部分自定义摘要列 bytes、offset、mip/slice、填充值，部分无摘要 | D3D11/D3D12/GL 的同类普通动作大多走 chunk/Important 参数；Metal 同类调用的详细程度不统一 | 统一从 chunk/Important 参数生成摘要或定义一致的短摘要；源/目标资源与大小可见，offset/mip/slice 等细节在 Inspector/Viewer；验证 T10 |
| present、capture 结束 | present 带图像 ID，`End of Capture` | Vulkan present 与其他驱动的结束标记采用同类方式 | 保留 |

## 后续实施顺序与验证

1. 先定一份 Metal action 命名表：直接调用以 chunk/`Important` 为基线；
   indirect 显示解析后的执行数量并用尖括号；多命令父行概括范围或数量，
   子行显示每条执行数量；pass/present/用户 marker 保留必要的自定义名称。
   明确 **Show custom action names** 开关两种显示，以及 API Inspector 中
   始终可找到所有原始参数。直接 compute 的 group/grid 标为 `Important`，
   threads-per-group 留在 API Inspector/CS；直接 draw 的 count 与有意义的
   instance count 保持可见，base/offset 不放在精简行；blit 的源、目标及
   传输量保持可见，其他子资源细节留在 Inspector/Viewer。
2. 第一组修正直接 draw/dispatch 与单次 indirect draw；只改事件文字或
   `Important` 标记，不改 EID、action flags、回放和资源内容。自动核对
   T01/T05/T11/T13/T14/T21/T28/T32/T33 的行名、参数和画面。
3. 第二组修正 ICB 父/子行与 blit；自动核对 T10、T20–T27 的层级、
   父行 seek、子 action 数、名称和输出。必要时让用户在一个最终构建上
   只复验变更过的 Event Browser 行，不重复原场景完整 L4。

这些显示改进与 T32/T33 功能验收分开跟踪；执行前按当时批次清单补齐
定向验证及 GUI 待验登记。保留现有所有未提交改动。
