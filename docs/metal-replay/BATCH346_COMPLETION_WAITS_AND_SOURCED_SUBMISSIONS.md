# BATCH346：完成等待、CPU 描述符更新与未保留资源的提交

2026-10-01，持续目标 active，未提交或推送。

发现 Serialise_waitUntilCompleted 是空操作，会让下一次 CPU 描述符更新覆盖尚未
完成的提交输入。对照 D3D12 command queue Signal/Wait 的 DeviceWaitForIdle，
Vulkan vkWaitForFences 的 DeviceWaitIdle / vkQueueWaitIdle 的 QueueWaitIdle，
补充真实完成等待，校验已知且已提交的 command buffer，禁止等待未提交 reservation，
报告 native GPU error。回放不会安装捕获端 CPU callbacks；捕获起止 cutoff 仍沿用
B336 status polling，不引入 callback/self-lock 死锁。

v10 小合约允许同一队列最多两提交，下一提交/CPU slot 更新必须出现在已提交、
已关闭所有 encoder、无 pending producer/inline 后的完成等待之后。预检跟踪
command 出生、commit、完成、当前 encoder 所属 submission；旧 v1-v9 保持原限制。

fixture：首次消费旧 Texture4 后 commit/wait，CPU 改 payload 与来源，第二提交
执行 GPU 描述符更新、后续计算与顶点/片元绘制。旧/新为 122→186，第二捕获
186→122；四轮 seek/GPU 字节/像素通过，trace 确认显式 CPU wait 后 status=Completed。
另覆盖 UE 实际使用的 commandBufferWithDescriptor(retainedReferences=false,
errorOptions=EncoderExecutionStatus) 和 commandBufferWithUnretainedReferences。
沿用 MetalResourceManager 对 native 对象的持有及 FinishReplayCommands 后释放，
没有新增资源保留机制。

两种提交各 22 组顺序反例，descriptor options 另 1 组，以及各 27 组图形反例均
API+CLI 拒绝且无 frame GPU wait。描述符整套十六类 /383 组通过；旧六帧定向
通过；两队列捕获起止/完成回调/自动捕获与 blocked reservation 恢复再验通过。
日志见 local-m2-descriptor-replay/unretained-descriptor-suite.log、
submissions-initial-wait.log、unretained-targeted.log。最终库 SHA256
64714dcbd922a03edd34bab7ac52be05f8556829290909c6be2267e9e245eeb9。

真实 UE a5907a66… 静态结构为 117 次 descriptor command buffer 提交，全部
retainedReferences=false，158 个 render pass，309 indexed /6 indirect /38 direct draw，
34 signal-event、2 Empty purge。未发现 frame waitUntilCompleted，故这次两提交的
host-wait fixture 不代表它的全部异步复用已支持。下一项 heap 资源/非重叠 ring
payload/无 CPU wait 的有序提交，再扩展 indexed/indirect。完整 UE 图像/MRT/pass
尚未验收，全量回归未跑，新增人工 UI 未验。
