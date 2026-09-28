# BATCH310：Binary Archive mesh 函数添加（2026-09-28）

`addMeshRenderPipelineFunctionsWithDescriptor:error:` 接通为 chunk1383。当前
仅支持无 object stage、无 linked functions 的普通 mesh/fragment 函数、单
BGRA8/RGBA8 颜色附件、sampleCount=1、maxMeshThreads 1–1024、
maxMeshGrid≤1048575。捕获记录两个函数和有界字段；回放重建 descriptor，
资源图保留 mesh/fragment→archive 关系。系统不提供该调用的 metal-cpp
包装，因此原生调用前检查 selector 可用性。

T310 在空 archive 添加 mesh 函数；同一 archive 另缓存并命中执行中的
`write_grid` compute pipeline，由 GPU 写出 mesh 间接 draw 参数。原生
Metal Validation、注入截帧、API/CLI 回放和 mesh 间接 draw GPU 输出通过；
34 例畸形输入安全拒绝。含旧 tile/mesh 变体在内的 27 份跨族 API/CLI
定向回归通过；T35/T305–T310 各 10 次生命周期打开通过。库/app SHA256
`fecbaa45dd8a…` 一致。

边界：mesh 函数添加与回放时序已验证，但 mesh pipeline 自身的 archive
依赖及 archive-miss 命中尚未加入现有 mesh pipeline 序列化。object stage、
linked functions 和复杂 descriptor 仍拒绝。旧 IOGPUResource panic 未归因；
本批短测后没有新 panic 报告。完整累计压力门禁与 GUI 未运行。

集中 UI QA：chunk1383 在 mesh pipeline 创建前，archive 父资源含 mesh 和
fragment 函数；执行中的 compute pipeline 引用同一 archive 且 options=4。
GPU 间接 mesh draw、画面像素和事件 seek 按 T89 检查。本项不要求 mesh
pipeline `binaryArchives` 字段。
