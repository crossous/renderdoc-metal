# 阶段 35：T34 render inline bytes 与 buffer batch binding

状态：实现和自动验证完成，用户 GUI L4 待验；阶段保持开放。

## 已实现

- `setVertexBytes:length:atIndex:` 与 `setFragmentBytes:length:atIndex:` 捕获实际字节，
  replay 限制为 Metal 的 4096-byte inline 上限，并清除对应旧资源绑定状态。
- `setVertexBuffers:offsets:withRange:`、`setFragmentBuffers:offsets:withRange:` 保留
  range、null/bound 信息、资源与 offset，验证数组长度、slot 和 buffer 边界。
- `setVertexBufferOffset:atIndex:` 更新真实 encoder、ICB 前两槽缓存和 Replay API
  的 vertex/storage buffer offset/size。

## 自动证据

T34 原生像素、capture/XML、五个新 chunk、Pipeline State 的 VS storage slot 0/1
offset 16/256、FS slot 3/4 offset 0/16、usage 和中心 RGBA
`(0.5625, 0.5, 0.5, 1)` 均通过。十类 T34 畸形 range/resource/offset/slot capture
被拒绝；CLI replay 和 lifecycle 通过。GUI 项见 `QA_BATCH35-37.md`。
