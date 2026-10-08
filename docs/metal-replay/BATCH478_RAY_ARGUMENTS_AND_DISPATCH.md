# B478：官方光追参数绑定与 compute dispatch

2026-10-05，PHASE57 持续进行，两项公开光追能力仍 false。

## 实现

Vulkan vk_descriptor_funcs.cpp 按 buffer 资源与 offset 重建 descriptor，D3D12
CreateShaderResourceView 同样使用资源身份。本批沿用 Metal 已有 argument packet
注册、资源 ID 和 native encoder 重编码，补 device-created 只读 Pointer 描述；不保存
或搜索进程原始地址。无反射的 device descriptor 不含 pointee 大小/对齐，校验非空
剩余 byte range，native encoder 提供 replay GPU address；shader 已反射大小仍沿用
严格范围检查。标量默认 arrayLength0 按 native 语义视为一个 slot，数组重叠/越界仍拒绝。
Managed packet 可选择并在 EID0 恢复后发布 GPU 副本。

AS residency 声明扩展现有 useResource(s)，保留 registry、类型、设备及 usage 检查。
barrier 仍使用原 buffer/texture 子集。

光追 compute 使用反射所需 AS slot、primitive/instance 类型、已构建状态及原 buffer/
texture/sampler 最小绑定检查。通过证明的 ray dispatch 不再被旧纹理 copy oracle 的
输入输出格式、尺寸相等和每像素 buffer 字节数限制误拒绝；direct/indirect threadgroups
保留 grid、threadgroup 和实际 pipeline 上限检查。dispatchThreads 扩展尚未验收。

## 验证

backend 与 app 内库 SHA256 均为
5a5d16834d22e31ed8ebe7e2861793d259a6e4cc388f44d2325e9d10136fb736。

| 检查 | 结果 |
| --- | --- |
| device pointer 数组2、Managed packet、Managed 成员 offset4、实际 compute 输出7 | native/capture/API 往返/CLI 3 loops PASS |
| 原 T60 texture/sampler device packet | CLI 3 loops PASS |
| T60 非法描述 | 27 个拒绝，无崩溃 |
| B477 10 例、B475 12 例 | API/CLI PASS |
| API 事件选择（以上与 triangle 合计） | 2,436 PASS |
| 官方 triangle 新捕获/原生 oracle/API/CLI | 逐字节一致，44 事件三轮前后定位 PASS |
| 官方 procedural 捕获 | 与 native 一致；离线仍 FAIL setAccelerationStructure |
| 全量、GUI、真实 UE RT、生命周期 | NOT RUN |

产物 build-macos-debug/ray-samples/apple-basic/b478-manifest.json、verify-b478.log、
replay-triangles-b478-ray-dispatch.log；pointer fixture 为 captures/metal-ray-b477/arguments_capture.rdc。
新测试用 util/test/metal/metal_ray_argument_capture.mm 与 replay.cpp；正式官方 replay
oracle 和 --stage replay 已接入，会真实报告两场景的闭环结果。

验证中最初测试 shader 使用不合法的 C 数组 id 属性，改为 MSL array 后 native 成功；
该测试编译错误不算 backend 缺陷。descriptor 坏输入中 Pointer 与 scalar0 已成为
合法输入，对应负例改为未支持类型、越界数组，其余身份/访问/重叠检查保留。

## 接续

B479 正在补 VK/DX12 对应 AABB（Metal boxes）冻结输入和初态依赖恢复，推进
procedural sphere/compact TLAS。随后集中回归这一波扩大边界、坏输入、生命周期，
再进入真实 UE。ARC 析构顺序问题尚待，未提交/推送。
