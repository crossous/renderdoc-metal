# 阶段 36：T35 command queue/buffer/compute encoder 创建变体

状态：实现和自动验证完成，用户 GUI L4 待验；阶段保持开放。

## 已实现

- `newCommandQueueWithMaxCommandBufferCount:` 保留并验证非零容量。
- `commandBufferWithUnretainedReferences` 与 `commandBufferWithDescriptor:` 重建
  command buffer；descriptor 保留 retainedReferences 和 errorOptions。
- `computeCommandEncoderWithDispatchType:` 与 `computeCommandEncoderWithDescriptor:`
  保留 Serial/Concurrent dispatch type，并走统一 compute pass/action 初始化。
- `waitUntilScheduled` 透传真实等待并记录结构化 chunk；replay 读取时由前置 commit
  提交命令，避免在 capture 加载阶段额外阻塞。

原 descriptor 的其他平台字段和 sample-buffer attachments 本轮没有序列化；fixture
只覆盖已明确建模的字段，文档不声称 descriptor 全量支持。

## 自动证据

T35 原生输出 buffer 为 `17,17`；capture/XML 含五个创建变体及一条
`waitUntilScheduled`。Replay API 看到两条
dispatch、同一 output buffer 的 offset 0/4、正确 CS_RW usage 与最终值；4 类非法
queue/dispatch/error options capture 被拒绝。3-loop CLI replay 和 lifecycle 通过。
GUI 项见 `QA_BATCH35-37.md`。
