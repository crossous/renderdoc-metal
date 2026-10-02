# BATCH337：显式 slot 来源的 GPU 描述符复制与回跳

2026-10-01，本地 M2 Pro；目标 active，未提交或推送。完整 UE replay 未完成。

## 实现与边界

复用 B335 的逻辑 slot shadow、显式 source ResourceId+offset 与既有 native blit
和 replay submission 路径。coverage v5 允许静态 Shared buffer24 条目从已知
CPU payload 复制到声明 GPU-owned 的目标：

- copy 必须完整24 bytes、命中 live source/destination slot、同 descriptor type；
  source 不能 GPU-owned 或携带未验证的 GPU expected 值。
- CPU预检把 source payload 的逻辑值及来源与 copy 后的 GPUExpected/Binding
  对上；缺失、重复、偏移、晚于消费者或字段不一致均在 GPU 前拒绝。
- GPUExpected 只更新逻辑 shadow；运行中不调用 ReplayCPUBufferUpdate 写目标，
  也不由普通 CPU diff 覆盖 GPU 输出。实际 VA 来自已重定位 payload 的 GPU复制。
- 帧首/seek使用缓存 Initial Contents 校验逻辑声明，不能在尚未上传初始状态时
  读取新建 native buffer 当成 capture 原字节。恢复时显式重定位初始 GPU-owned值。
- 保持一个 command buffer、≤4次1×1×1 dispatch、≤2次24-byte copy、2×2 clear、
  单buffer≤64KiB、总静态heap/buffer≤1MiB；禁止 draw、帧内 birth/alias、GPU
  compute scatter 和其他 blit。UE provider 仍不声明完整 coverage。

## 实际 GPU 证据

`metal_descriptor_sourced_gpu_capture.mm` 两份捕获：第一份 shader读41→GPU
复制B payload→shader读80；第二份不CPU重写目标，以第一次GPU结果为帧首，
shader读80→GPU复制A payload→shader读41。每份检查两次shader实际输出、
实际间接VA/inlineVA、普通常量及DEADBEEF、copy EID目标与payload全24字节一致、
四轮事件回跳、EID0恢复及2×2 BGRA像素128/64/32/255。

独立 replay 先占1MiB，capture VA 90194739204/90194739464 对应 replay
90195820548/90195820808，table VA也不同。确实是新进程重定位后的 GPU字节。

新增21 API+CLI负例：GPU ownership缺失/v4契约、copy缺失/重复/partial/offset/
wrong object/end顺序、expected缺失/早于copy/晚于consumer、source缺失/错误/
offset、预期VA/普通常量不一致、CPU替代GPU值、initial destination/payload不一致。
全部在首次frame GPU wait/Private upload之前拒绝。

## 定向终端结果

运行 `bash util/buildscripts/scripts/test_metal_descriptor_relocation_macos.sh`：
八类原生/注入、各两份GPU replay/seek、**99组API+CLI负例**、slot诊断与冻结
metadata竞态通过。日志 `build-macos-debug/metal-descriptors.bvPOIp`；顶层
`descriptor-sourced-gpu-final.log`。独立早期两份日志在metal-descriptor-sourced-gpu。
库SHA256 `4bd948d286c29505976e891b85e064f838cb555c71a52d3bacd4b80a280013d7`。

全量回归未跑；本批人工UI未验；完整UE画面/MRT/pass scope全部仍待验。
真实UE最新捕获仍为B336 a08ef579…，1,476/1,476帧首槽位字节一致，未做UE GPU提交。
继续按官方UE UpdateDescriptorHandle.usf适配一线程循环scatter来源与计算更新；
B337的blit验证不是UE计算更新完成证明。
