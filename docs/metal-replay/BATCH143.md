# BATCH143：同一BLAS的两个Top-Level实例

在T142单实例链上新增两个默认布局实例引用同一底层三角形AS的top-level build chunk。
尺寸查询与build接受数量1或2；两实例chunk记录并验证数量、128-byte描述符快照、
Shared实例buffer、单一child AS、scratch/目标容量及实例mask/index/有限矩阵值。
更多实例数量、多个不同child、非默认stride/offset、motion/indirect路径仍未接通。

原生Metal Validation的两个实例分别平移x=-2和x=+2，compute shader使用
`intersector<triangle_data, instancing>`对x=-2/0/+2发射射线，结果稳定为hit/miss/hit。
注入回放的Shared输出在三个dispatch后依次为`(1,9,11)`、`(1,0,11)`、
`(1,0,1)`，末→首→中→末seek保持一致。阶段脚本做3次原生、注入捕获、
T135–T143共9份API/CLI定向回归和23个T143畸形chunk拒绝：

```sh
bash util/buildscripts/scripts/test_metal_as_instances_macos.sh
RENDERDOC_METAL_LAST_TEST=143 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

`supportsRaytracing`仍为false；此安全子集不覆盖完整ray tracing能力。
最终一体化全量142份capture、2926个畸形用例、1420次生命周期打开通过，resident
growth 7,012,352 bytes；日志`/tmp/metal-batch143-full.log`。库与app内嵌库SHA
`c028455d3a89…`，T143 capture SHA `0a41991b814b…`。T70/T133预期拒绝在最新
库上复验通过。GUI/Computer Use未运行，人工差异项并入集中清单；不提交/推送。
