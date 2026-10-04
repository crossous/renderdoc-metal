# B467 — Temporal MetalFX 与 VRR

2026-10-05。保留此前全部未提交修改；本轮没有提交或推送。

## 交付

- Metal 3 `MTLFXTemporalScalerDescriptor::newTemporalScalerWithDevice:` 接入公开 API wrapper；捕获 `encodeToCommandBuffer` 的 scaler 身份、Color / Depth / Motion / Exposure / Reactive Mask / Output / Fence 与参数，回放调用 native MetalFX。同一个 scaler 在一次回放中跨 command buffer 保持历史；整帧重新回放释放并重建 scaler，避免上次 inspection 的历史污染。command buffer 关联持有 scaler，覆盖 unretained-reference 和未提交 partial replay 生命周期。
- Temporal UI 使用单独的 MetalFX 流程页面，显示尺寸、格式、content size、scaler、jitter、motion-vector scale、pre-exposure、captured reset、reversed depth、auto exposure、dynamic-resolution settings、content-scale range、reactive mask、synchronous initialization 和回放历史状态。Texture Viewer Inputs/Outputs 使用公共 descriptor inspection，不伪造内部 shader 或 compute PSO；各输入有明确槽位名称，可以跳转和跟随。
- Usage：Color 为 `MetalFX - Input`；Depth / Motion Vectors / Exposure / Reactive Mask 分别有 `MetalFX - … Input`；输出为 `MetalFX - Output`。自动曝光时忽略的 exposure 以及禁用的 reactive mask 不作为输入，也不报告读 Usage。所有新枚举追加到原枚举末尾，既有值保持不变；Timeline 按读/写分类。
- VRR 复用既有 map 捕获/回放，在 Metal Pipeline State 的 **RS / Rasterizer** 页展示 Enabled、Logical Screen Size、Layer Count、map 资源跳转、各层 Physical Size / Horizontal Rates / Vertical Rates。按 D3D12 的光栅化阶段位置放置，不伪造 Metal 没有的 VRS combiners / shading-rate image。绑定的 map 保留 `StateObject` 身份，在真实 draw / mesh draw 上报告 `Rasterization Rate Map` 读取 Usage；参数 buffer 若参与普通复制/绑定，按其真实操作记录。
- 修复 GUI 连续切捕获时 Texture Viewer descriptor-name 回调访问旧缩略图。UI 线程建立弱控件引用和不可变更新快照；回放线程只查询该快照；回 UI 后检查事件 generation 与控件生命周期，丢弃已过期结果。真实切换复现过此问题，修复后同一测试通过。
- 公共 Pipeline State / RPC 添加 Temporal 与 VRR 参数。旧 `.rdc` 的 VRR state 和 Usage 在新后端打开时重建，不需要重截；新增 Temporal 使用末尾追加 chunk（1414），旧 chunk ID 不变。CLI 与 GUI 内嵌后端已同步。

## 精确性与范围

**公开 API 没有导出 MetalFX 的 private 前帧历史的方法。** 若捕获里某 scaler 的首次 encode 没有 reset，回放第一次调用会建立新历史，并在参数页明确显示 `Pre-capture history unavailable: replay starts with reset; output may differ until a captured reset.`；保留应用原始 Reset History 值，不伪称应用设置过 reset。警告在之后的非 reset 调用中保持，遇到捕获内 reset 后清除。捕获包含 reset 起点时，之后的历史在 native scaler 上重建；normal / auto / minimal 测试的两个输出均与原生逐字节一致。

这不是对任意中途截帧的精确历史快照支持。没有 Metal shader 单步、MetalFX 内部 shader 编辑或 Pixel History；B467 的 VRR 参数 buffer 测试使用预创建的 Shared 源/目标。当时补测发现“draw 之后才创建同一提交内的 Shared blit 目标”会触发部分回放 CPU snapshot 完成失败，该变体在 B467 不计 PASS。此一般回放缺口已在 [B468](BATCH468_FUTURE_SHARED_PARTIAL_REPLAY.md) 修复，新增普通绘制与 VRR 后创建 buffer 测试通过。

本轮没有接入 Metal4 Temporal、Temporal Denoised、Frame Interpolation。reactive mask 使用 macOS 14.4+ API；scaler 本体使用 macOS 13+，需设备支持。VRR 沿用已有设备/层数/descriptor 边界，本机层数上限测试是 4；T98 的两层元数据与 Usage 通过，但该夹具只实际绘制 slice 0，不宣称 slice 1 已验证。

## 对照依据

