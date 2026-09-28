# BATCH83 — 最小 Object/Mesh pipeline 与对象buffer

本机Apple M2 Pro原生Metal Validation验证了object shader用buffer值生成16-byte
payload，并设置1×1×1 mesh grid，mesh shader读取payload形成三角形。随后接通
同步object+mesh pipeline（新chunk1301）和`setObjectBuffer`（新chunk1302），
回放重建三个函数身份、payload长度、object/mesh线程上限、grid上限、颜色格式，
严格拒绝不支持的descriptor。buffer资源按读前写引用标记。MeshDispatch仍不伪装
为普通顶点draw；MetalPipe尚无object/mesh专用面板，只展示pipeline资源与fragment。

- 定向：`bash util/buildscripts/scripts/test_metal_capture_batch83_macos.sh`；五次原生
  Metal Validation、捕获/XML、CLI3、Replay API对象payload驱动中心RGB约
  `0.7/0.2/0.4`、16个畸形输入拒绝、T78–T82回归通过。
- 全量：`RENDERDOC_METAL_LAST_TEST=83 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`；
  83 captures、2128 malformed、830 lifecycle已通过，resident growth 2179072 bytes，
  日志`/tmp/metal-batch83-full.log`。
- 捕获SHA`e7e52fa73e7d…`、库/app SHA`77fd1d77b4cf…`。
- 原始剩余**63 bridge / 42旧chunk**，T70实际缺口另计64/43。
  GUI/Computer Use未运行，50份capture待集中人工。
