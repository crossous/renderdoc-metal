# B419：sourced visibility结果缓冲区

实际UE17709 depth/stencil pass所有纹理/尺寸/初始内容/counter都合法；拒绝项是visibilityResultBuffer8606。8606为BG heap22 Shared Tracked placement buffer，262144B，完整initial contents884；7个Counting offsets128..176。不是depth格式或resolve filter问题。

候选65允许有完整BG initial contents的Shared Tracked visibility buffer（8B..1MiB），验证object/type/device/retirement并跟随pass/parallel child。mode<=Counting，8B对齐，enabled mode需要8B结果范围。写资源纳入noteResource/modifiedBuffers/opaqueWrites/renderIndirectWrites，复用现有Native setVisibilityResultMode；没有新的query模拟或同步。Private/frame-born结果buffer仍未覆盖。

c30b04d2/metal-visibility.JgmODr：6captures/24reset-seeks/108indirect+24fresh+24mixed+51graphics+30visibility API+CLI反例组通过。Native Counting offset8结果4；末尾Disabled offset0导致Native清零word0，16/24哨兵不变。原first draw EID还未执行Disabled，partial seek word0仍0x1234；后续resolve EID/fullframe word0为0，reset EID0恢复0x1234及count0。原Native与replay的不同事件范围均严格验证，未改Native操作；最初helper把完整frame word0期望错误应用到partial draw，诊断后修正测试期望。Shared/Private间接参数/parallel三路径通过。

真实UE越过visibility，推进至帧内R8颜色目标17468 Load pass。无完整UE GPU上传/提交；UI Mac锁定，未提交或推送。
