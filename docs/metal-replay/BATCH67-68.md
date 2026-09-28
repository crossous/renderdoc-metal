# BATCH67–68：patch draw 重载与同进程 shared-texture handle

承接 BATCH66 的104 bridge / 68旧chunk。本批接通三个 patch draw 重载：直接indexed、
indirect、indexed-indirect；以及 `MTLTexture::newSharedTextureHandle` 和
`MTLDevice::newSharedTextureWithHandle` 同进程导出/导入。剩余 **99 bridge / 63旧chunk**，
Max1284不变。

T67 的三次真实绘制分别为红/绿/蓝，包含3个draw事件、资源绑定与前后seek。
间接patch参数可能由GPU在编码后生成，原生命令可执行，但当前 action 的数量字段
保留0表示未知，**不从编码时CPU缓冲内容推断**；T67夹具仅验证CPU初始化参数，
不宣称GPU写后参数的事件字段已可解码。13个畸形变体均被明确拒绝。

T68从descriptor-backed Private shared texture导出原生handle，同进程导入为另一个
纹理资源。两次GPU clear源纹理后，Fragment通过导入纹理采样，画面和seek证明共享
同一底层内容。捕获时把原生handle关联到源wrapper；回放以源纹理重新导出并导入。
关联不会跨进程序列化，跨进程传入或重建后失去关联的handle会录成Source=0，离线
回放明确拒绝。7个畸形身份输入被拒绝。

验证命令：

```sh
bash util/buildscripts/scripts/test_metal_capture_batch68_macos.sh
```

T67 capture SHA-256 `93f02f45b77b83355969bc157e69e8d299b751ab2633322f5cb23ed70453834f`；
T68 capture SHA-256 `05ea59ff1f1f7cb6b6208399cb574869d8d99a6ef1274ec4e692ebb6a3017fe9`。
最终库及app内嵌库 SHA-256 `f8b8b15d4588177fd1a7e3cf916100931aaea4ec5053b0cae662234d99d36349`；
GUI executable未改，未运行UI/Computer Use。T67/T68加入集中人工QA。

最终联合回归：**69份capture API/CLI、1903个畸形输入、690次lifecycle打开**通过，
resident growth 786432 bytes；日志`/tmp/metal-batch68-host-final.log`。`git diff --check`
和批次脚本语法检查通过。没有提交或推送。

ICB GPU indirect执行范围仍未接通，下一开发波次优先；完成条件和技术边界见
[PLAN.md](PLAN.md)“待接通”节。
