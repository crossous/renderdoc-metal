# PHASE56：Metal AS 更新、复制与压缩初态

2026-10-05，接续 [PHASE55](PHASE55.md)。继续遵守用户准则：先检查本仓库
Vulkan/DX12 的实际支持及 RenderDoc 实现，复用输入快照、资源依赖、提交与重建；
两个后端不提供的光追内部调试不在 Metal 扩展，Metal 独有语义仅在执行所需时适配。

## 已完成的两个循环

1. [B474](BATCH474_RAY_REFIT_AND_COPY_INITIALS.md)：支持 refittable 三角形初态，
   完成后的原地/异地 refit 初态和普通 copy 初态；补 nil 目标的原地写法。
   每轮清除旧 AS 逻辑构建状态，修复帧内目标被上一回放误判为已构建的问题。
2. [B475](BATCH475_RAY_COMPACT_INITIALS.md)：恢复 compact 初态、compact 后再 copy、
   TLAS 引用 compact BLAS、帧内 compact 依赖的初态尺寸查询。commit 校验源版本；
   恢复使用全尺寸临时结构和本机完成的尺寸查询，容量不足时拒绝 compact copy。

AS 初态 schema4 保存冻结输入及 compact/尺寸查询证明；schema1/2/3仍可读。
不改变 buffer/texture 初态或 driver chunk编号；仍是 opaque GPU 执行，不增加新 viewer。

## 当前基本可用范围

单 geometry、无 motion/primitive data/transformation buffer，独立 Shared 输入，
Float3/Float4、非索引或UInt16/UInt32、既有 stride/offset/table/opaque/duplicate 组合。
64 MiB 完整输入跨度与既有 bridge/refit 描述符限制仍适用。usage None 或 Refit；
refit要求完成的兼容源构建记录，参数保持一致。普通/compact copy要求可恢复的完成
源版本；源在编码与提交之间经另一提交改变时保守拒绝。

default-descriptor TLAS 保持1–65,536实例、1–4唯一primitive子结构、重复子引用及
既有变换；子结构现在可来自上述 build/refit/copy/compact。仍需子版本与TLAS构建时
一致，先重建全部primitive AS，再重建TLAS。完成且符合既有producer规则的Shared
尺寸查询可用于帧内compact，UInt/ULong及offset/type在加载前校验。

构建/refit提交仅含AS工作和CPU数据记录；无同CB输入写入，包括NoCopy Shared
地址重叠；此前GPU提交完成、无其它enqueue reservation。原生完成状态用于证明，
捕获切换锁内不等待应用完成回调。提交后改写源字节不影响冻结AS几何。

上述Shared场景的 build/refit/copy/compact 初态与逐事件回放基本链路已可用，
仍不能宣称任意工程光追可用。`supportsRaytracing` / `supportsRaytracingFromRender`
保持false，避免应用进入尚未验证的路径。

## 当前候选证据

backend/bundle为 `b5451e7946e66b8e3536985ac3abe129abfcfe6c78cc2fd43ac3b0d9056efa6a`。
最终定向检查：47新+171旧正例、5,199新例事件选择、281损坏捕获、4语义边界、
490生命周期；schema1/2/3兼容及构建/静态检查通过。详见B475的日志、hash和限制。
未跑全量308帧、GUI或真实UE光追；保留工作区修改，未提交/推送。

## 下一阶段优先级

- Private及同CB GPU生成的几何/索引/实例：参考Vulkan/DX12在GPU执行点复制输入，
  建立Metal encoder边界、producer和别名证明，避免读取后来状态。
- boxes初态与更新、多个geometry，以及有VK/DX12对应支持的motion等执行能力；
  保留原有帧内接口，逐项补初态和回放闭环。
- CPU编码与GPU提交间的复杂函数表变更、跨queue/heap/间接绑定依赖。
- 真实启用光追的工程与更广回归，通过后再评估两项设备能力查询。

串行入口：`util/buildscripts/scripts/test_metal_ray_compact_macos.sh`；结果目录可用
`RENDERDOC_METAL_RESULT_DIR`、捕获目录用`RENDERDOC_METAL_CAPTURE_DIR`覆盖。该入口为
定向检查，不会自动宣称全量或GUI验收通过。
