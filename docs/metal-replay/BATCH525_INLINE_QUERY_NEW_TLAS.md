# B525：帧内新建 TLAS 的 inline query

完成。参照DX12 Serialise_BuildRaytracingAccelerationStructure与VK Serialise_vkCmdBuildAccelerationStructuresKHR的动态构建/typed地址重定位，以及现有Metal TraceRay当前配方。query不再要求TLAS在初态表中；顺序预检必须已记录对应typed frozen build，并验证队列、提交及encoder顺序，实际调度还检查m_LastBuildKind5。AS分配/身份本身不能授权query。两个Shared static header分别指旧初态与新帧内目标，子BLAS保持原闭包。

四场景native→capture→API/CLI3：1/64实例各root0/8，96事件前后往返/EID0。真实结果2241→2258、3312→3329；新TLAS无InitialContents recipe，只有帧内typed build。oracle强制AS-ID/VA变化、两header/roots/output独立重定位及保护字、初态输出7、Private实例源全部清零、旧AS仅首query读、新AS仅后query读，child依赖和两个输出usage通过。

两形状各88坏组，共176组/352 API4-CLI1预提交拒绝，无GPU wait。新目标漏build、build错目标、首query提前引用新目标全部拒绝；四合法UserID75/child-ID重编号控制API/CLI3及24-event oracle通过。

当前库旧B524八query仅API加强oracle；B521/B522/B523旧16 query API/CLI3；B512四TraceRay API/CLI3；固定官方Apple MIT两scene/270事件/EID0/10查询/CLI3和六能力通过。旧capture未fresh native。完整78IR/75RT/308、旧50texture/view/heap、旧84反例、32CPU/AIR、Qt/ARC、官方13坏sample NOT RUN，不继承旧库。UE没有新运行/截帧/GPU重放。原系统冻结/Qt/AS ForceCrash根因未闭环，同CB普通BLAS→TLAS仍拒绝。

首次probe native-build失败：将MTLResourceID直接赋uint64，改用公开结构的._impl字段后verified-probe与完整矩阵通过，原失败保留。生产guard未绕过。构建/GPU使用共享锁串行，有限超时，无提交推送。

产物：build-macos-debug/metal-ray-b525/manifest.json、matrix-manifest.json、negative-matrix-manifest.json、final/四场景、final-invalid/两形状、regressions/manifest.json与official/gate-results。最终库快照librenderdoc-final.dylib。最终源码哈希在manifest。

backend及bundle SHA256：99e80e2d2473281e90ec0b3b69fc864f7fffd24ecfaee10000c05146f5496b33；GUI：3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。

UE源码更正：MetalRayTracing.cpp:2544创建IndirectArgumentBuffer用StorageModeShared，2449–2457按Contents()写64-byte头及贡献GPU地址，并在构建时更新；MetalBindlessDescriptors.cpp:91–100用其GPU地址创建AS描述，但隔离provider缺Created来源记录。实际缺口是Shared动态/子分配header闭包和typed producer，不能把GPU实例输入或任意Private推测当作header事实。下一有界Shared header偏移，再动态producer/低工作集UE。两supportsRaytracing flags均false。

最终1280完成日志严格扫描无断言/overrun/streamseek/资源表或快照诊断。
