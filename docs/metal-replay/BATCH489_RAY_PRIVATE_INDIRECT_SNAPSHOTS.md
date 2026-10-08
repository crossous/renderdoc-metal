# B489：Private 间接实例的 GPU 执行点输入冻结

backend/bundle SHA256：349df4162fe467e1ded02560cad7e2075f15b5970471fcd48525c19685040ea9。
GUI仍3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。

## 实现与对照

沿用Vulkan CopyInputBuffers、DX12 CopyBuildInputs的执行点复制及typed BLAS关联。
扩B486 AS encoder-end隐藏blit到kind9间接实例；在应用后来encoder覆盖前冻结
Private输入，提交完成且无GPU错误后读取真实72-byte实例，只保留packet使用的
AS GPU ID及相应版本的primitive recipe。重放仍用B488 schema7与UserID/live child
规范化，原始GPU ID不会提交给重放设备。不增加chunk/schema或AS内部查看能力。

当前范围：帧前、Tracked独立Private、非heap/非aliasable、输入/唯一副本预算64MiB、
查询过的1–4个primitive候选且此前command buffer构建。未使用的候选可从TLAS
依赖裁剪，已单独覆盖unused-built场景；未知ID仍拒绝。帧内Private、heap/untracked、
跨队列未完成子结构、同CB子AS、motion/refit/嵌套TLAS/超过4候选未扩大支持。

## 已运行验证

入口util/buildscripts/scripts/test_metal_ray_private_instances_macos.sh，八例包含
Private单实例、同CB GPU上传、同CB构建后GPU擦零、两子AS三实例和stride80/offset64/
高位userID、未使用但已构建候选、后来CB擦零、compact/copy与flags。
native/capture真实ray编号73或4026531913；API 34×8×3=816事件前后往返与EID0、
CLI各3loops PASS。擦零例所有资源读回全零，但AS仍恢复原编号，避免最终状态替代
构建输入。8×46=368损坏初态拒绝PASS；20旧API/CLI和T12416坏输入PASS。
9 captures×10生命周期PASS、resident growth327680bytes；起止库hash一致。

官方两scene新截/离线像素逐字节相同、44/46事件三方向与CLI、10组尺寸查询、
41坏sample与6能力查询PASS。证据metal-ray-b489/indirect-manifest.json、
followup-manifest.json、ray-samples/apple-basic/gate-results、captures/metal-ray-b489。
旧C924官方结果归档gate-results-b488-c924730b。

初构建-Wshadow（object局部名）失败，改childObject后通过；replay辅助程序打印
ResultDetails.Message引入未导出DoStringise符号导致链接失败，改为数值code与已有
internal_msg后通过。原失败日志/manifest保留before-shadow-fix与before-error-print-fix。
这两项属于构建/验证工具失败，无GPU结果被计入通过。

## 接续与未验

349df416下UE实际诊断FAIL，session20261005-203243在AS bridge:187拒绝间接实例输入，尚无RT dispatch证明；新输入可能heap/存储/范围，下一补诊断后按实际字段开发。本库集中全量未运行。旧B488 C924集中
308/7784/3080 PASS、growth4653056bytes仅属于旧库。下一以UE实际新失败驱动开发。
ARC设备父对象提前释放、原用户Qt崩溃与GUI仍未验收；Qt旧模型回调独立修复已通过，
不能等同原crash。生产两项RT能力false；保留未提交修改，未提交/推送。
