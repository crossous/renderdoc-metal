2026-10-02 [B438](BATCH438_INTERLEAVED_GPU_PRODUCER_OWNERSHIP.md)/[B439](BATCH439_RETIRED_PRIVATE_TEXTURE_BACKING.md)：当前 b814fdbb 已通过同一 UE faa8540e 的严格审计、正常 OpenCapture、整帧 EID9924 与反复 EID0 重置。三份原生900×640图像逐字节一致，重编码JPEG与原捕获缩略图逐字节一致；EID2017实际indirect为8,1,1，BasePass五MRT及两组64³体积MRT重复读回稳定。修复各自定向后立即返回同一UE，再进行一次当前库验收全量308/7786/3080，通过后再次返回同一UE通过。当前库 qrenderdoc 正常加载，实际第五MRT、体积Slice63及完整编辑器/天空/黄色平台观察通过，正常退出；这是助手UI操作记录，未声称用户人工验收或独立原始无损图像golden。校验归档763历史目标至外置CauseUseMac（12.22GiB逻辑数据），34原始项目截帧全部保留，清理后构建约3.2GiB，后续验收产物生成后约3.7GiB、可用约13GiB。无提交推送。当前使用入口及已验收副本见 [运行说明](../../util/ue/README.md)。

# 集中 GUI QA 总入口

2026-10-01 BATCH332–333：六类tiny终端、56负例及slot诊断导出/拒绝通过。
本批新增Viewer人工验收未运行；UE NewMap编辑器正常启动观察不算UE replay
人工验收。完整UE、MRT/pass scope与旧T待验保持，全量未跑。见[BATCH333](BATCH333_UE_PARTIAL_PROVIDER.md)。

2026-10-01 BATCH331：最终5fab70bf…库/app真实Viewer Private view小帧
89/167/89、哨兵与绑定正确，正常退出。plain Private最终UI另次被锁屏阻止，
不算通过；完整UE与旧T待验保持，全量未跑。见[BATCH331](BATCH331_FRAME_DESCRIPTOR_RESOURCES.md)。

2026-09-30 BATCH330：人工UI新增通过两类descriptor小帧。最终78e3cecc…库
table EID9/25/9输出122/161/122，混合EID15/34/53/15输出41/121/160/41，哨兵
不变；窗口loaded/no problems detected、正常退出。此结果不验收UE真帧或旧T
系列，旧待验保留。全量未跑。见[BATCH330](BATCH330_DUPLICATE_IDENTITIES_AND_VIEWER.md)。

2026-09-30 BATCH328–329：最终库/app `bd255367cb60…`，实际typed小帧GPU replay
与GPU/CPU混合entry更新通过；UE自动重截end=1、primary布局记录通过，UE
完整replay仍未达到。Computer Use返回macOS锁屏，人工Viewer验收未完成，
已请求手动解锁；成功UI QA增量0，旧待验保留。见
[BATCH329](BATCH329_UE_DESCRIPTOR_LAYOUTS_AND_DRAWABLE_LIFETIME.md)。

2026-09-30 BATCH327：修复间接资源capture过滤，助手已自动重截NewMap。
最终库/app `6c0e5d6f8cfe…`，sampler表映射覆盖已有CPU证据；UE回放仍在
未实现descriptor relocation处拒绝。没有新人工UI验收，全部旧项继续保留。
不再要求用户重截，见 [BATCH327](BATCH327_GPU_IDENTITY_RESOURCE_CLOSURE.md)。

2026-09-30 BATCH326：本地NewMap新UE帧只有CPU审计证据，缺原生GPU身份映射。
新诊断库保守拒绝该族的GPU回放，尚未实现descriptor重定位；无新GUI通过。
库/app `c7cb40b786ff…`，用户需重截获取地址诊断数据，见
[BATCH326](BATCH326_GPU_IDENTITY_DIAGNOSTICS.md)。所有旧待验项继续保留。

2026-09-30 BATCH325 增量：确认 23:15 WindowServer watchdog 整机重启，
没有新的 GUI 验收；当前远程机暂停 UE 大帧 GPU/GUI 测试。累计成功
UI QA 增量 **0**；见 [BATCH325](BATCH325_WINDOWSERVER_WATCHDOG_2026-09-29.md)。

2026-09-29 BATCH324 增量：UE 真帧诊断副本的 API-only 定位停在 Metal
`waitUntilCompleted`；未获得打开结果，WindowServer 短暂不就绪后恢复。
本批无 GUI 操作，画面、MRT 和 scope 均未验收；累计成功 UI QA 增量
**0**。见 [BATCH324](BATCH324_UE58_GPU_WAIT_AND_REMOTE_HOST_STOP.md)。

2026-09-29 BATCH323 增量：帧尾 buffer `Empty` 的 4 KiB 小夹具终端
原生/注入/API/CLI、GPU 字节与 seek 通过；`UE58_capture.rdc` 尚未用
本批库回放或人工打开。累计成功 UI QA 增量 **0**；见
[BATCH323](BATCH323_TERMINAL_PURGEABLE_REPLAY.md)。

2026-09-29 BATCH322 增量：`UE58_capture.rdc` 仅经 CPU XML 审计证实有
UE/Nanite 场景 scope、89 pass 与 10 个 MRT pass；安全门控仍阻止 GPU
回放。没有新人工 UI 操作，旧 `frame1770` 黑屏失败仍在。
累计成功 UI QA 增量 **0**；见 [BATCH322](BATCH322_UE58_CPU_AUDIT_AND_COMPLETION_PROBE.md)。

2026-09-29 BATCH321 增量：`UE58_capture.rdc` 人工开启失败，终端定位 buffer
view 顺序和下一处 purgeable/GPU 完成阻塞；远程机 WindowServer watchdog
重启后停止 GPU/GUI 验证。累计成功 UI QA 增量 **0**；见
[BATCH321](BATCH321_UE58_CAPTURE_VIEW_ORDER_AND_WATCHDOG.md)。

2026-09-29 BATCH320 增量：用户已人工打开 UE `frame1770`，文件打开成功，
但无 UE scope 且 RT 黑色，**内容验收失败**。新版 viewer 的 scope 仅由
终端 API 证实；受控 viewport 按钮尚未人工点击，新版 UI 尚未验收。
累计成功 UI QA 增量仍为 **0**；见 [BATCH320](BATCH320.md)。

2026-09-29 BATCH319 增量：UE v0x10 `frame1770` 的有界 API/CLI 单次打开
通过；待人工检查 `Lvl_FirstPerson` PIE 画面、事件树、资源与回跳。
T319 帧内 Shared placement buffer 的 8/12→9/13→8/12→9/13 已由终端
GPU 读回验证，亦待 UI 抽看。当前库/app 内嵌库 SHA256 均为
`c9bdc6bea56256140df635a65c30d5b56531c78a233b24a23568a5a372acfd6a`。
累计 UI QA 增量 **0**；见 [BATCH319](BATCH319.md)。

2026-09-29 BATCH315–318 增量：T317/T318 BC1/BC1 sRGB/BC5 placement
纹理待人工 UI 核对资源身份、帧首状态和事件 seek。旧 v0xF UE
`frame5394` 安全拒绝，不能列为 viewer 通过。库/app 内嵌库 SHA256
`c13816cf6485ab945aa08ba476bc173613d5fbd609cb15518a6e65aabee49354`。
累计 UI QA 增量 **0**；见 [批次证据](BATCH315-318.md)。

2026-09-29 M7 增量：单次回放脚本的 T313 终端检查通过，未运行 GUI；
累计 UI QA 增量 **0**。见 [M7 记录](UE58_M7_RECAPTURE_GATE_2026-09-29.md)。

本单合并此前所有未验功能，供额度重置后由用户要求时集中检查。现在不用执行。
T34–T69、T71–T132、T135–T153、T156–T157、T159–T312 与 T10 marker 共274份 capture；同一个 qrenderdoc 进程即可，
不重复旧批已验功能。T70与旧版 T133 是预期回放拒绝负例，不打开。

2026-09-29 增量：T313/T314 新版 placement 复用截帧待人工 UI 验收；
终端已检查 GPU 字节及两轮事件回跳，累计 UI QA 增量 **0**。
当前可用 app 为 `/tmp/rdm-t312-build/bin/qrenderdoc.app`，
库与 app 内嵌库 SHA256 均为 `243043fb9dc1459afd3e1c2a6571dd7b880e1b76bf5b2122156a28a695fa1337`。
旧版 T133 不计入待验；详见 [BATCH313–314](BATCH313-314.md)。

## 固定版本与准备（只做一次）

仓库 `/Users/crossous/Developer/renderdoc-metal`，应用
`build-macos-debug/bin/qrenderdoc.app`；replay库与app内嵌库均为 `2f88058307f5…`，
GUI executable为 `fe8bcf852b68…`（新增argument buffer成员槽位映射，只编译、未启动）。
所有capture在 `captures/metal-smoke`。本单为最新版本入口，覆盖旧验收单里的库hash。

