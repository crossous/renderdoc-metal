# PHASE46：T45/T46 定时 Present 与 Buffer Debug Marker

状态：实现和最终联合终端验证通过；两个T分别等待GUI L4，阶段开放。

接通四个旧bridge/chunk缺口：`presentDrawable:atTime:`、`afterMinimumDuration:`、
buffer `addDebugMarker:range:` / `removeAllDebugMarkers`。两个present入口复用普通present
的drawable解绑、command record、backbuffer和output layer登记，保持自动捕获闭环。
replay保留时间参数并产生对应Present action/输出资源，不按捕获机器时钟睡眠或重新调度。
拒绝负数、NaN、无穷时间及错误对象身份；不把离线metadata等同真实屏幕帧调度测量。

buffer标记调用native并记录字符串/范围；活跃capture写入frame stream并引用buffer。
后台removeAll仅丢弃已被覆盖的旧marker chunks，不丢buffer创建或数据；避免反复reset/add
无限累积旧注释。Replay保留structured API元数据，不重建跨seek的native debug marker表，
也没有新增Buffer Viewer范围高亮UI。range用减法界限检查防溢出。

T45/T46复用PHASE45同步链，分别设置 `RENDERDOC_METAL_TIMED_PRESENT=time` / `duration`。
T45使用已过去的合法atTime=1.25秒；T46 minimum duration=0.001秒，不做长时间等待。
额外172-byte buffer全0x72，只被debug marker引用，必须捕获并保留初始内容。
捕获帧内：remove→payload range[4,20)→remove→final range[32,44)；后台只保留最后reset/final。

必跑：两模式native验证层5帧/capture8帧/XML/3-loopCLI；复用完整fence依赖与seek验证，
marker-only buffer逐字节和Present action输出；T45 21、T46 10类异常无crash/hang拒绝。
旧普通present与共享包装受影响，按PHASE45执行47份联合回归；T00空帧额外定向验证。
GUI只新增API参数/字符串/资源链接/两个Present变体检查，不重复T44所有GPU数值。
T45和T46各自保留待验状态，统一到QA_CONSOLIDATED；现在不做Computer Use。
