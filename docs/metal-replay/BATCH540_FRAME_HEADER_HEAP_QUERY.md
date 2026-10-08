# B540 — 帧内 Header、当前 AS 构建与 kind3 heap-query

2026-10-06，PHASE59。完成已知 AS 对象的**帧内 64-byte Header 创建/发布、CPU kind3 槽位、当前 Private typed TLAS build 和实际 converted query**。实际 UE 的 Private placement AS 输入快照及 Lumen PSO/root/indirect/GPU producer契约未闭合，两项生产能力 false、持续目标 active。未提交/推送/重置，保留用户代码和安装引擎/原工程。

## 实现与对应接口

先查本仓库 DX12 `D3D12RTManager::PatchRayDispatch`、Vulkan `Serialise_vkCmdBuildAccelerationStructuresKHR`：沿用已知对象、typed build 输入/依赖、descriptor地址重定位与提交顺序；不读取 AS 内部，不提供 RT shader单步或 RT Pixel History。

- coverage65 的显式 heap-query namespace现在接受帧内 Header事实。新 Header 必须是实际已创建、非alias、Shared standalone/placement的64-byte buffer、offset0；声明拥有全部64字节。先验证创建/身份和已知 AS/贡献来源，才发布 current Header。没有合成未知padding或先分配未出生资源。已有背景 Header仍按完整初态恢复。
- Runtime声明按捕获的 frame write顺序核对，重定位 AS ID与贡献 GPUVA；只在原发布位置使新Header可见，等待先前提交的reader。预检拒绝未提交reader上的Header更新；CPU提交快照与当前声明逐字节匹配。预检 scope恢复raw/currentHeader/cursor，EID0恢复初态map，frame allocation通过既有birth生命周期释放/重建。
- Descriptor heap可以在初态尚无live slot，但仅在显式heap-query namespace、完整初态所有未声明槽位零值验证后接受；随后必须有实际dispatch和有效的原位置分配/write/kind3 binding。没有用帧内值替代初态。
- Slot frame预检接入已有冻结几何/间接TLAS build配方验证，维护独立current AS recipes；本批实际验证Private standalone instance输入。校验encoder生存期、scratch/input/输出/Header/贡献/table排斥、已提交reader、子AS当前配方及其build提交顺序。Query只能消费先前已提交的build，或同一command内先行的build。分配/AS ID本身不证明AS已构建。
- Runtime `RecordRayIRIndirectASBuild`/几何current map支持新heap-query namespace；旧coverage3仍使用相同typed验证。支持初态已构建目标重建、背景已分配但无初态配方的新TLAS目标，以及空初态→有效目标。未新增AS帧内分配支持。
- 有界Private零fill仅用于已知buffer、有效blit/range且排除query输出/Header/贡献/table，供真实sample构建后清零验证；typed frozen配方负责重放输入，不能依赖最终清零后的source内容。Shader未声明consumer、GPU Header/contribution写、未声明slot/GPU producer、未知PSO/root以及间接query dispatch仍拒绝。

新Header仅64-byte/offset0与本机UE13230/13233/13234实际分配相符。较大新Header/padding需要完整submission快照；没有把旧coverage3偏移场景的通过推广到新Header任意范围。Contribution backing仍≤16KiB；query AIR只读取AS ID，验证贡献地址/范围/全部字节，不宣称shader读贡献数组。coverage65几何解析复用旧方法，但本批新sample没有执行其几何build，不宣称新namespace多几何验收。

## 最终库上的 native→capture→replay

产物根 `build-macos-debug/metal-ray-b540/final/`，汇总 `final-manifest.json`。入口 `variants.py`、`old-heap.py`、`old-dynamic.py`、`regressions.py`、`lifecycle.py`、`preflight.py`、`freeze.py`。全部构建/GPU工作共用IR锁，有限超时/帧数、进程组清理，无UE/qrenderdoc并发。每份manifest记录DXC/converter版本、runtime头、DXIL/metallib、capture/helper/backend哈希及构建方法。

| 新场景 | TLAS | Header / 贡献 | 四uint输出，native/capture/API/CLI |
|---|---|---|---|
| new-shared | 背景分配、无初态配方，帧内build；1实例 | 新standalone64 / Shared offset0 | PASS，2258/2258/0/0 |
| new-placement-private64 | 新目标；64实例 | 新Shared placement64 / Private offset4096 | PASS，3329/3329/0/0 |
| same-private | 已初始化目标重建；1实例 | 新standalone64 / Private offset32 | PASS，2258/2258/0/0 |
| empty-placement | 空初态目标变为64实例 | 新Shared placement64 / Shared offset0 | PASS，3329/3329/0/0 |

四capture，**128次事件选择/EID0**。真实cs6_6 ResourceDescriptorHeap query AIR、捕获原metallib、每份一个非零query/一个frameTLAS build核验；先发布Header，再GPU上传Private输入、build、清零source，再query，符合UE factory先声明Header后build的顺序。强oracle验证输出、AS ID/贡献VA/HeaderVA变化、typed槽位、Header保留字与贡献全部字节、read/write usage，跨末事件、0、build前/build后、query前/query后往返。EID0的新Header无native分配，descriptor初态零；build/query前输出仍7，query后精确值，Private输入构建后为0。64实例最后UserID137。

