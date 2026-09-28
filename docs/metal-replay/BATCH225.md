# BATCH225：indexed triangle 的 index buffer 偏移

新增末尾 chunk `buildIndexedTriangleOffset`，捕获并回放 UInt16/UInt32
索引类型、非零 indexOffset 与 opaque 标记；在原有 indexed build 保持
旧格式的同时，检查索引对齐、偏移与剩余 buffer 容量。

T225 使用 UInt16 buffer 前缀 4 个无效索引，实际三角形从 byte offset 8
开始。Metal Validation 原生 1 帧与注入捕获 3 帧均得到 TLAS 三条 ray
`1/0/1`、render ray `1`；API/CLI 回放相同，15 个畸形 chunk 被拒绝。
T01/T02/T09/T11/T12/T35/T49/T52/T53/T137/T167–172/T218–225
共 24 份 API/CLI 哨兵通过；T35/T167/T218/T223–225 共 6 份×10
生命周期打开通过，resident growth 655360 bytes。

捕获 SHA-256 `11b4a39d7352…`；库与 app 内嵌库
`99941fffb20b…`，app 只构建未启动。累计脚本支持
`RENDERDOC_METAL_LAST_TEST=225`，未跑完整长时回归。
原始计数仍为 55 个 bridge 调用（另有定义 1）、17 个未处理 chunk
宏匹配（含定义 1）；消除的是现有 bridge 的限制条件。GUI L4 累计
187 份待验。06:26 IOGPU panic 根因未知，短时测试未见新 panic；
仍暂停完整 GPU 压力回归与 UI/Computer Use。
