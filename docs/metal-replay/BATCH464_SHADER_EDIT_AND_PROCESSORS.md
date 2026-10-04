# B464 — Shader Edit、嵌入源码与 Shader Processors

日期：2026-10-04。保留 B440–B463 工作树改动，未提交/推送。

## 实现与其他 API 对照

复用公共 `PipelineStateViewer::SetupShaderEditButton/EditShader/EditDecompiledSource`、
Shader Viewer 的编译/Apply/Revert 和 `ShaderProcessingTool`。后端对照本地 D3D12/Vulkan
`RefreshDerivedReplacements`：构建全部依赖 PSO 成功后才发布替换。后台失败不发布新映射；
公共 CaptureContext 编辑器遇编译失败会 RemoveReplacement 返回原 shader，沿用其他 API 行为。
Metal 部分补齐的是 native library/function/PSO 编译及资源释放，不重新造编辑器。

- 新增 `MetalLib`、`MetalAIRAsm` 编码，追加到枚举尾部，已有编码数值不变。
  捕获 metallib 不再被标为 Unknown，公共外部 processor 可正确匹配输入和文本输出。
- MSL 源码直接 Edit；带 HSRD/HSRC 的 metallib 在内存解析 SARC/bzip2/ustar 源码。
  各 archive、文件数、解压与累计源码大小有界，不按捕获文件名写入磁盘。
  公共 include 处理保留 `<metal_stdlib>` 并展开捕获 local header。不会从未捕获的文件
  推测源码；debug archive 中的额外 compiler options/macros 尚未自动恢复，复杂离线
  源码依赖需保留在编辑输入或外部 compiler 配置中。
- 二进制 Edit AIR 提取入口所属的有效 LLVM module，去掉 metallib section headers；
  `xcrun metal -c`、`metallib` 再编译。保留 recorded MSL/AIR 版本和 macOS deployment target，
  避免主机 Metal4.0/AIR2.8 默认值与 UE 的 MSL3.2/AIR2.7 不兼容。
- function constants 与 specializedName 保留；原库入口通过 entrySourceName 传给编译器和
  外部处理器。UI 和后台 AIR 选取同样使用原入口，避免函数别名匹配失败。
- 保留各 render/compute/mesh/tile 原 native descriptor，重建共享该函数的全部管线；
  保持 RT formats、vertex/tessellation layout、sample count、payload limits 等状态。
  frame-born PSO 第一次原始模板保留，编辑中重放创建不能覆盖原模板。
- 改动 GPU mapping 前完成已提交工作；临时 function/PSO 使用资源管理器 typed native release。
  原 unregister-only wrapper 释放不会释放 native object，首次审查已纠正该 leak。
- 当前事件的 ShaderReflection 更新为 edited shader，View 能显示实际编译的编辑，而非旧源码。
  Revert/关闭编辑恢复原映射；Free 正在使用的 target 也先撤销再释放。
- 包含 linked function tables 的 PSO 明确拒绝 Edit，防止新 PSO 使用旧 function handles。
  尚未支持 ray/intersection helper 的完整表依赖重建。Edit 不启用 shader debugging/单步。
- View 保留 Apple AIR、Captured MSL、editable AIR；外部 processor 使用公共配置及下拉框。
  配置例子和具体操作见 [Shader Tools](../../util/shader_tools/README.md)。

## 高级反编译器调查

