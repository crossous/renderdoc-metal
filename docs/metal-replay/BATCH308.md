# BATCH308：Binary Archive 单节点 stitched library 添加（2026-09-28）

`addLibraryWithDescriptor:error:` 接通为 chunk 1381。与 T303/T304 共用单函数、
单输入、单输出节点的 descriptor 约束和原生重建器；捕获记录源 visible 函数、
graph/function 名称，回放按时序向 archive 添加，资源图保留函数→archive 关系。
复杂 graph 仍拒绝，不原生透传包装对象。

T308 空 archive 先加入单节点 stitched library，再加入普通 compute/render
pipeline 函数，两个 pipeline 使用 archive-miss 选项。原生 Metal Validation、
注入捕获、API/CLI 回放通过；44 例畸形输入安全拒绝；T301/T302/T305–T307
负例复测通过。14 份跨族定向 API/CLI 回归与 T35/T305–T308 各 10 次生命周期
打开通过。库/app SHA256 `7c43ac13093f…` 一致。旧 IOGPUResource panic
仍未归因，本批短测后无新增报告；没有运行完整累计压力门禁或 GUI。

集中 UI QA：T308 中 chunk1381 应先于 pipeline 创建；archive 父资源包含
`archive_visible` 源函数，两个 pipeline 同指 archive；32 个 uint 仍为 17–48，
中心像素约 `(0.2,0.7,0.3)`，前后 seek 正常。本例证明添加与回放时序，
不宣称 stitched 函数在 GPU 函数表里执行；后者另见 T303/T304。
