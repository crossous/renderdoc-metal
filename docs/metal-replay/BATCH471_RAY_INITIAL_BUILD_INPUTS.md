# B471：帧前三角形 AS 的可移植初态

2026-10-05，接续 B470。Vulkan `vk_acceleration_structure.cpp` 和 DX12
`d3d12_initstate.cpp` 保存构建输入并在回放设备上重建 AS；Metal 采用同样模型，
不复制 opaque AS 二进制、不增加光追内部调试。

首批支持单 geometry、非索引 Float3/Float4 三角形、usage None、独立 Shared
顶点缓冲区。描述符参数在编码时固定，顶点字节在原生 commit 前固定；只有
AS 专用 command buffer、此前提交已完成、没有其它 enqueue reservation、
输入没有被本 command buffer 写入时才保存。scratch 和 compacted-size 输出
登记写引用，避免把 GPU 即将改写的顶点误当作输入。输入跨度上限 64 MiB。
保留原生提交以读取完成状态，无新增 completion callback；capture-start
不等待应用完成回调，避免与捕获切换锁互相等待。提交失败或输入无法证明时
不保存可恢复初态。refit/copy/compact 提交会保守失效旧输入记录。

AS 初态经 System InitialContents 序列化并校验 schema、ID/type、参数、
字节范围、payload 长度与 native allocation 容量，再从独立 staging/scratch
重建；每次完整回放重置重新构建。CS/VS/FS/Tile AS 绑定补上初态读取引用。
旧 buffer/texture 初态格式和 driver chunk ID 不变。

## 最终验收

入口 `util/buildscripts/scripts/test_metal_ray_basics_macos.sh`；记录目录
`build-macos-debug/metal-ray-b471`，总日志 `build-macos-debug/metal-ray-b471-gate.log`。
backend/bundle SHA256：
`a2bf58695bd497ad5cce31fb1008ecda053dbf88c2ef4ae11af9b87221943fab`。

| 检查 | 结果 |
| --- | --- |
| 三种帧前 AS native/capture/API/CLI | PASS；原帧和构建后改写均 0/1/0/1，提交前改写为 0/0/0/0 |
| 三方向每 API event 选择 | 每帧 35 个事件 × 3；独立 AS 输入与当前顶点缓冲区内容均正确 |
| B469/B470 基础与 fence | native/capture/API/CLI/55 个异常输入 PASS |
| 初态异常输入 | 25 项：schema、ID/type、offset/stride/format/count/flags、缺参数/重复/缺初态均明确拒绝，无 signal/hang |
| 同 CB GPU 生成输入 | background-fences native capture 正确；离线明确拒绝，保持安全边界 |
| 旧帧回归 | 30 份 API/CLI PASS；旧 visible/renderIFT/computeIFT 57 个异常输入 PASS |
| 生命周期 | 7 captures × 10 = 70 次；resident 增长 376,832 bytes，PASS |
| 构建 | renderdoccmd/app PASS，backend/bundle 一致 |

未跑全量 308 帧、GUI 或真实 UE 光追。本批只是首个可证明的帧前 BLAS 子集；
索引、refittable、TLAS、Private/同 CB GPU 输入、提交交错仍需继续补。
设备两项光追能力仍 false，不能将本批结果当作任意应用光追支持。
