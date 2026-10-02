# B392：有界直接绘制参数（定向/容量端点通过）

对照本地官方 RenderDoc D3D12 Serialise_DrawIndexedInstanced 和 Vulkan
Serialise_vkCmdDrawIndexed：保留原始 index order、实例和 base 参数，复用通用 action metadata。
v49 sourced preflight 不再限定 0/1/2 三角形，允许 Triangle/TriangleStrip、512 draws、
单次 vertex×instance<=65536、累计<=1Mi、uint32 instance/offset 和 int32 baseVertex。
初始化索引仍需已知字节、offset/stride/range、有效 index+baseVertex 在[0,65535)，
完整资源/typed source/targets/IR draw constants 验证保持。

精确库 43a58922be9fe6c789041e0cb3b3906a31c578675b08683eb3b920fb696e531d。
metal-direct-draw.WcoBiE：256 draws/capture，4 captures/16 seek cycles/130 API+CLI反例。
metal-direct-draw.sqm3SF：同库512 draws端点，同样4/16/130通过。
Private UInt16乱序5/3/4与baseVertex=-3，UInt32四顶点TriangleStrip，双实例/baseInstance7，
全部action数量/参数、GPU descriptor producer/consumer及2×2全像素一致。
新增反例独立匹配IR常量，验证单次/累计work、513draw、32bit参数和负effective index；
所有拒绝均不提交帧内GPU work。第一次helper误把indexOffset当绝对，修正为index binding相对0。

CPU核对当前UE背景initial index的实际最大effective vertex为2207；未以固定0/1/2代替索引数据。
当前UE212 direct draws工作量18438，另48 indirect尚未放开。
精确全量最近v46，v49待后续组合。完整UE GPU replay与人工UI未验；目标active，无提交/推送。