| 推荐顺序 | capture | SHA前缀 | 重点 |
| --- | --- | --- | --- |
| 1 | t34_capture.rdc | e6ee77deb50a | inline/batch buffers |
| 2 | t40_capture.rdc | 73a3398cec48 | compute inline/offset/threadgroup |
| 3 | t42_capture.rdc | 25c6e7349f22 | compute声明、barrier、三dispatch依赖 |
| 4 | t43_capture.rdc | 7139504e2357 | render声明、barrier、vertex写buffer |
| 5 | t44_capture.rdc | caa028676f68 | Fence跨Blit/Compute/Render同步 |
| 6 | t45_capture.rdc | 9e71f6b21883 | 只查atTime与buffer marker差异 |
| 7 | t46_capture.rdc | 7b1fbea45e1f | 只查minimum duration变体差异 |
| 8 | t38_capture.rdc | 92d869806241 | VS/FS/CS sampler LOD |
| 9 | t35_capture.rdc | 7fbed8c95df6 | command创建与两次compute |
| 10 | t39_capture.rdc | 0d552603e709 | Private buffer数据与seek |
| 11 | t36_capture.rdc | 436b3262fef5 | render动态状态、visibility/store |
| 12 | t37_capture.rdc | f87cbbc8ca00 | pitched blit、slice/mip |
| 13 | t41_capture.rdc | 35745dc5f7ab | blit descriptor、四种optimization |
| 14 | t10_debug_capture.rdc | 2d4a8d609bae | 只复验三条marker |
| 15 | t47_capture.rdc | a28eef0f401b | 同步pipeline options/reflection、compute descriptor |
| 16 | t48_capture.rdc | f71eaf76384b | binary library加载、无原始MSL、shader反射与依赖 |
| 17 | t49_capture.rdc | 82771996ea03 | 回调注册身份、callback前后CPU参数与dispatch回退 |
| 18 | t50_capture.rdc | f56406953e8d | CPU纹理读取元数据、Managed同步、参数回退 |
| 19 | t51_capture.rdc | e0fc940f6ce0 | 异步创建初始化、五pipeline、快照与左右半屏 |
| 20 | t52_capture.rdc | e0116d679d5f | Event身份、六signal/wait、三提交与事件回退 |
| 21 | t53_capture.rdc | 6ad98408529a | GPU ICB reset/copy/optimize、空命令、四阶段与初值回退 |
| 22 | t54_capture.rdc | 7fa91d16ce14 | indexed短重载、两实例与bindings偏移 |
| 23 | t55_capture.rdc | 6be475171749 | 四种Function创建、intersection函数链接、特化身份/数据与回退 |
| 24 | t56_capture.rdc | 9cb0d420ac9c | argument texture/sampler数组、非零id、两资源/反射/回退 |
| 25 | t57_capture.rdc | c852c41b973c | argument buffer成员/constants、两packet与两提交数据/画面回退 |
| 26 | t58_capture.rdc | c88c79833879 | 三种view、父子资源、subset/swizzle、三阶段seek |
| 27 | t59_capture.rdc | 0a9c40b8edef | buffer-backed纹理、父buffer/子texture、初值/CPU/GPU写回退 |
| 28 | t60_capture.rdc | 847db07d39f5 | Device argument encoder、两个texture/sampler packet与回退 |
| 29 | t61_capture.rdc | adad0c224323 | no-copy buffer、offset256、三提交CPU写入与回退 |
| 30 | t62_capture.rdc | 88d2fbc35494 | buffer/texture purgeable 状态、同一draw的资源与像素 |
| 31 | t63_capture.rdc | 9d88a151f271 | 四种动态顶点stride绑定、单视图amplification、四阶段seek |
| 32 | t64_capture.rdc | 10400b736950 | descriptor-backed Private shared texture、三次GPU clear/sample |
| 33 | t65_capture.rdc | 1b78708a1d3e | SharedEvent初值100、跨两队列、两signal/三wait、两draw回退 |
| 34 | t66_capture.rdc | 29473c46466a | factor buffer、scale 1、一个直接patch draw与中心像素 |
| 35 | t67_capture.rdc | 93f02f45b77b | 三种patch draw重载，红绿蓝与回退 |
| 36 | t68_capture.rdc | 05ea59ff1f1f | shared texture handle同进程导出/导入，源写入与导入采样 |
| 37 | t69_capture.rdc | 3ac0d738542d | SharedEvent handle同进程导入、跨队列共用时间线与seek |
| 38 | t71_capture.rdc | 5d4f73a88a97 | Heap父子buffer、Private GPU数据、draw与seek，比较T39 |
| 39 | t72_capture.rdc | 958442028c01 | Heap父子纹理、三次GPU clear/sample与seek，比较T64 |
| 40 | t73_capture.rdc | 2bff846250a6 | 四种Render useHeap(s)声明、同Heap身份、三draw像素与seek，比较T72 |
| 41 | t74_capture.rdc | 921596de7c36 | Tile pipeline、六种buffer绑定、三条dispatch计数和后续draw，验证seek |
| 42 | t75_capture.rdc | 647f1bb524cb | Tile纹理/采样器六重载，纹理采样使GPU计数翻倍，验证seek |
| 43 | t76_capture.rdc | 1ca3f463afa1 | Tile动态threadgroup内存含offset16和清除、三条dispatch，验证seek |
| 44 | t77_capture.rdc | 0470cd9f7ecf | 异步Tile pipeline创建、descriptor快照、三条dispatch与buffer/draw回退 |
| 45 | t78_capture.rdc | 160f902aef23 | 同步Mesh pipeline、一次MeshDispatch、三角形中心与背景像素 |
| 46 | t79_capture.rdc | 28f4ba76c665 | 四种mesh buffer/bytes绑定、三次MeshDispatch左/中/右三角形与seek |
| 47 | t80_capture.rdc | 28d188275e44 | 六种mesh texture/sampler绑定、纹理采样使三角形各右移约70像素 |
| 48 | t81_capture.rdc | e6126828325b | 一次直接drawMeshThreads，32×1×1线程网格，MeshDispatch与中心像素 |
| 49 | t82_capture.rdc | 5c89deaf9996 | 异步Mesh pipeline、调用时BGRA8/function快照、一次MeshDispatch |
| 50 | t83_capture.rdc | e7e52fa73e7d | Object/Mesh三函数pipeline、object buffer驱动payload、一次MeshDispatch与中心像素 |
| 51 | t84_capture.rdc | d29d5e86c1a9 | Object buffer/bytes四重载、三次payload驱动三角形与seek |
| 52 | t85_capture.rdc | e3bf1eee74e6 | Object texture/sampler六重载、纹理采样经payload平移三角形与seek |
| 53 | t86_capture.rdc | 6b70534fc597 | Object threadgroup memory 16/32/48-byte、scratch驱动payload与三次seek |
| 54 | t87_capture.rdc | f6b5c2a71ac9 | 异步Object/Mesh pipeline、调用时BGRA8/object函数快照与中心像素 |
| 55 | t88_capture.rdc | 81a4e8f91479 | Shared间接mesh参数offset16、一次MeshDispatch与绿色三角形 |
| 56 | t89_capture.rdc | 67f3eb8a6005 | GPU compute写Private间接参数、随后mesh绘制绿色三角形 |
| 57 | t90_capture.rdc | 8ca736e3c39f | Object/Mesh间接绘制、payload驱动紫红三角形 |
| 58 | t91_capture.rdc | 3ea72dbf14f6 | Object四线程的直接Mesh threadgroups draw |
| 59 | t92_capture.rdc | e18b6eb53bb5 | Object四线程的直接Mesh threads-grid draw，网格4×1×1 |
| 60 | t93_capture.rdc | 0b4476799c01 | Object两组的直接Mesh threadgroups draw，网格2×1×1 |
| 61 | t94_capture.rdc | 28f1ccb2592a | Object输入线程网格64×1×1，16个Object组输出Mesh |
| 62 | t95_capture.rdc | b4aa8a2f99ef | 单层all-1 Rate Map创建/参数复制/pass资源引用，绿色三角形 |
| 63 | t96_capture.rdc | 9bfcb4bddea0 | 半水平速率Rate Map，物理宽度208、绿色三角形收缩 |
| 64 | t97_capture.rdc | 1cab41975a47 | 双层Rate Map各自采样数组及参数数据；pass不绑定map |
| 65 | t98_capture.rdc | 1e6e084fd2fe | 双层Rate Map绑定2-slice array target；mesh slice0、blit到drawable |
| 66 | t99_capture.rdc | 161abbbbaeb2 | 两mesh组写array slices0/1；slice1半速率、blit到drawable |
| 67 | t100_capture.rdc | 6997b8195353 | 异步mesh grid2快照，两层map，slice1回拷 |
| 68 | t101_capture.rdc | 394b6a8f960b | VS/FS四阶段timestamp采样、resolve Copy到buffer |
| 69 | t102_capture.rdc | b9df0c466069 | reflection binding argument encoder、两个texture/sampler packet |
| 70 | t103_capture.rdc | a38e39c760c4 | Counter sample indices2–5、resolve range2–5到buffer offset16 |
| 71 | t104_capture.rdc | 22147d810218 | 单动态库→可执行library依赖，compute buffer值3，draw中心红色 |
| 72 | t105_capture.rdc | 7520798eaddd | 双动态库→可执行library有序依赖，compute buffer值6，draw中心红色 |
| 73 | t106_capture.rdc | 86823865a8b3 | compute pipeline descriptor预加载动态库，buffer值3与红色draw |
| 74 | t107_capture.rdc | b67564bf9344 | render fragment pipeline预加载动态库，shader调用动态函数 |
| 75 | t108_capture.rdc | 07b7eef99a4f | render vertex pipeline预加载动态库，shader调用动态函数 |
| 76 | t109_capture.rdc | 5c2186bd7f02 | render pipeline options/reflection的fragment预加载动态库 |
| 77 | t110_capture.rdc | 791e88f7e8e6 | 异步编译dynamic source，后续动态库和linked compute/draw |
| 78 | t111_capture.rdc | 3a9dcd1eccd8 | 异步编译executable source，带动态依赖和linked compute/draw |
| 79 | t112_capture.rdc | 26f406abdcec | 异步render fragment预加载动态函数与GPU红色draw |
| 80 | t113_capture.rdc | 123dfe302042 | 异步render options/reflection的fragment预加载 |
| 81 | t114_capture.rdc | 91400f1a0218 | 异步compute descriptor预加载动态库与buffer3 |
| 82 | t115_capture.rdc | 1ebdda69de37 | 异步render vertex预加载动态函数与GPU红色draw |
| 83 | t116_capture.rdc | 9ab0c4160699 | Placement heap中显式offset768的Private buffer，GPU数据与画面 |
| 84 | t117_capture.rdc | 3051cc3039d7 | Placement heap中显式offset128的Private纹理，三阶段clear/sample |
| 85 | t118_capture.rdc | e3fc631efe28 | 一层嵌套argument buffer，子texture/sampler四象限采样 |
| 86 | t119_capture.rdc | bbf65de21056 | fragment pipeline 直接链接visible函数，PSO依赖关系 |
| 87 | t120_capture.rdc | 4e27d1884a94 | fragment handle/table单槽绑定，GPU红色绘制 |
| 88 | t121_capture.rdc | 0d9fa4321400 | fragment表范围绑定与函数范围更新 |
| 89 | t122_capture.rdc | 8f1561df2b59 | vertex visible函数表单槽绑定，GPU绿色绘制 |
| 90 | t123_capture.rdc | 3d670a81bb2d | vertex表范围绑定与事件回退 |
| 91 | t124_capture.rdc | ccbff8491538 | compute visible函数表单槽绑定，516-byte输出 |
| 92 | t125_capture.rdc | b07cf63d6c69 | compute表范围绑定，516-byte输出 |
| 93 | t126_capture.rdc | 3c47723d8b24 | argument buffer可见函数表单槽编码、compute间接调用 |
| 94 | t127_capture.rdc | 0200b2db3881 | argument buffer表数组编码、compute间接调用 |
| 95 | t128_capture.rdc | d8593f07a9e0 | tile函数表单槽绑定，三次dispatch增量+1 |
| 96 | t129_capture.rdc | 96b50761c01b | tile表范围绑定，三次dispatch及最终画面 |
| 97 | t130_capture.rdc | e2704199df7c | 异步tile pipeline快照、linked函数及表、三次dispatch |
| 98 | t131_capture.rdc | 49579bc8afd7 | Heap buffer `makeAliasable`，资源父子关系；无重叠复用 |
| 99 | t132_capture.rdc | 686a80808ce7 | Heap texture `makeAliasable`，资源父子关系；无重叠复用 |
| 100 | t135_capture.rdc | 0acbe500d4aa | AS单三角形build、GPU压缩尺寸写回、事件seek；T134查询无独立UI项 |
| 101 | t136_capture.rdc | 8348a71bee6b | AS UInt16索引三角形build、索引资源、GPU尺寸与seek |
| 102 | t137_capture.rdc | d3e4b90600db | AS bounding box build、box资源、GPU尺寸与seek |
| 103 | t138_capture.rdc | e10385ed0664 | `newAccelerationStructureWithDescriptor:`分配、同T135 build与seek |
| 104 | t139_capture.rdc | 18bdace6e72a | 等容量AS build→copy→write、两个AS资源与事件seek |
| 105 | t140_capture.rdc | ea999aadd79c | GPU尺寸决定1280-byte目标的压缩copy、两个CB与seek |
| 106 | t141_capture.rdc | 0ca68274fcea | refittable AS build→GPU blit更新顶点→原位refit，compute ray命中→未命中 |
| 107 | t142_capture.rdc | 54d563c23e9b | 单实例TLAS引用BLAS，x=2变换后两条ray分别miss/hit |
| 108 | t143_capture.rdc | 0a41991b814b | 同BLAS的x=-2/+2两个实例，三条ray依次hit/miss/hit |
| 109 | t144_capture.rdc | 3afa4d1f0c9a | 不同BLAS的x=-2/+2两个实例；索引0/1，x=-2/+2/+3三条ray依次hit/miss/hit |
| 110 | t145_capture.rdc | 78a3802c5057 | Fragment slot0绑定TLAS，射线命中第二BLAS、中央绿色像素及Shared值1 |
| 111 | t146_capture.rdc | abeaf2f81566 | Vertex slot0绑定TLAS，射线命中第二BLAS、中央绿色像素及Shared值1 |
| 112 | t147_capture.rdc | 10073db853eb | Tile slot0绑定TLAS，tile dispatch射线命中第二BLAS、Shared值1 |
| 113 | t148_capture.rdc | 1843e61a594d | Fragment slot2绑定intersection table，修正交点函数签名后Shared值0、中央红色 |
| 114 | t149_capture.rdc | 91a1118b4d74 | Vertex slot2绑定intersection table，Shared值0、中央红色 |
| 115 | t150_capture.rdc | c500b52ab6d4 | Tile slot2绑定intersection table，tile dispatch写Shared值0 |
| 116 | t151_capture.rdc | 7531603f7cfa | Fragment range `(2,1)`绑定表，Shared值0、中央红色 |
| 117 | t152_capture.rdc | 937d11fab706 | Vertex range `(2,1)`绑定表，Shared值0、中央红色 |
| 118 | t153_capture.rdc | 6673610202f8 | Tile range `(2,1)`绑定表，tile dispatch写Shared值0 |
| 119 | t156_capture.rdc | 96ef002c3c03 | Fragment表slot0 opaque-triangle单槽更新，绑定fragment slot2后Shared值1、中央绿色 |
| 120 | t157_capture.rdc | 61dc07c81b38 | Fragment表opaque-triangle range `(0,1)`更新，绑定slot2后Shared值1、中央绿色 |
| 121 | t159_capture.rdc | c0ae4c26d4ca | Fragment IFT的buffer参数slot0值1，Shared值1、中央绿色 |
| 122 | t160_capture.rdc | dba038596c62 | Fragment IFT的buffer参数range `(0,1)`值1，Shared值1、中央绿色 |
| 123 | t161_capture.rdc | 997d1dcf918a | Fragment IFT嵌套visible table参数slot1、buffer参数slot0，Shared值1、中央绿色 |
| 124 | t162_capture.rdc | 6d9ff50d3ba4 | Fragment IFT嵌套visible table参数range `(1,1)`、buffer参数slot0，Shared值1、中央绿色 |
| 125 | t163_capture.rdc | b0c5269d639b | 同T161并在render pass显式`useResource(Read, Fragment)`声明嵌套表，Shared值1、中央绿色 |
| 126 | t164_capture.rdc | 581fdf7ca5f9 | 两个默认descriptor AS encoder分别构建BLAS/TLAS，compute Shared末`0,1` |
| 127 | t165_capture.rdc | ae98acfea13e | 第二BLAS为显式opaque三角形，自定义reject函数被跳过，render Shared值1、中央绿色 |
| 128 | t166_capture.rdc | a540260cce00 | 第二BLAS经opaque descriptor分配，再显式opaque build；render Shared值1、中央绿色 |
| 129 | t167_capture.rdc | 198c95ce8dbf | 第二BLAS为UInt16 indexed opaque triangle；自定义reject函数被跳过，render Shared值1、中央绿色 |
| 130 | t168_capture.rdc | fdba55218a1d | 第二BLAS为UInt32 indexed opaque triangle；与T167共享语义，render Shared值1、中央绿色 |
| 131 | t169_capture.rdc | ea73a885eaaa | 第二BLAS经UInt16 indexed opaque descriptor分配后build，render Shared值1、中央绿色 |
| 132 | t170_capture.rdc | d843c33ff562 | 第二BLAS为UInt16 indexed非opaque三角形，reject函数生效，render Shared值0、中央红色 |
| 133 | t171_capture.rdc | 7dadf0fbcb2b | 第二BLAS经UInt32 indexed opaque descriptor分配后build，render Shared值1、中央绿色 |
| 134 | t172_capture.rdc | e6052b8de72c | 第二BLAS为UInt32 indexed非opaque三角形，reject函数生效，render Shared值0、中央红色 |
| 135 | t173_capture.rdc | a52f95b7871e | 第二BLAS顶点buffer前16字节干扰数据，vertexOffset16仍命中Shared1、中央绿色 |
| 136 | t174_capture.rdc | ed91b6c2ad4b | 第二BLAS经descriptor分配、再从顶点buffer offset16构建，Shared1、中央绿色 |
| 137 | t175_capture.rdc | 22fd0ff433ef | 第二BLAS scratch buffer offset256 构建，Shared1、中央绿色 |
| 138 | t176_capture.rdc | 0d18e6a1d3cd | 第二BLAS同时 vertexOffset16 与 scratchOffset256，size分配、Shared1绿色 |
| 139 | t177_capture.rdc | f1a68a4f1bea | 第二BLAS同时 vertexOffset16 与 scratchOffset256，descriptor分配、Shared1绿色 |
| 140 | t178_capture.rdc | b8c37cb4c5a7 | box几何经descriptor分配AS，build与GPU compacted size1280 |
| 141 | t179_capture.rdc | c65cfc84faf9 | 同一box几何descriptor含两个box，build chunk boxCount2、GPU compacted size1280 |
| 142 | t180_capture.rdc | dcb164b33c4e | 同一box几何descriptor含三个box，build chunk boxCount3、GPU compacted size1280 |
| 143 | t181_capture.rdc | 1ffbca9bdc9b | 非indexed三角形descriptor含两个三角形，build chunk triangleCount2、GPU compacted size1280 |
| 144 | t182_capture.rdc | 014076f1de47 | indexed descriptor含两个三角形、六UInt16索引，build chunk triangleCount2、GPU compacted1280 |
| 145 | t183_capture.rdc | 821c608d220b | 计算+片元 AS 先绑定TLAS、执行ray、再设零资源清空；输出不变 |
| 146 | t184_capture.rdc | d270206e78a0 | 计算+顶点 AS 先绑定TLAS、执行ray、再设零资源清空；输出不变 |
| 147 | t185_capture.rdc | 4346f2a70218 | 计算+tile AS 先绑定TLAS、执行ray、再设零资源清空；输出不变 |
| 148 | t186_capture.rdc | 5e69883ab2a8 | 单实例 TLAS 经 instance descriptor 分配，size1792、随后 build 与 ray 结果不变 |
| 149 | t187_capture.rdc | dd2ac6c2e8b2 | 同一 BLAS 的两实例 TLAS 经 descriptor 分配，size1792、左右 ray 命中 |
| 150 | t188_capture.rdc | 4134493f0c66 | 两个不同 BLAS 的双实例 TLAS 经 descriptor 分配，size1792、左右 ray 命中 |
| 151 | t189_capture.rdc | 5d2641a26749 | box AS build→GPU compacted size1280→跨CB压缩复制→目标写回1280 |
| 152 | t190_capture.rdc | 249264392fe8 | 双非 indexed 三角形 descriptor AS 经相同压缩链，目标 size1280 |
| 153 | t191_capture.rdc | 8f65ddb5925d | UInt16 indexed 三角形 AS 经相同压缩链，目标 size1280 |
| 154 | t192_capture.rdc | efafce7ba185 | build→普通复制 BLAS→以复制目标构建 TLAS，GPU ray 仍为 origin0=0 / origin2=1 |
| 155 | t193_capture.rdc | 8cd542cb4230 | GPU size1280→跨CB压缩 BLAS→以压缩目标构建 TLAS，GPU ray 仍为 origin0=0 / origin2=1 |
| 156 | t194_capture.rdc | 9fb66545a41d | UInt16 indexed BLAS 压缩后作为 TLAS 子结构，GPU ray 为 origin0=0 / origin2=1 |
| 157 | t195_capture.rdc | 9b1251e08dd8 | 双非indexed三角形 BLAS 压缩后作为 TLAS 子结构，GPU ray 为 origin0=0 / origin2=1 |
| 158 | t196_capture.rdc | 76fd780faa53 | 单实例 TLAS build→普通copy→复制目标绑定compute ray，输出0/1 |
| 159 | t197_capture.rdc | 68954d273dd3 | 单实例 TLAS GPU size1536→跨CB压缩copy→压缩目标ray，输出0/1 |
| 160 | t198_capture.rdc | ca18e1a89989 | 同一BLAS双实例 TLAS 压缩后ray，输出1/0/1 |
| 161 | t199_capture.rdc | 6fa0a4e4e523 | 不同BLAS双实例 TLAS 压缩后ray，输出1/0/1 |
| 162 | t200_capture.rdc | 6433b65790d2 | refit 三角形 AS 后压缩copy，压缩目标 ray 从命中1变为未命中0 |
| 163 | t201_capture.rdc | f88dec560f6e | refit 双三角形 AS 后压缩copy，压缩目标 ray 从命中1变为未命中0 |
| 164 | t202_capture.rdc | 973fada30a67 | 单三角形 refit AS 直接descriptor分配，再refit/压缩/ray为1→0 |
| 165 | t203_capture.rdc | 9075fbb89809 | 双三角形 refit AS 直接descriptor分配，再refit/压缩/ray为1→0 |
| 166 | t204_capture.rdc | a482ea1c7ab3 | 原位 refit scratchOffset256，GPU ray 1→0 |
| 167 | t205_capture.rdc | 344835a4e0dd | refit 到独立 AS，目标 ray 1→0 |
| 168 | t206_capture.rdc | fd80bfc0a01f | 独立目标 refit 加 scratchOffset256，目标 ray 1→0 |
| 169 | t207_capture.rdc | c9c8c7e4f056 | 独立 refit 目标压缩复制，再 ray 1→0 |
| 170 | t208_capture.rdc | 02e8403ac813 | 独立 refit 目标使用512-byte紧凑scratch，再 ray 1→0 |
| 171 | t209_capture.rdc | 6e0720aede63 | box buffer offset48，前两盒为干扰数据；扩展build及GPU压缩容量1280 |
| 172 | t210_capture.rdc | b8a3683c2828 | box build scratchOffset256，扩展build及GPU压缩容量1280 |
| 173 | t211_capture.rdc | d985008c577b | box/scratch 双偏移组合；扩展build及GPU压缩容量1280 |
| 174 | t212_capture.rdc | 804b35b122cf | 偏移盒型descriptor直接分配，再扩展build；GPU压缩容量1280 |
| 175 | t213_capture.rdc | c448d192a774 | box stride32 单盒build，GPU压缩容量1280 |
| 176 | t214_capture.rdc | 0a4be5cb25a4 | descriptor分配后以stride32构建双盒，GPU压缩容量1280 |
| 177 | t215_capture.rdc | 30ddba848395 | 双盒stride32加boxOffset48/scratchOffset256，GPU压缩容量1280 |
| 178 | t216_capture.rdc | 371207066173 | URL加载动态库；原路径删除后仍链接compute3与红色draw |
| 179 | t217_capture.rdc | d93d2c7e4a8f | URL库加普通动态库双依赖；compute6与红色draw |
| 180 | t218_capture.rdc | ca4ec2cfe01a | 双盒stride32；compute交点表单槽绑定，射线1/1/0 |
| 181 | t219_capture.rdc | 3afdf3e0be8c | 双盒加boxOffset48/scratchOffset256；射线1/1/0 |
| 182 | t220_capture.rdc | 4aa223e6b279 | 双盒stride32；compute交点表range绑定，射线1/1/0 |
| 183 | t221_capture.rdc | fc1ad4daaf88 | 双盒stride32；box几何tableOffset1，射线1/1/0 |
| 184 | t222_capture.rdc | 84dee8cfbef0 | tableOffset1加boxOffset48/scratchOffset256，射线1/1/0 |
| 185 | t223_capture.rdc | 678fd2ae45a1 | opaque box几何，射线0/0/0 |
| 186 | t224_capture.rdc | b66c7ca05986 | opaque加tableOffset1、boxOffset48/scratchOffset256，射线0/0/0 |
| 187 | t225_capture.rdc | 11b4a39d7352 | indexed UInt16三角形indexOffset8，ray 1/0/1与render ray 1 |
| 188 | t226_capture.rdc | 2019936bbe7f | descriptor分配的indexed UInt32三角形indexOffset8，ray 1/0/1与render ray 1 |
| 189 | t227_capture.rdc | 1d861889320a | indexed UInt16 三偏移48/8/256，ray 1/0/1与render ray 1 |
| 190 | t228_capture.rdc | 6305ce8ae637 | indexed UInt32 三偏移+descriptor分配，ray 1/0/1与render ray 1 |
| 191 | t229_capture.rdc | 306a61d47e18 | indexed UInt16 仅vertexOffset48，ray 1/0/1与render ray 1 |
| 192 | t230_capture.rdc | d4882f84b346 | indexed UInt16 仅scratchOffset256，ray 1/0/1与render ray 1 |
| 193 | t231_capture.rdc | 66c70d9ffdd5 | 非 indexed triangle 表偏移1，render ray 1 |
| 194 | t232_capture.rdc | ae2948bfc7ab | indexed UInt16 triangle 表偏移1，render ray 1 |
| 195 | t233_capture.rdc | f99aaf786a1d | indexed UInt32 表偏移1+三偏移+descriptor分配，render ray 1 |
| 196 | t234_capture.rdc | 016b90163910 | box 禁止重复交点调用，GPU ray 1/1/0、回调数2 |
| 197 | t235_capture.rdc | e9abb40b380c | 禁止重复加 box/scratch/table 偏移，GPU ray 1/1/0、回调数2 |
| 198 | t236_capture.rdc | 9901236e8a09 | opaque 加禁止重复，GPU ray 0/0/0、回调数2 |
| 199 | t237_capture.rdc | 39c04471ce92 | 非 indexed triangle 禁止重复交点调用，表槽1、render ray 1 |
| 200 | t238_capture.rdc | 586f10a6e265 | UInt16 indexed triangle 禁止重复交点调用，表槽1、render ray 1 |
| 201 | t239_capture.rdc | 6aebf01c4a99 | UInt32 indexed 加三偏移和 descriptor，禁止重复交点调用、render ray 1 |
| 202 | t240_capture.rdc | 57ca8102c0f3 | 禁止重复的原位 refit，ray 1→0 |
| 203 | t241_capture.rdc | 1875abd63868 | 禁止重复加 descriptor 分配、独立目标 refit，ray 1→0 |
| 204 | t242_capture.rdc | c3aab50c2212 | 禁止重复加 scratchOffset256 原位 refit，ray 1→0 |
| 205 | t243_capture.rdc | 53698f1b9e5a | 禁止重复 refit 后压缩复制，ray 1→0 |
| 206 | t244_capture.rdc | 74415587bcf2 | Float3/stride16 的非 indexed triangle，ray 1,0,1 |
| 207 | t245_capture.rdc | 7e775e034509 | Float4/stride16 的非 indexed triangle，ray 1,0,1 |
| 208 | t246_capture.rdc | 09292d535f79 | Float3/stride16，vertexOffset16/scratchOffset256，ray 1,0,1 |
| 209 | t247_capture.rdc | ce712cfa0d7f | Float4/stride16，禁止重复交点调用，ray 1,0,1 |
| 210 | t248_capture.rdc | 11ff5c001b2d | indexed Float4/stride16、UInt16，ray 1,0,1 |
| 211 | t249_capture.rdc | af195241374c | indexed Float4/stride16、UInt32、三偏移16/8/256和descriptor分配，ray 1,0,1 |
| 212 | t250_capture.rdc | c956c89a3989 | indexed Float3/stride16、UInt16、禁止重复交点调用，ray 1,0,1 |
| 213 | t251_capture.rdc | 14c1cd5b07a3 | Float3/stride16 原位 refit，ray 1→0 |
| 214 | t252_capture.rdc | 806c279a77dd | Float4/stride16 原位 refit，ray 1→0 |
| 215 | t253_capture.rdc | 952a939e3f1b | Float4 descriptor 分配加独立目标 refit，ray 1→0 |
| 216 | t254_capture.rdc | 17017c2fc35f | Float3 禁止重复交点调用加 scratchOffset256 refit，ray 1→0 |
| 217 | t255_capture.rdc | a5b5b33feb6d | Float4 refit 后压缩复制，ray 1→0 |
| 218 | t256_capture.rdc | 1c0ce0b6ad6f | 基本 box AS 原位 refit，射线 1/1/0→0/1/0 |
| 219 | t257_capture.rdc | c1061c97ba04 | boxOffset48、scratchOffset256、表槽1和禁止重复调用的 refit |
| 220 | t258_capture.rdc | 2b7628bba8b | box AS 独立目标 refit，核对源/目标身份和 seek |
| 221 | t259_capture.rdc | aaaf47ca1353 | box AS refit 后复制并射线查询，核对 copy 资源身份和 seek |
| 222 | t260_capture.rdc | 93026056f190 | copied box AS 再次 refit，四次射线 1/1/0→0/1/0→0/1/0→1/1/0 |
| 223 | t261_capture.rdc | 403be1965785 | box refit 改用另一块 box buffer，核对两个资源 ID 与射线 |
| 224 | t262_capture.rdc | 2994416908dc | 默认 triangle refit 改用另一块顶点 buffer |
| 225 | t263_capture.rdc | eeeb2695c029 | Float4/stride16 refit 改用另一块顶点 buffer |
| 226 | t264_capture.rdc | d1bd3761dfa6 | noDuplicate triangle refit 改用另一块顶点 buffer |
| 227 | t265_capture.rdc | 549b1eb65aae | 独立目标 triangle refit 改用另一块顶点 buffer |
| 228 | t266_capture.rdc | 7aecbaaa6b8c | UInt16 indexed triangle refit，build/refit 事件与 ray 1→0 |
| 229 | t267_capture.rdc | 25f25b5c78fc | UInt32 indexed triangle refit，核对 indexType 与 seek |
| 230 | t268_capture.rdc | 927906e9703c | indexed refit 改用另一块 index buffer，核对资源身份 |
| 231 | t269_capture.rdc | 3d93bf83cc3d | indexed refit 独立目标，核对源/目标身份 |
| 232 | t270_capture.rdc | 6d163a9977b2 | indexed refit 后压缩复制，核对 size/copy/action 顺序 |
| 233 | t271_capture.rdc | 9926b78a401c | descriptor 分配的 indexed refit，核对分配与 build/refit |
| 234 | t272_capture.rdc | 857a4c7e0812 | indexed refit Float4/stride16 |
| 235 | t273_capture.rdc | dfc930805924 | indexed refit `allowDuplicate=false` |
| 236 | t274_capture.rdc | 2ef97c52fddd | indexed refit scratchOffset256 |
| 237 | t275_capture.rdc | 8b967ef5513e | indexed refit indexBufferOffset2，跳过无效前缀 |
| 238 | t276_capture.rdc | fbfb4c5ff7a8 | indexed refit vertexBufferOffset36，跳过干扰三角形 |
| 239 | t277_capture.rdc | 14a025309ff6 | descriptor 分配加 vertexBufferOffset36 |
| 240 | t278_capture.rdc | dae4dcfeeaad | indexed refit tableOffset1 |
| 241 | t279_capture.rdc | 3874008ea63c | indexed refit opaque 属性切换 |
| 242 | t280_capture.rdc | 526d56c9a07a | descriptor/noDuplicate/scratch/index/vertex/table/opaque 组合 |
| 243 | t281_capture.rdc | a7c620268d97 | UInt32 indexed refit 的 indexOffset8 |
| 244 | t282_capture.rdc | 900a0ca02260 | Float4 indexed refit 的 vertexOffset16 |
| 245 | t283_capture.rdc | 896f8016e693 | UInt32/Float4 双偏移加 descriptor 分配 |
| 246 | t284_capture.rdc | 1454f5afe3a9 | 双三角形 UInt16 indexed refit |
| 247 | t285_capture.rdc | 4e99f77f3fbb | 双三角形 UInt32 indexed refit |
| 248 | t286_capture.rdc | c6d921bba4a3 | 双三角形 refit 独立目标后压缩复制 |
| 249 | t287_capture.rdc | 92b7fe152a89 | 双三角形 refit 换索引 buffer |
| 250 | t288_capture.rdc | b7e470480231 | 双三角形 refit 换顶点 buffer |
| 251 | t289_capture.rdc | ea820685ac3f | 双三角形 refit 同时换顶点和索引 buffer |
| 252 | t290_capture.rdc | abff92866814 | 同源 BLAS 三实例 TLAS，三次射线命中 |
| 253 | t291_capture.rdc | 3a2a5078c0e9 | 同源 BLAS 四实例 TLAS，四次射线命中 |
| 254 | t292_capture.rdc | dbf056a75a44 | 三实例 TLAS 按 descriptor 分配 |
| 255 | t293_capture.rdc | 11c90bf3dad0 | 四实例 TLAS 压缩复制后射线 |
| 256 | t294_capture.rdc | cd5fe6dca0a5 | 三个不同 BLAS 的 TLAS，三次射线命中 |
| 257 | t295_capture.rdc | 9b90bc5d9f36 | 四个不同 BLAS 的 TLAS，四次射线命中 |
| 258 | t296_capture.rdc | 861b6c858532 | 三个不同 BLAS，按 descriptor 分配 TLAS |
| 259 | t297_capture.rdc | d5518fdcc044 | 四个不同 BLAS，TLAS 压缩复制后射线 |
| 260 | t298_capture.rdc | 00ed80a5d7cc | 同一 BLAS 八实例 TLAS，八次射线命中 |
| 261 | t299_capture.rdc | 6f84e5d6111d | 三实例复用两个 BLAS，三次射线命中 |
| 262 | t300_capture.rdc | 60ae66ccaa14 | 五实例复用两个 BLAS，五次射线命中 |
| 263 | t301_capture.rdc | 2780f4d81854 | Binary Archive资源；compute/render pipeline共用依赖，32个`i+17`值、中心像素约`(0.2,0.7,0.3)` |
| 264 | t302_capture.rdc | b93bf5da8a68 | Binary Archive异步compute/render；对照T301的资源、输出与事件seek |
| 265 | t303_capture.rdc | a4df65b1fb4b | 单节点Stitched Library资源链、32个float值2…64及中心像素 |
| 266 | t304_capture.rdc | 3602ebde29d3 | 异步Stitched Library chunk1377；与T303对照资源链及GPU输出 |
| 267 | t305_capture.rdc | 12b0c5a7ce8e | 空 Binary Archive；compute/render 函数变更 chunk1378/1379 |
| 268 | t306_capture.rdc | cf5b600b3e24 | 空 Binary Archive；异步 pipeline 创建对照 T305 |
| 269 | t307_capture.rdc | 348f4169bb2b | Binary Archive 添加 visible 函数 chunk1380；来源库资源关系 |
| 270 | t308_capture.rdc | 15e67b9c276c | Binary Archive 添加单节点 stitched library chunk1381 |
| 271 | t309_capture.rdc | 32025e6fed64 | Binary Archive 添加 tile 函数 chunk1382；GPU tile 三阶段对照 |
| 272 | t310_capture.rdc | ee79cc878d8b | Binary Archive 添加 mesh 函数 chunk1383；GPU 间接 mesh draw 对照 |
| 273 | t311_capture.rdc | 96acccaa0e49 | tile pipeline 使用 Binary Archive，options4；对照 T309 |
| 274 | t312_capture.rdc | 2beb183fccd6 | mesh pipeline 使用 Binary Archive，options4；对照 T310 |

