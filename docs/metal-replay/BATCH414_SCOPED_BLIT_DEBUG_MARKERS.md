# B414：blit encoder的调试标记与隐式结束

真实UE encoder17661记录DistanceFields/UpdateDistanceFieldAtlas两层push后copy，再endEncoding，无显式pop。原Metal录制及既有UI m_DebugGroupPaths都将组限制在encoder内，encoder结束即结束这些组；preflight不能把合法的implicit close拒绝。

候选65支持blit push/pop/signpost，仅允许live encoder，深度<=64，显式pop不得下溢；end清除所属encoder的组。部分blit seek dependency保留原marker chunk，Native copy/dispatch/PSO不改。

7a73a0cd/DZPPYb显式pop6captures/24seek已通过；6e963f9a/woYJ4q最终隐式close6captures/24seek/108indirect+24fresh+24mixed+15marker API+CLI反例组通过。API动作树精确包含两层group和signpost，GPU采样/MRT/ordinary数据、zero-argument dispatch/parallel/Private保持正确。marker反例为旧coverage64/深度溢出/错误encoder/pop下溢/encoder结束后使用，GPU前拒绝。

真实UE pre-submit65继续到useHeaps stream273408；未提交UE GPU。6e963f9a精确库组合全量已启动（cpu-slots-combined-regression），期间只允许静态分析和对象编译，不链接该运行库。诊断仍需核对useHeaps失败的精确字段，不能推测tracked/identity哪项失败。

精确组合全量完成：6e963f9a库前后哈希一致，308captures/7786malformed/3080 lifecycle opens通过，resident growth13,615,104B。日志cpu-slots-combined-regression.log。
