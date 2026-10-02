# 待人工 QA 清单

2026-10-02 [B426](BATCH426_DRAWABLE_CLEAR_BEFORE_SHADER_READ.md) 固定d6c890d4全量308/7786/3080通过、growth0B/hash一致；[B427](BATCH427_PER_GENERATION_SLOT_CONSUMERS.md) 6/24/210通过；[B428](BATCH428_ACQUIRED_DRAWABLE_INITIAL_PIXELS.md) 当前01db4c65、2/8/16通过，非零seedRGB10A2 acquired snapshot/NativeLoad/初始恢复验证。旧UE eb9386b7 缺currentLoad drawable初始像素不可伪造，已保留，正在自动重截；UI已可访问，整帧GPU/UI未验收，持续目标active，无提交推送。


2026-10-02 [B422](BATCH422_LAYERED_FRAME_VOLUME_TARGETS.md) / [B423](BATCH423_LOGICAL_GPU_TABLE_RETIREMENT.md) / [B424](BATCH424_SOURCED_LINEAR_TEXTURE_UPLOADS.md)：3D layered MRT 2/8/20、logical GPU retirement 6/24/180（含later resolve Native bytes）、Shared staging→R8/BGRA8_sRGB 4/16/38 通过；实际UE已继续到帧后段，最新 cdf6b4fc pre-submit 进行中，整帧GPU/UI未验收。最近全量仍6e963f9a 308/7786/3080，最新库全量待跑。持续目标active，无提交推送。


2026-10-01 [B363](BATCH363_DESCRIPTOR_BACKING_REUSE.md)：实际UE的12处typed backing overlap完成CPU归属/slot退役审计；新增明确ResourceId对校验和重叠CPU快照后的活表重定位。连续截帧的旧退休表initial依赖遗漏已修复。精确246a9d5c…8捕获/32seek/176反例，Private兼容220反例、跨kind MRT兼容331反例通过；旧21类674反例通过；全量308/7786/3080通过，驻留增长13205504B。完整UE尚未GPU replay，人工UI待解锁，持续目标active。


2026-10-01 [B361](BATCH361_PRIVATE_HEAP_DESCRIPTOR_SOURCES.md) / [B362](BATCH362_ENCODING_TIME_ALIASES_AND_PARTIAL_SNAPSHOTS.md)：Private来源8捕获/32seek/216反例，未提交CB期间Private alias220反例，Private buffer→MRT texture跨kind331反例通过。Shared partial tail遗漏future alias提交快照已修复；精确57ba6f4a…8捕获/32seek/204反例及8份非冗余snapshot捕获/32seek（GPU首读184/248、后续225/161）通过，旧21类/674反例通过。57ba6f4a…全量308/7786/3080已通过、growth11943936B。实际UE 0649fac8…12处descriptor backing alias仍需适配；完整UE尚未GPU replay，UI待解锁，持续目标active。


2026-10-01 [B359](BATCH359_FRAME_TEXTURE_DESCRIPTOR_RELOCATION.md) / [B360](BATCH360_TRACKED_ASYNC_HEAP_ALIASES.md)：frame Private texture→MRT→下一pass来源重定位通过八捕获/32seek/319反例，a9c8e85b…全量308/7786/3080通过，增长11501568B。tracked Shared heap异步alias与useHeaps间接CPU快照遗漏已修复，863f5998…八捕获/32seek/200反例及兼容MRT/旧alias/18帧通过；该新库全量308/7786/3080通过，增长13942784B。UE 0649fac8…重截已保留，369条CPU快照、1669初始来源全部匹配，帧内source/producer/inline错误0；完整UE GPU replay仍未通过，继续Private/cross-kind alias与实际绘制范围，UI待解锁，持续目标active。


2026-10-01 [B357](BATCH357_PARALLEL_FIVE_MRT.md) / [B358](BATCH358_FRAME_PLACEMENT_TEXTURE_BIRTHS.md)：parallel/MRT5 八捕获/32seek/223反例通过；修复 UE 帧内 texture/view 被提前写入 initial section 和 heap 父依赖遗漏，四捕获/24seek、18旧帧/13反例通过。新 UE ef14a1ed… 65 texture/14view 按帧内顺序记录，1676初始来源/112producer/2737inlineVA无帧内错误，125CB/peak32。当前库 ecc44d57…旧21类回归中；bfe86da…全量308/7786/3080已通过。完整UE尚未replay，继续future texture来源与实际资源范围；UI待解锁，持续目标active。


2026-10-01 [B354](BATCH354_LEADING_DESCRIPTOR_RETIREMENTS_AND_CAPTURE_SIZE.md) /
[B355](BATCH355_IMPLICIT_PLACEMENT_BUFFER_ALIASES.md)：稳定320×240重新捕获
f744c413…，1678帧初来源及内容、112GPU producer、2737inlineVA全匹配，
126CB/peak33、170pass/393draw，完整UE尚未提交GPU replay。物理placement
alias保留两份对象和旧descriptor：8捕获/32seek/184反例通过（f69220dd…）；
v16旧21类/674反例通过。B353精确5b08e331…全量308/7786/3080已经通过，
后续精确新库全量待跑；UI待解锁。持续推进signal-only与异步alias。


2026-10-01 [B353](BATCH353_INTERLEAVED_SUBMISSIONS_AND_PARTIAL_TAILS.md)：真实 UE
75 个 command buffer、峰值 11 的交错创建/提交已复现；修复 partial seek
尾部队列预约死锁和 future submission 过早恢复快照。库5b08e331…，retained/
unretained 四捕获、16轮 seek、GPU 累加308/像素186-122、114反例、旧九帧通过。
B352 的0f684365…已完成308正例/7786反例/3080生命周期完整回归，增长13549568B；
B353 全量复跑中。新的320x240真实FirstPerson截帧8ebcbfa6…已保存并自动退出，
尺寸来自插件实际日志；原大帧保留，正在做CPU审计。完整UE replay尚未通过，
UI仍待解锁；持续目标active。

2026-10-01 [B351](BATCH351_UE_DRAW_CONSTANTS_AND_INDEXED_SOURCES.md) /
[B352](BATCH352_COMMAND_BUFFER_QUEUE_PROXY_LIFETIME.md)：UE IR绘制常量、UInt16/UInt32
静态索引来源和冗余提交快照验证接通；发现并修复临时queue的包装父对象悬空。
21类/674反例、真实UE更新shader小图形/27反例、双队列完成回调、旧八帧通过；
库0f684365…，queue连续10×2捕获通过。此前93eb26b1…的308图像/CLI与分段反例
至T312、3080生命周期通过（增长12369920B），该新库完整累计复跑仍待执行。
新UE隔离诊断模块feeed228…已编译，开始低负载进程设置自动截帧并退出；原帧
已保留。完整UE画面/MRT/pass尚未完成，UI仍待解锁；持续目标active。


2026-10-01 [B350](BATCH350_REGRESSION_RESOURCE_AND_FAILURE_LIFECYCLES.md)：ICB 动作树、
无 coverage 的普通帧内 buffer 回跳、共享 DummyDriver 与提交快照归属已修复。
308 份图像/CLI 阶段、T49 87反例与相关定向通过，全量剩余反例/lifecycle 执行中。
库93eb26b1…；最新 UI 待解锁，完整 UE 未完成，持续推进 UE 绘制参数与索引路径。


2026-10-01 [B349](BATCH349_EXPLICIT_FRAME_HEAP_ALIASES.md)：v13 显式 heap 退役与
同地址不同资源重定位、连续两捕获/四次 seek 图像通过；十九类/560 反例通过。
全量首轮 T20 ICB action tree 崩溃已定位并修正，重跑中；最新 UI 待解锁，
完整 UE 未完成。持续目标 active，继续回归与真实 UE 绘制参数/索引路径。


2026-10-01 [B348](BATCH348_ASYNC_SUBMISSIONS_AND_CPU_SNAPSHOTS.md)：v12 同队列
无捕获端 CPU wait 的两提交与 Shared 快照恢复同步通过，GPU 累加 308/正确像素。
十八类 /507 API+CLI 反例、真实 UE 更新 shader 小图形链路、旧六帧定向通过。
库 b2edab43…；全量未跑，最新 UI 未验，完整 UE 未完成。持续推进 frame heap alias。


2026-10-01 [B347](BATCH347_FRAME_HEAP_DESCRIPTOR_PAYLOADS.md)：v11 帧内 heap
payload/table 创建与来源重定位接通，四轮重建仍 GPU 字节/像素正确；十七类/
453 API+CLI 反例通过。库52b42a31…；全量未跑，新增 UI 未验，完整 UE 未完成。
持续目标 active，继续无 CPU wait 的有序提交与 Shared 快照同步。


2026-10-01 [B346](BATCH346_COMPLETION_WAITS_AND_SOURCED_SUBMISSIONS.md)：完成等待
不再为空；v10 同队列两提交及 waited CPU 描述符更新、未保留资源方式通过。
十六类 /383 API+CLI 反例、旧六帧定向、两队列捕获回调/恢复通过。库64714dcb…。
全量未跑，新增 UI 未验，完整 UE 未完成；持续目标 active，推进 heap/ring/异步提交。


2026-10-01 [B345](BATCH345_SOURCED_MRT_AND_PASS_SCOPES.md)：v9 双 MRT 与跨 pass
采样、action.outputs / colorTargets / BeginPass-EndPass / 四轮 seek 全部正确；
十四类描述符与 284 组 API+CLI 反例通过，库 c5570049…（该批验证版本）。
全量未跑，新增 UI 未验，完整 UE 未完成；持续目标 active，推进完成等待与多提交。


2026-10-01 [B343](BATCH343_SOURCED_GRAPHICS_DESCRIPTOR_CONSUMERS.md) /
[B344](BATCH344_ACTUAL_UE_UPDATE_TO_GRAPHICS.md)：v9 顶点/片元 inline 按来源重编码，
真实 UE 更新 shader 接入小图形消费者；两捕获/四轮 seek/正确 GPU 字节与像素通过。
十三类描述符 / 241 组 API+CLI 反例通过，真实 UE 链路另有 27 组通过。
库 490bf140…；全量未跑、新增 UI 未验、完整 UE 尚未完成。持续目标 active，
继续 MRT / 多 pass / 多提交 / alias。


2026-10-01 [B342](BATCH342_ACTUAL_UE_DESCRIPTOR_UPDATE_SHADER.md)：从真实UE
截帧提取原编译UpdateDescriptorHandle shader，单条/四条更新原生GPU、各两捕获、
四轮seek与API字节/CLI通过；payload为opaque copy，尚无该真实UE条目的consumer。
库仍37063340…，全量未跑/新增UI未验/完整UE未完成，持续目标active；继续
vertex/fragment来源重编码和draw验证。


