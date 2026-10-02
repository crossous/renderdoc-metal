# BATCH352：command buffer 的 queue 包装对象生命周期

2026-10-01，持续目标 active；未提交或推送。

诊断slot夹具合法使用[[device newCommandQueue] commandBuffer]，在同一进程捕获
两帧。原生GPU已Completed且error=nil，但控制捕获随机返回8/13，误报commit后
仍有未提交队列reservation。定向记录证明map queue=ResourceId::10070880076，
record queue=ResourceId::0，type=18300928；queue包装内存已失效，不是原生Metal
拒绝该调用，也没有GPU完成错误。

命令buffer只存原始queue wrapper指针，而原生保留的是实际queue。旧queue bridge
还以原生associated proxy的唯一引用回应new*的所有权，不能代表app和child各自
的包装引用。按已有Metal new*代理所有权方式，让queue proxy直接接管原生+1；
Capture command buffer在SetCommandQueue中保留parent proxy，替换或析构释放。
Replay继续由resource manager持有queue，不增加capture proxy引用；没有改为忽略
reservation，也没有要求应用额外长期保存queue变量。

修复前连续重测第2轮再次复现；修复后10轮进程、每轮2捕获全部通过。新增失败
诊断只报告capture start等待失败或end缺失backbuffer，并保留原拒绝行为。
当前库0f68436555b02175c896ccdcc783503c5662c61fbf346bcf323c88ea5b5fd802。
完整21类/674反例、真实UE更新shader小图形、双队列callback/cutoff和旧八帧定向
通过；VwvNQb / WGpJoW及queue-owned日志。此前93eb26b1…的308图像/CLI及分段反例到T312、3080 lifecycle通过，
增长12369920 bytes；新queue库完整累计回归待执行，最新UI仍待解锁，UE未完成。

## 完整回归（2026-10-01 12:38）

相同 `0f68436555b02175c896ccdcc783503c5662c61fbf346bcf323c88ea5b5fd802`
库完整运行 `RENDERDOC_METAL_LAST_TEST=312 bash
util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh` 成功：308 个
正例、7786 个损坏输入、3080 次生命周期打开；常驻增长 13,549,568 B。
日志 `build-macos-debug/local-m2-descriptor-replay/queue-owned-full-regression.log`。
描述符定向、真实 UE scatter shader 加小图形消费者的定向、捕获初始等待
也均已通过。人工 UI 仍待解锁，完整 UE GPU replay 未通过；后续 B353
继续处理真实帧中创建顺序与提交顺序不同的 command buffer。
