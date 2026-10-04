# B468 — 后创建 Shared buffer 的部分回放

2026-10-05。保留此前全部修改，未提交或推送。

## 修复

同一 command buffer 先编码 draw，再创建普通 Shared buffer 并编码 blit，最后 commit。加载整帧时该 buffer 存在，提交快照合法；跳到较早 draw 时，native buffer 尚未创建，但提交完成仍需要处理加载时保存的全部 CPU 快照。原 `ApplyFutureSharedAliasCPUUpdate` 只认识 descriptor coverage 的 frame buffer 元数据，普通未注解捕获因此返回 false，报 `Invalid Metal replay completion`。

普通 `newBufferWithLength` / `newBufferWithBytes` 在加载帧内创建时，现在保留长度、options 和 creation chunk offset。无 descriptor coverage 时，只有已验证的普通 buffer、native 对象尚未创建、创建位置严格晚于所选回放前缀、Shared storage、无 heap parent、且快照范围合法，才允许延后这份更新。此 allocation 对较早 GPU 命令没有物理 alias 影响，不需要提前创建；后续跳转仍按真实 creation chunk 重建，并在提交前恢复 CPU 快照。

仍按原流程恢复已经存在的 buffer 的提交快照，包含 draw 编码后、commit 前的 CPU 写入。descriptor / heap alias 路径与验证规则保持原逻辑；未知资源、错误类型、非 Shared 及越界不因此被忽略。不修改 `.rdc` schema，已有失败捕获可以直接重放。此变更没有为未验证的 `newBufferWithBytesNoCopy` 或新的 heap alias 情形提供豁免。

## 验证

- 修复前，同一 `late_capture.rdc` API seek 失败，`before.log` 记录 `Metal submission CPU snapshot future alias failed ... buffer=ResourceId::22 ... bytes=24624`，退出 3；修复后同一文件的 draw seek、像素和 VRR Usage 三循环通过，退出 0。
- 新普通捕获包含两次 draw、后创建 `newBufferWithBytes` source / `newBufferWithLength` destination、创建后的非零 CPU 修改，以及只覆盖中间 160 字节的 GPU copy。全部 25 个 API 事件正向／反向／再次正向共 75 次通过，每次从 EID0 重置；创建之前 buffer readback 为空，之后 source 字节准确，destination 在 copy 前保持 CPU 值、copy 后覆盖区域准确且边缘保留。两次 draw 的全部 32×32 RGBA8 像素准确，证明已有 uniform 的晚 CPU 写入仍被应用。
- VRR 后创建 `newBufferWithLength` 和 `newBufferWithBytes` 两种捕获均通过 native validation、原生参数复制、反复 draw seek、像素／map Usage 和 CLI 三循环。普通捕获 CLI 三循环通过。
- 8 个 malformed captures：未知／零／错误类型资源、越界 offset、uint64 overflow offset、payload size mismatch、Private snapshot target、过短 creation，均干净拒绝，无 crash。
- B467 六份已有捕获 API / CLI 三循环通过；normal / auto / minimal 两输出仍与各自 native oracle 逐字节一致，history warning / reset 恢复检查通过。
- 完整定向 event-navigation harness 通过：marker owner、retained/unretained 的 alias、nonoverlap、producer、tail、split_binding，共十种 submission 场景及对应 alias / scope gate。未跑全量回归。
- 实际 GUI 通过 EID13→16→21→13→0→13，回放 fatal 状态成功、两次 draw 全像素匹配。窗口留在普通测试帧 EID13 的输出纹理；不代替鼠标交互或外观验收。

后端与 app 内嵌库 SHA256 同为 `4bda5de2e8a5254ae7aa7c44ca232716b45e588db36dce732f7d0d9e804091ce`。

日志：`build-macos-debug/metal-future-shared/`，包括 `before.log`、`after.log`、`validation.log`、`inspection.log`、`invalid.log`、`temporal-regression.log`、`event-navigation.log`、`ui.log`。event-navigation 详细日志在 `build-macos-debug/metal-event-navigation.tszjWw/`。

## 测试帧与验收

目录：`/Users/crossous/Developer/renderdoc-metal/captures/metal-future-shared-b468/`。

| 文件 | 验收 |
|---|---|
| `ordinary_capture.rdc` | EID13 第一次 draw、16 buffer copy、21 第二次 draw；按此顺序后跳回13，输出均为 RGBA8 64/128/191/255。source / destination 的 copy Usage 为16。 |
| `late_capture.rdc` | VRR EID9 draw；Shared copy target 在编码 draw 后创建。跳 draw 与 copy，再返回 draw，不报完成回放失败。 |
| `late-bytes_capture.rdc` | 同上，target 使用 `newBufferWithBytes` 初始化。 |

公共 EID 把独立 CPU 调用与 GPU submission 分组，因此上述普通捕获的 source / destination creation 显示为 EID1 / 3，而 draw 为13。测试按 captured file offsets 判定物理创建先后，不按重排后的 EID 大小推断时序。

```bash
APP=/Users/crossous/Developer/renderdoc-metal/build-macos-debug/bin/qrenderdoc.app
CAP=/Users/crossous/Developer/renderdoc-metal/captures/metal-future-shared-b468
open -n "$APP" --args "$CAP/ordinary_capture.rdc"
open -n "$APP" --args "$CAP/late_capture.rdc"
```

重现：

```bash
cd /Users/crossous/Developer/renderdoc-metal
bash util/buildscripts/scripts/test_metal_future_shared_macos.sh
build-macos-debug/bin/qrenderdoc.app/Contents/MacOS/qrenderdoc \
  captures/metal-future-shared-b468/ordinary_capture.rdc \
  --ui-script util/test/metal/metal_future_shared_ui.py
```
