# B510：间接 TLAS 的非零 scratch offset

接续 PHASE59/B509，补 UE 在 `MetalRayTracing.cpp:2459` 传入的 ScratchBufferOffset。该 bridge 分支原本直接 METAL_NOT_HOOKED；本批以样例实证补接口，尚未证明用户 UE 崩溃当时的具体 offset 值。生产 compute/render RT仍false，未运行UE，构建/GPU测试串行，未提交或推送。

## 依据与实现

DX12 `d3d12_serialise.cpp:330–348/1965` 用 D3D12BufferLocation 保存 scratch Buffer+Offset，并恢复 live GPU VA+offset；Vulkan `vk_acceleration_structure.cpp:944–965` 根据设备的 minAccelerationStructureScratchOffsetAlignment 分配 replay scratch。Metal 已有三角形路径校验256字节对齐及 length-offset，本批沿用该接口语义，不新增任意地址推断。

新增 `MTLAccelerationStructureCommandEncoder::buildIndirectInstancesWithScratchOffset` chunk，append enum保持所有旧ID；仅非零native offset选择新chunk。typed目标/子BLAS/实例/scratch和8个已有参数/冻结描述之后，独立序列化scratchOffset。旧零offset chunk不新增字段，旧捕获继续按原格式读。CPU structured-export、core replay、帧AS frozen补写和IR预检都处理新chunk。

间接TLAS的Shared/Private/空/屏蔽准备路径校验scratch对象、256对齐、offset<=length及实际buildScratchBufferSize<=length-offset，再调用Native build时传入offset。IR当前配方的scratch校验同样扣除偏移；几何配方仍独立于工作scratch，EID0恢复可自行分配scratch。当前只扩间接descriptor，UserID/direct/motion等其他分支不因本批被全局放开。

## 实际样例与保护区

新增八个真正 native→capture→replay 场景：Shared间接、Private间接、初始空、Private全屏蔽、Private帧内重建、初始空→Private帧内重建、heap-only帧内重建、global+heap+local六sampler帧内重建。scratch offset固定256，分配needed+256；在Native调用前用GPU把前方256字节填为0xa5，之后GPU拷贝读回并逐字节核验。读回buffer在捕获前创建；最终再读取一次保证初态scratch资源进入捕获闭包。

API oracle在每次EID0和三方向每个事件核验保护区，同时核验TraceRay输出、绑定、usage、强制AS-ID/VA/texture变化。帧内重建仍UserID73→74，输入清空后正确；初始空/屏蔽仍验证未命中→命中。保护区证明偏移真实生效，不只检查额外字段存在。

`metal_ir_ray_frame_invalid.py`兼容两个chunk，追加未对齐、offset=end、剩余空间不足、最大uint64与对齐后的大整数反例。合法新chunk offset0控制故意允许并验证原保护区被工作scratch使用，同时仍要求光追输出和事件正确；不是把256保护失败忽略。另UserID75、子BLAS ID重编号及同CB导入控制沿用完整oracle。offset0旧chunk仍有独立旧捕获与原58场景回归。

## 最终固定库验收

backend和bundle SHA256 `e11c809ea2484477bb5ec632fc7de06e5521e457b08fcb1521b2295ad7f3b344`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。构建、GPU、capture、replay均串行，起止hash相同。最终产物在 `build-macos-debug/metal-ray-b510/final/` 与 `captures/metal-ray-b510/final/<mode>/ir-runtime_capture.rdc`；manifest、validated-modes、remaining-manifest、source-binary-hashes、shutdown-log-audit分别保存实际结果和范围。

| 验证范围 | 实际结果 |
| --- | --- |
| 66个IR场景 native/capture/API/CLI三循环 | PASS，5421次主场景事件选择及EID0；新offset八场景1086次 |
| 新offset前方256字节0xa5保护区 | PASS，原生/捕获、EID0及每个事件逐字节检查 |
| IR损坏capture及合法控制 | PASS，1038拒绝/81控制；新四组offset frame 176/16 |
| 初始资源列表 | PASS，1个实际预分配上限拒绝/1份往返控制；外层错误分类差异未修改 |
| 旧targeted、argument、packet、B501/502 IR | PASS，18旧targeted、41旧argument反例、6旧packet、2份旧IR API/CLI；无注解IR仍明确拒绝 |
| 官方MIT Apple sample | PASS，2scene/270事件、10尺寸查询、13坏capture；固定seed1/4frames/64×64 |
| 能力查询 | PASS，6入口；生产RT仍false |
| 正常关闭/重开 | PASS，4capture×10，resident growth 1064960 bytes |
| 失败关闭、CPU导出、seek/reset | PASS，30失败+10成功、10CPU导出/30seek-reset，growth 393216 bytes |
| 最终日志与CPU | PASS，3283份日志无断言/overrun/未知资源/device关闭诊断；gate互斥、Python/Bash/diff检查 |
| UE/完整75 RT/完整308/原Qt crash/ARC提前释放/独立旧table | NOT RUN或NOT VALIDATED，不能继承旧库PASS |

官方缓存ZIP SHA256 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`；来源、许可、改动和构建方法随gate记录。wrapper已集成66场景及新反例，本轮按相同组件串行运行，未将wrapper整体调用计作通过。系统报告库存没有新增/修改的watchdog、panic、WindowServer、forceReset或UE相关报告；该结果不能证明旧冻结根因已修复。

首轮ea90库的首个非零Private frame、44/4反例及八新场景曾通过，但旧零offset失败；`first-offset/`、`first-offset-invalid/`、`before-legacy-macro-fix/`均为中间证据，未计最终PASS。e11c最终全部66及相关回归重新执行，没有继承旧库通过。

## 保留的实现失败

新增Frame frozen补写的条件SERIALISE_ELEMENT_LOCAL宏初次没有括号，宏展开的局部变量不在有效scope，编译失败；加完整条件块后通过，记录两份build-cmd日志。

新增8场景在ea90库通过，但旧零偏移帧失败：Serialise_buildIndirectInstances的条件SERIALISE_ELEMENT宏也展开多条语句，缺完整块导致其最后的Serialise仍执行。frozen旧chunk没有offset字节，reader把填充0xbb读作13527612320720337851，native准备函数正常拒绝，没有执行错误scratch构建。补齐块，并在ABI audit断言旧chunk不含新scratchOffset字段。`before-legacy-macro-fix/`保存8中间PASS及旧场景FAIL；e11c新库所有66模式及旧捕获重新验收通过；中间结果不计最终通过。零offset失败CLI诊断在 `zero-frame-diagnostic.log`，修复后的B509日志在 `legacy-b509-API.log`/`legacy-b509-CLI.log`。

## 下一步与未运行边界

实际UE未运行，也没有声称修复该次ForceCrash或原Qt sizeHint/系统冻结；旧WindowServer watchdog与强制重启根因仍未定位，不运行旧长UE/full followup。需要继续补帧内AS目标/几何/生命周期、必要Shader/IR producer关联与UE实际使用的布局。heap CBV/UAV/typed view、nested UB、大heap/工作量、Private/GPU IR、动态heap、必要SBT/callable/render RT仍未验收；AS内部查看、RT shader单步、RT Pixel History不承诺。只有sample、实际UE RT、相关最终回归和native能力判断全部达标后才启用能力，目标保持active。
