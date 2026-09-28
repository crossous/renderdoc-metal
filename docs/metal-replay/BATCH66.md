# BATCH66：直接 patch tessellation

接通 `MTLRenderCommandEncoder` 的 `setTessellationFactorBuffer`、
`setTessellationFactorScale` 与直接 `drawPatches`，包括对应旧 chunk1153–1155。
剩余 **104 bridge / 68旧chunk**，Max1284不变。间接、indexed patch draw仍未接通。

T66在真实三角patch pipeline上使用half factor buffer、scale 1和一次直接patch绘制。
原生 Metal API Validation、注入捕获、回放API/CLI、patch draw事件与中心RGB均由终端
验证。11个畸形资源身份/偏移/scale/draw输入被明确拒绝；其中篡改factor buffer身份
最初暴露崩溃，已改为先验证资源ID再解析对象。完整命令：

```sh
bash util/buildscripts/scripts/test_metal_capture_batch66_macos.sh
```

Capture SHA-256 `29473c46466a71326a2bb6c90f9376503e7e5ad1f04747b944f0726f2c899f49`；
replay库 SHA-256 `d098a04a1a7b53d8917fa125ce5a70bcf0db2f1987f8cc8cc834b74299f3dea7`。
GUI未打开；T66并入集中人工QA。GPU生成的ICB indirect执行范围另列后续功能族，
不能由编码时CPU内存快照替代GPU执行时范围。

联合结果：**67份capture API/CLI、1883个畸形输入、670次lifecycle打开**通过，
resident growth 1376256 bytes；日志`/tmp/metal-batch66-host-final.log`。之后增强了T66
clear→draw→clear→draw回退断言，并在最终代码上再次跑T66定向API/CLI通过。
