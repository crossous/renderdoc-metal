# BATCH61：no-copy Buffer创建

承接BATCH60的129 bridge / 77旧chunk。接通`newBufferWithBytesNoCopy`的bridge和旧
chunk1006，剩余**128/76**，Max1278不变。抓取时真实Metal仍使用应用的页对齐指针及其
deallocator；创建chunk保存初始4096字节，Shared buffer继续参与CPU提交更新跟踪。离线
回放拥有独立数据副本，不调用原应用deallocator。当前离线验证范围严格为Shared默认
options、1–64MiB、payload长度一致且新ResourceId；不接受其它options/存储模式。

T61 fixture确认`contents()==原指针`，原生模式4帧、三提交的RGB`10/20/30`、
`40/50/60`、`70/80/90`以及原生deallocator在释放后调用一次。抓取4帧及CLI回放
通过。API按第三→第一→第二→第一→第三draw核对shader、offset256、4096字节payload、
4080字节哨兵/padding、像素和GPU/CPU回退。13个身份、长度及options畸形样本明确拒绝；
曾发现任意高位options会让Metal内部断言，现已用精确白名单挡在Native调用前。

重要边界：捕获资源记录会延长真实buffer的生存期；fixture观察到抓取模式中，应用释放
wrapper当下deallocator计数仍为0。回调最终时机/无界应用的page-pool压力未作为本批通过
条件，不能宣称与未注入原生程序完全等价；后续真实应用QA应观察此项。没有提前调用
应用回调或释放其指针，避免GPU/记录仍持有buffer时发生UAF。

一键`bash util/buildscripts/scripts/test_metal_capture_batch61_macos.sh`通过；
`/tmp/metal-batch61-final.log`记录**62份capture API/CLI、1772异常样本、620次
lifecycle打开**，resident growth1327104bytes。T61 SHA-256
`adad0c224323d32ab70947466669cbdc267b03b898bbe4d736d11fbf316fb063`，库/app
`0f7f42e63b340674c23f1a443fba0f8163d97879a676ce1ad49b53b2af3ca431`；GUI executable
仍`fe8bcf852b683bc463a3be883e6c54208b7bd45054a24f5dbace58c912346d74`。未运行GUI/
Computer Use、未提交/推送；新增UI待验T61，总单现29份。
