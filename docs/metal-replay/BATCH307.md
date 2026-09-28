# BATCH307：Binary Archive 单个 visible 函数添加（2026-09-28）

`addFunctionWithDescriptor:library:error:` 接通为 chunk 1380。当前只接受无
specialization/常量/options/预载 archive、名字不超过 128 字节的 visible
函数；intersection descriptor 形态尚无原生回放正例，继续拒绝。捕获记录
来源库和函数名，回放重建 descriptor，并把来源库
登记为 archive 父资源。其余形态明确拒绝，不让原生未包装对象漏入截帧。

T307 空 archive 先加入 `archive_visible`，随后加入普通 compute/render pipeline
函数，使用 archive-miss 选项创建两种 pipeline 并执行 GPU。原生 Metal Validation、
注入捕获、API/CLI 回放通过；43 例畸形输入安全拒绝；13 份跨族定向回归和
T35/T305/T306/T307 各 10 次生命周期打开通过。库/app SHA256
`0a62ea89993e…` 一致。尚未运行完整累计压力门禁或 GUI；06:26 旧
IOGPUResource panic 仍未归因，本批短测后无新增 panic 报告。

集中 UI QA：T307 对照 T305/T306，chunk1380 应先于两条 pipeline 创建；
archive 资源关系含 source library 和三个函数，compute/render pipeline 同指
archive。dispatch 后 32 个 uint 为 17–48，中心像素约 `(0.2,0.7,0.3)`；
前后 seek 正常。此测试不宣称 visible 函数已在 GPU 上通过函数表调用，
只验证 archive 添加、资源链和同一 archive 的普通 pipeline 命中。
