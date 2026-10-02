# BATCH343：顶点/片元来源 inline 与计算更新后的绘制

2026-10-01，持续目标 active；未提交或推送。

v9 将既有明确 ResourceId 来源的 inline VA 重编码接到 setVertexBytes / setFragmentBytes。
复用已有 shader reflection 和 pipeline 状态记录；读取原生 bufferDataSize/alignment，
预检各阶段所需绑定。保留普通 stride 字段，不按原始数字猜测资源。inline byteSize
进入 pipeline snapshot，避免 UI 把 setBytes 显示为零长度。通用状态/反射依据与
D3D12/Vulkan 的绑定快照保持一致；gpuAddress / gpuResourceID 的重编码为 Metal 专有。

新增静态小图形合约：单 CB、最多四次 1×1×1 dispatch、最多两次三顶点单实例 triangle，
2×2 单采样 RGBA/BGRA 单目标，无 vertex fetch / mesh / object / 直接 texture/sampler
绑定。帧中新 buffer/table 沿用 v8 出生校验。图形 root 必须逐次声明阶段/槽位和明确
来源；仍拒绝完整 UE 帧以及未覆盖路径。MTL drawPrimitives 三种直接重载共享相同
序列化参数，应按实际 chunk 枚举解析，而不是仅比较显示名称。

fixture 先消费 Texture4 描述符，计算 shader 写入另一 texture ID，再由顶点消费
Buffer0、片元消费 Texture4/Sampler7；native / 两份捕获 / 每份四轮事件回跳 /
API GPU 字节 / 最终像素均通过。两个捕获 red 分别为 186 / 122；VA/texture ID
与捕获时不同，sampler 可能由系统 intern 为相同 ID，普通 metadata/bias 保留。

27 组 API+CLI 图形反例（阶段、来源、长度、顺序、缺失 pipeline、draw 参数、encoder
出生/结束）全部在 frame GPU 提交前拒绝。描述符整套十三类与 241 组反例通过：
日志 build-macos-debug/metal-descriptors.pOwptV，脚本 test_metal_descriptor_relocation_macos.sh。
最终库 SHA256 490bf140dd3d73420d47d30b192a034eeea90755a34922778ef5df44fbd87944。
全量回归未跑，新增人工 UI 未验，完整 UE 图像/MRT/pass scope 尚未完成。
