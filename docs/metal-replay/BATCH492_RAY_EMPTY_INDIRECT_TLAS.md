# B492：显式空间接 TLAS 配方

触发：B491e6f18f11实际UE session20261005-215022越过多子候选检查，下一FAIL为
Private/Tracked placement instanceCount=0/stride72/offset0。尚未证明RT dispatch。

仓库Vulkan vk_acceleration_structure.cpp的CopyInputBuffers在currentDstOffset==0
时创建最小输入buffer并跳过输入copy/barrier，保存zero primitiveCount；DX12
D3D12RTManager::CopyBuildInputs保存NumBLAS=0，不读取实例输入。Metal按相同
黑盒语义保存显式空状态，避免把没有冻结字节误当成未完成配方。

已编辑kind10/schema8初态与现有buildIndirectInstances零count分支；要求没有子AS、
payload/indices空、合法typed descriptor和输入范围。原生提交完成才物化；重建使用
72字节独立最小allocation、Indirect/count0，不读取原buffer，也不重定位不存在的
AS ID。Shared帧内、Shared/Private帧前，Tracked placement初态来源沿用B490边界；
尺寸/heap查询只扩大到Indirect/count0/无children/usageNone。未扩空refit/compact/copy
或帧内Private、untracked、跨队列语义。生产能力false。

五个empty fixture已写：Shared帧前、padded Shared帧内、Private帧前、placement帧前、
placement同CB distinct alias擦零。检查实际ray miss0/0/0/0、native/capture尺寸一致、
AS初态仅空TLAS且不包含未用BLAS，以及事件往返/EID0、独立坏输入和生命周期。
入口util/buildscripts/scripts/test_metal_ray_empty_instances_macos.sh。

Python/shell语法检查PASS。原fixture offset64==length64被原生MTL检查拒绝，保留原FAIL/log；空输入分配至少一stride，offset0或padded offset64，新增参数校验严格offset<length。修正后native基线ray0/0/0/0 PASS，size1024/scratch5632/refit512/heap1024 align256；e6旧库capture SIGTRAP于zero query。B491集中308/7784/3080已PASS，growth0/hash一致，当前开始新库串行构建，未验新库GPU。
最终backend/bundle SHA256：ae81e24acebff8ccc76a8e1d9562cd2038b002a6dd4ffdccbc1259e51279c50f。
GUI仍3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。
五例native/capture尺寸与heap查询一致，实际ray0/0/0/0 PASS；38×4+44=196事件
各三方向，共588次，EID0/CLI各3 loops PASS。4×35+29=169损坏初态/帧内拒绝，
20旧检查/T12416反例PASS；6×10生命周期PASS，resident growth589824bytes。
backend起止与bundle hash一致。产物captures/metal-ray-b492、
build-macos-debug/metal-ray-b492/indirect-manifest.json。新库官方/UE/63份RT接续中，
集中未跑；e6f18f11固定集中308/7784/3080/growth0 PASS属于旧库。ARC/用户原Qt crash仍未闭环，未提交/推送。

B492 ae81e24a接续结果：官方两scene/10尺寸查询/41坏sample/6能力查询、63份B482–492 RT7197事件、AS身份旧反例/schema2/旧Indirect46反例PASS，冻结/工作库起止hash一致。UE session20261005-222357已越过zero count，下一FAIL为private间接primitive候选count0（limit1024），instanceCount尚需补诊断，不能推定为null AS。当前库集中未跑；B491 e6集中308/7784/3080/growth0 PASS仅旧库。下一Native inactive instance探针及UE精确候选/参数诊断；生产能力false。
