# B542 — Private placement BLAS 几何冻结与 UE 压缩子配方

2026-10-07，PHASE59。补齐 tracked Private placement 顶点/索引的 encoder-end 冻结、初态 recipe 与帧内 BLAS 重建；真实 UE TLAS 引用的两个 compact BLAS 现在均有完整捕获配方。生产 `supportsRaytracing` / `supportsRaytracingFromRender` 仍 false，持续任务 active。未提交、推送或重置，保留用户 RDHeaderView 改动、原 UE 工程及安装引擎。

## 对照与实现边界

先核对仓库 Vulkan `VulkanAccelerationStructureManager::CopyInputBuffers`（vk_acceleration_structure.cpp:186）、DX12 `BuildRaytracingAccelerationStructure`（d3d12_command_list4_wrap.cpp:1131）及 Metal 初态/compact 重建。沿用已知对象、typed build-input 冻结与独立重建，不查看 AS 内部，不提供 RT shader 单步或 RT Pixel History。

- B541 已捕获主 TLAS 的三个 packet 与两个已知子 AS，但子 BLAS recipe 仍缺失。本批实际 UE trace 确认几何源均为 Private/Tracked placement，非 Shared。将原先只允许间接实例输入的 tracked placement encoder-end 快照扩展到 kind1/2/8 顶点及索引；校验捕获对象类型、精确 native owner、显式退休和 heap tracking，保留 scratch/AS 同 heap 写入排斥、每 buffer/encoder 64MiB 上限。原始构建输入随后擦除，重建仅用独立冻结数据。
- 初态 kind1/2 使用原有 packed 有效顶点/索引，kind8 保留整个 VBO/IBO 及多 descriptor 原 offset/stride/顺序；source/index 原分配范围继续校验。Private placement 初态和帧内 frozen geometry 复用既有验证与 staging 生命周期，无新 chunk 格式。Shared/Managed placement、refit、未知别名及其他未验证输入不扩展。
- compact AS 继续从完整 staging AS 进行本机 compact-size 查询、目标容量核验和 compact copy。没有全局放开能力或伪造原生支持。
- fixture 新增三角形、UInt16/UInt32 索引、2/64 几何及 UE 同布局48三角形；初态源上传→build→整 buffer 清零→compact/TLAS/query。未引用 NaN padding 合法保留。帧内几何另覆盖 coverage3 和 coverage65 CPU kind3 heap-query/新 Header。
- 强 heap-frame oracle 按实际 geometry+TLAS 两 build 定位事件，验证各 build 前后、query 前后、反向定位和 EID0，核验 Header/descriptor 重定位、贡献、usage、输出和 Private 源清零。

## 最终固定库验收

产物 `build-macos-debug/metal-ray-b542/final/`。入口 variants.py、heap-frame.py、negative.py、frame-negative.py、heap-frame-negative.py、acceptance.py、UE-proof.py、freeze.py。build/GPU/UE 串行共用 IR lock；固定输入、有限帧数与 process-group timeout，没有 UE/qrenderdoc 重叠。

| 范围 | 实际结果 |
| --- | --- |
| 六初态堆几何 native→capture→强API/CLI | PASS，192事件/EID0；triangle/indexed3=2292、indexed compact64=3329、multi2 compact3=2305、multi64 compact64=4148、UE48 compact64=3376。真实最后 PrimitiveIndex47/GeometryIndex63 命中；源 VBO/IBO 全零，冻结 recipe 正确 |
| 四帧内堆几何 coverage3 | PASS，96事件/EID0；triangle1=2508、indexed3=2542、multi2/3=2555、multi64/64=4398，两个非零 query 的前后输出/usage |
| 四帧内堆几何 coverage65 heap-query/新 Header | PASS，160事件/EID0；与上述相同结果，geometry+TLAS build 各自前后定位及新 Header 生命周期 |
| 四 fresh 旧静态 heap-query | PASS，64事件/EID0；Shared32、Private placement、64实例末尾 Header、空64；共18 query capture / 512事件，不含官方/其他旧回归事件 |
| 损坏 capture/合法控制 | PASS，八源714坏组/1428 API4+CLI1 无GPUwait拒绝，26合法强API/CLI控制。初态四源396组/19合法，coverage3帧内两源286组/5合法，coverage65帧内两源32组/2合法 |
| 旧回归 | PASS，56 checks（含 helper builds）：B536格式、旧16格式/large R32、fresh163初态纹理2capture12696subresources、八B526 query、B512 multi64 TraceRay、四旧compact API/CLI、六能力入口、官方两scene270事件10query/CLI3 |
| fresh Shared compact | PASS，四mode native/capture/API/CLI，18checks含两build；background-compact、indexed-compact-frame、tlas-indexed-child-compact、compact-copy |
| controller生命周期 | PASS，multi64 compact与frame multi64各10 controller open/seek-reset/readback/close，resident growth11190272bytes，小于64MiB上限 |
| 最终日志/CPU | PASS，4994完成日志无严格assert/overrun/streamseek/未知类型/resource-map关闭诊断；py_compile 与 git diff --check PASS |
| 实际 UE 重放 | 未通过完整验收；normal-open身份guard API4，CPU65/pre-submit65动态heap-query闭包guard API4，均无GPUwait/初态上传/frameGPU。没有 UE 输出比较、事件往返/EID0 或完整参数绑定通过证据 |
| full78IR/75RT/308/frame-family/Qt-ARC/官方坏13 | NOT RUN；不继承旧库通过。用户 Qt/ForceCrash/系统重启具体根因未由本批证实 |