T301/T302 在集中 QA 时各打开一次并对照：确认 archive 资源 ID 可从创建 chunk
跳转，compute/render descriptor 的 `binaryArchives` 各含同一个 ID；在
dispatch 后读 128 字节输出 buffer，32 个 uint 应为 17–48；末次 draw
中心像素约为 RGB `(0.2,0.7,0.3)`。再 seek 到 dispatch 前、后、末次
draw，确认前后状态不会串帧。普通 compute/render archive 变更另见 T305/T306。

T305/T306 同次对照：空 archive 创建后，chunk1378/1379 依次加入 compute、
render 函数；archive 资源关系包括三个源函数，两个 pipeline 都引用同一 archive，
options=4。dispatch 后仍是32个 uint 17–48，中心像素约 RGB `(0.2,0.7,0.3)`；
T306 是异步 pipeline 创建。前后 seek，确认未串帧。

T307 额外检查 chunk1380 的 `archive_visible` 来源库和 archive 资源关系，
该 chunk 应早于两条 pipeline 创建；普通 compute/render 输出仍同 T305。
这里不要求 visible 函数经 GPU 函数表执行。

T308 额外检查 chunk1381 的源 visible 函数、单节点 graph 与 archive 资源关系；
普通 compute/render 输出仍同 T305。stitched 函数的 GPU 函数表执行另见 T303/T304。

