# BATCH78 — 最小 Mesh pipeline 与直接 draw

同步 `newRenderPipelineStateWithMeshDescriptor` 和直接 `drawMeshThreadgroups` 已接通
真实 capture/replay。当前范围明确限于无 object function、一个 mesh function 加一个
fragment function、单 BGRA8/RGBA8 附件、1×采样和默认其余 descriptor 属性；超出范围
在回放时明确拒绝。mesh draw 验证 pipeline 身份、1×1×1 object threadgroup、GPU
支持的 mesh threadgroup/网格上限，生成 `MeshDispatch` 事件而不伪装为普通顶点 draw。

T78 原生 shader 通过 `metal::mesh` 产生三角形；Replay API 在 mesh 事件确认中心
RGB约`0.2/0.7/0.3`、背景黑色、fragment shader/pipeline 身份与1×1×1网格、
32×1×1 mesh 线程。现有Metal Pipeline State没有mesh/object专用字段，暂只保留
mesh pipeline 资源身份和fragment shader，不虚构vertex shader。

- 定向入口：`bash util/buildscripts/scripts/test_metal_capture_batch78_macos.sh`。
  五次原生 Metal Validation、捕获/XML、三次CLI replay、Replay API、17个畸形
  pipeline/draw chunk 拒绝、T74–T77回归。
- 完整入口：`RENDERDOC_METAL_LAST_TEST=78 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
  78 captures、2052 malformed、780 lifecycle已通过，resident growth 0 bytes；
  日志`/tmp/metal-batch78-full-final.log`。
- T78 capture `160f902aef23…`，库/app `dfe3ec542c36…`。GUI未运行；集中QA
  新增一次mesh dispatch的事件、pipeline资源和中心/背景像素，不要求显示虚构VS。

原始剩余标记76 bridge / 42旧chunk；T70 GPU indirect ICB range另计77/43。
下一步在同一资源链补 object/mesh buffer、texture、sampler绑定，再测试
`drawMeshThreads`及异步pipeline；不直接把不受硬件支持的间接mesh draw算入完成。
