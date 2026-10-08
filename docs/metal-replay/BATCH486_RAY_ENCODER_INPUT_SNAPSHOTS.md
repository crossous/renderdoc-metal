# B486：同提交 GPU 几何输入的执行点快照

backend/bundle SHA256：a2e878313ec4f55f427d8ef72d1e337bcf89fcef38084584b6e651e11ac14265。
GUI binary仍为3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。

## 输入缺口与边界

UE上波已越过VFT容量，卡在Private/offset/Indirect实例描述。先建立原生间接实例
基线：Shared、Private、同CB上传、同CB上传后GPU覆盖四例实际ray query输出
0/73/0/73，保留userID与AS GPU标识。80c2d706捕获探针SIGTRAP(-5)，不能记为
干净unsupported；离线未跑。证据在metal-ray-b486-pre-alias-audit/baseline-manifest.json。
本批没有放开间接实例guard，也没有重复启动同一失败的UE。

对照Vulkan vk_cmd_funcs.cpp Serialise_vkCmdBuildAccelerationStructuresKHR及
VulkanAccelerationStructureManager::CopyInputBuffers，DX12
BuildRaytracingAccelerationStructure后的D3D12RTManager::CopyBuildInputs：
在AS使用输入的位置复制构建数据，而非用提交结束后的最终缓冲内容重建。

Metal的encoder不能内嵌blit。本批在AS encoder原生endEncoding后立即附加隐藏blit，
按encoder关联kind8多indexed geometry输入；之后应用encoder可继续覆盖输入。
只允许独立、非heap、非aliasable、Tracked的Private VB/IB，各/累计唯一快照
上限64MiB，并拒绝本AS encoder的scratch/compacted-size输出与输入同一对象。
Shared/Managed/NoCopy、untracked、heap、refit/TLAS未扩展，维持原有提交级证明。
即使同encoder不写Shared，后续NoCopy别名也可能绕过按对象的自动依赖跟踪；
因此初版09ed122f的更宽候选不作为最终支持证据，日志独立保留。

快照在GPU提交完成且无error后物化，延续schema5/kind8输入与索引范围检查；
提交处理不以晚期快照替换执行点副本。不改变chunk枚举、初态格式或公开能力。
此范围不能证明任意跨queue或未同步写入正确，应用仍需遵守Metal同步要求。

## 已运行验证

入口util/buildscripts/scripts/test_metal_ray_input_snapshot_macos.sh。
四例background-multi-indexed-private-same-cb及u32、gpu-mutated组合：同CB先blit
上传Private VB/IB，再构建两段geometry，随后在同CB把VB/IB清零。
native/capture实际ray0/1/0/1、重放仍命中1且资源字节已零、408事件三方向/EID0、
CLI各3 loops、4×30=120坏输入PASS。20旧定向API/CLI检查、T12416坏输入PASS；
5 captures×10生命周期PASS，resident growth409600bytes，库起止hash一致。

新增Shared同提交上传/覆盖边界：native/capture实际ray正确；离线CLI exit1并明确
拒绝setAccelerationStructure缺失初态，不以最终零字节错误重建AS。
该expected refusal是边界PASS，不代表Shared同CB功能支持。
结果metal-ray-b486/snapshot-manifest.json，captured inputs在captures/metal-ray-b486。

官方Apple两个scene再次native/capture/offline逐字节输出、44/46事件三方向及CLI×3
PASS，10尺寸查询与native一致；gate-results为最终a2e87831库。
41坏sample输入、6能力查询、24份B482/B484/B485/B486旧/新RT共3027事件选择
复验PASS；固定库集中全量308捕获/7784坏输入/3080生命周期打开PASS，growth0bytes、
exit0、工作/冻结库起止hash一致。结果followup-manifest.json和
frozen-validation-a2e87831/full-regression.log。该验证期间只编辑了后续B487源码，
未构建新库；B487待验代码不归入此a2e87831证据。

## 未验收与接续

UE间接实例构建/实际RT dispatch/离线、ARC提前释放、原用户Qt crash与本批GUI未验。
Qt独立header模型生命周期修复已有CPU反证/修复PASS，不等于原crash复现。
下一步先保留实例userID语义，再做typed AS GPU ID→ResourceId→live ID重定位和GPU输入。
生产supportsRaytracing/FromRender保持false；未提交/推送，持续推进。