| 后端 | 当前 checkout 中的实际显示 / Usage |
|---|---|
| D3D12 | `D3D12PipelineStateViewer.cpp` 的 RS 显示 base shading rate、combiners、image；`RSSetShadingRateImage` capture 标记 frame read，但 `D3D12CommandData::AddUsage` 没有为 draw 加专用 VRS usage。 |
| Vulkan | RS 显示 pipeline shading rate / combiners；OM 附件显示 shading-rate / density map；`WrappedVulkan::AddFramebufferUsage` 没有为这些 map 加专用读取 usage。 |
| Metal | VRR 参数放 RS；map 是 StateObject 而非 texture；此次增加专门的 map Usage。没有修改 VK / D3D12 后端。 |

语义参考：[Apple MetalFX 教程](https://developer.apple.com/videos/play/wwdc2022/10103/)、[Apple VRR](https://developer.apple.com/documentation/Metal/rendering-at-different-rasterization-rates)。

## 验证

当前 backend / app 内嵌库 SHA256 均为 `a371c1406e7fb11e018e578966881d6782b22a5aee581f0ea6e5369404d53dc4`。

- 六份 native validation / capture / API inspection / CLI 三循环通过；API inspection 做 EID0 reset、正向、反向、再次正向。normal / auto / minimal 的 128×128 RGBA16F 两输出与各自 native oracle 全部逐字节一致；MV 的原始 RG16F `.5, -.25` 被读取并验证。
- missing-history 明确保留警告；recovery 第三次 reset 后清除警告。忽略的 exposure / reactive mask 没有伪造 descriptor 或 Usage。scaler 及 VRR map 都有正确 StateObject 资源身份。
- Temporal 27 个、VRR 18 个 malformed input 均干净拒绝，不进入非法 native encoding。
- 15 份旧捕获定向回归（9 sentinel + T95/96/97/98/74/308）通过；B465 七份 AIR/source/Spatial/fetch/Tile/memoryless/combined 资源、像素、Usage 检查通过。T95/96/98 追加 VRR RS 元数据和 draw Usage 检查；T97 未绑定 map 时不显示启用状态。
- 真实 GUI 使用公开 UI API 连续切 VRR / missing-history 三轮通过；RS 位置和参数、Temporal 历史提示、五个实际 input 缩略图、output 缩略图、全部六个资源跟随和 exact-history 状态检查通过。自动检查不代替鼠标右键菜单外观或用户最终布局验收。本轮没有重跑全量回归。

日志目录：`build-macos-debug/metal-temporal-vrr/`。主要日志为 `validation.log`、`temporal*-inspection.log`、`vrr-inspection.log`、`temporal-invalid.log`、`vrr-invalid.log`、`regression.log`、`ui.log`。

## 测试帧与人工验收

路径：`/Users/crossous/Developer/renderdoc-metal/captures/metal-features-b467/`。

| 文件 | 事件 / 重点 |
|---|---|
| `temporal_capture.rdc` | EID7 reset；EID17 延续历史。MetalFX Pipeline 参数、五输入一输出、纹理跟随及右键 Usage。 |
| `temporal-auto_capture.rdc` | EID7/17。Auto Exposure=Yes；Inputs 不含应用设置但被忽略的 Exposure，Exposure 资源没有 FX 读取 Usage。 |
| `temporal-minimal_capture.rdc` | EID7/17。三个必需输入，未设置 Exposure，Reactive Mask=No。 |
| `temporal-missing-history_capture.rdc` | EID7/17。应用已在截帧前 warmup，捕获没有 reset；参数页显示历史不可恢复警告，不做 native 精确像素断言。 |
| `temporal-recovery_capture.rdc` | EID7/17 有警告，EID27 reset 后清除。 |
| `vrr_capture.rdc` | EID7 draw。RS Enabled=Yes、Logical=400×300、单层横速率0.5/0.5、纵速率0.75/0.75；M2 Pro 上 Physical=208×230。map 打开 Resource Inspector 查看 draw 的 Rasterization Rate Map Usage；后续普通 pass 不应继承 map。 |

```bash
APP=/Users/crossous/Developer/renderdoc-metal/build-macos-debug/bin/qrenderdoc.app
CAP=/Users/crossous/Developer/renderdoc-metal/captures/metal-features-b467
open -n "$APP" --args "$CAP/temporal_capture.rdc"
open -n "$APP" --args "$CAP/vrr_capture.rdc"
```

可重现测试：

```bash
cd /Users/crossous/Developer/renderdoc-metal
bash util/buildscripts/scripts/test_metal_temporal_vrr_macos.sh
build-macos-debug/bin/qrenderdoc.app/Contents/MacOS/qrenderdoc \
  captures/metal-features-b467/vrr_capture.rdc \
  --ui-script util/test/metal/metal_temporal_vrr_ui.py
```
