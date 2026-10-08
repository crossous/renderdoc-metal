# M1 heap backing 状态与动态绑定恢复闭环

采用 [执行复盘](EXECUTION_REVIEW_2026-10-08.md)；本记录承载当前批次详细诊断/结果，旧 B544 记录保留。历史基线/API创建输入恢复 `568e50eb`保留。当前候选c88d10d4已完成第二份UE无损整帧、reset及query事件绑定，详细现状见文末按需控制输入批次；下面各旧节仅对应其hash。两生产RT flags仍false。

## 一页根因诊断

| 职责 | 已观测事实 | 未证明的事实/结论 |
|---|---|---|
| 对象/地址 | 旧 UE capture `403f90ea…` 中 buffer14714由 heap3138 placement factory创建，逻辑长度393216、offset2250752；VA90999511040及typed表25/slot20592身份已记录。预检已越过typed发布/来源检查 | 当前拒绝不来自缺对象或该typed地址身份；不意味着所有其他GPU地址已闭环 |
| 普通输入 | `ValidateDescriptorSlotFrame::validateRuntimeDispatch` 动态namespace分支要求每个非零候选至少有满足readBytes的 `bufferRestoredRanges`；当前14714为空。没有找到该对象初态/CPU输入/在前producer；后来的15262/15263 copy晚于消费 | 没有CPU producer证明不等于确实丢失已定义输入。尚无证据原shader实际访问14714；也未证明原heap字节曾被定义 |
| 物理 backing | 45个heap3138纹理的实际Native footprint审计未覆盖目标跨度，texture/AS不能作为raw buffer初态；MTLDevice创建零保证不适用于该heap factory | 仍须区分帧前旧alias定义、帧内原GPU producer、API未指定普通内容；禁止用默认零填补 |
| 提交 | 旧c573禁initial/frameGPU的45秒/3GiB诊断在commit4981760前API4，peak1296728064B，无GPU wait | 仅预检失败；没有UE整帧GPU输出。未来写入不能成为原输入，候选驻留不能成为实际读证据 |
| 展示 | dispatch1591744/PSO6399有真实AS闭包及237reads/4writes，动态索引未知、完整namespace保守候选 | 候选不展示为实际访问；optional数值/访问精度不是Native执行资格 |

**选择的可判别实验**：独立Native sample在捕获前用原GPU blit定义旧placement carrier，完成提交后使旧对象aliasable；捕获中在同backing非零offset创建新buffer，无冗余帧内upload，真实ray query动态读取。这样已定义帧前字节确实存在、旧owner不入帧，旧实现会丢失必要输入。新增出生点原字节记录/恢复应使其完整输出与合法独立组合一致；缺/短payload与错误offset仍必须拒绝。该实验验证一个真实机制，不预先宣称它解释UE14714。完成定向验证后重截UE检查该边界是否实际可观测；旧capture不会因capture侧修复自动具备新信息。

## DX12/Vulkan 数据流对照卡

| 职责 | 本仓库具体实现及行为 | Metal复用/差异 |
|---|---|---|
| 捕获/初态 | `D3D12ResourceManager::Prepare_InitialState` 对Resource_Heap读取GetUnwrappedWholeMemBuffer；`Apply_InitialState` Copy/ForceCopy恢复真实backing。`WrappedVulkan::Prepare_InitialState` 保存device memory；`Apply_InitialState(eResDeviceMemory)`按MemRefs/InitReq Copy/Clear及wholeMemBuf恢复 | 物理字节与逻辑alias/typed地址分离。Metal没有可raw读取整heap的公开buffer；只在真实buffer合法出生点复制其逻辑范围，texture/AS opaque内存不用作buffer数据 |
| 地址/descriptor | `WrappedID3D12Resource::GetResIDFromAddr`保存身份/offset；`CopyDescriptorsSimple`及`D3D12Descriptor::GetRefIDs`复制typed描述并追踪引用。VK `Serialise_vkBindBufferMemory`保存memory/offset；descriptor对象绑定与device address重建独立于普通数值 | 现有typed24 ABI namespace保存身份、offset、发布和重放地址；新raw普通出生字节不当作typed GPU指针证明，不扩CPU数值解释 |
| 执行 | `Serialise_Dispatch`/`Serialise_vkCmdDispatch`恢复对象/参数后直接调用原Native调度 | Native shader执行原GPU计算；出生状态恢复不用CPU模拟query/index |
| 依赖 | `Serialise_ExecuteCommandLists` DataUploadSync/队列切换等待；`Serialise_vkQueueSubmit`保存提交、等待并序列化队列依赖 | 出生观察只在此前GPU已完成且捕获帧尚未提交使用该heap时进行，避免等待应用回调或用后来的结果补早期输入；seek重执行出生chunk |
| 展示 | `D3D12Replay::GetDescriptorAccess`追加有效dynamic feedback到staticaccess；`VulkanReplay::GetDescriptorAccess`从pipeline staticaccess/可用feedback展示 | namespace恢复候选不冒充actualaccess；展示仍partial。当前普通字节资格仍有CPU区间依赖，尚需按真实producer/未定义内容证据整改，不能声称已全部解耦 |

VK中mid-frame无srcBuf转Clear是其初始化策略，并非Metal所有heap内容保证。没有原已定义数据时，不能据此制造逐字节相同证明。

## 当前机制/边界

新增append-only `MTLBuffer::CaptureHeapBirthContents`：实际Private placement buffer出生后、返回应用前记录Native逻辑字节及buffer/heap/offset；现有typed协议兼容，不新增coverage许可等级。重放验证真实factory、length/offset、当前生命周期、先前heap提交使用及统一预算，再复制原bytes。保留单次16MiB staging与既有128MiB帧预算，不扩大数字上限。

若前GPU尚未完成或该heap在本捕获帧已提交使用，则不采样、不阻塞应用等待，仍走原初态/producer路径；这是当前观察机制边界，是否适用于异步UE待验证。raw snapshot不能证明其中GPU指针已重定位。新出生chunk发生在消费前，不是消费后冻结。普通shader写入由Native生成。

## 结果入口

外盘 `build-macos-debug/metal-ray-b544/development/heap-birth-T1-key/manifest.json` 与独立组合/UE manifest；结果完成后在此汇总。构建失败日志保留：shadow local、protected resource-reference访问、chunk count assertion；对应源码已修正，最终build实际结果单列。未运行的UE/最终官方矩阵不计通过。

## 第一次UE实验与方法切换

`7bc84b24` 的真实出生字节机制：T1原例和独立12288B/heapOffset768/readOffset12280/Header128/贡献32/输出24组合，Native完整已定义字节、真实query输出、capture/API/CLI及事件往返/EID0通过；缺/短真实payload和错误offset在GPU前拒绝。较早构建和测试仅历史证据，不作为后续库通过。

使用原隔离MetalRayScene与MetalRHI重截有限帧，capture `164a95ab…` 包含非零HW ray query调度，**没有出生快照chunk**。同45秒/3GiB预检API4、peak1806073856B、无GPUwait/strict；拒绝在新动态候选14696（heap5898/offset2849792/length393216，table25/slot21312），不是实际读取证据。Native74个纹理footprint查询及记录中既有buffer未发现出生前目标跨度重叠，但已退休帧前对象未必在文件中，不能由此直接认定全新。

两次capture侧相关调整未解除同类语义阻塞，按复盘切换方法：保留真实帧前alias输入机制，另区分API未规定的普通内容，不继续靠邻近sample或CPU shader范围证明。

**实际新增机制**：每个真实heap生命周期保存所有成功buffer/texture/AS分配的Native物理footprint并合并区间；记录覆盖已退休对象，capture开始不清历史，同heapfactory与历史更新串行。只有无任何旧owner覆盖的Private placement新范围可记录append-only `CaptureHeapBirthUnspecified`；它表示API无原定义字节，**不产生初始化区间、不猜零、不复制数值**。opaque texture/AS只是否定fresh的占用证据，不是buffer内容。旧alias仍要求真实出生字节或原生产链。preflight确认factory/identity/offset/live/物理不重叠后，普通动态候选不再因CPU初始化证明为空永久拒绝；typed指针/句柄、必要payload、发布和提交校验独立保留。raw字节chunk原格式保持兼容。

T1独立组合增加17个未初始化且未实际读取的合法候选，在原生shader未知动态索引下保持完整query输出，验证候选≠实际访问且API未指定普通内容≠损坏。中间候选通过；最终候选的验证与UE结果见以下更新。

直接相关旧负例重新分类：删除ordinary upload/dispatch、把producer移晚、以未规定ordinary源覆盖、让动态窗口触及无定义内容不天然构成malformed。测试生成器不再要求这些数值/时序变体保持旧API4；历史失败保留。真实缺typed来源、缺必需定义payload、无效factory/view/地址/lifetime等负例保留。部分变体的Native输出对照及普通状态传播仍需后续机制，不能把“不再用作拒绝oracle”写成已支持或通过。

## 修正“无快照”的归因：实际coverage捕获门控

第二份UE `0c5e764d…` 在 `180b1169` 仍无出生状态chunk，预检在候选14716/table25/slot19584的同类内容分支API4，peak1799274496B，无GPUwait/strict；未执行整帧。核对生产代码与捕获记录后发现，UE provider刻意不声明coverage，capture侧 `m_DescriptorCoverage` 为0；新增采集函数却以 `<4` 退出。**因此前两份UE实验不能证明异步队列或heap历史边界阻止了采集。先前归因仅假设，已纠正，不能作为永久限制。** Native样例声明65掩盖了这一通用捕获路径差异。

实际移除capture侧coverage许可条件。API出生状态采集不依赖descriptor许可；typed重放仍要求完整预检map，普通legacy重放依据实际对象/parent/offset/length/storage与fresh物理不重叠直接验证记录，缺/短真实payload不补零。保留旧raw出生chunk格式。Native footprint缺失时永久使本heap历史“不完整”，不能把后续范围误判fresh。下一轮必须以不声明coverage的独立样例和fresh UE核验此结论，不能继续将原无状态capture反复打开。

## 当前候选结果与剩余义务

固定backend/bundle `062175ef9115e238bb984cebc4511bf47d7d4ad447ae43e2fc61a04d20699c27`。T0构建及diff检查通过；`heap-state-generic-full-T1`独立12288B/offset768/read12280/Header128/贡献32/输出24+17个未初始化候选，Native完整已定义输入、ray query真实输出、capture/API/CLI、事件往返/EID0与真实定义payload/地址破坏拒绝通过。`heap-state-generic-no-coverage`覆盖不声明coverage的Native/capture与禁GPU恢复预检通过，**该输入本身未做完整GPU重放，不计默认UE准入**。`heap-state-generic-T2-compat`核验旧raw出生chunk输入在当前库的完整输出/事件和CLI；结果以manifest为准。

UE fresh capture `12e9326dd3a90e8ec5d90882db58049b4268980ba6ddb3617ee0e94bc7ceba2b`，实际保存5份旧backing出生byte状态，真实HW ray query调度已审计。预检在约1秒/peak50413568B、无GPUwait/strict时被新API投影缺口阻止：frame source15481 `Depth32Float_Stencil8`，view15512 `X32_Stencil8`，合法320×240/2D/level0 count1/slice0 count1/Private/PixelFormatView。`metal_texture.cpp::ValidTextureView`已有Native stencil aspect规则，但 `ProjectMetalTextureView` 的统一descriptor投影未共享它；**不是malformed，也没有证据heap当前消费闭包已通过**，本capture尚未到原动态候选消费点。先前两次无状态capture保持为历史失败，不能混为当前库结果。

当前 M1仍进行中，M2–M4未达标。下一项可改变结论的动作：按DX12 SRV PlaneSlice/typed depth-stencil view及Vulkan image-view aspectMask对照，共享Metal Native aspect projection，使用独立深度/模板sample尺寸/array/subrange合法组合验证；然后复用此fresh UE capture推进同一预检。若回到heap动态候选仍无输入，先诊断实际出生条件/旧owner/原GPU producer义务，不继续扩大CPU数值表达式或同类样例。native部分路径coverage仍是历史临时许可，尚未全面清除；普通条件/ranged producer/alias unspecified状态传播也尚未闭环，不能把本批范围外路径称支持。

本候选未运行：普通未声明typed协议capture的完整GPU legacy路径、当前hash官方RTsample/最终全套IR/RT/生命周期/Qt/ARC/native caps，以及UE整帧输出/事件/EID0/绑定。RT生产两flags仍false，未提前启用。测试数不作进度，进度为真实出生输入恢复、fresh ordinary状态分类和捕获侧coverage门控移除。

最终产品：backend及bundle SHA256 `062175ef9115e238bb984cebc4511bf47d7d4ad447ae43e2fc61a04d20699c27`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。当前完整结果/源码与产物哈希索引为外盘 `development/heap-state-generic-evidence.json`，冻结检查点 `checkpoint-heap-state-generic-062175ef`。持续任务提示已按复盘替换并读回核验，仍ACTIVE/hourly、保留原目标chat；简短规则持续覆盖旧场景策略。所有新捕获与中间文件写外盘；内部仍28GiB可用，未覆盖用户RDHeaderView改动、未重置/提交/推送。唯一原始/失败证据保留，不为清理重跑构建或复制整批日志到多个文档。


## 共享 aspect/父资源恢复闭环（当前候选 e4e8568c）

**实际根因与修改**：`ProjectMetalTextureView`只允许同格式，虽然Native `ValidTextureView`已有DS32S8→X32S8规则，frame-view扫描仍拒绝合法UE view15512。Native与预检现在共用API投影，记录真实view格式及mip/slice范围；whole/subset/swizzle调用都经相同对象/范围检查，不按尺寸或UE名称放行。stencil逻辑布局是一字节aspect，不能成为独立allocation或buffer texel格式。Native readback按parentRelativeLevel/Slice映射父资源，再提取真实stencil平面，替代full2D/full-mip限制。当前独立GPU验收覆盖subset API，whole/swizzle在本候选未独立GPU验收，不把共用代码视作其完整通过。

`ValidateDescriptorSlotFrame`的Native AIR数值分类将S8的shader unsigned ABI与RenderDoc的Depth展示类别分开；父资源真实shader writer/initial/clear及已提交依赖沿view共享。depth/stencil clear保留plane/mip/slice身份，深度清除不能证明stencil输入，投影外clear不能证明view输入。深度attachment改用已支持布局/真实subresource/RenderTarget条件，移除2D/512/单mip和level/slice=0场景限制；单sample、范围、usage、预算和原同步仍保留。普通GPU数值仍由原shader执行，accessDisplay=partial不阻止重放。

独立样例另暴露 `PrepareDescriptorSlotShadow`把初始shadow为空拒绝，即使table全零且首次合法typed发布在frame资源出生之后；已改为核验实际initial bytes、消费前slot/source/publication。不是添加无关背景槽来取得许可。首次失败保存在stencil-aspect-T1-first；修复后的Native/capture/API/CLI与事件恢复已通过。旧`stencil-level`负例把合法count3→2当坏数据，已改成真正超parent的count4；旧失败留存，相关旧大gate本候选未全量跑。

**本批DX12/Vulkan对照补充**：`WrappedID3D12Device::CreateShaderResourceView`以原SRV描述创建Native view并捕获DynamicDescriptorWrite；`D3D12Descriptor::Init(resource,SRVdesc)`保存父ID和完整typed描述，`GetRefIDs`引用父Resource。`Serialise_DynamicDescriptorWrite`重建Native descriptor，不复制独立view初态。`WrappedVulkan::Serialise_vkCreateImageView`保存/unwrap原image及subresourceRange/aspectMask后Native创建；capture record AddParent(imageRecord)。`WrappedVulkan::Prepare_InitialState(eResImage)`分别CopyImageToBuffer depth和stencil planes，Apply_InitialState的对应copy恢复实际父image。Metal复用父身份、原API view、父初态/producer与原queue提交；差异是Native X32S8 shader ABI及parentRelative mip/slice需要显式投影，RenderDoc读回用已支持的8byte D32S8表示提取1byte stencil，而非raw读取opaque footprint。地址依旧由现有typedSlot identity/offset重定位；执行/提交/optional feedback职责延续上面对照卡，没有引入CPU shader值证明。

Apple官方API契约归档外盘stencil-aspect-view-contract.md、stencil-aspect-pixel-format-usage.md（其SHA见当前manifest）。sRGB/linear转换无额外PixelFormatView要求；首次通用投影错误把此标记应用于已有BGRA sRGB→linear，UE background加载因此失败，stencil-aspect-UE-pre-submit保留。按官方契约纠正后当前UE越过该处。未扩大未实现的D24/MSAA/nested view或一般跨色彩布局恢复范围；这些机制未完整验收，不宣称全Metal view API可用。

**真实结果**：T0 backend/bundle build与diff检查通过。`stencil-aspect-T1-final/manifest.json`：320×240/2D，以及独立127×73/array3/parent4mips/view base1 count2/slice base1 count2，各两阶段真实clear→动态unsigned stencil read，后者跨提交；Native八GPU words、完整projected stencil pixels、capture/API/CLI与事件往返/EID0通过。actual AIR texture read=3、accessDisplay=partial已接受。缺父、越mip/slice、错aspect、缺必要view usage、aspect冒充allocation均GPU前拒绝，没有默认填零或跳过address/source条件。

`stencil-aspect-T2-final/manifest.json`：当前库重放既有heap出生状态真实ray query（完整输出/48事件/EID0/公共绑定）、Native JIT color multiview（unknown展示）和MRT depth/stencil render（完整256pixels与32事件），API/CLI通过。T2第一次runner遗漏query偏移/dynamic环境，导致oracle错误；其失败保留，修正为原fixture参数后通过，后端未因此改动。不将旧capture的Native/capture侧hash计作当前库Native验收。

**UE当前结论**：同capture `12e9326d…` 在当前库完成约31秒禁GPU预检、peak1609302016B，无GPUwait/strict，越过原view拒绝，但在commit4824640、runtime consumer3584704/PSO6409拒绝：table25/slot18336的buffer15298无普通状态资格。实际对象heap5929/offset2588672/393216bytes、VA92520742912及typed发布都有记录；target仅出现factory/identity/binding，尚无实际shader访问证据。帧内在其出生30391之前有三个commit30375–30377，含同heap其他buffer15262/15264/15265/15268/15280的真实小范围blit。Native71 texture footprint与序列化110 buffer factory审计没有发现较早记录对象覆盖target；**这不证明已退休背景owner不存在或API原bytes被定义**。现有capture helper对整个heap的任何既往提交引用立即跳过raw birth，早于pending GPU状态检查；这是可见的保守机制边界，不得把async或旧owner假设写成已证实根因。详见stencil-aspect-next-heap-audit/diagnosis.json。

下一可判别实验是同heap已完成的disjoint prefix提交→新逻辑buffer出生→真实query读取；出生状态应只在实际出生位置应用，不补任何早期consumer。先核验整heap阻断是否应由对象/physical范围及实际同步义务替代；再记录UE实际skip原因，不能直接删除pending GPU/输入/address检查或无限重截同一失败。M1尚未闭环，M2整帧GPU输出未取得，生产两RT flags仍false；UE后续GPU/events/bindings、官方与最终集中IR/RT/生命周期/Qt/ARC/native caps均未验。

唯一当前详细证据索引为外盘development/stencil-aspect-evidence.json，产品/源码检查点为checkpoint-stencil-aspect-e4e8568c; 历史062175ef与中间3e1de5f结果各自保留。新产物全外盘，内盘约28GiB；没有重置、提交/推送或覆盖用户RDHeaderView。持续规则/automation已采用复盘并保持稳定，不为此子修复新增场景许可或提示条款。


## 出生点恢复与已完成前缀（当前候选 0074fa43）

**根因证伪**：独立 Native sample 在同一 heap 的无重叠范围提交真实 4-byte blit 并等待完成，然后在旧 backing 创建新的 Private placement buffer，真实 ray query 读取帧前已定义字。旧 e4 候选 Native/capture 输出正确，但整 heap 既往引用门控漏记出生 payload、API在GPU前拒绝。baseline失败保留在 heap-prefix-baseline。它证明这一机制缺口，不证明此前 UE 每个候选实际被 shader 访问。

