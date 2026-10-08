# B470：AS fence 同步与基础光追回放验收

2026-10-05。接续 [B469](BATCH469_RAY_MARKERS_AND_TABLE_CLEAR.md)，实施准则与
未支持范围见 [PHASE54](PHASE54.md)。保留工作区修改，无提交/推送。

## 对照与实现

Vulkan `Serialise_vkCmdPipelineBarrier`、DX12 `Serialise_ResourceBarrier` 均保存
并重放 GPU 同步，使 producer/build/consumer 的结果正确。MTLFence 的编码器
依赖没有逐字段相同的API；Metal复用自身已有 compute/blit/render fence机制，
不创造单独AS同步系统。

新增 AS encoder `updateFence` / `waitForFence` 的 ObjC bridge、wrapper、
序列化和回放分发，末尾追加chunk1418/1419，Max1420。沿用
`ValidMetalFence`、`Updated(epoch, encoder)` 和 `CanWait(epoch, encoder)`：
类型/native对象/当前encoder须有效，wait须有本轮已发生的其它encoder更新。
旧回放epoch不能使缺失更新合法；同encoder自等待在native之前拒绝。按既有
command buffer record引用fence，不改变一般资源Usage分类或新增viewer。

## 最终候选与结果

backend与bundle均为
`ae2a28953d01b1cf7f514eb71a9d2788eac7ecab96788d20e617c9f394435b5e`。
一键入口：`bash util/buildscripts/scripts/test_metal_ray_basics_macos.sh`。
可用 `RENDERDOC_METAL_RESULT_DIR` / `RENDERDOC_METAL_CAPTURE_DIR` 指定本批产物目录；
既有定向帧固定从 `captures/metal-smoke` 读取，避免把新帧目录传给旧帧测试。
最终记录：`build-macos-debug/metal-ray-b470/`，gate入口日志
`build-macos-debug/metal-ray-b470-gate.log`。

| 检查 | 结果 |
| --- | --- |
| native / capture，基础与fence两种帧 | Metal API Validation PASS；GPU ray均0/1/0/1，尺寸1280 |
| GPU producer → AS build | blit写入Shared/untracked几何，更新upload fence；AS等待后build；build事件读回9个float逐字节正确 |
| AS build → compute consumer | AS更新built fence，下一CB四个compute encoder等待后执行查询，PASS |
| marker与API seek | 3个group/2个signpost归属正确、无假continuation；54/65事件各3方向，共357次选择，GPU前缀/尺寸/AS绑定PASS |
| 新异常输入 | 基础19 + fence36 = 55；错误ID/type/已结束encoder/缺update/同encoderwait均拒绝，无信号退出或hang |
| 既有异常输入 | visible19、renderIFT20、computeIFT18 = 57；含未知非空handle拒绝，PASS |
| 既有API与CLI回放 | 30份旧帧PASS；连同两份新正例共32份，非全量 |
| CLI循环 | 两份新帧各3轮PASS |
| 生命周期 | T35/基础/fence/T300各10次，共40次；resident增长720,896 bytes，PASS |
| 构建与一致性 | renderdoccmd/app构建PASS；backend/bundle hash一致；diff whitespace检查PASS |
| GUI | 未验；不能把terminal/API结果视为UI验收 |

两份新帧SHA256：

- `captures/metal-ray-b470/table_capture.rdc`：
  `c17d3934fbe5a200425fa7add5c62cec851d6042f156308c79f6dd19962fe3ea`
- `captures/metal-ray-b470/fences_capture.rdc`：
  `855ac2f59ef88f1f4c3fc486deb1983ff8db6975d7fca54771f88cd819033840`

初轮B470 gate因新捕获目录环境变量传给旧帧脚本而找不到T01，不计PASS；
修正目录传递后完整gate通过。之后增加background边界模式，单独构建fixture并
验证native/capture/预期拒绝，入口也已纳入该模式；这不改变最终后端hash。
未跑全量308帧/累计负例/3080生命周期，也未重测真实UE本批图像。

## 实证剩余缺口：帧前已构建AS

新增fixture `background`模式，在StartFrameCapture之前完成AS build，再在帧内
做同样四次query。native与注入运行均GPU结果0/1/0/1；捕获成功。当前离线
明确拒绝 `MTLComputeCommandEncoder::setAccelerationStructure`，因为帧流缺少
AS重建初态，不能静默绑定未构建AS。预期拒绝检查PASS，无信号/超时。
捕获 `captures/metal-ray-b470/background_capture.rdc`；日志
`background-native.log`、`background-capture.log`、`background-reject.log`。

这是通用应用光追的实际阻塞，不是应该靠更多descriptor参数排列解决的问题。
Vulkan/DX12已有AS初态保存/重建模型，后续应先补这一链及执行期实例数据。
当前基础受控执行链已可用，但两项设备光追能力仍false，不宣称任意工程光追
可捕获回放。最终真实验收需明确启用光追的工程/帧；现有低配置UE帧不能替代。
