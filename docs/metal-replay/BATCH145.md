# BATCH145：Fragment阶段消费Top-Level AS

在T144的两个不同BLAS/双实例TLAS链上，接通旧chunk
`MTLRenderCommandEncoder::setFragmentAccelerationStructure`。fragment shader对x=+3
发射真实GPU ray；只有引用第二个BLAS的实例才能命中。原生Metal Validation稳定返回
Shared值`1`并绘制绿色像素。捕获/回放的API验证包括TLAS资源身份、slot0、
compute→fragment事件顺序、fragment输出末→前→末seek为`1→7→1`及中央绿色像素。

bridge/回放拒绝未包装、未构建或错误类型的AS、越界slot和错误render encoder；
命令记录保留AS资源依赖。T145另有29个畸形输入拒绝用例。该子集不代表vertex/tile
AS绑定、intersection function table或完整ray tracing支持；
`supportsRaytracing`仍为false。

阶段命令：

```sh
bash util/buildscripts/scripts/test_metal_as_fragment_macos.sh
RENDERDOC_METAL_LAST_TEST=145 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

原生、注入捕获、T142–T145定向API/CLI和29个畸形输入通过；完整回归
144份capture、2979个畸形用例、1440次生命周期打开通过，resident growth
7,913,472 bytes，日志`/tmp/metal-batch145-full.log`。库/app内嵌库SHA
`a624ff54d9b8…`，T145 capture SHA `78a3802c5057…`。
不运行GUI/Computer Use，不提交或推送。