**实际通用修改**：CaptureHeapBufferBirth 以新逻辑对象的出生边界判断，不再因同 heap 无关较早提交拒绝；仍要求此前 Native GPU 已完成且无 error，原 bytes 立即在 factory 返回应用前保存，保留单次/整帧预算。preflight 核验 factory/parent/offset/length/live/单份 payload，以及该对象尚未编码使用；Raw出生数据在流中实际位置才产生 restored range，不补此前消费者。Serialise_CaptureHeapBirthContents 在内部上传前只等待此前已提交 replay command buffers，重建原 capture 观察到的完成边界，不提交仍在编码的命令、不结束应用encoder。fresh unspecified仍不上传/猜零；raw payload不成为typed指针重定位证明。trace记录实际采集结果，诊断不是资格条件。

**对照卡更新，覆盖上方旧整 heap 限制**：D3D12ResourceManager::Prepare_InitialState/Apply_InitialState(Resource_Heap)使用WholeMemBuffer保存/恢复物理 backing；WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists 的DataUploadSync及队列切换等待隔离上传与原执行。WrappedVulkan::Prepare_InitialState/Apply_InitialState(eResDeviceMemory)使用wholeMemBuf与MemRefs/InitReq保存/复制原内存，Serialise_vkQueueSubmit保留提交及队列等待。Metal没有公开整 heap raw buffer，因此原对象出生字节在真实 stream 出生点恢复，并只等待已提交前缀；不能当作较早别名初态。现有typed资源身份/offset/发布负责地址，原dispatch执行负责GPU数值，optional展示仍partial/unknown。原卡“本帧该heap未被提交使用”及整heap门控描述现在仅为历史候选边界，不再是当前要求。

**固定候选真实结果**：backend/bundle `0074fa43c1c31e7af8800564e1e1fa849ec05c23b09be4f918d824d4376b2a5d`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。T0最终构建/diff通过。heap-prefix-T1-key与独立heap-prefix-T1-independent：原组合12288/read12280/heapOffset768/输出24/实例3及合法65536/read65532/heapOffset1024/输出8/实例2组合，真实完成前缀、完整出生输入、Native/capture/query完整输出、API/CLI与事件往返/EID0通过；保留17或3个未实际读取冷候选，partial展示可接受。heap-prefix-contract：真正缺已定义出生输入、offset不匹配、实际ZIP payload短1byte、出生记录移至对象已编码使用后，在API/CLI的GPU前拒绝；不把普通upload/dispatch变化一概当损坏。heap-prefix-T2仅当前库重放旧heap RT、JIT multiview、MRT depth/stencil和两份stencil aspect captures，全输出/事件/EID0及CLI通过，历史capture/Native hash不计当前capture侧通过。未全量重跑旧矩阵。

**真实 UE 阶段**：新 capture `7bb9c524e5b28975c8b6d3e85dcb2c32a80622089812fa069f3609321f05d954`，实际非零HW ray query已审计。出生日志312条：93份original-birth-bytes、7份prior-GPU-not-completed、212份factory-or-budget；这不是所有skip都可忽略的结论。约22秒/peak1706229760B禁GPU预检越过之前heap候选，下一在render encoder12633前拒绝，未运行initial GPU/整帧GPU。实际附件9311/9312都是背景Private/Tracked `TextureType3D`、RGBA16Float、64³、初态已保存；旧逻辑只准帧内同格式volume layered attachment，背景volume虽恢复却被type=2D/特定cube条件拒绝。初步称数组的描述已以实际factory核对纠正为3D volume；renderTargetArrayLength64是render pass层数。编号仅定位测试，不用于生产资格。

下一在相同通用恢复批次按真实API附件类型、mip/slice/depth-plane和原初态/生产链处理背景与帧内layered附件，不扩尺寸/格式清单、不新增coverage等级；先独立Native合法变体，再复用新UEcapture推进。异步出生观察边界仍保留，未证明这些7份skip是新阻塞根因。M1仍active；M2真实整帧GPU输出、M3与M4未完成，生产两RT flagsfalse。当前官方/最终集中回归、设备判定及UE GPU输出/事件/绑定未跑，不计通过。

唯一当前证据索引development/heap-prefix-evidence.json、检查点checkpoint-heap-prefix-0074fa43；原失败及历史e4/062保留，所有新产物外盘。稳定AGENTS/持续提示已采用复盘，无需为每个诊断修改持久许可规则。无reset/提交/推送，用户RDHeaderView未覆盖。


## 附件布局与原始状态共享（当前候选 74989823）

**真实根因**：fresh UE `7bb9c524…` 的背景RGBA16Float/64³ volume已有完整初态，旧preflight却把layered资格限于帧内同格式volume；initialColorTarget再限2D/特定cube。独立Native volume样例完整定义texels，原Load→部分/完整layeredClear正确，旧0074库API/CLI在GPU前拒绝；baseline保留layered-attachment-baseline。非零mip、slice与depth-plane的另两组Native合法组合在生产修改前已经通过。

**通用机制**：render pass color与depth使用同一attachmentLayout，从实际背景Native对象或已验证frame factory读取布局；公用MetalTextureReplayLayout及真实RenderTarget usage、单sample、lifetime与统一预算。width/height按实际mip，layers按当前array/cube slices或3D mip depth及所选slice/depthPlane计算，用减法校验范围避免溢出。删除按coverage授予2/8192尺寸、64层及RGBA16Float帧内volume许可，未扩大统一layout预算。色彩资格采用已有可读回uncompressed颜色布局族，Native对象/PSO承担设备格式合法性；不再用2D/特定cube/格式列表区分原初态恢复。Load/DontCare保持原API，帧前Load仍需真实初态/在前原命令，帧内未规定ordinary像素不造零；当前drawable保留实际producer要求，resolve/MSAA/tile等未实现机制不借此声称支持。Unknown store仍由原encoder setter恢复。部分mip/layer clear不再成为whole-image clear事实，depth/stencil明确记录每个实际层，畸形巨大layer数在geometry失败后不会进入循环。

**具体对照**：WrappedID3D12GraphicsCommandList::Serialise_OMSetRenderTargets直接捕获被消费的D3D12Descriptor，GetTempDescriptor重建RTV/DSV并Native绑定，保留view的mip/3D范围；D3D12ResourceManager::Prepare_InitialState/Apply_InitialState用placed footprints/CopyTextureRegion恢复真实各subresource。WrappedVulkan::Serialise_vkCmdBeginRenderPass unwrap framebuffer/renderpass；Serialise_vkCmdBeginRendering序列化完整RenderingInfo并恢复layerCount、附件image view与renderArea，Native执行；Prepare_InitialState(eResImage)按mip/layer复制原image。两者将父资源原状态和descriptor view分开，不用引擎名称授予支持。Metal复用真实父资源/Native view身份及原初态/命令/提交，增加原生depthPlane/array-length geometry；原GPU handles/VA仍沿既有typed身份和offset重定位，CPU不求shader数值，展示partial。提交沿已有command lifecycle、依赖与seek重建，不更改shader/pass名称或PSO/EID资格。Apple官方renderTargetArrayLength/depthPlane契约归档外盘，0层数按非layered一层检查；depthPlane只用于3D，其余color附件该字段不作场景限制。

**当前真实验证**：backend/bundle `74989823e96d38e058d4ce25e133cdf3d2e0a71d084fb928882fa4951359e5c9`，GUI仍3ba30e36。T0构建/diff通过。layered-attachment-T1三组Native/capture/API/CLI：背景RGBA16Float volume16×12×8/2mips；独立帧内RGBA8 array29×17/5层/3mips，mip1 slice1 layers2；独立背景R32Float volume21×13×10/3mips，mip1 depthPlane1 layers3。完整已定义初态、Load后内容、选中及未选中mip/层/plane、六次事件往返/EID0与deferredStore均通过。该样例只验证通用附件API，不宣称RT或UE整帧通过。

layered-attachment-contract真正越layers、uint64溢出层数、越mip/slice/当前mip extent、缺必要背景初态及实际ZIP初态短1byte，在API/CLI的GPU前拒绝。layered-attachment-T2用当前库重放旧真实heap RT、JIT multiview、MRT depth/stencil及两stencil aspect文件，完整输出/事件/EID0与CLI通过；不继承旧Native/capture hash为新库捕获验证。未全量扩大拒绝矩阵。

**UE下一根因，未绕过**：当前禁GPU预检约31秒/peak1759772672B、无wait/strict，已经越过volume附件，下一commit8440832在runtimeconsumer7199808/PSO6443的动态buffer namespace缺table25/slot53832 publication。实际背景generation4507/type4有allocate→publish→binding(buffer11086)→retire，均在捕获scope之前；source11086无factory/identity/initial进入文件，原table initial/原publish仍有非零旧pointer91418487808/length32768。这里不是普通零数据或可删除的live缺来源检查，也没有实际shader访问该退休行的证据。现有OverlayDescriptorSlotBuffer在initialRestore已经清除明确退休行的GPU字段，但nullBufferFields预检只读未patch的原始初态，snapshot又只带live行，可能使实际已重建invalid的行被当成未恢复live namespace；待独立判别，不把它写成已修复。

后续对照已实际核对：D3D12Descriptor::Create(SRV)对未引用/删除源使用default null Native descriptor；Vulkan Serialise_InitialState(eResDescriptorSet)在重建writes时跳过stale/unreferenced invalid slots。Metal raw ABI没有可保留的旧地址，必须区分明确退休且Native初始化已invalidated的行与仍live但缺必需source/initial的行，并证明mapped-memory更新没有重新引入旧VA。不能直接跳过所有missing slots、默认零掩盖活跃必要输入或仅凭CPU不知索引永久拒绝。下一先独立真实query复现前述状态差异，再决定通用机制；当前仍未取得UE整帧GPU输出，生产RT false，M1 active/M2–M4未完成。

唯一证据索引development/layered-attachment-evidence.json与checkpoint-layered-attachment-74989823。官方/最终矩阵/设备判定及UE GPU输出、事件、绑定未跑，不计通过；产物全外盘，保留未提交工作与原失败，无reset/提交/推送。


## 初始化失效身份与动态 namespace 一致性（当前候选 d5336f15）

**判别与根因**：独立Native真实query在scope前allocate→publish→bind→retire一行，释放原source；原24byte行仍有非零旧VA与length256，query动态读取其他有效行。7498 baseline Native/capture正确，预检却因该退休行缺publication拒绝（retired-namespace-baseline）。代码核实OverlayDescriptorSlotBuffer(initialRestore=true)早已按明确退休/匹配generation的leading retirement清除声明的GPU字段，保留普通metadata；nullBufferFields却从未patch原始initial读取非零旧VA。失效Native初态与预检namespace事实不一致，不是已定义活跃输入丢失，也没有实际访问退休行的证据。

**实际修改**：ValidateDescriptorSlotFrame初始化null bitmap同时采用m_DescriptorSlotInitial的明确dead状态或IsDescriptorPreludeRetirement的有效leading retirement，与已有Native初态overlay一致。没有新增“缺source填零”，没有改Native执行或frame retirement：帧内退休本身不改物理bytes、不产生null事实；CPU/GPU后写仍使旧事实失效，重新引入非零VA仍要求实际身份/offset/发布/重定位。活跃槽的必要初态、地址、生产者、提交和生命周期检查保留。此规则依据真实协议/恢复状态，不依据slot数值、UE名称、EID、PSO或coverage许可。

**仓库对照卡补充**：D3D12Descriptor::Create(SRV)对unused或deleted source用default null Native SRV（AS/CBV各自路径不借此放行）；Vulkan Serialise_InitialState(eResDescriptorSet)重建writes时跳过stale/unreferenced invalid slots，descriptor initial不把所有历史source都永久FrameReference。两者初态描述的是当前有效资源身份，并非保持删除对象旧地址。Metal raw ABI需要初始化overlay清除已失效地址字段，保留ordinary length；本次预检同步实际overlay事实。原捕获typed事件保存generation/source/offset，活跃身份沿既有typed重定位，真实提交snapshot及后写失效路径不变，原shader执行动态访问，展示partial不冒充实际读证据。这里只处理明确失效初始generation，不声明所有退休行或未知指针都可置空。

**当前候选验证**：backend/bundle SHA256 d5336f1523bf945affa6f57b5dae879426af58bf15441cd9979e366f3520a093，GUI 3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。T0最终构建/diff通过。retired-namespace-T1-key真实query：12288B/read12280/heapOffset768/Header128/贡献32/输出24/实例3；独立组合65536/read65532/heapOffset1024/Header64/贡献16/输出8/实例2，17冷候选与已完成disjoint heap prefix。两项Native/capture/API/CLI完整真实输出、定义出生输入、48/56事件往返/EID0及公共绑定通过；每个恢复事件还读取Native namespace，失效行只GPU字段为0、length256保持。原Native非零旧VA证据保留，没有对API未规定ordinary字节建立oracle。

retired-namespace-contract：错误retirement generation、删除实际retirement使缺source变live、删除活跃binding、提交前真实CPU payload重新写入旧VA，都API4/CLI1且无GPUwait/strict；同样CPU更新写显式8byte空地址的合法对照API/CLI与完整query/events/EID0通过。真实binary ZIP已改，不用XML长度假变更。retired-namespace-T2用当前库重放旧heap RT、JIT multiview、MRT depth/stencil、两stencil aspect及三layered attachment captures，完整输出/事件/EID0与CLI通过；不将历史capture的Native侧hash计本库捕获验收。未全量重跑旧矩阵。runner未记录完整T1/T2墙钟耗时，不能补造数据。

**UE实际阶段**：复用原RT capture7bb9c524e5b28975c8b6d3e85dcb2c32a80622089812fa069f3609321f05d954，45秒/3GiB禁GPU诊断约34秒结束，peak1370963968B，无GPUwait/strict。已越过table25/slot53832退休行缺publication，consumer7199808/PSO6443真实AS2/237reads/4writes的restoration accepted；后续7204352/7212672也accepted。下一consumer7219648/PSO6460 unresolvedCalls2，commit8440832仍API4；不能写成UE整帧通过。

实际库5717/function5718的原AIR审计发现两次air.fence_texture_2d，分别有typed texture operand；其余数值/texture/barrier调用已有分类。这是当前同步语义分类缺口，尚未删除检查或声称安全。下一沿原纹理身份/生命周期及Native GPU排序恢复处理texture fence，不将它当CPU普通表达式；用独立Native read_write texture写→fence→读和合法view/尺寸/offset变体验证后复用当前UE。名称/ID仅测试定位。若fence身份无法恢复仍需修复，不能把未知调用一概放行。

M1仍active，M2真实整帧GPU输出未取得，M3/M4未完成，两生产RT flagsfalse。当前官方sample/固定最终集中IR-RT-生命周期-Qt/ARC及native caps、UE整帧输出/事件/绑定尚未验，不计通过。当前唯一详细索引development/retired-namespace-evidence.json与checkpoint-retired-namespace-d5336f15；历史失败/7498检查点保留。规则/AGENTS/持续提示已按复盘替代旧策略并保持稳定；新产物外盘、内部28GiB/外盘1.6TiB可用，无reset/提交推送或覆盖用户RDHeaderView。


## 原生纹理同步与投影子资源生产链（当前候选 6b35e12e）

**根因与可判别实验**：当前UE库5717的两次air.fence_texture_2d原指令不是未知external资源调用。独立Native R32Uint2D17×11执行实际write→fence→read，完整八字输出正确；旧d533 capture/API在GPU前因unknownCall1拒绝。独立RGBA32Float array27×19/3层/3mips、view mip1 count1/slice1 count2也在生产修改前Native/capture正确而拒绝。两baseline为机制复现，manifest的PASS表示预期拒绝已复现，不是重放通过。

**通用机制**：MetalAIR::UniformResourceAccess按原Native texture fence完整void/typed texture/addrspace(1) ABI分类ordering引用；方法后缀与实际opaque texture类型必须一致，缺身份或错误ABI仍unknown/拒绝。ordering不读像素、不写像素、不生成CPU数值或producer；当前typed namespace必须恢复该texture，并保留writable角色校验，原compiled shader负责GPU同步。ordering-only引用不伪装为GetDescriptorAccess像素读/写。原buffer/table/address重定位、实际pipeline/dispatch/queue命令不更改。当前实际UE3D与texture_buffer_1d ABI也沿该通用方法族分类；它们在本库没有独立整帧GPU输出证据，不从2D/array样例宣称全族完成。

实测另暴露两个通用义务：显式texture usage缺ShaderWrite仍能进入GPU（中间e594日志有真实wait/OpenCapture成功），虽然Apple原契约要求read/write使用相应flags。生产路径现在按实际Native或验证frame factory/view的usage检查read、write或ordering权限；usageUnknown=0保留原API灵活语义，metadata查询不强加像素读权限。common attachmentLayout也对Unknown usage保留原render用途，不新增默认flags。官方shaderWrite/Unknown usage Markdown已外盘归档，来源为https://developer.apple.com/documentation/metal/mtltextureusage/shaderwrite与/unknown；driver在某设备未报错不是契约成立证据，未把它改成合法负例对照。

第二项由独立array+Unknown usage样例区分：Native有真实各mip/slice clear与GPU输出，但旧hasNativeTextureProducer只承认whole-image color clear；parent部分子资源不能传递给投影view。现在initializedColorCommands记录原clear的实际resource/mip/layer或3D depthPlane及command，view按自身/parent投影范围与同/已提交command查找原producer。保留原whole-image事实的严格边界；局部clear不变成所有pixels已定义，也不造零。fence本身不产生任何初始化事实。本样例读前另有真实parent clear；任意同shader写→fence→读而无在前恢复状态，以及更一般ordinary GPU producer状态传播，尚未因此全量验收。

**本批DX12/Vulkan数据流卡**：WrappedID3D12Device::Serialise_CreateComputePipelineState保存原CS bytecode/rootsig并创建真实pipeline；WrappedVulkan::Serialise_vkCreateShaderModule保存原pCode/codeSize、unwrap链并Native创建module。Serialise_Dispatch/Serialise_vkCmdDispatch恢复实际参数后原Native调度，shader内barrier/fence由原bytecode执行，不由CPU数值模拟。D3D12Descriptor::Create(UAV)按原Resource ID重建Native writable descriptor；Vulkan Apply_InitialState(eResDescriptorSet)用原typed writes恢复descriptor与对应binding state。资源初态沿D3D12ResourceManager::Prepare_InitialState/Apply_InitialState的实际subresource CopyTextureRegion、WrappedVulkan::Prepare_InitialState/Apply_InitialState(eResImage)的mip/layer/plane复制；不是将局部clear提升whole-image。提交沿Serialise_ExecuteCommandLists的DataUploadSync/队列等待及Serialise_vkQueueSubmit原等待/提交。GetDescriptorAccess两后端展示static与可用dynamic feedback，展示精度不决定Native同步执行。Metal复用原shader/对象/typed句柄重定位/初态/提交，额外记录MSL texture ordering operand及parent view的mip/layer clear，因为raw argument ABI与Metal view投影需显式恢复；未新增CPU表达式、coverage许可或UE名称条件。

**固定候选真实验证**：backend/bundle SHA256 6b35e12e7ed2f1d014fd8ea1664ec0133e2f4c28121653aaeafb502bbe8e14b3；GUI仍3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。T0构建/diff、texture-fence-stable-CPU相关完整AIR provenance/effects测试通过，含fence不产pixel事实、missing handle及错误typed ABI，CPU source哈希已记录。T1 texture-fence-stable-key真实Native/capture/API/CLI、完整R32Uint view pixels和八GPU字、六次事件往返/EID0通过；独立texture-fence-stable-independent改为RGBA32Float array、27×19、非零mip/slice、usageUnknown、跨submit，同样完整pixels/output/reset通过，无后端场景改动。定向runner累计实际用时约9.11s/4.64s（包括编译/捕获/正负验证），非UE或RT整帧耗时。

