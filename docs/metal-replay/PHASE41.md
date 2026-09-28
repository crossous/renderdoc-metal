# PHASE41：T40 compute inline、offset、threadgroup memory

状态：实现与最终联合终端 batch 通过；GUI L4 待验，阶段开放。

新增 Compute `setBytes`、`setBufferOffset`、`setThreadgroupMemoryLength` 的 ObjC bridge、
capture、chunk 路由与 native replay。三个入口原先只有隐式转发，不在未接通标记计数里。
新 chunk 1252–1254 追加到末尾，不改变旧 ID。

T40 由两个 compute encoder 完成五次 reduction，交替使用 dispatchThreadgroups/Threads。
8 threads、两块动态 threadgroup uint 内存，求和 `(84 + 8*bias)*multiplier`，结果写入
80-byte buffer 的 offsets 0/16/32/48/64：`92,200,108,232,124`，每个值之后12 bytes为零。
常量 slot2 在 inline→真实buffer→inline 间切换；输出 slot0 连续修改 offset，常量真实
buffer 最后从offset0改到16。inline调用后修改CPU数组，验证captured值仍为调用时副本。
最终 framebuffer RGBA `92,200,108,255`。

Replay 状态记录 inline 的长度（无虚假 ResourceId），真实buffer绑定后清除inline状态；
用 reflection 检查active buffer最小大小/对齐，dynamic threadgroup必需槽与最小元素大小。
所有三种dispatch路径增加真实pipeline/device线程限制，以及static+dynamic memory总量检查。
新encoder清除inline/threadgroup状态。不能仅凭reflection证明任意shader索引安全。

重要边界：本机 Metal API Validation 明确拒绝 `setBytes` 后直接 `setBufferOffset`；
正式fixture只对已有MTLBuffer使用offset接口，畸形测试保证inline/未绑定槽被干净拒绝。
inline数据由API Inspector查看；未新增inline常量解码UI或threadgroup专用Pipeline字段。
保留旧texture-output/indirect布局和depth=1限制，不声称通用3D compute已完成。

必跑：T40 validation-layer native、capture/XML、完整80-byte readback/seek/状态/像素、
41类畸形capture；涉及共有dispatch校验，联合重放全部T01–T41与T10_debug，含旧compute异常。
GUI项并入 `QA_CONSOLIDATED.md`，不在本轮执行。
