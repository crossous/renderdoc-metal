# BATCH158：T36延迟store与旧textureBarrier回放修复

在06:26 kernel panic后改用每组约10份的Metal Validation定向回归，发现T36此前
隐蔽的两个验证层错误：回放创建render encoder时把捕获的`StoreActionUnknown`
提前改成`Store`，随后又执行应用原有的动态store setter；以及本机原生Metal
Validation明确拒绝旧`textureBarrier`。前者是回放时序缺陷，后者是当前设备对
旧API的限制。T36原生夹具在Validation下直接调用旧API也同样终止；尝试
`memoryBarrierWithScope`的全范围替换又被设备拒绝，不能把它当成通用等价实现。
Apple文档已将旧API标为deprecated并指向
[memoryBarrierWithScope](https://developer.apple.com/documentation/metal/mtlrendercommandencoder/texturebarrier%28%29?language=objc)。

修复后，带纹理的`Unknown`保持到真正的encoder setter；只有事件回放提前截断、
未到setter时才在结束encoder前补`Store`。旧textureBarrier仅在同一render pass
尚未执行任何draw/dispatch/ICB操作时跳过：此时没有同pass先前纹理写入需要排序；
已有GPU工作则明确拒绝，绝不无条件略过同步。新增T36“把barrier移到draw后”
畸形capture，必须以目标chunk失败而不是Metal Validation中止。

当前库下T36 API/CLI、begin→end事件回看与15个畸形输入通过；另26份跨资源、
mesh、同步、AS、intersection table的定向哨兵通过。此前分组运行过其余旧捕获，
T35/T36/T43/T148/T156/T157共60次生命周期打开通过，resident growth
901,120 bytes。
但不同组的库版本并非全部为当前最终库，且完整3430负例/1540次生命周期回归
尚未重跑。06:26 panic的唯一根因仍未证实；不做UI/Computer Use。

终端定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh t36 --sentinel t43 t71 t74 t78 t91 t101 t120 t145 t148 t153 t156 t157
MTL_DEBUG_LAYER=1 python3 util/test/metal/metal_render_dynamic_state_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t36_capture.rdc
```