真实缺texture binding、只读descriptor、显式usage缺Native写权限的破坏输入API4/CLI1且无GPUwait/strict。Unknown variation不把去掉Write bit（本来为0）当破坏。texture-fence-stable-T2用本库重放原heap RT、退休namespace RT、JIT multiview、MRT depth/stencil、两stencil aspect、三layered attachment，完整输出/事件/EID0/API/CLI通过；历史capture的Native/capture hash不计本库Native验收。没有逐小改29全量重跑。中间gate两次字段/opener路径错误、e594缺usage guard进入GPU、1f独立view生产链拒绝都保留FAIL/日志，最终才声明上述范围通过；中间结果不冒充当前库。

**UE真实阶段与下一诊断**：同原RT capture7bb9c524…，中间1f907468在40.67s/peak1331003392B越过2D fence，到consumer8271936/PSO9638的texture_buffer_1d fence未分类。原library9046/function9047 AIR已单独归档，实际含3D与buffer1d读、写、atomic与fence。当前6b35候选40.97s/peak1231093760B、无GPUwait/strict，前consumer7219648已越过，consumer8271936 unknownCall由1变0、textureCalls12；仍在commit8440832的后续资源契约检查拒绝。它不是新的未知普通表达式，也没有实际整帧GPU输出。具体是哪一texture身份/usage/布局/输入/依赖检查还未诊断，不把猜测写成已证实缺口。下一给现有texture校验增加精确拒绝证据或用调试器定位真实slot/source/state，再修恢复契约；不反复扩相邻shader数值案例、不删除资源检查。若预检accepted，立即用审计保留原payload/commands的独立候选与有限预算执行UE整帧GPU/readback/事件重放。

M1 active，M2整帧RT GPU输出、M3/M4仍未完成；生产两RT flagsfalse。官方sample/最终集中IR-RT-生命周期-Qt/ARC/native caps与UE整帧output/events/binding未跑，不计通过。当前唯一证据索引development/texture-fence-evidence.json与checkpoint-texture-fence-6b35e12e；稳定AGENTS/持续提示沿复盘规则，无需每个API症状增许可条款。新产物全外盘，内部28GiB/外盘1.6TiB可用，用户RDHeaderView hash仍0e66d6ee…；无reset/提交推送或覆盖用户改动。


## 整数纹理 ABI 与规则格式字节恢复（当前候选 d22c3c1d）

**根因诊断**：对现有 runtime texture 校验增加具体 binding/usage/numeric 拒绝诊断，没有先删除检查。6bd9dc9f 的 UE 禁 GPU 预检明确定位 consumer8271936 的 source3437：实际 MTLBuffer::newTextureWithDescriptor 是 R32Sint、TextureBuffer、16384 texels、offset0/row65536、usage3，父buffer2665真实存在；原 library9046/function9047 使用 unsigned AIR i32 读写 ABI。失败是强制 UInt/SInt 精确相同的分类条件，不是本处身份或 usage 丢失。该诊断约42.53s、peak1388003328B、无GPUwait/strict；不算 UE GPU 重放。

**判别实验与实际修改**：独立合法 MSL read_write texture<int> 和 R32Sint 保持 signed 类型、原绑定和格式；只把原 AIR read/write 的 i32 ABI 后缀 s 改为 u（调用和声明共4处，其他指令/operand/root metadata保持），由 Native 原 shader 执行两次 write→fence→read。高位整数 0x80000000+31/131 的八字输出正确，旧6bd9仍拒绝。该 AIR 变换、原 signed metallib/IR 和实际 Native/捕获证据外盘保存；不是 CPU shader 数值模拟或把 unsigned MSL 绑定到不合法格式。生产 TextureNumericFamily 统一 signed/unsigned 整数位模式族，浮点/normalized 与整数仍分开；保持原格式/bytecode和全部 typed 身份、地址、初态、producer、生命周期、usage与提交检查，统一和runtime路径均使用同一规则。

独立组合在修复设计前选择 RG32Sint23×13（两组件），Native 同样正确但中间3049282e replay被 GetTextureDataBlockShape 的历史格式白名单拒绝。没有追加 RG32Sint 许可项：该公共布局入口现在依据 MakeResourceFormat 的 Regular 类型、数值组件族/数量/字节宽度，以及既有 GetTextureBlockShape 的一致1×1 texel表示，导出普通颜色格式的实际逻辑字节布局。原 Native factory、初态 capture/restore、readback、view和heap布局共享此机制；packed/compressed/depth/stencil保持独立表示，预算、真实subresource和footprint checks不扩大。本次 R32Sint17×11 与 RG32Sint23×13 均无需后端格式/尺寸分支。

**DX12/Vulkan数据流卡补充**：已实际阅读 WrappedID3D12Device::CreateUnorderedAccessView：保存 DynamicDescriptorWrite 的原资源、counter、typed desc及destination，标记实际资源引用；D3D12Descriptor::Create(UAV)使用原uav.AsDesc()和重放资源，最终创建 Native UAV，未把可选CPU shader integer signedness作为 descriptor恢复资格。WrappedVulkan::Serialise_vkCreateBufferView 保存 CreateInfo（包括真实format/offset/range）、unwrap原buffer后Native创建view，并记录 DerivedResource；descriptor writes使用原typed view。D3D12ResourceManager::Prepare_InitialState/Apply_InitialState按 GetCopyableFootprints 与实际subresource CopyTextureRegion恢复；WrappedVulkan::Prepare_InitialState(eResImage)按格式GetByteSize、FormatImageAspects、mip/layer和VkBufferImageCopy恢复，平面独立。Metal复用原typed格式/对象、按真实字节布局复制和原shader执行；Metal raw argument句柄仍需显式重定位，逻辑staging与opaque heap footprint分开。原Serialise_Dispatch/vkCmdDispatch、ExecuteCommandLists/vkQueueSubmit依赖和初态同步保持；GetDescriptorAccess的optional分类不能取代原GPU执行。此次没有新增AIR数值表达式、场景coverage/EID/pass/PSO许可。

**固定候选验证**：backend/bundle SHA256 d22c3c1dfc0475b15d6f1ed3d46f85d4b799543f14ab6efa1cf716b464e5fc8e，GUI仍3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。T0相关构建通过。texture-integer-ABI-T1-key-final和independent-final均真实Native/capture/API/CLI成功，完整view像素、八高位GPU字、六次事件往返/EID0符合原输入；实际缺binding、readonly descriptor、显式usage缺Write、integer shader对应float view四类真实破坏均API4/CLI1、无GPUwait/strict。原始Native格式与shader均匹配，破坏float case才更改真实factory/view格式。

texture-integer-ABI-T2 用当前库重放原 heap/退休namespace真实RT、JIT multiview、MRT depth/stencil、两stencil、三layered attachments，以及原texture-fence2D/array+Unknown usage，共22次相关API/CLI检查，原完整输出/事件/EID0通过；历史capture的Native与捕获库hash不计当前Native验证。未做逐小补丁全矩阵重跑；未运行的官方sample/最终IR-RT-生命周期-Qt/ARC/native caps不计当前通过。中间3049的RG32Sint布局拒绝和6bd9基线拒绝保持原日志/manifest，后续成功不覆盖失败。

**UE实际阶段**：同原RT capture7bb9c524e5b28975c8b6d3e85dcb2c32a80622089812fa069f3609321f05d954，在当前d22库55s/3GiB边界内预检44.83s、peak1065418752B结束，无GPUwait/strict。8271936的signed/unsigned误拒绝解除，8291776恢复accepted；下一8293120/3997为unknownBuffer6，其texture14/unknownCall0/unknownAS0，仍于commit8440832拒绝。未开启 initial/frame GPU，不能称 UE整帧或M2通过。原library2660/function2661已CPU归档：AIR内有三个内嵌addrspace(2) constant [6 x i32]数组，以及动态GEP/null-select/read；它们可能被误当外部buffer来源，目前只是下一可证伪根因，尚未证明六个unresolved均由其导致，不据此直接放行。下一明确区分原shader自带常量与外部device指针；前者应由原Native bytecode携带，后者仍需实际重定位/初态/发布证明。预检accepted后立即走独立审计候选的有限UE整帧GPU输出。

M1仍active，M2–M4未完成，两生产RT flagsfalse。当前唯一证据索引development/texture-integer-ABI-evidence.json和checkpoint-texture-integer-d22c3c1d；产物均外盘，规则/AGENTS/持续提示继续使用复盘要求并覆盖历史冲突，不按新症状增许可。用户RDHeaderView保存；未reset/提交推送。


## Shader 自带数值常量与外部缓冲来源分离（当前候选 9d987f9f）

**实证根因**：9d之前的6d3a6f22仅增加失败指令诊断。真实UE consumer8293120/3997的unknownBuffer6全部定位到三条原load：%580/%585/%590，地址来自三个内嵌addrspace(2) constant [6 x i32]数组的GEP/null-select；每个PointerSet未知项+无sourced候选重复计数两次。完整初始module已捕获，数据不是外部MTLBuffer/未重定位VA，也不缺heap字节producer。诊断44.94s/peak1274675200B、无GPUwait/strict。同机制独立Native R32Uint17×11样例由真实texture read产生索引，读取6项原模块常量；Native/capture输出正确，但6d的API2/CLI1在GPU前拒绝，原diagnostic直接指向模块constant load。baseline manifest PASS只表示预期拒绝复现。

**通用修改与边界**：MetalAIR::Value新增ShaderConstant来源类别，只承认同一原module内完整internal/private、addrspace(2)、pointer-free numeric storage定义。类型覆盖纯数值scalar/vector/array/struct组合，数量/名称不是许可；无external地址引用或pointer initializer。GEP、bitcast及select/phi保留所有来源，原Native bytecode拥有常量存储/布局/动态索引。读取不制造ResourceId、CPU numerical value、initial payload、producer区间或物理heap证明；报告shaderConstantReads并保留optional display partial。所有load返回的数值仍Unknown，CPU loader不读常量表，未新增xor/浮点/调色计算的CPU实现。原shader/pipeline/dispatch/queue和typed地址恢复不改变。

Pointer-free标记不授予任意GPU地址：外部constant声明、pointer/handle initializer仍不能用本路径代替恢复；从常量i64再inttoptr得到未知device指针依然unresolved；向ShaderConstant写入或atomic write不能建立producer/通过资格。真实null-only未知源仍不获恢复。合法分支中module constant与null的选择沿既有候选语义保留，由原GPU分支执行，不将静态候选当实际访问。实际外部buffer、表、AS/Header、texture/sampler生命周期、初态/producer、发布/重定位/提交义务全部保持。

**DX12/Vulkan对照卡补充**：本批再次实际阅读WrappedID3D12Device::Serialise_CreateComputePipelineState：Serialise原Descriptor后，OrigDescriptor.CS原BytecodeLength/pShaderBytecode由WrappedID3D12Shader::AddShader保存并用于DeferredPipelineCompile；随bytecode的常量由Native shader携带，不额外注册成外部资源初态。WrappedVulkan::Serialise_vkCreateShaderModule保存CreateInfo/pCode/codeSize，unwrap扩展链后NativeCreateShaderModule并创建Info/wrapper，原SPIR-V自有常量同样不产生独立device memory恢复义务。外部资源仍沿本卡已有D3D12ResourceManager::Prepare_InitialState/Apply_InitialState和Vulkan Apply_InitialState真实buffer/image路径、typed descriptor/地址重定位、原Dispatch/vkCmdDispatch和ExecuteCommandLists/vkQueueSubmit等待依赖；GetDescriptorAccess展示可partial，不能用CPU未知动态值否认原shader内常量存在。Metal额外需要在AIR provenance区分shader-owned numeric storage与raw argument/device地址；不是改shader或另造GPU解释器。嵌入实际captured VA仍需原重定位要求，不能因“常量”而豁免。

**固定候选与真实测试**：backend/bundle SHA256 9d987f9fb68d9bdbec38da1e6c07937f877431f652c96aa3bf4f18e81b010a28；GUI仍3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。T0最终构建、diff及当前源码完整AIR CPU provenance/effects测试通过；CPU实际覆盖constant/null来源、不调用external loader、不返回CPU数值、缺完整定义、i64→device地址与只读写入。中间build的局部matcher变量触发-Wshadow，已更名修复，失败log保留；CPU首wrapper在真实CPU通过后误试旧T1目录，assert在编译/GPU前阻止，记录为wrapper失败而非完整runner通过，未覆盖原产物。

shader-constant-T1-key：Native/capture/API/CLI，原R32Uint17×11、6项constant、texture读取产生动态index；独立shader-constant-T1-independent在生产修改前选定11项不同constant、RGBA32Float array27×19/3层/3mips、view非零mip/slice、usageUnknown、跨submit。两项完整八GPU字、全view像素、六次事件/EID0正确，无后端场景修改。实测runner累计约7.98s/5.01s，非UE时间。缺真实texture binding、readonly descriptor及key显式usage缺Write均API4/CLI1且无GPUwait/strict；Unknown usage不作虚假缺Write负例。shader-constant-T2以本库重放原heap/退休namespace真实RT、JIT view、depth/stencil、layered attachments、fence2D/array及R32Sint/RG32Sint位模式样例，原完整输出/事件/EID0和26次相关API/CLI检查通过；旧capture的Native/capture hash不计当前Native验证。官方/最终集中IR-RT/生命周期/Qt-ARC/native caps未在本库跑，不计通过。

**UE实际阶段**：复用原RT capture7bb9c524e5b28975c8b6d3e85dcb2c32a80622089812fa069f3609321f05d954。首次55s CPU/no-GPU边界显式超时kill，peak1201569792B；保留为INCOMPLETE，不当malformed/通过。确认进程终止后使用120s/同3GiB有限CPU预算继续完整检查，65.97s/peak1544634368B结束，无GPUwait/strict。consumer8293120 unknownBuffer0、shaderConstantReads3，36外部sourced buffers/1writer恢复accepted；已越过原commit8440832，下一commit8440960的fragment stage2返回validModule0。没有initial/frame GPU执行，M2仍未达到。

下一根因已用原AIR证伪：实际PSO3642的fragment3411/library3409入口记录含两个直接metadata引用!15/!17及尾部function属性字符串。旧direct argument regex只允许尾部数字metadata引用，因此不选择!17并按“模块无效”返回。CPU审计移除该尾部属性后旧regex立即识别!17；原capture/AIR未改，未将修改版当GPU验证。下一按通用入口metadata语法保持真实direct argument list，不能回退全module扫描而混入间接成员ordinal，也不能按特定属性/UE名称放行；独立Native fragment合法属性及未参与设计的绑定/形状组合验证后再回UE。该处尚未生产修复/验收，真实whole-frame output/events仍缺。

M1 active，M2–M4未完成，两生产RT flagsfalse。当前唯一证据development/shader-constant-evidence.json与checkpoint-shader-constants-9d987f9f，失败/原模块/完整产品源码hash均保留外盘；用户RDHeaderView未改，无reset/提交推送。执行规则与持续提示沿复盘稳定原则，不为本次常量/属性症状增加许可等级。


## 入口属性与 typed table 实际绑定恢复（候选 29c8aa53）

**根因及通用机制**：原UE PSO3642的fragment3411入口元数据含真实outputs/direct-arguments两个引用和尾部属性字符串。旧AIR parser仅接受尾部数字引用，错误返回validModule0。现在只读取这两个直接引用，允许原入口尾部属性，不按属性名称/UE名称许可，也不回退全module扫描而混入间接成员ordinal；原bytecode与Native shader属性不改。独立Native early-fragment shader先在9d正确输出而GPU前被拒绝。03d22已越过parser，却因draw路径把所有反射含指针的struct强制视为MTLArgumentEncoder packet而失败。

第二项实际整改区分raw typed table和encoder packet。runtime恢复全部身份、字段发布、地址重定位、实际输入、生命周期与提交依赖成功后，保存命令流位置/阶段对应的实际pipeline、slot、resource、offset。HasRestoredRuntimeTableBinding在当前真实消费点精确匹配并核验Native buffer对象、范围和原table metadata，才允许ValidateArgumentBufferBindings接受没有encoder packet的typed表。命令位置只是通用消费身份，不是固定EID许可；仅有declaration或可选access报告不足以通过。encoder原成员检查、原Native draw/queue/sync、typed恢复链不删除。动态像素索引仍由原GPU执行，展示partial不否认恢复。Native自有module常量机制继续保持。

**DX12/Vulkan对照**：已实际阅读WrappedID3D12Device::Serialise_CreateGraphicsPipelineState及WrappedVulkan::Serialise_vkCreateGraphicsPipelines，它们保存原graphics stage字节码/模块/RootSignature或layout并用真实Native pipeline，不将入口属性转为CPU准入。DX12 Serialise_SetGraphicsRootDescriptorTable用原BaseDescriptor重定位后Native绑定，Serialise_SetGraphicsRootConstantBufferView以GetResIDFromAddr保留资源+offset；Vulkan Serialise_vkCmdBindDescriptorSets恢复layout、原set和dynamic offsets，Native绑定并更新真正pipeline state，descriptor buffer/set是不同恢复途径。Metal复用“按实际绑定所用机制和身份恢复”的职责划分，raw typed地址另需既有字段publication/relocation和本次消费凭据，不能假造encoder调用。初态/物理backing沿本卡已有Prepare_InitialState/Apply_InitialState；ExecuteCommandLists/vkQueueSubmit及Metal原submit依赖保持。两后端GetDescriptorAccess静态描述+可选feedback独立于Native执行；本次不要求CPU解释fragment像素数值。

**固定候选证据**：backend/bundle SHA256 29c8aa5355fc0838d1e212c408248be358fc41e3946a8a9541d7032bc1f69650。最终build/diff和完整AIR CPU来源/效果单元通过，实际覆盖属性含逗号/不透明记录、direct ordinal与indirect ordinal冲突、缺绑定和缺direct list；新测试初次重名编译失败已修复留证。fragment-metadata-T1-key-stable：真实Native/capture/API/CLI、BGRA8 16×16、slot3。independent：修复前选定RGBA16Float 37×23、slot11。两项全颜色/深度像素、各32次事件往返/EID0、16公共资源身份查询通过，不增加后端格式/尺寸/slot分支；实际缺fragment绑定均API4/CLI1且无GPUwait/strict。属于真实graphics API机制验证，sample没有RT调度，不冒充UE光追验收。

中间29c的key-final已经完成GPU执行，但EID0测试错误要求default clear alpha=0；真实Native warm clear的默认alpha=1正确恢复为255。单独诊断确认，而非删除EID0检查；新sample明确设置原Native clear=(0,0,0,0)，输出预期与实际定义一致。两次production build的shadow/const错误、Native属性位置编译错误、ZIP成员索引零填充提取错误均保留，不计通过。fragment-metadata-T2在本库重放heap/retired实际RT、JIT views、depth/stencil/layered、fence、整数ABI和module常量的相关capture，完整原输出/事件及30 API/CLI通过；历史Native/capture hash不计本库Native通过，未重跑无关最终全矩阵。

**UE阶段与根因实验**：同原RT capture7bb9c524…，120s/3GiB边界下实际68.51s/peak1226309632B、无GPUwait/strict，原入口及恢复阻塞已越过；仍在commit8440960的consumer7793152/PSO3680 fragment validModule0拒绝。没有initial/frame GPU，M2未达到。原library3444/function3446的实际AIR已提取：两个define，一个真实fragment入口及原module内部helper；入口metadata没有尾部属性且正常匹配。旧definitions!=1要求误将含internal helper的完整模块拒绝。该helper原调用的控制效果仍需按Native API与原函数体确认，不能按其名字跳过；下一先用同module函数选择/真实helper来源实验，保留外部linked函数身份和调用预算，继续取整帧GPU，不扩CPU shader数值运算。

唯一外盘索引development/fragment-metadata-evidence.json；checkpoint-fragment-bindings-29c8aa53/manifest.json SHA256 c6c977875125c2b4215b639387771a91d2fd87cd75e0000c85dfacb74b4794ce，105份源码/四产品、271产物hash核对。GUI与用户RDHeaderView未改。M1 active/M2–M4未完成，两生产RT flagsfalse；官方sample/最终集中IR-RT/生命周期/Qt-ARC/native caps与UE整帧输出/绑定未在本库验收，不计通过。执行规则及持续提示继续覆盖冲突旧策略，临时症状不成为跨会话许可；全新证据外盘，内部28GiB/外盘1.6TiB可用，无reset/提交推送。


