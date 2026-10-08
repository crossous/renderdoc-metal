# B487：直接实例 userID、偏移与填充步长

backend/bundle SHA256：26303b3c83ba8d1e98af03bd419ea94dbc61ef7976600d3d51ce59ad395b7d67。
GUI仍3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。

## 实现与对照

B486间接实例原生基线返回userID73；转换间接AS引用之前必须保存该字段，不能
降成默认64-byte实例并丢掉userID。Vulkan实例的custom index、DX12 InstanceID均随
实例输入复制与重放保留（vk_acceleration_structure.cpp CopyInputBuffers、
d3d12_manager.cpp CopyBuildInputs）。Metal的UserID直接描述采用68-byte packed
布局，支持完整uint32编号；此必要语义按Metal公开布局实现，不增AS内部viewer。

新增append chunk buildUserIDInstances（Max1424），保存typed child资源、source、
scratch、offset/stride/count和规范化的68-byte描述输入；帧内Shared直接构建校验
原始CPU输入与捕获副本、子AS已构建且来自此前提交、资源/设备、预算、范围和
输出分配。拒绝未知type、motion、refit与前置同CB GPU producer，Managed仅帧前。
不放开间接AS GPU标识或Private实例输入。

kind5的UserID初态采用schema6，类型与schema严格配对；既有schema1–5布局不变。
按source offset/stride提取每条68-byte实例，保留transform/options/mask/IFT offset/
child index/userID，初态重建使用独立packed staging。源码静态断言默认64-byte
前缀与userID位于offset64。复用已有子AS初态版本、compact/copy及EID0重建流程。
尺寸/heap查询允许UserID与合法偏移/填充步长，不读取描述payload。

## 已运行验证

入口util/buildscripts/scripts/test_metal_ray_user_id_macos.sh：七例覆盖单实例、
两子AS三实例、offset64/stride80、完整高位编号0xf0000049、flags、构建后改源
transform和userID、帧内构建、Managed帧前、compact后普通copy。
真实ray输出0/73/0/73或0/4026531913/0/4026531913；native/capture/API732事件
三方向/EID0/CLI各3 loops PASS。6×41初态+38帧内=284坏输入干净拒绝PASS。
20旧API/CLI检查、T12416坏输入PASS；8 captures×10生命周期PASS，
resident growth458752bytes，库起止hash一致。user-id-manifest.json记录。

最初异常测试把“缺失子初态”的合法拒绝消息限制得过窄，帧内两子AS测试沿用
单子AS初态数1；修正预期为实际拒绝消息及两个子初态后重跑全批通过。原始FAIL
manifest与日志保留before-oracle-fix/before-frame-oracle-fix，不把初轮计为PASS。

官方两scene新截/native/offline字节、44/46事件三方向/CLI×3、10查询PASS，
gate-results固定本26303b3c库。41坏sample/6能力查询、24份旧RT/3027事件及7份Managed/
flags旧例、schema2兼容（3 loops）与旧TLAS24坏输入PASS，followup-manifest.json记录。
初轮兼容脚本选用了已是schema2的B472源帧，删除不存在的indexSource时失败；
改用B476 schema4源帧进行真正的schema2降级转换后通过，错误选帧日志保留。本库集中全量/实际UE/GUI未跑；a2e87831的308/7784/3080
和growth0只属于B486库，不冒充本库全量结果。

## 接续

下一批针对Shared间接实例建立typed GPU ID→ResourceId关联与重放规范化，不
扫描任意整数；再补Private/GPU执行点证据，之后复验UE。ARC与原Qt crash仍未验。
两项生产光追能力false，持续推进，未提交/推送。