[metal2vulkan](https://github.com/steelbrain/metal2vulkan) 是 AIR/LLVM → Vulkan SPIR-V 的 alpha
项目，宣称 VS/post-tessellation VS/FS/CS 支持。[SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)
可将 SPIR-V 转成 MSL，不能直接读取 AIR。组合链有研究价值，但尚未本地构建、没有 UE
ABI/绑定/dispatch contract 和 GPU 语义验证，未安装或默认为可编辑 MSL。仅通过 spirv-val
不代表图像正确。[MetalLibraryArchive](https://github.com/YuAo/MetalLibraryArchive) 主要提供
容器/AIR 与已嵌入源码提取，不能反编译丢失的原源码。未找到已验证的成熟 AIR→MSL 替代品。

## 验证记录

证据根目录：`build-macos-debug/metal-shader-edit/`。

### 定向终端

首次候选 `2b05b6a1…`：8×8 MSL/AIR VS/FS/CS 两次 compile→replace→EID0 reset→remove/free，
错误代码保留上一次合法编辑；FS 改为红色在两个共享 shader 的 PSO 都生效；AIR CS 写值100..103
改为200..203并恢复。嵌入 debug MSL、frame-born pipeline、T66 post-tess VS/FS、T79/T80 Mesh/FS、
T85 Task/Mesh/FS 同源码两轮替换通过。View reflection 指向实际 edited target。
最终候选 `06ae2318b25fb69312a60cd1d59cac08664200db58f59d63372c5f572728e3a2`，
库与 app bundle 一致。`directed-final.log` 的 source/AIR/debug/alias-source/alias-air/frame-born
全部通过，包括 specializedName 与原入口不同的情况。有效 MSL VS 的 vertex attribute 与原
vertex descriptor 不兼容时，编译 PSO 返回明确错误，后台保留此前合法 FS 编辑；
`incompatible PSO preserves last good edit=1`。

一次初始失败是测试预期错误：FS 写入未绑定 RT1 被 native Metal 允许，不能作为无效 PSO
测试。改用缺失 vertex attribute 的合法 VS 后，负例能验证实际管线编译失败，未改后端
来迎合错误预期。原失败日志保留，最终脚本出口为0。

入口（先退出 qrenderdoc/UE，GPU 串行）：

```sh
bash util/buildscripts/scripts/test_metal_shader_edit_macos.sh
# 另加同一 UE 的 AIR FS3928/CS3612 编辑验证：
bash util/buildscripts/scripts/test_metal_shader_edit_macos.sh \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc
```

### 同一真实 UE

捕获 SHA-256：`c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`。

最终 `06ae2318…` 的 `final-ue-3928.log`、`final-ue-3612.log` 也分别通过两轮 AIR
同代码 compile→replace→EID0 reset→remove/free；所有实际写目标的整张 mip0 字节一致。
`final-events/events.log` 的3612/3928/4036各两轮 reset 和九个 GBufferA Usage 通过；
所有 GBufferA 读回 SHA-256 为 `33d636f6cc68eebcce5813669fe0a81f45d60d595c4770087b856706a459d55f`。
`final-images/events.log` 正常打开和两次完整 reset 的三份900×640呈现图 SHA-256 均为
`fc5f3afe33b3ec7a523d447d59d0b517337d4c8f8867f6d863f99f0ab5eb4f61`。

首次候选 FS3928、Nanite CS3612 两轮 AIR 同代码替换/撤销通过。扩大 CS 验证至实际四个写目标
SceneColor/GBufferA/B/C 全 mip0 字节都一致；不是只比较首个 color target。
`ue-3612-outputs.log`。FS 的全部纹理输入及写目标此前也一致，`ue-3928-all-resources.log`。

扩展到全部只读纹理的首次 CS 比较失败必须保留：仅 readonly Nanite.VisBuffer64 在 EID0 reset
后出现 raw bytes 差异，四个写目标没有差异。独立 `control` 模式不编译、不替换任何 shader，
只进行五次 reset，同样重现 VisBuffer64 变化。证据 `ue-3612-all-resources.log`、
`ue-3612-trace.log`、`ue-3612-control.log`，首差 offset327681（word offset327680）。
目前原因未确认，不归因于 shader edit，也不宣称所有 UE 只读输入严格稳定。

### 全量

冻结 `2b05b6a1…` 库的 `full-regression.log` 已通过：308 captures、7786 malformed、
3080 lifecycle，growth11517952B，start/end hash一致、exit0。
最终 `06ae2318…` 另冻结到 `full-final-build` 的 `full-final-regression.log` 也通过：
308 captures、7786 malformed、3080 lifecycle，growth1343488B，start/end hash均为
上述完整06ae SHA、exit0。没有使用上述首候选或 B463 的全量替代最终库的全量。
全量后 `post-full-3928.log`、`post-full-3612.log` 的两轮 AIR 编辑/恢复通过；
`post-full-events/events.log` 的3612/3928/4036各两轮、九个 Usage及 GBufferA 字节通过。
六份 GBufferA 均为上述33d636 SHA，光照 SceneColor 两份均为
`51527fdfeca62b87e62ef006f7108aad889bbef6146562aea6e2801b94466ac3`。
`post-full-images/events.log` 的正常打开与两轮整帧通过，三份呈现图保持上述fc5f3afe SHA；
原捕获 SHA 不变。所有 GPU 验证串行，qrenderdoc/UE 在终端测试期间关闭。

### 实际 UI

当前 `06ae2318…` 的微型 `debug_capture.rdc`：

- 实际点击 FS Edit，自动打开嵌入 `shader.metal`、MSL/Builtin、原入口fs。
- 用公开 `CaptureContext.EditShader` 加载三行红色 FS；实际点击 Compile & Apply 后
  状态为 Edited Shader Active，Texture Viewer 显示红色；Remove changes 恢复原蓝色。
  直接输入源码的 CUA 操作曾 timeout，Find/Replace没有匹配，未计为源码键盘输入 PASS。
- 入口改为 `missing_entry` 实际 Apply 后 Errors 显示找不到函数，公共编辑器恢复原 shader；
  改回fs再次 Apply 成功，随后实际 Remove changes 恢复原 shader。
- 通过公开 Config API 添加并保存 `Apple AIR (external)`，保留已有 processors。
  View 实际下拉框出现该处理器，选择后执行并显示入口fs的有效 LLVM AIR 文本；再切换
  Captured MSL 能显示原代码。不是只检查配置字段或执行后台脚本。
- 退出时选择不保存临时捕获编辑，未修改微型 RDC；未调用此前阻塞的原生 Save 文件面板。

同一 UE 最终全量后正常打开，公开 stage API 选择3928 FS，然后实际点击 Edit Fragment：
external processor 执行 exit0，返回104617B，编辑器显示 `MetalAIRAsm`/Builtin 与原入口。
实际 Apply 后 Edited Shader Active、Errors 为空；实际 Remove changes 后 Original Shader
Active，Texture Viewer 的原场景和命名 GBuffer 输入正常。Edit 下拉实际列出 external、
Edit AIR (Apple toolchain)、Edit MSL replacement；点内置 AIR 打开 `edited.ll`，无需外部配置。
Nanite EID3612 CS 通过实际菜单选择内置 AIR，打开 `edited.ll`，MSL3.2/AIR2.7 原模块
保持入口 `Main_00012ae4_d2df2e04`；实际 Apply 后 Edited Shader Active、Errors 为空。
随后实际 Remove changes，UI 已恢复 Original Shader Active。最终界面返回3928 FS 的
Pipeline State；FS/CS 都没有 active replacement，临时编辑未保存进原 UE RDC。

实际按钮操作与公开 API 导航分开记录；没有把终端 PASS 记成 UI PASS。
