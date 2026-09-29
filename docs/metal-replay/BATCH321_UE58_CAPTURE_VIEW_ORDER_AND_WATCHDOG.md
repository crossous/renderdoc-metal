# BATCH321：UE58_capture 的 buffer view 顺序与本机 watchdog

2026-09-29，Apple M4 Max / macOS 26.6，开发工作树为独立
`renderdoc-metal-t312`，基础 HEAD `e0a26f7e65e22a3890aa38a318bde51d24babf6b`。
原 `renderdoc-metal` 的四项本地改动已比对，其中构建脚本的两项已合入本工作树；
原目录没有 reset、stash 或清理。用户的原始截帧在
`SocoTestProj/Saved/RenderDocMetalCaptures/UE58_capture.rdc`，26,061,196 字节，
SHA256 `a96e685f726608bb30a84f2ae0c7485d2edaaad82553b8e95da1df44e91915f0`。
会话 `20260929-202104` 的 manifest 与 UE 日志证实注入库
`ca90af4c95ab69858145eadf1e3023de16cbbb424d565eff864e3b91c7432872`、
按钮主动绘制 viewport、受控 capture start/end 以及文件保存。

## 原始首阻塞与修复

原始 RDC 的有界 API `OpenCapture` 退出 4，日志
`rdm-ue-replay.P2TA0y/api.log`：`MTLBuffer::newTextureWithDescriptor` 的父
buffer 尚未创建。XML 中首例 view `250154` 位于帧前 chunk 1816，
父 placement buffer `250150` 位于帧内 chunk 2540。共有 15 个这样的
view；每个在原生时间戳上均晚于父 buffer，同线程；错误来自 capture
资源记录把 view 提前放进帧前区。D3D12/Vulkan 的资源记录也须保留父资源
身份与创建依赖。Metal 修改为：活动帧内创建的 buffer texture view 在
有序帧流写创建 chunk，资源记录仍保留供后续帧使用；逐事件回放时先释放
旧 view、再重新绑定新父 buffer 的 view。

本机在重启前的最小 Metal Validation 正例、注入截帧、API/CLI 单次回放、
资源父子身份和像素事件回跳均通过；4 例畸形输入安全拒绝。
正例 capture SHA256 `c9b5f3587da39a758107b8401024bf51e9fde07ebb89ad1c9c3da77d868ed419`；
原生像素 BGRA `128 255 128 128`。夹具源码在
`util/test/metal/metal_ue_frame_typed_buffer_views.mm`，使用旧的 typed view
回放 GPU 探针及新的负例脚本。重启清空了 `/private/tmp` 的构建和夹具
截帧，不能把这些文件当作可交付物。

## 下一阻塞与主机停止门槛

仅为诊断曾生成一份**未交付**的原帧顺序修正副本，SHA256
`45db270790407660288878daaace741f0209bb08183a2b0221324c2d813deb2d`。
它越过 buffer view 阻塞，随后有界 API 打开在 Metal Validation 下以
`SIGABRT` 退出：`Cannot set purgeability state to volatile while resource is in use
by a command buffer`，峰值 RSS 约 894 MiB；CLI 未运行。原始 XML 的帧内
chunk 11174/11175 是对 buffer `251031/251032` 的
`PurgeableStateEmpty`（值 4），此前有 blit 读取。UE 5.8
`MetalBuffer.cpp` 的 buffer 池回收路径会调用 Empty。RenderDoc Metal 在
commit 后不复现完成回调边界，`waitUntilCompleted` chunk 回放也为空实现。
这属于 GPU 完成/资源回收功能族，不能删除状态调用守卫或忽略此事件。

20:41–20:43 `WindowServer` 两次 watchdog 诊断后，20:43:38 发生
`userspace watchdog timeout: no successful checkins from WindowServer ... in
120 seconds` kernel panic 并重启；原始 panic 文件在
`/Library/Logs/DiagnosticReports/panic-full-2026-09-29-204338.0002.panic`。
这不是 `IOGPUResource::free` 断言；与本次回放/定向测试在时间上接近，
但现有日志不能证明单一触发点。14:30 本机也发生过同类 WindowServer
watchdog。按 BLACKBOX_GATE，自 21:02 重启后停止本机所有 GPU 测试。
原始 `.rdc` 完整未改；诊断副本随 `/tmp` 在重启时消失。

为避免再次将尚在使用的资源交给 Metal，当前代码在加载到
`CaptureScope` 时、执行帧 GPU 工作前扫描帧内 `MTLBuffer/Texture::setPurgeableState`
的 Empty/Volatile 事件并明确拒绝；单 chunk 守卫再作防线。这是**安全拒绝**，
不是可回放实现。后续须在另一台可承受测试的 Mac 上构造包含完成回调、
异步 command buffer 与 purgeable 转换的原生/注入夹具，验证提交、完成、
回收与事件 seek，再对真帧复测。不能在当前远程机继续尝试打开修正副本。

## 构建与未运行项目

重启后仅执行 `cmake --build /private/tmp/rdm-t312-build --target renderdoc
renderdoccmd -j 4`，编译通过；库 SHA256
`47b219e7273dc1d9c3d5faf8982370a87ac16526beb605550c3937b7ce4eb476`。
构建日志见 `evidence/ue58-t321-static-build.log`，panic 摘要见
`evidence/ue58-t321-windowserver-watchdog.txt`。该库没有在重启后做 GPU
回放验证。全量回归、长时压力、当前帧人工 UI 内容验收均未运行。
旧 `frame1770` 人工 UI 内容验收仍失败；累计成功 UI QA 增量 **0**。
当前 `UE58_capture.rdc` **仍无法正常开启**，首个仍未解决的功能阻塞是
帧内 purgeable 转换与 GPU 完成边界，之后还需检查场景画面及 GPU VA。