重新编译/新捕获四旧B539初态heap-query sample（64事件），六旧B538动态Header场景/七capture（192事件），均native/capture/API/CLI PASS。主要sample合计15capture、384事件选择；不把负例合法控制或生命周期事件计入此数。

## 反例、旧回归和生命周期

| 最终测试目录 | 坏组 | API4/CLI1无GPUwait拒绝 | 合法强API/CLI控制 |
|---|---:|---:|---:|
| new64-bad | 80 | 160 | 1 |
| same-bad | 79 | 158 | 1 |
| empty-bad | 72 | 144 | 1 |
| static64-bad | 63 | 126 | 1 |
| old-dynamic-bad | 128 | 256 | 3 |
| 合计 | **422** | **844** | **7** |

新反例包含Header-before-birth、missing-birth/identity、late binding publication、错误Shared/length、缺build（仅无初态目标）、错误target/child/source/scratch/count/stride/snapshot/NaN/options、scratch偏移、未结束AS encoder、未提交build、query提前于build；以及kind3/PSO/root/inline、GPU Header/贡献改写和旧动态header反例。对已有初态目标，省略冗余rebuild可以合法查询旧AS，未把它列为应拒绝的坏例。全部成功导入后在GPUwait前拒绝。新Header与build支持后，旧“空AS encoder”不再当坏例，改为确实缺endEncoding。

47项相关旧回归PASS：B536六个17-format API、旧16-format两capture API/CLI、旧large R32三份API/CLI、新163初态纹理native/两capture/API/CLI（12696 subresource）、八B526 query API/CLI、B512 multi64 TraceRay API/CLI、六capability、官方Apple两scene270事件/10尺寸query/CLI loops3。官方MIT sample继续使用已审查ZIP pin `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`；来源 `https://docs-assets.developer.apple.com/published/ade36d76f1bb/AcceleratingRayTracingUsingMetal.zip`，本库实际结果与build/hashes见 `regressions/official/gate-results/manifest.json`。

两份新的frameHeader/query capture×10轮controller open/seek-reset/readback/close PASS，resident growth **458752 bytes**，保留64MiB上限；专用oracle另验精确值。最终2985份日志无严格断言/stream seek/overrun/map诊断，见 `final-manifest.json`。完整78IR/75RT/308、全量旧frame-family、Qt/ARC、官方13坏capture未运行。

## 实际 UE 新发现及下一批

本批复用B538 `c41abb06…`真实Lumen capture，没有启动新的UE capture。最终库normal-open/CPU65/pre-submit65仍三项API4、无GPUwait/初态上传/frameGPU，停在UE未声明的frameHeader/query contract；未证明UE输出、事件/EID0、总预算或完整GPU重放。生产flags保持false。

新增逐项提取的 `UE-build-facts.json`显示三次实际TLAS build：AS2559/2563/2566，输入3089/3094/3101，parameters分别 `[0,72,3,3,0,0,0,0]` / 两份count0；三个descriptorBytes皆0、children皆0。输入是**Private/Tracked placement**，options544、whole backing589824bytes；scratch也是Private placement、1376256bytes。这不同于本批有明确冻结bytes/knownchildren的standalone输入，不能用sample结果覆盖UE。现有Private typed入口排除heap输入，说明下一轮应优先补该实际捕获快照缺口，并检查AS ID引用的known-child闭包。

下一B541：基于仓库VK/DX12 build-input freezing，补Private placement输入的每次build快照及known-child来源；真实有限sample覆盖GPU上传/清零、3实例/空/64实例、事件和反例，再重新截UE帧检查实际快照。随后推进实际Lumen PSO/root/indirect与descriptor GPU producer。不得仅删guard、将无快照空children当有效3实例TLAS或提前全局启用能力。

## 保留失败、哈希

首backend build使用不存在的AS setLabel chunk、NS::Range默认构造导致失败；修正后backend/bundle build成功，保留 `first-failed-*build*.log`。首sample native/capture成功但旧PrepareDescriptorSlotShadow拒绝空初态shadow，保留 `first`；只对声明的heap-query加入完整零初态验证后重放通过。第二次API/CLI已通过但gate按旧双query计数断言失败，保留 `second`，修成新单query与无初态target审核后最终四场景完整PASS。未把这些中间失败计通过。

- Backend/bundle：`bfeca02b96c92d5152fd198e29306c5a2327d0581054835c747d60706149a8b5`；冻结 `final/librenderdoc.dylib`。
- GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。
- provider复用：`aba8b52b92e43ffd7b64e146874db4d1de3b2a7d1ebd66b113dff089f507b1d1`，未重编。

历史Qt viewItemSize/AS bridge ForceCrash/系统重启根因本批未确证；不声称已修复。