T309 检查 chunk1382 源 tile kernel→archive 资源关系；tile 三阶段 dispatch
输出和 seek 同 T74，普通 draw pipeline 的 archive 依赖与 options4 应保留。
tile pipeline 本身的 archive 命中不列为已支持项。

T310 检查 chunk1383 的 mesh/fragment 函数→archive 资源关系；GPU 间接
mesh draw、像素与 seek 同 T89，执行中的 writer compute pipeline 引用
archive 且 options4。同步 mesh pipeline 的 archive 命中另见 T312。

T311/T312 分别对照 T309/T310：tile/mesh pipeline 新增的 `binaryArchives`
字段各引用同一 archive，options=4；资源图有 archive→pipeline。既有
tile 三阶段与 mesh 间接 draw 的 GPU 数据、像素和正反向 seek 保持一致。

T303/T304 同次对照：同步chunk1020与异步chunk1377均应显示
`function=stitch_scale`、`graphName=stitched_scale`；源函数→stitched
library→visible函数→compute pipeline 的资源跳转完整。dispatch 后128字节
buffer为32个float `2,4,…,64`，中心像素约RGB `(0.2,0.7,0.3)`；
seek 到dispatch前后核对状态。复杂graph尚未支持，不列入本次人工验收。

打开Event Browser、API Inspector、Pipeline State、Texture/Buffer Viewer，清空事件筛选。
此前已修复display参数60/64-byte布局差异，由Metal验证层自动覆盖；UI公共画面检查即可，
不增加重复capture或额外一轮人工流程。
通用检查只需记一份：资源链接可打开、前后选事件画面/数据跟随、换capture不崩溃、状态栏。
出现异常只记录 `T编号 + 调用名/EID + 实际值或截图`，不用重跑整套。EID以新版capture为准。

## 本次新增的最小差异项

### T299–T300 多实例复用两个 BLAS

API Inspector核对`buildRepeatedDistinctInstances`携带两个不同BLAS，
count为3/5；实例描述符索引按0/1重复引用。事件树分别有三/五次
compute ray dispatch；输出槽随seek从`7/9/11(/13/15)`依次变为1，
反向seek恢复即可。

### T298 同一 BLAS 的八实例 TLAS

API Inspector 核对 `buildInstances.count=8`，八个实例共用一个 BLAS；
事件树有八次 compute ray dispatch。Buffer Viewer 随事件 seek，输出
八槽从 `7/9/11/13/15/17/19/21` 逐个变为1，反向 seek 恢复。
65536是实现的有界上限，不作为本次人工 GPU 压测要求。

### T294–T297 三/四个不同 BLAS 的 TLAS

API Inspector 核对 `buildMultipleDistinctInstances.children` 是按序排列的
三个/四个不同 BLAS，实例描述符的 AS 索引依次为0、1、2（、3）。事件树
对应三/四个 BLAS build、一个 TLAS build 和三/四次 compute ray dispatch。
T296 核对 TLAS descriptor 分配；T297 核对 GPU compact size 写入与
压缩复制。随事件 seek，输出槽从 `7/9/11(/13)` 依次变为1；资源跳转和
反向 seek 正常即可。

### T290–T293 同一 BLAS 的三/四实例 TLAS

API Inspector 核对 `buildInstances.count` 为 3/4，实例描述符均引用同一
BLAS；事件树应有对应的 TLAS build 和 3/4 次 compute ray dispatch。
T292 核对 descriptor 分配，T293 核对 compact size 写入及压缩复制。
随事件 seek，三/四个输出槽从 `7/9/11(/13)` 依次变为 1；
资源跳转与反向 seek 正常即可。

### T284–T289 双三角形 indexed refit

API Inspector 核对 `triangleCount=2`、T284 UInt16/T285 UInt32 六索引；
T286 的 refit 目标不同于源且后续有压缩复制；T287/T288/T289 分别
核对换索引、换顶点、同时换两块 buffer 的资源 ID。六份均检查
GPU ray 随 seek 从 `1,9` 到 `1,0`；不要求 AS 内部几何可视化。

### T281–T283 UInt32/Float4 indexed refit 偏移

API Inspector 核对 T281 的 UInt32 indexOffset8、T282 的 Float4/stride16
vertexOffset16、T283 的两者合并与 descriptor 分配；build/refit 参数应
一致。GPU ray 随 seek 从 `1,9` 到 `1,0`，资源链接可跳转即可。

### T272–T280 indexed triangle refit 参数

API Inspector 对照 `buildRefittableIndexedTriangle` 与 `refitIndexedTriangle`：
T272 Float4/stride16，T273 `allowDuplicate=false`，T274 仅 refit 的
scratchOffset256，T275 indexOffset2，T276 vertexOffset36，T277 同时按
descriptor 分配，T278 tableOffset1，T279 opaque 切换，T280 将除 Float4
之外的七参数合并。除 scratchOffset 外，几何属性在 build/refit 应一致。
九份均只需核对 GPU ray 随 seek
从 `1,9` 到 `1,0`，及相关资源链接；不要求内部 AS 几何可视化。

### T251–T255 格式化 triangle refit

API Inspector 核对 `buildRefittableFormattedTriangle` 和
`refitFormattedTriangle` 的 stride16、Float3/Float4、源/目标资源身份。
T253 为 descriptor 分配加独立目标，T254 为 `allowDuplicate=false` 和
scratchOffset256，T255 在 refit 后还有 compacted size 写入/压缩复制。
五份均检查 GPU ray 随 seek 为 `1,9`→`1,0`，不要求内部几何可视化。

### T248–T250 indexed triangle 顶点格式与步长

API Inspector 核对 `buildIndexedFormattedTriangle`：T248 为 Float4/UInt16，
T249 为 Float4/UInt32 且顶点/索引/scratch 偏移 `16/8/256`，AS 由
descriptor 分配；T250 为 Float3/UInt16 且 `allowDuplicate=false`。
三份检查 AS→TLAS 资源链接、compute ray `1,0,1` 与事件 seek。