2026-10-01 [B341](BATCH341_FRAME_SOURCED_BUFFER_TABLES.md)：普通buffer创建与
临时表声明正确入帧；v8新建source/table回跳重建，强迫VA每轮改变仍采样正确。
十二类tiny/214 API+CLI负例及旧六帧定向通过。真实UE a5907a66…的1,479帧首槽/
106 producer来源全部匹配，仍CPU证据。库37063340…；全量未跑，新增UI未验，
完整UE未完成，持续目标active。继续真实UE更新shader的单pass GPU验证。


2026-10-01 [B339](BATCH339_UE_NATIVE_DESCRIPTOR_FIELDS.md) / [B340](BATCH340_GPU_TEXTURE_DESCRIPTOR_UPDATES.md)：
UE原生buffer/texture/sampler字段及Texture4/5 buffer view接通；GPU纹理描述符
计算更新后实际采样122→186/186→122、双捕获/seek/像素通过。十一类tiny /171
API+CLI负例、3D/BC上传GPU407及12负例、旧六帧定向通过。新UE b341dea4…
1,477帧首槽/31 producer来源匹配；仍CPU证据。全量未跑，新增UI未验，完整UE
图像/MRT/pass replay未完成；持续目标active，继续frame births/alias/render。
最终库a0f35fed…，未提交/推送。


2026-10-01 [BATCH338](BATCH338_SOURCED_COMPUTE_PRODUCERS.md)：计算更新tiny实际
GPU与seek通过，九类/123 API+CLI负例及旧六帧通过。真实UE新帧5039ca0f…的
1,477帧首槽和118 GPU producer/expected/source关联全部匹配；仍仅CPU证据。
下一项对齐UE真实Buffer/Texture/Sampler类型，再扩展frame/alias/render。
全量未跑，新增UI未验，完整UE未完成，持续目标active。库78130e17…。


2026-10-01 [BATCH337](BATCH337_SOURCED_GPU_DESCRIPTOR_COPY.md)：sourced slot GPU
复制实际41→80、GPU初始状态80→41及回跳通过；八类tiny/99 API+CLI负例通过，
库4bd948d2…。全量未跑，新增UI未验，完整UE仍未完成；继续计算scatter路径。


2026-10-01 BATCH334–336：七类描述符 GPU/seek、78 API+CLI 负例、应用队列
帧首/帧末 callback、自动 capture、失败恢复与旧六帧定向终端通过；真实 UE
新帧1,476活槽初始字节全匹配。全量未跑，本批人工 UI 未验（锁屏），完整
UE 正确图像/MRT/pass scope 全部仍待验，旧项保留。持续目标 active。见
[BATCH336](BATCH336_CAPTURE_QUEUE_COMPLETION.md)。


2026-10-01 BATCH332–333：inline及slot诊断终端已验，新增Viewer未验。
完整UE replay/正确画面/MRT/pass scope与旧T全部待验；正常Metal UE启动不等同
回放通过。全量未跑，持续目标active，自动截帧/CPU审计推进中。见[BATCH333](BATCH333_UE_PARTIAL_PROVIDER.md)。

2026-10-01 BATCH331：最终5fab70bf…人工Private buffer texture view小帧通过，
EID15/36/15输出89/167/89与DEADBEEF不变；正常退出。先前v3 Shared小帧41/80/41
也通过。最终plain Private另次UI遇锁屏，未记通过；完整UE和全部旧T待验保留。
定向五例/48负例/旧六帧通过，全量未跑。见[BATCH331](BATCH331_FRAME_DESCRIPTOR_RESOURCES.md)。

2026-09-30 BATCH330：用户已解锁，最终库/app `78e3cecc338b…` 两类新小帧
人工Viewer打开/compute绑定/Buffer数值/事件回跳通过。table122/161/122，混合
41/121/160/41、DEADBEEF不变；旧库还核对呈现像素。仅本批两类通过，完整UE
及全部旧T系列待验保持；全量未跑。见[BATCH330](BATCH330_DUPLICATE_IDENTITIES_AND_VIEWER.md)。

2026-09-30 BATCH328–329：最终库/app `bd255367cb60…`，两类typed小帧GPU/seek、
13负例与旧六帧API+CLI通过。UE保存时thumbnail生命周期应用崩溃已修，自动
重截end=1，完整UE仍因帧内身份和不完整coverage在GPU前拒绝。人工小帧Viewer
尝试被macOS锁屏阻止，已请求手动解锁；成功UI QA增量0，全部旧待验保持。
不要求用户再重截，见[BATCH329](BATCH329_UE_DESCRIPTOR_LAYOUTS_AND_DRAWABLE_LIFETIME.md)。

此文件是从 `BATCH29-30.md` 起跨批次保留的 L4 待验登记。每次接手、批次交接和
回复结果前都要读取本表；不要只根据最近一条聊天消息判断是否已验收。

## 当前待验

2026-09-30 BATCH327：用户诊断帧已收，捕获侧间接资源遗漏由最小原生复现
证实并修复；助手自动重截，sampler映射已有覆盖。完整UE GPU replay仍在
重定位未实现处预期拒绝，未做qrenderdoc UI。最终库/app `6c0e5d6f8cfe…`，
六帧终端回归与原生跨进程typed packet验证通过；全部旧待验继续保留。
无需用户再截，下一步是实际replay/GPU descriptor更新实现，见
[BATCH327](BATCH327_GPU_IDENTITY_RESOURCE_CLOSURE.md)。成功UI QA增量0。

2026-09-30 BATCH326：本地用户NewMap新帧只完成CPU审计，仍缺原生GPU身份
映射；新诊断库/app `c7cb40b786ff…` 在GPU前拒绝未支持的原地址/ID族。
原生/注入小夹具和旧六帧终端通过，需用户重截以获取映射。未运行完整UE
GPU replay或qrenderdoc UI，全部旧待验保持，见
[BATCH326](BATCH326_GPU_IDENTITY_DIAGNOSTICS.md)。

2026-09-30 本地 M2 Pro：新库与 viewer 同 hash，Testproj 插件已安装/编译，
4 KiB terminal Empty 与 T35/T62 定向终端通过；Entry 会话已启动等待用户重截。
新 UE 帧尚未取得，正确画面、scope/MRT、资源与 replay 待验；全量与人工
qrenderdoc UI 均未运行。全部旧待验项保持，见
[本地接手记录](LOCAL_M2_SETUP_2026-09-30.md)。

2026-09-29 BATCH320：用户已人工打开 UE `frame1770`，发现无 scope 和
黑色 RT，内容验收失败。新库终端 API 可见旧帧的 21 个 marker；项目级
受控 viewport 按钮已编译，待用户点击一次检查场景 pass。新版 UI 尚未
验收，累计**成功** UI QA 增量 0。见 [BATCH320](BATCH320.md)。

2026-09-29 BATCH319 增量：UE v0x10 `frame1770` 在最终库下有界 API/CLI
单次终端打开通过，用户尚需人工核对 `Lvl_FirstPerson` PIE 的画面、
事件树和资源；T319 帧内 Shared placement buffer 也待 UI 抽看。
库/app hash 与命令见 [批次证据](BATCH319.md)。累计 UI QA 增量 **0**。

2026-09-29 BATCH315–318 增量：UE v0xF `frame5394` 已截取但缺 BC 帧首
初始内容，最终库安全拒绝；T317/T318 的 BC placement 纹理原生/注入、
定向 API/CLI、GPU 像素/原始块及事件回跳通过。待人工 L4 看三个压缩纹理
资源、帧首初始内容和正反向 seek；新 UE v0x10 帧尚未取得。
累计 UI QA 增量 **0**。见 [批次证据](BATCH315-318.md)。

2026-09-29 M7 增量：新增的单次回放脚本已用 T313 终端验证，尚无新 UE
截帧和新 UI 检查；累计 UI QA 增量 **0**。见
[M7 记录](UE58_M7_RECAPTURE_GATE_2026-09-29.md)。

按用户要求，本轮没有启动 qrenderdoc 或使用 Computer Use。T34–T69、T71–T132、T135–T153、T156–T157、T159–T312 和 T10 marker
增量已完成计划内终端自动验证，但以下 GUI L4 尚未执行；统一验收与最新库版本见
`QA_CONSOLIDATED.md`。旧 batch QA 文档保留细节，总单为最新入口。用户计划重置后集中QA。

2026-09-29 增量：T313/T314 placement buffer 显式 alias/释放复用已定向终端通过，
待人工 UI 检查两资源身份、创建顺序及前后事件回跳；累计 UI QA 增量 **0**。
旧 v0xE T133 仍为预期拒绝负例，不打开。证据见 [BATCH313–314](BATCH313-314.md)。

