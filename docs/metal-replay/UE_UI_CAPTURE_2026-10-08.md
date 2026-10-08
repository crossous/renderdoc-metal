# 1280×720 UE Lumen / Nanite / VSM UI capture

本卡是正常尺寸 UI 交付的唯一当前记录，延续 B544/Phase59。先前 `f1ba1862` 的 compute RT M1–M4 已完成；这里的新候选结果不继承旧哈希 PASS，也不重开 render-stage RT。

## 场景与真实执行

外盘根：`/Volumes/CauseUseMac/RenderDocMetalArchives/20261008-UE-Lumen-Nanite-VSM-720p`。

UE 5.8，独立 `scene/project/MetalRayScene.uproject`，15 个启用 Nanite 的网格，6 个材质，金属反射物体、可移动太阳/天空光；固定相机，实际视口 1280×720。用户原工程未改。

Lumen Hardware RT / Inline、Nanite、VSM 运行时为 1；不是只看配置或 marker：`feature-proof.json` 记录 6 次绑定 ray-query shader 的非零 dispatch；`feature-geometry-commands.json` 中 68 条非零 Nanite/VSM 几何/调度命令，含非零 indirect geometry draw。名称和资源/事件编号仅定位诊断，不能成为后端支持条件。

High=2；独立应用设置 SurfaceCache.AtlasSize=1024、Shadow.Virtual.MaxPhysicalPages=1024、普通 streaming pool=64MiB、Nanite pool=32MiB/root pages=256。保持 Lumen、Nanite、VSM 开启，明确不声称默认 Epic/Cinematic 缓存。此机器统一 replay 内存预算不变。默认 High 的 VSM R32Uint 16384×2048×2 为 256MiB，超出既有单 logical texture 128MiB 预算；不得称为 128MiB 或删除预算。

`capture/original.rdc` capture backend `850c540b`，capture SHA256 `c827cc7e7ee1f58e7a9a620a9583e9093208d1e0b797ebce1caa975d371a27f8`。Native 同帧视口 `capture/native-viewport.bgra`；EndFrameCapture 之后、下一次 draw 之前保留真实 viewport RHI texture 读回，不在捕获内部插入诊断等待。Native 成功，supervisor 有界清理成功，不等于 replay/输出/整体通过。

## 通用机制与 DX12/Vulkan 对照卡

