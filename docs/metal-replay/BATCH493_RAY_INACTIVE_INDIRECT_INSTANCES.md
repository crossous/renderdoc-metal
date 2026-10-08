# B493：全禁用间接实例槽位

B493开发检查点：native inactive探针已证实count1、ASID0/mask0可构建并ray miss；产物build-macos-debug/metal-ray-b492/inactive-native-probe.log。新增kind11/schema9全禁用槽位配方、Private执行点冻结及完整72-byte packet oracle，五场景与反例脚本已编辑/语法检查；尚未构建或GPU验收。当前仍ae81e24a，公开能力false。实际UE的零候选输入需新诊断，不推定语义。

对照Vulkan CopyInputBuffers与DX12 CopyBuildInputs保持实例语义；DX12地址重定位保持NULL地址。Metal独立保存transform/options/mask/IFT offset/userID与零ASID，只有每个槽位mask0且ASID0才接受无子AS配方。不猜未知非零ID、不开放帧内Private或混合有效/禁用槽位。

2026-10-05 B493定向验收：全禁用Indirect槽位kind11/schema9，仅接受每packet ASID0/mask0，完整transform/flags/IFT offset/userID冻结。最终backend/bundle 08028e4c7e2048c3ac37d0de00d712de139c3026b4960d8c836d4cc12f3b255f，GUI3ba30e36。五例Shared帧前/帧内、Private/placement帧前、同CB distinct alias擦零：native/capture/API588事件/EID0/CLI及完整72-byte snapshot oracle PASS；4×43+36=208坏输入、20旧检查/T12416反例、6×10生命周期PASS（growth425984bytes/hash一致）。产物captures/metal-ray-b493、build-macos-debug/metal-ray-b493/indirect-manifest.json。官方/UE/68份RT与本库集中接续中；ARC/原Qt crash/GUI/UE RT未闭环，公开能力false，未提交/推送。

B493 08028e4c接续验收：官方两scene/10尺寸查询/41坏sample/6能力查询、68份B482–493 RT7785事件、AS身份旧反例/schema2/旧Indirect与空TLAS反例PASS；固定库集中308/7784/3080 PASS，growth0bytes/exit0/工作和冻结库起止hash一致。产物frozen-validation-08028e4c/full-regression.log与metal-ray-b493/followup-manifest.json。UE session20261005-225938进入截帧后Private/Tracked placement、54实例、background0被bridge主动ForceCrash拒绝；仍未证明RT dispatch/输出/离线。B494已编辑，待串行构建/GPU，不能计此证据；生产能力false。
