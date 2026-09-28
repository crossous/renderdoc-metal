# BATCH134：加速结构静态 primitive 尺寸查询

T134 只接通两个**纯查询**入口：`MTLDevice::accelerationStructureSizesWithDescriptor:`
和 `heapAccelerationStructureSizeAndAlignWithDescriptor:`。当前支持非 motion 的
primitive descriptor，几何为三角形（含索引）或 bounding box；查询前复制描述符并
把 vertex/index/transform/primitive-data/box buffer 的 RenderDoc 包装对象替换成原生
MTLBuffer。其它 descriptor 类型仍走 `METAL_NOT_HOOKED`，不宣称完整 ray tracing。

本机 Apple M2 Pro、Metal API Validation：原生与注入运行的无索引三角形、索引
三角形、bounding box 三种查询数值完全一致，均为 AS 1536 / build scratch 6400 /
heap 1536 / align 256。T134 捕获的 API/CLI 回放及受影响旧 T71/T116/T117/T131/T132
定向回归通过。重跑命令：

```sh
bash util/buildscripts/scripts/test_metal_as_queries_macos.sh
```

查询不产生 replay chunk，不改变捕获格式；T134 的捕获仍只验证既有
`Metal_Private_Buffer` 帧。**没有执行加速结构资源创建、build encoder 或 shader
intersection；`supportsRaytracing` 仍应保持 false。** 两个方法内的非支持描述符
fallback 仍含 bridge 标记，原始标记计数保持 35/29；这次按真实支持子集记录，
不通过删除标记制造进度。T134 没有独立 UI 功能验收，后续若查看该 capture，
仅按普通帧确认显示与像素。
