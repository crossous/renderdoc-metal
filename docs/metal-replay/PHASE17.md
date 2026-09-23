# 阶段 17 细化计划：T16 vertex texture/sampler binding

T15 point/line 基础拓扑关闭后，继续补 M4.7 尚缺的 vertex stage texture/sampler 绑定。
本阶段只处理受控 source-created MSL 的直接 vertex texture2d/sampler slot；批量绑定、
argument buffer、vertex storage buffer、function constants 和预编译 metallib 分开验证。

当前状态：P17.1-P17.4 全部通过，T16 已关闭；下一项为 `PHASE18.md` P18.1。

## 实现与验证记录

- `Metal_Vertex_Texture` 使用 384-byte Float2 position/Float2 UV vertex buffer、2×2 RGBA8
  texture 和 Point/Clamp sampler。vertex shader 在 `texture(0)`/`sampler(0)` 采样四个 texel，
  fragment shader 只透传颜色；四个六顶点 quad 分别产生红、绿、蓝、黄，未绑定资源时无法得到
  该输出。未注入原生 BGRA readback 核对四象限与 clear 边框像素。
- 直接 `setVertexTexture`/`setVertexSamplerState` 已接通 bridge、wrapper、chunk、frame reference、
  GPU replay 和 event snapshot；vertex 与 fragment descriptor offset 分离，source-created MSL
  的 vertex argument reflection 驱动标准 used/unused 过滤。draw action 记录 `VS_Resource` usage。
- T16 XML、资源原始字节、VS Input、通用 descriptor/reflection、clear/draw/回退像素、标准 DDS
  export 由自动 smoke 核对；T03/T12 fragment 定向 smoke 均通过。由 XML+ZIP 派生的 texture/
  sampler slot 128 与空资源四份 RDC 均在对应 chunk 被拒绝。
- `/tmp/t16-final-regression.log`：T00-T16 原生/capture/XML/replay/output、逐份 CLI replay、
  17×10 lifecycle 均通过，resident growth 114688 bytes。正式 L4 使用最新 qrenderdoc 打开
  与 `captures/metal-smoke/t16_capture.rdc` SHA-256 相同的 `/tmp/t16-ui-capture.rdc`；EID 2 的
  Event/API、VS Texture 17/Sampler 18、Point/Clamp、标准 Texture/Buffer/Mesh VS Input、
  Resource Inspector `VS - Texture` usage 与四象限输出均正确。UI DDS 与自动 DDS 均为
  480128 bytes，SHA-256 同为 `95597aa5bcb91a27f057df2f4f36cbeb49423ac7e5275d5ed17ca29b03ca21cf`；
  `t16_pipeline_state_standard.html` 包含 VS Texture/Sampler；状态栏为 `No problems detected`。

## P17.1：fixture 与 native 语义

- 建立确定性的 T16：顶点 shader 从小型 RGBA8 texture 采样顶点位移或颜色，使用显式 vertex
  texture/sampler slot，并保留未绑定时可辨认的输入哨兵。
- 固定 texture 字节、vertex buffer、原生像素和 CPU 参考。

验收：未注入运行的多个采样区域和边界像素符合 CPU 参考。

## P17.2：capture/replay/state

- 接通直接 `setVertexTexture`/`setVertexSamplerState` 的 wrapper、ObjC bridge、资源引用、
  序列化、GPU replay 和 event snapshot；保持 T03 fragment 绑定路径原有语义。
- 断言 XML、binding slot、resource usage、readback、clear/draw/回退及无效 slot/资源诊断。

验收：T16 capture/replay 输出和原生一致，vertex stage 状态来自同一事件。

## P17.3：标准 Viewer

- 在 VS Pipeline、通用 descriptor/reflection、Resource Inspector 和标准 Texture Viewer 中
  显示 vertex texture/sampler；验证资源跳转、显示内容及 HTML/DDS export。

验收：自动 smoke 与最新 qrenderdoc 对同一 EID、资源和像素一致。

## P17.4：阶段收口

- 开发中运行 T16 与受影响的 T03/T12 fragment texture/sampler 路径定向验证。
- 阶段末运行一次 T00-T16 完整回归及一次最新 qrenderdoc 实机验收。
- 同步 README/STATUS/PLAN/HANDOFF/TEST_MATRIX/DECISIONS，留下下一阶段任务。

验收：完整回归通过，qrenderdoc 状态栏为 `No problems detected`。
