# BATCH142：GPU可观察的单实例Top-Level AS

接通一个默认布局、零偏移、单实例的top-level AS：尺寸查询把包装后的实例buffer与
底层AS转换为原生资源，build记录目标AS、child AS、实例buffer、scratch及64-byte
实例描述符快照，回放验证资源类型、容量、子AS已在前序command buffer构建、mask/index
和原始描述符字节一致。其它实例数量、间接/motion布局、非默认stride/offset及实例
descriptor形式分配仍显式拒绝。`supportsRaytracing`仍为false，不能据此推断通用光追完整。

T142原生Metal Validation下，1536-byte底层三角形AS经平移`x=+2`的实例构建为
1792-byte top-level AS。`intersector<triangle_data, instancing>`通过compute绑定TLAS，
原点x=0的射线miss、x=2的射线hit。API回放在两个dispatch后读回`(0,9)`→`(0,1)`，
末→首→末seek重现；9是未写第二槽的哨兵。阶段脚本完成3次原生、注入、T135–T142
共8份API/CLI定向回归和21个畸形chunk安全拒绝：

```sh
bash util/buildscripts/scripts/test_metal_as_instance_macos.sh
RENDERDOC_METAL_LAST_TEST=142 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

T142的一体化全量通过141份capture、2903个畸形用例、1410次生命周期打开，
resident growth 5,455,872 bytes；日志`/tmp/metal-batch142-final.log`。当时库/app内嵌库
SHA `608f4e1c2225…`，T142 capture SHA `54d563c23e9b…`。GUI/Computer Use未运行，
人工差异项并入集中清单；不提交/推送。