## 同模块函数身份与原生控制效果（候选 7b46ae65）

延续上一节同一个M1恢复闭环，不新建案例许可批次。实际UE模块的“define总数=2”不表示shader缺失；一个fragment入口及其原internal helper共享原module。独立Native sample用真实noinline private helper与动态fragment坐标predicate，在29c完整Native/capture输出正确但API4/CLI1/GPU前被拒绝，原AIR确含入口+helper两个定义。baseline只表示机制拒绝复现。不是更改原UE bytecode或用样例名称授权。

**实际通用修改**：UniformResourceAccess选择目标符号的唯一真实定义，允许模块包含其他函数；重复目标定义仍invalid。runtime analyse从原module的internal/private真实定义建立调用身份，沿实际call传每个ordinal的Value来源，使用已有8层/128call预算；递归/未找到定义/未恢复外部linked身份仍不凭名称绕过。helper无独立Native binding metadata，localDefinition调用凭据要求完整参数数目/原private linkage/没有继承top-level绑定，只使用caller参数，拒绝借另一个入口或struct member的ordinal。子调用的资源/地址/读写/unknown/literal provenance全部汇总到消费恢复检查，返回普通shader数值仍Unknown，原GPU完成动态计算。

原Native air.discard_fragment的完整void()/实际声明/graphics上下文按API控制效果分类：不读写外部资源、不产生初态或producer，不求predicate、不实现CPU输出。其conditionalAccesses影响原producer保守性；缺原声明、错误context/ABI与一般external调用保持unknown。实际Native独立sample含并调用该helper，但所选屏幕坐标predicate不触发discard，完整输出与原GPU结果比较；没有据此宣称任意discard场景或UE当前实际输出通过。源模块/参数恢复已不依赖“整个shader只能一个define”的历史条件。

DX12 Serialise_CreateGraphicsPipelineState/WrappedID3D12Shader::AddShader与Vulkan Serialise_vkCreateGraphicsPipelines/Serialise_vkCreateShaderModule保存完整原shader module，内部函数和fragment控制随bytecode由Native执行，不登记成丢失的外部资源。Metal原initial/backing/typed VA或句柄、实际Root/Buffer绑定及ExecuteCommandLists/vkQueueSubmit对应提交依赖沿本卡保持，raw table资格仍须上一节的消费恢复凭据。内部函数额外来源跟踪用于保护真实外部指针恢复，不把CPU数值求值或GetDescriptorAccess完整性变成执行义务；两后端静态描述+可选feedback展示职责沿本卡分离。本批没有新增shader名称、EID、PSO、slot或格式组合条件。

**固定候选** backend/bundle SHA256 7b46ae653a0d13f39be3be4c13f77166acd372cb090d68b161e4c94f41fa98dd。构建/diff与当前完整AIR CPU suite通过；旧“增加一个无调用helper就应拒绝”的unit oracle导致首次CPU失败，已改为验证原恢复访问不变；重复entry、缺helper参数、缺资源、未声明discard、compute context负例仍保留，失败log不覆盖。local-module-helper-T1-key和independent实际Native/capture/API/CLI、slot3→11/BGRA8 16×16→RGBA16Float 37×23、完整颜色/深度、各32事件/EID0与16公共绑定通过；真实缺fragment绑定API4/CLI1/GPU前拒绝。独立组合在本机制修改前已经选定，无后端shape许可。

local-module-helper-T2以本库重放实际heap RT/retired namespace RT、两个module常量capture与原external-linked graphics，完整原输出/事件/EID0的相关API/CLI通过（10次）；29c的其他回归不计本hash通过，未按小补丁重复全部矩阵。官方sample/最终集中IR-RT/生命周期/Qt-ARC/native caps未在本hash跑，不计通过。

**实际UE** 同原RT capture7bb9c524…，禁initial/frame GPU下70.67s/peak1446150144B退出，无GPUwait/strict/预算kill。consumer7793152/PSO3680从validModule0到validModule1，122 sourced buffer读/5texture/5sampler/unknownBuffer0、两次helper调用成功；仍有unknownCall1，于同commit8440960拒绝。原source审计的唯一新Native opaque操作为air.sample_compare_depth_cube.f32，原void discard helper已正确分类。当前纹理handle和pixel API识别只匹配_texture_，未匹配_depth_与sample_compare_depth族。这个具体源诊断是下一Native API/资源恢复实验入口，不能因pointer-free数学“其他都支持”便删除unknown检查；需要确认depth/sampler身份、像素初态/subresource、原compare采样ABI和依赖，以及独立合法depth变体。现有证据不能称真实UE整帧GPU通过。

唯一证据development/local-module-helper-evidence.json；checkpoint-local-module-7b46ae65/manifest.json SHA256 c3f21fc65cc097a865206714804894c7fd449afe712106083fe4f48d5dce8b3c，105源码/四产品及167产物hash核对，前29c检查点保持原值。GUI仍3ba30e36…，用户RDHeaderView仍0e66d6ee…；全产物外盘。M1 active/M2–M4未完成，两生产supportsRaytracing能力false，官方+UE/output/events/bindings/设备与相关最终回归门槛不改。下一先Native depth比较/typed恢复通用机制，再尽早回UE；规则/AGENTS/持续提示沿复盘稳定原则覆盖旧策略，无reset/提交推送。


## Native 深度采样与实际绑定恢复（候选 340e330b）

本节继续 M1 heap/动态绑定闭环。先复现原 UE `air.sample_compare_depth_cube.f32` 的完整 Native ABI，发现其 opaque `_depth_` 句柄及比较采样未进入纹理/比较 sampler 恢复检查。独立 Native 第一次运行还揭示了普通 `setBytes` 被强制声明指针布局、直接 typed table 未被安排消费验证，以及 Native sampler 错套 converted root 布局的三项通用限制。这些是实际 API 类型/恢复职责问题，没有依据 shader/pass/slot 或尺寸格式许可。

实质修改：AIR 的真实 depth opaque load/cast、read/sample/compare/gather 族按纹理读取与 sampler 来源分类，仍由原 Native shader 执行坐标、比较和数值结果；没有 CPU 采样器。Native sampler 表以当前 slot 的真实 kind2 来源恢复，不要求 converted static-root 布局。普通内联 API payload 保留原字节；原声明的非零指针继续重定位，读取 metadata 使用 find，避免凭查询制造空布局。直接绑定 typed table 和普通 inline Native 消费者都进入实际资源/地址/提交恢复检查，不能通过删除 root 声明绕开它。没有原 AIR 且 inline 字段没有地址身份声明时暂不认证；这是地址来源机制边界，并非只因访问展示未知而拒绝。合法性、4096-byte Native setBytes API 上限、生命周期、初态、真实提交与预算检查保留，已有 coverage65 仅标识协议路径，不新增场景等级。

对照已读 DX12 WrappedID3D12Device::CreateShaderResourceView/CreateSampler 保存资源+原 view/sampler 描述；D3D12Descriptor::Create(SRV/Sampler) 在重放对象上创建原格式/范围/ComparisonFunc。Vulkan Serialise_vkCreateImageView/Serialise_vkCreateSampler 保存原 CreateInfo，unwrap 后调用 Native factory，登记真实对象和 view subresource。两者保留原 shader compare operation，不执行 CPU 比较。普通字节对照 DX12 Serialise_SetComputeRoot32BitConstants 原 UINT array 与 Native root API、Vulkan Serialise_vkCmdPushConstants 原 values/offset/stage 和 Native 调用；实际绑定参照 Serialise_vkCmdBindDescriptorSets 的对象/dynamic offsets 与执行点状态。初态/地址/提交/展示仍沿本卡 Prepare_InitialState/Apply_InitialState、真实资源+offset、ExecuteCommandLists/vkQueueSubmit 和可选 feedback 的分工；Metal 额外恢复 raw typed table 的 gpuResourceID 和 inline VA，不从普通整数猜对象。

固定 backend/bundle SHA256 `340e330bb2d5db9c410f0152d2cb4eaaae671d206d2b5c935dff37110d7d877e`。depth-compare-stable-key 与 independent 的原 MSL→Native→capture→API/CLI 通过：Cube Depth32Float 12×12/mip1/view6×6×6 → 2D Depth16Unorm 19×13/mip2/view4×3，sampler/output slot、table/sampler/output offset、跨提交清除均变化。每组完整8个采样输出字、全部 Native 深度字节、6次前后往返/EID0 和实际 descriptor identity 通过；分别累计1296/72像素核对。Native-only 与捕获进程在 capture 前独立读回三种已定义清除状态，baseline hash 一致；没有用 CPU 浮点→UNorm 舍入假设替代设备结果。真实 sampler 绑定缺失的 API4/CLI1 均在 GPU 前拒绝。

失败证据保留：初次 sampler.label 只读编译问题；前两基线停在 setBytes，不能称已复现 depth call；622b8e02 基线才是 unknownCall1/depth 资源未识别；首正例揭示 Native sampler 根限制；Depth16 首变体 API11 的0.5量化判据错误（Native实际0x7fff），改用独立 Native 字节基准后通过，不改生产格式恢复逻辑。12d52575/12220ee8 属于开发检查，结果不算340e最终验收。

depth-compare-stable-T2 当前库实际 heap RT、退休 namespace RT、两个 module 常量、external-linked graphics 和 inline pointer 来源控制的相关 API/CLI 输出/事件/EID0 通过；完整 AIR CPU suite 和相关构建/diff通过。删除真实 inline layout/source、保留原非零 VA 的负例 API4/CLI1/GPU前拒绝，普通 setBytes 放行没有绕过地址恢复。没有重复29份全套；官方/最终集中 IR-RT/生命周期/Qt-ARC/native caps 未在本候选跑，不计通过。

真实 UE capture7bb9c524… 的340e禁 initial/frame GPU核验86.47s/peak1141702656B、API4、无 GPUwait/strict/预算终止。深度比较原消费链和后续多组消费者通过；本次详细提交诊断指向 command12661、consumer8446144 的 stage1，在 AIR 报告前的 CPU vertex_id 投影失败。原 index12213 是4096-byte Private heap buffer，原 CPU source12191 有完整2400字节 snapshot，经 command12217 的复制先于消费提交；600个uint32索引 min0/max399，另有完整 heap birth 字节。未证明地址或必要初态缺失，也未取得 UE 整帧 GPU 输出。下一区分 CPU 状态预算/失效与实际 Native 索引输入恢复，按 DX12 IASetIndexBuffer/DrawIndexedInstanced、Vulkan vkCmdBindIndexBuffer/vkCmdDrawIndexed 的资源+offset/原调度方式整改；真实缺输入和依赖仍须失败，不能用零或未来 producer 补过去读取。

唯一外盘证据 depth-compare-evidence.json / checkpoint-depth-bindings-340e330b；中间日志与失败均外盘，原捕获、GUI3ba30e36…及用户RDHeaderView0e66d6ee…保持。AGENTS 与持续任务 metal 的稳定复盘规则已核对生效，明确覆盖旧策略。M1 active/M2–M4 未完成、两RT flagsfalse；保留官方sample、真实UE输出/事件/EID0/绑定、设备判断与最终相关回归后启用再验的门槛。内盘27GiB/外盘1.6TiB可用，全部新增中间产物外盘，无 reset/提交推送。


## Native 索引输入恢复与可选数值投影（候选 d152c324）

实质整改将 indexed draw 的真实资源/offset/count/type/完整字节区间和提交顺序纳入普通输入恢复账本。coverage65 路径不再要求 CPU 求出每个索引、保持数值缓存或满足历史65535顶点值许可；原 Native draw 执行 GPU 生成索引。CPU 投影仅在当前字节确知且预算足够时提供 vertex_id 范围，否则 unknown。每个实际消费仍必须验证原索引区间已有真实初态、创建输入、上传或在前 producer，不能借未来写入、合成零或仅凭声明放行。旧协议维持兼容；旧 draw 形状/计数及 index allocation 边界尚未全部整改，不声明任意规模绘制。

对照已读 DX12 Serialise_IASetIndexBuffer 保存 GPU 地址对应 ResourceId+offset、size/format，Serialise_DrawIndexedInstanced 保留原参数/Native draw；Vulkan Serialise_vkCmdBindIndexBuffer 保存真实 buffer/offset/type、Serialise_vkCmdDrawIndexed 用原参数执行并更新事件。两者不要求 CPU 算出所有 index。初态沿 Prepare_InitialState/Apply_InitialState，地址沿真实资源重定位，提交沿 ExecuteCommandLists/vkQueueSubmit；Metal 使用现有 heap 物理恢复区间和原 API 命令顺序，将 CPU known 与实际字节恢复分开，展示未知不冒充缺输入。

backend/bundle SHA256 d152c3245937135a0ce3cbe76a481a19d823da14c5d2440bb187c8b9aafb485f。native-index-final-key 与 independent 的原 Native GPU popcount 动态产生索引→capture→API/CLI 完整输出通过；ushort offset0/BGRA8 16×16 → uint offset16/RGBA16Float 37×23、不同动态旋转/绑定槽。全部颜色/深度像素、索引及 padding 字节、32次事件往返/EID0、16资源身份核对通过，合法变体未改后端。索引越真实buffer范围 API4/CLI1 在 GPU 前拒绝；删除 GPU producer 但保留原完整创建输入的合法控制 API0/CLI0，不能继续把“移除dispatch”一概称 malformed。首负例脚本误把 inline XML byte array 当 ZIP 成员的失败保留于 native-index-key-contract，修复真实字段后两个最终样例通过。

native-index-T2 本库重放实际 heap RT/退休 namespace RT、module常量、linked graphics、两 depth Native baseline、inline来源控制，完整输出/事件/EID0通过；真实非零 inline VA 删除地址声明仍 API4/CLI1/GPU前拒绝。完整 AIR CPU suite、构建/diff通过；旧capture Native哈希不计当前 Native通过。官方/最终全矩阵/native caps/能力开启验收未跑。

真实 UE 原capture7bb9c524…在本库禁initial/frame GPU预检86.50s、peak1462452224B、API4，无GPUwait/strict/预算kill。索引12213的真实恢复通过，vertex_id=unknown；后续同consumer8446144/PSO9453的 %.unpack160（%106）指针来源被判 unknownBuffer1。尚未取得 UE 整帧 GPU输出。独立原模块 CPU 实验已证伪“动态索引本身丢 namespace”：unknown 数值保留24-byte typed namespace并通过來源传播；当前实际绑定/已发布row与数值select的差异仍在审查，不把假设写成永久拒绝。实验是来源传播诊断，不是GPU验收。

唯一索引 native-index-evidence.json / checkpoint-native-index-d152c324；所有原始失败、当前源码/产品/日志外盘。GUI及用户RDHeaderView保持，规则/AGENTS/持续提示已采用复盘方案。M1 active，M2–M4未完，两RTflags false；不修改最终开启门槛，不reset/提交推送。

## 指针条件来源与普通 GPU 贡献数据恢复（候选 f3e2d595）

继续已有 M1，不回退到复盘的568e/14714基线。本批移除两种把可选数值分析/普通GPU数据当作恢复许可的前提；Native索引输入/heap backing前批机制保留。固定 backend/bundle SHA256 `f3e2d59515851a631787e62918b4296e852ef0fcca3c0efc7908d0194c41b67b`。证据与不可替代失败均在外盘 development；唯一索引 `native-restoration-evidence.json`，检查点 `checkpoint-native-restoration-f3e2d595/manifest.json`。不以旧候选结果冒充本库 Native 验收。

| 根因/恢复义务 | 实际整改及保留约束 |
|---|---|
| API/设备与实际输入 | 保留原factory、真实对象/offset/长度/线程组和Native调用；实际索引读取范围继续独立恢复。Native GPU生成索引不再要求CPU推导全部vertex_id。 |
| 地址来源 | 已知数值select选中null时，旧CPU分析丢掉另一路真实pointer来源，随后对没有CPU CFG执行证明的helper误报unknownBuffer；现在pointer选择始终合并两路来源，普通scalar选择仍可折叠。实际资源身份/offset、nullable来源和literal VA义务仍分别验证；null-only、缺绑定、原始未重定位VA不被放行。 |
| 普通初态/原命令 | AS Header中的非零贡献指针必须重定位；被指向的普通贡献数组允许由原Native GPU kernel产生，不再在帧末统一要求它从未被写。实际每次消费的资源/区间/生命周期/初态或在前producer仍校验。Header地址字段本身仍不能被未声明GPU写覆盖。 |
| 提交与预算 | 原command buffer顺序、producer提交、AS构建图、上传/发布时序及整体预算保留。没有将未来写入借给早期消费，没有捕获执行后的值覆盖初态。 |
| 历史/展示边界 | 不新增coverage/slot/尺寸许可。CPU数值未知可以partial；显式旧frozen-dispatch协议的贡献快照仍保留不可变义务，不能宣称所有Native writer组合已实现。无inline或typed-table的直接buffer writer仍受旧调度安排限制，属于待修机制，不称非法。 |

**具体 DX12/Vulkan 数据流对照。** `Serialise_SetGraphicsRootDescriptorTable`（d3d12_command_list_wrap.cpp）保存真实heap ResourceId+descriptor index，重放unwrap重建heap地址并原生绑定；`Serialise_SetGraphicsRootConstantBufferView`保存实际VA的ResourceId+offset再取重放地址。`Serialise_vkCmdBindDescriptorSets`（vk_cmd_funcs.cpp）序列化layout/sets/dynamic offsets并恢复对象、原生绑定；set/table访问展示与执行分开。Metal原始ABI表和内联指针需要独立typed publication及字段重定位，不能凭普通数值猜地址；合并来源用于保守恢复，而非CPU执行shader分支。两后端 `GetDescriptorAccess` 的static/可选feedback用于展示，不承担Native执行许可。

`D3D12ResourceManager::Prepare_InitialState` / `Apply_InitialState`（d3d12_initstate.cpp）区分descriptor heap snapshot和普通资源初态；`WrappedVulkan::Prepare_InitialState` / `Apply_InitialState(eResDeviceMemory)`（vk_initstate.cpp）按实际内存InitReq复制/填充和barrier。`Serialise_ExecuteCommandLists` / `Serialise_vkQueueSubmit`保留原命令及提交依赖；普通UAV/贡献数组由原producer运行，不在帧末要求从未写过。Metal复用该职责划分，Header地址发布与普通贡献字分开；AS内部仍保持黑盒。未恢复typed字段/缺必需输入或失效producer继续拒绝。

**Native与当前候选验证。** known-pointer-selection-key/independent先在d152复现真实Native合法而CPU丢来源，c79修复后Native/capture/API/CLI全画面/深度、各32次事件/EID0、16身份查询通过；独立组合在修复前选定RGBA16Float37×23、不同slot和GPU uint索引offset16，关键BGRA8 16×16/ushort，未加后端形状分支。本候选f3通过这两份capture的API/CLI完整回归；c79 Native证据只计其hash。贡献数组sample在f3 fresh Native/capture/API/CLI通过：1实例/同类Header0+贡献0/slot2/跨submit → 3实例+active2/Header64+贡献32/slot7/同submit+输出offset16；真实count参数限制原kernel写范围，`clz`动态值由GPU计算，CPU仍Unknown。完整贡献字节（含padding）、hit/distance/instance/miss输出、各56次事件前后/EID0及AS/UAV/descriptor身份通过。改坏实际Header contributionOffset越真实allocation，两组API4/CLI1且GPU前拒绝；完整坏capture矩阵本批未跑，不算PASS。

失败分类保留：contribution-data-baseline/key/independent初次先停在没有inline/table的直接buffer writer安排，不证明帧末不可变分支；sample加入参与真实写范围判断的count参数后通过，不能宣称后端已解除这一独立API缺口。known-pointer CPU首轮失败是旧literal/select oracle：合并来源后literal访问必须显式保留，已修正测试而非删除真实VA检查；日志保留。native-restoration-final-T2在f3重放实际heap RT、退休namespace RT、两module常量、linked graphics、两depth Native基准和上述pointer组合，原输出/事件/EID0通过；真实inline非零VA删除typed来源仍GPU前拒绝。相关构建、完整AIR CPU suite与diff检查通过。没有逐小改29全量重跑，官方/final IR-RT/生命周期/Qt-ARC/native caps/开启后复验未在本库完成。

