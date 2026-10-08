# B490：Tracked placement heap 的间接实例输入

backend/bundle SHA256：0495cfde70452f4ccf28bcb34ad98d166e37920c365f569c7d7de35d262ce582。
GUI仍3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。

## 触发与对照

B489349df416 UE FAIL之后，诊断库4e35382c在session20261005-203727记录实际
Private/Tracked/heap/isAliasable=1/background、offset0/stride72/count55/length589824。
在先前guard处拒绝，尚无RT dispatch/可用离线帧。有限时启动期间曾等待Metal管线
编译，process-sample.txt保留；不是光追成功证据。

继续按Vulkan CopyInputBuffers/DX12 CopyBuildInputs在AS执行点复制，而非用最终
buffer猜输入。Metal placement资源从创建起即可报告isAliasable，不能把它等同于
显式退休。仅kind9帧前输入允许Private、Tracked placement heap且整个heap也Tracked，
仍拒绝显式m_CapturedAliasable退休、untracked/automatic heap和CPU-backed输入。
[Apple WWDC21 heap依赖跟踪](https://developer.apple.com/videos/play/wwdc2021/10286/)
说明tracked heap上的读写访问会按heap建立依赖；本批真实distinct alias覆盖验证
其隐藏AS后blit输入冻结结果。snapshot校验额外拒绝scratch/query和AS输出同heap
范围与输入重叠。未采用额外native event或改变应用提交顺序。

初态恢复旧总guard仍拒绝heap，已仅对kind9 Private placement来源放行；构建提交
使用自己的冻结packed68 staging与live子AS，绝不读取当前heap作为重建输入。
沿用schema7/原chunk/typed标识/子版本及EID0，不改变其他heap AS输入支持范围。
仍限1–4个查询过的primitive候选/此前提交子AS/64MiB，不含帧内Private/同CB子AS/
motion/refit/嵌套TLAS/跨队列证明。

## 已运行验证

入口util/buildscripts/scripts/test_metal_ray_placement_instances_macos.sh，七例包含
单实例、同CB上传、同CB同buffer擦零、同CB distinct native alias擦零、后来CB
alias擦零、两子AS三实例/offset64/stride80/高位userID与compact/copy。
native/capture实际ray0/73/0/73或完整4026531913；API 34×7×3=714事件/EID0/
CLI各3loops PASS。distinct alias场景资源读回全零，AS编号不变，证明输入冻结。
7×46=322损坏初态拒绝、20旧API/CLI、T12416反例PASS；8×10生命周期PASS，
resident growth311296bytes，起止hash一致。产物captures/metal-ray-b490、
metal-ray-b490/indirect-manifest.json。

首fixture heap不足4KiB被既有重放拒绝；对齐测试allocation后继续失败于初态heap
总guard，修复后正例通过。alias名称还触发旧fixture BLAS NoCopy别名反例，导致
没有子AS/TLAS初态；修正alias模式只适用于旧非indirect场景后distinct alias通过。
这些原始FAIL/log/manifest及alias-failure.zip.xml保留，未计PASS。额外event候选
只构建未GPU验证且已撤回，不宣称其为同步缺陷修复。

## 接续与未验

本库官方两scene/10查询/41坏sample/6能力查询与53份B482–490 RT6021事件PASS。UE session20261005-210821越过heap guard，随后FAIL于AS bridge:213的1–4候选检查，尚无RT dispatch证明。固定库集中308捕获/7784坏输入/3080生命周期PASS，resident growth13074432bytes、exit0、工作及冻结库起止hash一致；证据frozen-validation-0495cfde/full-regression.log与followup-manifest.json。B491仅源码编辑，等本集中结束后才构建，不归于本证据。旧C924集中308/7784/3080
PASS/growth4653056bytes属于旧库。B489独立Private八例/816事件/368坏/90生命周期
及官方PASS为349df416旧库证据，待当前库旧RT复验。下一按UE实际新失败开发。
ARC/原用户Qt crash/GUI仍未验；不启用两项生产能力，保留修改、未提交/推送。
