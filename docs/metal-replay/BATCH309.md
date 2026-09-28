# BATCH309：Binary Archive tile 函数添加（2026-09-28）

`addTileRenderPipelineFunctionsWithDescriptor:error:` 接通为 chunk1382。当前只接受
已有 tile pipeline 支持的普通无 linked-function descriptor：一个 tile kernel、
BGRA8/RGBA8 单颜色附件、sampleCount=1、maxThreads≤1024，保留
`threadgroupSizeMatchesTileSize`。捕获记录源函数和有界字段，回放重建 descriptor，
资源图保留函数→archive 关系。

T309 在空 archive 添加 tile 函数，随后通过同一 archive 添加并命中普通 render
pipeline；tile pipeline 自身仍不绑定 archive。原生 Metal Validation、注入捕获、
API/CLI 回放、tile 三阶段 GPU 输出与事件 seek 通过；25 例畸形输入被拒绝。
含既有 tile 变体在内的22份跨族 API/CLI 定向回归通过；T35/T305–T309 各10次
生命周期打开通过。库/app SHA256 `13f127004ba0…` 一致。

边界：tile 函数添加及其回放时序已验证，但 tile pipeline 的 archive 依赖和
archive-miss 命中尚未接通到现有 tile pipeline 序列化，不能把后者称为已支持。
复杂 linked functions、mesh archive 变更仍拒绝。旧 IOGPUResource panic 未归因；
本批短测后无新增报告。完整累计压力门禁与 GUI 未运行。

集中 UI QA：chunk1382 应先于 tile pipeline 创建；archive 父资源包含 tile
kernel，普通 draw pipeline 仍引用 archive 且 options=4。三个 tile dispatch
输出和中心像素、反向 seek 按既有 T74 检查。本项不要求 tile pipeline 的
`binaryArchives` 字段。