| 批次 / 功能 | 状态 | 最终构建与 capture | 待验重点 |
| --- | --- | --- | --- |
| BATCH311–312 / tile/mesh pipeline archive 依赖 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t311_capture.rdc` SHA前缀`96acccaa0e49`、`t312_capture.rdc` `2beb183fccd6`；库/app`2f88058307f5…` | 对照T309/T310，tile/mesh pipeline 的binaryArchives各有同一archive且options4，资源图archive→pipeline；GPU tile三阶段/mesh间接draw及正反向seek按T74/T89。 |
| BATCH310 / archive 添加 mesh 函数 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t310_capture.rdc` SHA前缀`ee79cc878d8b`；库/app`fecbaa45dd8a…` | chunk1383先于mesh pipeline创建，mesh/fragment→archive资源关系；执行中的write_grid compute pipeline用archive options4；GPU间接mesh draw、像素和seek按T89。mesh pipeline自身archive依赖不在本批承诺内。 |
| BATCH309 / archive 添加 tile 函数 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t309_capture.rdc` SHA前缀`32025e6fed64`；库/app`13f127004ba0…` | chunk1382先于tile pipeline创建，tile kernel→archive资源关系；普通draw pipeline使用archive options4；三次tile dispatch输出和seek按T74。tile pipeline自身的archive依赖不在本批承诺内。 |
| BATCH308 / archive 添加单节点 stitched library | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t308_capture.rdc` SHA前缀`15e67b9c276c`；库/app`7c43ac13093f…` | chunk1381先于pipeline创建，源visible函数→archive资源关系；与T305–307对照32个uint、中心像素和seek。本例不声称stitched函数经GPU函数表执行。 |
| BATCH307 / archive 添加 visible 函数 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t307_capture.rdc` SHA前缀`348f4169bb2b`；库/app`0a62ea89993e…` | chunk1380先于pipeline创建，来源library→archive资源关系；与T305/306对照32个uint、中心像素和事件seek。此项不声称visible函数已在GPU函数表调用。 |
| BATCH305–306 / 空 Binary Archive 的函数变更 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t305_capture.rdc` SHA前缀`12b0c5a7ce8e`、`t306_capture.rdc` `cf5b600b3e24`；库/app`1e0165e982e7…` | 空archive创建、chunk1378/1379按序添加compute/render函数、三个函数→archive→两个pipeline资源链；options4，32个uint为17–48，中心像素及事件seek；T306异步对照。 |
| BATCH304 / 异步单节点 Stitched Library | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t304_capture.rdc` SHA前缀`3602ebde29d3`；库/app`a4547b2ebb75…` | 对照T303，异步chunk1377、同一函数→library→pipeline资源链、32个float 2…64与中心像素、事件seek。 |
| BATCH303 / 单节点 Stitched Library | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t303_capture.rdc` SHA前缀`a4df65b1fb4b`；库/app`a4547b2ebb75…` | chunk1020中函数13→stitched library14→visible函数15→compute pipeline；graph名`stitched_scale`，32个float为2…64、中心像素约`(0.2,0.7,0.3)`；事件seek。 |
| BATCH302 / Binary Archive 异步 pipeline | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t302_capture.rdc` SHA前缀`b93bf5da8a68`；库/app`b297a988af63…` | 与T301同次对照：异步compute/render chunk 1269/1266，同一archive资源依赖、options4，32个`i+17`值和中心像素、事件seek。 |
| BATCH301 / Binary Archive 导入与 pipeline 依赖 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t301_capture.rdc` SHA前缀`2780f4d81854`；库/app`b297a988af63…` | 资源列表中的Binary Archive；compute/render descriptor均引用同一资源，archive-miss选项4；dispatch后32个`i+17`值、中心像素约`(0.2,0.7,0.3)`；事件seek。普通compute/render变更另见T305–306。 |
| BATCH299–300 / 多实例复用两个 BLAS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t299/300_capture.rdc` SHA前缀`6f84e5d6111d` / `60ae66ccaa14`；库/app`ac97012d33b5…` | `buildRepeatedDistinctInstances`两个不同子资源、count3/5及重复索引0/1；三/五次ray逐个命中，事件seek与资源身份。 |
| BATCH298 / 同一 BLAS 的八实例 TLAS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t298_capture.rdc` SHA前缀 `00ed80a5d7cc`；库/app `e67638f19213…` | `buildInstances.count=8`、单一BLAS子资源、八次 ray 从 `7/9/11/13/15/17/19/21` 依次变为1；事件 seek、资源身份及反向恢复。65536只是安全上限，非GPU正例。 |
| BATCH294–297 / 三四个不同 BLAS 的 TLAS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t294…297_capture.rdc` SHA 前缀 `cd5fe6dca0a5` / `9b90bc5d9f36` / `861b6c858532` / `d5518fdcc044`；库/app `bdc80ef81a6…` | `buildMultipleDistinctInstances.children` 的 3/4 个有序不同资源、实例描述符 AS 索引0…N−1、三/四次 ray 均命中；T296 descriptor 分配、T297 TLAS 压缩复制；事件 seek 与资源身份。 |
| BATCH290–293 / 三四实例 TLAS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t290…293_capture.rdc` SHA 前缀 `abff92866814` / `3a2a5078c0e9` / `dbf056a75a44` / `11c90bf3dad0`；库/app `986a0d98b8f8…` | `buildInstances` count3/4、三个或四个 GPU ray 命中；T292 descriptor 分配、T293 TLAS 压缩复制；事件 seek 与资源身份。 |
| BATCH284–289 / 双三角形 indexed refit | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t284…289_capture.rdc` SHA 前缀见 [批次记录](BATCH284-289.md)；库/app `2c2bc09e0e18…` | triangleCount2、UInt16/UInt32、T286 独立目标+压缩、T287 换索引、T288 换顶点、T289 同时换两类 buffer；ray 1→0、事件 seek 和资源身份。 |
| BATCH281–283 / indexed refit UInt32/Float4 偏移 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t281…283_capture.rdc` SHA 前缀 `a7c620268d97` / `900a0ca02260` / `896f8016e693`；库/app `2c2bc09e0e18…` | T281 UInt32 indexOffset8、T282 Float4 vertexOffset16、T283 两者加 descriptor 分配；build/refit 参数一致，ray 1→0 与事件 seek。 |
| BATCH272–280 / indexed refit 参数扩展 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t272…280_capture.rdc` SHA 前缀见 [批次记录](BATCH272-280.md)；库/app `2c2bc09e0e18…` | 核对 Float4/stride16、noDuplicate、scratchOffset256、indexOffset2、vertexOffset36、descriptor+vertexOffset、tableOffset1、opaque 切换和 T280 七参数组合；build/refit 参数保持一致，ray 1→0 与事件 seek。 |
| BATCH266–271 / indexed triangle AS refit | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t266…271_capture.rdc` SHA 前缀 `7aecbaaa6b8c` / `25f25b5c78fc` / `927906e9703c` / `3d93bf83cc3d` / `6d163a9977b2` / `9926b78a401c`；库/app `cf971d35cd45…` | `buildRefittableIndexedTriangle`→`refitIndexedTriangle`，核对 UInt16/UInt32、T268 换 index buffer、T269 独立目标、T270 compact、T271 descriptor 分配；ray 1→0 与事件 seek。 |
| BATCH261–265 / refit 切换几何 buffer | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t261…265_capture.rdc` SHA 前缀 `403be1965785` / `2994416908dc` / `eeeb2695c029` / `d1bd3761dfa6` / `549b1eb65aae`；库/app `c94ea0b15648…` | build 与 refit 的 box/vertex 资源 ID 应不同；T261 box、T262 默认 triangle、T263 Float4、T264 noDuplicate、T265 独立目标。核对字段、资源跳转、事件 seek 与 ray 变化。 |
| BATCH259–260 / refitted box AS 复制与再次 refit | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t259/260_capture.rdc` SHA 前缀 `aaaf47ca1353` / `93026056f190`；库/app `cbfb8ec0daff…` | T259 refit→copy→ray；T260 copied AS 再 refit→ray。检查两次 refit、copy action、资源身份、事件 seek 和 `1/1/0→0/1/0→0/1/0→1/1/0`。 |
| BATCH256–258 / bounding-box AS refit | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t256…258_capture.rdc` SHA 前缀 `1c0ce0b6ad6f` / `c1061c97ba04` / `2b7628bba8b`；库/app `cbfb8ec0daff…` | build→refit，T256 原位，T257 box/scratch 偏移、表槽1及 noDuplicate，T258 独立目标；核对资源身份、字段、事件 seek 和 ray `1/1/0→0/1/0`。 |
| BATCH251–255 / 格式化 triangle refit | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t251…255_capture.rdc` SHA 前缀 `14c1cd5b07a3` / `806c279a77dd` / `952a939e3f1b` / `17017c2fc35f` / `a5b5b33feb6d`；库/app `2668361301ba…` | `buildRefittableFormattedTriangle`→`refitFormattedTriangle`：T251 Float3，T252 Float4，T253 descriptor+独立目标，T254 noDuplicate+scratchOffset256，T255 refit后压缩复制；ray 1→0，核对格式字段、源/目标身份和 seek。 |
| BATCH248–250 / indexed triangle 顶点格式与步长 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t248…250_capture.rdc` SHA 前缀 `11ff5c001b2d` / `af195241374c` / `c956c89a3989`；库/app `e9d06ac87d24…` | `buildIndexedFormattedTriangle`：T248 Float4/UInt16，T249 Float4/UInt32 加 vertex/index/scratch 偏移16/8/256及descriptor分配，T250 Float3/UInt16 加禁止重复交点调用；核对字段、资源身份、ray `1,0,1` 与 seek。 |
| BATCH244–247 / triangle 顶点格式与步长 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t244…247_capture.rdc` SHA 前缀 `74415587bcf2` / `7e775e034509` / `09292d535f79` / `ce712cfa0d7f`；库/app `21a036a49317…` | `buildFormattedTriangle`：T244 Float3/stride16，T245 Float4/stride16，T246 vertexOffset16/scratchOffset256，T247 Float4 加禁止重复交点调用；核对字段、AS→TLAS 资源身份、ray `1,0,1`、事件 seek。 |
| BATCH240–243 / refit 禁止重复交点调用 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t240…243_capture.rdc` SHA 前缀 `57ca8102c0f3` / `1875abd63868` / `c3aab50c2212` / `53698f1b9e5a`；库/app `61fe88a3591a…` | `buildRefittableTriangleNoDuplicate`→`refitTriangleNoDuplicate`；T240 原位、T241 descriptor+独立目标、T242 scratchOffset256、T243 refit 后压缩复制。GPU ray build=1/refit=0，检查资源身份、事件顺序和 seek。 |
| BATCH237–239 / triangle 禁止重复交点调用 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t237…239_capture.rdc` SHA 前缀 `39c04471ce92` / `586f10a6e265` / `6aebf01c4a99`；库/app `103f584d33e5…` | `buildTriangleNoDuplicate`：非 indexed、UInt16 indexed、UInt32 indexed+48/8/256 三偏移和 descriptor 分配；表槽1、compute ray `1,0,1`、render ray `1`；字段、事件、资源身份和 seek。此场景不证明去重上限。 |
| BATCH234–236 / box 禁止重复交点调用 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t234…236_capture.rdc` SHA 前缀 `016b90163910` / `e9abb40b380c` / `9901236e8a09`；库/app `0a4547ec4347…` | `buildBoundingBoxNoDuplicate`：T234 基本、T235 boxOffset48/scratchOffset256/tableOffset1、T236 opaque；GPU callback counter 均为2，ray 分别 `1,1,0` / `1,1,0` / `0,0,0`。此场景不能区分允许重复的默认值与禁止重复的行为上限；重点看字段保真与事件/资源身份。 |
| BATCH231–233 / triangle 表偏移 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t231…233_capture.rdc` SHA 前缀 `66c70d9ffdd5` / `ae2948bfc7ab` / `f99aaf786a1d`；库/app `08d484d3b59f…` | `buildTriangleTableOffset.tableOffset=1`，交点函数位于表槽1；T233 另有 UInt32 和 48/8/256 三偏移及 descriptor 分配；dispatch 后 ray `1,0,1`、render ray `1` |
| BATCH227–230 / indexed triangle 组合偏移 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t227…230_capture.rdc` SHA 前缀 `1d861889320a` / `6305ce8ae637` / `306a61d47e18` / `d4882f84b346`；库/app `96ad1c3c78a3…` | `buildIndexedTriangleExtended`：T227/228 顶点48/索引8/scratch256，T228 UInt32+descriptor分配；T229 仅顶点48，T230 仅scratch256；ray `1,0,1`、render ray `1` |
| BATCH226 / UInt32 indexed 偏移 + descriptor 分配 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t226_capture.rdc` SHA 前缀 `2019936bbe7f`；库/app `99941fffb20b…` | `buildIndexedTriangleOffset.indexType=UInt32/indexOffset=8/opaque=true`；BLAS 经 descriptor 分配；ray `1,0,1`、render ray `1` |
| BATCH225 / indexed triangle 索引偏移 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t225_capture.rdc` SHA 前缀 `11b4a39d7352`；库/app `99941fffb20b…` | `buildIndexedTriangleOffset.indexOffset=8`、opaque=true；索引 buffer 前缀无效，实际三角形从 offset8 读；ray `1,0,1`、render ray `1`，事件回退 |
| BATCH223–224 / opaque box ray | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t223/224_capture.rdc` SHA 前缀 `678fd2ae45a1` / `b66c7ca05986`；库/app `1e37a4f471f9…` | box build 的 opaque=true；T224 还叠加 tableOffset1、boxOffset48/scratchOffset256；dispatch 后 uint `0,0,0`，与 T218–222 的 `1,1,0` 对照 |
| BATCH221–222 / box 几何 table offset | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t221/222_capture.rdc` SHA 前缀 `fc1ad4daaf88` / `84dee8cfbef0`；库/app `44cc7a5efb3f…` | 交点函数表槽1、box build 的 tableOffset1；T222 与box/scratch偏移组合；dispatch后uint `1,1,0` |
| BATCH218–220 / box ray 与 compute 交点函数表 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t218…220_capture.rdc` SHA 前缀 `ca4ec2cfe01a` / `3afdf3e0be8c` / `4aa223e6b279`；库/app `d26ccf06016a…` | 双盒 stride32 GPU ray `1/1/0`；T219 boxOffset48/scratchOffset256；T220 compute 表 range 绑定，事件跳转与资源身份 |
| BATCH216–217 / URL 动态库 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t216/217_capture.rdc` SHA 前缀 `371207066173` / `d93d2c7e4a8f`；库/app `8d98d33c39c0…` | URL 动态库字节嵌入、原路径不存在仍可回放；T217 与普通库双依赖，compute 值 3/6、draw 中央红色 |
| BATCH213–215 / box AS stride32 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t213…215_capture.rdc` SHA 前缀 `c448d192a774` / `0a4be5cb25a4` / `30ddba848395`；库/app `cffc4b0d117…` | stride32 单盒/双盒、双盒加 boxOffset48/scratchOffset256；GPU compacted size1280；内部第二盒射线几何尚未证明 |
| BATCH209–212 / box AS 偏移 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t209…212_capture.rdc` SHA 前缀 `6e0720aede63` / `b8a3683c2828` / `d985008c577b` / `804b35b122cf`；库/app `920be51b84d8…` | boxOffset48、scratchOffset256、组合及 descriptor 分配；GPU compacted size1280，事件/资源回退；不要求 box 几何预览 |
| BATCH204–208 / refit扩展 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t204…208_capture.rdc` SHA 前缀 `a482ea1c7ab3` / `344835a4e0dd` / `fd80bfc0a01f` / `c9c8c7e4f056` / `02e8403ac813`；库/app `56c3811f7405…` | scratchOffset256、独立refit目标、组合、独立目标压缩后ray及512-byte紧凑refit scratch；ray 1→0 |
| BATCH202–203 / refit descriptor 分配 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t202/203_capture.rdc` SHA `973fada30a67…` / `9075fbb89809…`；库/app `a7c43e2e37fa…` | 单/双三角形 refit descriptor 分配 size2048，之后refit→压缩size1792→目标ray，命中1→0 |
| BATCH200–201 / refit 后压缩ray | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t200/201_capture.rdc` SHA `6433b65790d2…` / `f88dec560f6e…`；库/app `45b447817a59…` | 单/双三角形原位refit后GPU size1792→跨CB压缩copy→目标ray，build命中1、refit后未命中0 |
| BATCH196–199 / TLAS复制和压缩后ray | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t196/197/198/199_capture.rdc` SHA `76fd780faa53…` / `68954d273dd3…` / `ca18e1a89989…` / `6fa0a4e4e523…`；库/app `a08d2768c106…` | T196普通复制TLAS后ray 0/1；T197单实例压缩TLAS后ray 0/1；T198/T199同BLAS/异BLAS双实例压缩后ray 1/0/1，压缩size1536 |
| BATCH194–195 / indexed与多三角形压缩 BLAS ray | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t194/195_capture.rdc` SHA `9fb66545a41d…` / `9b1251e08dd8…`；库/app `2e33d8225d00…` | UInt16 indexed / 双非indexed三角形分别经GPU size1280→跨CB压缩copy→TLAS build→ray，结果0/1 |
| BATCH192–193 / 复制后 BLAS ray | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t192/193_capture.rdc` SHA `efafce7ba185…` / `8cd542cb4230…`；库/app `2e33d8225d00…` | T192 build→普通copy→以目标BLAS构建TLAS并ray；T193 GPU size1280→跨CB压缩copy→目标BLAS构建TLAS并ray；两者结果0/1 |
| BATCH189–191 / AS 压缩复制扩展 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t189/190/191_capture.rdc` SHA `5d2641a26749…` / `249264392fe8…` / `8f65ddb5925d…`；库/app `f8fa07f7fd24…` | box / 双非 indexed 三角形 / indexed 三角形分别 build→GPU size读回→跨CB压缩copy；目标 size1280、写回1280；不要求目标ray绑定 |
| BATCH186–188 / TLAS descriptor 分配 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t186/187/188_capture.rdc` SHA `5e69883ab2a8…` / `dd2ac6c2e8b2…` / `4134493f0c66…`；当前库/app `b9b786586f9d…` | TLAS 经 descriptor 分配 size1792，随后单实例/同BLAS双实例/异BLAS双实例 build；核对实例关系及原有 ray 输出；heap布局查询无需GUI |
| BATCH183–185 / AS 绑定清空 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t183/184/185_capture.rdc` SHA `821c608d220b…` / `d270206e78a0…` / `4346f2a70218…`；库/app `54ed11b8cce6…` | 计算+片元/顶点/tile 分别先绑定 TLAS、执行 ray、再设零资源清空；确认事件顺序与原有画面不变 |
| BATCH182 / T182 多 indexed triangle descriptor AS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t182_capture.rdc` SHA `014076f1de47…`；库/app `1a8c4816af5a…` | `buildIndexedTriangle.triangleCount=2`、六UInt16索引、72字节顶点buffer；GPU compacted1280，须核对count与索引字段 |
| BATCH181 / T181 多三角形 descriptor AS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t181_capture.rdc` SHA `1ffbca9bdc9b…`；库/app `44a338d15489…` | `buildNonOpaqueTriangle.triangleCount=2`、72字节顶点buffer；AS size1536/compacted1280 与单三角形相同，须看count字段 |
| BATCH180 / T180 多 box descriptor AS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t180_capture.rdc` SHA `dcb164b33c4e…`；库/app `ba68ff520274…` | `buildBoundingBox.boxCount=3`、72字节box buffer；AS size1536/compacted1280 与1/2 box相同，须看count字段 |
| BATCH179 / T179 双 box descriptor AS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t179_capture.rdc` SHA `c65cfc84faf9…`；库/app `cb8f903f05ff…` | descriptor 分配 box AS，`buildBoundingBox` 的 boxCount2；size1536/compacted1280 与单 box 相同，须核对 chunk 数量字段 |
| BATCH178 / T178 box descriptor AS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t178_capture.rdc` SHA `b8c37cb4c5a7…`；库/app `9e5c56446ded…` | `newAccelerationStructureWithDescriptor` 分配 box AS，随后 `buildBoundingBox`、compacted size1280；write→build→write seek |
| BATCH176–177 / T176–T177 AS 双偏移 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t176/177_capture.rdc` SHA `0d18e6a1d3cd…` / `f1a68a4f1bea…`；库/app `1bb6f06a68b5…` | 第二BLAS build 同时 vertexOffset16、scratchOffset256；T176 size分配、T177 descriptor分配，Shared均`1→7→1`、中央绿色 |
| BATCH175 / T175 scratch buffer 偏移 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t175_capture.rdc` SHA `22fd0ff433ef…`；库/app `1bb6f06a68b5…` | 第二BLAS `buildOpaqueTriangle` scratchOffset256、Shared`1→7→1`、中央绿色；未对齐/越界负例仅终端验 |
| BATCH174 / T174 偏移顶点 descriptor AS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t174_capture.rdc` SHA `ed91b6c2ad4b…`；库/app `8a9f713e6364…` | 第二BLAS经 descriptor 分配，随后 `buildOpaqueTriangle` vertexOffset16；Shared`1→7→1`、中央绿色 |
| BATCH173 / T173 静态三角形顶点偏移 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t173_capture.rdc` SHA `a52f95b7871e…`；库/app `b5c8dff22b37…` | 第二BLAS `buildOpaqueTriangle` 的 vertexOffset16，前缀16字节为干扰数据；Shared`1→7→1`、中央绿色 |
| BATCH171–172 / T171–T172 UInt32 indexed 对照 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t171/172_capture.rdc` SHA `7dadf0fbcb2b…` / `e6052b8de72c…`；库/app `73a801b87e7b…` | T171 opaque descriptor+build命中Shared1绿；T172非opaque旧build被reject为Shared0红；两份共用UInt32索引语义 |
| BATCH170 / T170 indexed 非 opaque 反向对照 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t170_capture.rdc` SHA `d843c33ff562…`；库/app `73a801b87e7b…` | 第二BLAS旧 indexed chunk、fragment reject 生效，Shared`0→7→0`、中央红色；与T167/T169相反 |
| BATCH169 / T169 indexed opaque descriptor AS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t169_capture.rdc` SHA `ea73a885eaaa…`；库/app `73a801b87e7b…` | 第二BLAS由 indexed opaque descriptor 分配并随后 build；Shared`1→7→1`、中央绿色，分配chunk只存容量 |
| BATCH168 / T168 UInt32 indexed opaque 三角形 AS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t168_capture.rdc` SHA `fdba55218a1d…`；库/app `9a7f700ace5c…` | T167 的 UInt32 对照，第二BLAS显式 indexed opaque build、Shared`1→7→1`、中央绿色 |
| BATCH167 / T167 indexed opaque 三角形 AS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t167_capture.rdc` SHA `198c95ce8dbf…`；库/app `9a7f700ace5c…` | 第二BLAS显式 UInt16 indexed opaque build；fragment自定义reject被绕过，Shared`1→7→1`、中央绿色，与T148非opaque相反 |
| BATCH166 / T166 opaque descriptor 分配 AS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t166_capture.rdc` SHA `a540260cce00…`；库/app `a0d4076aeb32…` | 第二BLAS经 descriptor 分配，随后 `buildOpaqueTriangle`；fragment自定义reject仍被绕过，Shared`1→7→1`、中央绿色 |
| BATCH165 / T165 显式opaque三角形AS | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t165_capture.rdc` SHA `ae98acfea13e…`；库/app `19ae3d8f9332…` | 第二BLAS的opaque build，fragment自定义reject链仍命中；Shared`1→7→1`、中央绿色，与T148相反 |
| BATCH164 / T164 默认descriptor AS encoder | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t164_capture.rdc` SHA `581fdf7ca5f9…`；库/app `0ace9b0c4581…` | 两个descriptor AS pass构建BLAS/TLAS，compute输出末→首→末`0,1→0,9→0,1`；无counter attachment |
| BATCH163 / T163 函数表显式residency | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t163_capture.rdc` SHA `b0c5269d639b…`；库/app `917ed5804329…` | render `useResource(Read, Fragment)`声明嵌套表；交点结果Shared`1→7→1`、中央绿色，错误encoder畸形输入已终端干净拒绝 |
| BATCH161–162 / T161–T162 IFT嵌套visible table | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t161/162_capture.rdc` SHA `997d1dcf918a…` / `6d9ff50d3ba4…`；库/app `1ad5d34e7b80…` | fragment表slot2中visible table参数slot1/range`(1,1)`，buffer参数slot0；Shared末→前→末`1→7→1`，中央绿色；零值反向对照仅终端验 |
| BATCH159–160 / T159–T160 IFT buffer参数 | 原生/定向终端通过；panic后全量暂停，待人工L4 | `t159/160_capture.rdc` SHA `c0ae4c26d4ca…` / `dba038596c62…`；库/app `d9498312a666…` | fragment表slot2中buffer参数slot0/range`(0,1)`，Shared末→前→末`1→7→1`，中央绿色；零值反向对照仅终端验 |
| BATCH156–157 / T156–T157 opaque-triangle表快捷更新 | 定向终端通过，06:26 GPU panic后全量暂停，待人工L4 | `t156/157_capture.rdc` SHA `96ef002c3c03…` / `61dc07c81b38…`；库/app `e291aae57542…` | fragment slot2表的单槽/range `(0,1)`更新；与T148红色/Shared0相反，应为绿色/Shared1，末→前→末`1→7→1` |
| BATCH148–153 / T148–T153 Intersection Function Table | 修正shader签名后重录，定向终端通过；新capture尚无全量，待人工L4 | `t148–153_capture.rdc` SHA依次`1843e61a594d…`、`91a1118b4d74…`、`c500b52ab6d4…`、`7531603f7cfa…`、`937d11fab706…`、`6673610202f8…`；库/app `d9498312a666…` | 三阶段单槽/range slot2表绑定；compute为`1,0,1`，渲染阶段Shared末→前→末`0→7→0`；fragment/vertex红色，tile为dispatch |
| BATCH146–147 / T146–T147 Vertex/Tile TLAS射线 | 全量终端自动通过，待人工L4；阶段开放 | `t146/147_capture.rdc` SHA `abeaf2f81566…` / `10073db853eb…`；库/app `2140c924a230…` | vertex与tile各绑定TLAS slot0，Shared输出末→前→末均为`1→7→1`；T146中央绿色，T147为tile dispatch而非普通draw |
| BATCH145 / T145 Fragment TLAS射线 | 全量终端自动通过，待人工L4；阶段开放 | `t145_capture.rdc` SHA `78a3802c5057…`；库/app `a624ff54d9b8…` | TLAS绑定fragment slot0，绘制中央绿色；Shared输出末→前→末为`1→7→1`，与T144两个BLAS引用关系一致 |
| BATCH144 / T144 不同BLAS双实例 | 全量终端自动通过，待人工L4；阶段开放 | `t144_capture.rdc` SHA `3afa4d1f0c9a…`；库/app `42099b733d9f…` | 两套BLAS、实例索引0/1、x=-2/+2变换；三条GPU ray从x=-2/+2/+3返回`1,0,1`，末→首→中→末检查Shared输出 |
| BATCH143 / T143 同BLAS两个TLAS实例 | 全量终端自动通过，待人工L4；阶段开放 | `t143_capture.rdc` SHA `0a41991b814b…`；库/app `c028455d3a89…` | x=-2/+2两实例、128-byte描述符、三条ray hit/miss/hit；输出末→首→中→末`1,0,1`→`1,9,11`→`1,0,11`→`1,0,1` |
| BATCH142 / T142 单实例TLAS与ray-query | 全量终端自动通过，待人工L4；阶段开放 | `t142_capture.rdc` SHA `54d563c23e9b…`；同上库 | BLAS1536→TLAS1792，单实例平移x=2；绑定TLAS的两次ray dispatch，输出末→首→末`0,1`→`0,9`→`0,1`；不要求AS内部几何可视化 |
| BATCH141 / T141 底层AS原位refit与ray-query | 终端自动通过，待人工L4；阶段开放 | `t141_capture.rdc` SHA `0ca68274fce…`；库/app `9f1860586102…` | refittable build→首ray dispatch→GPU blit顶点→refit→次ray dispatch；输出buffer末→首→末为`1,0`→`1,9`→`1,0`；不要求AS内部可视化 |
| BATCH140 / T140 压缩AS copy | 终端自动通过，待人工L4；阶段开放 | `t140_capture.rdc` SHA `ea999aadd79c…`；库/app `2458b8caf470…` | 源1536→目标1280 bytes，build→源尺寸write→compact copy→目标write；Shared尺寸/输出均1280，末→copy→末seek；不要求射线着色器可视化 |
| BATCH139 / T139 等容量AS copy | 终端自动通过，待人工L4；阶段开放 | `t139_capture.rdc` SHA `18bdace6e72a…`；同上库 | 两个1536-byte AS，build源→copy目标→write目标，1280→0→1280 seek；不要求内部几何可视化或压缩分配 |
| BATCH136–138 / T136–T138 AS扩展 | 终端自动通过，待人工L4；阶段开放 | `t136/137/138_capture.rdc` SHA `8348a71bee6b…` / `d3e4b90600db…` / `e10385ed0664…`；同上库 | UInt16索引三角形、bounding box与descriptor创建；同AS身份、index/box资源、1280→0→1280 seek；不要求完整光追 |
| BATCH135 / T135 静态三角形AS build | 终端自动通过，待人工L4；阶段开放 | `t135_capture.rdc` SHA `0acbe500d4aa…`；同上库 | AS资源size1536、begin/build/write/end事件及同一资源ID、Shared输出8字节1280的write→build→write seek；不要求AS内部可视化或完整ray tracing |
| BATCH131–132 / T131–T132 Heap aliasable安全子集 | 终端自动通过，待人工L4；阶段开放 | `t131/132_capture.rdc` SHA `49579bc8afd7…` / `686a80808ce7…`；库/app `d4f73865b86c…` | Heap→子buffer/texture关系、尾部`makeAliasable`调用、既有数据/画面与seek；**不**测试重叠复用，T133为预期拒绝且不打开 |
| BATCH128–130 / T128–T130 Tile Visible Table | 终端自动通过，待人工L4；阶段开放 | `t128/129/130_capture.rdc` SHA `d8593f07a9e0…` / `96b50761c01b…` / `e2704199df7c…`；库/app `cf2a2ccc430e…` | Tile PSO→linked visible函数→handle/table，单槽/范围绑定，异步描述符快照，三次tile计数、最终像素和seek；不要求intersection table |
| BATCH124–127 / T124–T127 Compute Visible Table | 终端自动通过，待人工L4；阶段开放 | `t124/125/126/127_capture.rdc` SHA `ccbff8491538…` / `b07cf63d6c69…` / `3c47723d8b24…` / `0200b2db3881…`；同上库 | Compute直接和argument-buffer间接表绑定，单槽/范围更新，516-byte写入、draw与seek；核对资源父子链接 |
| BATCH119–123 / T119–T123 Render Visible Table | 终端自动通过，待人工L4；阶段开放 | `t119/120/121/122/123_capture.rdc` SHA `bbf65de21056…` / `4e27d1884a94…` / `0d9fa4321400…` / `8f1561df2b59…` / `3d670a81bb2d…`；同上库 | Fragment/Vertex直接链接、handle/table、单槽/范围绑定、红/绿GPU画面与seek |
| BATCH118 / T118 一层嵌套ArgumentEncoder | 定向自动通过，待人工L4；阶段开放 | `t118_capture.rdc` SHA `e3fc631efe28…`；库/app `84df606e5379…` | 父/子encoder及内层buffer资源链接、texture四象限和seek；Pipeline State暂不展示嵌套树 |
| BATCH116–117 / T117 Placement Heap Texture | 定向自动通过，待人工L4；阶段开放 | `t117_capture.rdc` SHA `3051cc3039d7…`；库/app `8dab14641eb8…` | Heap→Private 1×1 texture，offset128，三次clear/sample及事件seek；不要求alias |
| BATCH116–117 / T116 Placement Heap Buffer | 定向自动通过，待人工L4；阶段开放 | `t116_capture.rdc` SHA `9ab0c4160699…`；同上库 | Heap→Private 516-byte buffer，offset768，fill/compute/copy/fragment数据与画面seek；不要求alias |
| BATCH112–115 / T115 异步Vertex预加载 | 定向自动通过，待人工L4；阶段开放 | `t115_capture.rdc` SHA `1ebdda69de37…`；库/app `5043305f5061…` | 异步render vertex调用动态函数，预加载资源ID、buffer3与红色draw/seek |
| BATCH112–115 / T114 异步Compute预加载 | 定向自动通过，待人工L4；阶段开放 | `t114_capture.rdc` SHA `91400f1a0218…`；同上库 | 异步compute descriptor预加载资源ID、buffer3与红色draw/seek |
| BATCH112–115 / T113 异步Render Options预加载 | 定向自动通过，待人工L4；阶段开放 | `t113_capture.rdc` SHA `123dfe302042…`；同上库 | options/reflection callback路径的fragment预加载、buffer3与红色draw/seek |
| BATCH112–115 / T112 异步Render Fragment预加载 | 定向自动通过，待人工L4；阶段开放 | `t112_capture.rdc` SHA `26f406abdcec…`；同上库 | fragment动态函数预加载、buffer3与红色draw/seek；离线不运行应用回调 |
| BATCH110–111 / T111 异步可执行库 | 定向自动通过，待人工L4；阶段开放 | `t111_capture.rdc` SHA `3a9dcd1eccd8…`；库/app `2082ea672c61…` | 异步executable源含动态库依赖，GPU buffer3、红色draw与seek；离线不运行应用回调 |
| BATCH110–111 / T110 异步动态源码库 | 定向自动通过，待人工L4；阶段开放 | `t110_capture.rdc` SHA `791e88f7e8e6…`；同上库 | 异步dynamic source→dynamic library→executable依赖，buffer3、红色draw与seek |
| BATCH106–109 / T109 Render options preload | 定向自动通过，待人工L4；阶段开放 | `t109_capture.rdc` SHA `5c2186bd7f02…`；库/app `4c5ee0e420c6…` | options/reflection render pipeline的fragment预加载动态库，GPU buffer3、红色draw和seek |
| BATCH106–109 / T108 Vertex preload | 定向自动通过，待人工L4；阶段开放 | `t108_capture.rdc` SHA `07b7eef99a4f…`；同上库 | vertex函数调用动态库，vertex预加载资源链接、buffer3、红色draw和seek |
| BATCH106–109 / T107 Fragment preload | 定向自动通过，待人工L4；阶段开放 | `t107_capture.rdc` SHA `b67564bf9344…`；同上库 | fragment函数调用动态库，fragment预加载资源链接、buffer3、红色draw和seek |
| BATCH106–109 / T106 Compute preload | 定向自动通过，待人工L4；阶段开放 | `t106_capture.rdc` SHA `86823865a8b3…`；同上库 | compute descriptor预加载动态库资源、buffer3、红色draw和seek |
| BATCH104–105 / T105 双动态库链接 | 定向自动通过，待人工L4；阶段开放 | `t105_capture.rdc` SHA `7520798eaddd…`；库/app `7d2400de2ed2…` | 两份动态source/library依赖、一个compute+draw，buffer值6、中心红色与前后seek；不依赖应用原临时路径 |
| BATCH104–105 / T104 动态库链接 | 定向自动通过，待人工L4；阶段开放 | `t104_capture.rdc` SHA `22147d810218…`；同上库 | 一份动态source/library和可执行library依赖、buffer值3、中心红色与前后seek |
| BATCH103 / T103 Counter非零范围 | 全量自动通过，待人工L4；阶段开放 | `t103_capture.rdc` SHA `a38e39c760c4…`；库/app `30a8ca932d9f…` | 8槽indices2–5、resolve range(2,4)到offset16，四个递增timestamp与未写区域零值 |
| BATCH102 / T102 BufferBinding encoder | 全量自动通过，待人工L4；阶段开放 | `t102_capture.rdc` SHA `b9df0c466069…`；库/app `30a8ca932d9f…` | reflection binding创建，texture id0/sampler id1、长度16对齐8；左右packet及像素、前后seek |
| BATCH101 / T101 Stage-boundary counter | 全量自动通过，待人工L4；阶段开放 | `t101_capture.rdc` SHA `394b6a8f960b…`；库/app `77b615b94015…` | VS/FS pass四阶段索引0–3、sampleBuffer身份、resolve Copy与目标buffer四个递增timestamp；数值不要求固定 |
| BATCH100 / T100 异步Mesh+Rate Map | 全量自动通过，待人工L4；阶段开放 | `t100_capture.rdc` SHA `6997b8195353…`；库/app `d349808b9692…` | 异步pipeline快照含mesh grid上限2，第二层光栅与blit，x100绿/x175黑；应用callback不重放 |
| BATCH99 / T99 Rate Map第二层 | 全量自动通过，待人工L4；阶段开放 | `t99_capture.rdc` SHA `161abbbbaeb2…`；库/app `d349808b9692…` | 两mesh组分别写slice0/1，slice1水平半速率、blit到drawable，x100绿/x175黑；检查maxMeshGrid=2与seek |
| BATCH98 / T98 双层Rate Map数组pass | 定向自动通过，待人工L4；阶段开放 | `t98_capture.rdc` SHA `1e6e084fd2fe…`；库/app `d5ddec74b502…` | 2层map绑定2-slice array target，mesh绘制slice0、blit到drawable绿色中心；不宣称slice1已光栅 |
| BATCH95–97 / T95–T97 Rate map | 全量自动通过，待人工L4；阶段开放 | `t95/96/97_capture.rdc` SHA `b4aa8a2f99ef…` / `9bfcb4bddea0…` / `1cab41975a47…`；库/app `d93f1c816b54…` | T95单层all-1 map与pass引用，T96水平0.5使物理三角形收缩，T97双层descriptor与参数buffer；T97不宣称array-target双层渲染 |
| BATCH93–94 / T93–T94 Object输入网格 | 全量自动通过，待人工L4；阶段开放 | `t93/94_capture.rdc` SHA `0b4476799c01…` / `28f1ccb2592a…`；库/app `3612c349a613…` | T93两个object threadgroup、T94 64×1×1输入线程网格各自产生紫红三角形，不能把输入object网格当作单个输出mesh网格上限 |
| BATCH91–92 / T91–T92 多线程Object | 定向自动通过，待人工L4；阶段开放 | `t91/92_capture.rdc` SHA `3ea72dbf14f6…` / `e18b6eb53bb5…`；库/app `dd543ab096a7…` | 一次object四线程的直接threadgroups与threads-grid各自产生紫红三角形；T92事件网格4×1×1，前后seek |
| BATCH88–90 / T88–T90 Mesh间接绘制 | 全量自动通过，待人工L4；阶段开放 | `t88/89/90_capture.rdc` SHA `81a4e8f91479…` / `67f3eb8a6005…` / `8ca736e3c39f…`；库/app `d74b63d4a264…` | 三份capture各一次`drawMeshThreadgroups(indirect)`与绿色T88/T89、紫红T90三角形；T89前有compute写Private参数；事件网格显示未知是正确的，不要求推断CPU值 |
| BATCH87 / T87 异步Object/Mesh | 定向自动通过，待人工L4；阶段开放 | `t87_capture.rdc` SHA `f6b5c2a71ac9…`；库/app `1e9b10c9196a…` | 异步三函数pipeline、调用时BGRA8/function快照、一次MeshDispatch与中心像素；离线不执行应用回调 |
| BATCH86 / T86 Object动态内存 | 定向自动通过，待人工L4；阶段开放 | `t86_capture.rdc` SHA `6b70534fc597…`；库/app `fbc12b54dfb3…` | 三次threadgroup memory长度16/32/48、三个MeshDispatch及像素/回退；内存值本身由自动测试验证 |
| BATCH85 / T85 Object纹理/采样器 | 定向自动通过，待人工L4；阶段开放 | `t85_capture.rdc` SHA `e3bf1eee74e6…`；库/app `6091de8a8265…` | 六种texture/sampler、采样后payload使三角形到x≈120/250/370及事件回退 |
| BATCH84 / T84 Object buffer/bytes | 定向自动通过，待人工L4；阶段开放 | `t84_capture.rdc` SHA `d29d5e86c1a9…`；库/app `aef27dba4f8d…` | 四种object buffer/bytes API、三次MeshDispatch及x≈80/220/360逐事件像素和回退 |
| BATCH83 / T83 Object/Mesh pipeline | 定向自动通过，待人工L4；阶段开放 | `t83_capture.rdc` SHA `e7e52fa73e7d…`；库/app `77fd1d77b4cf…` | 三函数身份、payload16、setObjectBuffer引用、一次MeshDispatch及中心RGB约179/51/102；不要求object专用UI |
| BATCH82 / T82 异步Mesh pipeline | 定向自动通过，待人工L4；阶段开放 | `t82_capture.rdc` SHA `5c89deaf9996…`；库/app `8d09e99f81c5…` | 异步创建API与BGRA8/mesh函数调用时快照、一次MeshDispatch及中心像素；离线不调用应用回调 |
| BATCH81 / T81 Mesh thread-grid | 定向自动通过，待人工L4；阶段开放 | `t81_capture.rdc` SHA `e6126828325b…`；库/app `2eea4b68f328…` | 一次drawMeshThreads，32×1×1线程网格、MeshDispatch、中心绿色三角形；不要求普通VS面板 |
| BATCH80 / T80 Mesh纹理/采样器 | 定向自动通过，待人工L4；阶段开放 | `t80_capture.rdc` SHA `28d188275e44…`；库/app `54f1bbde30f5…` | 六种绑定、采样后左/中/右各平移约70像素及seek；不要求mesh专用UI |
| BATCH79 / T79 Mesh绑定 | 定向自动通过，待人工L4；阶段开放 | `t79_capture.rdc` SHA `28f4ba76c665…`；库/app `4fc7b5c759ba…` | 3次MeshDispatch左/中/右绿色三角形、4种buffer/bytes API、前后seek；不要求虚构VS/mesh专用UI |
| BATCH78 / T78 Mesh pipeline | 定向自动通过，待人工L4；阶段开放 | `t78_capture.rdc` SHA `160f902aef23…`；库/app `dfe3ec542c36…` | 一个同步mesh pipeline、一次MeshDispatch、三角形中心绿色/背景黑色与资源链接；不要求虚构VS/mesh专用UI |
| BATCH77 / T77 异步Tile pipeline | 定向自动通过，待人工L4；阶段开放 | `t77_capture.rdc` SHA `0470cd9f7ecf…`；库/app `173fadcf7c8a…` | 一个异步tile pipeline创建入口、BGRA8/function/options快照、三条dispatch与buffer/draw回退；离线不执行回调 |
| BATCH76 / T76 Tile动态内存 | 定向自动通过，待人工L4；阶段开放 | `t76_capture.rdc` SHA `1ca3f463afa1…`；库/app `8af6f90b645b…` | 四次threadgroup memory绑定含offset16与零长度清除、三条dispatch、buffer三个计数和seek |
| BATCH75 / T75 Tile纹理/采样器 | 集中自动通过，待人工L4；阶段开放 | `t75_capture.rdc` SHA `647f1bb524cb…`；同上库 | 六种绑定、纹理红通道2/255、三阶段buffer为T74两倍、最终draw与seek |
| BATCH74 / T74 Tile shader | 集中自动通过，待人工L4；阶段开放 | `t74_capture.rdc` SHA `921596de7c36…`；库/app `e74885bf9e09…` | 一个tile pipeline、三条tile dispatch的25×19等实际网格与16×16线程组、12-byte buffer的三段GPU计数和后续draw红色；末→首→中→末seek；Tile专用Pipeline State面板尚未建模，不要求显示为VS/FS |
| BATCH73 / T73 Render heap residency | 集中自动通过，待人工L4；阶段开放 | `t73_capture.rdc` SHA `2bff846250a6…`；库/app `57efba093c53…` | 四种useHeap(s)与同一Heap资源链接、三个draw颜色`10/20/30`→`40/50/60`→`70/80/90`和末→首→中→末seek；T70负例不可打开 |
| BATCH71–72 / T71 Heap buffer | 集中自动通过，待人工L4；阶段开放 | `t71_capture.rdc` SHA `5d4f73a88a97…`；库/app `c36b553dcd14…` | 自动Private Heap父子资源、516-byte buffer计算结果/fragment绑定与T39一致、事件回退；placement/alias不要求 |
| BATCH71–72 / T72 Heap texture | 集中自动通过，待人工L4；阶段开放 | `t72_capture.rdc` SHA `958442028c01…`；同上库 | Heap父子资源、1×1 Private纹理三次GPU clear/sample，RGB`10/20/30`→`40/50/60`→`70/80/90`及回退；T70负例不可打开 |
| BATCH69 / T69 shared-event handle别名 | 集中自动通过，待人工L4；阶段开放 | `t69_capture.rdc` SHA `3ac0d738542d…`；库/app `22c244afc13a…` | 导入event为第二资源但共享GPU时间线；两个draw灰度10→80、两queue依赖与后→前→后seek |
| BATCH67–68 / T67 patch draw重载 | 集中自动通过，待人工L4；阶段开放 | `t67_capture.rdc` SHA `93f02f45b77b…`；库/app `f8b8b15d458…` | 三个draw依次红绿蓝；直接indexed的3控制点/1实例，两个间接draw参数显示未知但画面正确，前后seek |
| BATCH67–68 / T68 同进程shared handle | 集中自动通过，待人工L4；阶段开放 | `t68_capture.rdc` SHA `05ea59ff1f1f…`；同上库 | 源纹理handle导出与导入资源父子关系；同一导入纹理两阶段采样RGB`20/40/60`→`70/90/110`及回退；跨进程handle不支持 |
| BATCH66 / T66 直接patch细分 | 集中自动通过，待人工L4；阶段开放 | `t66_capture.rdc` SHA `29473c46466a…`；库/app `d098a04a1a7b…`，GUI executable未变 | 一个 `drawPatches` 子事件、factor buffer资源链接、scale值1、中心像素RGB约`64/128/191`、draw与clear间回退；间接/索引patch尚未接通 |
| BATCH65 / T65 SharedEvent初值/GPU同步 | 集中自动通过，待人工L4；阶段开放 | `t65_capture.rdc` SHA `1b78708a1d3e…`；库/app `d69fa7da1f8…`，GUI executable未变 | 初值100、三次wait/两次signal跨队列身份与值；两draw灰度10→80及回退。后续CPU赋值仍明确拒绝；同进程handle导入另见T69 |
| BATCH63–64 / T63 动态顶点 stride | 集中自动通过，待人工L4；阶段开放 | `t63_capture.rdc` SHA `9d88a151f271…`；库/app `8772ed87e268…`，GUI executable未变 | 四次draw的不同顶点来源、offset/stride、RGB与末→首→中→末回退；单视图amplification API参数 |
| BATCH63–64 / T64 descriptor-backed shared texture | 集中自动通过，待人工L4；阶段开放 | `t64_capture.rdc` SHA `10400b736950…`；同上库 | Private 1×1创建chunk、三次GPU clear后同一FS texture采样、RGB与事件回退；shared handle不在范围内 |
| BATCH62 / T62 purgeable状态 | 集中自动通过，待人工L4；阶段开放 | `t62_capture.rdc` SHA `88d2fbc35494…`；库/app `e11191df7a77…`，GUI executable未变 | 两资源的KeepCurrent/NonVolatile调用、同一draw绑定与RGB`15/26/37`；设备只读查询无需新增UI项 |
| BATCH61 / T61 no-copy buffer | 集中自动通过，待人工L4；阶段开放 | `t61_capture.rdc` SHA `adad0c224323…`；库/app `0f7f42e63b34…`，GUI `fe8bcf852b68…` | 创建chunk、FS offset256、三提交像素/数据回退；抓取回调延迟另见BATCH61 |
| BATCH60 / T60 Device ArgumentEncoder | 集中自动通过，待人工L4；阶段开放 | `t60_capture.rdc` SHA `847db07d39f5…`；最新库/app见上行 | 创建chunk、两packet的FS texture/sampler成员与左右画面回退；详见总单 |
| BATCH58–59 / T58 纹理view | 集中自动通过，待人工L4；阶段开放 | `t58_capture.rdc` SHA `c88c79833879…`；最新库/app见上行 | 三种view资源父子链接、subset/swizzle、三阶段九draw画面与回退；详见总单 |
| BATCH58–59 / T59 buffer-backed纹理 | 集中自动通过，待人工L4；阶段开放 | `t59_capture.rdc` SHA `0a9c40b8edef…`；同上构建 | 父buffer/子texture链接，初值→CPU更新→GPU写三阶段画面与回退；padding已自动核对 |
| BATCH57 / T57 Argument数据 | 集中自动通过，待人工L4；阶段开放 | `t57_capture.rdc` SHA `c852c41b973c…`；库 `8e84e249d684…`、GUI `fe8bcf852b68…` | FS buffer成员id0/2/3、资源链接/offset、两packet及四draw画面/数据回退；详见总单 |
| BATCH54–56 / T54 indexed短重载 | 自动通过，待人工L4；阶段开放 | `t54_capture.rdc` SHA `7fa91d16ce14…`；库 `71cf518d1e5b…` | 两实例、bases0、index offset4与vertex offsets8/24、Mesh/资源/红蓝回退；legacy blit已自动验不另加UI打开项 |
| BATCH54–56 / T55 Function创建 | 自动通过，待人工L4；阶段开放 | 更新`t55_capture.rdc` SHA `6be475171749…`；最新库见首行 | 四shader/PSO身份、特化初始化、548-byte结果与回退；新增两intersection function链接，不要求重演callback或新增常量面板 |
| BATCH54–56 / T56 Argument数组 | 自动通过，待人工L4；阶段开放 | `t56_capture.rdc` SHA `9cb0d420ac9c…`；同上库 | textures id2/3、samplers id6/7、数组名称/资源链接、混合四象限/回退；仅本波收窄范围 |
| BATCH53 / T53 ICB operations | 自动通过，待人工 L4；阶段开放 | `t53_capture.rdc` SHA `6ad98408529a…`；库 `a5dcf57234590…` | GPU reset/copy/optimize、单命令 reset、空命令与有效 draw、复制状态、四阶段画面及初值回退 |
| BATCH51–52 / T51 async creation | 自动通过，待人工 L4；阶段开放 | `t51_capture.rdc` SHA `e0fc940f6ce0…`；库 `8dbbe2d60146…` | 六种异步初始化、五pipeline/shader链接、buffer/左右半屏回退；原生回调/错误/反射已自动核对，离线不执行block |
| BATCH51–52 / T52 Event synchronization | 自动通过，待人工 L4；阶段开放 | `t52_capture.rdc` SHA `e0116d679d5f…`；同上库 | 两Event/三提交、六signal/wait的资源与值、buffer/画面回退；不要求SharedEvent或并行时间线 |
| BATCH50 / T50 texture CPU readback | 自动通过，待人工 L4；阶段开放 | `t50_capture.rdc` SHA `f56406953e8d…`；库 `a648ce05f576…` | 四次getBytes、三次Managed同步、独立资源保留、CPU派生参数回退；离线不复写应用指针，不要求3D viewer |
| BATCH49 / T49 command buffer handlers | 自动通过，待人工 L4；阶段开放 | `t49_capture.rdc` SHA `82771996ea03…`；库 `1cd92c21c774…` | 八条scheduled/completed注册、两command buffer身份、CPU参数/计算结果回退；离线不执行应用block |
| BATCH48 / T48 binary libraries | 自动通过，待人工 L4；阶段开放 | `t48_capture.rdc` SHA `f71eaf76384b…`；库 `6df633b629eb…` | 五种加载初始化、binary shader反射/资源链接、首末dispatch回退；无原始MSL属预期 |
| BATCH47 / T47 pipeline creation variants | 自动通过，待人工 L4；阶段开放 | `t47_capture.rdc` SHA `a28eef0f401b…`；库 `16c921313c2f…` | 六pipeline、options/descriptor初始化记录、CS/VS/FS资源、事件回退及左右半屏 |
| BATCH45-46 / T44 fences | 自动通过，待人工 L4；阶段开放 | `t44_capture.rdc` SHA `caa028676f68…` | Blit/Compute/Render同步链、fence身份、GPU数据与事件回退 |
| BATCH45-46 / T45 atTime + buffer markers | 自动通过，待人工 L4；阶段开放 | `t45_capture.rdc` SHA `9e71f6b21883…` | time=1.25、Present输出、marker范围/移除及172-byte独立buffer |
| BATCH45-46 / T46 minimum duration + buffer markers | 自动通过，待人工 L4；阶段开放 | `t46_capture.rdc` SHA `7b1fbea45e1f…` | duration=0.001、Present变体与输出，仅做相对T45差异 |
| BATCH43-44 / T42 compute resource/barrier | 自动通过，待人工 L4；阶段开放 | `t42_capture.rdc` SHA `25c6e7349f22…` | single/batch声明、scope/resource barrier、markers、计算链与事件回退 |
| BATCH43-44 / T43 render resource/barrier | 自动通过，待人工 L4；阶段开放 | `t43_capture.rdc` SHA `7139504e2357…`；GUI `3cc9c3b63507…` | batch/stage声明、跨draw依赖、VS Storage Buffers可写条目、资源保留 |
| BATCH41-42 / T40 compute inline/offset/threadgroup | 自动通过，待人工 L4；阶段开放 | `t40_capture.rdc` SHA `73a3398cec48…` | inline 参数、真实buffer offset、threadgroup memory，五次reduction与事件回退 |
| BATCH41-42 / T41 blit descriptor/optimization | 自动通过，待人工 L4；阶段开放 | `t41_capture.rdc` SHA `35745dc5f7ab…` | descriptor、四条hint参数、13×7 hint-only纹理，原copy结果不变 |
| BATCH39-40 / T38 sampler LOD | 自动通过，待人工 L4；阶段开放 | `t38_capture.rdc` SHA `92d869806241…` | VS/FS/CS clamp 参数与有效 sampler 描述、覆盖/清空、黄黑蓝画面和compute三色 |
| BATCH39-40 / T39 private buffer readback | 自动通过，待人工 L4；阶段开放 | `t39_capture.rdc` SHA `0d552603e709…` | 516-byte Private GPU buffer 内容、局部读回、事件前后跳转 |
| BATCH35-37 / T34 render inline/batch binding | 自动通过，待人工 GUI L4；阶段开放 | qrenderdoc app；`t34_capture.rdc` SHA `e6ee77deb50a…`；库见上方 | 五项 state 调用/API 参数、VS/FS buffer offsets、inline slot 不残留旧资源、最终像素与状态栏 |
| BATCH35-37 / T35 command 创建变体 | 自动通过，待人工 GUI L4；阶段开放 | 同一 app；`t35_capture.rdc` SHA `7fbed8c95df6…` | 两个 compute pass/dispatch、Concurrent/Serial、`waitUntilScheduled`、output offsets 0/4；首dispatch `17,0`、次dispatch `17,17`并回退（BATCH57修正初值）、clear与状态栏 |
| BATCH35-37/38 / T36 render 动态状态 | 自动通过，待人工 GUI L4；阶段开放 | 同一 app；`t36_capture.rdc` SHA `436b3262fef5…` | 原六项状态和五项 marker，新增 visibility/store/options/barrier；counter 55488、EndPass store 摘要、RS、中心/角落输出；未建模专用字段不要求显示 |
| BATCH38 / T37 blit transfer | 自动通过，待人工 GUI L4；阶段开放 | 同一 app；`t37_capture.rdc` SHA `f87cbbc8ca00…` | 六个 copy 的参数/资源、buffer padding、slice/mip、前后 seek、最终纯色 |
| BATCH38 / T10 marker 增量 | 自动通过，待最短 GUI 复验；原 T10 已验结论保留 | 同一 app；独立 `t10_debug_capture.rdc` SHA `2d4a8d609bae…` | 三条 blit marker API 与字符串，原画面不变；不要求新增分组层级 |

T32/T33 及此前批次的 GUI 已验状态不变。T31 GUI 导出入口此前由用户明确免除
复验，不记作 GUI 实测。action 名称后续工作见 `ACTION_NAME_ALIGNMENT.md`。

## BATCH33-34 验收归档

| 批次 / 功能 | 状态 | 验收单与版本 | 用户反馈 |
| --- | --- | --- | --- |
| BATCH33-34 / T32 compute indirect dispatch | 自动与 GUI L4 通过；批次已关闭 | `QA_BATCH33-34.md`；`t32_capture.rdc` SHA-256 `9aa1432c8d9a…`；GUI `f1ebea2ddb86…`；库 `aa21c9a6983d…` | EID 6/12 的参数、CS、资源、画面、状态栏及新版 `<2, 2, 1>` 摘要已确认 |
| BATCH33-34 / T33 GPU 生成间接参数 | 自动与 GUI L4 通过；批次已关闭 | `QA_BATCH33-34.md`；`t33_capture.rdc` SHA-256 `4478197f9994…`；同一 GUI/库 | EID 7/10/12/18 的参数、CS、资源、画面、状态栏及新版 `<2, 2, 1>` 摘要已确认 |

## BATCH31-32 验收归档

| 批次 / 功能 | 状态 | 最终构建与 capture | 验收单 | 用户反馈与备注 |
| --- | --- | --- | --- | --- |
| BATCH31-32 / T30 compute sampler | 自动与 GUI L4 通过；批次已关闭 | `build-macos-debug/bin/qrenderdoc.app`；`captures/metal-smoke/t30_capture.rdc`；capture SHA-256 `8bfe33501fad…`；最新库 `7a494e202d1b…` | `QA_BATCH31-32.md` | 原功能已验；用户进一步确认 EID 4/13 顶层及 `$action()` 筛选符合预期 |
| BATCH31-32 / T31 compute batch binding | 自动与 GUI L4 通过；批次已关闭 | 同一 app；`captures/metal-smoke/t31_capture.rdc`；capture SHA-256 `f5ad1c86660b…`；最新库 `7a494e202d1b…` | `QA_BATCH31-32.md` | EID 7、12–14、18 及后续画面/空槽/筛选/状态栏已确认；GUI 导出入口本轮免复验，自动 DDS/raw 已核对 |

## BATCH29-30 验收归档

| 批次 / 功能 | 状态 | 最终构建与 capture | 验收单 | 用户反馈 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| BATCH29-30 / T28 `dispatchThreads` | 自动与 L4 通过 | `build-macos-debug/bin/qrenderdoc.app`；`captures/metal-smoke/t28_capture.rdc`；capture SHA-256 `b54fb5128aaf…`；GUI `b8259e2471ce…`；库 `81738a1bcbde…` | `QA_BATCH29-30.md` | 用户确认原 L4、10×7 Texture 20 DDS、右侧 Input/Output 缩略图；DDS 与自动参考完全一致 | 无 |
| BATCH29-30 / T29 compute buffer binding | 自动与 L4 通过 | `build-macos-debug/bin/qrenderdoc.app`；`captures/metal-smoke/t29_capture.rdc`；capture SHA-256 `99787263f8b9…`；GUI `b8259e2471ce…`；库 `81738a1bcbde…` | `QA_BATCH29-30.md` | 用户确认 Buffer 20、API Inspector、状态 API EID、画面、导出、状态栏、`$action()` 筛选及右侧缩略图 | 无 |

## 2026-09-24 历史反馈与修复状态

- 用户重存的 `captures/metal-smoke/t28-output-ui.dds` 为 408 字节，SHA-256
  `ada9ab66b7c5…`，与 T28 自动参考 DDS 逐字节一致；T28 DDS 项已验收。
  用户确认 T22 父行可展开为 EID 6/7，点父行正常绘制；T22 新行为复验已通过，
  移出待验表。当前剩 T29 `$action()` 行为及本轮 Texture Viewer 缩略图修复的
  T28/T29 最短 GUI 复验。
- 缩略图根因是 Metal 未支持通用 `ReplayOutput::DrawThumbnail` 所需的 Headless
  output。已修复并在最新库 `81738a1bcbde…` 上通过 T01/T03/T09/T11/T12/
  T16/T17/T22/T25/T28/T29 共 11 份 Replay API 和逐份 CLI replay；
  12 份 capture × 10 轮 lifecycle 通过。自动证据不替代右侧缩略图 GUI 观察。

- 以下为缩略图修复前的历史检查点；顶部表格及本节前两条是当前状态。
  当时用户确认 T25 的多个 instance 显示、ICB 子 draw 收入 execute、点击 execute
  可画到最后一个子 draw；还确认 T27/T28 的所见画面与 `setBuffer` 等 API 有 EID。
  T25 此次新行为最短 L4 已通过，移出待验表；T27 原 L4 已通过。T28 的 UI DDS
  文件未提及，磁盘上仍是旧 Texture 30 文件。T29 的 EID 存在已确认，但
  `$action()` 隐藏/恢复尚未明确反馈。T22 指定 EID 5 的最终红蓝画面未明确反馈。

- 用户已确认 **T29 Buffer 20 变化**和新版 API Inspector 中 `setBuffer` 等调用可见。
  旧表中 T29 的“EID 2/5 待验”已被此反馈覆盖；旧 EID 已失效，不再要求重复。
- Metal 事件树无筛选时缺少状态调用、ICB 父子层级/父行 seek 和 indexed instanced
  子行数量为本轮新缺陷。已在 frame load 中为非 action 调用分配 EID，ICB 父行改为
  MultiAction，点击父行回放至最后子 draw，范围显示 location/length，子行显示
  `instances=2`。正式 T28/T29 captures 已重生成，当前 GUI binary SHA-256
  `b8259e2471ce…`。T29 当前 EID 10/11 为两条 setBuffer，EID 12 为 dispatch。
  Replay API/CLI、异常拒绝与 lifecycle 定向通过；上述新 GUI 行为仍须用户 L4。
- T28 的 10×7 Texture 20 UI DDS 已重存并核对通过。当前 EID 7 是 dispatch，
  缩略图复验步骤见新验收单顶部。
- T22/T25 原功能验收继续有效，仅对本轮改动的 Event Browser 层级、父行画面和
  实例数重新登记最短复验。BATCH29-30 仍未关闭；旧批次不因这次待复验追溯关闭状态。

## 登记与状态规则

1. 新功能开始时就在本表按 T 编号登记“开发中，后续需人工 L4”；同时在本轮结果中
   提醒该功能将需要人工 QA。功能尚未形成正式 capture 时不要求用户提前测试。
2. 最终自动验证完成后，填入最终构建、正式 capture、验收单路径或对应回复，以及
   最后一次影响 GUI 的代码/构建标识；状态改为“自动通过，待人工 QA”。同一批可给
   用户一份合并验收单，但每个 T 的状态分别保留。
3. 只有用户针对明确的验收单回复“全部符合”，或逐项明确通过，才能把对应 T 的
   L4 标为“已通过”。收到部分反馈时只更新已明确覆盖的项；未提到的步骤和 T
   继续“待人工 QA”。未回复、漏看结果、聊天结束、时间经过、下一批开始，均不构成通过。
4. 若反馈不符，记录 T 编号、EID、步骤和现象，状态为“需修复/复验”；修复后 agent
   重跑受影响自动检查，更新 capture/验收单并给最短复验步骤。影响待验功能的后续
   代码或构建变化，也要判断旧验收单是否失效；失效则更新证据和待验版本。已验收
   的功能若被后续 GUI 变更实质影响，也应重新登记受影响的最短 L4 复验项。
5. 批次只有在其所有 T 的 L4 均“已通过”且其他关闭条件也满足时才可标“已关闭”。
   可以继续开发下一功能或下一批，但要在 `STATUS.md`、本表、交接提示和每次结果中
   列出全部仍待人工 QA 的 T；后续验收单优先合并这些未验项，避免重复打开程序。
6. 不自动催促或替用户补做 GUI QA。若用户说不理解或反馈不符，先解释/修复并给
   精简复验；沟通仍无法确认或用户明确要求时，才由 agent 用 Computer Use 定向检查。

本表记录结果而非替代验收单。T28/T29 的正式 captures 与验收步骤见
`QA_BATCH29-30.md`；T30/T31 的正式 captures 与验收记录见 `QA_BATCH31-32.md`。
T30–T33 所在批次均已关闭；当前 T34–T69 和 T10 marker 待人工 GUI L4，见
`QA_CONSOLIDATED.md`。
