# Metal 后端持续开发

适用于本仓库 Metal 捕获、恢复、重放与测试。采用 `docs/metal-replay/EXECUTION_REVIEW_2026-10-08.md` 与配套 MAIN_THREAD_PROMPT；本规则明确替代冲突的历史场景许可、CPU producer/表达式准入和测试 oracle，不重做已完成事项。历史记录只代表对应候选证据。

- 先读 STATUS、HANDOFF、PLAN、RAYTRACING_ENABLEMENT 与当前大批次入口，保留未提交代码；heap backing/动态绑定大批次及对应compute RT的M1–M4已于f1ba1862完成；见最新STATUS/最终证据。新任务按通用恢复契约推进，不回退重做已完成整帧/机制，不把未验Metal3/render-stage当完成。
- 按通用 API 契约区分对象/物理内存/生命周期、地址/typed 发布、执行输入状态、提交依赖和可选访问分析。UE/Lumen 只作验收负载，禁止按功能/pass/shader名、PSO hash、固定 EID/slot或格式/尺寸/usage组合授予支持；coverage只作协议兼容。
- 每个机制批次读本仓库 DX12/Vulkan 捕获→初态→地址→提交→展示的具体实现，维护一张对照卡。两者不支持的调试能力保持不支持；Metal 独有的必要语义单独说明。
- 普通 GPU 数据恢复原初态、API 创建 payload和原命令/同步，由 Native shader/API执行。CPU不知道值或无法逐字节证明普通 producer，不等于输入丢失；可选展示允许 partial/unknown。保守候选不等于实际访问，也不单独产生整 allocation CPU初始化证明义务。
- 非零 GPU 地址/句柄必须真实重定位并有布局、身份、offset与发布时序；缺必需输入、失效对象/初态、无效地址或真实提交依赖仍修复或明确失败。不能用默认零、未来 producer、消费后快照掩盖丢失的已定义输入；texture/AS opaque footprint不证明普通 buffer字节。API未规定普通内容与损坏 capture分开处理，不建立未定义字节精确 oracle。
- 负例按实际违反的契约分类；移除 upload/dispatch 不天然等于 malformed。合法变化、输出变化和展示未知不是永久拒绝依据，错误旧 oracle标作废但保留历史失败。
- 一个正在闭环的大能力批次，至多两个后续依赖；T0相关构建/布局，T1关键正例+独立合法组合+真实契约负例，T2稳定批次/UE集成，T3固定候选最终矩阵。每次说明新结论，不逐小改全量重跑，不将旧哈希通过算作新候选。两次相关修改仍停同一语义阻塞，先证伪根因，不扩邻近案例/CPU表达式。
- 保留对应光追开启门槛：官方 sample、真实 UE RT整帧输出/事件往返/EID0/绑定、相关回归和设备条件均通过后才开启并复验；compute RT不自动证明render RT。RT保持黑盒，不承诺AS内部查看、RT单步或RT Pixel History。
- GPU和构建串行共享锁、有限帧/预算/监督，不与用户 GPU工作冲突。中间产物及唯一证据在 `/Volumes/CauseUseMac/RenderDocMetalArchives`，清理可重建且未使用冗余；不覆盖用户工程/改动、不reset、不自动提交推送。
- STATUS短当前摘要，HANDOFF必需事实/下一动作，PLAN里程碑；详细实验仅一份当前批次记录及外盘manifest，PHASE/BATCH/矩阵索引链接过去。持续任务提示保持稳定原则，临时假设不升级为跨会话永久拒绝。
- 验证分别记录 GPU 执行、输出比较、整体验收；输出失败/待定时整体不得 PASS。无序追加只比较语义规定的记录，不把原始排列或全部中间字节当通用 oracle；集合相同、单个固定输入 Native kernel 均不证明完整 UE 图像。容差必须有对应完整 Native 行为及 API 语义依据，不因差异小而添加。
- 输出诊断每轮写明假设、区分原因的实验和分支动作，优先区分资源恢复、地址、提交同步、合法非确定性、比较工具。连续两轮未缩小范围就换方法，不无限增加上游 pass 快照；明确 readback/等待对调度的影响。原因闭合后在同一候选做必要回归、独立合法变体和最终开启验收。
