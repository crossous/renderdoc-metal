# BATCH63–64：动态顶点布局与 descriptor-backed shared texture

承接 BATCH62 的 114 bridge / 74 旧 chunk。本波接通 **6 个 bridge、2 个旧 chunk**：

| 族 | 接通范围 | 自动证据与边界 |
| --- | --- | --- |
| T63 动态顶点布局 | Render `setVertexBuffer:offset:attributeStride:atIndex:`、批量 `setVertexBuffers:offsets:attributeStrides:withRange:`、`setVertexBufferOffset:attributeStride:atIndex:`、`setVertexBytes:length:attributeStride:atIndex:` 四重载；旧 `setVertexAmplificationCount:viewMappings:` chunk1109 | 四阶段真实三角形、动态/静态 stride、pipeline 重绑、批量绑定、offset-only 与 inline bytes，反向/正向 seek 后精确核对顶点资源、offset、stride、像素；amplification **仅 count=1 且零映射或空映射**。其它多视图映射明确拒绝，不声称 multiview 已接通。新增 43 个畸形输入。 |
| T64 Shared Texture 创建 | `MTLDevice newSharedTextureWithDescriptor:` 及旧 chunk1011 | 仅 Private、2D、RGBA8/BGRA8、单 mip/slice/sample、合法 usage 的 descriptor；三次 GPU clear 后经同一 texture 采样，核对资源身份、像素与跨事件 seek。Metal 原生验证确认此 API 不允许 Shared storage mode；已把实现与 fixture 改为 Private。**shared handle 导入/导出仍未接通**。新增 28 个畸形输入。 |

新增 chunk1278–1281 对应四种顶点 stride 重载，`MetalChunk::Max` 从1278增至1282。
其余两个接通路径复用既有 chunk ID。当前剩余 **108 bridge / 72 旧 chunk**。

一键终端门禁：

```sh
bash util/buildscripts/scripts/test_metal_capture_batch64_macos.sh
```

此入口包含 T64 native/capture、T01–T64 与 T10 debug 的 API/CLI 回放、Metal 验证层、
已登记畸形输入与 lifecycle；T63 原生/capture/输出断言由本波定向与集中门禁覆盖。
最终代码上 **65 份 capture、1855 个畸形输入、650 次 lifecycle 打开**通过，resident
growth 2392064 bytes；日志 `/tmp/metal-batch64-final.log`。T63/T64 capture SHA-256 分别为
`9d88a151f271a3fd7604b099c5a4e641da1c8fae7b1182ad12314273611021aa`、
`10400b736950038a2fd8b672dd337f846149bef4104cfe1f83e478f452c67557`；库与 app 内库均为
`8772ed87e268e8731582d8328a67370d823e4b51b86d9974b482cf48875b573a`，GUI executable
`fe8bcf852b683bc463a3be883e6c54208b7bd45054a24f5dbace58c912346d74`（未改）。
GUI/Computer Use 未运行，T63/T64 加入 [QA_CONSOLIDATED.md](QA_CONSOLIDATED.md) 的
集中人工清单，共 T34–T64 + T10 marker **32 份**；未提交或推送。

下一波若要继续降低标记，优先拿真实目标帧的阻塞 API 排序。heap aliasing、mesh/tile、
ray tracing 与跨进程 shared handle 不能通过原生透传假装可离线回放。
