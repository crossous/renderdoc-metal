# 阶段 16 细化计划：T15 point/line 基础拓扑

T14 indexed instancing/base vertex 关闭后，继续补 M5.4 尚缺的常见 primitive topology。
本阶段只承诺直接 point/line draw；indexed line、indirect line、线宽扩展和几何 shader 不混入。

当前状态：P16.1-P16.4 全部通过，T15 已关闭；下一项为 `PHASE17.md` P17.1。

## 实现与验证记录

- T15 `Metal_Point_Line` 使用 11 个 interleaved Float2/Float4 顶点，首尾及三段之间设视口外哨兵；
  三次直接 draw 分别为 Point `(start=1,count=1)`、Line `(3,2)`、Line Strip `(6,3)`。
- 原生 BGRA readback、RDC XML 与 replay smoke 验证三种颜色、端点、事件边界、action topology、
  非零 vertex offset、264-byte 顶点数据和 clear→draw→回退图像。细线在光栅化边界附近以小范围
  像素断言，避免把单个非覆盖像素误判为失败。
- 通用 `PipeState`、VS Input、Mesh preview、标准 Buffer Viewer/Resource usage 与 480128-byte
  DDS export 已由自动 smoke 覆盖；从 XML+ZIP 派生的非法 primitive 99 与零 vertexCount capture
  均在 `drawPrimitives` chunk 被明确拒绝。T01/T02/T05/T14 定向 smoke 均通过。
- `/tmp/t15-final-regression.log`：T00-T15 原生/capture/XML/replay/output、逐份 CLI replay、
  16×10 lifecycle 全部通过，resident growth 737280 bytes。
- L4 在完全重启最新 qrenderdoc 后载入正式 T15 RDC：Event Browser 的 Point/Line/Line Strip
  action、EID 2/3/4 Pipeline 拓扑、Line Strip API 的 `primitiveType/vertexStart=6/vertexCount=3`、
  标准 Buffer Viewer 的全部顶点值、Buffer 16 的 Resource Inspector `Vertex Buffer` usage，以及
  Point/Line/Line Strip Mesh VS Input 均可见核对。Texture Viewer 显示红点、绿线、蓝折线；
  UI 保存的 `captures/metal-smoke/t15_output_ui.dds` 与自动 DDS SHA-256 均为
  `4bedcfdcddf745e379a2f693eac75a5dedec53df32ffba7ddad9011f60de0a5c`。
  `t15_pipeline_state_standard.html` 包含 Line Strip 和 Buffer 16；状态栏为
  `No problems detected`。

## P16.1：fixture 与 native 语义

- 建立确定性的 point/line fixture，显式区分 Point、Line、Line Strip，并以颜色和端点哨兵
  验证 primitive count、vertex start 与拓扑。
- 固定原生 readback、输入 buffer 字节、每个 draw 的预期覆盖区域。

验收：原生像素与 CPU 参考一致，三个 draw 的事件边界可区分。

## P16.2：capture/replay/action

- 复用直接 `drawPrimitives` 纵向链路，补齐需要的 Metal topology 转换、event snapshot 与
  GPU replay；不改变 T01/T05/T13 已验证的 triangle 路径。
- 定向断言 XML、action topology、vertex offset、clear/draw/回退图像及错误参数诊断。

验收：三个 draw 的 capture/replay/output 一致。

## P16.3：标准 Viewer

- IA Pipeline、标准 VS Input/Mesh Viewer、Buffer Viewer 与 API/Event 对同一 draw 展示正确
  资源、拓扑和 offset；补充标准导出/资源跳转。

验收：自动 smoke 与最新 qrenderdoc 对同一 EID 和像素一致。

## P16.4：阶段收口

- 开发中只跑 T15 与受影响的 T01/T02/T05/T14 draw 路径定向验证；阶段末运行一次 T00-T15
  完整回归和一次最新 qrenderdoc 实机验收。
- 同步 README/STATUS/PLAN/HANDOFF/TEST_MATRIX/DECISIONS，给出后续任务提示。

验收：完整回归通过，qrenderdoc 状态栏为 `No problems detected`。
