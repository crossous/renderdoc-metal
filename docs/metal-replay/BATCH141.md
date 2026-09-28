# BATCH141：GPU可观察的底层AS原位Refit

接通单个无索引、Float3、零偏移三角形底层AS的`usage=Refit` build、新的
`refitAccelerationStructure:descriptor:destination:scratchBuffer:scratchBufferOffset:`
安全子集，以及compute encoder的单槽`setAccelerationStructure:atBufferIndex:`。
只允许同一AS原位refit、同一顶点buffer和三角形数量、独立command buffer、容量和
refit scratch尺寸充足；indexed/box/instance、非零offset、options重载等仍显式拒绝。
`supportsRaytracing`仍为false，不能据此宣称完整光追支持。

T141的ray-query compute在原生Metal Validation下，build后输出命中1；GPU blit把三角形
移离射线，原位refit后输出未命中0。初版夹具用CPU直接改写Shared顶点，注入回放却得到
`1,1`，因此改为GPU blit并重新捕获。当前API回放的两个dispatch输出为`1,9`→`1,0`
（9为未写槽哨兵），末→首→末seek也复现。3次原生、注入、T135–T141共7份
API/CLI定向回归、28个T141畸形chunk拒绝均通过。完整回归命令：

首次全量在生命周期夹具末尾以`success=0`退出，原因是旧夹具假定T35外每份capture
都有draw；T141只有compute dispatch。夹具已限定识别T141的refit chunk并允许无draw，
随后T35+T141及全140份capture各10轮重复打开均通过。首次日志中的61.8 MB为
首轮提前退出时baseline未建立的值，不能解释为内存泄漏。

```sh
bash util/buildscripts/scripts/test_metal_as_refit_macos.sh
RENDERDOC_METAL_LAST_TEST=141 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

T141 capture SHA `0ca68274fceaf6…`；库与app内嵌库 `9f1860586102…`。
最终一体化全量140份capture、2882个畸形用例、1400次生命周期打开通过，resident
growth 0 bytes；日志`/tmp/metal-batch141-final.log`。GUI/Computer Use未运行，
人工差异项只加入集中清单。不提交/推送。