官方 MIT Apple sample 来源 https://docs-assets.developer.apple.com/published/ade36d76f1bb/AcceleratingRayTracingUsingMetal.zip ，ZIP SHA256 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`。复用已审查 cache，当前库重新构建/运行两 scene；许可 hash、构建和 seed/帧数见 `regressions/official/gate-results/manifest.json`。

## 新 UE 证据

UE5.8.3 CL58210709、B527隔离3mesh/2light工程、B537 provider；有限命令行开启 Lumen HardwareRayTracing/Inline，并核验实际cvar数值及来源。新 capture SHA256 `9359c9e20b13249ab786cd16c2b90010aad1d3f291a78c9760fd3e8208e3117c`，69862532bytes；原 capture/日志位于 `first/UE-capture/` 与 manifest 所记 owned session `20261007-003645`，等字节复制到 `final/UE-capture/`。运行库始终 a138d70a；之后仅修改fixture/oracle。owned cleanup editor -9 是有界结束，不计 UE 正常自行退出。

- 主 TLAS2559：kind9，offset0/stride72/type3/count3，216冻结bytes；GPU ID67/67/71、mask37/options6，精确对应 child6859/6863。source589824、scratch1376256、options544 placement。
- child6859：compact kind2，params `[0,12,30,48,0,0,1,0,0,0]`，有效顶点648、索引288bytes；child6863：compact kind2，2三角形，顶点48/索引12bytes。两子配方均存在，另两空TLAS为kind10/0bytes。没有为了修空packet错误伪造 null child。
- 60个 Lumen AIR 全部反汇编，0未验证条目；两 bound shader 真正包含 allocate/reset/next intersection-query 调用，间接group分别[2,66,1]/[132,1,1]。捕获96 direct+56 indirect compute；仅有label/普通compute不算光追。
- `UE-query-binding-facts.json` 记录查询 PSO4077/4141 的实际 binding：resource heap25/slot0、sampler heap26/slot1、root index2分别40/48bytes。它们是5/6个CBV/静态表指针的表，首指针来源4318+58624/161792；没有 typed heap-query consumer 声明。该证据只证明捕获绑定事实，不授予 sample 16byte root/4uint output 合同，也不证明 UE 光追输出正确。
- 64 heaps 共1491815424bytes，initial blob1127834757bytes；预检在 query namespace 前置 guard 拒绝，不能宣称总预算已通过。

## 保留失败与审计修正

首次 coverage3 frame geometry fixture 错用589824-byte Shared上传/验证buffer，触发原64KiB创建初态边界；保留 `first-failed-frame-triangle`，改该fixture使用原有小 placement输入，未放宽driver guard。首次 UE48 已native/API/CLI通过，但gate错误选择kind1而实际kind2；保留 `first-failed-ue48-compact`，修selector后全部复验。首次 freeze 审计脚本写错旧heap fixture目录名，保留 `first-failed-freeze.log`，修为实际四目录后freeze通过；这不是driver/GPU失败。

初态源变为足够大的已知Private scratch allocation，对kind1/2 packed owned数据仍合法；kind8要求整个VBO长度一致而拒绝。heap hazard metadata变untracked，原 replay creation 会增强 tracking，列为合法控制并验证强API/CLI结果。没有为让坏例通过增加伪拒绝。初态bulk反例执行时的script副本为 `heap-invalid-initial-acceptance.py`；后续只追加 coverage65 frozen-frame几何专用分支并另验32组，原bulk分支不变。

## 最终哈希与下一批

- backend / bundle / final冻结dylib：`a138d70a9bd28ca504c5d21bdee6cb91ab7602310ce1741160658649a20df051`
- GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`
- 复用provider：`aba8b52b92e43ffd7b64e146874db4d1de3b2a7d1ebd66b113dff089f507b1d1`

`final-manifest.json` 保存产物/source/manifest SHA与各项计数。下一B543按实际 Lumen 40/48byte CBV-root、typed query PSO、间接组及 descriptor GPU producer-consumer 支持闭包，先独立有限 sample 验证，再原生 UE 新capture/输出/事件/EID0；保留现有namespace/预算拒绝直至有完整证据。两个生产能力继续关闭，持续任务未完成，无需用户决定。
