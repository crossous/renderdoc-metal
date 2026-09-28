# Binary Archive 资源链（T301–302、T305–312，2026-09-28）

已接通文件 URL 导入的 `MTLBinaryArchive`、有界二进制快照、回放时临时文件重定位，
以及 compute/render pipeline descriptor 的有序 `binaryArchives` 依赖。创建 chunk
`MTLDevice_newBinaryArchiveWithDescriptor`（1038）已接通；两类 pipeline 的
`FailOnBinaryArchiveMiss` 选项（bit 4）在回放保持。Metal capture 版本从 0x9
升为 0xA，旧 capture 的 descriptor 不读取新增字段。

原生探针先把 compute/render 函数加入 archive，序列化、移动并重新导入；两种
pipeline 都须在 archive-miss 选项下成功，空 archive 对照均失败。T301 再以
注入捕获和终端 API/CLI 验证：archive payload 32960 字节，两个 pipeline
共用 ResourceId 12；移走原始文件后仍能回放；GPU 读回 32 个 `i+17` 结果，
API资源图还确认该 archive 是两个 pipeline 的父资源；中心像素约为
`(0.2,0.7,0.3)`。19 例畸形资源、payload、依赖与 options
截帧被拒绝；旧截帧跨族定向回归 8 份通过，T35+T301 各 10 次生命周期打开通过。
T302 用相同 archive 走异步 compute/render callback 创建，API/CLI、19 例
畸形截帧、T35+T302 各 10 次生命周期打开也通过。
最终库下两份 capture 均在源 archive 文件临时移走后 CLI ×2 通过，
随后文件已恢复。

T305/T306 已接通空 archive 的创建及普通 compute/render `add*PipelineFunctions`
变更与同步/异步 pipeline，详见 [BATCH305–306](BATCH305-306.md)。T307 增加
有界的 `addFunctionWithDescriptor:library:`，见 [BATCH307](BATCH307.md)。
T308 增加单节点 `addLibraryWithDescriptor`，见 [BATCH308](BATCH308.md)。
T309 增加 tile 函数添加，但 tile pipeline 的 archive 依赖仍未序列化，见
[BATCH309](BATCH309.md)。
T310 增加 mesh 函数添加，但 mesh pipeline 的 archive 依赖亦未序列化，见
[BATCH310](BATCH310.md)。
随后 T311/T312 已接通同步 tile/mesh pipeline 的 archive 依赖与 miss
策略，见 [BATCH311–312](BATCH311-312.md)；T309/T310 的限制是历史检查点。

边界：文件导入与空 archive 的普通 compute/render 函数添加已支持；tile/mesh、
`addLibrary` 仅支持单节点 stitched graph，`addFunction` 仅支持普通
visible 函数；tile 函数添加限定无 linked functions，tile pipeline archive
依赖的异步变体、object-stage mesh archive 及 intersection descriptor 仍拒绝。
`serializeToURL`
可导出当前 native archive，但之后若把导出文件作为新 archive 导入，走新的
快照链。非文件 URL、过大/过小或不可读 payload 在捕获阶段拒绝。

GUI 待验：资源列表可看到 Binary Archive；compute/render pipeline 的 API
Inspector 均引用同一 archive；事件 seek 后 32 个输出值和中心像素正确。
当前不执行 UI 验证。06:26 旧 IOGPU panic 尚未归因；本批短测无新 panic。