### T244–T247 triangle 顶点格式与步长

API Inspector 核对 `buildFormattedTriangle` 的 `vertexStride=16`；T244/246
为 Float3，T245/247 为 Float4；T246 核对 vertexOffset16/scratchOffset256，
T247 核对 `allowDuplicate=false`。四份均看 AS→TLAS 资源链接、compute ray
`1,0,1` 和事件 seek；不要求内部几何可视化。

### T240–T243 禁止重复的 triangle refit

API Inspector 核对 build 与 refit 都是新 no-duplicate chunk，T241 为
descriptor 分配及不同目标，T242 `scratchOffset=256`，T243 有 compacted
size 写入及 copy-and-compact。GPU 输出随事件 seek 为 `1,0`→`1,9`→
`1,0`；检查 AS 源/目标资源身份，不要求内部几何可视化。

### T237–T239 triangle 交点函数禁止重复调用

API Inspector 看 `buildTriangleNoDuplicate`，核对 T237 无索引，T238
UInt16 索引，T239 UInt32 索引及顶点/索引/scratch 偏移 `48/8/256`、
BLAS descriptor 分配；三份表槽1、compute ray `1,0,1`、render ray `1`。
检查资源链接与事件 seek，不把相同射线结果解释为去重上限证明。

### T234–T236 box 交点函数禁止重复调用

API Inspector 核对 `buildBoundingBoxNoDuplicate` 的字段：T234 基本双盒，
T235 的 box/scratch/table 偏移为 `48/256/1`，T236 为 opaque。
三份 dispatch 后 GPU callback buffer 应为 2，ray 分别为 `1,1,0`、
`1,1,0`、`0,0,0`；看 blit 清零→build→dispatch 的事件顺序、
资源身份和 seek。
这个固定场景无法证明允许/禁止重复的行为差异，不把计数2误当充分证明。

### T231–T233 triangle 交点函数表偏移

API Inspector 确认 `buildTriangleTableOffset.tableOffset=1`，交点函数表
count2、函数写槽1；T231 不带索引、T232 带 UInt16 索引，T233 为
UInt32 且 vertex/index/scratch 偏移 `48/8/256`、BLAS 经 descriptor 分配。
三份 compute ray 为 `1,0,1`，render ray 为 `1`。错槽导致 `0` 的对照
已经终端验证，GUI 只检查正例的资源链接、事件顺序和 seek。

### T227–T230 indexed triangle 偏移组合

四份均检查 `buildIndexedTriangleExtended`。T227/228 的 vertex/index/
scratch offset 分别为 `48/8/256`，T228 indexType 为 UInt32、BLAS
由 descriptor 分配；T229 为 `48/0/0`，T230 为 `0/0/256`。
GPU compute 三阶段 buffer 为 `1,0,1`，render ray 为 `1`；前后 seek
不能把旧 buffer 值或资源身份串到其他捕获。无需判断 BLAS 几何预览。

### T226 UInt32 indexed offset 与 descriptor 分配

与 T225 共用事件/输出核对，但 `indexType=UInt32`，BLAS 由
`newAccelerationStructureWithDescriptor` 创建；前缀 8 byte 为两个
无效 UInt32 索引，实际索引从偏移 8 开始。无需额外画面判断。

### T225 indexed triangle 索引偏移

API Inspector 确认 `buildIndexedTriangleOffset` 的 `indexOffset=8`、
`triangleCount=1`、`opaque=true`；索引 buffer 前 8 byte 是无效前缀，
实际三项 UInt16 索引从偏移 8 开始。compute 三阶段结果 `1,0,1`，
render ray 为 `1`；前后 seek 不串线。无需人工判断 BLAS 几何预览。

### T223–T224 opaque box 几何

API Inspector 中确认 `buildBoundingBoxOpaque`，T223 `tableOffset=0`，
T224 `tableOffset=1`、`boxOffset=48`、`scratchOffset=256`。两份 dispatch
后的 12-byte 输出 buffer 均为 uint `0,0,0`；与 T218–T222 非 opaque
版本的 `1,1,0` 对照。无需人工判断 box 几何预览。

### T221–T222 box 几何交点函数表偏移

API Inspector 里交点函数表有两个槽，`setFunction` 指向槽1，
`buildBoundingBoxTableOffset.tableOffset=1`。T222 另有
`boxOffset=48/scratchOffset=256`。两份 dispatch 后 12-byte buffer
应为 uint `1,1,0`；无需人工判断 box 几何预览。

### T218–T220 box ray 与 compute 交点函数表

三份均应看到 `buildBoundingBoxStrided`、stride32、boxCount2，后接 compute
dispatch。T219 的 boxOffset48/scratchOffset256；T218/T219 API Inspector
显示单槽 `setIntersectionFunctionTable`，T220 显示 range 版
`setIntersectionFunctionTables`。dispatch 后 12-byte 输出 buffer 三个 uint
为 `1,1,0`，切回 build 前不应误显示 dispatch 后结果。画面仅清屏，
无需人工判断 box 几何预览；GPU 结果和畸形捕获已由终端验证。

### T216–T217 URL 动态库

API Inspector 应显示 `newDynamicLibraryWithURL`，其后可执行库依赖 T216
为该单库、T217 为 URL 库和普通动态库两项。选择 dispatch 时输出 float
分别为 3/6，draw 中央为红色；前后 seek 值、画面和依赖资源身份不串线。
原 URL 路径不应作为 GUI 验收前提；临时文件清理与畸形数据仅终端验证。

### T213–T215 box AS 非默认 stride

API Inspector 中三份均为 `buildBoundingBoxStrided`、`boxStride=32`；T213
`boxCount=1`，T214 `boxCount=2` 且经 descriptor 分配，T215
`boxCount=2/boxOffset=48/scratchOffset=256` 且经 descriptor 分配。
AS size1536、GPU compacted size1280；前后 seek 的资源/画面不应丢失。
不要求内部 box 几何预览或第二盒射线命中，后者仍需程序化交点测试。

### T209–T212 box AS 双类偏移

四份仅需在 API Inspector 核对 `buildBoundingBoxExtended` 及 `boxCount=1`：
T209 的 `boxOffset=48/scratchOffset=0`，T210 的 `0/256`，T211 的 `48/256`，
T212 的 `48/0` 且创建事件为 `newAccelerationStructureWithDescriptor`。
Shared 尺寸 buffer 在写前为 0、写后为 1280，AS 容量 1536；前后 seek 资源与
最终画面保持一致。不要求内部 box 几何可视化；错位/越界仅终端负例验证。

### T182 多 indexed triangle descriptor AS

确认 `buildIndexedTriangle.triangleCount=2`、UInt16 index buffer 含六索引，
72 字节顶点 buffer；GPU compacted size1280。与 T181 不同，需核对索引资源
身份和类型；不要求逐三角形几何预览。越过 index buffer 容量仅终端负例验证。

### T181 多三角形 descriptor AS

确认 `buildNonOpaqueTriangle.triangleCount=2` 和 72 字节顶点 buffer，
而非旧的单三角形普通 build chunk。AS size1536、compacted size1280
在当前设备仍与 count1 相同，须核对 chunk 字段；不要求几何预览。

### T180 三 box descriptor AS

与 T178/T179 共用其余检查，仅确认 `buildBoundingBox.boxCount=3` 和
72 字节 box buffer。AS size1536、compacted size1280 在当前设备仍未变化；
必须看 count 字段，不要求逐 box 几何预览。

### T179 双 box descriptor AS

与 T178 共用其余检查，只额外确认 `buildBoundingBox` 的 `boxCount=2`
和 48 字节 box buffer。AS size1536、compacted size1280 与单 box 在当前
设备相同，不能只看数值认定已构建两个 box；不要求逐 box 几何预览。

### T178 box descriptor AS

与 T137 共用 box build/compacted-size 检查，额外确认创建事件是
`newAccelerationStructureWithDescriptor`。AS size1536，GPU compacted size1280，
Shared 目标 buffer 的 write→build→write 回退与 T137 相同；不要求 box 几何预览。

### T176–T177 AS 双偏移

两份 capture 的第二 BLAS `buildOpaqueTriangle` 都应有 `vertexOffset=16`、
`scratchOffset=256`，Shared 末→前→末 `1→7→1`、中央绿色。T177 额外核对
descriptor 分配，T176 为 size 分配；只需比较这一个差异，不重复开面板。

### T175 scratch buffer 偏移

核对第二BLAS `buildOpaqueTriangle` 的 `scratchOffset=256`，使用比普通
scratch 多 256 字节的 buffer；Shared 末→前→末 `1→7→1`、中央绿色。
不要求显示 scratch 内部内容，未对齐/越界仅由终端负例覆盖。

### T174 偏移顶点 descriptor AS

在 T173 的检查上额外确认第二 BLAS 经 `newAccelerationStructureWithDescriptor`
分配，随后 `buildOpaqueTriangle` 的 `vertexOffset=16`。Shared 末→前→末为
`1→7→1`、中央绿色。分配 chunk 只存容量，不要求显示内部几何字段。

### T173 静态三角形顶点偏移

核对第二BLAS的 `buildOpaqueTriangle` 记录 `vertexOffset=16`，顶点 buffer
前 16 字节不是三角形数据。Shared 末→前→末 `1→7→1`、中央绿色；无须预览
AS 内部几何。未对齐/越界偏移仅由终端负例覆盖。

### T171–T172 UInt32 indexed 双侧对照

共用 T169/T170 的检查，只确认索引类型为 UInt32：T171 第二BLAS经 descriptor
分配再 `buildIndexedOpaqueTriangle`，Shared `1→7→1`、中央绿色；T172 沿用
`buildIndexedTriangle`，Shared `0→7→0`、中央红色。只需打开一次资源和事件面板。

### T170 indexed 非 opaque 反向对照

核对第二BLAS沿用 `buildIndexedTriangle`（不是 opaque chunk），仍绑定相同
fragment `reject_triangle` 函数。Shared 末→前→末 `0→7→0`、中央红色；
与 T167/T169 的 indexed opaque `1→7→1`、绿色对照，不重复检查其它面板。

### T169 indexed opaque descriptor 分配

核对第二BLAS由 `newAccelerationStructureWithDescriptor` 分配，随后
`buildIndexedOpaqueTriangle` 的 UInt16 索引 buffer；Shared 末→前→末为
`1→7→1`、中央绿色。分配 chunk 只显示容量，不要求显示 descriptor 内部几何。

### T168 UInt32 indexed opaque 三角形

与 T167 共用检查，仅核对 `buildIndexedOpaqueTriangle` 的 `indexType=UInt32`，
索引 buffer 资源身份及 Shared 末→前→末 `1→7→1`、中央绿色。不重复开面板。

### T167 indexed opaque 三角形

第二BLAS的 `buildIndexedOpaqueTriangle` 应含 UInt16 索引 buffer、count1 和
显式 opaque 几何；与 T148 共用自定义 reject 函数对照，Shared 末→前→末为
`1→7→1`、中央绿色。只核对 API 事件、资源身份和画面，不要求 AS 内部预览。

### T166 opaque descriptor 分配

核对第二BLAS由 `newAccelerationStructureWithDescriptor` 分配，随后出现
`buildOpaqueTriangle`；这里分配chunk只保留容量，opaque语义由build chunk体现。
与T165共用fragment自定义reject链，Shared末→前→末为`1→7→1`、中央绿色；
不要求API Inspector显示未被序列化的描述符内部字段。

### T165 显式opaque三角形

核对第二BLAS的`buildOpaqueTriangle`、TLAS两实例与fragment表绑定。与T148同一
`reject_triangle`函数链对照，T165的render Shared末→前→末为`1→7→1`、中央绿色；
T148仍是`0→7→0`、中央红色。不要求AS内部几何预览。

### T164 带默认descriptor的AS encoder

核对两个`accelerationStructureCommandEncoderWithDescriptor`入口均生成正常AS pass
事件，先BLAS、后TLAS，compute射线输出末→首→末为`0,1→0,9→0,1`。
无counter sample attachment；不要求AS内部几何预览。其余通用资源/事件检查与T142
合并做，不重复打开额外面板。

### T163 函数表显式资源声明

继承T161的交点函数和嵌套表检查；额外确认render pass中有对该visible table的
`useResource(Read, Fragment)`声明，后续Shared末→前→末仍为`1→7→1`且中央绿色。
错误encoder ID的进程异常已由终端负例覆盖，人工只需检查这一个新声明及资源链接。

### T161–T162 IFT嵌套visible table

fragment IFT仍绑定render slot2，交点函数参数slot0为Shared buffer、slot1为
visible function table（T162用range `(1,1)`）。核对嵌套表、函数handle、PSO/阶段
身份和资源链接；末→前→末Shared值`1→7→1`，中央绿色。值0反向对照只做终端验证。

### T159–T160 IFT buffer参数

表本身仍在fragment slot2；T159/T160分别以单槽/range API把Shared buffer绑定到
交点函数参数slot0。末→前→末Shared输出`1→7→1`，中央绿色；API Inspector可核对
表/buffer资源身份、offset0及range `(0,1)`。零值反向对照只做了终端验证，不额外
列入人工GUI项。旧T148–T153已以修正`instancing`签名的新capture替换，按下节验收。

### T156–T157 Opaque-triangle快捷更新

