# BATCH354：捕获尺寸与帧起点的过期描述符退役

2026-10-01，持续目标 active，未提交或推送。

## 实际重新截帧

Testproj FirstPerson 的 `UE58_small_viewport_8ebcbfa6.rdc` SHA256
`8ebcbfa6dac00e0af720bdc8a0358dff820aeca1d5a7c7d23d3f277c95af2d7f`，
29,850,269 B。会话20261001-125546，plugin cc32c4c2…，库5b08e331…，
isolated MetalRHI feeed228…；自动退出通过，安装MetalRHI ace97360…未变。
插件实际日志320x240，原2744x1690；UE官方SystemSettings命令行override
确认Nanite streaming pool32MiB、initial root256pages，不改项目配置文件。
576MiB buffer消失，最大buffer128MiB；但声明8个512MiB heap共4096MiB，
旧大纹理仍有8160x8160，不能以尺寸或文件大小宣布回放负载足够小。
124个CB、peak32、166render pass、389draw（327indexed/32direct/30indirect），
说明受控区间包含多次编辑器绘制；完整GPU replay仍未提交。

CPU逐槽：1877帧初live/initial bytes全匹配，1865帧末live。104/104 producer
及2705inline VA字段匹配、778普通IR绘制常量合法；生命周期、inline、
producer、frame value epoch问题0。初始source身份缺失9个；全部由CaptureBegin
后的连续10个free中的匹配generation/type项退役，在第一个driver资源创建
之前，不存在GPU消费。帧末source问题0。audit保留全部原始9条问题，并新增
leading_slot_retirements和initial_binding_issues_retired_in_prefix，不自动忽略。

## 对照源码与修复

Vulkan `vk_descriptor_funcs.cpp` 的stale-binding处理明确支持资源已释放但
shader不使用的descriptor。UE `FMetalDescriptorHeap::FreeDescriptor` 使用
DeferredDelete；刚resize立即CaptureStart会冻结旧slot，下一tick才free。
捕获插件新增可选env尺寸控制，FViewport::AsSceneViewport / SetFixedViewportSize
和SWindow::Resize，结束后恢复尺寸。随后加普通Draw+2秒editor ticks的warmup，
让尺寸变更与延迟退役发生在capture前。无尺寸env时原有按钮行为不变。
使用cached clang参数在隔离目录编译，不跑UBT或修改Engine；安装插件前保留
源码和dylib。最新warmup plugin SHA256
`ec8301bed467b7361dacce34ce25e4c9dcb461373fef48aabcae6aa798946ea5`。

Replay v16只接受CaptureBegin之后紧邻、没有任何其他driver操作插入的free
前缀（最多64项）。先证明slot generation/type匹配、frozen bytes完整且匹配，
然后在Native初始表清除所有声明GPU identity字段，保留普通metadata。逻辑
slot仍由实际free事件退役；SourceID/代际错、删除free、推迟free、非空free、
frozen bytes不匹配均拒绝。GPU-written table也只允许这条非消费前缀free；
不能拿这个规则放宽帧中CPU更新或忽略GPU完成。旧contract行为保持。

## 定向终端

库SHA256 `4d185570192cf8c94e510ae78afbb38807092df6293ce7f1498a8b368ba6279e`。
`bash util/buildscripts/scripts/test_metal_descriptor_retirement_macos.sh`通过，
日志metal-retirement.f0Cg9Q。CPU/GPU-written两种table、4份捕获，expired
buffer/texture来源没有creation保留；EID0的VA/TextureID全清零且普通metadata
不变。16轮seek GPU累加308、像素186/122；90组API+CLI反例通过。
B353相同5b08e331…全量308/7786/3080通过，增长12795904B；v16完整回归
尚未跑、人工UI待解锁，完整UE replay仍未通过。继续重新捕获稳定尺寸帧、
审计heap重用及资源预算，然后接间接绘制、parallel/MRT/signal路径。

## 稳定尺寸后的实际捕获与后续验证

warm plugin ec8301be… 实际捕获 f744c4130b9650ea6bb364fa2864c5c22cf29054dd43941a20b5824ef2e11a8d，
41,830,450 B，session20261001-133455，owned UE52700正常退出。
44517chunks、scope27012、126CB/peak33、170passes/393draw；8×512MiB heap仍存在。
帧初1678/1678 frozen contents/explicit source全匹配，帧末1825且来源问题0。
frame epoch/inline/producer/lifetime问题0；112/112GPU producer、2737 inlineVA、
786 ordinary draw constants。连续前缀free9个但没有缺失初始来源，warmup消除了
前一份resize捕获的9个raw source问题。

v16库4d185570…现已通过21类/674API+CLI反例完整descriptor定向套件，日志
retirement-descriptor-suite.log。这是定向，不是v16完整308-frame累计回归。
B353库5b08e331…全量308正例/7786反例/3080生命周期已完成，增长12795904B。
最新v17/v18物理alias扩展及精确库测试见B355；人工UI仍待解锁，UE完整GPU
replay仍未运行。
