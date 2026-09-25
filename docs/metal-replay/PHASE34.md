# 阶段 34：T33 GPU 生成参数的 compute indirect dispatch

状态：P34.1–P34.4 功能、自动验证及用户 GUI L4 均通过；阶段已关闭。
用户已确认 CS 间接参数栏与 Event Browser 实际 threadgroup 数量摘要。
基于 T32 的间接 compute dispatch，T33 在同一
command buffer 中先由 compute kernel 写入间接参数，再由后续 compute encoder
读取并 dispatch。与 `PHASE33.md` 组成 `BATCH33-34.md`。

## 验证清单

- L1：T33 native 固定参考、正式 capture/XML、GPU 生成的 12 字节参数、
  Replay API 的参数与输出 readback、action/state/usage、写入前/写入后/
  dispatch 后 seek、CLI replay、异常参数拒绝和本场景 lifecycle。重点验证
  replay 使用 GPU 写入后的值，而非 capture 时的初值。
- L2 必跑：T32/T11/T28/T29/T30/T31、T10（encoder 间可见性）、
  T13/T21（间接参数）、T01（最终输出）。
- L2 条件：改动资源初始内容/所有权，加 T00/T09；改动通用事件树或 pass
  scope，加 T07/T08/T22/T25；改动 render pass/attachment，加 T06/T07/T08；
  改动通用 descriptor/reflection，加 T12/T16/T17/T18/T19。新风险先补 T 编号。
- L4：批末用户检查参数写入前后 Buffer Viewer、间接 dispatch 的 Event/API、
  CS Pipeline、Texture Viewer 输出及 `No problems detected`；新导出入口若有
  改动，合并进同轮验收单。
- L3 默认不执行；触发条件与 T00–T33 的完整范围见 `BATCH33-34.md`。

## P34.1 fixture/native

用第一个 compute encoder 写入 shared 参数 buffer 的非零 offset，第二个
encoder 执行间接 compute dispatch；固定写入值、输出纹理像素与哨兵区域。
先证明未注入运行和跨 encoder 可见性正确。

## P34.2 capture/replay/state

保留 GPU 写入与间接读取的资源依赖和事件顺序；Replay API 核对写入前、
写入后、dispatch 后的参数与输出，并验证前后 seek。异常 buffer/offset/grid
稳定拒绝，不以 capture 时 CPU 快照替代 GPU 生成值。

## P34.3 标准 Viewer

自动核对 Buffer/Texture/CS Pipeline、资源 usage、输出 DDS/raw 与事件切换；
将可见交互留给批末用户 L4。

## P34.4 批次收口

最终构建按 `BATCH33-34.md` 去重执行联合 L0/L1/L2、逐份 CLI replay 和
lifecycle，按触发条件决定 L3。准备 T32/T33 正式 captures 与合并 GUI
验收单；用户明确验收后再关闭阶段及批次。
