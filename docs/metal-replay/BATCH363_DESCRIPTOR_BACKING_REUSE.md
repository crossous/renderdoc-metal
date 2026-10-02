# B363：descriptor backing 的逻辑退役和物理复用

持续目标仍是实际 UE5.8.3 Testproj 整帧 replay 正确，不以本批定向通过结束。
保持现有改动；没有提交或推送。完整 UE 尚未提交 GPU replay，UI 尚待解锁。

## 实际捕获依据

0649fac867fa8b98fd114494c4cf4ce0ad709983529e9ba8fbd0434fd97ffdf8 的
Native layout 审计包含 12 处 typed descriptor backing 相关 overlap。
新增 `util/ue/audit_ue_metal_descriptor_backing_aliases.py` 以 ResourceId、
slot event 和 encoder→command 归属核对 API 顺序，不执行 GPU 工作。

旧表 15284/15357/15492 均已 free 所有已知槽位；这些表的旧直接 CBV root
使用都已提交。新表 16040/16094 复用的旧普通 buffer 15563/15575/15577/15583
的已知直接或 inline 引用也已提交。12 对均未发现 birth 后旧引用。
提交不等于完成；此证据只满足 replay 既有 wait-before-CPU-restore 路径的
前提，不授权对整帧提交，也不通过 raw VA 判断资源身份。

## 实现

按仓库 D3D12 `d3d12_device_wrap.cpp::CopyDescriptors` 和 Vulkan
`vk_descriptor_funcs.cpp::ReplayDescriptorSetCopy` 的逻辑 shadow 机制，区分
descriptor 槽位身份与底层分配/GPU 对象生命周期。v25 仅记录预检证明的
old/new ResourceId 对：旧表全部槽位退役、旧已知消费者所属提交已 commit、
Tracked Shared placement、GPU-authored table 排除。复用后旧逻辑 backing
不得再绑定，但保留 Native 对象，不调用隐式 makeAliasable。

useHeaps 的保守提交快照可能同时包含退休 A 和新 B。恢复 A 的 captured bytes
后必须重新编码相交 B 的活槽位；退休 A 的死槽位不再将 B 的 GPU 字段清零。
GPU expected destination 不通过 CPU overlay 恢复。v25 在提交前验证活 CPU
descriptor bytes 与当前 logical source 完全一致。

连续捕获揭示上一帧已明确 makeAliasable 的退休表仍通过 history 和全局
table 列表引入下一帧 initial resource section。capture 记录活槽位和 source，
仅在明确退役、无本表活槽位、无其他活槽位引用时从冻结 initial history/table
引用排除；保留历史供后续 generation，直接 frame 引用仍会记录该对象。

## 验证记录

精确 c2e886e548722e5243b17827fe7e1d658b0f196cc2cfb06593d632919b186cd6：
`metal-table-alias.BgUren`，retained/unretained × frame/background input，
8 捕获、32 次回跳通过。旧 A root 为 source offset4；退休后 B root 为 offset8。
两者 GPU 输入开始相同，producer 再写 80；GPU 数值和画面为 122/186→225/161。
原生和 replay 均通过 Metal Validation；实际 A/B 重复物理快照已从 XML 验证。
17 alias +27 graphics ×4 =176 API/CLI 反例在 GPU 提交前拒绝。

后续补充 encoder 在 dispatch/draw 时的当前 slot source 和 producer source
引用归属；该精确最终库的定向、Private/MRT、旧 21 类和全量结果待完成后更新。
之前 B362 精确57ba6f4a…全量308/7786/3080已通过，growth11943936B，
不能替代本批最终库验证。

精确最终库246a9d5cf76bf41746f00aa6899ed244ceb8aaae85a512eae3b7a0ab4dca2748：
`metal-table-alias.cNOUrC`，8/32/176通过；`metal-async-alias.rDsXd8`，
Private encoding-before-commit兼容8/32/220通过；`metal-frame-mrt.Yvvfxf`，
跨kind MRT兼容8/32/331通过。旧21类674反例已通过；全量308/7786/3080通过，驻留增长13205504B。
