# B378：Managed/background drawable 初始状态

已确认实际b2b56a6c…唯一未覆盖来源17291为背景900×640 BGRA drawable，storageMode Managed。
slot退役34896晚于首次dispatch31810，不能忽略。WrapDrawableTexture缺dirty标记，
InitialTextureLayout排除Managed；补dirty、GPU→Shared staging快照/Shared→GPU恢复，
不从Managed CPU副本读取。普通Managed纹理也沿用同一GPU snapshot路径。

v37预算支持Managed背景初始数据及drawable逻辑字节；typed background texture source
要求完整initial，无数据即preflight拒绝。当前frame drawable仍由普通replay image代替，
不因是输出target而强制有初始数据；不改变整帧coverage/未来资源限制。
Apple官方storage-mode文档明确Managed CPU/GPU副本的显式同步要求；本路径GPU→GPU无需
假设CPU副本同步。参考 https://developer.apple.com/documentation/metal/setting-resource-storage-modes
本机Native CAMetalLayer1×1 storage=1，Standalone replay Managed纹理创建/读回均经验证。

d5wR5V Native122已过，捕获因为两个layer选择了背景层，EndFrameCapture无backbuffer而失败；
用例在capture前注册presentation layer并显式StartFrameCapture(nullptr,layer)/EndFrameCapture(nullptr,layer)。
metal-private-texture.6PpjIv两捕获/八回跳、Native及Replay descriptor shader122/DEADBEEF、
Managed1×1初始像素4B/覆盖恢复和Private3×5多mip像素通过；30组API+CLI反例，
含legacy-v36/missing/duplicate/short/long/wrongID/offset/format均在GPU upload/wait前拒绝。
此极小用例保留未呈现的背景drawable，不宣称完整UE或用户UI已通过。
精确7ef3fa13aba543587906b51edf15eea93d0b75862830d2761d153317f59bc6bc。

全量最近精确e3c11635…308/7786/3080、growth0B通过；组合新库全量与实际UE重截/CPU审计接续，
目标active，无提交/推送。

实际UE再捕获已完成：session20261002-020710，owned editor93893正常退出0。
新捕获bfa3f117afe96c98050947fd0b842910ade790a77125dedd45869648fe06cf74，
103283591B，库精确7ef3fa13…，另存UE58_managed_initial_bfa3f117.rdc；旧b2b56a6c帧保留。
CPU zip.xml审计：444 texture initial/645511206B，packed layout issues0；
850 effective frozen unique texture sources=489 texture initial+361 parent buffer initial，
没有missing source。900×640 Managed drawable15993/16030均有2304000B完整初始内容。
另外当前帧新drawable等无initial不能被混同背景来源；仍由完整typed来源检查决定合法性。
生命周期0错误、inline0错误、62 GPU producer值全部匹配；frame-start1815 live slots
全部initial匹配。value-epoch扫描4560条背景历史来源identity未保留记录，frame_value_epoch_issues为0；
历史不能当成当前帧错误，也不以冻结帧初匹配代替每次写入epoch的检查。
query-only Native1697 placement范围0错误，67历史重叠；backing复用15469→16597
于39318，旧descriptor retired、已提交且无后续引用。GPU replay submitted0。
现行CPU texture birth前导候选12 exact-generation frees、15200B五条普通buffer copy，proof issues0。
真实首个frame heap texture29909是192×104 RG11B10Float、Private tracked、usage3，
仍超v37 tiny frame texture创建规则；继续定向适配，不直接给实际UE宣告coverage。

精确7ef3fa13…组合全量本轮已完成：308 captures、7786 malformed cases、3080 lifecycle
opens全部通过，resident growth9256960B；日志managed-source-family-combined-regression.log。
运行期间只编辑v38候选源文件，未重编译或替换该dylib；结束后才启动新定向构建。
人工UI锁屏未验收，真实UE GPU replay尚未提交，持续适配active。
