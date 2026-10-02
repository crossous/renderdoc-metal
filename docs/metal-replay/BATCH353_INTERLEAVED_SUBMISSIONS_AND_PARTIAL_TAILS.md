# BATCH353：真实 UE 创建与提交顺序、部分 replay 的队列预约

2026-10-01，持续任务 active。未提交或推送。

## 真实帧证据

UE5.8.3 Testproj 默认 FirstPerson 场景自动截帧及退出成功。捕获
`UE58_draw_constants_9af3f6bd.rdc`，14,230,670 B，SHA256
`9af3f6bd70c12b12be01fa389139f3ead6510a8a84069e355322c735a3cc1140`。
会话 `Saved/RenderDocMetalSessions/20261001-121550`。
实际加载 isolated provider SHA256
`feeed2284852266f8a43e52c428a2cec9c491056675b7a0b46bfeebd16d0e2ff`；
原安装 MetalRHI 未修改。

CPU 审计：1562/1562 帧初 live 槽 initial contents 匹配，1809 帧末 live
槽来源匹配；17/17 GPU producer 的来源和 payload 匹配，1978 个 inline VA
字段来源匹配，626 个普通 IR draw scalar 调用合法。生命周期、帧内 value
epoch、inline、producer、binding 问题均为 0。3367 项历史非保留源不能
视为本帧 live 失配。视口实际 2744x1690，窗口命令行分辨率并未控制它。

75 个 command buffer 全部同一队列 11、75 次 commit、无捕获 CPU wait，
peak uncommitted 11；没有显式 enqueue。前三个创建为 12492/12494/12498，
提交为 12494/12492/12498。22 个 GPU signal、无 GPU wait。87 个 render
pass，313 次 draw（281 indexed、17 direct、15 indirect），MRT 包括 5 色附件。
审计器补齐 parallel render 子 encoder 的父 command buffer 后，原来的
3 项未归属 draw 全部找到父资源，549 个 GPU 操作的未归属数为 0。
声明 heap 容量 4160 MiB，最大 buffer 576 MiB；仍未提交完整 UE GPU replay。

## 通用同步适配

对照 D3D12 `Serialise_ExecuteCommandLists` 和 Vulkan `ReplayQueueSubmit`
的 ResourceId / submission order 模型，继续使用 Metal replay core 的独立
command buffer 状态。描述符 v15 预检按 encoder 所属 command buffer
检查存在、未提交、没有同 buffer 的重叠 encoder；支持创建与提交顺序
不同及 up to 256 个 command buffer（与 core 上限一致）。GPU 工作量、
shader binding、来源、微型资源预算和跨队列/event/fence 拒绝条件保持。
Shared descriptor CPU 改写仍不能越过已经编码、尚未提交的使用；等待仅
完成同一队列对应 submission prefix。显式 enqueue 不得提交到尚未提交的
前序预约之后，避免 CPU 快照等待自身未提交队列预约。

测试先复现 partial seek 的 native queue 死锁：尾部按创建顺序先 commit
空 buffer 后等待它，而前方的已 enqueue buffer 尚未 commit。现在记录
显式 enqueue 和隐式 commit 的队列顺序，尾部先提交预约。另一个 partial
seek 错误是对仅创建、尚未编码的 future buffer 恢复未来资源快照；现在
只补交已有编码或预约的尾部，保留 future resource 出生边界。编码追踪
包括 render/compute/blit/AS/parallel encoder 和 event；不忽略 GPU 等待。

## 定向终端验证

库 SHA256 `5b08e3319217fa4a5a12bb3d4684f92e7a19b04740c1d03ea606f447e03eb2c3`。
`bash util/buildscripts/scripts/test_metal_interleaved_submissions_macos.sh`
通过，日志 `build-macos-debug/metal-interleaved.zc6JwF`。两种 retained /
unretained 创建方式、4 份截帧，每份 75 个 buffer、peak 11，16 轮 seek，
GPU 累加严格 308、最终像素 186/122；114 组 API+CLI ownership/order/
reservation/graphics 反例通过。超上限 257 buffer 在预检拒绝。
最初一次测试错误地保留 75 个未提交 Native buffer，触及 Apple SDK
注明的默认队列 64 个 non-completed buffer 上限；已改为和真实 UE 一样
的 75 总数、peak 11，没有扩大队列或掩盖错误。

B352 库全量回归已通过 308 正例 / 7786 反例 / 3080 生命周期。B353 通用
尾部代码改变后的全量待复跑，人工 UI 待解锁，完整 UE replay 未通过。
下一步降低实际捕获负载（可选受控视口尺寸、UE 自带 Nanite pool 参数），
继续适配 15 个间接绘制、5 MRT、parallel render、signal 和资源预算。

最终相同5b08e331…库全量复跑通过：308捕获 /7786损坏输入 /3080
生命周期打开，常驻增长12,795,904 B；日志 interleaved-full-regression.log。
旧九帧含T52 event、T65 shared event及T135 frame birth通过，日志
interleaved-targeted.log。最新人工UI仍未完成，UE完整GPU replay未提交。
