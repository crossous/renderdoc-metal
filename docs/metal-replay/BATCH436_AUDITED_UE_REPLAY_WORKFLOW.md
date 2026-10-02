# B436：本地 Testproj 自动预检、候选副本与真实 GPU 图像核验

入口 `util/ue/run_testproj_metal_replay_macos.py` 提供 `--check`、`--capture`、`--replay original.rdc`、`--verify replay.rdc`、`--ui replay.rdc`。默认本机 UE5.8.3 Testproj、M2低负载运行参数、已经验证导出的隔离 MetalRHI provider。已有 capture 按 SHA256 复制保存，绝不修改旧原件；UE45秒自动截帧后退出，随后串行处理。没有替换安装引擎，没有修改项目渲染配置，没有并发运行 UE 和 qrenderdoc。

原始 provider capture 不自动宣称完整 coverage：布局本身不足以证明 descriptor generation/lifetime/CPU writes。`prepare_ue_metal_replay_candidate.py` 先执行原库严格 pre-submit 检查，确认来源、生命期、资源恢复、提交顺序计划；该诊断在初始GPU上传和帧GPU提交之前强制退出。成功才导出 XML+ZIP，在设备创建后增加唯一 coverage65 声明，并生成独立 candidate。往返导出检查原始所有 chunk 的字段、属性、顺序（只忽略容器重新计算的 chunkIndex/length）及所有 ZIP binary 和 thumbnail SHA256。输入和库哈希必须不变。任何失败不发布输出，不跳过原始GPU命令，不填入猜测资源数据，不以捕获间接证据代替执行参数。

候选发布只证明元数据审计通过。随后以正常 API OpenCapture（无 diagnostic coverage 或 prefix override）读回呈现纹理，执行两次 EID0→最后事件，比较全部原生 BGRA8 bytes。CPU辅助 `ue_metal_replay_thumbnail_match.cpp` 采用本地官方 `core.cpp::ResamplePixels` 的整数取样及原 `jpge` quality90，将原生回放数据编码后与原 capture JPEG逐字节比较。当前入口限定一个BGRA8呈现目标，其他格式明确报不支持，不伪造通过。局部MRT验证仍由 `ue_metal_replay_event_probe.cpp` 单独执行，UI及全量也单独记录。

## 定向 / 真实 UE 结果，2026-10-02 19:01

Python编译检查、`--check` 本机路径/provider manifest/library一致性检查通过。首次转换审计准确拒绝新增声明的零timestamp/threadID被serializer省略；调整新增声明仅包含实际序列化属性，保持原始所有属性审计严格不变。

`--replay` 返回同一份原始 `ef026832e5f8c3591e6234b86298b8452d1c53b0c6e9bba9a45652ebf21a6415`。严格pre-submit通过，39156原始chunks、18702binary members及thumbnail逐项保持；候选39157chunks SHA256 `63c55db1ddb22bf9d8924891d9d920ac82d6bebe396fbc84afee92edf76a234e`。库 `f52cc7256899ba1fbc13b5c4f7cf3fb6c3c481b6835a0643c4e47b097d2e778e` 正常打开、两次重置完整帧及 Native image 全通过，900×640全图 SHA256 `1e7f9e791cdabde129856f23e9a13e3463c0d311324dc95e8eb25b8045e74e34`；JPEG与原缩略图逐字节相同，SHA256 `15d15618c45e04b63bec4f642205c2facad0a048ac3d1af8a5f41630756de6b9`。

产物：`build-macos-debug/local-m2-descriptor-replay/testproj-20261002-185713-037312/`。`results.json`、`audit/candidate-audit.json`、`images/normal-replay.log`、三份 `.bin` 和 `replayed-native-thumb.jpg`。

## 全量和 UI 分开记录

本流程不会默认运行全量；真实UE图像与B435局部MRT通过后，当前f52已冻结并启动一次验收全量。日志 `late-signal-frozen-combined-regression.log/json`，最终结果未返回。B435先前正常UI打开/切换MRT/末层/帧末画面通过；B436自动候选再次UI打开及新自动截帧待机器解锁。此文不将终端验证计入人工UI通过。

验收启动记录：首次使用原始入口意外触发Qt重新构建，发现后终止，冻结hash改变，结果计为setup-aborted（不计测试通过）。`late-signal-regression-setup-aborted.log/json`保留。恢复主库f52和renderdoccmd，使用既有verify-only入口重新启动，确认不构建且起始hash一致；最终全量结果另记。

2026-10-02 19:16 验收全量最终通过：冻结f52库hash起止一致，308正常capture、7786畸形反例、3080重复开启/回收均通过，resident growth11,190,272B。日志/JSON `late-signal-frozen-combined-regression.log/json`。这是全量独立结论；随后立即回同一原始UE审计副本63c55db1检查更多局部事件，不以回归数量代替真实UE回放证据。
