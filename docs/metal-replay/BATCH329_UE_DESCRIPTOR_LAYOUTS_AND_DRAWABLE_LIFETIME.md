# BATCH329：UE 实际表布局、截帧结束时序与 drawable 生命周期

2026-09-30，本地 M2 Pro / 16 GiB，UE 5.8.3 Testproj。未提交/推送，保留旧
capture 与项目备份。完整 UE replay 仍未成功；本批实际修复捕获保存崩溃。

## 插件与实际布局

插件新增 `UE58MetalDescriptorLayouts.cpp`，仅针对匹配的 UE 5.8.3 headers。
从真实 RHI context→FMetalDevice→BindlessDescriptorManager 取得 primary resource/
sampler heap，使用 `GetCurrentBufferOrNull`、suballocation offset/length，记录
schema 1/2 的 24-byte entry 布局。未修改 Engine、项目 Content/Config。

Testproj 中插件源与 API header 已同步，UBT `TestprojEditor Mac Development
-MaxParallelActions=4` 构建成功，日志 `ue-plugin-final-build.log`。早期使用
GetCurrentBuffer 引入 AllocateBuffer 隐藏链接依赖，已改为 OrNull。
只记录 primary 布局，不宣称临时表、有效槽位与 CPU 写入来源的完整 coverage。

原来的下一帧 ticker 可能在 Slate present 前结束，导致 end=0/no backbuffer。
新增 `metal.capturePresented` device annotation：活跃捕获且已取得 backbuffer
才返回0；插件通过共享 phase/counter 状态轮询，最多2秒，成功后 GPU idle/end，
没有 present 则 discard。只用于就绪判断，不是伪造 GPU 完成或 replay 标记。

## 真实捕获崩溃与修复

会话 `20260930-221602`，PID43795，在 delayed end 后发生一次 **UE 应用崩溃**。
栈为 AGX `LegacyBlitContext::copyTextureToBuffer` → `WrappedMTLDevice::EndFrameCapture`
→ 插件 EndCapture；发生在缩略图读取，不是 UE GPU replay。本次没有整机重启。
CrashContext 位于 Epic/UnrealEngine/5.8/Saved/Crashes 的对应 pid43795 目录。

仅保留 command buffer 不能证明 drawable/surface 在 thumbnail copy 时仍可用。
对照 Metal completed 后回收路径与本机 SDK CAMetalLayer.h 的 drawable 复用规则，
修复 capture recording 对 native drawable、native texture、wrapped proxy 的保留，
直到 native GPU 完成、缩略图读取和序列化后才释放。direct drawable present
单独保留首个被选中的 backbuffer。backbuffer 使用 atomic，capture 切换边界
使用既有 transition lock，防止结束后晚到的 present 发布旧对象。

不能保留整个长帧的每一个 drawable：提交后只为选定 thumbnail 继续保留 acquisition，
其余 drawable 释放额外持有，texture/proxy 仍按 recording 生命周期保留。两个
小表 capture 各六次 present、每次销毁应用引用和 autoreleasepool、两次连续 capture
通过；超过通常 drawable pool 数量仍正常保存并回放正确像素。没有跳过任何等待。

## 自动重截与当前 UE 拒绝原因

会话 `20260930-222825`，PID45627，库 `81f182173d83…`（保留修复已加入，
最终 pool 优化之前），NewMap/15FPS/50%/Nanite off。22:29:32 开始，
22:29:33.171 end=1，文件保存完成；本次 editor 已关闭。旧文件均另存保留。

新帧 12,660,193 bytes，SHA256：
`7cd838c777636296c5897c5127ac93e59b3dd61f9c449deece9e2597ffdda86e`。
保护备份：`Testproj/Saved/RenderDocMetalCaptures/UE58_NewMap_layout_lifetime_7cd838c7.rdc`。
本地产物 `build-macos-debug/local-m2-descriptor-replay/ue-layout.rdc`。

CPU XML：13634 chunks、389 draw、101 render passes、7 heaps 声明3.5 GiB。
真实布局 buffer24/schema1/offset0/count786432/stride24；buffer25/schema2/
offset0/count4096/stride24。没有 coverage。身份记录1815：buffer662、texture1085、
sampler68；508条首次身份查询位于帧内（278buffer/230texture），其中至少344条
来自帧内创建。首个 chunk4293 对应 chunk4292 的 buffer-backed texture11784。
分阶段证据 `ue-layout-identity-phases.json`；不能把这些全部当成帧首资源。

最终库 API 返回 APIReplayFailed（进程4），CLI退出1，明确拒绝 frame-born/invalid
GPU identity，日志没有 replay wait/private initial upload；**没有提交完整 UE 帧**。
另有无 coverage 的独立限制，即使处理帧内身份也不能直接越过剩余验证。

UE 源码 MetalBindlessDescriptors.cpp：FreeDescriptor 只延迟交还 allocator，
不清除 table bytes；UpdateDescriptorImmediately 直接 CPU 写入；
FlushPendingDescriptorUpdates 使用临时256-byte heap、entries/indices allocator
slice 和 packed uniform，由 compute 更新标准表再插入 barrier。
因此下一阶段需要有效槽位/帧内资源生命周期、临时表与 slice 布局，以及 CPU
写入来源和实际 render/packed uniform 重定位。不能将缺项置零、扫描常量替换，
或把 GPU 更新快照当 CPU 写入。先在极小用例扩展，再讨论本机3.5 GiB帧。

## 分类验收

- **定向终端通过**：最终库与 viewer 内嵌库均为
  `bd255367cb6030fea2b1d05b4e4894a3401539d55d7f553b22a3c85c799da600`；
  BATCH328 两类真实 GPU replay/seek、13负例、旧六帧 API+CLI 全通过。
  实际 UE 重截 end=1，捕获布局有 CPU 证据；完整 UE API/CLI 是预期拒绝。
- **全量回归未运行**，不能用六帧定向结果替代。
- **人工 UI 未完成**：Computer Use 返回 macOS 锁屏且无法自动解锁，已请求用户
  手动解锁；没有新的成功 GUI QA。解锁后先验证极小帧的打开、事件切换和画面。
- **UE 正确开启与 replay 目标尚未完成**。现有帧可继续定位，无需用户再次按钮重截。

当前记录及日志位于 `build-macos-debug/local-m2-descriptor-replay`；保留一次实际
应用崩溃的证据，不把修复后的成功捕获或 tiny replay 当作 UE replay 验收。