**真实 UE 首次执行整帧 GPU，但 M2 尚未验收。** original capture `7bb9c524e5b28975c8b6d3e85dcb2c32a80622089812fa069f3609321f05d954`含已审计的实际HW query调度。f3禁GPU预提交核验通过（92.243743s，peak1325531136B，无GPUwait/strict）；其中API4是有意的诊断停止，不能算正常OpenCapture。candidate utility只对同库/同capture、精确success marker/exit4、无GPUwait/strict/预算停止的manifest复用该证据，另行核对全部original chunks/header/ZIP payload/thumbnail，仅添加通用coverage65声明。原capture不覆盖，不提前改生产RT开关。

正常OpenCapture已执行initial uploads和整个frame命令并完成6759对wait；随后因一个compute indirect per-use数值差异API4，尚未取得presented输出。首whole runner在已知API4后又因日志路径被probe切换产生FileNotFoundError，实际GPU已结束，准确耗时/RSS缺失，不伪造统计。独立per-use trace同库/同候选：124.489127s/peak1775042560B、无strict/预算停止；56对仅EID4298有`actual224,1,1`对`capture101,1,1`。generic producer trace125.014654s/peak1773649920B，仍同差异，证明生产kernel EID4214写出byte0=`112,1,1`，原AIR从counter0分别ceil(/64)写byte0、ceil(/32)写byte12；差异在生产时已存在，不是后续snapshot读错。完整whole输出/复现/EID0尚未通过，不能把命令执行完成或预检通过算验收。

**下一根因卡。** `indirect-root-AIR/restoration-diagnosis.json`核对实际argument buffer3127/offset12、producer4045、CBV4313+62464原发布及两条真实typed row：output3371/backing3127、counter12376/backing12372。counter有原heap出生字节和在前16-byte blit；前两个indexed draw的stage1绑定只是vertex引用，不是writer，原vertex AIR无device写，不能误称fragment producer。下一读实际写counter的compute/texture生产链及其输入/原时序，先查恢复错误或允许变动，不重复无变化整帧。编号/名字只用于定位。已读 `WrappedVulkan::FetchIndirectData` + `Serialise_vkCmdDispatchIndirect`：按真实buffer/offset/per-use GPU复制读取更新action，执行原间接API；DX12 `PatchExecuteIndirect`仅patch VBV/IBV/root/DispatchRays地址，普通Dispatch数值原样保留。Metal当前数值一致性拒绝及captured groups收窄invocation/零groups跳过恢复仍混合；任何解除普通结果相等要求之前，必须先改为不依赖captured exact groups的完整间接消费恢复，保留真实资源/地址/提交/预算，不只删memcmp。当前不据此宣布数值差异合法。

规则/根AGENTS/持续任务metal已核对采用2026-10-08复盘，明确替代旧场景许可/CPU数值资格及错误负例；保持Sol high和当前未提交工作。M1 active，M2输出/ M3事件与跨负载/ M4最终开关均未完成，两生产RT flagsfalse。GUI仍3ba30e36…、用户RDHeaderView仍0e66d6ee…；内盘27GiB/外盘1.6TiB可用，新增大产物均外盘，无reset/提交推送。


## 原生间接调度恢复与执行参数分离（候选 c54b5fb4）

**根因与恢复义务。** 上一候选f3e2d595已恢复原indirect buffer3127/offset12、Header地址、typed namespace和提交生产链，实际生产kernel从counter生成224，而capture观察为101。实际counter writer的AIR已取证：PSO4038/function2694读取两份2D纹理，执行筛选和纹理原子累加；PSO4045从counter生成两个不同步长的调度参数。保守vertex绑定不是writer，不制造CPU producer证明。数值差异尚未证明来自合法变动，也未证明图像一致；仍用于输出诊断。真正的通用问题是captured groups参与消费恢复范围、零工作跳过及CPU预算，随后再以数值相等作为资格。删除memcmp而保留这些前提会允许实际GPU访问超出已恢复范围，不能采用。

**实际机制。** 通用Native/converted runtime indirect消费者不再使用capture观察建立invocationExtent或跳过零观察消费；按完整可寻址typed namespace恢复对象、已定义输入和当前地址，线程组局部builtin仍按实际Native API参数。将真实12-byte indirect输入区间按原提交顺序纳入恢复账本，保留原对象/offset/ordinal、出生/alias、初态/producer与依赖约束；没有把普通GPU writer变成CPU确定字节/标量事实。数值证据仍保留，用于警告和展示；action显示消费点实际GPU数据。

间接消费点使用Native小kernel保存实际三uint并检查既有统一工作量预算，原shader按同一实际参数执行；超预算或旧冻结协议footprint不符时阻止该原消费并明确失败。此helper不计算原shader结果、不使用capture数字替代执行参数、不扩大预算。实际direct总工作量单独计账，actual indirect工作量在GPU上扣减；通用indirect的旧capture观察不再决定预算。原buffer/texture alias barrier、debug绑定0–3的已重定位bytes恢复、unretained snapshot所有权和新context/EID0预算重置已落实。显式旧query配方还依赖冻结footprint，保留独立协议限制并由GPU核验；不称其已经通用化。原生间接调度与可选CPU展示仍有其他实现范围，本组不宣称全部解除。

**具体对照。** Vulkan `WrappedVulkan::FetchIndirectData`（vk_draw_funcs.cpp:28）按实际buffer/offset制作每次消费的readback，ALL_WRITE→TRANSFER barrier后复制；`Serialise_vkCmdDispatchIndirect`（1280）执行原Native source+offset，事后GPU数据更新action，不要求captured count相等。DX12 `D3D12DebugManager::PatchExecuteIndirect`（d3d12_debug.cpp:1218）DISPATCH/DRAW普通数字原样保存，只patch VBV/IBV/root/DispatchRays地址；没有地址字段则返回原arg buffer。Metal不能在compute encoder内插入blit，沿已有per-use小kernel实现同一时刻快照，额外恢复被helper覆盖的Native绑定、buffer-texture alias同步和本后端原有统一预算。资源初态沿本卡DX12/VK Prepare/Apply_InitialState的真实backing/原payload；typed VA/句柄仍按真实对象+offset与gpuResourceID重定位；原commit顺序沿ExecuteCommandLists/vkQueueSubmit思路；CPU数值和访问范围分析不能代替上述恢复；实际地址来源仍是独立恢复约束，未声称AIR与地址恢复完全解耦。

**当前候选与实际检查。** backend/bundle SHA256 `c54b5fb40338e5d92b7711c3c53d5ed961873ef030af1d1aef198bdc199c1883`。T0构建通过（indirect-native-build-budget/manifest.json）；9de3f4b7及更早中间构建/enum cast失败保留，不能混算本库。indirect-native-T1-budget fresh key与独立Native/capture/API/CLI通过：offset16/16、groups1/3/2 → offset28/32、groups3/2/4、unretained；GPU popcount原计算、完整64B输出与padding、实际actions/原indirect绑定及7组reset/seeks均核验。仅将捕获观察改为0、2或262145仍执行原合法GPU输出；实际writer产生262145则GPU guard拒绝，actual trace确认262145/1/1且wait完成；实际来源offset不符则GPU前拒绝。前者是实际预算边界，后者是真实输入身份契约，不能把普通观察变化当malformed。旧execution_match_gate已改为原GPU结果/接受语义，历史拒绝证据保留。

UE同原capture7bb9c524…禁initial/frame GPU预检通过：102.355056s/peak1770700800B，API4为诊断约定，accepted=true、无GPUwait/strict/预算终止。正常whole-GPU和本候选T2结果另追加，不把预检当整帧输出通过。产物位于外盘B544/development/indirect-native-*，M2输出合理比较、M3事件/跨场景和M4最终门槛仍须实际完成，两生产RT flagsfalse。


### 当前候选实际 UE 输出与事件结果

- `indirect-native-UE-whole/manifest.json`：原capture7bb9c524…的通用声明审计副本5a6756b7…，所有原chunks/payload/thumbnail保留，正常OpenCapture和整帧Native GPU完成，127.081494s/peak1780072448B，无strict或预算终止。真实呈现900×640 BGRA共2304000B，SHA256 `74440ea5ad82241c29a8158e33c09940da47cee93febfe0fd299e1e0efa79fd2`。200draw/152dispatch，末事件9971。不是跳过frame的CPU诊断。
- `indirect-native-UE-resets/manifest.json`：正常open加两次完整EID0→末事件重放，127.857616s/peak1779744768B；三次完整presented bytes上述hash一致、API0、无strict/预算终止。
- Native基准来自原capture时间点的真实backbuffer JPEG。首LANCZOS对齐使用了错误采样方式，粗比较FAIL保留；读`RenderDoc::ResamplePixels`（core.cpp:1523，整数point/width向8对齐，Metal FramePixels默认不翻转）后按真实规则对齐，沿原先固定阈值PASS，整图MAE0.549243/255、p99=7，viewport MAE0.406838、p99=6。进一步直接使用仓库`jpge`与`EncodeThumbPixels`原quality90对实际replay raw bytes重新编码，**与原capture JPEG逐字节相同**，两者SHA256 `48a3ed24b00f0e4878ace00615938af18db7f4e0ba35bb5ce2fcca589392e7e8`。这证明原缩略图覆盖的最终输出一致，不能冒充完整无损Native raw pixel baseline或全部中间RT纹理一致。
- `indirect-native-T2/manifest.json`：当前库实际scoped RT/retired namespace、常量、linked graphics、depth/pointer来源、原inline binding和真实VA缺失拒绝，以及三组旧间接query（triangle、64geometry/unretained/末offset、same-encoder）输出/事件/EID0通过；旧numeric observation oracle新语义实际跑通。当前整套AIR CPU通过。Native新key/independent仅计本候选T1；旧RT capture重放计本候选T2，不将旧Native库结果混算为fresh本库。
- `indirect-native-UE-AIR-all/manifest.json`：审计工具已去掉默认Lumen标签筛选，当前capture全部119compute入口均disassembled，6入口有query lifecycle、5非零绑定调度。这是实际shader/call与绑定证据，不把debug名当RT资格；不声称每个invocation都走query分支。
- `indirect-native-UE-ray-events/manifest.json`：按上述独立AIR证据定位5个实际query事件，正常GPU后逐次正向、反向、事件前后及EID0定位；PSO/函数、原资源表binding、root实际byteSize、indirect source+offset一致，154.505793s/peak1780137984B，API0/无strict/预算终止。展示accessEntries为14–30，允许partial，不把分析完整度变成执行资格。生产不依据这些PSO/EID/名字。直接query事件public indirectBuffer字段仍保留之前间接值；本probe只对真实indirect action核对source，没有把这个旧展示字段冒称direct API输入。此展示问题独立记录，不能作为资源已恢复的Native拒绝依据。

**结论/下一步。** 本批当前heap/typed地址/普通数据/间接恢复闭环已取得真实UE整帧输出与JPEG原基准一致，并通过本捕获的完整reset和query定位；M2首份整帧输出达成。M3跨捕获/跨场景及更完整Native基准仍需验证；M4官方sample、最终固定相关矩阵、设备条件、真实生产截帧路径及启用后复验未完成，两RT生产flags继续false。本捕获目前使用审计后通用声明副本，不能称未改生产流程已通过。下一优先第二份已有独立UE capture的同等API恢复/输出与无损Native基准，非继续扩CPU表达式或追逐功能名；同时将direct event stale indirect展示、间接source整allocation1MiB历史限制/三维线程组实现边界列作相邻通用机制，不伪称所有合法Metal组合已支持。

GUI3ba30e36…、用户RDHeaderView0e66d6ee…未改；全部新中间与失败证据外盘，内盘27GiB可用，无reset/提交推送。唯一冻结入口 `development/indirect-native-evidence.json` 与 `checkpoint-indirect-native-c54b5fb4/manifest.json` 保存当前源码/产品/文档和各个实际结果；旧f3冻结不覆盖。

## 按需控制输入与跨捕获恢复（候选 c88d10d4）

**诊断。** 保留 c54 的首份 UE 输出/事件证据，不回退复盘的568e/14714基线。旧403f捕获没有heap出生状态记录，动态候选14714的已定义性无法从原记录补出；未重跑无变化的失败，也未把它改称非法API。另用当前capture机制取得独立UE捕获 `97169436…`，请求352×256并核对55个实际texture描述为该尺寸、99个heap birth记录；完整119 compute AIR仍有5次非零query。Native capture库是c54，以下重放库是c88；不冒称两者同hash。原同帧无损PNG896×637由进程局部公开config设置取得，未FlushConfig；用户配置哈希始终ea359e1a…。

c54预检在最后提交command13792/consumer18827456/PSO437被拒绝。其真实AIR是三个64-bit字组成的descriptor行scatter，而非普通shader结果写入；table13799的两个read条目来源6841+16640/+16896，write条目目标表25，6个实际DescriptorSlotProducer记录源行/目标行。地址/typed metadata已有，不可直接删动态writer检查。控制输入6841+17152等是已捕获CPU发布的普通整数；dense CPU观察缓存已用64,933,622B，剩余2,175,242B不足为2MiB allocation分配两份全长数组。于是4-byte控制输入变unknown，已存在的typed scatter恢复被错误挡住。外盘last-submission-typed-scatter-ledger.json保留具体来源；PSO/编号只定位证据，不进入生产分支。

**实际机制。** `ValidateDescriptorSlotFrame::validateRuntimeDispatch`在dense scalar cache不可用时，只按需观察所读整数区间：原完整初态、在前已提交CPU snapshots及本提交CPU snapshots依原序叠加；不读取未来提交或消费后GPU内容，不填未知字节，不新解shader表达式。任何GPU/物理alias writer使旧版本失效，fallback不得重新播种；same-dispatch写范围失效逻辑、真正typed字段重定位和scatter发布校验保持。已有缓存/当前来源不一致仍拒绝。保留原64MiB缓存预算，未扩大上限。`validateComputeCBVSubmission`把实际CPU上传区间的恢复事实放在可选数值缓存之前：缓存分配失败不再抹掉原命令已经恢复的普通输入。未声称GPU动态descriptor写入已完全通用；无可恢复typed来源/发布或失效非零指针仍失败。

**具体对照。**

| 职责 | DX12/Vulkan函数与数据流 | 本批Metal行为/必要差异 |
| --- | --- | --- |
| 捕获/初态 | `D3D12ResourceManager::Prepare_InitialState`/`Apply_InitialState`分别保存真实resource/heap bytes及descriptor shadow；`WrappedVulkan::Prepare_InitialState`/`Apply_InitialState(eResDeviceMemory)`按真实InitReq区间copy/fill及barrier | 原heap出生、初态和CPU snapshots不变；真实上传恢复区间独立于可选dense观察缓存。Metal不能用texture/AS opaque footprint替代普通buffer字节 |
| 地址/发布 | `D3D12Descriptor::GetRefIDs`保留SRV/UAV身份、CBV由`GetResIDFromAddr`解析，UAV为PartialWrite；`WrappedID3D12Device::CopyDescriptorsSimple`记录source refs和DynamicDescriptorCopy、更新logical shadow | Metal原24-byte表内是实际VA/ID，必须逐来源patch；`TrackDescriptorGPUCopy`及DescriptorSlotProducer继续核对真实源、目标、类型、版本，期待元数据不得CPU覆写GPU目标 |
| 执行/依赖 | `Serialise_Dispatch`/`Serialise_vkCmdDispatch`执行原native命令；`Serialise_ExecuteCommandLists`/`Serialise_vkQueueSubmit`保持原提交/同步 | 仍执行原scatter/query/API命令与CPU发布边界。按需观察只用于已有恢复路径，不发明GPU生产值；GPU/alias写后旧整数观察不能复活 |
| 展示 | `VulkanReplay::GetDescriptorAccess`从static access/当前descriptor store构成展示，与原CmdDispatch分开；DX12 feedback同样不构成普通shader算术解释器 | 动态访问可partial；本次没有按视口、UE名称、PSO/EID授予资格，也没有新增CPU表达式求解 |

**实际结果与范围。** 后端/bundle SHA256 `c88d10d49c0fbe3d66f78fc885862d4946761d5d6080be5694d98a23c1496679`。

- T0构建10.440696s通过；实际AIR suite、相关pycompile/diff检查通过。
- 独立Native控制缓冲40MiB/offset128→36MiB/offset252并改变原线程组计算，两份fresh Native→capture→完整API/CLI输出、输出padding/原64-byte Private CBV及事件/EID0通过；cache按需分支实际被调用。非法非零root对象和offset等于allocation length仍API4/CLI1/GPU前拒绝。
- 新UE971捕获的无GPU预检102.632822s/peak1,295,990,784B accepted=true；API4是有意诊断停止，不能算整帧。所有原chunks、payload和PNG审计保留，只加通用声明的candidate `a2bf0c06…`；原capture不覆盖、生产RT flags不提前开启。
- 正常OpenCapture/整帧GPU127.118236s/peak1,302,528,000B、API0，实际200 draws/152 dispatches、last9968。900×640原BGRA读回2,304,000B SHA256 `04ed9435baa822f3c30abd166a94f32592ac63317f5f18e39df2897f80f23c7a`。按仓库ResamplePixels原整数point mapping、原方向比较同帧PNG，896×637 RGB全部相同：差异像素/通道/最大误差均0，无容差和替换滤波。未采样像素/alpha、各RT中间结果不借此认证。
- 同候选两次EID0→末事件完整重放127.574335s/peak1,302,396,928B，三次完整raw hash同上，严格诊断/预算停止均无。该实验只证明完整reset，不代替query事件绑定验收；后者及本候选scoped T2运行结果追加在此。

外盘入口 `development/scalar-interval-*`、`cross-capture-UE-lossless`及`lossless-capture-settings`。旧c54第一份捕获及403f失败证据不可变。M2已有真实输出；M3第二捕获整帧/无损基准/reset已通过，query往返/绑定及scoped T2已通过；M4官方sample、固定最终相关矩阵、设备、未改声明的生产截帧流程及开启后复验未完，两flagsfalse。保持Sol high/未提交代码，持续任务稳定规则已整合复盘，并去掉固定“当前M1”的过时措辞；用户RDHeaderView未改，内盘27GiB/外盘1.6TiB可用，全部大产物在外盘。

**批次追加验收。** `scalar-interval-UE-ray-events/manifest.json`按此捕获119-entry AIR inventory定位5个真实query事件3533/3642/4215/4377/4604，原PSO/函数、root大小40/48/56/64/72、typed table及真实indirect source/offset在正向/反向/前后/EID0往返保持一致；154.350293s/peak1,308,459,008B，API0、无strict/预算停止。直接dispatch的旧stale indirect展示仍单独记录，不冒称真实API输入。`scalar-interval-T2/manifest.json`当前库scoped原heap RT、退休namespace RT、间接RT/unretained/same-encoder、module常量、linked/depth/pointer/inline控制以及真实缺地址拒绝、完整AIR CPU suite通过；32.714747s，max进程RSS129,548,288B。复用旧capture，不冒充当前fresh Native。

`scalar-interval-old-cache-baseline/manifest.json`用不可变c54 dylib加载独立合法40MiB fixture，DYLD实际载入路径核验，1.760480s API4且无GPUwait/strict；当前c88同fixture Native/输出通过。这样缓存根因被独立复现，而非只观察UE拒绝位置变化。新Native helper设置及监督工具的21个既有测试通过，初次wrapper嵌套同一GPU锁只阻住启动、未启动UE/GPU的失败及更正保留；不称GPU/系统崩溃已修复。

**当前出口。** M1本批缺口闭环、M2输出达成、M3第二捕获的独立尺寸/无损输出/reset/真实query事件绑定通过。M4仍开放：下一优先通用capture协议事实持久化→未改声明的真实UE截帧重放；稳定候选再集中官方sample、相关最终/生命周期/原生设备与启用后复验，不每次小改全量重跑。c54首份捕获结果属于其历史hash，本c88未重新跑该原输入；403f旧缺出生事实的捕获不凭新代码获得缺失字节。唯一外盘当前证据索引 `scalar-interval-evidence.json`、检查点 `checkpoint-scalar-interval-c88d10d4/manifest.json`，生产两flags继续false。

