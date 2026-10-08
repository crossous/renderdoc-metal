# B533：UE 小 Lumen 缓存实际截帧

完成工具与真实capture/AIR审查，UE GPU重放仍未通过。--small-lumen-caches从本机UE renderer公开cvars取得：surface atlas512，ScreenProbe/TranslucencyRadianceCache grid4、probe8、atlas32。四clipmap最大256cells小于32²probe容量；final (8+2)*32为320²。保持HWRT/Inline、trace预算与光追调度，不修改RenderDoc生产flags/driver guard。运行时要求所有cache/core RT cvar值正确且LastSetBy=SystemSettingsIni，并要求绑定非零HW query和全部AIR审查完成，否则工具失败；--check路径与实际模式均完成。

隔离B527三mesh/two-light工程、UE5.8.3CL58210709、同旧isolatedprovider9d6349a6，delay6/startup120/stall12/post20。新capture9238d0c36d5a67683aca6cc4fecb25604b4d139be71acff8300f54b7df11da42/70,728,712bytes，3typed间接TLAS/99direct+56indirectcompute、7heapAS/identity，164native编译完成无pending。owned cleanup -9且capture saved，不计editor自行正常退出。

60原metallib Lumen AIR全部审查、0unvalidated，原pipeline/function/library绑定和per-use冻结间接组证明两个HW query shader被非零调度：Main_0000a274_46403057 chunk32274 [2,64,1]，e330d5013fa2d374b0de3c037c7087d9ef6c971434399c978489c167e8a25a1c；Main_0000c358_ed865861 chunk34121 [132,1,1]，18b9c6bc46efa7b1cdbb3718041b3fbb0123033799913e2856ea3d9bfda03c56。AIR allocate/reset/next/commit/committed记录完整；不证明每invocation动态分支、UE输出/离线重放/绑定或事件验收。

实际frame/背景texture证明surface512²、radianceAtlas320²、R32Uint indirection16×4×4与12×4×4。52heap合计3,556,769,792bytes，比B528 2,962,489,344增加；初态blob1,170,276,095，比旧1,608,781,321减少。不能宣称整体成本单调下降，更不能认为预算达标：heap alone已超过policy最大3072MiB-128MiB，full budget scan尚未达。源码定位isolated provider MetalBuffer.cpp block64MiB（installed原512MiB）；可能进一步独立减小allocator粒度，必须实测wholeheap，保留driver收费/总预算。

原新RDC不改，normal-open和mandatory-no-GPU CPU65均API4，无GPUwait/断言/overrun/streamseek/资源表诊断。normal仍frame-born身份未完整声明，CPU65下一R16Uint2D4096×16/mip1/usage3 frame拒绝。无GPUreplay/图像/event/typed-binding/EID0验收。后端未改，官方/相关样例与回归使用B532同hash通过证据；本批未额外重跑，不把B533 capture计为完整重放通过。full78IR/75RT/308、Qt/ARC等未跑，原用户Qt/AS ForceCrash与系统冻结根因未闭环。

产物build-macos-debug/metal-ray-b533：manifest.json、UE-capture/{manifest.json,original.rdc,original.zip.xml,ray-workload-inventory.json,AIR-proof/manifest.json}、small-cache-memory.json、preflight-manifest.json及四API日志；session20261006-195554完整native/compile/supervisor日志。原用户工程/installed engine未改，原editor settings hash不变。

backend/bundle SHA256 dba15ac3b254f91afa4f999dfc0d1b1df61a894ef0723bb9b7f86241d929fc7c；GUI3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。构建/GPU共享锁串行、有限超时，syntax/diff通过。两flagsfalse；下一B534实际R16Uint二维sample，再provider heap粒度、sRGB及typed动态header/producer闭包。持续active，无提交推送。
