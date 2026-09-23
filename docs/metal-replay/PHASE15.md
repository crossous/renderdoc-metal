# 阶段 15 细化计划：T14 indexed instancing 与 base vertex

T13 关闭后，先补首版 P1 draw 路径中的 indexed instancing/base vertex。ICB 与 heap 继续保留为
独立 P2 决策，不混入 T14。

当前状态：P15.1-P15.4 已关闭（2026-09-23）。下一项为 `PHASE16.md` P16.1。

## P15.1：fixture 与语义基线

- 新增确定性 indexed instancing fixture，使用同一份 index/vertex/instance buffer，设置非零
  `baseVertex`、非零 `baseInstance` 和多个实例；用哨兵数据使忽略任一字段都能被像素断言发现。
- 固定 index 类型、byte offset、stride、拓扑、实例色和 native readback；盘点对应 Metal overload。

验收：原生图像和 CPU 参考一致，输入字节与各 offset 可确定。

## P15.2：capture 与 replay

- 只接通 `drawIndexedPrimitives(... instanceCount, baseVertex, baseInstance)` 的 bridge、chunk、
  序列化、GPU replay 和 action metadata；保留基础 indexed draw 路径。
- 对 index buffer、vertex/instance buffer 的 frame reference、用量和 EID seek 做定向验证；
  对无效 index 范围或参数做显式拒绝。

验收：XML、action、buffer 数据、clear/draw/回退图像一致。

## P15.3：标准 Viewer

- IA Pipeline 的 index/vertex binding、通用 VS Input、Mesh Viewer instance 选择与标准 Buffer
  Viewer 指向实际资源和 offset；Event/API、Resource Inspector 和导出保持同一 ResourceId。

验收：自动 smoke 与最新 qrenderdoc 对同一实例、索引、资源和像素一致。

## P15.4：阶段收口

- 开发中只跑 T14 及受影响的 T02/T05/T13 draw 路径定向验证；阶段末运行一次 T00-T14
  完整回归及一次最新 qrenderdoc 实机验收。
- 同步 README/STATUS/PLAN/HANDOFF/TEST_MATRIX/DECISIONS，给出下一任务提示。

验收：完整回归通过，qrenderdoc 状态栏为 `No problems detected`。

## 当前不在本切片内

- indirect indexed draw、ICB、GPU 生成参数、heap、多 queue、任意应用 capture。

## 阶段结果

- `Metal_Indexed_Instancing` 使用 UInt16 index buffer `{4,4,0,1,2,4}`，byte offset 4，
  `baseVertex=1`、`instanceCount=2`、`baseInstance=1`；vertex/instance 首尾哨兵均在视野外，
  原生 readback 固定为左红、右蓝、中央背景。
- 新增 indexed-instanced-base overload 的 bridge/chunk/capture/replay/action；index offset 对
  `MetalPipe::indexBuffer.byteOffset=4` 绑定相对解释，精确 byteSize 为 6，避免标准 Mesh/Buffer Viewer
  把 index offset 加两次。draw-time resource usage 标记 vertex/instance/index buffer。
- XML、Buffer 数据、`Indexed|Instanced` action、`baseVertex=1`、两实例 VS Input、
  clear→draw→clear→draw seek、像素和 6-byte 原始索引保存均有自动断言。由 XML+ZIP 派生的
  byte offset 3（未对齐）与 10（越界）capture 均在 replay 拒绝。
- `/tmp/t14-final-regression.log`：T00-T14 一键回归、逐份 CLI replay、15×10 lifecycle 全部通过，
  resident growth 1,015,808 bytes。最新 qrenderdoc 从工作区正式 `.rdc` 打开，EID 2 的 Event/API、
  IA Buffer 16/17/18、标准 Buffer Viewer offset 4/length 6、Resource Inspector 的 `Index Buffer`
  usage、Mesh instance 0/1、红蓝输出、Pipeline HTML、索引 CSV 保存及状态栏
  `No problems detected` 均通过。导出文件在 `captures/metal-smoke/`。
