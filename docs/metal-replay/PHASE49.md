# PHASE49：Command Buffer 回调与 Shared CPU 更新回退

T49 / `Metal_Command_Handlers`。自动验证通过，GUI L4待验，阶段保持开放。
两种bridge和旧chunk1049/1054接通，无新chunk，Max仍1264；剩余160 bridge / 96 chunk。

## 接通范围

`addScheduledHandler:` / `addCompletedHandler:` 由native Metal保持原有block复制、线程和
调用时机，但向应用传回原包装command buffer，而非泄漏真实对象。桥接block保留proxy，
callback结束后随native block释放；不另建线程、不主动执行应用block。nil继续交原生处理。
捕获只记录注册类型及CommandBuffer身份，不保存代码、block地址或捕获变量。
Replay校验资源类型/存在性，呈现注册API事件；不会重新执行或等待应用回调。

## 此fixture发现并修复的旧问题

1. 旧CPU快照在native commit之后读取，快速GPU/完成回调可能已改写共享内存，导致上一
   提交混入回调的新输入。快照移到native commit之前，提交、Present和记录生命周期仍在原位置。
2. 旧CPU更新在编码draw之后、commit之前写入capture。部分event replay停在draw时可能
   还没读到更新；旧初始内容也没有恢复，后退事件会残留后一次的CPU数据。
   Loading按command buffer索引shared CPU更新；active replay在该buffer开始时预先应用，
   并完成前一提交以避免CPU/GPU写入竞争。每次完整/WithoutDraw重放恢复这些buffer的初值，
   OnlyDraw延续前半段状态。首次加载先扫描更新涉及的buffer并恢复非零初值，随后执行
   loading pass；加载后只保留实际有CPU更新的buffer初值缓存。
3. `Internal_MTLBufferModifyCPUContents` 不再把不可信payload直接读进GPU内存。先读bytebuf，
   校验resource/shared模式、owner command buffer、offset/size、实际payload长度，然后复制。
   bytebuf与旧raw byte指针序列化格式一致；旧capture没有改写。初始buffer内容也检查身份、
   长度和重复记录。

这不是通用InitialContents、全部资源所有权或多队列调度重构：当前修复针对shared submission
CPU更新。GPU写入可能被现有diff逻辑冗余记录为后续共享内存差异，未新增写入来源追踪；
纹理/argument buffer重定位、外部同步、跨队列/跨进程回调副作用仍需独立切片。
离线只重放已捕获的GPU命令/资源更新，文件IO、网络、应用逻辑和回调时序不重现。
本批没有增加CS constant专用usage记录或新的回调时间线UI。

## Fixture / 断言

两个command buffer，每个注册两scheduled、两completed，总计八个回调。检查回调
command buffer/device/queue/label身份、状态、NSError、恰好一次，以及闭包在每帧pool
释放后销毁。生产者blit写300-byte buffer全29，首次compute使用参数7/11/13，产生三段
各32项递增数据。
完成回调读取生产者，把12-byte参数改为41/67/101。消费者清零412-byte output再计算，
三段各32个uint32，从41/67/101递增，末尾28bytes为0；draw输出RGBA41/67/101/255。

终端断言覆盖回调前后两dispatch、clear/draw来回seek、12-byte参数恢复、所有output值及
padding、CS结构反射/绑定、输出usage和像素。XML/zip另查八条注册的两种身份，以及参数
CPU差异payload的9个字节。不把注册事件解释为回调执行事件。

测试入口、87类异常、完整联合结果和最终版本见BATCH49。GUI三项最小增量并入
QA_CONSOLIDATED；旧待验项全部保留，用户确认前不关闭阶段。
