# B484：加速结构 GPU 标识查询与资源关联

backend/bundle SHA256：
cde959303fd49b3271e937aa7e46fbb1494ea91c9100d173aaaee94ec80e96fa
GUI binary：3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。

## 实现与对照

Vulkan vk_get_funcs.cpp 的 vkGetAccelerationStructureDeviceAddressKHR 直接unwrap
AS后查询原生地址；DX12 d3d12_resources.h 的 GetGPUVirtualAddress 返回原生地址，
GPUAddressRangeTracker及RefBuffers负责资源关联/依赖。Metal AS gpuResourceID 同样
精确返回native标识，追加CaptureGPUIdentity专用chunk（Max1423），与通用buffer/
texture/sampler descriptor coverage保持独立。

每对象只记录一次；捕获中每次查询都保留ResourceId依赖，首次帧中查询同时保留
资源record和frame副本，复用既有RefGPUIdentityResources覆盖预先缓存的getter。
重放记录原标识，只允许已创建、当前设备的AS；零/未知/错误类型/冲突标识拒绝，
相同metadata重复合法。原始整数不会直接提交到回放GPU，也不扫描任意buffer整数。

此批仅完成getter与可验证资源关联。Indirect TLAS实例字段、UE IR header以及
VFT/IFT GPU ID使用的重定位尚未实现；不能把原生getter通过当成raw GPU consumer
回放支持。两项生产光追能力继续false。

## 实跑

入口：util/buildscripts/scripts/test_metal_ray_identity_macos.sh。
六场景：identity、background-identity、background-identity-unused、
heap-frame-placement-identity、background-heap-placement-identity、
background-multi-indexed-tlas-identity。

native/injected均稳定非零，injected明确对照wrapper.real的原生getter值；四次实际
ray query结果0/1/0/1。未构建且未绑定的额外AS仅凭getter仍出现在capture中，
并可重放，无需虚构AS初态。六份API三方向/EID0/输出/绑定、共738事件选择及
各3轮CLI PASS。每份6个损坏identity拒绝+相同副本合法，合计36坏输入、6合法
副本，重复getter metadata去重通过；18旧API/CLI检查通过。7 captures×10打开
生命周期PASS、resident growth409600bytes，backend起止hash一致。

官方两scene重新native/capture/offline像素逐字节一致、44/46事件三方向及CLI×3
PASS；10组尺寸/heap查询与native相同、41坏sample输入和6能力查询PASS。
产物build-macos-debug/metal-ray-b484/identity-manifest.json、sample-manifest.json；
当前官方gate-results为cde95930，32333bb6已归档。

## 尚未验收

本hash全量未运行；上一波32333bb6已通过308捕获/7784反例/3080生命周期，增长0，
不冒记本hash全量PASS。GUI原用户crash场景仍未知，独立header修复证据见B483。
ARC提前释放问题仍未修。实际UE session20261005-174756已越过AS getter，随后管线VisibleFunctionTable
断言失败。renderdoc.log明确为compute VFT count>32被桥接拒绝；UE
MetalRayTracing.cpp:1233使用AllShaders.Num()+1，下一批补此实际容量缺口。
ue-ray-diagnostic/b484-cde95930/manifest.json为FAIL；没有可验收RT capture，
RT dispatch/离线未证明。尝试取线程快照时进程已退出，未产生可用线程证据。

第一次坏输入测试把metadata移到DriverInit前，文件容器正确拒绝，但错误消息
不属于预期AS资源关联，测试FAIL；已改为保留DriverInit、移动到AS创建前，
目标资源关联拒绝PASS。原日志/manifest保留before-birth-oracle-failure后缀。
五例首轮PASS manifest保留；随后加入未构建/未绑定getter依赖，最终六例PASS。
未自动提交或推送。
