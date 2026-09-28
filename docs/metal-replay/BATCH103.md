# BATCH103 — Counter subrange and nonzero resolve offset

T101已证明四个stage-boundary timestamp和零偏移resolve。T103在8槽sample
buffer内选索引2/3/4/5作为vertex/fragment起止，blit从range(2,4)
解析到目标Shared buffer偏移16。原生验证四个值非零递增、与CPU直接解析
逐字节一致，前后未写区域保持零；capture/XML、CLI、Replay API在
resolve→draw→resolve seek后同样验证偏移读回和像素。

```sh
bash util/buildscripts/scripts/test_metal_capture_batch103_macos.sh
RENDERDOC_METAL_LAST_TEST=103 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

定向三轮原生×五帧、CLI3、API、22个畸形输入拒绝及T01/T95/T101/T102
哨兵捕获已通过。全量日志`/tmp/metal-batch103-full.log`：**103 captures、
2469畸形、1030 lifecycle**全通过，resident growth 622,592 bytes。
T103 capture SHA-256 `a38e39c760c4…`，库/app内嵌库`30a8ca932d9f…`。
本批不删除
bridge/旧chunk标记；原始剩余仍 **49/40**，T70另计。未做GUI/Computer Use、
提交或推送。

集中GUI QA：`t103_capture.rdc`的render pass sample indices为2/3/4/5；
`resolveCounters`源range.location=2、length=4、目标offset=16。
目标buffer前16字节为零，随后四个64-bit timestamp非零递增，后缀为零。
具体时间戳每次回放允许变化；draw中心RGB≈51/179/77。