与T148自定义intersection函数使非不透明三角形未命中形成反向对照：T156/T157的
fragment intersection table仍绑定slot2，但表元素由opaque-triangle单槽/range
快捷API写入，render Shared末→前→末为`1→7→1`，中央应绿色。检查表资源、
签名`Instancing | TriangleData`及单槽index0/range`(0,1)`。两份捕获已通过
定向终端测试；06:26 IOGPUFamily kernel panic后全量回归主动暂停，未通过整批验收。

### T36 延迟store验证层兼容

仍使用原T36 capture，不新增人工项。检查原有动态store最终`C0/D/S=Store`、
中央颜色、深度/模板及begin→end回看即可。当前库修复了Validation下的setter
顺序；旧textureBarrier只在pass内尚无GPU工作时安全略过，后置情况明确拒绝。

### T148–T153 Intersection Function Table

六份capture分别核对fragment/vertex/tile单槽及range绑定的表资源、slot2、阶段与
PSO一致。普通compute射线仍为`1,0,1`；表内自定义交点函数使渲染阶段Shared
输出为`0`，末→前→末回看为`0→7→0`。T148/T149/T151/T152中央红色；
T150/T153为tile dispatch而非普通draw。检查事件与资源链接即可，不要求展示
AS内部几何或宣称完整ray tracing。

### T146–T147 Vertex与Tile阶段TLAS射线

两个capture分别核对vertex/tile的AS绑定资源为双BLAS TLAS、slot0。末→前→末
回看Shared输出都是`1→7→1`；T146中央像素绿色，T147应显示tile dispatch事件
（无普通draw）。不要求AS内部几何预览，亦不把tile无draw视为异常。

### T145 Fragment阶段TLAS射线

核对fragment AS绑定的资源是T144构建的TLAS、slot0；draw之前Shared输出仍为
哨兵7，draw之后为1，末→前→末seek稳定。中央像素为绿色，表示fragment shader
从x=+3的射线命中第二个BLAS。无需检查AS内部几何可视化。

### T144 两个不同BLAS的Top-Level实例

确认前序两个三角形AS build对应不同资源；128-byte实例buffer的两个descriptor
分别使用索引0/1，TLAS chunk保留两个child资源引用。三次dispatch后Shared输出
末→第一→第二→末为uint32 `(1,0,1)`→`(1,9,11)`→`(1,0,11)`→`(1,0,1)`。
第二个BLAS局部中心x=1、平移x=+2，因此第三条x=+3射线的命中可区分误用child0。
不要求AS内部预览或完整光追调试。

### T143 同一个BLAS的两个Top-Level实例

Event Browser/API Inspector核对128-byte实例buffer含两个描述符，平移分别x=-2与
x=+2，均指向同一个底层AS；top-level AS build应使用两个实例。三次dispatch的
Shared输出末→第一→第二→末依次为uint32 `(1,0,1)`→`(1,9,11)`→
`(1,0,11)`→`(1,0,1)`。不要求AS内部预览、不同BLAS数组或完整光追调试。

### T142 单实例Top-Level AS

核对1536-byte底层AS与1792-byte top-level AS的build事件顺序；TLAS的child资源是
底层AS，64-byte实例buffer的变换矩阵平移`x=2`、mask255、AS索引0。compute单槽绑定
TLAS而非底层AS，两次ray dispatch的Shared输出末→第一次→末依次为uint32
`(0,1)`→`(0,9)`→`(0,1)`。黑色drawable仅用于截帧，不要求AS内部可视化或
完整光追调试功能。

### T141 底层AS原位Refit与Ray Query

Event Browser/API Inspector核对同一个2048-byte AS：`buildRefittableTriangle`、第一次
`setAccelerationStructure`/dispatch、GPU buffer blit、`refitTriangle`、第二次绑定/dispatch。
输出Shared 8-byte buffer按末→第一次dispatch→末依次为uint32 `(1,0)`→`(1,9)`→
`(1,0)`；9是第二槽在写入前的哨兵。AS应显示资源链接，两个dispatch使用相同AS身份。
不要求AS内部几何预览、射线图像或完整ray tracing功能；黑色drawable仅用于截帧。

### T135–T139 静态底层AS build、描述符创建与Copy

AS资源应显示为Acceleration Structure，创建size1536。Event Browser中同一个
command buffer的AS encoder有begin、build、write compacted size、end；API
Inspector的AS/vertex/scratch/output buffer身份应一致。切到write查看Shared
output buffer前8字节为`00 05 00 00 00 00 00 00`（小端1280）；回退build应为零，
再前进write应恢复1280。AS内部结构不用可视化，最终普通画面沿用T39。完整ray tracing能力
尚未开放，T133/T70负例不要打开。T136另查UInt16 index buffer与build的资源链接；T137
另查24-byte bounding box buffer；T138创建事件应是descriptor入口而非size入口，分配大小
仍为1536。三份几何/创建新capture同样检查1280→0→1280，无需重复完整画面流程。
T139另查两个AS资源均为1536 bytes、build源→copy目标→write目标的身份；输出buffer的
write→copy→write回退值同样是1280→0→1280。它不是压缩后的1280-byte目标分配。
T140另查两个command buffer：源1536-byte AS build与Shared尺寸写回先完成，目标1280-byte
AS再执行compact copy与目标尺寸写回。源尺寸buffer在copy处为1280；目标输出buffer的
末→compact copy→末seek为1280→0→1280。不要把T139的1536-byte目标与T140混淆。

### T118 一层嵌套ArgumentEncoder

API Inspector确认父`newArgumentEncoder`、子`newArgumentEncoderForBufferAtIndex:0`、
父`setBuffer`到16-byte子buffer，子`setTexture:0`/`setSamplerState:1`。四象限
像素和clear→draw回退同T12；texture使用关系应存在。Pipeline State当前仅公开
父层直接成员，不要求出现完整嵌套树，也不据此判GPU回放失败。

### T116–T117 Placement Heap 非重叠资源

T116检查Heap→516-byte Private buffer父子资源关系与offset768；fill后、compute后
数据及中心像素和前后seek同T39。T117检查Heap→1×1 Private纹理父子关系与
offset128，三阶段clear/sample画面与seek同T72。两者只支持不重叠资源，
不要求GUI展示或验证alias生命周期。

### T112–T115 异步Pipeline预加载

按T112 fragment、T113 options/reflection fragment、T114 compute、T115 vertex
核对各自`preloadedLibraries`资源链接。四份capture均一条compute、一条draw，
buffer为`float 3`，中心红色；按事件回退后再次前进仍一致。异步callback不应
在离线回放时运行。

### T110–T111 异步动态源码链

T110看`newLibraryWithSource(completionHandler)`结果为dynamic source库，
T111看同入口结果为带`dependencies`的可执行库。两者后续均有动态库资源
链接、compute buffer `float 3`、红色draw与事件seek；离线不应再次执行
应用completionHandler。

### T106–T109 Pipeline 动态库预加载

检查T106 compute、T107 fragment、T108 vertex和T109 render options/reflection
创建chunk的预加载动态库资源ID链接。四份capture均为dispatch后buffer
`float 3`、draw中心红色；末→dispatch→draw seek时仍一致。T107–T109
render shader确实调用动态函数，不是仅登记descriptor属性。

### T104–T105 动态库链接

检查一份/两份动态library分别链接到可执行library，随后一条compute dispatch
与一条draw；T104的输出buffer是`float 3`，T105是`float 6`，draw中心为
红色。按dispatch→draw→dispatch来回seek时，buffer值仍正确，draw像素保持
红色。回放不需要原应用的临时metallib路径存在；URL导入动态库不在本次支持范围。

### T103 Counter 子范围与目标偏移

在`t103_capture.rdc`确认8槽sample buffer的pass索引依次2/3/4/5，
`resolveCounters`范围location2/length4、目标offset16。目标buffer
前16字节保持零，之后四个64-bit timestamp非零递增，后缀零；数字本身
不固定。draw中心RGB≈51/179/77；前后seek可正常读buffer。

### T102 Reflection BufferBinding argument encoder

查看创建chunk中的reflection快照：texture2d id0、sampler id1、长度16、
对齐8、supported=true。两次draw共享argument buffer但各选择不同packet，
x100 RGB≈20/40/60、x300 RGB≈70/80/90；前后seek时fragment成员资源
链接和像素跟随。复杂布局仍明确不支持，不在本项验收范围。

### T101 阶段边界计数器

查看普通 VS/FS 三角形 draw 与 render pass 的 sampleBuffer attachment：资源
ID一致，start/end vertex、start/end fragment 索引依次为0/1/2/3。
后续 `resolveCounters` 是 Copy 事件，目标buffer前32字节有四个非零递增的
64-bit timestamp；不同回放的具体值允许变化。draw中心像素约RGB 51/179/77，
在resolve→draw→resolve之间seek，确认资源与画面不丢失。不要把此结果
误读为encoder内 `sampleCountersInBuffer` 已支持。

### T100 异步 Mesh + 第二层 Rate Map

T100与T99的两层画面相同，但pipeline由异步回调创建；API Inspector中的
创建时快照要保留`maxMeshGrid=2`、BGRA8和mesh函数，不能被应用随后修改的
descriptor覆盖。离线不执行应用callback。MeshDispatch两个组、slice1光栅和
blit回drawable，最终x100绿/x175黑；前后seek稳定。

### T99 双层 Rate Map 的第二层光栅

MeshDispatch输入grid为2×1×1，mesh pipeline的`maxMeshGrid=2`。两个组的
primitive分别写入array slices0/1；slice1水平物理宽208，在x100为绿色、
x175为黑色。随后blit从source slice1到drawable，最终同样x100绿/x175黑。
前后seek检查两层与回拷结果跟随，不要把逻辑400宽当作第二层物理宽。

### T98 双层 Rate Map 数组 pass

API Inspector中render pass的map资源应指向T98的双层map，目标array length为2，
color附件为2-slice纹理。MeshDispatch在slice0输出绿色三角形，之后有blit复制
slice0到drawable，最终中心仍为绿色；前后seek保持对应结果。此夹具没有证明
slice1真实光栅输出，不把它列为人工必验。

### T95–T97 Rasterization rate map

T95在API Inspector检查map创建、`copyParameterDataToBuffer`和render pass map
资源链接；绿色三角形与普通T78一致。T96的第二组水平样本为0.5，物理宽度在
本机为208；三角形横向收缩，画面/事件切换仍稳定。T97检查两个layer的不同
采样数组和参数buffer资源；该fixture没有绑定双层map到array render target，
不要求看到双层渲染效果。三份均检查前后seek与capture切换。

### T93–T94 Object 输入网格

T93一次直接Mesh threadgroups draw的输入网格是2×1×1；T94一次直接Mesh
threads-grid draw的输入网格是64×1×1。两份均有紫红三角形、可前后seek；
不应把object输入网格错误地限制为单个object组可输出的mesh网格大小。

### T91–T92 多线程 Object stage

T91 的直接 `drawMeshThreadgroups` 和 T92 的 `drawMeshThreads` 均用
4×1×1 object threadgroup；T92事件树线程网格显示4×1×1。两份都应有
object→payload→mesh 的紫红三角形，前后切换时画面跟随。

### T88–T90 Mesh 间接绘制

三份 capture 的 API Inspector 均有一次 `drawMeshThreadgroups(indirect)`，参数
buffer 偏移16，事件树显示 `indirect, <?, ?, ?>` 而非猜测网格尺寸。
T88/T89中心为绿色三角形；T89需先有compute dispatch写入Private参数，再绘制；
T90有object buffer→payload→mesh，中心为紫红色。末→首→末选择时画面跟随。

### T87 异步Object/Mesh pipeline

API Inspector有一次带object function的异步mesh pipeline创建；快照为BGRA8、
payload16、三个函数身份有效，尽管应用之后修改原descriptor。一次MeshDispatch
产生T83相同三角形；离线不重演应用回调。

### T86 Object threadgroup memory

API Inspector检查三次`setObjectThreadgroupMemoryLength`长度16/32/48、slot0；
三次MeshDispatch画面与T84相同且可回退。scratch值已由原生shader与Replay API
验证，GUI不要求显示object threadgroup内存内部内容。

### T85 Object texture/sampler

API Inspector检查两次单纹理、一次批纹理及四种sampler重载含LOD clamp；
三次MeshDispatch绘制中心约x=120/250/370、y=150，原无采样位置应为黑色。
现有Pipeline State无object资源专用面板，不要求虚构vertex-stage绑定。

### T84 Object buffer/bytes

API Inspector检查两次setObjectBuffer、三次setObjectBytes、一次offset及一次
batch绑定；三次MeshDispatch分别在x≈80/220/360附近绘制RGB约179/51/102
的小三角形，切换事件时旧位置回到黑色。不要求现有Pipeline State显示object
资源专用面板。

### T83 Object/Mesh pipeline

API Inspector应有一个同步object+mesh pipeline（object/mesh/fragment函数）和一次
`setObjectBuffer`，Event Browser的一次MeshDispatch生成中心RGB约179/51/102。
现有Pipeline State无object/mesh专用面板，不要求显示为普通VS；资源身份与
fragment shader应可检查。

### T82 异步Mesh pipeline

API Inspector有一次`newRenderPipelineStateWithMeshDescriptor(completionHandler)`，
快照为BGRA8、mesh function有效、options=ArgumentInfo，尽管应用随后改变原descriptor。
一次MeshDispatch仍产生T78相同绿色三角形；离线不会重演应用回调。

