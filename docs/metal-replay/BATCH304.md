# BATCH304：异步单节点 Stitched Library

在 T303 的受限 graph 子集上接通
`newLibraryWithStitchedDescriptor:completionHandler:`。异步调用重建不含
wrapped 指针的 native descriptor；completion 中包装借用的 library 返回值，
保持源函数引用直至回调，序列化到追加 chunk 1377（Max 1378）。
同步/异步共用 descriptor 子集验证，复杂 graph 仍明确拒绝。

原生 Metal Validation、注入截帧、API逐事件读回与CLI回放均通过。
`t304_capture.rdc` SHA-256 `3602ebde29d3…`，graph `stitched_scale`
对应32个float `2,4,…,64` 和中心像素约 `(0.2,0.7,0.3)`。
T303/T304 各17例畸形截帧被拒绝；跨族12份捕获的 API+CLI
定向回归通过。T35+T303+T304 各10次生命周期打开通过，resident
增长475136字节。长时累计压力回归、GUI/Computer Use均未运行。

最终库与app内嵌库 SHA-256均为 `a4547b2ebb75…`；GUI executable只构建
未启动，集中待验累计266份。bridge/chunk 原始防御宏匹配63/15：
这两个原始数字不会因为新增的异步正向支持再下降，因异步错误 graph
仍保留一个显式守卫。06:26旧IOGPU panic未归因，本批无新panic。
