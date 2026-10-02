# B399：sourced延迟StoreAction v55（定向通过，全量进行）

UE MetalStateCache创建非memoryless color目标时StoreActionUnknown，MetalCommandEncoder结束前设置最终动作。
复用现有Native ResolveDeferredStoreActions：局部回放提前结束先保留全部Unknown color/depth/stencil；完整回放按原setter。
对照RenderDoc Vulkan vk_state.cpp使用STORE进行局部load pass、D3D12保存rpRTs/rpDSV的已有处理。
v55预检按encoder/attachment记录Unknown，合法Store setter消费计划；必须在serial或parallel end前全部明确。
颜色索引、depth/stencil存在、encoder活性/所有者先验证，未来depth/stencil初始化仍分aspect证明。
未改变Native capture或原始store语义；StoreDontCare/Resolve和无完整最终动作尚不在此有界sourced范围。

极小夹具在draw之后才设置最终Store，首次/末次draw的局部seek会停在setter之前。
serial/parallel/5MRT、D16/D32/D32S8、future heap depth及counter组合18captures/72cycles。
新gate分别删除/破坏每个最终setter、错owner/attachment及放到end之后，全部应在frame GPU提交前拒绝。
脚本 `bash util/buildscripts/scripts/test_metal_descriptor_deferred_store_macos.sh`。
当前仅对象编译；B398精确全量运行期间不替换库，整帧UE/UI未验收。持续目标active，无提交推送。

实际UECPU store audit：89passes、94 Unknown→Store setters，138有效附件final全为Store，owner错误0、未解析Unknown0。报告ue-heap64.store-action-audit.json。
10个真实UE graphics PSO按原metallib/functions/linked functions/blend/depth等参数Native编译全部通过，无command queue/CB/GPU提交；覆盖无fragment深度及无attachment UAV PSO，不能替代Native执行或全帧验收。
文件extract_ue_metal_graphics_subset.py / metal_graphics_compiler_probe.mm及ue-graphics-compiler-inputs/manifest.json、ue-graphics-compiler-reflection.json保存精确来源。

精确库f12b4ea6807ef21bc63512f982cc13a1770360e6fc2b160a8e0fb62774ed3ceb，metal-deferred-store.2bbKWB：18 captures/72 seeks/354 API+CLI negative groups通过，全部Native像素和部分pass结束后的depth/stencil读回正确。v55 full deferred-store-combined-regression.log准备运行，真实UE/UI仍未通过。

2026-10-02 精确f12b4ea6…全量通过：308 captures/7786 malformed/3080 lifecycle，growth1441792B，结束hash一致。新capture参数候选期间仅对象编译，不链接。定向18/72/354、全量、人工UI分别记录；真实UE及人工UI仍未验收。