### T81 Mesh thread-grid draw

API Inspector应有一次`drawMeshThreads`，线程网格32×1×1、mesh组32×1×1，
Event Browser为MeshDispatch；中心RGB约51/179/77、角落黑色。和T78
`drawMeshThreadgroups`比较事件语义，不把32线程网格误显示成32个threadgroup。

### T80 Mesh texture/sampler

API Inspector检查两次单纹理、一次批量纹理、四种sampler重载（含LOD clamp）；
三次MeshDispatch画面中心约位于x=170/270/370、y=150，T79对应x=100/200/300。
前后选事件检查三角形与清屏回退，不要求现有Pipeline State伪显示mesh资源。

### T79 Mesh buffer/bytes

API Inspector有两次setMeshBuffer、三次setMeshBytes、一次setMeshBufferOffset和
一次setMeshBuffers；三次MeshDispatch分别呈现左、中、右绿色三角形，选事件
前后画面正确回退。mesh pipeline与fragment shader链接可打开；现有Pipeline
State没有mesh buffer专用面板，不要求显示为vertex buffer。

### T78 最小 Mesh pipeline

API Inspector应有一次`newRenderPipelineStateWithMeshDescriptor`和一次
`drawMeshThreadgroups`；Event Browser显示`MeshDispatch`而非普通顶点draw，
网格1×1×1、mesh线程32×1×1。中心约RGB`51/179/77`，角落黑色。
当前Pipeline State只保留mesh pipeline资源与fragment shader身份，不要求虚构的
vertex shader或尚未建模的mesh/object面板。

### T77 异步Tile pipeline

API Inspector应有一次`newRenderPipelineStateWithTileDescriptor(completionHandler)`，
pipeline仍是BGRA8且tile function身份正确，尽管应用发起调用后改动了原descriptor。
三条dispatch和buffer/最终draw/seek与T74一致；离线不执行应用回调，也不要求
Tile专用shader显示为VS/FS。

### T75–T76 Tile资源与动态内存

T75在API Inspector核对两次单纹理、一次数组纹理、两次单采样器、两次数组采样器
（其中各一次LOD clamp）；1×1纹理红通道为`2/255`，因此12-byte buffer三段依次
为`tileCount×2/×4/×6`。T76核对四次threadgroup memory调用：长度/offset依次
为`16/0`、`0/0`（清除）、`16/16`、`16/0`；三个dispatch后的buffer回到
T74的`tileCount×1/×2/×3`。两者只需末→首→中→末seek与最终draw红色；不要求
Tile专用stage显示为VS/FS。

### T74 Tile shader

API Inspector中核对一个tile pipeline、三条`dispatchThreadsPerTile`，网格应对应
render target按16×16分块，线程组16×16×1；12-byte buffer三个4-byte段依次为
`tileCount×1/×2/×3`，末→首→中→末seek时后续段尚未写入。最终draw读取第三段，
中心呈红色，回退后恢复。Tile专用shader不应假装显示为VS/FS；当前UI尚未建模该阶段。

### T73 Render heap residency

四个`useHeap/useHeaps`入口均应出现在API Inspector，指向同一Heap；带`stages`的
两条显示fragment阶段。三个draw的中心颜色及末→首→中→末回退沿用T72，只做差异确认。
不要求placement/alias，也不在GUI打开T70预期拒绝捕获。

### T71–T72 Heap

T71在资源列表中核对一个Heap与它创建的516-byte Private buffer；draw绑定该buffer，
内容、画面和前后seek应与T39相同。T72核对Heap与1×1 Private纹理的父子关系；
三个draw采样同一纹理，中心像素RGB依次为`10/20/30`、`40/50/60`、`70/80/90`，
末→首→中→末切换时画面跟随。无需测试placement、sparse、aliasable；T70不可
离线重放，不在GUI中打开。

### T55扩展与T69

T55初始化资源里有两个`newIntersectionFunctionWithDescriptor`（同步/异步），分别
被两个compute pipeline的`linkedFunctions`引用；原四次compute输出与画面不变。
不要求ray-tracing执行结果或完整函数表UI。T69有`newSharedEventWithHandle`导入的第二
个Sync资源；第二条command buffer的wait/signal用导入资源，前后两条使用源event，
信号值仍是同一时间线的100→101→102。两个draw中心依次为灰度10、80；末→首→末
切换画面和buffer数据跟随。跨进程handle不要求GUI验证。

### T67–T68

T67三个patch draw依次红、绿、蓝；直接indexed事件为3控制点/1实例，两个间接事件
数量字段为未知（0），不应显示编码时CPU推断值。切到前一个事件时后续颜色尚未出现，
再切回末事件恢复。T68源纹理导出handle后导入为另一资源；两个draw都绑定导入纹理，
中心画面依次RGB`20/40/60`和`70/90/110`，后→前→后回退。跨进程handle不要求UI验证。

### T66

一个三控制点的`drawPatches`事件；factor buffer链接可打开，scale为1。draw中心约为
RGB`64/128/191`，切回clear再切回draw时画面恢复。间接与indexed patch draw不在本项范围。

### T65

1. 资源创建应有一个`newSharedEvent`及初始`setInitialSignaledValue(100)`；
   API Inspector可核对同一Event在三条command buffer上的两次signal、三次wait，
   包括首次wait100和值递增。两个draw分别依赖前一次GPU信号；
   Fragment buffer slot0均指向同一256-byte buffer。
2. 两draw中心画面依次为灰度RGB`10/10/10`、`80/80/80`；后→前→后切换时画面、
   buffer内容与状态跟随。字节级填充、跨epoch重建及12个畸形输入已自动验证。
   GPU使用后的CPU修改signaledValue是明确失败边界；同进程shared handle导入见T69，
   不要求在GUI打开负例。

### T63–T64

1. T63四次draw在 Pipeline State 的VS输入中，position slot0的有效stride均为16；
   offset依次0、48、144、0，最后一次是inline bytes，故不要求它链接到原position
   buffer。color slot1是同一个buffer，offset依次0、48、96、144，stride16。
   中心像素RGB依次`10/20/30`、`40/50/60`、`70/80/90`、`100/110/120`；
   末→首→第三→第二→末seek后状态/画面跟随。首draw还有单视图零映射
   `setVertexAmplificationCount`；不要求多视图画面。
2. T64资源创建里应有`newSharedTextureWithDescriptor`，1×1 Private纹理；三次draw
   的FS texture slot0均指向这一资源。中心RGB依次`10/20/30`、`40/50/60`、
   `70/80/90`，末→首→中→首→末seek正常。Shared handle和跨进程共享不在本项范围；
   不要求人工查看底层纹理字节，像素/资源身份已自动断言。

### T62

1. 帧内 buffer 和 texture 的 `setPurgeableState(NonVolatile)` 应可在 API 调用中
   检查；抓取前的 `KeepCurrent` 仅要求自动测试确认，不要求 GUI 展示初始化 chunk。
   这是资源状态调用，不要求出现新的可见绘制事件。
2. 唯一 draw 的 FS buffer slot0 与 texture slot0 均能链接到有效资源；中心画面
   RGB `15/26/37`。像素、资源身份和畸形状态已自动验证；人工仅确认 Inspector 与
   资源链接。设备只读查询和宿主编译开关无 GUI QA 项。

### T61

1. 资源初始化见`newBufferWithBytesNoCopy`，4096-byte Shared buffer；三draw的FS
   buffer slot0为同一资源offset256，API Inspector调用参数可见。逐draw画面RGB为
   `10/20/30`、`40/50/60`、`70/80/90`，按末→首→中→末切换应恢复。
2. Buffer Viewer在offset256的四个uint随事件分别为上述RGB加0，周围字节保持a5。
   逐字节/padding已自动核对，不必人工完整导出。deallocator时机差异属抓取生命周期
   单独边界，不靠GUI事件树验收；真实应用QA再观察page-pool压力。

### T60

1. 资源初始化可见`newArgumentEncoderWithArguments`及两个descriptor：id0为2D sampled
   texture、id1为sampler。两个draw的FS argument buffer slot0分别选offset256/272；
   `packet.colour`和`packet.filter`成员资源链接在两包间切换纹理、共用sampler。
2. 首draw左半屏RGB`20/40/60`、右黑；第二draw左仍`20/40/60`、右`70/80/90`。
   后→前→后时成员状态和画面恢复。325-byte原始packet/哨兵和非法descriptor已自动验证，
   不要求人工解码或复验负例。

### T58–T59

1. T58资源初始化中应能找到同一父纹理的简单view、mip/slice subset view和swizzle
   view，父子链接可打开。九个draw分三阶段、每阶段三横带；RGB依次为第一阶段
   `12/34/56 | 21/43/65 | 65/43/21`，第二阶段`44/66/88 | 31/53/75 |
   75/53/31`，第三阶段`54/76/98 | 41/63/85 | 85/63/41`。末→首→中→末
   应恢复。各mip/slice初值和GPU别名写已自动逐字节验证，不要求人工导出。
2. T59资源初始化中应看到Shared父buffer与buffer-backed子texture的派生链接。
   三draw RGB依次`10/20/30`、`40/50/60`、`70/80/90`；末→首→中→首→末
   回退正常。offset32/rowPitch16/总长153与padding、初值、CPU更新、GPU clear
   已自动逐字节验证，人工只看链接、画面与共同状态栏。

### T57

1. 四个draw，FS slot0的外层argument buffer在offset256/352间切换。FS Storage Buffers
   应显示`packet.single`、`packet.inputs[0/1]`（成员id0/2/3），可打开三个成员buffer；
   第1/3个draw的成员offset为16/32/48，第2/4个为32/48/64。不应将descriptor内部地址
   显示成7936等大槽号；外层621-byte buffer与成员buffer不能混淆。
2. FS texture成员id4/5、sampler id6/7在两个packet间交换资源身份；texture链接为两个
   1×1纯色资源，Point/Linear sampler跟随切换。资源初始化里有arrayElement选择、
   buffer逐成员编码和constant getter；不要求批量独立行、CPU指针或新的常量解码面板。
3. 顺序点四draw，左右RGB依次为`17/27/37 | 黑`、`17/27/37 | 57/67/77`、
   `23/33/43 | 黑`、`23/33/43 | 68/78/88`，alpha255。末→首→末时画面和上述绑定恢复；
   成员数据、bias/delta、padding已逐字节自动核对，不要求人工重复计算。
   帧内资源重绑的拒绝变体只做自动测试，不新增人工打开项。

### T54–T56（共享一次公共检查）

1. T54选indexed draw：3 indices、2 instances，UInt16 index offset4，vertex bindings
   offsets8/24，bases为0。左红右蓝；clear→draw→clear→draw跟随，Mesh instance0/1可切换、
   资源链接可打开。API短重载本身没有baseVertex/baseInstance字段，这是预期。Legacy
   blit无options字段的派生capture已做自动API像素验证，不新增人工打开项。
2. T55四dispatch、一draw；四个不同CS shader/PSO，第三个CS名`cs_function_named`，其余
   `cs_function_variant`。四种创建在资源初始化记录，不要求出现在帧事件树。548-byte output
   四段offset0/128/256/384，32个uint的起值17/43/79/127、步长3，尾36bytes零；只看
   首→末→首dispatch时区域恢复，不重复逐元素手算。最终RGBA17/43/79/255，资源链接可打开。
   原生callback/error、copy/reset、对象释放已自动测，不要求离线执行回调或新增专用常量面板。
3. T56一个draw，FS argument buffer slot0，texture成员`arguments.colours[0/1]`对应id2/3，
   sampler成员`arguments.filters[0/1]`对应id6/7；前者Point/ClampEdge，后者Linear/Wrap。
   四象限RGBA依次144/60/72、32/148/88、36/76/184、136/140/80（alpha255）；两texture链接
   一个是原四色纹理，一个纯色40/80/120，事件回退及公共状态栏正常。数组API规范化为逐成员
   初始化调用，不要求显示独立batch行或reflection指针。buffer/constants/多packet由T57覆盖。

## 前批最小差异项（仍待验）

### T53

1. 四个 execute 父行范围为 0+1、0+6、2+3、1+4；共 14 个子项，9 draw / 5 empty。
   空项标为 `ICB[n] empty command`，不带 Draw 标记。API Inspector 可见两 reset、两
   copy、三 optimize，源/目标 ICB 链接及范围可查看。单命令 reset 位于资源初始化记录，
   不要求出现在帧内事件树，也不要求 ICB 原生二进制解码面板。
2. 复制后的绿色 indexed draw 使用不同 PSO；vertex slot0/1 offsets 为 16/0，index
   为 UInt16、offset4/size12。检查 80-byte Params 和 16-byte tint 资源链接、状态跟随；
   参数/padding 已自动核对，不重复手算。空项不要求有 Mesh 输出或虚构的 draw bindings。
3. 四父行画面依次全屏洋红、红/绿/蓝三带、红/黑/蓝、红/黑/蓝；选末→首→第二→末，
   首阶段应恢复洋红，无上一轮 GPU 改写残留。状态栏与公共检查共用。旧 T20–T27 不需
   重验；`t53_preframe_unknown_capture.rdc` 是预期拒绝的负例，不加入 UI 打开清单。

## 前批最小差异项（仍待验）

### T51

