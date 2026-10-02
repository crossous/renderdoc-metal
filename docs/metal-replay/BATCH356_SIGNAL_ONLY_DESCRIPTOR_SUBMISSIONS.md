# BATCH356：描述符提交中的单向 GPU event signal

2026-10-01，持续目标 active，未提交或推送。

实际warm UE帧f744c413…有35次encodeSignalEvent，0次GPU event wait。CPU审计
event_timeline_analysis按编码和command buffer提交分别验证递增，两种顺序
都没有问题；无frame host mutation。沿用已有MTLEvent/MTLSharedEvent的
PrepareReplay：每个seek epoch重建Native event，Shared恢复合法初始host值。

v19描述符preflight接受既有单队列上、已知未提交CB、encoder全部结束后的
signal，检查已知event/owner/type、严格递增的编码值、每个CB第一/最后signal
与真实提交顺序、256signal限额。GPU wait、future dependency、host mutation
及shared-event alias handle仍拒绝，没有引入替代同步机制或绕过完成等待。
CPU snapshot restoration继续等以前已提交GPU work，signal自身不会制造
依赖未来submission的等待。

定向精确库SHA256 bfe86da5097ff17c016c16431f049315d56807a1d4cd9ffacd4003f4f3b500ce。
script test_metal_descriptor_signals_macos.sh，metal-signals.BkaRuS：普通/shared
× retained/unretained8捕获、32seek、累加308、pixels186/122、168API+CLI
反例通过。Shared初始host值100，GPU信号101/102；重复seek必须重建timeline。
同库旧21类/674反例、implicit alias184反例、prefix retirement90反例、
75CB/peak11交错提交114反例、实际UE更新shader→小graphics27反例全通过。

同库T01–T312完整308正例/7786畸形输入/3080生命周期打开均通过，日志
signals-full-regression.log，resident growth 12042240 B。UI再次实际截图检查
仍被Mac锁屏阻挡；人工UI尚未验收。
完整UE replay尚未通过，继续parallel/MRT5、异步alias及实际资源/绘制范围。
