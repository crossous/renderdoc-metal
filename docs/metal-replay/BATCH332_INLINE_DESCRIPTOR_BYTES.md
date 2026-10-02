# BATCH332：compute inline VA 的显式布局与真实重定位

2026-10-01，本地 M2 Pro/16 GiB，HEAD `c4be68bb7fce662e4dd8498408981fa2e824c3b6`，未提交/推送。

新增 chunk1402 `MTLComputeCommandEncoder::DeclareDescriptorBytes`，不改变旧编号或
section16。受限coverage v3下，producer在实际compute encoder的setBytes前声明
UInt64x4 `{bindingIndex, offset, count, stride}`；明确字段为VA64，binding<31，
总长度<=4096，aligned/bounded。未声明、声明晚于使用、未知/结束encoder、
冲突布局与无法唯一解析的VA都拒绝。只有声明的8字节字段重定位，普通常量不扫描。

CPU preflight按创建/声明/使用/结束顺序检查，未来placement地址只在创建后可用；
actual setBytes重编码为replay VA+offset。沿用原有GPU完成等待、初始内容恢复和
frame resource重建，不更改seek语义。UE源码的CBVTable确为已知VA数组；这不
表示普通SetShaderBytes全部都是地址，也不表示render stages已经支持。

夹具 `metal_descriptor_inline_capture.mm/replay.mm/gate.py`：两块frame-created
Shared placement输入，24-byte inline packet的首尾常量不变，中间VA实际被GPU
解引用。输出41/80，DEADBEEF不变；shader另输出实际读取的VA，确认与capture VA
不同。两次捕获、12次事件seek、三次EID0 reset及2x2 BGRA像素通过。

首次probe错误地检查了未绑定root table的零VA，exit11；已改为检查shader输出
的inline VA，保留失败记录。最终六种tiny变体及56组API+CLI负例通过：
`build-macos-debug/metal-descriptors.WidfAv`，完整日志
`local-m2-descriptor-replay/late-identity/suite-inline-final.log`。
当时库/app SHA256 `7efec33f77fbd53167b739ffc9df9b0fb5f0531f81e875113fb82ac069ed2039`。
旧T01/T09/T35/T49/T52/T62定向API/CLI通过；全量回归未运行，新增inline人工UI
未验。旧BATCH331人工UI证据不转记为本批通过。UE仍没有完整coverage，未放行。

下一项是实际槽位generation、临时payload slice及render inline字段，按D3D12/Vulkan
明确descriptor shadow与更新记录的原则推进。见BATCH333的官方源码与局部编译路线。