1. 三个 dispatch、两个 draw；CS 为 `cs_async`，VS/FS 为 `vs_async`/`fs_async`。
   三个 compute 和两个 render pipeline 资源身份不同；初始化记录可见六个带
   completionHandler 的创建入口，不要求它们出现在帧内 Event Browser。Options 为 0/3，
   render attachment 为 BGRA8、compute descriptor maxThreads 为 64；这些快照已自动核对。
2. CS output slot0 offsets 为 0/128/256，slot1 为 4-byte inline AsyncParams。428-byte
   output 三段各 32 uint 起值为 31/83/127，尾 44 bytes 零；选首→末→首 dispatch，
   观察已写区域随事件恢复即可，不重复逐元素手算。PSO/shader/buffer 资源链接可打开。
3. 第一 draw 左半为 RGBA31/83/127/255、右半黑，第二 draw 全屏同色；来回切换不残留。
   不要求离线执行 callback、显示 block/error 地址或专用 reflection 面板。原生回调
   次数、错误、反射和对象生命周期已自动验证，GUI 不重复验证这些不可见行为。

### T52

1. API Inspector 可见六次同步，顺序 signal→wait→signal→wait→signal→wait，两个
   Event 资源身份稳定；值依次 25/25/27/27/29/29（三组相等对）。对应三 command buffer，
   队列为 A/B/A；最后一对 signal/wait 同属第三个提交。Event 为 Sync 资源，不要求专用面板。
2. 两 fill、一个 dispatch、一个 draw；在 output clear→dispatch→draw→clear→draw
   间切换。444-byte output 首 96 项 uint 在 dispatch 后为 51+i，尾 60 bytes 恒零；
   clear 后全零。不要以首次 clear 前未定义的 output 数据判失败。资源链接及画面跟随即可。
3. 最终 RGBA51/83/115/255，状态栏检查与其他 capture 共用；可额外在六个 signal/wait
   间往返一次确认交互无挂起。精确数据和同步点回退已自动覆盖，不要求 GPU 时间线或
   可视化并行调度。SharedEvent、外部事件与 future-signal wait 不在本次范围。

## 前批最小差异项（仍待验）

### T50

1. API Inspector可见四次getBytes，row pitch依次20/28/16/32；两次slice重载image pitch
   为84/128、slice为1/0，最后一次region depth=2。它们记录读取元数据，不含原应用指针
   或host返回缓冲。之前有三次synchronizeTexture：mip/slice为1/0、1/1、0/0，资源链接可打开。
2. 选draw→最后getBytes→draw：68-byte buffer前五项uint32从43/79/113/151/152恢复为
   全零再恢复，尾48bytes恒零；最终RGBA43/79/113/255。数值已自动核对，人工重点看
   事件切换和资源链接，不要求UI再次执行CPU读取。
3. 11×7纹理mip1是灰度43，18×10数组slice1/mip1灰度79；7×5只读纹理灰度113，
   19×3仅同步引用纹理灰度197，alpha均255。检查资源保留与2D子资源选择即可。
   8×4×2的3D原生读回已自动验证，但现有Texture Viewer/Replay API不支持3D展示，
   本项只看其API资源身份，不要求显示3D内容。状态栏检查和其他capture共用。

## 前批最小差异项（仍待验）

### T49

1. 两个command buffer，各两条`addScheduledHandler`与两条`addCompletedHandler`，总八条。
   API Inspector只有CommandBuffer资源链接；它们是注册事件，不是block执行事件，不要求
   block地址、执行线程/时间或回调专用面板。两次dispatch入口均`cs_handlers`。
2. 在后一次dispatch→前一次dispatch→后一次dispatch之间切换：12-byte Params从
   41/67/101恢复为7/11/13再恢复；412-byte output前三段各32项，前一次分别从7/11/13
   递增，后一次分别从41/67/101递增。28-byte尾部全零。数值已自动核对，人工只看回退及资源链接。
3. 选最后draw，最终RGBA41/67/101/255，状态栏无错误。离线不运行应用回调；不要把UI
   不触发block视为失败。旧capture未重录，公共回退路径已在50份自动回归中重新覆盖。

## 前批最小差异项（仍待验）

### T48

1. 五个dispatch加一个draw；CS入口为`cs_binary`，五个不同shader身份，其父资源为五个
   library。初始化记录包含file/URL/data/bundle/default五种创建，metallib数据各10765bytes。
   origin是诊断路径，不需要重新提供原文件；原路径失效回放已经自动验证，不必人工搬文件。
2. CS output是676-byte buffer，offset依次0/128/256/384/512；slot1为4-byte BinaryParams。
   首dispatch→末dispatch→首dispatch观察buffer已写区域跟随，末尾36bytes始终0。
   五段起值29/61/97/137/173、每段32项递增已自动逐元素核对，不重复手算。
3. draw最终暗蓝色RGBA29/61/97/255，VS/FS分别`vs_binary`/`fs_binary`；shader及buffer
   资源链接可打开、状态栏无错误。Binary shader仅有入口/反射，没有捕获MSL源文件；
   不以“缺源码”判失败，不要求二进制反汇编或新增library面板。

## 前批最小差异项（仍待验）

### T47

1. 四个dispatch、两个draw。选首/末dispatch，CS均`cs_main`，四个不同pipeline资源ID，
   output slot0 offsets为0/128/256/384；slot1是4-byte inline Params。后两pipeline的
   初始化记录为compute descriptor：maxThreads64、整倍数true、buffers Mutable/Immutable。
   function/descriptor/render三个创建入口各两份，optionsValue分别3/0。创建调用在初始化
   记录中，不要求出现在帧内Event Browser中；无专用reflection弹窗要求。
2. 打开532-byte output，先选首dispatch再末dispatch再返回首dispatch，观察有效数据区域随
   事件恢复即可。四段起始uint32分别17/41/73/109，每段32项递增，最后20 bytes为0；
   已自动逐元素核对，不需要手算全部数据。CS/VS/FS shader和资源链接均可打开。
3. 第一个draw只画左半，右半黑；第二个draw画满，最终RGBA17/41/73/255的暗色画面。
   来回切换画面跟随、无错误状态栏。这一份增量与旧14份合并，旧已验功能不用重做。

## 前批最小差异项（仍待验）

### T44

1. 事件顺序为Blit→Compute→Render→Blit→Compute→Render，两个dispatch、两个draw。
   API Inspector能查看十条update/wait：Blit两update一wait、Compute两update两wait、
   Render一update两wait。四个fence资源身份稳定；最后Compute update复用第一个Blit的fence。
   Render第一次wait和update是Vertex(1)，最后wait是Fragment(2)。
2. 304-byte data按uint32查看：首次dispatch前三项3/4/5，首个draw后10/11/12；该draw
   只写buffer，backbuffer黑色是预期。中间copy后308-byte readback前三项10/11/12，
   第二dispatch后15/16/17；最后4 bytes保持0。选末dispatch→首dispatch→末dispatch验证回退。
3. 最终画面接近黑色，拾取RGBA15/16/17/255；不是彩色三角形。VS/CS写buffer及copy usage
   能跳转；fence为Sync资源，不要求新增“Fence面板”或GPU时间线显示。

### T45 / T46（共用数值不重验）

1. T45选真实`presentDrawable(atTime)`事件（不要选合成End of Capture），API Inspector
   的time=1.25、输出指向backbuffer，最终颜色同T44；离线replay不应按该时间等待。
2. 帧内buffer marker顺序为remove→`payload range` offset4/length16→remove→
   `final range` offset32/length12。172-byte独立buffer全0x72，可通过资源链接打开。
   marker只在API元数据中展示，不要求Buffer Viewer范围高亮；后台旧setup标记已被reset覆盖。
3. T46只查看Present变体`afterMinimumDuration`、time=0.001以及最终同色输出；marker/API
   能正常显示即可，不重复T44所有fence、数据和seek操作。两个T分别反馈验收结果。

## 上批差异项（仍待验）

### T42

1. 一个compute pass内三个dispatch和最终一个draw；API Inspector可见single/batch资源
   声明（含空数组）、两类barrier；scopeValue=1。三条marker的字符串分别为
   `T42 dependent compute`、`T42 resource barrier`，以及pop。只作API事件，不要求嵌套组。
2. 272-byte buffer按uint32查看：第i项（i=0..67）依次为i+1、3i+10、3i+21；选第1→3→2→3
   次dispatch，值正确恢复。CS slot0为可写buffer，offset0/size272，usage含compute write。
3. 44-byte独立buffer全0x6d；11×9独立纹理为RGBA34/68/102/255。两者只有声明引用，仍能
   打开且切事件不丢内容。最终backbuffer为RGBA21/117/222/255。

### T43

1. 三个draw：前两次仅vertex写buffer、关闭rasterization，黑色backbuffer是预期，不是失败。
   112-byte buffer的前三个uint32先20/40/60，再27/47/67，剩余全0。第三次draw最终画面
   为RGBA27/47/67/255。按第1→3→2→3个draw回退，数据与画面应跟随。
2. 前两个draw的VS Storage Buffers表有slot0的可写buffer，offset0/size112，链接可打开；
   resource usage有vertex write。第三个draw换pipeline后不能残留旧VS可写条目；fragment
   读取该buffer。共用原Pipeline页面，不要求新的独立RW面板。
3. API Inspector显示普通/staged batch、带stage单资源声明，scope barrier的after/before
   都为Vertex(1)，resource barrier为Vertex(1)→Fragment(2)。52-byte独立buffer全0x6d，
   12×9独立纹理同T42颜色；仅声明资源也不能丢失。

## 前批新增项（仍待验）

### T40

1. 两个compute pass、五个dispatch。API Inspector能看到三次`setBytes`（slot2，16 bytes，
   首个uint依次1/2/5）、四次buffer offset、threadgroup memory长度32/64/48与slot5清零。
   三次dispatchThreadgroups为1×1×1，另两次dispatchThreads为8×1×1；每组8 threads。
2. 80-byte输出buffer的offset0/16/32/48/64按事件依次写入uint32 `92,200,108,232,124`，
   每项之间12 bytes保持0。第1→第5→第2→第5个dispatch往返选择，未执行的输出应恢复0。
3. CS slot0输出offset依次0/16/32/48/64；slot2先inline、再真实256-byte buffer的0/16，
   最后回到inline。inline槽没有真实resource链接是当前设计，不应残留上一buffer；原始bytes
   看API Inspector。未实现inline常量解码或threadgroup专用Pipeline字段，不要求不存在的UI。
4. 最终纯色RGBA `92,200,108,255`，状态栏/资源链接/切换检查与其他capture共用。

### T41

1. API Inspector显示`blitCommandEncoderWithDescriptor`、`hasSampleBuffers=false`，仍有
   Blit pass边界。四条optimization分别为GPU whole、GPU slice1/level1、CPU whole、
   CPU slice0/level1；whole与slice版本同名，应通过参数区分。
2. 额外13×7纹理只有hint引用，Texture Viewer应可打开且为RGBA `17,34,51,255`；前后
   跳转不丢资源。最终backbuffer仍为 `64,128,192,255`。原T37的六种copy/padding详细
   UI步骤不用在T41再重复一次。
3. optimization只作API事件，不要求新增copy action。counter sample附件明确未支持，
   不在本次人工验收范围。

## 更早新增项（仍待验）

### T38

1. 三次compute dispatch后，CS sampler slot2有效LOD分别为1..1、2..2、0..2。
   第二次batch范围2+2，slot3应为空；single nil的slot3也不残留。Buffer前3个float4依次
   写入`(0,1,0,1)`、`(0,0,1,1)`、`(1,0,0,1)`；未执行部分保持0。
2. 三个draw的VS/FS slot2 LOD分别为`1..1 / 2..2`、`2..2 / 1..1`、`0..2 / 0..2`；
   sampler资源身份相同，不能因同对象而串stage范围。已有sampler UI如果没有展示全部字段，
   记录显示缺口；API Inspector参数与GPU结果仍须正确，不能默认为UI已通过。
3. 最终画面从左到右是黄、黑、蓝（x分界128/256）。前后切dispatch/draw，LOD和数据恢复。

### T39

1. 选择fill和dispatch，打开被compute写入的516-byte buffer（不是native参考Shared副本）。
   fill后全`a5`，dispatch后前16 bytes为`03 0a 11 18 1f 26 2d 34 3b 42 49 50 57 5e 65 6c`。
   offset515最后一个byte为`18`；切回fill再前进结果一致。
2. 最终backbuffer为深色RGBA `3,3,24,255`。Private buffer页面不应空白或崩溃；资源usage
   包含compute写、copy source及fragment读。

## 先前待验差异项（不重复公共操作）

- T34/T35：沿用 `QA_BATCH35-37.md` 对应两节；T35增加初值回退差异：首dispatch的前两个
  uint应为`17,0`，次dispatch为`17,17`，末→首仍为`17,0`。这覆盖旧单中可能残留的
  首dispatch即`17,17`预期；BATCH57已修正Shared初值恢复并用旧/新录capture自动验证。
- T36：同单原状态项，再加 `QA_BATCH38.md` 的visibility/store/barrier增量。
- T37：`QA_BATCH38.md` 的pitch/offset/padding与slice/mip；D32S8读回≠完整深度显示shader。
- T10_debug：只检查push/signpost/pop字符串及原画面；不要求新增嵌套分组，不重验旧T10。

若全部符合，回复 `本单 T34–T312 与 T10 marker 全部符合`。也可逐项反馈；未反馈项目继续待验。
`QA_PENDING.md`是状态登记，不能因为本单已经合并就自动关闭任何PHASE/BATCH。
