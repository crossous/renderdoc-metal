# B376：完整 initial 的背景 2D 颜色来源

v35将背景2D颜色来源的尺寸/格式校验对齐已有initial-state的通用format/block/mip规则。
8192边长/14mip/128MiB单资源，sample1/array1/depth1；CPU预检逐mip算紧密逻辑字节，
核对已捕获initial长度，计入256MiB sourced预算。GPU ID始终按ResourceId的Replay Native对象重新编码。
view通过父资源完整initial恢复，不额外复制baseline。深度/多重采样/未来资源预算未放开。

对照D3D12 Serialise_InitialState/Apply_InitState及Vulkan vkCreateImageView的AddParent/baseResource，
沿用“父资源恢复、视图引用”的公共模型。已初始化R16Float/RG11B10/RGBA8/BGRA8的2D clear pass
使用实际Native attachment尺寸，显式renderTargetWidth/Height不能超出attachment；draw/dispatch数量等旧约束保留。

metal-private-texture.CpBexk：R16Float192×104 Private parent及同格式view，两捕获/八回跳，
GPU122/DEADBEEF；每次回跳核对完整19968像素半浮点（initial0x3400=.25，clear0x3a00=.75），
角/中心PickPixel一致；46组API+CLI错误数据在Private upload/wait之前拒绝。
精确e3c11635242459a4c909c78af97b6527a82b0e99abfd1a4fc649ba8ecb185b01。
直接来源metal-private-texture.JkReIY同库两捕获/八回跳/36组反例也通过，总计四捕获/16回跳/82组反例。精确e3c11635…组合全量已通过：308捕获、7786反例、3080 lifecycle opens，resident growth 0B，日志large-buffer-2d-source-combined-regression.log；实际UE未完整GPU replay，UI仍锁屏，
持续目标active，无提交/推送。
