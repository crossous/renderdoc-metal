# BATCH226：UInt32 indexOffset 与 descriptor 分配组合

沿用 T225 的 `buildIndexedTriangleOffset`，T226 改用 UInt32 索引，
buffer 前 8 byte 放两个无效索引、实际 3 个索引从 offset8 开始，并通过
`newAccelerationStructureWithDescriptor` 分配 BLAS。这同时验证 ObjC
descriptor 的 native buffer 转换保留 indexOffset。

Metal Validation 原生 1 帧、注入捕获 3 帧、API/CLI 回放均为
compute ray `1/0/1`、render ray `1`。15 个畸形输入安全拒绝；19 份
跨族 API/CLI 哨兵通过；T35/T167/T171/T223–226 共 7 份×10
生命周期打开，resident growth 1196032 bytes。

捕获 SHA-256 `2019936bbe7f…`。库/app 内嵌库仍为
`99941fffb20b…`（本批仅改 fixture 与验收逻辑，app 未启动）。
累计脚本支持 `RENDERDOC_METAL_LAST_TEST=226`，未运行长时全量回归。
原始计数 55 bridge 调用（另有定义 1）/17 未处理 chunk 宏匹配
（含定义 1）；GUI L4 累计 188 份待验。06:26 IOGPU panic 根因未定，
短时测试未见新 panic。