## 通用捕获协议事实持久化（开发候选 202bbb55）

c88稳定闭环证据及检查点已冻结。继续同一大批次/M4相邻依赖，不重做其恢复机制。当前开发backend/bundle `202bbb553113d077db69a1e5c08f7c738a4b62d625aa6546cd8ced5e5bb0786a`，仅进一步改变capture协议记录；不能将c88通过直接算成本库验收。

已读 `WrappedID3D12Device::EndFrameCapture`（d3d12_device.cpp:3249–3268）与 `WrappedVulkan::EndFrameCapture`（vk_core.cpp:3287–3308）：DriverInit之后插入原resource records、initial contents和needed refs，再进入CaptureScope。格式/原对象事实由后端记录，不要求应用声明某场景许可。Metal差异是原始24-byte typed表VA/ID及外部ABI需要既有协议解码，之前原生UE文件有完整事实而无协议记录，必须另外加声明副本才走已验证恢复路径。

实际修改 `WrappedMTLDevice::EndFrameCapture`：在结束活动capture的锁保护时间点冻结“存在typed表且无显式legacy协议”事实，于DriverInit之后、原resource records之前写已有当前协议65。它描述文件格式，不授予UE/PSO/shader/尺寸/RT许可；全部对象/地址/发布/生命周期/初态/提交检查及两RT设备flags不变。保留显式legacy版本，不新增场景等级，不在读取任意旧无版本文件时直接推断通过。无需应用手动注解coverage，也无需修改原capture。

T0 build6.860279s通过。`capture-protocol-T1`两fresh Native→原始capture→正常完整GPU API/CLI输出、事件/EID0通过：40MiB/offset128普通控制缓冲，以及独立3实例/输出24/Header128/贡献32、12KiB partial backing+offset12280的真正Native ray query；样例仅省略application coverage注解，实际typed来源/AS事实照常记录，导出核验各原文件只有一个后端协议65。两项11.528109s/8.666980s，不经过任何声明副本工具。

`capture-protocol-UE-native`本库原生UE截帧39.143088s成功，原capture SHA256 `1dc7eb0f6f20f307a7bc1b0d9ed8f18b1565d7e72f6c1ce2b79637ec6612a65b`。核验55个实际texture描述384×272、97个heap出生记录、原文件协议65、119-entry AIR/5次非零query、同帧PNG896×637；已知物理backing1,129,483,520B（heap children不重复计），用户配置哈希ea359e1a…不变。实际普通OpenCapture/整帧GPU已对该原文件完成，没有pre-submit override或声明修改；API0，125.048380s/peak1,342,210,048B，last10223/200draw/152dispatch/891texture/1133buffer、无strict/预算停止。此为执行通过，不把后述精确输出失败计为PASS。


**当前输出差异与可判别实验。** `capture-protocol-UE-whole/lossless-comparison.json` 按原point mapping/原方向精确比较PNG，16像素/16通道相差1；保持FAIL，不改容差。`capture-protocol-UE-resets/manifest.json` 两次EID0/整帧原生执行均API0（125.449538s/peak1,309,081,600B），但严格raw hash重复性FAIL。`variation-diagnostic.json` 保存四次原始输出的两两比较：一次reset与Native sampled RGB全部一致，另两次各2通道差1；所有重复差最大17像素/17通道/1色阶。区域[213,330,450,397]，不能据此推断原因或宣告同步恢复无缺口。原捕获完整119 AIR包含reassoc/contract及Native float SIMD sum，仅为浮点时序假设，不证明本差异起点；既有仓库PNG默认容差不在此直接替代原精确oracle。

`capture-protocol-UE-ray-events/manifest.json` 原文件5个非零query、PSO/function/root/table/真实indirect来源正反向、前后/EID0通过，153.343664s/peak1,306,902,528B，API0，无strict/预算停止。只证明实际绑定与定位，不能代替细微输出根因。`capture-protocol-contract-negatives/manifest.json` 两协议负例按重复header/未知version的真实格式契约构造，API4/CLI1、无GPUwait/strict，不把合法API变化当负例。

下一可判别实验是实际query消费点的已展示可写候选原生读回，正向/反向/前后/EID0重复：新增独立测试工具可选输出目录，按通用descriptor身份/实际对象/子资源取回，不改变生产资格/RT flags。只读展示出的候选，未知展示仍不能当实际访问；预算是诊断32MiB/selection、8MiB/object，不能成为后端支持上限。若RT中间输入/输出稳定而最终图像变化，沿后续native依赖链定位；若query输出不稳定，查对应实际typed资源/初态/同步，再与原生固定输入重复基准对照。没有选择最好的一次hash来宣称全通过。


**追加实测与下一依赖。** 当前库 `capture-protocol-T2/manifest.json` 32项scoped兼容/恢复集成检查通过，实际32.327215s，最大子进程RSS121,683,968B；包括显式旧protocol的普通、RT/heap/indirect/retired/linked输入和真实缺VA恢复拒绝。复用有来源的旧capture，只计当前库重放，不计fresh Native或最终T3。

`capture-protocol-UE-query-readbacks/manifest.json` 162.670096s/peak1,304,084,480B，API0、无strict/预算停止；33个原生readback共6,684,912B，11组（event,kind,resource）各三次正向/反向/EID0观察。七组texture子资源及两组buffer全部raw hash相同。两组buffer3115/12654 raw顺序变化：三次两两比较中4-byte值多重集及8-byte记录多重集均相同，差异分别限于前640/80个word，padding其余未变化；不能用整体hash给未定义或无序队列建立错误负例。实际原AIR5695:2701–2718有Native atomic.add分配序号后写两word记录，证明此shader包含原生原子追加机制，但尚未完整将每个root/table字段映射到上述变动backing，因此仅记录为强候选解释，不宣告最终图像原因已证明。AIR2699没有相应device-buffer store，可写候选并不等于实际writer；这也确认不能把访问展示作为实际执行证明。

当前M4剩余依赖收束为：1）沿已观测无序队列和稳定query纹理到后续输出的真实资源/命令链核实精确图像变动，再建立原生固定输入重复基准/适用oracle；2）候选稳定后集中官方sample、相关最终/生命周期/设备条件与启用后复验。没有新增后端拒绝、feature/EID/尺寸特判，也没有把此次严格比较失败升级为永久API限制。


**队列顺序根因已局部确认。** `unordered-queue-source-audit.json` 沿原始提交32650、root4310+38656及其真实CPU publication30868（原payload hash212dcaf2…）解析root+368/+376，经原typed表25映射到counter12022与输出12654。AIR5695的2701–2718确实用Native atomic.add取得序号，再往12654写两word/8-byte记录；三次8-byte记录多重集相同。其hash顺序不同属于无序追加的原命令执行结果，不能改成“丢失输入”拒绝或在CPU排序代替GPU。这是独立恢复/展示分类证据，仍不能把最终图像的1色阶波动直接归因到它；buffer3115原writer与后续图像链仍待核实。当前生产没有新增拒绝逻辑。

保持第一恢复批次实际修改与c88已达成M1–M3；本202生产capture流程及原文件正常OpenCapture/事件闭环已实测，但精确图像重复性尚待上述原因核实，T3官方/final/lifecycle/device/启用复验未跑。新的固定证据入口 `capture-protocol-evidence.json` 与 `checkpoint-capture-protocol-202bbb55/manifest.json`；包含strict比较FAIL、成功执行和局部诊断，不把这些合成“全通过”。


## 原生 RT 对象身份与合法空绑定：候选 3961dd8c

本组仍在同一 heap/动态绑定恢复批次的 M4，Sol high、旧未提交工作保留。MAIN/review 的通用恢复契约已纳入 AGENTS、执行入口和 ACTIVE 持续提示，明确覆盖旧场景许可/CPU 表达式准入及错误负例。未回退 14714 旧阻塞，未重做 c88 已冻结的恢复整改。能力开关仍 false。

### 根因、恢复义务与具体机制

固定候选202的官方样例验收发现真实身份缺口：原合法三角形捕获的 `MTLComputeCommandEncoder::setAccelerationStructure.structure` 实际非零目标改成不存在的99999999。容器导入成功；`DoSerialiseViaResourceId` 将非零缺失指针变为nil，setter的合法空解绑路径丢失“原目标不存在”的事实。随后dispatch失去AS，落入普通texture/grid拒绝，并已有GPU wait。`fixed-official-lifecycle-202/manifest.json` 是真实 FAIL，不是错误负例；后续生命周期当时未运行。

实际修改 `WrappedMTLDevice::ScanDescriptorMetadata`：在任何资源初态载入/提交之前，以原序列化 ResourceId 扫描 AS、Visible/IntersectionFunctionTable 的真实factory及compute/render/argument/nested table标量和数组绑定。非零目标必须已有匹配类型的原创建记录；零仍是合法空解绑。对象记录计入既有统一preflight账本；与coverage/AIR/shader索引推断无关，没有新协议等级、场景名、slot/尺寸特判或提高预算。当前类型/原生实例、AS build、pipeline/stage、alias生命周期、地址发布、必要初态及提交检查仍由原路径执行。该扫描只修对象身份的丢失，不声称新实现了GPU动态写地址或所有资源寿命预检。

对直接相关旧负例纠正的是错误阶段文本预期：compute table identity、visible range、nested visible gate现在接受具体“Missing or wrong-type Metal ray binding object”诊断；仍保留真实非零身份损坏、非零退出及合法nil→restore正例，不把合法变化命名成malformed。后两项只做语法检查，独立完整gate本候选未跑；compute gate实测结果见外盘，不混记。

| 对照职责 | 仓库具体函数及行为 | 本组 Metal 复用/必要差异 |
|---|---|---|
| 身份与重定位 | DX12 `DoSerialise(D3D12BufferLocation&)` 保存ResourceId+offset，从重放对象GPUVA重定位；`Serialise_SetComputeRootShaderResourceView` 执行重定位后的原绑定，`ValidateRootGPUVA` 跳过0。其缺失对象也可能清0，不能伪称DX12已有本组提前拒绝 | 维持已重定位对象与原API执行；本组区别原零/非零缺失，不直接改变通用指针反序列化，避免影响可遗漏对象 |
| descriptor 捕获/恢复 | VK `DoSerialise(VkWriteDescriptorSetAccelerationStructureKHR&)` 保存typed handles；`Serialise_vkUpdateDescriptorSets`→`ReplayDescriptorSetWrite`恢复对象绑定。后者明确允许忽略从未被使用而遗漏对象的descriptor更新 | Metal这些直接binding在capture给每个非零目标AddParent（AS另MarkASInitialReferences）；故缺factory是违约，不机械照搬VK“未用更新可忽略”到已保留parent的直接绑定。合法null保持 |
| 初态与物理 backing | 本卡已读DX12 `Prepare_InitialState/Apply_InitialState`、VK `Prepare_InitialState/Apply_InitialState(eResDeviceMemory)`按捕获时间点恢复；AS依各自recipe恢复 | 当前Metal原AS配方、Private输入、heap birth及typed地址恢复均保留；本组在它们可能提交之前验证真实目标，不用shader CPU表达式或清0替代 |
| 提交 | DX12 `Serialise_ExecuteCommandLists` DataUploadSync/跨queue等待；VK `Serialise_vkQueueSubmit`跨queue/wait信号处理 | 本组不改提交调度；Metal `Serialise_commit`、原等待/事件、CPU snapshot与seek依赖路径仍负责。现有v65 CPU snapshot应用等待已提交CB已核对，不能未经证据归因最终波动为这项缺失 |
| 展示 | 两后端 `GetDescriptorAccess` 的static/可用feedback展示与Native执行分开 | optional candidate/unknown不拒绝。这里拒绝的是原非零对象身份真正不可恢复，不是显示信息或GPU普通运算未知 |

### 同一候选的实际验证

backend/bundle完整SHA256 `3961dd8ca9e019fe91455260c8d8b3d910398110dedd4de17e1b7fa9626e5d52`；T0构建9.410s。GUI `3ba30e36…` 不变；用户RDHeaderView与renderdoc.conf原hash不变。GPU/构建均共享锁串行，子进程60s（UE180s）/3GiB监督；外盘 `B544/development/ray-binding-identity-*` 保存唯一产物与失败，不覆盖202/c88检查点。

- `ray-binding-identity-T1` PASS：官方原64×64三角形/程序几何捕获的实际AS引用损坏，API4/CLI1且无GPU wait；每种10轮失败→成功完整输出/原事件三方向/EID0同进程生命周期通过。分别54.068/54.969s，迭代基准后RSS增长0B，峰值82MB内。合法文件使用官方Native精确RGBA32输出，未只验OpenCapture。
- `ray-binding-identity-native-variant` PASS：独立80×48、seed7，两种官方Native scene；另一个command buffer执行不同slot的AS/函数表scalar/array合法nil解绑。每种两次原生完整61440B输出相等，新原始capture与Native、完整replay像素及事件/EID0一致，CLI两轮通过，24.282s。capture hashes三角形e2d1bcd0…、程序几何9a437321…，无需再改后端。官方ZIP pin4ee961f8…、license及shader736daf24…逐项审查记录；只复用已验证shader编译，当前runner/replay重新构建，没有重新下载或改官方源码。Native device query、默认/probe capability和AS size检查亦通过；生产flags未开。
- `ray-binding-identity-T2` PASS：32项定向集成32.863s，覆盖真实RT indirect/unretained、heap/retired namespace、普通观察变化不作许可、常量/linked graphics、depth、known pointer及真实非零VA来源缺失GPU前拒绝。使用旧capture在当前库重放，不能计成当前新Native capture或最终全部回归。
- `ray-binding-identity-UE-whole`：未经修改的原始UE capture1dc7eb0f…（原捕获库202）当前库正常整帧并两次EID0完成，126.731s/peak1303363584B/API0，无strict/监督停止。比较仍FAIL：真实视口源10734的384×272 packed10bit最多7像素R/B各1色阶；真正CaptureEnd呈现13031的900×640 BGRA最多2像素1色阶，三次原896×637 PNG按原integer point mapping差2/2/0像素。重复hash仍719b8a…/0661f49…，没有扩大容差、比较无关UI或提前开能力。

当前库UE query事件与existing table gate已实际通过（见下面最终结果），不是沿用202旧库结果。QT/ARC广泛生命周期、完整固定最终RT矩阵、启用及启用后Native重截/重放未跑。官方子集与本组生命周期不能合成M4全通过。

### 输出诊断的确定事实与下一动作

202历史且本轮首次记录的实验：`image-chain-T1`仅辅助工具编译失败；`image-chain-T1-fixed`误选无Draw的render encoder12941定位失败，GPU已Open但没有有效目标观察。随后按实际action.outputs/原encoder定位纠正为真实producer，不增生产EID条件。`image-chain-T1-observations`沿6个真实renderpass/3次EID0取样：HDR12564出现52像素/89half components变化，maxAbs9.536743e-5；其他被观测packed输出稳定。当时13068是其他editor窗口，**不是最终呈现**，已沿原CaptureEnd→13031纠正。`image-chain-whole`完整frame结束才观察，确证视口源10734也变7pixels/1packed step、最终13031变2pixels/1byte step；因此不是仅最后UI合成波动。

`image-chain-HDR-inputs`在实际HDR additive draw之前/之后观察三次，目标及10个显示readonly纹理的首mip/slice该次全部稳定。插入readback等待改变执行时序，不能计为整帧重复性修复；这些只是显示候选及观测范围，尚无原生中间baseline，也没有全cube/mip/绑定buffer证明。结合原真实atomic追加12654的8-byte记录multiset相等，只能局部确认顺序合法，不能自动解释最终图像。

下一组可判别实验应沿实际HDR producer及其恢复输入/依赖到10734定点取证，并补原生固定输入重复基准；区分原生浮点/无序结果与真实恢复/同步差异，再选择机制修复或正确oracle。不要第三次仅重复无变化whole失败，不把候选资源扩成整allocation CPU表达式证明，不继续邻近格式/功能放行。若需修恢复，仍先证实地址/已定义输入/真实提交义务。当前无需用户决策，持续目标ACTIVE。


**本候选最终追加。** `ray-binding-identity-UE-ray-events` PASS：153.397s/peak1303248896B，原文件未改，5个真实非零Native query事件3761/3870/4443/4605/4832的原PSO/function/typed root/indirect source绑定、正反序、消费前后及EID0通过。候选和恢复集合展示不冒充actual access。`ray-binding-identity-table-contract` PASS：1.472s，现有intersection table gate的合法nil→原table恢复三轮CLI接受，实际未知与wrong-type非零目标拒绝；无崩溃/strict。更新测试只接受更早且具体的身份诊断，未改变正/负例含义。

本组结论是：真实原生RT对象身份缺口已修，合法变体无需后端分支，官方输出/相关生命周期与同库UE执行/事件依赖通过；精确UE输出重复性仍FAIL、不能开启。固定源码/产物和全部本轮/202诊断及初始失败hash入口为外盘 `ray-binding-identity-evidence.json`、`checkpoint-ray-binding-identity-3961dd8c/manifest.json`。旧202/c88 manifest不可修改。下一轮遵循上面因果实验，不再用单纯整帧重跑代替定位。


### HDR 因果链诊断（同一 3961dd8c，未改变生产资格）

本轮没有重做已冻结的 AS/函数表身份整改或整套验收，也未修改生产后端/能力开关。继续 M4 的精确输出失败诊断；新增/修改的 replay helper 只是测试定位与原生 readback，不把 encoder/EID/PSO、格式或尺寸作为支持条件。完整实验、来源和中间失败在外盘 development/HDR-*，旧396/202检查点不修改。

先从原始 draw/附件定位：实际 additive SceneColor draw EID5260（encoder12902/PSO3680）之前已有 SceneColor 写入 EID5072（encoder12890/PSO3583）。两次只在 draw 后读回的独立实验，均见13个 SceneColor pixel/16 halfword变化，maxAbs 9.5367431640625e-5；并非单个 half ULP。前一 writer 的两个输入12599/12605也变，其余本次观察的纹理首mip/slice及四组绑定buffer区间稳定。原始 typed metadata、CPU publication和AIR明确映射至更早的 compute dispatch EID4815/PSO9354/function8851/library8850；实际AIR1329/1366是texture store，根4310+89856的字段0/16→typed表25的1186/1188→这两个真实目标。使用编号仅定位实验。库完整hash d3a2a8ffae9921496413b7285dc59d034dd1131b58c2d9767beaa6e402071701，原capture1dc7eb0f…未改。

辅助工具原先对同一event的两个target重复 force selection，并把第二次输入观察覆盖同cycle文件。这不是生产恢复修改；单独保留 `HDR-packed-writer-after` 的历史结果，不能据此宣称“所有消费输入稳定”。实际修正helper：按API dispatch与usage候选定位compute target，同一cycle/event只选择一次，随后观察所有target/输入。usage只是候选，actual store另由上面的原AIR/typed来源确认；未用展示结果决定后端资格。CPU source审计亦纠正：AIR %.1.unpack+32来自CBV12527+32，真实noise texture6236/slot1901；不能误读为4310+32（后者是另一个输出）。原映射及纠正保留在origin.json。

`HDR-packed-writer-grouped` 的三次 EID0→dispatch 同执行观察：两个输出分别2个hash，差102/118 bytes；四组根输入（4310+89856/576B、12480/6752B、12527/48B、23/144B）一致。13个已展示只读纹理首mip/slice中，10926/10935分别变化425/426pixels，位置范围x111–273、y127–130，其他11组稳定。这把根因继续收束到更早的真实输入生产链；不能用后续 float conversion/最终UI合成解释，也不能把未观察的资源称为已验证。

`HDR-native-fixed-input` 使用原metallib、完整实际读取的首子资源（原texture均单mip/单slice）、普通root快照及新Native typed texture/sampler IDs，在未注入Metal下独立执行同一kernel十次。五个输出每次逐字节相同，两个目标与replay cycle0完全一致。Native运行1.590s、build1.047s，无strict/预算停止。它是固定恢复输入的隔离因果实验，**不是原UE Native中间baseline，也不是光追验收**；不能凭十次稳定承诺任意浮点shader绝对确定，也没有扩大容差或改shader。原生fixture源码/JSON/输入来源/产品均在外盘，保留原Native library与精确binding拓扑，不用CPU计算shader表达式。

