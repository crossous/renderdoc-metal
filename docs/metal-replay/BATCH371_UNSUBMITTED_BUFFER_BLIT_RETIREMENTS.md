# B371：未提交buffer blit编码期间的明确退役

b2b56a6c…真实UE冻结2030slot，其中121个失效texture来源在首个提交31677前按generation
释放：75在CB出生31604前，46穿插于buffer blit编码31609..31617及其后至31661。
五条buffer->buffer copy尺寸4800/3200/3200/1600/2400，共15200B，均为已知frame buffer。
首次shader工作31810；没有在退役前提交GPU。v29的64条/CB出生边界不能覆盖该合法序列。

对照Vulkan未使用stale descriptor机制与D3D12/Vulkan buffer copy，v30仅进一步接通
有界、未提交、非descriptor backing的frame buffer复制。完整预扫描限定最多8个CB/8个
blit encoder/16个copy/64KiB总复制量/256个退役，所有copy必须已知frame-born src/dst、
独立对象、有效range；任何shader encoder、提交、texture copy或未列明操作终止前导。

预扫描保存精确src/dst/offset/size和encoder证明，禁止复制descriptor backing；运行和
第二阶段preflight仅放行这些tuple，检查frame live/alias，登记GPU写入及提交依赖。
仍校验旧slot type/generation/initial bytes，无GPU地址猜测，旧v16..v29规则保留。
不是以“buffer blit没shader”放开任意后续复制、已提交工作或失效资源引用。

首轮cb413c2d…被旧第二阶段blit coverage==5拒绝，无GPU执行，随后同时接通有界普通复制。
精确978f9a40665e7ab449d9b47545b70de27fea9342e456a389483096273c1d7c7f，
metal-retirement.lN443R四捕获/16seek、原生16B copy、每seek复制byte1..16、GPU122/186累加
308、像素186/122、EID0失效VA/texture清零、94 API+CLI反例通过。
80个退役超过旧64上限的metal-retirement.GiOfTN夹具通过四捕获/16seek/112反例，
新增range/source/descriptor backing/encoder/order拒绝检查均在GPU等待前失败。
实际UE更精细CPU对照发现31573的buffer texture birth会终止v30前导，之后还有
31598 heap texture birth；因此尚不能声称本规则完整覆盖121条，继续v31创建适配。

精确978f9a40…库全量308/7786/3080通过，resident growth12058624B；
日志blit-retirement-combined-regression.log，运行期间库hash未变化。
实际UE无完整descriptor coverage，尚未GPU replay，人工UI未验收；继续全资源/间接参数/
view/格式/预算范围适配。持续目标active，不提交/推送。
