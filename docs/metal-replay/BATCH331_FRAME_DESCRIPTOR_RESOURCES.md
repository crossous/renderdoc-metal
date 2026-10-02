# BATCH331：帧内 placement buffer/view 的显式重定位

2026-10-01，本地 M2 Pro / 16 GiB。HEAD c4be68bb7fce662e4dd8498408981fa2e824c3b6，
保留全部工作树改动，未提交/推送。UE 原始保护备份 7cd838c7… 未修改。

## 实现范围

coverage v3 在既有 v2 CPU/GPU 来源契约上增加帧内新建的非重叠 placement
buffer 和支持的 buffer-backed texture view。这里只支持已明确声明布局的小帧；
UE 插件仍没有 coverage 声明。

- 扫描记录 frame creation 后才接受新 kind0 buffer VA 或 kind1 texture ID。
  重复 pre-frame identity 仍必须同 kind/value；其它新对象及 conflicting ID 拒绝。
- whole-frame preflight 以 CPU 元数据维护按事件出生的 buffer/view 候选集。
  不为未来资源创建 native 对象，不为未来字段编码 GPU 命令。当前时点之前的
  指针/ID 必须有唯一对象；future IDs 不能出现在帧首或提前的 table CPU update。
- placement 检查 parent heap、storage、长度（每个不超过128 MiB）、设备
  size/alignment、堆范围和与帧首/先前帧内 allocation 的重叠。Shared/Private
  及各自 Tracked 选项均有正例；alias reuse、purgeable、其它 births 保守拒绝。
- view 的格式、维度、storage/options/hazard/usage、offset、row、range、
  native alignment 检查提取为 ValidateMetalBufferTexture，由 CPU preflight 和
  实际创建共用。仅沿用已有支持的 Private TextureBuffer / Shared RGBA8/BGRA8
  2D 范围，没有扩大不支持的 view 描述符。
- 沿用已有 replay epoch 与 resource wrapper：每次回跳先 FinishReplayCommands，
  释放 view native，再释放 parent placement buffer native，清 frame heap ranges；
  按事件重建并 ReplaceRealResource，typed 字段获取重建后的 VA/ID。
  未删除/绕过任何 GPU 完成等待。

## 定向终端结果

最终库与 app 内置库 SHA256 相同：
`5fab70bfcc92074dac78f0f5b79d6f1cbfa12ea1d12bc88a7e7b95fa5f23c273`。
构建 renderdoc、renderdoccmd、build-qrenderdoc 成功。

脚本：`util/buildscripts/scripts/test_metal_descriptor_relocation_macos.sh`。
最终日志 `build-macos-debug/metal-descriptors.MExCZK`，摘要
`local-m2-descriptor-replay/late-identity/suite-view-final.log`。

五种极小用例 table / GPU update / Shared frame placement / Private frame placement /
Private frame buffer texture view，均原生 Metal Validation 通过、各两份真实 capture
通过 API replay GPU 字节/像素和事件 seek。帧内资源用例每份验证12次双向seek、
3次 EID0 reset，root pointer 恢复零、源随后重建、输出恢复正确。

frame placement 两种存储各输出41/80，指针含+4/+8偏移，非描述符常量与
DEADBEEF哨兵不变。新 view 用例64 KiB heap、两份512 byte Private buffer、
R32Uint TextureBuffer，VA+256与texture ID同时读取，bias7，输出89/167。
另一个 replay 进程先分配padding buffer/texture，逐次核对 replay VA和texture ID
不等于真实捕获值；常量与帧首两个零字段恢复正确。保存BGRA像素128/64/32/255。

48个负例分别执行API/CLI：原有10个table、5个GPU provenance、Shared/Private
各10个placement、新增13个view。包含 query/view/write 提前、未知parent、非法
range/row/alignment/format/storage、resource ID冲突及晚期非法texture ID。
各预期失败日志没有 frame GPU wait 或 Private initial upload。此断言针对这些
夹具，不代表任意捕获在初始资源阶段都不可能提交 GPU 上传。

旧 T01/T09/T35/T49/T52/T62 API+CLI 均通过；日志
`late-identity/targeted-view.log`（目录 metal-targeted.aFVDUQ）。
Python编译、shell语法和 git diff --check通过；Build.cs有既存LF/CRLF提示。
**全量回归未运行。**

## 人工 UI 结果

最终5fab70bf…库/app实际打开 EgT9h0/frame_view_capture.rdc，窗口显示loaded/
no problems detected。Computer Use双击EID15/36/15，Pipeline State的table
Buffer17 offset8、output Buffer18；Buffer Viewer输出十六进制59/A7/59
（89/167/89），DEADBEEF哨兵不变，正常退出。日志 viewer-frame-view.log。

此前c7bf…v3 Shared小帧Viewer EID9/24/9也得到41/80/41，正常退出。
最终 MExCZK/frame_private_capture.rdc 另一次UI尝试时macOS又已锁屏，未记为通过；
该本次测试Viewer已终止清理。Private view 的最终UI通过不清除这项或全部旧T待验。

## 完整 UE 与下一处阻塞

最终库对真实UE原件 API code19/process4、CLI exit1，在 metadata 阶段拒绝；
日志 ue-api-view.log / ue-cli-view.log 没有 frame wait/private initial upload。
完整UE GPU/GUI未验收，不能以新 v3 小帧结果声称 UE replay已修好。

现有 UE5.8.3（CL58210709）是 installed/promoted build，MetalRHI内部
FreeDescriptor/UpdateDescriptorImmediately/FlushPendingDescriptorUpdates/
IRBindResourcesToEncoder 没有相应 public exports。当前插件可拿两个primary
buffer，无法观察全部slot有效性、临时slice/覆盖表、每次CPU来源及inline CBV布局。
环境证据 late-identity/ue-provider-environment.json；符号缺失本身不证明所有
替代插桩方案绝对不可行。

具体源码证据与待补记录见 [UE58 provider要求](UE58_DESCRIPTOR_PROVIDER_REQUIREMENTS.md)。
已询问用户是否有可重编译MetalRHI的UE5.8.3源码环境及其路径。用户现在无需
重截；须先补齐provider和driver支持/小例验证，之后才生成有完整记录的新帧。
