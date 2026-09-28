# BATCH75 — Tile texture / sampler bindings

六个 render tile 绑定入口已接通：`setTileTexture(s)`、`setTileSamplerState(s)`及
两种 LOD clamp 重载。每个入口记录独立 chunk；回放验证 encoder 身份、slot/数组长度、
同设备资源类型和有限且有序的 LOD，然后调用原生 Metal。纹理和采样器被标记为帧读取引用。

T75 在 T74 tile pipeline 上增加 1×1 RGBA8 纹理和 sampler。tile shader 实际采样
红通道的 `2/255`，三个 dispatch 对同一 12-byte buffer 依次写入
`tileCount×2/×4/×6`；最终 draw 读取第三段。测试覆盖 single/batch 和带/不带
LOD 的绑定，且自动验证末→首→中→末事件 seek、输出缓冲区和最终像素。
仅在环境变量 `RENDERDOC_METAL_T75_TILE_SAMPLE=1` 下启用，T74 捕获保持不变。

- 定向入口：`bash util/buildscripts/scripts/test_metal_capture_batch75_macos.sh`。
  包含原生 API validation 五次、捕获/XML 身份与六重载、三次 CLI replay、
  Replay API 数据/像素、22 个畸形 chunk 拒绝，以及 T74 回归。
- 完整入口：`RENDERDOC_METAL_LAST_TEST=75 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
  此批次累计预期 75 captures、2019 malformed、750 lifecycle；实际通过结果以日志为准。
- UI 未测试。集中 QA 只需额外打开 T75，核对六种 tile 绑定和三阶段 buffer 数值；
  Tile 专用 Pipeline State 面板尚未建模，不要求显示为 VS/FS。

当前原始标记：80 bridge / 43 旧 chunk；另计 T70 GPU indirect ICB range 未接通则为
81/44。后续优先解决 tile threadgroup memory，再评估 mesh/object 与 ray tracing 的
完整资源链，不把原生透传误计为已接通。
