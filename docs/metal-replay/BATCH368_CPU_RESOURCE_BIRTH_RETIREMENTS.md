# B368：无GPU编码前导中的资源创建与描述符退役

当前实际UE目标仍未完成，持续推进；不提交/推送。

734378ff… CPU审计显示49个来源Native对象已经失效，但其精确generation slot frees
全部发生在首个CB创建30905前。heap newBuffer与新slot事件穿插在free之间；v28
仅连续free前缀可证明13条，不能把其他36条直接当可用Native identity。
Vulkan允许已失效但不会被使用的descriptor；采用明确的不消费证明，禁止地址猜测。

v29仅允许以下CPU操作穿插：standalone/heap buffer birth、CaptureGPUIdentity查询、
DescriptorSlotBinding/Table/GPUWrites声明、slot allocation/CPU write/free。任何CB创建、
encoder操作、GPU expected value或不在allowlist的操作立刻结束前导。精确Free的generation、
type、offset、initial bytes及完整后续lifetime仍由原校验验证；旧v16..v28规则保持原样。

新夹具在两条expired source free之间创建16字节Shared frame buffer和捕获其VA查询；
两份捕获、CPU/GPU-written表、GPU图像/累加、回跳、旧版本与畸形输入验证待执行。
Source已编辑，当前库eda11f64…全量仍在运行，未提前重建或冒充新实现测试结果。

精确cbfa65f9ca96bdde9d2217f6a01b21d405e671b07c7425b805cf172444a6c3c8，
metal-retirement.4dsJfD四捕获/16seek，CPU与GPU-written表、GPU122/186累计308、
像素186/122及EID0 stale字段清零通过。19×2 retirement反例与27×2 graphics
反例，共92组API+CLI通过；v28降级、CB创建后free、dispatch/commit后free仍在GPU前拒绝。
runner旧硬编码PASS打印90，已依据各gate的19/27实测修正为92；没有修改测试行为。
旧v16兼容四捕获/16seek/90反例通过（metal-retirement.f0zFJR）；全量与UI未重复，eda11f64…全量此前通过。
