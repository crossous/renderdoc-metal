# BATCH144：两个不同BLAS的Top-Level实例

T144在T143的双实例基础上，为两个实例各建一个不同的底层三角形AS，分别以
`accelerationStructureIndex=0/1`指向双元素AS数组。第一个局部三角形中心x=0、
实例平移x=-2；第二个局部中心x=1、实例平移x=+2。原生Metal Validation下
从x=-2/+2/+3发出的三条GPU射线稳定返回`1,0,1`。若第二个实例错误引用第一个
BLAS，x=+2/+3的结果会改变，因而这是子结构索引的GPU可观察验证。

bridge仅接通默认布局、两实例、两套已在前序command buffer构建的不同BLAS，
并新增独立`buildDistinctInstances` chunk；捕获/回放检查两套child身份、默认
mask/index、有限变换矩阵、Shared实例buffer、128-byte快照、目标及scratch容量。
命令记录保留两个child资源依赖。其它数量、共享/重复索引组合、非默认offset/stride、
motion及间接实例仍明确未支持，`supportsRaytracing`保持false。

阶段命令：

```sh
bash util/buildscripts/scripts/test_metal_as_distinct_instances_macos.sh
RENDERDOC_METAL_LAST_TEST=144 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

T144原生、注入捕获、T142–T144定向API/CLI、24个畸形chunk拒绝已通过。
整批回归143份capture、2950个畸形用例、1430次生命周期打开通过，resident growth
6,225,920 bytes，日志`/tmp/metal-batch144-full.log`。库与app内嵌库SHA
`42099b733d9f…`，T144 capture SHA `3afa4d1f0c9a…`。
事件seek的Shared输出末→首→中→末依次为`(1,0,1)`、`(1,9,11)`、
`(1,0,11)`、`(1,0,1)`；没有运行GUI/Computer Use。