|机制|具体仓库函数及行为|Metal 实际修改 / 保留边界|
|---|---|---|
|原 dispatch 参数|DX12 `Serialise_Dispatch`；VK `Serialise_vkCmdDispatch` 保存参数并调用原 Native API，回调/事件与执行参数分开|`MetalComputeDispatchExtentFits` 移除 direct 单次/累计 invocations 的 CPU 场景许可；保留非零线程组、64-bit 溢出、真实 Native PSO/设备线程组/内存约束。indirect 的历史 invocation 数值上限也移除：保存 GPU 当次参数并用同一快照执行，保留实际参数 64-bit 溢出和冻结 RT invocation 契约；统一资源/快照/初态预算保留。|
|draw/no-op/无附件|DX12 `Serialise_DrawInstanced`、`Serialise_OMSetRenderTargets` 保留零数量与零 attachment；VK `Serialise_vkCmdDraw`、`Serialise_vkCmdBeginRendering` 重放原参数与 renderingInfo|无附件 explicit extent 使用已声明 API 16K 范围，direct 和 indirect 均允许；真实 zero draw 不产生 shader/AS consumer，但 typed slot、对象、发布、indirect per-use 参数验证保留。direct serializer 不再拒绝合法 zero count。render indirect 的既有有限预算未在本次改动。|
|纹理尺寸/逻辑初态|DX12 `Prepare_InitialState` / `Apply_InitialState` 保存物理资源与 copy footprints；VK `Prepare_InitialState` / `Apply_InitialState` 恢复 device memory/init requirements|`MetalTextureReplayLayout` 统一 2D logical 16K；`InitialTextureLayout` 复用同一格式族/mip/array/溢出布局，再验证 Native pitch/staging 范围。逻辑 128MiB、物理 aggregate/initial/snapshot 预算均保留。|
|view 的真实物理输入|VK `AddImgFrameRef` 引用 view 的 base image/base memory/subresource；DX12 `Descriptor::GetRefIDs` 引用实际 SRV/UAV/RTV resource/counter|capture `newTextureView` 保存实际 API source ResourceId；`MarkResourceFrameReferenced` 对其父对象加入 ReadBeforeWrite，父真实初态写入 capture。不能把 view creation AddParent 当初态引用，也不能从 Native 指针错误 cast GetWrapped。旧缺父初态 capture 不伪修复。|
|Drawable 原方法/并发|DXGI `GetBuffer` → Native GetBuffer/WrapSwapchainBuffer；`Present` / VK `Serialise_vkAcquireNextImageKHR` 保持原 Native 调用和对象身份|ObjC hook 按实际 Class 保存原 IMP，受共享锁保护、先发布再安装，处理继承/类局部 override；Native present 仍先执行，独立全局锁串行 Metal FrameTick，frame counter atomic，避免并行 drawable 引起 Tick/FrameTimer corruption。Metal selector hook 差异有必要单独实现。|
|不可变 shader 语法与每次调用事实|DX12 `ShaderEntry::AddShader` / `GetDetails` / `BuildReflection` 按实际 bytecode 存解析/反射；VK `ShaderModule::Init` Parse SPIR-V，`ShaderModuleReflection::Init` 分别处理 stage/entry/spec constants|AIR 固定 regex 编译一次，有限 lexical cache 仅匹配同一不可变 grammar/文本；bindings、loader、CPU state、typed address、resource/submit/AS 资格都在当前 invocation 重算。缓存耗尽回原 matcher；不缓存 scene 支持资格、不增加表达式，也不因 display 未知免除地址恢复。|

16K API 范围依据 [Apple Metal capability tables](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf) 的 Metal3/Mac2 2D 范围；本次不把 Apple10 32K/额外 render RT 自动加入支持。

## 根因与实验分支

1. 原 capture 同样普通 Native startup 曾崩溃：LLDB 显示 Drawable texture hook 递归；并发首次安装把 hook 本身写成 original IMP。修复以实际 Class 原 IMP 为身份。独立 8-layer 多线程后来暴露 `RenderDoc::Tick → FrameTimer::push_back` 并发内存破坏，增加 Metal Tick 串行/atomic counter。系统 crash 与失败日志保留。
2. 独立 wide texture view 的 capture 仅几 KiB，父真实上传缺失：view creation 的 parent dependency 不会自然产生 frame initial reference。最终 source ResourceId 引用补齐后，R8/R16/mip/slice/offset 三组合全输出与 EID0 精确通过；错误 Native GetWrapped cast 的中间失败保留。
3. 正常 UE frame 的 direct dispatch、16K logical texture、attachmentless/direct draw/zero consumer 曾被历史场景限制挡住。修改落于原 API 执行/物理初态机制，资源恢复仍必须满足契约。
4. 当前正常整帧 replay 预检耗时：先假设等待/namespace 扫描，采样实际缩小到 `validateRuntimeDispatch → UniformResourceAccess` 的重复语法解析。关诊断 trace 后仍 CPU 100%，不是 trace/GPU wait 根因。固定 regex 仅减少构造，仍超时；第一次 lexical cache 被 negative alternatives 填满，采样显示 fallback matcher。改为仅缓存实际语法命中并对 call 匹配做语义等价的文字前筛；下一实验验证 CPU loading 时间及实际整帧输出，失败则重新采样真实热点，不加场景放行或容差。
5. 独立 attachmentless T1 runner 初次漏 `RENDERDOC_METAL_CAPTURE_RENDER_INDIRECT_ARGUMENTS`，缺 per-use evidence 的拒绝正确，不是 backend 丢失已保存输入。修正 runner 后 HD/odd/wide 正例通过。direct zero 的 serializer 检查实际修正。negative converter 的 length=0 路径有 scratch writer chunkID assertion，不能算干净 API 负例；新脚本用非零原 length+32，保留失败日志，必要契约负例另行验证。

