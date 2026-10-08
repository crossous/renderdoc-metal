# B488：Shared 间接实例的 typed AS 标识重定位

backend/bundle SHA256：c924730b4dde4c55b37e5c6d4d63a50f065ece7eccc199b346f91ffabf8bc825。
GUI仍3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。

## 实现与对照

对照Vulkan CopyInputBuffers/初态重建与DX12 CopyBuildInputs和GPUAddressRangeTracker
的BLAS关联。只在72-byte Indirect公开实例的AS字段(offset64)解析查询过的AS GPU ID；
通过该device的typed AS ResourceId关联子结构，不扫描普通buffer中的整数。
捕获仍提交应用原有Indirect descriptor给Metal；重放将完整transform/options/mask/
IFT offset/userID与typed child转换成UserID直接实例，使用live child handles。
不向重放设备提交捕获期的原始AS ID，也不原地改写用户buffer。

新增append buildIndirectInstances（Max1425），校验Shared、非heap、source offset/
stride/count与预算、CPU捕获副本、已构建且来自此前提交的1–4个primitive子AS、
资源/设备与大小。GPU producer、Private/Managed间接输入、motion/refit、未查询/
未知/零AS标识与嵌套TLAS仍拒绝；这不是任意bindless地址重定位支持。

kind9/schema7初态保存原始packed72描述和typed children，捕获使用查询关联的
childGPUIdentities作内部验证；重放从子AS身份metadata重建关联，再转换成独立
packed68 staging。沿用子结构版本、先BLAS后TLAS、compact/copy与EID0恢复。
AS getter现在也记录capture-side ID；读取身份metadata拒绝不同活AS占有相同ID，
同AS相同ID副本仍允许。规范化stage只在提交完成后释放，callback不持有wrapper。
尺寸/heap查询允许Indirect合法填充步长；已验stride80/offset64。

## 已运行验证

入口util/buildscripts/scripts/test_metal_ray_indirect_instances_macos.sh：七例覆盖
单实例、两子AS三实例、stride80/offset64与高位userID、flags、源描述在AS构建后
完整擦零、帧内构建、compact后copy、mask0。真实ray输出0/73/0/73，或完整高位
4026531913，mask0全零；native/capture/API732事件三方向/EID0/CLI各3 loops PASS。
擦零例各事件读取source全零，而AS依然返回原编号，证明未从最终buffer猜恢复。

6×46初态+42帧内=318坏输入PASS，包括原始AS ID零/未知、身份metadata缺失/
冲突占有、错type/birth、offset/stride/count、NaN/options/mask/IFT、子依赖及schema。
20旧API/CLI检查与T12416坏输入PASS；8 captures×10生命周期PASS，growth720896bytes，
库起止hash一致。indirect-manifest.json记录，captures/metal-ray-b488保存输入。
首轮C++构建因同一行两个序列化宏产生相同__LINE__局部变量名失败；分行后构建
通过，原始失败日志metal-ray-b488-build.before-macro-line-fix.log保留。

官方两个scene/10查询、41坏sample/6能力查询、38份B482–488 RT/4491事件、AS身份旧反例/
schema2兼容已PASS，固定库集中全量308/7784/3080 PASS，growth4653056bytes、exit0、
工作/冻结库起止hash一致。后续B489只编辑源码，等待此全量结束后才构建；不归入本库证据。
初轮复验发现B487/B488入口误留B486产物目录，找不到约定路径；修正入口与manifest、
无损移动原捕获后38份重新通过。旧FAIL与冻结目录保留before-artifact-path-fix。证据gate-results、
metal-ray-b488/followup-manifest.json与冻结库目录。B486 a2e87831集中308/7784/3080
PASS/growth0属于旧库；本库集中结果单独记录。

## 接续与未验

下一Private/GPU间接实例执行点冻结与关联，再复验UE；现有UE仍会被Private guard
拒绝，未重复同一无变化失败。实际RT dispatch/UE离线、ARC、用户原Qt crash与GUI
均未验收。生产两项RT能力false，持续推进，未提交/推送。
