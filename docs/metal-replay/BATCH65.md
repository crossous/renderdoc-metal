# BATCH65：SharedEvent 初值/GPU同步与 T63 布局校验

承接 BATCH63–64 的 108 bridge / 72 旧 chunk。本批接通
`MTLDevice newSharedEvent` bridge 与旧 chunk1033，复用已有 command-buffer GPU
`encodeSignalEvent` / `encodeWaitForEvent`，剩余 **107 bridge / 71 旧 chunk**。
SharedEvent 保留正确的 Objective-C 协议身份；每次 replay epoch 都重新创建原生
SharedEvent，不继承后来事件选择留下的信号值。原生对象的协议反射在不同回放进程
不稳定，最终按实际 `signaledValue` selector 识别，T52/T65定向与T65十进程反复通过。

T65 用一个 SharedEvent、两条 queue、三个 command buffer 完成 CPU 初值100→
首次GPU wait100→填充/signal→wait/draw/signal→wait/再次填充/绘制。初值仅支持
**该Event首次GPU使用之前**的单调赋值；会记录初值chunk1283并在每个replay epoch
恢复。原生 Metal 验证、注入 capture、两draw资源/像素及前后seek均有终端断言。
GPU使用后的`setSignaledValue`及`newSharedEventHandle`导出在抓取时仍让应用正常运行，
但会记录诊断chunk1282，离线回放明确失败。跨进程handle导入仍未接通；listener
回调不在离线重演保证范围内。`MetalChunk::Max`为1284。

同波补强 T63：有当前 render pipeline 时，四种动态 stride 调用在 native 回放前
核对绑定槽位的布局类型，拒绝动态槽位的静态哨兵与静态槽位的动态 stride；
新增 5 个畸形输入，T63 累计 48 个。T65 另有 12 个畸形身份/信号值输入。

一键命令：

```sh
bash util/buildscripts/scripts/test_metal_capture_batch65_macos.sh
```

脚本还会实录 GPU使用后 CPU 修改与 handle 导出两个合法但不支持的负例，核对明确失败
诊断，随后集中跑全部 capture API/CLI、畸形输入和 lifecycle。最终代码上 **66份
capture、1872个畸形输入、660次 lifecycle 打开**通过，resident growth 2162688 bytes；
日志 `/tmp/metal-batch65-host-final.log`。T65 capture SHA-256 为
`1b78708a1d3ed795d55f1ba8860250ff418506c5ce50de13415d747372ccaea6`；库与 app 内库
`d69fa7da1f8ddb0bdace702e867be31c9553b2aa02dfb106b1d252c22718d3f6`，GUI executable
`fe8bcf852b683bc463a3be883e6c54208b7bd45054a24f5dbace58c912346d74`（未改）。
GUI/Computer Use 未运行；T65 已并入集中人工 QA，共 T34–T65 + T10 marker **33份**；
未提交/推送。
