# PHASE40：T39 private GPU buffer readback

状态：终端自动完成，GUI L4 待验，阶段开放。

516-byte Private buffer 每帧先fill为0xa5，然后纯buffer compute kernel写入
`byte(i*7+3)`；拷贝到shared buffer供native对照，fragment读取三个private bytes输出。
Capture/XML验证存储模式未被偷偷转成Shared。

Replay API经单独staging读回，不依赖fixture中供native比较的Shared副本。覆盖全量、
非对齐、最后一个字节、0长度表示剩余、超长截断、非法ID/类型/offset与往返seek。
同时扩展dispatchThreads的buffer-output合法路径；reflection约束active绑定大小和对齐。

必跑：T39完整native/capture/API/CLI、T38 texture→buffer、T28/T29/32/33旧compute及异常。
共享 GetBufferData 改动使所有buffer消费者相关，最终跑40份capture API/CLI/lifecycle。
Private staging释放和进程内重复读回由400次lifecycle覆盖。Managed分支无本机专项证据，
不标为已验证；并行队列/异步读回性能不在范围。人工项见 `QA_CONSOLIDATED.md`。
