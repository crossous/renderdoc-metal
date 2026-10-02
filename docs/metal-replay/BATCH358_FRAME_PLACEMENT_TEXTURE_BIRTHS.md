# BATCH358：帧内 placement texture 与 view 创建顺序

2026-10-01，持续目标 active，未提交或推送。

旧 warm UE f744c413…中 65 个 texture 的创建时间落在帧内，却全部被写进初始
resource section。heap texture 路径缺少 buffer 已有的帧内创建记录；新 view
只通过父资源 record 引用 heap 时，DataWritten 跳过父创建还会遗漏 heap。

newTextureWithOffset 复用现有 frame resource record、CaptureTransitionLock 和
frame placement seek lifecycle。frame texture 的 view 也按序记录；seek 在
GPU 完成后先释放 view，再释放父 texture，保留 ResourceId/metadata 并在实际
创建事件重新绑定 Native 对象。不为帧内父 texture 保存不存在的帧初 snapshot。
heap buffer/texture birth 显式标记 heap 父依赖，view 标记父 texture 依赖。
placement overlap 仍拒绝，没有把共享内存或无 CPU wait 的 alias 直接放行。

精确库 SHA256 ecc44d573c98e5fa28ac1ee0d7641877033a05a70f8441057bcea2a7dbcb67b4。
test_metal_frame_heap_texture_macos.sh：metal-frame-heap-texture.MxZt1o，普通/
view 四捕获、24 seek，2×2 Private RGBA 清屏及 copy readback 41/173 正确；
结构化 export 验证唯一创建、heap 父依赖、texture→view→encoder 顺序。
13 placement texture 畸形输入及 18 个 sentinel/相关旧捕获 API+CLI 通过。
旧描述符 21 类/674 反例同库全部通过。B356 的 bfe86da…全量 308/7786/3080 已通过，
不是本次精确库全量结果。UI 仍待手动解锁；未提交完整 UE GPU replay。

新真实帧 ef14a1edd963ee2910f69434208fd634e0102fea346950754d5082b9984661ec，
27194064 B，session 20261001-152110，owned UE 91310 正常退出。已保存在项目
Saved/RenderDocMetalCaptures/UE58_frame_textures_ef14a1ed.rdc，旧帧保留。
65 texture、14 view 正确留在帧内，帧前 texture 时间落入帧内的数量 65→0。
1676 帧初/1822 帧末描述符来源无错误、112 producer/2737 inlineVA/786普通
绘制常量匹配；393 draw、170 pass、125 CB/peak32、34 signal/0 wait。

新增 audit_ue_metal_native_heap_layouts.py 使用 query-only helper，1723 placement
的本机尺寸/对齐/range 检查无错误，未创建 heap/resource/queue 或提交 GPU：
336 历史重叠包含 225 frame buffer→frame buffer、107 background buffer→frame
buffer、4 background buffer→frame texture。历史重叠不能证明逻辑存活或 GPU
完成，也不能用不同捕获之间的数量变化代替 alias 生命周期证明。

完整 UE 尚未通过。下一步接通 future frame texture 的 typed descriptor source
和 render attachment preflight，之后继续 Private/alias/绘制范围等真实差异。
