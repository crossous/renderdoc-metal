# B491：多子 AS 间接 TLAS 与动态描述数组

backend/bundle SHA256：e6f18f116c5540464654339c4a5b02099f77384499d69b7efc506af6a85d6fac。
GUI仍3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。

## 触发与实现

B4900495实际UE越过Private placement输入guard，FAIL于原1–4 primitive候选检查。
参考Vulkan CopyInputBuffers、DX12 CopyBuildInputs的动态TLAS实例/依赖数组，
保留typed身份关联和执行点快照，间接子AS预算扩到1024。直接Default/UserID仍4；
实例总数65536、输入64MiB、先前提交primitive版本与Tracked输入边界不变。
不增加AS内部查看、shader单步或RT Pixel History。

Shared帧内/帧前、Private候选、提交物化及schema7初态校验边界同步；typed GPU ID
用map查询并拒绝zero/collision，Private按真实packet首次出现顺序筛选候选，保持
子版本依赖。原chunk/schema不变。MultipleDistinctInstanceDescriptor原固定4项
nativeChildren数组改为动态rdcarray，否则扩大候选后造成实际栈越界。

## 已运行证据

入口util/buildscripts/scripts/test_metal_ray_many_instances_macos.sh。五例为5子Shared、
33子padded/high userID、33子Shared帧内、33子Private同CB、128子Private placement
同CB distinct alias擦零。构建多个不同geometry，仅最后BLAS位于可命中位置；实例
以相反AS顺序写入，检查正确userID73/4026531913、唯一typed身份与完整N个依赖。
真实native/capture/API/CLI PASS：38×4+44=196事件各三方向，共588次，含EID0。
4×46+42=226坏输入PASS，20旧检查/T12416反例PASS；6×10生命周期PASS，
resident growth327680bytes，backend起止与bundle hash一致。实际最多128子GPU已验，
1024仅重放/捕获分配预算，未把它计为已跑上限。

候选f991在33子捕获发生SIGABRT栈缓冲区溢出，定位描述工具固定4项数组；该候选
5子结果同样经过越界，不计PASS。ObjC测试复制对象auto类型错误已改明确descriptor
类型。最终e6的128 children-limit反例被反序列化allocation bound提前干净拒绝，
测试仅对此tag接受精确invalid/corrupted data: Reading off the end of data stream，
其他反例继续要求Metal错误。所有原FAIL/manifest/log/ips按before-*名称保留。

产物captures/metal-ray-b491、build-macos-debug/metal-ray-b491/indirect-manifest.json。
官方两scene/10查询/41坏sample/6能力查询与58份旧新RT6609事件、AS身份旧反例和schema2兼容PASS。实际UE session20261005-215022已越过多候选guard，下一FAIL为间接count=0；尚无RT dispatch/离线验收。固定库集中308捕获/7784坏输入/3080生命周期PASS，resident growth0bytes、exit0、工作/冻结库起止hash一致；证据frozen-validation-e6f18f11/full-regression.log与followup-manifest.json。旧0495固定集中308/7784/3080 PASS只覆盖旧库。

## 接续与限制

下一根据实际UE新失败驱动。帧内Private、同CB child build、untracked、ARC仍需证明；
用户原Qt尺寸测量崩溃未复现，独立header旧模型回调修复不等同原崩溃验收。
生产两项光追能力false，未提交/推送。集中与UE结果完成后更新本批。
