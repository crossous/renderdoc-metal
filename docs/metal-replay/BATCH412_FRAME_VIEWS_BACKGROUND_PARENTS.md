# B412：帧内buffer texture view的background父资源

真实UE view17651由background Private tracked heap buffer13790创建；父66560B完整initial，R32Uint TextureBuffer width16384/pitch65536/off0。旧frame preflight只允许future父buffer，错误拒绝合法的“旧buffer/new view”组合。

候选65复用ValidateMetalBufferTexture/原newTextureWithDescriptor，允许已加载、Native Private、完整initial、<=128MiB、无已捕获alias退役的background父buffer；同一判断用于future view的textureID source。仍检查捕获ResourceId/new view birth/格式、storage、pitch、alignment和父范围，不复制父数据或发出替代GPU命令。

444dc635/metal-background-frame-view.980rzR：2captures/8seek cycles/40API+CLI反例组通过。原背景父buffer在frame新建view，实际descriptor采样122；frame覆盖父数据、reset后恢复背景纹理ID/原像素，PickPixel及mips/ordinary字段验证。反例对新frameview本身修改，覆盖旧coverage64、完整initial、格式/offset/pitch、source绑定与copy顺序，GPU前拒绝。

真实UE pre-submit65越过view17651，下一dispatch stream248640；无UE GPU上传/提交。额外诊断正核对该计算PSO的直接buffer snapshot和work范围。最终组合全量待跑，人工UI未验，持续目标active，未提交推送。
