# 阶段 31：T30 compute sampler 直接绑定

T11/T28/T29 已覆盖 compute pipeline、texture/buffer 与两种 dispatch，但尚无
compute sampler 直接绑定。T30 用固定纹理和采样坐标区分采样器状态；与
`PHASE32.md` 组成 `BATCH31-32.md`。状态：自动与 GUI L4 均通过；用户确认
公共事件树修改后的 EID 4/13 Begin/End 顶层与 `$action()` 筛选，阶段已关闭。

## 验证清单

- L1：T30 native、capture/XML、Replay API action/state/descriptor/usage/readback/
  seek、采样输出、无效 sampler/slot 拒绝、CLI replay、本场景 lifecycle。
- L2 必跑：T11/T28/T29、T03/T16/T17、T01。若涉及 buffer/storage 映射，
  补 T18/T19；批末联合清单仍包含它们。
- L2 条件：通用 argument buffer/descriptor reflection 改动加 T12；render-pass
  改动加 T06/T07/T08；blit 前后事件分组改动加 T10；初始内容/所有权改动加 T00/T09/T10；公共事件/ICB
  改动加 T20/T22/T23/T24/T25/T26/T27。其他影响先补编号。
- L4：批末用户检查 Event/API、CS sampler 的 slot/过滤/寻址、Texture/Resource、
  dispatch 前后输出、HTML/DDS 与状态栏；agent 完成全部终端可判定项。
- L3 默认不跑；按批次触发条件升级时覆盖 T00–T31 全部场景及 CLI/lifecycle。

## P31.1 fixture/native

建立最小 `Metal_Compute_Sampler`：固定输入 texel、非整数采样坐标、两种可辨
sampler 状态、输出哨兵；先证明未注入运行的像素与 CPU 参考一致。

## P31.2 capture/replay

接通 compute `setSamplerState:atIndex:` bridge/wrapper/chunk/资源引用、GPU replay、
EID 与状态快照；验证 XML、slot、descriptor/usage、逐事件 readback/seek 和错误拒绝。

## P31.3 标准 Viewer

自动核对 CS Pipeline 的 sampler/texture、资源跳转数据及 DDS/HTML 内容；
可见交互留给批末用户 L4。

## P31.4 批内转交

完成 T30 必要 L0/L1/L2 和 CLI/lifecycle，登记批末 GUI 待验，进入 P32.1。