6. 整帧越过 CPU 预检后，`ce994d12` 实际 GPU 调度被旧 indirect 单次 262144 / 累计 8M 上限置零，随后 API 拒绝。对照 VK `FetchIndirectData` / `Serialise_vkCmdDispatchIndirect` 保存 per-use 参数后执行 Native indirect；DX12 `Serialise_ExecuteIndirect` / `PatchExecuteIndirect` 按 signature/address 修复，不用 CPU shader invocation 数作为 ordinary dispatch 许可。Metal 移除 invocation 数值预算，不增加上限：32-byte per-use 快照、原有 GPU barrier、原 binding 恢复、冻结 RT invocation 校验保持；实际 64-bit extent 溢出仍置零并明确失败。动态 `ulong` 除法的独立 Native pipeline compilation 重复 XPC connection interrupted，32-bit `mulhi` 双 limb 乘法编译通过并避免该依赖，错误报告补齐。三 fresh private GPU API-copy indirect 样例跨旧累计上限，正常 Native/完整输出/两次 EID0 精确匹配。
7. `typeDefinition` 的无锚点全模块搜索还会在长 percent-prefixed instruction 上反复回溯。改为逐行、literal separator 前筛后使用相同 grammar，不改变类型/资源绑定语义。完整旧捕获当前库从超时推进到实际三整帧 GPU 完成；不是 shader 表达式新增许可。
8. 同一最终候选的 fresh UE capture `a877c06d` Native/有限监督清理通过，但 normal replay 在 frame epoch 前的背景 placement newBuffer 重叠拒绝（4.4s，未进入整帧 GPU）。CPU inventory：背景 placement factories 从1281变1289，均无显式 makeAliasable；具体物理恢复/对象历史根因未闭合，不删除检查或称损坏 capture。该 capture 留作下一诊断输入，未作为可打开 UI 文件交付。

## 验证状态（外盘 manifest 为精确结果）

