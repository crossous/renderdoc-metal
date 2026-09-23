# 阶段 20 细化计划：T19 vertex storage buffer 直接绑定

T18 fragment storage buffer 已关闭。下一条切片补 vertex shader 的只读 storage buffer，
让物理 buffer slot、offset、reflection、`VS_Resource` usage、标准 Pipeline/Buffer Viewer
和事件回放与 T18 的 fragment 语义对齐。保持单个确定性 fixture，不扩展到 compute 或 ICB。

当前状态：P20.1-P20.4 已于 2026-09-24 关闭。T19 及必跑 T18/T16 通过；因实现修改了
`setVertexBuffer` 的普通 vertex input/storage 分类，条件清单中的 T02/T05 也已执行并通过。
定向证据足以界定影响范围，未触发 L3；最新 qrenderdoc L4 已通过。

## 本阶段验证清单

- L1 必跑：T19 的 native、capture/XML、replay/readback/state、CLI replay 和本场景 lifecycle。
- L2 必跑：T18（storage buffer 的 reflection/descriptor/usage 与 Buffer Viewer 共用路径）；
  T16（vertex shader 资源绑定、VS Pipeline 与资源跳转）。只运行这两个场景的相关断言。
- L2 条件触发：若修改 `setVertexBuffer` 对普通 vertex input 的绑定或 Mesh/IA 状态，
  加跑 T02（基础 vertex/index/IA）与 T05（多 vertex buffer/per-instance）。若又触及其他
  旧路径，先在此清单加上 T 编号和原因，再运行对应定向测试。
- L4 必跑：最新 qrenderdoc 打开 T19，核对 Event/API、VS Storage Buffers slot/offset/size、
  used/unused、标准 Buffer Viewer、Resource Inspector、资源跳转、HTML/raw export 和状态栏。
- L3 默认不执行：本阶段不是较大里程碑。若定向失败表明风险跨越上述 T 编号，且无法继续
  合理圈定范围，或进入发布/合并门槛，则升级为 T00-T19（含两端）全部场景的 native、
  capture/XML、replay/output、逐份 CLI replay 与 lifecycle 回归，并在 `STATUS.md` 记录原因。

## 本阶段完成证据

- P20.1：新增 `Metal_Vertex_Storage_Buffer`。768-byte shared buffer 在 offset 256 保存 24 个
  `float4` 位置，slot 4 被 vertex shader 使用，slot 6 绑定同一 buffer 但静态未使用；前后哨兵和
  红/绿/蓝/黄四象限 native readback 通过。
- P20.2：vertex storage 已进入 event snapshot、source MSL reflection、通用 descriptor 和
  `VS_Resource` usage，并与 IA vertex buffer 分离；slot 4 范围为 256+512，slot 6 为 320+448。
  replay 对越界 slot/offset 给出明确错误。T19 capture/XML、clear/draw/回退、raw data、CLI replay
  和 10 轮 lifecycle 通过。
- P20.3：VS Pipeline 新增标准 `VS Storage Buffers` 表，used/unused、Buffer Viewer、Resource
  Inspector、资源跳转及 HTML/raw 导出均使用同一 descriptor 范围。Metal 的 `VS_Resource` 文案
  显示为 `VS - Resource`，不再误报 texture。
- P20.4：最终增量构建与 T19/T18/T16/T02/T05 定向验证通过，`git diff --check` 通过。L3 未触发，
  因为分类公共路径已有 T02/T05 覆盖且所有定向测试均为绿色。最新 qrenderdoc 打开与正式 capture
  SHA-256 一致的 `/tmp/t19-ui.rdc`，核对 EID 2、`drawPrimitives(24)`、IA 空表、VS slot 4/6、
  256+512 Buffer 范围、四象限输出、`VS - Resource` 与 `No problems detected`；UI 产物为
  `/tmp/t19_pipeline_state_standard.html`、`/tmp/t19_storage_ui.csv` 和
  `/tmp/t19_storage_ui.bin`，其中 512-byte bin 与自动导出的对应子范围逐字节一致。

## P20.1：fixture 与原生语义

- 新增 `Metal_Vertex_Storage_Buffer`：source-created vertex MSL 以非零 `device const float4 *`
  物理 slot 读取颜色或位置；使用非零 byte offset、前后哨兵和可判别的四象限输出。
- 固定 CPU 参考像素、buffer 字节和可见范围，并设计错误 slot/offset 可被像素识别的输入。

验收：未注入 native readback 与 CPU 参考一致。

## P20.2：capture/replay/state

- 锁定 `setVertexBuffer` 的 XML、初始内容、event snapshot；把只读 vertex storage 纳入
  shader reflection、通用 Buffer descriptor 与 `VS_Resource` usage，不误归为 vertex input。
- 定向验证 clear/draw/回退、raw bytes、未绑定 slot、非法 offset/slot 的明确失败。

验收：T19 capture/replay 与 native 一致；按本阶段验证清单完成必跑和已触发的旧路径。

## P20.3：标准 Viewer

- VS Pipeline 增加 Storage Buffers 表；核对物理 slot、offset、size、used/unused、标准
  Buffer Viewer、Resource Inspector 与 HTML/raw export。

验收：自动 smoke 与最新 qrenderdoc 对同一 EID、资源范围和输出一致。

## P20.4：阶段收口

- 接手只读 README、STATUS 当前阶段与最新检查点、PHASE20 和 HANDOFF 验证规则；PLAN、历史阶段按需查阅。
- 按本阶段验证清单运行 L1/L2/L4；只有清单中的 L3 条件触发时才跑 T00-T19 全量回归。
- 同步当前阶段和必要索引文档；下一任务提示保持简短，不复制历史证据。

验收：T19 与清单所列旧路径的定向验证通过；qrenderdoc 的 Event/API、VS Storage Buffers、
标准 Viewer、资源跳转、保存/export 与 `No problems detected` 均通过。若触发 L3，再记录全量结果。
