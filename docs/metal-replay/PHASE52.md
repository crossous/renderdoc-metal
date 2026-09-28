# PHASE52：MTLEvent 创建、Signal 与 Wait

T52 / `Metal_Event_Sync`。终端自动通过，GUI L4 待验，阶段开放。
接通三个 bridge / 三个旧 chunk：newEvent 1032、wait 1062、signal 1063。
与 PHASE51 合并后剩余 **149 bridge / 90 实际未处理旧 chunk**，Max1270。

## 实现与边界

- 新增 Event wrapper/proxy、资源序列化、label/device 转发和 Sync 资源登记；捕获 command
  buffer 的 signal/wait、event 身份和值，并保留帧引用。eResEvent 追加在 Fence 之后。
- Replay 使用真实 Metal Event 编码，不是忽略同步的空实现。要求当前未提交 command
  buffer、无活动 encoder、正确 Event 资源、signal 严格递增；wait 必须不大于本次 replay
  已编码的 signal 值（初值 0）。非法值/错类型/未知资源在 GPU wait 前拒绝。
- Native Event 值不能回退；每次新 replay epoch 首次使用时重建 native Event，清零跟踪
  值。前次提交先完成，WithoutDraw/OnlyDraw 共享同一 epoch，避免从后面事件回退后继承
  旧 signalled value。已覆盖 signal/wait 之间、跨提交和 draw 前后往返。
- 支持已捕获的先 signal 后 wait，包括两队列和同 command buffer 的 signal→wait。
  现有 replay 仍串行完成各提交；**不声称重建任意多队列并行调度**。未来 signal 的 wait、
  外部/帧前状态、SharedEvent/import/listener 均未支持，不能假装等待已经满足。
- 非包装 event 在 bridge 中仍传给原生调用，但捕获为 unresolved event，离线明确拒绝，
  不将原生 ObjC 对象误转为 wrapper。newSharedEvent 原有未接通入口未改。

## Fixture / 自动断言

两队列、两事件，每帧三提交：A fill→signal；B wait→clear→compute→signal；A wait→
draw→signal→同提交 wait。原生验证层与注入 capture 各 12 帧、各 72 次 signal/wait。
事件值每帧递增，捕获帧六调用为 v/v/v+2/v+2/v+4/v+4；XML 验证事件及 CB/queue 身份。

输入 436 bytes、输出 444 bytes；compute 写 96 个 uint 为 51+i，尾 60 bytes 为零。
API 验证两个 Sync 资源、六条同步 API、两 fill/一 dispatch/一 draw、精确数据/padding、
usage、逐事件回退及最终 RGBA51/83/115/255。不断言首次 clear 前未定义的 output 内容。

122 类异常覆盖六调用的资源/CB 空值、未知/错类型/过期身份；wait 超前、signal 零值/
重复/下降；事件重复创建；删除 signals；活动 encoder 内或 commit 后编码。每例 30 秒
超时，必须干净非零且无信号退出，全部通过。合法 wait=0/1 两变体各 3-loop 通过。
SharedEvent 探索触发原有未接通入口，已排除正式 fixture，不计入支持或验证通过范围。

## 联合回归与 UI

Event 增加资源类型及 replay epoch 路径，必跑 T44–T46 Fence、T49 CPU 更新回退、T51
异步 PSO；合并批末跑 T01–T52 + T10 marker 共 53 份，详见 BATCH51-52。
后续若扩展 future-signal wait 或跨队列调度，必须新增能证明不会死锁的 native/capture/
replay/负例链，不仅放宽当前校验。通用资源/epoch 修改继续触发共享全量回归。

GUI 检查六条同步参数/资源身份、dispatch/draw 回退与像素；无 Event 专用面板、GPU
时间线或并行时序展示要求。最小步骤已合入 QA_CONSOLIDATED。