- 交付 capture `c827cc7e` Native COMPLETED；当前 `1845eca8` normal OpenCapture + 两次 EID0 的三完整 GPU 重放 COMPLETED，完整 Native 输出比较 FAIL，整体 INCOMPLETE。
- `a34b3bee`：三 large direct dispatch、三 wide parent initial/view、三并发 drawable fresh capture 均 Native/完整输出/事件与 EID0 精确通过；官方 Apple triangle/procedural 重复 Native、fresh capture、output/events、API/CLI、Native capability/size/unbound 均通过。不是后续 hash 的通过证据。
- `84d0f325`：AIR component 通过；5 组 attachmentless 正例（HD、odd parallel+unretained、16K wide、direct zero、indirect zero）Native/replay 定义原子 count/sum/sentinel 与 7 次 EID0 往返通过；负例 converter assertion 尚未合格，整体未 PASS。
- 最终 backend/bundle `1845eca83cf44f152d8b8d908df4275bcf2ba15df193c0fb10bbf61c90c67eed`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`；CLI `07b1f0703741e29a2b68fc7c19f3a6a62b259ebe21c329ebc8300f00cf45be1c`。构建通过，bundle/backend 相同。
- 同最终候选 `guard-T1/manifest.json`：三 direct、三 Private GPU copy indirect、三 wide parent initial/view 的 fresh Native/capture/三完整输出与 EID0 精确通过；三并发 Drawable fresh process 通过。`related-guard-UI/manifest.json`：五 attachmentless 正例/往返通过，修正后的42实际契约负例 API/CLI GPU前拒绝且无 converter/assertion。`final-AIR-CPU.json`：现有 AIR component 精确断言通过，没有增 CPU 数值表达式。
- 同最终候选 `official-delivery-RT/manifest.json`：Apple Metal2 原许可/固定archive/metallib，triangle/procedural 两次 Native一致、fresh capture输出、API events/TLAS binding、CLI、Native capability/size/unbound 均通过。完整 converted 矩阵本次 NOT_RUN，旧 f1 的 M4 不冒充新 hash 的通过。
- `replay-guard/manifest.json`：实际三整帧 GPU 完成168.875s，peak RSS 2,598,191,104 bytes，无 Metal API Validation/assertion/ForceCrash。视口 RGB10A2 1280×720，原 UE RHISurfaceDataConversion/FColor::Requantize10to8 精确转换为 BGRA，同帧 Native 比较无 gamma/flip/tolerance；三次分别18133 / 23804 / 15578个像素不同，最大通道差37 / 24 / 24。三 raw hash 不同，不把差异小、Native单kernel或候选集合当完整图像因果证明。输出 FAIL；整体 INCOMPLETE。
- GUI手工验收 NOT_RUN，用户将通过命令开启。Native/readback、shader显示 partial、完整图像验收和GUI验收分别记录。

## UI 交付

交付 `UE-Lumen-Nanite-VSM-1280x720.rdc`（90,758,617 bytes）是 `capture/original.rdc` 的原字节复制，capture SHA256 保持 `c827cc7e7ee1f58e7a9a620a9583e9093208d1e0b797ebce1caa975d371a27f8`。它在当前1845正常API完整加载/执行成功，与 capture-final/a877 的失败区分。

`UE-Lumen-Nanite-VSM-1280x720-Replay.png` 为实际重放 cycle0 的 RGB 预览；`...-Native.png` 为该同帧 Native 预览；展示PNG只呈现RGB、关闭alpha显示，精确比较仍包含全部BGRA字节。均已视觉检查。外盘 `manifest.json` 收录文件/产品/测试/限制，`open-UI.command` 保存原命令：

```sh
open -n "/Users/crossous/Developer/renderdoc-metal/build-macos-debug/bin/qrenderdoc.app" --args "/Volumes/CauseUseMac/RenderDocMetalArchives/20261008-UE-Lumen-Nanite-VSM-720p/UE-Lumen-Nanite-VSM-1280x720.rdc"
```

首次 Debug loading 约3分钟。Texture Viewer 选择 `BufferedRT` 看场景，Event Browser 可定位 Lumen ray-query、Nanite 与 VSM（诊断名称不决定后端支持资格）。不创建未验证的 UI 自动选择脚本，不让脚本固定EID决定支持。原 Qt GUI崩溃本次未自动认证。用户 RDHeaderView.cpp、renderdoc.conf、安装UE executable 哈希核对未变；全部中间文件在外盘，未提交/推送/reset。

## 下一诊断 / 交付边界

本次用户的可打开截帧+预览+UI命令已提供，UI手工验收待用户执行；完整图像能力认证未完成。

下一轮假设须区分（A）真实恢复/地址/依赖缺口，（B）Native 合法无序/非确定性，（C）RGB比较工具错误。现有整帧EID0变化只证明 replay 输出不恒定，不证明任何一项。先选择无需增加上游 pass 快照的可区分实验：对相同完整 Native 固定初态/参数的输出重复，或在相同恢复状态下观测有序约束的变化；严格记录额外观察等待影响。两轮无缩小即换方法，不扩 CPU expression、不加容差。fresh a877 的背景 alias 恢复义务独立闭合，不以旧成功 capture 豁免新 capture。最终1845相关矩阵/更多设备/render RT均不自动记通过。

## 仓库内交付包

按用户明确提交要求，原字节截帧、实际 Replay/同帧 Native 预览、SHA256SUMS 和可移植验收 manifest 已纳入 [util/test/metal/captures/ue-lumen-nanite-vsm-720p](../../util/test/metal/captures/ue-lumen-nanite-vsm-720p/README.md)。仓库根目录的打开命令见该 README。GPU 完成、输出 FAIL、整体 INCOMPLETE 的状态保持原样，不把提交代码当作验收通过；外盘诊断和构建产物不进入 Git。

发布前必要检查：88个Python文件语法与23个shell脚本语法、diff空白检查通过；本地RDHeaderView生命周期改动的CPU-only offscreen Qt旧/当前model及header析构回归通过，capture supervisor单元测试通过。该widget回归不等于完整GUI验收或原崩溃堆栈因果闭合；本次提交未追加GPU全矩阵，既有候选结果仍按上述哈希和范围记录。
