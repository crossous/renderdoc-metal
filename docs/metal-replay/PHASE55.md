# PHASE55：Metal 光追加速结构初态恢复

2026-10-05，接续 [PHASE54](PHASE54.md)。用户准则仍是先查本仓库 Vulkan/DX12
支持与实现；复用 RenderDoc 的捕获、资源依赖、提交与重建模型。光追保持黑盒，
只保证执行及下游资源结果，不增加两个后端均不支持的光追内部调试。

## 已完成的三个循环

1. [B471](BATCH471_RAY_INITIAL_BUILD_INPUTS.md)：在编码时固定描述符，在提交时
   冻结顶点字节，恢复帧前非索引三角形 BLAS。验证提交前/构建后改写的区别。
2. [B472](BATCH472_RAY_INITIAL_INSTANCES.md)：冻结实例描述符及 BLAS 构建版本，
   先重建底层结构、再重建 TLAS；覆盖重复子引用、实例改写及帧内 TLAS 消费
   帧前 BLAS。拒绝底层结构在 TLAS 构建后改变的未证明情形。
3. [B473](BATCH473_RAY_INDEXED_INITIALS.md)：保存独立的 UInt16/UInt32 索引输入，
   按冻结的索引计算顶点范围并重建；支持 TLAS 引用索引 BLAS。补齐 Float4、
   stride/offset/padding 边界，尺寸查询也使用冻结输入。兼容 schema1/2。

初态通过既有 System InitialContents 保存，不追加 driver chunk；每次完整
回放重置从输入重建，不导出或伪造 Metal AS 二进制。CS/VS/FS/Tile 绑定登记
初态读引用，依赖资源保留创建记录；使用独立临时上传和 scratch 对象，完成后释放。
串行 GPU 验收入口：`util/buildscripts/scripts/test_metal_ray_basics_macos.sh`。

## 当前可用范围

单 geometry、无 motion/primitive data/transformation buffer、usage None 的
Float3/Float4 三角形 BLAS；非索引或 UInt16/UInt32 索引，允许既有 table offset、
opaque 和 duplicate 标志。输入为独立 Shared 缓冲区、64 MiB 范围内。
default-descriptor TLAS 支持 1–65,536 实例、1–4 唯一 BLAS，以及已有变换和
重复子引用；沿用 options=0、mask=0xff、instance table offset=0。

构建 command buffer 需仅包含 AS 工作与 CPU 数据记录；输入没有在其中被
scratch/compacted-size 或其它 GPU 工作写入（含不同buffer ID的重叠Shared范围），
之前提交已完成，没有其它未提交
enqueue reservation。完成证明读取保留的原生提交状态，不引入 completion-handler
等待。已完成的前序 GPU 写入 Shared 输入可以读取；同一 CB 或未完成 producer
不能静默当作 CPU 输入。构建后应用改写源缓冲区不改变冻结的 AS 输入。

这些受控基础路径现已可以捕获并离线反复选择事件；不等于任意应用光追可用。
`supportsRaytracing` / `supportsRaytracingFromRender` 仍 false。

## 接续优先级

- refittable BLAS、refit/copy/compact 后的初态和旧构建版本恢复；现阶段提交
  这些变更会保守失效旧输入记录，帧内原有接口仍保留。
- 同 CB GPU 几何/实例 producer、Private 输入、执行点冻结/重定位、跨提交依赖，
  按 Vulkan/DX12 构建输入快照方案继续扩展。
- 多 geometry、boxes/curves/motion 等有对应后端支持的执行能力，逐项补齐；
  Metal 独有描述符只在必要时处理。
- 函数表在 CPU 编码与 GPU 提交之间交错变更的快照；真实启用光追的工程验收。

本阶段未运行完整 308 帧套件、GUI 或 UE 真光追验收。每批的后端 hash、定向
结果、负例和生命周期分别记录，历史 PASS 不自动归到后续候选。
