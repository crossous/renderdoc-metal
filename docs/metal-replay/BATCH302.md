# BATCH302：Binary Archive 异步 pipeline 闭环

沿用 T301 已导入的同一 file URL archive，改由异步 callback 创建 compute
和 render pipeline。completion 结果保留独立引用，descriptor 捕获快照仍保存
有序 archive 依赖和 `FailOnBinaryArchiveMiss` 选项；回放前先重建 archive。
没有新增 bridge 或 chunk 类型，因此防御标记数量不变。

原生 Metal Validation 和注入截帧通过，`t302_capture.rdc` SHA-256
最终 `b93bf5da8a68…`。结构化导出显示 archive chunk 1038、异步 compute
chunk 1269、异步 render chunk 1266，两个 descriptor 均引用 ResourceId 12、
选项值 4。API 逐事件断言 32 个 compute `i+17` 值与中心像素约
`(0.2,0.7,0.3)`，CLI 回放通过；19 例畸形输入干净拒绝，T35+T302 各
10 次生命周期打开通过，resident 增长 458752 字节。只有此前 06:26
那份 IOGPU panic，未见新增。完整累计压力回归未运行，GUI 未启动。
最终库重建后，T301/T302 都在临时移开源 archive 文件时各 CLI ×2
回放通过，文件已恢复；最终两份捕获重新录制并再跑 9 份 API+CLI
跨族定向回归、各 19 例畸形输入。库与 app 内嵌库 SHA-256 均为
`b297a988af63…`。

集中 UI QA：同 T301 的资源跳转、两个 pipeline descriptor、事件 seek、
buffer/像素读回；另外确认异步 chunk 名称及 callback 产出的两个 pipeline
在事件树前创建。T301/T302 同属一个功能族，可在同次 QA 中对照。
