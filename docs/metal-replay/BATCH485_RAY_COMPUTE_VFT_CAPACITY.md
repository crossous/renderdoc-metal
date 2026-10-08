# B485：UE 光追 compute Visible Function Table 容量

backend/bundle SHA256：
80c2d706aeabfbc3b8363ddb91023a13e0576466433638a201435acd5b0f49a4
GUI binary：3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。

## 实际触发与实现

B484 UE session20261005-174756在MetalRayTracing.cpp:1233断言VisibleFunctionTable；
RenderDoc日志明确compute newVisibleFunctionTable桥接层把count>32拒绝。UE实际
传入AllShaders.Num()+1；函数表容量不应等同于buffer binding slot数。

先对照Vulkan vk_draw_funcs.cpp Serialise_vkCmdTraceRaysKHR的SBT区域原样序列化/
重放，以及DX12 d3d12_command_list4_wrap.cpp DispatchRays的shader record关联和
资源依赖。沿用Metal已有functionHandle→function→pipeline资源图与每槽更新chunk。
本批compute VFT从32项扩大至65536项的明确重放分配预算，捕获与重放统一使用
常量；原生创建仍决定设备是否支持。边界采用uint32且桥接先校验NSUInteger，
避免截断。handle类型、所属pipeline/stage、index/range、nil清空规则不放宽。

不扩render VFT、compute/render IFT、buffer slot、AS geometry/instance IFT offset、
linked binary functions或GPU raw ID consumer。旧T124“33非法”改为65537预算越界；
33的真实GPU正例已验证，不能将旧人为限制保留为负例。

## 验证

入口util/buildscripts/scripts/test_metal_ray_large_visible_macos.sh。
四例large-visible-33、large-visible-256、large-visible-65536、
background-large-visible-256：真实ray query命中后调用表末槽visible function返回3；
IFT清空/rebind带来0/3/0/3输出，足以检查末槽不是只分配未使用。包含尾部range
更新、nil清空和恢复，结构化文件检查count、全部5次VFT更新/2nil/3末槽更新。

native/capture/API三方向/EID0/每事件字节及AS绑定、共705选择、每帧CLI×3 PASS。
4×8=32坏输入（零/预算/overflow count、末槽后index、unknown/wrong/intersection
handle）干净拒绝；20旧API/CLI检查和T12416反例PASS。5 captures×10生命周期
PASS、resident growth196608bytes，backend起止hash一致。visible-manifest.json记录。

官方两scene再次native/capture/offline像素一致、44/46事件三方向/CLI×3 PASS，
10尺寸查询native一致；41坏sample输入、6项原生/默认/诊断能力查询PASS，结果
metal-ray-b485/followup-manifest.json和gate-results。cde95930官方结果已归档。

## 集中与 UE 关口

本波两族后串行UE复验，再固定80c2d706集中：20份B482/B484/B485 RT captures
逐事件复验，随后308/7784/3080旧回归。当前集中回归PASS，产物以followup-manifest及
frozen-validation-80c2d706/full-regression.log为准；308捕获、7784坏输入、
3080生命周期打开PASS，growth0bytes、exit0，frozen/工作库起止hash一致。
UE诊断session20261005-180235已越过VFT容量，下一fatal为AS encoder bridge
line178的实例描述检查。UE BuildAccelerationStructure实际设置Private实例buffer、
offset、GRHIRayTracingInstanceDescriptorSize与Indirect类型，无children array；
当前只接Default/Shared直接实例。UE manifest FAIL，RT dispatch/离线未证明。
尝试线程采样时该进程已退出，没有可用线程证据，不能宣称hang。
20份RT captures在冻结库通过，共2619事件选择；旧集中回归已通过。

ARC提前释放仍未修；UI独立header模型回调修复通过，但原用户crash场景未复现。
两项公开RT能力false。本批尚未跑真实GUI，未自动提交/推送。