同库实际监督结果：`HDR-inputs-after` 127.571s/peak1303085056B；`HDR-first-writer-after` 129.136s/1302511616B；`HDR-packed-writer-usage` 125.834s/1303609344B；`HDR-packed-writer-after` 129.784s/1303298048B；修正后 `HDR-packed-writer-grouped` 127.839s/1303478272B。均API0且无strict/RSS/timeout停止，仅代表成功收集观察，不把原strict整帧FAIL改成PASS。各helper相关编译已实际通过；没有生产改动，因此未为诊断重跑无关回归。GPU/构建串行共享锁，180s/3GiB监督，唯一证据外盘，内盘27GiB可用。

下一动作以10926/10935的实际原命令writer、原初态与提交次序为入口；再用发生变化的一组固定输入验证原Native kernel是否精确再现相应输出变化。若差异由真实输入恢复/依赖造成，修通用机制并用独立合法Native变体验证；若实际原生程序产生无序/浮点变化，先取匹配原生证据再建立有依据的oracle。仍保持全部真实地址/资源/初态/依赖检查与最终开启门槛；没有新场景许可/拒绝，也未开启两个RT flags。


**因果链追加。** `HDR-native-input-cycle1` 用 grouped cycle1 的普通输入（变化的10926/10935，其余输入同值）与同一原库/新Native typed对象执行十次：五个输出各只有一个hash，两个目标每次与对应 replay cycle1 逐字节相同；运行0.256s，无strict/预算停止，复用已编译的隔离fixture。这精确复现“不同输入→不同输出”，并未扩大任何容差，也未证明更早输入的正确性。

`HDR-history-input-usage` 正常原始文件Open125s/peak1303986176B，定位4个RW候选：4765/4108、4778/4111、4790/4114、4802/4116，均实际indirect dispatch；之后4815才读这两个输入。原捕获indirect counts分别922、297、413、0；0是原记录值，实际Native仍读取当前间接buffer，不拿captured count决定资格。usage本身不证明store，前三条的原AIR/typed字段另核对；第四条0计数不声称写了pixel。

`HDR-history-writers-after` 132.872s/peak1303445504B/API0，按原事件选择前三个producer，观察两个target首子资源、原root与显式队列/count范围。三次输出该实验稳定，3115无序队列raw排列仍变化；插入更早pass readback及多次prefix重建改变时序，不能把稳定结果宣告为整帧修复。3115并非4778本shader实际tile-list输入：该dispatch原root4310+81920+32→slot1031→12640（65536B），root+200→12022；实际AIR加载这两个buffer。不能借此前3115队列的multiset合法性解释这里未观察的实际输入。原录制rootpublication30868/hash212dcaf2…、typed版本与库2725/2727见实验captured-root-map.json；candidate与actual读取分开。

下一单dispatch实验仅选择4778，不插入更早pass观察；已修改测试helper按显示readonly descriptor观察实际buffer区间，并保留其候选/undefined padding标记。要核对12640/12022、实际36B间接参数与源纹理；若本shader输入变化，则继续真实producer/提交恢复；若输入确实固定而输出变，才取同一原生producer的固定输入隔离实验。当前未新增后端机制/拒绝，不将局部十次Native结果作为UE Native重复基准或M4通过。


**单producer结果。** `HDR-single-history-writer-after` 127.747s/peak1302872064B/API0，三个EID0→4778没有更早pass观察。10926/10935输出及本次其余纹理/根参数/12022内容稳定。真实间接前36B均(297,1,1,413,1,1,0,1,1)。实际readonly tile-list12640三次raw顺序变化，整个4-byte值多重集相同；它是普通无序列表，不能仅据hash拒绝，也不能据其顺序变化断言图像原因。candidate buffer观察与明确root/count观察文件分别命名，未建立padding精确oracle。下一仅定位最后非零writer4790，禁止插入更早pass观察来伪装修复。

### 输出资格与因果诊断收束（2026-10-08，3961dd8c 不变）

采用本次用户修订，覆盖冲突旧诊断策略：GPU execution、output comparison、overall acceptance 分别记录；比较失败/待定不写整体 PASS。13 份未冻结 HDR manifest 已保留 `manifest-legacy-process.json` 后修正为三项状态，入口 `HDR-status-separation.json`；旧396/202/c88冻结证据没有修改。helper 成功退出文案改成执行完成/输出未验证，后续 Native capture runner 同样单列三项。AGENTS 与 ACTIVE 持续提示已同步。普通队列排列、全部中间资源字节、集合相等和单 Native kernel 均不是完整 UE 输出 oracle。连续两轮不收束就换方法；readback/等待的调度影响必须记录。

**可判别假设及 CPU 结果。** 仅定位4790的 `HDR-single-last-history-writer-after`：128.516s/peak1304215552B/API0，两组目标稳定，12640队列排列变化、多重集相同。其后仅定位原0-count dispatch4802的 `HDR-zero-history-dispatch-after`：128.071s/peak1303691264B/API0，真实 indirect36B仍297/413/0，目标两组变化1230/1233bytes；不能称0-count原生shader写出了变化，它只观察到此前执行的状态。多个 prefix/readback 实验改变调度，稳定结果没有修复含义。本轮停止增加上游快照，改用已有同一次执行的数据验证“坐标列表、索引及照明是否一起重排”。

`HDR-logical-pairing-audit.py` / `HDR-zero-history-dispatch-after/logical-pairing-audit.json`：原root字段148=408个regular、64=24列，AIR2727中真实读取的列表12022包含六个唯一屏幕坐标，cycle0→1排列为0→3、1→0、2→1、3→4、4→5、5→2。索引纹理10886仅六处变化，全部与同一排列吻合。按原shader布局映射414个实际探针（未分配尾部不建立字节oracle），深度12522的1656B全相同；ambient10911残差7B、directional10915残差144B/140half components/maxAbs0.00014495849609375、irradiance10919残差204B。全部剩余变化仅在六个adaptive探针，408个regular探针相同。不能只凭深度/集合相同宣称照明正确。

首次试算把directional系数当作x*6相邻texel，会制造0与非零的大差异；已按实际 `LumenScreenProbeFiltering.usf` 787–788 的x+coefficient*24分带布局纠正，最终audit只采用正确布局。没有修改生产shader或API路径。历史同资源12022在更早dispatch被用作counter，不意味着在后续生命周期/命令中它不能作为坐标列表；跨事件的语义不能机械沿用。

**原 Native shader 的具体数值机制。** 原CPU发布30868与typed表明确映射：dispatch chunk30619/PSO4089/function2713/library2712、root4310+63744的0/8→slots2312/2313→10911/10915；chunk30654/PSO4091/function2715/library2714、root4310+67840的0→slot2314→10919。原AIR2712:768–817使用group坐标PCG再quantized ambient store；AIR2714:197–249使用dispatch坐标PCG再store。安装UE源码 `LumenFloatQuantization.ush` 24–28与 `Quantization.ush` 13–24亦明确用物理坐标和frame index选择随机量化偏移。合法原子分配改变物理位置，可改变随机量化seed；这比“少数LSB所以允许”有具体原因依据，但尚未闭合directional上游残差及最终图像，不直接授予任何最终容差。源码/AIR的精确hash保存在audit里。

**方法切换：完整 Native UE，而非继续向上游拍快照。** 实际新增 test-only `MetalNativeFrameBaseline.cpp`：在未注入RenderDoc的进程中拒绝相对/覆盖输出路径，受限384×272完整视口、三帧后退出；普通warmup后设置引擎r.Test.FreezeTemporalHistories/Sequences，各帧只在完整viewport draw之后ReadPixels，无中间pass等待。外盘 `HDR-full-native-baseline` 实际运行47.596s、退出0、无RenderDoc dylib，实际加载原isolated RHI2496f103…；三个417792B BGRA输出完整收集，后两帧相对第一帧变化6353/9321pixels。**这没有固定所有cache/时间/渲染输入，也未独立证明该uninjected run的具体RT调度，不能用此较大变化给原capture设容差。** 它证明单独冻结temporal cvars不足以建立所需重复基准，下一实验改为同一个真实UE捕获帧的Native输出与其replay直接比较。帧末readback仍可能影响跨帧调度，未伪称完全无扰动。

Native工具构建曾有两次失败，均保留：UBA经symlink/physical路径写PCH账本失败（28.736s）；改真实外盘路径、arm64/-NoUBA/-MaxParallelActions=2后缺打包的renderdoc_app.h（25.836s）。提供仓库原header后12.810s实际构建通过；新Native source已实际编译，没有只做语法盘点。全部包/PCH/项目/产物在外盘；原capture项目未改。安装Engine executable与用户RDHeaderView原hash核验未变。

**同帧完整 Native 对照正在闭环。** 捕获plugin新增可选 `UE_METAL_NATIVE_VIEWPORT_OUTPUT` / runner `--native-viewport-output`：保存该次完整viewport draw的Native `FColor`（BGRA8、RCM_UNorm、linearToGamma=false），不改原shader、不在pass中读回、不把观察结果当后端支持条件；空环境默认捕获行为不变，失败按原render-thread顺序discard并恢复状态。同帧视口读回的额外原生copy及等待可能改变后续调度，匹配也不能单独当作生产修复。独立包本追加构建9.039s通过。

`HDR-matched-native-capture` 新原文件6adfdc3f857c652c0fb2614faadbcc28ea6abfb8d9657d6ba8f6559b362894e5；Native视口417792B hash0acdeb6729c1006559d1874218d4c448aa35f732015ba349044946262523fecc。119原AIR entries均已核验、5条实际bound query非零调度，不能只凭r.RayTracing/Lumen开关称实际RT。原ReadPixels路径 `ReadSurfaceDataInternal`→texture copy，捕获chunk34381记录384×272源12290→Managed临时texture14562。源原factory明确RGB10A2Unorm，原 `ConvertRAWSurfaceDataToFColor` / `FColor::Requantize10to8` 定义无gamma的整数转换；比较将采用这条原生转换，不采用PNG采样映射或任意容差。此时仅Native capture执行完成，replay/输出比较待定。后端与bundle仍3961dd8c、两个生产RT开关false。


### 同帧 Native 对照暴露的通用 transfer/初态契约整改（2026-10-08）

**假设与判别。** 继续同一恢复批次。新原始 UE capture6adfdc3f 的端到端 Native 视口对照会录入额外的原生 copy→Managed 同步→CPU getBytes。原396在分配预检拒绝：工厂14562虽然捕获期间出生，却被 hoist 在 CaptureScope 之前，被误要求帧初态。Native 内存1470734592B、初态1085311925B、快照20223016B均满足原预算；不能通过扩大预算修复。真正 common InitialContentsList 对该对象 written=false，原帧完整copy生产其已定义内容。采用该契约而不是工厂文件位置判断必需初态。

**具体参考与修改。** common `ResourceManager::Serialise_InitialContentsNeeded`（1051起）把 `WrittenRecord{id,written}` 区分创建payload/无帧初态与需要保留数据；多条记录按逻辑OR，不准false覆盖true。DX12 `Common_CreateResource`（d3d12_device_rescreate_wrap.cpp）将工厂保存resource record；Vulkan `Serialise_vkCreateImage`、`Serialise_vkBindImageMemory`重建原对象/真实绑定并强化debug transfer usage；DX12/Vulkan `Apply_InitialState`只恢复规定初态。Metal分配预检只对“无snapshot且明确written=false”免去不适用的initial要求，任何supplied snapshot仍要求完整布局，true/缺契约的必需状态仍拒绝。没有造零或删地址/producer检查。

29dcc5d9实际构建9.736s通过；新UE原file normal Open97.667s/API4，仍未GPU执行/比较，进一步停在slot-frame预检。独立Native transfer-only单纹理先证明原API顺序合法（Native与captured Native完整bytes相同），再用trace0.003s将拒绝定位于 `copyFromTexture`。不是没有typed table：确实缺普通texture copy的descriptor-frame处理分支。

DX12 `Serialise_CopyTextureRegion`（5289起）、Vulkan `Serialise_vkCmdCopyImage`（vk_draw_funcs.cpp1647起）序列化源/目的对象及子资源并对unwrapped Native对象执行原copy，记录CopySrc/Dst；不要求CPU求像素值。Vulkan `Serialise_vkCmdPipelineBarrier`（4545起）保留屏障对象/访问与队列映射。Metal沿同一方式补共享region/格式block布局、实际对象/出生/退休状态、编码器所属提交和整帧记账，处理region/whole/slice-mip三个Native texture copy overloads，不按格式尺寸逐项许可。原copy真正执行，普通pixels不是CPU表达式。`Serialise_synchronizeResource`旧实现只校验编码器，现保留Managed Native GPU→CPU同步命令；Metal Managed coherency确需额外接口，未添加中间host wait。CPU packed scalar读回使用已有capture/initial block布局而非 `ResourceFormat::Special()`一概拒绝；depth aspect与compressed CPU read仍保留独立限制。

3dfcfe9d候选构建修复24.527s通过（首次WrappedMTLResource opaque forward declaration编译失败33.525s保留），两个fresh独立Native BGRA8/80×48/单子资源、RGBA16F/37×19/3mips×2slices/stagingoffset256→512，完整 Native→capture→replay 三次EID0输出精确匹配。UE99.508s仍API4，trace定位CPU getBytes9,699,968；未GPU执行/未比较，不能继承此前396的GPU结果。

**第二个判别与真实契约。** 对应用CPU read要求 `waitUntilCompleted` chunk错误：UE通过completed-handler/host等待也可合法获得CPU值。DX12 `WrappedID3D12Resource::Map` / `Serialise_MapDataWrite`（d3d12_device.cpp2155）与Vulkan `Serialise_vkFlushMappedMemoryRanges`（1413起）恢复捕获的后续CPU写值，不重新执行原应用CPU计算。Metal getBytes也不序列化host目的地，不重跑CPU读，只验证身份/CPU可见layout与在前提交；后续 `ApplyReplayCPUBufferUpdates`（3056起）仍真正等待已提交GPU再恢复原CPU publication。不会由“展示未知”产生拒绝，亦不会用未来提交替代在前依赖。独立sample新增完成回调等待、RGB10A2 packed读回、whole/range copy，验证同类合法变化无需改后端。当前最终候选ab834869，构建9.572s通过；日志文字换行的中间构建失败2.781s保留。两个RT flags保持false，原无observer UE strict FAIL仍未闭合。


**最终ab834869已跑范围。** `HDR-native-transfer-final-samples` 五个fresh Native→capture→normal replay：BGRA8单纹理、RGBA16F数组多mip、RGB10A2与完成回调host等待、RGBA16F whole copy与完成回调、RGBA16F slice/mip count copy与完成回调。所有定义的首/非首mip及slice均实际取回，三次EID0完整bytes匹配对应Native；Native/capture输出也相同，不要求padding字节。Region/whole/range均真实Native，不改后端适配新case。原helper多subresource文件名覆盖问题已修正（0/0名称保持兼容，其余附mip/slice）。7负例：移除written=true源初态、追加false不能掩盖true、把无帧初态目标改written=true、真实未知copy源、mip64、origin UINT64_MAX、CPU row1，全部API4且GPU前无wait/strict。没有把删除普通shader producer或无序顺序当malformed；工具 `metal_device_texture_no_initial_gate.py` 已持久化。

`HDR-native-transfer-final-UE-whole`128.336s/peak1797505024B/API0，正常原文件Open、完整三次EID0→wholeframe无中间pass观察，GPU执行完成。完整source12290 packed RGB10A2三次raw hash均c20291c597cf5698b0a048299d129692688bbef63413b830eb1cb858967309ae；使用原 `FColor::Requantize10to8` 转为BGRA后三次hash均0acdeb6729c1006559d1874218d4c448aa35f732015ba349044946262523fecc，与同一次真实UE Native417792B全相同：changedpixels/channels/maxerror均0。GPU、比较、整体分列，scope比较EXACT_MATCH，整体INCOMPLETE；没有以PNG采样/小容差替代，也没有把此新capture结果当旧1dc输出修复。严格失败/中间编译失败均保留独立目录。官方/旧相关/RT事件和最终能力矩阵不继承396通过。

**下一判别实验，不扩大上游。** 原1dc无observer仍有小差异，本匹配capture6ad含新增帧末copy/sync。假设“帧内observer改变GPU时序掩盖差异”必须单独排查：test-only plugin将原viewport FTextureRHIRef保留，在ordered EndFrameCapture后、后续draw之前取同帧NativeBGRA，不将observer复制/等待序列化进capture；生产Metal后端不改。独立plugin8.885s构建通过，实际新capture正在执行。结果若完整Native匹配，继续同候选回归/RT事件最终验收；若仍变化，回到恢复/地址/提交/合法无序机制的判别，不加容差或拍更多pass快照。该实验默认关闭，环境空时捕获行为不变，确切NativeAPI/source/输出与证明范围另记录。


**帧外observer结果与根因（同一能力批次）。** ab834869新UE捕获49.883s/正常退出，原file9ad3e3a9745e6a1fd82dfe4b4dc2e7724257d6e39ceede2cbf04c2ae36ead2a6；同帧Native viewport BGRA a5f151bec7fd6bfc0c42b50d6a6bb1e0f5fc1f860fdfd3ea0cd07090d2b51a71。原119 AIR/5非零query；test-only插件在EndFrameCapture后对保留的BufferedRT10319读回，XML无新增viewport texture transfer。normal replay34.495s/API4/peak1719058432B，GPU NOT_SUBMITTED/preflight FAILED/comparison NOT_RUN/overall INCOMPLETE。typed表25/slot13632动态候选buffer12265真实frame birth、heap2939/offset1146880/65536B/Private+Tracked，无出生数据或API-unspecified证据。Native捕获日志明确prior-GPU-not-completed；是原采集机制跳过，不是地址对象丢失，也不是实际读证据。旧9ad文件不凭新capture代码取得缺失字节。

**本轮假设→实验→机制。** 假设未完成GPU前缀使真实复用出生输入未保存。原应用队列异步copy的未构建方案经审查放弃：仅队列排队不足以证明alias内存依赖，未来跨队列使用也不能假定等待内部copy。实际修复在既有CaptureHeapBufferBirth中关闭已提交GPU前缀，只轮询Native final GPU status而非waitUntilCompleted应用回调；所有未提交预约明确不跨越。单次200ms/累计2s观察等待预算（新capture开始重置），原16MiB/128MiB字节预算不增；超时/error仍保留原恢复检查、不提交观察copy、不造零。此前GPU已完成后才在内部队列复制、完成、记录出生位置真实字节，故不与未来应用GPU提交竞态。生产支持没有UE/名称/slot/ID条件。此捕获边界仍会改变调度；新同帧Native原对照验证包含同一捕获机制，不能把旧帧差异自动归因于此。

**逐函数对照。** 重新读取D3D12ResourceManager::Prepare_InitialState(Resource_Heap)使用GetUnwrappedWholeMemBuffer、Apply_InitialState(Copy/ForceCopy)恢复原backing；Vulkan Prepare_InitialState将真实memory复制到staging并分批提交/FlushQ，Apply_InitialState(eResDeviceMemory)依InitReq恢复copy及barrier。WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists在原Native执行前DataUploadSync、切换queue时DeviceWaitForIdle；WrappedVulkan::Serialise_vkQueueSubmit切换queue/等待semaphore时QueueWaitIdle再提交原命令。Metal复用真实字节与前缀完成边界，区别是不能公开raw读取整heap、capture锁内不能等应用completion handler，因此在实际buffer合法出生点读其逻辑字节并只等GPU状态。地址仍独立由typed identity/offset重定位；原提交/producer、optional partial访问展示不改，不CPU模拟shader。

