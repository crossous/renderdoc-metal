# B511：IR 帧内三角形几何构建

接续B510/PHASE59。本批补真实动态BLAS几何及IR当前配方，生产supportsRaytracing与supportsRaytracingFromRender仍false。构建/GPU串行，不运行旧长UE/full followup，不提交或推送。

## 对照与边界

DX12 `d3d12_command_list4_wrap.cpp:990–1127` 在loading及active replay重放BuildRaytracingAccelerationStructure，记录BuildAccStruct；typed地址与帧前recipe保持独立。Vulkan `wrappers/vk_cmd_funcs.cpp:8688–8770` 的vkCmdBuildAccelerationStructuresKHR保存几何/range并在相应事件重放。Metal现有IR preflight只允许间接TLAS，所有BLAS build chunks直接拒绝；已有AS encoder-end冻结代码只覆盖Private multi-indexed与indirect。

本批新增append `buildFrozenTriangles`，保持所有旧chunk字段/ID；仅active capture、usageNone、单三角形descriptor、Private/Tracked且无heap/alias的顶点与可选Private索引、Private/Tracked scratch采用新路径；其他Shared/heap/refit输入继续原入口，未将旧接口悄悄改为失败。保存typed target/source/index/scratch、kind1/2、原偏移/stride/格式/索引类型/flags、scratchOffset及有效快照bytes。encoder-end GPU复制在后续擦零前冻结，native完成后保持原stream位置/metadata补写。几何尺寸、256对齐/剩余scratch、输入范围/flags/索引及有限坐标先验证，之后独立Shared staging重建；staging仅由native callback持有，不捕获wrapper。IR preflight和实际replay更新current配方，EID0回到immutable initial配方；新BLAS目标可从未构建状态进入当前闭包。TLAS仍typed子BLAS/IFT/UserID而不猜raw地址。

六样例包含顶点/索引GPU upload、existing或new BLAS、heap-only与global+heap+local六sampler。原三角形命中x0，frame geometry平移100（heap场景-100）交换x0/x100命中，TLAS帧内UserID73→74；几何与实例输入建AS后擦零，检查真实TraceRay前后输出及所有事件往返/EID0。样例初始资源和最终输出独立，不能用只成功建AS冒充RT调度。

## 最终固定库验收

backend/bundle SHA256 `d15e1e529a2e7f1d2228d08cd9be4e6232e8b5823a4e45be226b89486b3dd040`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，起止hash一致；构建与GPU测试串行。最终证据在 `build-macos-debug/metal-ray-b511/final/`，capture在 `captures/metal-ray-b511/final/<mode>/ir-runtime_capture.rdc`。manifest、validated-modes、remaining-manifest、source-binary-hashes、shutdown-log-audit和CPU-final-checks记录具体范围。wrapper已集成72场景与新反例，本轮按相同组件串行执行，未将wrapper整体调用计作通过。

| 范围 | 实际结果 |
| --- | --- |
| 72个主IR场景 native/capture/API/CLI三循环 | PASS，6822事件选择/EID0；新几何六场景1401次 |
| 原生GPU输入与真正TraceRay | PASS，Private顶点72bytes/可选索引24bytes建BLAS后擦零，命中在x0/x100交换，TLAS UserID73→74；有效44/12byte快照与偏移/stride保持 |
| 损坏IR capture/合法控制 | PASS，1509拒绝/105控制；新几何471/24，含NaN、索引越界、坏scratch/flags/缺构建，合法原坐标、UserID75、child ID与同CB控制 |
| 新旧BLAS事件读usage | PASS，旧BLAS只在before dispatch、新BLAS只在after dispatch；EID0独立initial恢复 |
| 初始列表 | PASS，1个预分配上限拒绝/1份往返；原外层错误分类差异未改 |
| 旧targeted/argument/packet/B501/502 IR | PASS，18旧targeted、41旧argument反例、6旧packet、2旧IR API/CLI；无typed声明IR仍拒绝 |
| 官方MIT Apple sample | PASS，两scene/270事件、10尺寸查询、13坏capture；ZIP来源/许可/构建记录随gate保存 |
| 能力查询 | PASS，6入口；默认生产RT false，native支持单独核验 |
| 正常关闭/重开 | PASS，4capture×10，含global+heap+local indexed frame geometry，growth 1130496bytes |
| 失败关闭/CPU导出/seek-reset | PASS，30失败+10成功、10CPU导出/30seek-reset，包含新几何NaN失败；growth 344064bytes |
| 日志/CPU/hash | PASS，4387最终日志无断言/overrun/未知资源/device关闭诊断；gate锁/Python/Bash/diff、起止binary hash |
| 实际UE/完整75RT/完整308/原Qt crash/ARC早释/独立旧table | NOT RUN或NOT VALIDATED，不能继承旧库PASS |

官方缓存ZIP SHA256 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`。新几何测试采用本机UE5.8.3 DXC→Apple IR转换的真实TraceRay，明确typed fixture注解，不冒充自动UE producer支持。系统watchdog/panic/WindowServer/forceReset/UE报告库存核验单独记录；原系统冻结及用户ForceCrash/Qt崩溃仍未证明修复。

## 保留的中间失败与修复

首个b14f1d74库 native/capture/CPU ABI通过，API触发Metal断言：3×16stride需要48bytes，frozen有效bytes只有44。新的staging分配漏了已有InitialASUpload的最后stride填充；已修成有效bytes加stride-format零填充，原44bytewire快照不改变。首轮产物 `build-macos-debug/metal-ray-b511/first-geometry/` 与 `captures/metal-ray-b511/first-geometry/` 保留，不计最终PASS。初次库SHA256 `b14f1d745038992fc194cb038945fb8896bb61a7f819adcae1d089f185ecc5e6`。

修复后27d6961a首个样例及75坏capture/4合法控制通过；首轮新BLAS目标API输出11/147正确，但oracle错误要求旧BLAS在最后调度被读，已改为第一次读旧/第二次读新，且互相不得标记错误事件。oracle初次使用SDObject.children编译失败，改用NumChildren/GetChild；两组中间失败分别保留在before-usage-oracle-fix与before-oracle-api-fix。桥接路由另限定完整已验证输入属性与scratch，保护其他原接口行为；build-app首次误用qrenderdoc目标失败，按本仓库build-qrenderdoc重新构建通过，日志保留。

最终起点backend/bundle SHA256 `d15e1e529a2e7f1d2228d08cd9be4e6232e8b5823a4e45be226b89486b3dd040`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。新索引BLAS目标native/capture/API228事件/EID0/CLI通过，82坏capture/4控制通过，包含删掉唯一新目标构建即拒绝；childGPU-ID合法控制只修改对应旧身份的初始packet，不改无关旧BLAS。最终固定库证据见上一节，以上先验不计最终PASS。

中间产物不计最终PASS；最终固定库全部组件已重新验收。

## 尚未完成

实际UE RT、可靠producer布局关联、Private placement几何/mixed Shared索引、IR multi-geometry/refit/copy、heap CBV/UAV/view/nested UB/大工作量/Private GPU IR/动态heap等仍需按真实sample/UE与DX12/VK边界补齐；不因本批放宽未证明范围。不承诺AS内部查看、RT shader单步或RT Pixel History。用户ForceCrash、Qt sizeHint和旧系统冻结未证明修复；能力开启与目标完成仍需完整原始验收条件。
