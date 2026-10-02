# B398：sourced render/compute counter附件（定向通过，全量运行中）

UE12230757包含89带counter的render passes、39 computeCommandEncoderWithDescriptor。
不删掉采样附件。已有Native串行/并行render、compute pass的计数器支持可直接复用。
抽取共用counter身份/类型/真实对象/设备归属/采样范围验证，让Native重放与sourced
preflight调用同一检查；Native descriptor和采样索引仍照原始捕获重建。
compute无counter时仍要求NoSample索引，数组固定4；render数组最大4，原有空附件语义保留。
v54还在preflight核对compute dispatchType以及descriptor附件，替代此前只检查encoder出生。

极小MRT夹具可同时采样两个compute consumer和两个render pass；Native原生与捕获路径
核对4对非零、顺序正确的timestamps。覆盖串行/并行、D32S8、frame heap和five MRT；
每个捕获继续验证已有descriptor producer、回跳、depth/stencil和完整color raw pixels。
预期12captures/48cycles。新增counter gate在frame GPU提交前拒绝身份/类型/范围/
数组/dispatchType错误，并复用Native验证器。

运行 `bash util/buildscripts/scripts/test_metal_descriptor_counter_macos.sh`。
当前仅编译对象和夹具，B397精确全量结束前不链接替换库、不并行提交GPU。
尚未报告定向/full/UI通过；真实UE全帧尚未提交。无提交或推送，持续目标active。

精确库8b15aaecd4609c40ca0935b27bceb4d88a82877a35ad688da415b446566c8012：Qpk2a4全部12 captures/48 seeks/150 API+CLI反例通过。v54精确全量counter-pass-combined-regression.log运行中，期间仅编辑下一候选及对象编译，不替换库。真实UE/UI尚未通过。

2026-10-02 全量精确8b15aaec…通过：308 captures/7786 malformed/3080 lifecycle，growth4931584B；结束hash一致。定向150反例全部通过。人工UI未验，UE完整GPU帧未提交，目标active。