f701cd1eed37e34a7459226f48fa9f4dd886a9963e4561bed1a612e72ab84b1e backend/bundle实际构建34.178s通过。HDR-GPU-prefix-native-independent：fresh Native→capture→API/CLI成功，出生时Native prefix status2（Committed，未完成），backend trace GPU-prefix-original-birth-bytes；12288B/heapOffset768/read12280/Header128/贡献32/输出24、3instance动态真query、无应用coverage声明，完整定义出生bytes及hit/distance/instance/miss/全输出与padding验证，事件/EID0/public partial访问查询通过。这里padding本身由样例明确定义才比较，不成为通用undefined oracle。样例只是本机制验证，非完整UE验收；官方/旧相关最终矩阵此hash尚未跑。当前fresh UE帧外observer重截，结果随后追加，不因进程退出成功将比较设PASS。ab四产品及Metal源码已外盘checkpoint-native-transfer-ab834869保存；所有失败原文件/日志保持。


**帧外同帧Native闭环结果。** f701cd1e fresh UE capture41.443s成功，原文件a6b893194ad92f867a64d2733dd614c33d83710d1c1db62b7f36780e437e2e63；完整Native384×272 BGRA297e75656561d24738ba23d0013ede688bf85ecb1a321eab7dcfc21fb278b04c。原119 AIR/5真实非零query、100真实出生contents（97直接完成、3实际命中GPU-prefix边界）、0新增observer texture copies。normal whole replay128.663s/API0/peak1750843392B，未被监督器停止、无strict；三次EID0 raw均4d70a1a6c161e897570f7beec56830e0c3d2878cd9ba8605f70b0799fcd90306。经原UE RGB10A2→BGRA整数转换后三次完整hash均与Native相同，零像素/通道差，无容差；GPU COMPLETED/comparison EXACT_MATCH/overall INCOMPLETE。证明本fresh帧不需要捕获内末尾observer才能正确重放；历史1dc strict FAIL原样保留，未宣称已推导出其全部微差因果。f701 frozen products/source与原结果在checkpoint-UE-output-f701cd1e；下一同候选相关回归、当前官方Native/capture、query事件及最终门槛集中验证，不逐诊断全矩阵。

**当前官方scoped结果。** HDR-fixed-official-native已实际通过f701 fresh triangles/procedural两场景、80×48/seed7/4帧/跨提交nullable scalar-array解绑合法组合，Native各重复两次完整RGBA32F相同，Native→capture→replay完整原输出和事件/EID0匹配、CLI成功。原Apple MIT archive4ee961f8与source逐文件核验、原shader736daf24及pin来源沿原官方manifest，未下载/覆盖新来源。六Native/default/probe/invalid/disabled/empty能力查询通过：本机Native compute/render均1，默认包装均0，精确过程probe仅compute1。五有绑定/无绑定sizes Native与包装一致。仅本组scoped比较MATCH，整体INCOMPLETE；两生产flags仍false，尚不能宣称全部最终门槛通过。


**同候选集中相关结果。** HDR-fixed-UE-ray-events normal原a6b89319捕获154.842s/API0/peak1750810624B，五非零query事件3430/3539/4112/4274/4501（仅diagnostic locator）前/后/倒序/EID0及原PSO/function/indirect/root/AS和typed绑定身份通过，public partial访问展示可用；本组不替代已单独完成的整图比较。HDR-fixed-related-integration35.063s完成原三类Private indirect TLAS、heap/retired namespace RT、shader常量、外部linked graphics/depth/pointer变体、inline来源及AIR CPU组；输出/事件匹配。HDR-fixed-official-lifetimes117.908s：每场景10次坏AS记录拒绝及10次成功完整输出/事件/reset controller生命周期，真实Native当前80×48输出作oracle，有限内存增长检查通过。HDR-fixed-transfer-samples4.972s五fresh变体全部Native→capture→replay完整定义子资源及三次reset相同；7真实初态/布局负例和4真实heap birth payload/offset/消费时序负例全部GPU前拒绝。没有缺普通producer/无序排列负例。HDR-fixed-qt-lifetime7.362s使用用户当前header源码、Qt5 offscreen模型退休/替换与header销毁通过，不能声称原rich delegate stack已在GUI复现/解决。当前Native样例以ARC编译通过；更广ARC/驱动生命周期最终矩阵范围仍以实际运行记录为准。

固定候选完整converted IR/RT、根参数/descriptor heap/AS/frame geometry与相关损坏/关闭矩阵正在运行；未完成部分不计PASS。全部序列共享GPU锁、有限timeout/内存，外盘产物；内部仍27GiB。两flags仍false。checkpoint-UE-output-f701cd1e固定保存此前源码/四产品/原结果；后续当前manifest索引会单列，不改写该检查点或旧hash失败。


**完整矩阵总预算与续验。** HDR-fixed-full-IR在1500.003s总预算结束（exit124），不是单项输出失败。所有79个Native/capture/replay正例均已实际完成并记录对应f701哈希；原契约负例已完成至最后一组global/heap-AS/private-frame/multi-indexed几何，最后legal-original-positions的CLI日志已结束但未取得完整退出/API记录，不能推断其通过。进程盘点无残留GPU/构建子进程。保留原总预算失败及最后RUNNING manifest；执行/比较/整体为已完成范围/已完成范围MATCH/整体INCOMPLETE。只在HDR-fixed-full-IR-resume补最后一组、initial-list-close与失败→成功生命周期，不重跑79正例或全部矩阵。最后一组完整geometry合法控制已经通过，其他结果随后追加。该失败为监督总预算，不扩大生产资源数字上限、不增加场景许可。


**固定候选矩阵续验闭合。** f701cd1e原full-IR在1500.003s总预算中断的记录与末组RUNNING不改写。独立续验last-geometry-contract 51.801s、initial-list-close 3.381s通过；failed-close第一次120.096s超时来自测试外层wrapper与Native binary重复获取同一GPU锁，GPU尚未提交，属于测试调度错误。移除外层锁后Native自己单次持锁，2.2455s通过：三类真实坏输入拒绝→正常打开/完整RT输出/placement字节/EID0/action/EID0共10轮，增长1490944B，无strict。原失败保留manifest-before-lock-diagnosis.json，替代结果明确引用HDR-fixed-failed-close/manifest.json；整套完成范围汇总HDR-fixed-full-IR-completion/manifest.json，79正例及最后契约/关闭范围由明确续验补齐，不重新跑已完成范围，整体能力验收仍INCOMPLETE。

**跨尺寸UE验收正在比较。** 同f701候选的新352×256捕获45.702s完成，原capture adb26f76a626133a95ff1e2861db4922cce30993c7e6affdbd2bb946d897593a；帧外同帧Native BGRA b4aaa25b6c35278fb5b1a8f334bac01892a43f6b739b95d1e694d35d11176f1d，原AIR全部审查，真实RT调度存在。仅Native捕获已完成，整帧重放/比较尚未完成，不能计PASS。BufferedRT10948只在diagnostic-target.json定位，生产路径没有尺寸、标签或ID许可。

**数量校正。** 按当前full-IR实际manifest复核，完整Native/capture/replay正例为78项（脚本末尾亦为seventy-eight），前面79为记录错误，不代表额外通过；completion汇总已更正。工程进度依据恢复机制、完整UE输出与合同范围，不依赖案例计数。

**跨尺寸完整输出通过。** 同f701候选352×256原adb26f76捕获，normal whole replay129.141s/API0/peak1868464128B，三次EID0 raw均cdae21f8286362103a379a93588e09a6ea527229b528988adc3fedb8ebe8e009；使用真实UE原整数转换后三次BGRA全与同帧Native b4aaa25b相同，changedpixels/channels/maxerror均0。HDR-fixed-UE-legal-variant-whole/manifest.json为GPU COMPLETED/EXACT_MATCH/overall INCOMPLETE；没有中间pass observer、容差、后端尺寸放行或用户工程修改。第二份事件验收仍在执行，生产capabilities与开启后重截/复验未完成。

**第二份UE事件完成。** adb26f76原352×256捕获，当前f701 query事件154.474s/API0/peak1833484288B，无strict或监督停止；119 AIR/5真实非零query的原绑定、正反序、前后及EID0通过。HDR-fixed-pre-enablement-evidence只汇总本候选实际完成范围，当前附加frame-family检查仍运行，生产compute开关尚未启用。启用时依DX12 CheckFeatureSupport OPTIONS5原生tier截限和VK物理RT feature/handle-capture-replay恢复条件，保留Native设备谓词；render-stage与Metal3 primitive-data样例独立未认证。开启后官方必须选择capture-production并检查公开谓词、UE必须--production-capabilities（无过程probe），固定新候选再完整验收，不把f701结果算作新hash通过。

**附加frame收集错误与续验。** HDR-fixed-frame-family已记录七类API/CLI成功；首depth-background_d16实际stdout完整输出/reset成功，但汇总未保存退出记录时因空日志路径已不存在而FileNotFoundError中断，不计该项通过。这属于收集脚本问题，不是GPU失败；保留原FAIL，修正为在持有日志文件描述符时读取，路径缺失时保留FD内容；HDR-fixed-frame-family-resume仅补未记录的depth范围，不重跑此前已记录范围。

**深度历史oracle校正。** 收集修正后首d16第一帧输出/CLI成功，第二帧API17（首dispatch结果不符）。核对原Native capture源：capture0 nextImage=other、capture1 nextImage=image；两帧共享资源连续使用，原Native完整结果序列122/186/122。原已保存第二帧replay_2.log明确before186/after122，原回归克隆却两帧都传122/186，属于错用第一帧oracle，不是小差容差或后端机制失败。保留HDR-fixed-frame-family-resume第二帧API17，修正为每帧按原Native状态序列的phase，HDR-fixed-depth-phase-corrected仅续验depth范围；生产路径没有新增格式、事件或结果分支。

**Compute开启候选（未验收）。** f701当前附加frame/depth范围已真实通过，两个UE独立尺寸各完整同帧Native零差三reset及事件/绑定、官方两scene/合法Native组合、78 IR/相关机制/真实损坏/生命周期已完成。源码WrappedMTLDevice::supportsRaytracing改为直接返回Unwrap(this)->supportsRaytracing，删除过程probe资格分支，不按capture类型/UE名称/设备型号放行；无Native支持时仍false。FromRender保持false并说明独立未验门槛。对照本仓库DX12 CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5)保留Native tier并截限已支持版本；VK物理rayTracingPipeline/handle-capture-replay检查需要真实Native能力和恢复机制。本轮没有新资源/地址/提交放宽，前述真实损坏拒绝仍在。新的产品哈希尚未构建/验收，不把f701结果算新候选通过；下一无probe的官方capture-production与UE--production-capabilities、同候选相关/完整IR验收。没有物理不支持设备可实跑，只可依据直接Native委托证明query不会伪造支持，不把该设备路径记测试PASS。

**开启候选实际构建。** f1ba1862c1801e7743761b8400660412d137051d107ca138b6e71932e293e54f backend/bundle，构建7.355s成功；GUI3ba30e36/CLI07b1f070不变。checkpoint-production-compute-f1ba1862冻结132源码/四产品，当前官方capture-production正在执行，必须实际查询published compute支持且无过程probe。GPU/比较/整体尚未完成，任何旧f701结果不继承为f1ba通过。Native不支持物理设备没有测试输入，记录未跑，不编造该设备PASS；query直接Native谓词不提高硬件能力。

**公开能力路径初步结果。** f1ba官方两scene fresh Native重复两次/production capture/完整RGBA32F/事件reset/CLI均通过（80×48/seed7/4帧）；capture triangles888741c4/proceduralf8f1c186，Native完整hash1376c538/bcec318c。六能力查询Native1/1、所有包装default/旧probe1/invalid/0/empty均1/0，说明公开compute跟随Native且旧probe不再授予资格，render未开；当前M2 Pro物理不支持设备未跑。原五绑定/无绑定sizes Native与包装一致。UE--production-capabilities fresh capture45.101s，1cba9f6194c66a57c4b1da237efad4a7bb8970b49eaac44c2e75d1b15cdaf262；119 AIR/5真实query、过程probe未观察，帧外同帧Native已保存。normal整帧GPU/输出比较正在执行，整体INCOMPLETE。只计f1ba当前实际结果，不继承f701或旧Native。

**开启后生产UE完整输出。** 同f1ba候选、无过程probe、原1cba9f61捕获，normal OpenCapture whole-only129.153s/API0/peak1838809088B，无严格诊断/监督停止。三次EID0 raw均ceb3cd1b0d776f75c7484e8c98053349979bcfa56c97627913ab65dea321becc；原UE整数转换后全部BGRA与同帧Native724a0d4e02237b4ac4a1662d9d4df75d0836cb01311453ffca9a7dae468a6965相同，零像素/通道/最大误差。production whole manifest GPU COMPLETED/EXACT_MATCH/overall INCOMPLETE；query事件/绑定正在执行，当前候选相关/完整IR门槛仍待跑，不把开启前结果继承。

**开启后UE query定位。** f1ba同原1cba9f61文件normal事件验收155.237s/API0/peak1810350080B，无strict/监督停止；五非零query的原PSO/function/root/indirect/AS/typed资源绑定、正反序、前后及EID0通过。完整Native图像已独立EXACT_MATCH，事件结果不能替代图像；当前剩余为固定f1ba独立通用变体、相关机制/生命周期/frame范围及完整IR复验。

**开启后独立恢复样例。** f1ba fresh HDR-production-native-independent13.348s整体runner0，Native/capture/API/CLI、完整已定义heap出生字节与query输出/48事件EID0/公共partial展示匹配；query_birth_pending=true，真实GPU-prefix-original-birth-bytes命中，无应用coverage声明。三实例/active2、heap12288/offset768、读12280、Header128/贡献32/输出24的合法动态组合不需改后端，capture866b749d0c89fbe782898013f66e1642b38ddb1a04bbd6c38df8bb3dabacf749。测试manifest不再用固定production_RT_enabled=false宣称当前源码能力状态，而记录本单项不认证全项目；实际公开Native谓词另由能力gate验证。驱动冻结源码与四产品起止hash不变。当前相关集成33.399s已成功，后续生命周期/transfer/Qt/真实坏输入继续串行，不以本单项替代完整UE或最终矩阵。

**开启后相关固定候选范围。** f1ba相关集成33.399s、official controller失败→成功完整输出/事件reset生命周期117.691s、五fresh独立Native transfer类型/格式/mip/view/区域/whole及callback等待变体5.330s、真实heap birth payload/offset/初态/时序坏组1.901s、Qt5用户当前header退休/替换/销毁3.724s及7真实transfer初态/范围负例均成功。HDR-production-related-suite标scope COMPLETED/overall INCOMPLETE；各子项输出匹配或真实损坏GPU前拒绝分开记录。Qt项不证明原rich delegate崩溃GUI复现，也不把物理不支持设备/Metal3/FromRender未跑范围称通过。当前frame/depth同候选追加正在执行，完整IR随后一次固定集中，不逐诊断全量重跑。

**M4固定最终矩阵执行中。** f1ba HDR-production-frame-family当前42 API/CLI检查完成，texture/texel/linked/frame-load及7类depth第一/第二帧Native正确phase、deferred/parallel、完整像素/事件/reset通过，无strict；这是当前重放结果，不继承旧库通过。HDR-production-full-IR已启动同候选完整转换光追矩阵：2100s总监督预算按前次1500s刚好截断末组的实测耗时预留，单项原有界限/生产资源预算未变；failed-close仅Native自己持锁，不再外层重复持锁。已经完成范围原样记录，progress.json标GPU IN_PROGRESS/已完成范围MATCH/overall INCOMPLETE，完整终止前不写全部通过。


### Compute RT 能力开启与最终验收完成（f1ba1862，2026-10-08）

当前对应compute门槛M1–M4已完成，不能扩称全部Metal3/render-stage RT。实际生产 `WrappedMTLDevice::supportsRaytracing` 委托Native谓词；无过程probe、无UE/pass/shader/hash/EID许可条件。`supportsRaytracingFromRender` 仍false。最终验收manifest：[FINAL_COMPUTE_RT_ENABLEMENT_f1ba1862](/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544/development/FINAL_COMPUTE_RT_ENABLEMENT_f1ba1862/manifest.json)，GPU COMPLETED/输出 MATCH（UE EXACT_MATCH）/overall PASS，范围限定本机Native支持设备、已验API恢复/预算和所需sample/UE负载。各单项本身不认证全项目，仍保留其overall INCOMPLETE并指向这个最终决定。

| 同一f1ba候选的实际完成范围 | 结果与依据 |
| --- | --- |
| 构建/冻结产品 | 7.355s成功；backend与bundle均f1ba1862c1801e7743761b8400660412d137051d107ca138b6e71932e293e54f；GUI3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91，CLI07b1f0703741e29a2b68fc7c19f3a6a62b259ebe21c329ebc8300f00cf45be1c。冻结132源码/四产品、driver源码与起止产品hash核验未变 |
| 公开能力/官方Apple Metal2 | 默认及旧probe/invalid/0/empty包装查询compute/render=1/0，Native=1/1；两scene生产capture需实际published compute支持，Native各重复两次、全61440B RGBA32F及事件/reset/CLI相同；80×48/seed7/4帧、跨提交nullable scalar/array绑定合法变体，原MIT/archive/shader哈希核验 |
| 真实UE/Lumen公开能力截帧 | 无probe、45.101s重截；原capture1cba9f61、119 AIR/5非零query；完整384×272同帧帧外NativeBGRA724a0d4e；whole129.153s/API0/peak1838809088B，三EID0完整raw均ceb3cd1b，经原UE整数转换与Native零像素/通道差，无容差。原stream没有viewport observer复制 |
| UE事件/绑定 | 155.237s/API0/peak1810350080B；原PSO/function/indirect/root/AS/typed资源，5实际query前后/倒序/EID0通过，partial展示不代替实际图像或CPU数值证明 |
| 独立通用恢复变体 | 13.348s fresh Native/capture/API/CLI，未完成GPU前缀实际命中heap出生边界、3instance动态真query、偏移与大小合法变化、无coverage声明；完整定义bytes/query输出、事件/reset/partial展示相同 |
| 相关恢复与生命周期 | 当前相关33.399s、官方坏AS→正常完整输出/事件/reset生命周期117.691s、五fresh transfer/format/view/mip/region/whole/callback等待变体5.330s、真实heap birth坏输入1.901s、七transfer契约负例与Qt模型生命周期3.724s通过；具体scope不扩称原Qt GUI崩溃复现 |
| frame/depth相关 | 当前42 API/CLI检查成功：linked/texel/frame-load、D16/D32/D32S8与frame/deferred/parallel两帧正确Native phase、完整像素/事件/reset；记录原收集失败和错误phase oracle，不将其改写通过 |
| 最终转换光追矩阵 | 当前完整1453.944s/exit0，78 fresh Native/capture/replay完整正例、36契约/实现限制控制族及initial-list-close完成；10轮三类失败→成功/full output/placement/EID0/action/EID0关闭重开，growth442368B。此前f701的1500s中断与重复持锁失败不继承；此次一次完成、无总预算停止 |

**剩余依据与未验证范围。** render-stage RT尚无自己的sample/UE执行门槛；Metal3 primitive-data/GPU-address官方sample另验，不能从Metal2/converted compute通过推导。实际Native不支持的物理设备本机没有，明确未跑；直接Native委托不会伪造支持，但不把源码推论记录成那个设备实跑PASS。NaN/Float4-NaN几何是本轮沿用的实现拒绝控制，未独立核实Native合法性，不能把“拒绝成功”称为Native API禁止这些值或新证明的malformed。普通producer删除、展示未知、无序排列不是本轮新增资格/负例。历史1dc strict图像FAIL保留，新帧成功不推导旧微差全部因果。AS内部查看、RT单步、RT Pixel History不承诺。

**状态收尾。** 最终汇总保留GPU完成/输出比较/overall独立状态；生产compute验收PASS仅对应明确范围，未跑内容不算通过。当前用户RDHeaderView哈希0e66d6ee、renderdoc.conf ea359e1a、实际installed UnrealEditor92a29c07均核验未变，安装引擎/原工程不覆盖；所有新产物和唯一证据外盘，未自动提交、推送或reset。compute目标达成后停止此持续任务，后续新能力按通用API批次另外推进。

**持续任务已停止。** 应用automation_update返回automationId=metal、deleteStatus=deleted（原名称“Metal 光追持续集成与启用”），已按对应compute目标达标的原约定停止。记录仅操作结果，不改写旧持续策略/失败证据；当前repo未提交/推送。
