# BATCH58–59：纹理 view 与共享存储别名

起点136 bridge / 82旧chunk，Max1278。延续终端开发与自动验证，GUI L4留在总单。

| 功能族 | 目标 | 数据与回归边界 | 状态 |
| --- | --- | --- | --- |
| T58 纹理 view | 三种`newTextureView`重载，父资源依赖、mip/slice与swizzle、GPU读写共享 | 原生/capture/API/CLI；父→子和子→父可见性、资源身份、seek；T03/T09/T37/T50 | 终端通过，GUI待验 |
| T59 buffer-backed texture | `newTextureWithDescriptor:offset:bytesPerRow:`，Shared父buffer与纹理共享；两个alignment查询不再报未接通 | 原生/capture/API/CLI；buffer与texture双向数据、padding/offset、seek；T12/T39/T57 | 终端通过，GUI待验 |

T58接通旧chunk 1076–1078：只支持Shared、RGBA/BGRA8、2D/2DArray、同格式、单采样、
非嵌套view。捕获记录父子依赖，回放登记Texture资源。为保证逐事件逆向seek，CaptureScope
保存父纹理所有mip/slice的初值，ReplayLog重置；每父纹理快照上限64MiB。不支持格式重解释、
Private/Managed、MSAA、cube、嵌套view，畸形输入会明确拒绝。

T59接通旧chunk1193：Shared buffer上的RGBA/BGRA8 2D纹理，按设备真实alignment校验
offset/row pitch及范围，登记父子依赖。捕获开始引用父buffer，并把使用子纹理的提交关联至
父buffer的CPU写跟踪，确保初值与两次提交间更新；回放保留共享存储语义。Private/Managed、
其他格式/纹理类型、多mip/slice及超范围布局不在本批承诺。

两fixture原生Metal验证、capture和逐事件Replay API/CLI均通过。T58检查9draw中父→view
及view→父GPU写入、subset与swizzle，并按`8,0,4,1,7,2,5,3,6,8`逆序/交错seek；
T59检查3draw的初值、CPU更新、GPU纹理clear对父buffer的双向可见性，以及每个padding字节，
按`2,0,1,0,2`seek。定向旧T和15份代表capture通过，56+35畸形变体拒绝无崩溃。

共享初值改动触发集中门禁：`/tmp/metal-batch58-59-final.log`，最新构建上T01–T59
及T10 marker共**60份API/CLI、1730畸形样本、600次lifecycle打开**全部通过；resident
growth524288bytes。`bash -n`及`git diff --check`通过。未运行GUI/Computer Use。

剩余**130 bridge / 78旧chunk**，Max1278未变。正式capture SHA-256：T58
`c88c79833879dd8a0b56fb0878119f24f89b412d82a3c03f56396ce5387b3d5c`，T59
`0a9c40b8edefc85c3158460ea4c08365d3d2a01e987c33c2cde6c4c9492e58df`。构建
库与app内嵌库一致：`474533dbd0be2f4429a8513c781fd974f1368c0bd98f0d694e8cf3b3dee28cea`；
GUI executable沿用`fe8bcf852b683bc463a3be883e6c54208b7bd45054a24f5dbace58c912346d74`。
一键入口`test_metal_capture_batch58_59_macos.sh`本波分段等价执行，未整体重跑；全量
replay入口设`RENDERDOC_METAL_LAST_TEST=59`。无提交/推送；历史工作树和用户UE路线保留。
