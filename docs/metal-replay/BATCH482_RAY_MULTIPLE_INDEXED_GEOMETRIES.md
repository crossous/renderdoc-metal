# B482：共享输入的多个索引几何段

最终 backend/bundle SHA256：
cad38602870e6110295d3c3e8a38c8e537669aaa33d4ed35e982e3a2f4c1b052

## 对照与实现

仓库 Vulkan vk_acceleration_structure.cpp 按 geometryCount 收集每段描述、buffer
范围并保存重建输入；DX12 d3d12_manager.cpp 的 CopyASBuildData 处理NumDescs和
逐geometry复制。两者均支持multi geometry，本批按UE实际两段共享VB/IB路径补齐。

新增buildMultiIndexed chunk，保留2–64段有序的顶点offset/stride/Float3或Float4、
三角形数、IFT offset/opaque/duplicate、usage、UInt16/UInt32索引offset/type。
核验身份/设备/各段范围/总三角形上限及实际driver AS/scratch尺寸后提交。
帧前kind8/schema5使用现有AS-only、同队列producer顺序和禁止输入写入规则，
在提交末尾复制完整VB/IB（每个≤64MiB），完成后逐段解码索引验证顶点范围。
重放使用冻结输入重建；schema1–4不变。TLAS按BLAS→TLAS依赖顺序恢复。

实际测试揭示Shared TLAS在子BLAS已完成但GPU快照未materialise时遗漏初态；
加入已完成状态检查和物化，不等待GPU或应用回调。compact/copy沿用独立recipe
和full-AS临时重建后compact机制。生产能力不变；RT仍opaque。

当前只支持共享VB/IB的indexed segments；不声称独立buffer多geometry、motion、
primitiveData/transform输入、多geometry refit或heap输入portable初态已通过。

## 测试证据

十个场景：multi-indexed、u32、private，以及background共享/u32/private/
private-gpu-mutated/tlas/compact/compact-copy。命中几何放在第二段，遗漏第二段
会得到错误输出；所有native/capture为0/1/0/1，API含EID0与三方向往返1176选择，
CLI各3loops PASS。构建后GPU覆盖输入仍恢复原AS结果。

十份捕获各30 malformed（300）clean拒绝；B481 heap placement14旧坏输入通过；
18份旧定向API/CLI PASS。11捕获×10生命周期PASS，growth688128bytes。
官方两scene重新native/capture/offline像素逐字节一致、44/46事件×3方向与CLI×3，
41坏输入及6能力查询PASS。全部归于cad38602；无本库全量/GUI验收。

结果build-macos-debug/metal-ray-b482/geometry-manifest.json、sample-manifest.json，
suite.log及逐项日志；入口util/buildscripts/scripts/test_metal_ray_geometry_macos.sh。
官方旧9122证据已归档gate-results-b481-9122ac7a。

真实UE session20261005-165322，manifest在ue-ray-diagnostic/b482-cad38602。
已越过两段几何构建；新失败metal_device_bridge.mm:1325的
accelerationStructureSizesWithDescriptor，源码证明UE RHICalcRayTracingSceneSize
只设置instanceCount就查询TLAS尺寸。B483修复query-only路径，不放宽实际build。
尚无可验收RT新capture，RT dispatch/离线未验，ARC待修，公开两项能力false。

## 历史失败

第一次fixture编译copy返回id，改显式descriptor指针后编译通过；日志保留。
过大初态数组被底层反序列化器clean拒绝，原测试只接受Metal层错误而误报；
补识别既有array/byte-buffer guard，原日志保留。TLAS缺初态是真实后端失败，
pre-tlas-fix/missing-tlas-initial日志保留；cad38602通过其修复后的全部门槛。
未提交/推送。
