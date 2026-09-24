# 批次 31–32：compute sampler 与批量资源绑定

本批固定两条相邻切片：`PHASE31.md` / T30 compute 直接 sampler 绑定，
`PHASE32.md` / T31 compute texture、sampler、buffer 的批量绑定。
从 P31.1 连续推进到 P32.4；T30 自动通过后记录批末 GUI 待验并直接进入 T31。
上一批 BATCH29-30 已完成自动验证与用户 L4，T22/T25 的事件树复验也已通过；
`QA_PENDING.md` 当前无待验项。

## 功能边界与顺序

1. T30 使用 source-created MSL 的 2D texture + compute sampler，构造可区分
   Point/Linear 或 Clamp/Repeat 的确定性输出。接通直接 `setSamplerState:atIndex:`
   capture、XML、replay、CS Pipeline、通用 descriptor/reflection、usage 与 Viewer。
2. T31 使用非零起始 slot 的 `setTextures:withRange:`、
   `setSamplerStates:withRange:`、`setBuffers:offsets:withRange:`；覆盖空槽清除、
   后续直接覆盖、已声明未使用槽，以及读/写资源范围。固定原始字节与输出。
3. 两条都按 fixture/native → capture/XML → replay/state/readback/seek →
   自动检查顺序完成。最终构建上去重跑联合清单、逐份 CLI replay 和 lifecycle，
   给用户同轮 qrenderdoc 的合并 T30/T31 L4 验收单。L4 未收到对应 T 的明确
   反馈前不关闭该阶段或批次。

## 最终联合验证清单

- L0：增量构建 qrenderdoc、renderdoccmd、受影响 demo/smoke；脚本语法、
  Python 编译与 `git diff --check`。
- L1：T30/T31 各自 native、正式 capture/XML、Replay API action/state/descriptor/
  usage/readback/seek、输出像素与原始字节、错误资源/slot/range/offset 拒绝、
  各自 CLI replay 和 lifecycle。
- L2 必跑一次：T11/T28/T29（现有 compute）；T03/T16/T17（直接和批量
  texture/sampler）；T18/T19（buffer storage 分类）；T01（render/output 基线）。
- L2 条件：若改动通用 argument buffer 或 descriptor reflection，追加 T12；
  改动 render pass/attachment，追加 T06/T07/T08；改动资源初始内容/所有权，
  追加 T00/T09/T10；改动 ICB 或公共事件层级，追加 T20/T22/T23/T24/T25/T26/T27。
  任何新受影响旧路径先补具体 T 编号和原因，再跑测试。
- L4：由用户在当前最终 qrenderdoc 同轮检查 T30/T31 的 Event/API、CS Pipeline
  sampler 与批量 slot/空槽、Texture/Buffer/Resource 跳转、逐事件输出、保存/export
  和 `No problems detected`。准确 EID、资源编号和画面以最终 capture/自动断言
  写入验收单；未验项持续记在 `QA_PENDING.md`。
- L3 默认不执行。若 L1/L2 暴露不能由上列编号圈定的跨场景风险，或进入发布/
  合并门槛，记录原因后在最终代码上覆盖 T00–T31 全部 native、capture/XML、
  Replay API/output、逐份 CLI replay 与 lifecycle。

当前状态：计划与验证范围已写定，T30/T31 尚未实现、构建或验收。
