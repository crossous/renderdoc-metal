# BATCH43–44：Compute/Render 资源声明、barrier 与 vertex 写 buffer

2026-09-26。实现和终端联合验证通过；GUI L4 延后，PHASE43/44 与本批保持开放。
没有 Computer Use、没有启动 qrenderdoc、没有提交；原有未提交工作和 UE 路线保留。

## 本批功能

- T42：七个 compute 入口（single/batch resource declaration、scope/resource barrier、
  push/signpost/pop marker）。追加 chunk1255–1261，Max1262，不重编号旧 capture。
- T43：五个 render 入口（staged single、普通/staged batch、scope/resource barrier）。
  复用旧 chunk1177/1178/1179/1186/1187。声明和资源 barrier 保留 frame references，
  共享校验只接受注册的 buffer/texture，拒绝非资源对象 ID、非法 usage/stage/scope。
- 修复失败的 render pipeline 创建仍包装空对象的问题，保持原生NULL/NSError语义；
  新 fixture 在 native/注入两路径都显式测试，失败创建不得进入capture资源表。
- 补齐 vertex RW buffer 的 storage分类、ReadWriteBuffer descriptor、VS_RWResource
  usage，以及 Pipeline Viewer 现有 VS Storage Buffers 表的数据入口；UI呈现仍未验。
- 剩余 bridge/chunk **176/114**（批前181/119，会话基线216/165）。本批12个新入口，
  七个compute方法此前是隐式forward、无旧标记，不能把标记降幅当成功能覆盖率。

## 终端 test batch

完整入口：`bash util/buildscripts/scripts/test_metal_capture_batch43_44_macos.sh`。
只重放现有 captures：
`RENDERDOC_METAL_LAST_TEST=43 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。

1. app/CLI/demos构建；两个模式各 Metal API Validation native5帧、注入capture8帧、
   XML参数/空数组/marker/chunk IDs/pipeline数量、3-loop CLI均通过。
2. T42三dispatch完整272-byte结果依次i+1、3i+10、3i+21；T43两次VS写入及一次FS读取，
   112-byte前三uint为20/40/60→27/47/67，其余padding0。均验证前进/回退seek、
   descriptor/usage、声明-only buffer/texture保留、最终像素和headless thumbnail。
3. 新 **103类**异常（T42 35、T43 68）无crash拒绝；包含空/未知/已知错误类型resource ID、
   混合数组每个位置、usage/scope/stage高位、unsupported stages、缺encoder。
   每个子命令30秒超时，信号退出不算正常拒绝。原239类保留，累计 **342类**。
4. 最终 **44 captures API/CLI**（T01–T43+T10_debug）、**440次lifecycle**通过；
   resident growth **2,899,968 bytes**，门限64MiB。无历史capture重录。
5. replay Metal验证层通过 T01/T02/T09/T12/T19/T40/T41/T42/T43 共 **9份**，覆盖共享
   display、旧资源声明、VS只读存储、新可写存储与两类barrier。已加入统一回归入口。
6. `bash -n`、新Python编译、`git diff --check`通过。旧T34识别不再匹配任意setVertexBytes；
   T21识别使用indexed-indirect调用特征而非52-byte buffer；保留原有功能断言。
   通用缩略图改用最后draw并检查全图有内容（T38最终中心合法黑色），专用中心像素断言
   不变。T43前两个raster-disabled draw允许没有彩色输出。

本轮分段完成最终整批：native/capture/XML/CLI见 `/tmp/metal-batch43-44.log`；该初次
整批遇到测试识别/缩略图假设误判，修复harness后完整联合入口通过，最终证据为
`/tmp/metal43-44-final.log`。上述修正仅影响测试，native/capture阶段无需再录制。

## 固定版本和边界

- 正式库与app内嵌库一致：
  `736925e6e19594ce2b0641c18a9f2019f65c7a78244ac3b600c9e619c9e0fee0`。
- qrenderdoc executable：
  `3cc9c3b63507006794f87e219921454aaf50469bebb05f72b10f5c4b98b1a4bc`。
- T42：`25c6e7349f22b599713869f3b60acc11f9eb635af60595464c7a0cdcaae87fba`。
- T43：`7139504e23575d191d30a01e2c29ca42dfe7560ade43b495cb1011ed358e5279`。

直接buffer/texture资源子集；Vertex/Fragment stages；compute scope Buffers/Textures，
render另含RenderTargets。不宣称heap/ICB/AS residency、Tile/Object/Mesh stage、完整
argument-buffer依赖图、通用UE兼容。当前fixtures验证buffer依赖链，尚未单独覆盖texture
写后读barrier、RenderTargets scope或所有GPU型号。scope支持不等于这些场景已验收。
marker只是structured API事件；声明不是copy/draw，不能生成假的GPU读写usage。

集中待验 **T34–T43 + T10 marker，共十一份capture**，见 QA_CONSOLIDATED。
旧九份项全部保留，新两份只加入差异检查，共用一个app进程；用户请求后再指导验收。
