# BATCH163：函数表显式资源驻留与畸形encoder拒绝

T161原生嵌套visible table可不调用`useResource`执行，但真实应用可能明确声明
函数表驻留。T163在render pass内以`useResource(..., Read, Fragment)`声明嵌套表，
再经IFT交点函数调用它。回放资源校验现支持已登记、同设备的visible/intersection
function table；buffer/texture原路径保留，barrier仍只允许原来的buffer/texture
子集。正例在原生Metal Validation、注入捕获、API/CLI回放均得到Shared值1、中央
绿色和正确事件seek。

畸形输入测试还发现原有render `useResource`回放未验证encoder的类型/当前身份，
把encoder ID换成AS可触发Objective‑C异常退出。已对render的单个/批量、带/不带
stage四种residency入口，以及compute的单个/批量入口补同样的类型、当前encoder
和真实对象校验。T163的8个residency畸形输入和6个嵌套表畸形输入均干净拒绝，
其中错误encoder不再信号退出。17份旧/新跨族哨兵API/CLI通过；T35/T161/T163
共30次生命周期打开通过，resident growth 622,592 bytes。

本批没有减少bridge/旧chunk标记（仍55/18，按原始宏匹配口径），也没有重跑06:26 kernel panic后的
全量GPU压力回归或GUI/Computer Use。库/app内嵌SHA `917ed5804329…`，T163
capture SHA `b0c5269d639b…`；集中UI待验125份。`supportsRaytracing`仍false。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t148 t156 t159 t160 t161 t162 t163
python3 util/test/metal/metal_function_table_residency_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t163_capture.rdc
```

完整累计回归入口已纳入T163，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=163 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
