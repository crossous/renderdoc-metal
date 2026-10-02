# B418：完整初始内容的背景颜色目标

实际UE encoder17698/texture15979为320x240 R32Uint（Metal enum53），loadClear/storeUnknown；完整initialContents已捕获。现有color whitelist只包含R16/RG11B10/RGBA8/BGRA8，误拒绝这个Native颜色attachment。CPU扫描真实UE所有颜色目标还包含R8Unorm/RGB10A2Unorm/RGBA16Unorm/RGBA16Float/BGRA8_sRGB。

候选65对这六种BG颜色格式要求Private+Tracked+RenderTarget usage、2D、完整initialContents；保留8192尺寸边界、单sample、level/slice/depthPlane0、无resolve、唯一target及Store/合法deferredUnknown。PSO draw仍需精确Native attachment format匹配。复用已有Native纹理格式、initial restore和render pass；没有修改PSO/clear/draw语义，frame texture whitelist未扩大。

4409dea6/metal-background-color.2QU0k7：12captures/48reset-seeks/60 API+CLI初始化、format、attachment反例组通过。每格式8x4 Private Tracked背景纹理、GPU初始化全0，frame原Native clear/storeUnknown再明确setColorStoreActionStore。Native捕获结束后blit读回first pixel，replay全部32pixels与原Native精确字节相同、EID0恢复全部initial pixels；R32Uint为17/31，float/unorm/sRGB Native转换无CPU替代。16/32bit formats及deferred store都测试。helpers第一稿调用ResultDetails::Message导致未导出DoStringise链接失败，改为既有internal_msg读取后编译通过。

真实UE推进至depth/stencil pass17709，需要精确核对depth shape与unused resolve filter。未提交UE GPU回放；人工UI目前Mac锁定。
