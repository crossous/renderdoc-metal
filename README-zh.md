[English](README.md) · [简体中文](README-zh.md)

<p align="center">
  <img src="qrenderdoc/Resources/logo.svg" width="88" alt="RenderDoc 标志">
</p>
<h1 align="center">RenderDoc Metal</h1>
<p align="center"><strong>在 macOS 上捕获 Metal 帧，检查图形调用与资源。</strong></p>
<p align="center">
  <a href="https://github.com/crossous/renderdoc-metal/releases/tag/v1.46-metal.1"><img src="https://img.shields.io/badge/download-macOS_arm64-3bb779" alt="下载 macOS arm64 应用"></a>
  <img src="https://img.shields.io/badge/API-Metal-555555" alt="Metal API">
  <img src="https://img.shields.io/badge/status-development_preview-d99a29" alt="开发预览">
  <a href="LICENSE.md"><img src="https://img.shields.io/badge/license-MIT-4279bf" alt="MIT 许可证"></a>
</p>
<p align="center">
  <a href="#下载应用">下载</a> · <a href="#测试-ue-截帧">测试截帧</a> ·
  <a href="#metal-图形能力">图形能力</a> · <a href="#unreal-engine-集成">UE 集成</a> ·
  <a href="#构建与验证">构建与验证</a>
</p>

## 仓库说明

本项目是基于 **RenderDoc 1.46**、独立维护的 **[baldurk/renderdoc](https://github.com/baldurk/renderdoc) 分叉（fork）**。代码以独立仓库形式导入，因此 GitHub 页面没有显示 fork 关系。这不是上游官方的 macOS 发行版。

当前重点是 **macOS Metal 捕获、恢复与重放后端**，以及 qrenderdoc 的资源、管线和 shader 检查能力。资源恢复、地址重定位、生命周期与提交依赖尽量参考仓库中的 Vulkan/DX12 实现。UE/Lumen 是验收负载，后端不会按引擎功能名称决定支持资格。

| 分支 / 版本 | 用途 |
| --- | --- |
| **`metal-replay-v1.46`** | 默认分支，持续开发 Metal 后端、测试与 macOS 集成。 |
| `codex/ue58-safe-checkpoint` | 较早的 UE 集成检查点，保留作历史参考。 |
| **`v1.46-metal.1`** | 优化构建的 macOS arm64 Release 应用，按开发预览版发布。 |

## 实际界面

![macOS qrenderdoc 中的 UE Nanite 截帧](docs/metal-replay/images/ue-nanite-qrenderdoc.png)

*例图来自本仓库提供的 1280×720 UE 截帧：选中 Nanite BasePass 事件，查看 GBufferC 与绑定资源。截图只保留 qrenderdoc 窗口内容，不包含桌面、其他应用窗口或系统界面。*

<details>
<summary><strong>展开查看整帧重放预览</strong></summary>

![整帧重放预览](util/test/metal/captures/ue-lumen-nanite-vsm-720p/UE-Lumen-Nanite-VSM-1280x720-Replay.png)

场景实际开启 **Lumen 硬件光追 / inline ray query、Nanite 与虚拟阴影贴图（VSM）**，使用 High 画质和已记录的缓存预算。已观察到非零光追调度与几何命令。

</details>

> **当前验收状态：** 已记录的 Debug 候选完成三次整帧 GPU 重放及两次 EID 0 重置，但与同帧 Native UE 图像的比较仍失败；另一份新捕获存在背景 placement 恢复缺口。可下载的 Release 应用已通过构建、打包和运行时检查，其 GPU 输出及最终矩阵尚未认证。详见 [截帧 manifest](util/test/metal/captures/ue-lumen-nanite-vsm-720p/manifest.json) 与 [当前证据](docs/metal-replay/UE_UI_CAPTURE_2026-10-08.md)。

## 下载应用

**[下载 Apple Silicon 版 RenderDocMetal.app](https://github.com/crossous/renderdoc-metal/releases/download/v1.46-metal.1/RenderDocMetal-v1.46-metal.1-macos-arm64.zip)** · [版本说明与校验值](https://github.com/crossous/renderdoc-metal/releases/tag/v1.46-metal.1)

1. 解压 ZIP，将 **`RenderDocMetal.app`** 移到 `/Applications`。
2. 打开应用。后端、命令行工具、Qt、Python 与固定版本 shader tools 均已打包，**运行不需要 Homebrew**。
3. 如需编译或编辑 shader，请安装包含 Apple Metal 工具的 Xcode。

应用为 **arm64**，构建部署目标为 macOS 12.0，已检查的设备环境为 **Apple M2 Pro / macOS 26.1**。具体 Metal API 仍受设备和系统版本约束，本次发布未认证 Intel Mac 或其他设备。应用采用临时签名，未做 Apple 公证；若系统阻止打开，可按 Apple 的 [“仍要打开”流程](https://support.apple.com/zh-cn/102445)操作。

## 测试 UE 截帧

**[下载 1280×720 RDC 文件](https://raw.githubusercontent.com/crossous/renderdoc-metal/metal-replay-v1.46/util/test/metal/captures/ue-lumen-nanite-vsm-720p/UE-Lumen-Nanite-VSM-1280x720.rdc)**（约 91 MB）· [截帧、预览与哈希](util/test/metal/captures/ue-lumen-nanite-vsm-720p)

通过 **File → Open Capture** 打开，或在终端运行：

```sh
open -n "/Applications/RenderDocMetal.app" --args "/path/to/UE-Lumen-Nanite-VSM-1280x720.rdc"
```

| 查看内容 | 操作方法 |
| --- | --- |
| 最终画面 | 选中帧末事件；在 **Texture Viewer** 中查找 **`BufferedRT`**，开启 RGB、关闭 Alpha。 |
| Nanite 几何 / GBuffer | 在 Event Browser 搜索 **`Nanite`** 或 **`BasePass`**，查看输入、输出与 Pipeline State。 |
| Lumen 光追调度 | 搜索 **`Lumen`**，选择 ray-query compute 事件，检查 shader、加速结构与资源绑定。仅有 marker 名称不代表确实执行了光追。 |
| 虚拟阴影贴图 | 搜索 **`VirtualShadow`** 或 **`Shadow`**，查看相应 draw/dispatch 与纹理。 |
| 事件往返 | 在前后事件之间切换，再返回帧末，检查资源内容与绑定。自动化 EID 0 测试另有记录。 |

此前 Debug 构建首次加载这份较大截帧约需三分钟；Release 加载速度尚未形成公开性能结论。这是 **UI 手工验收样例**，不是像素完全一致的认证结果。场景、比较方式与 SHA256 检查见 [样例说明](util/test/metal/captures/ue-lumen-nanite-vsm-720p/README.md)。

## Metal 图形能力

“已实现”表示存在相应 API 路径和定向原生测试，不代表覆盖所有应用、设备和合法 API 组合。

| 类别 | 当前适配能力 | 范围与边界 |
| --- | --- | --- |
| 捕获与重放 | Metal device、command buffer/encoder、API 事件与 marker、帧资源、事件选择及重放重置。 | 保留统一资源预算和真实恢复、生命周期检查。 |
| 光栅图形 | Render pass、附件 load/store、深度/模板、混合、viewport/scissor、直接及已支持的间接 draw、MSAA/resolve。 | 保留 API/设备合法性检查；不提供通用 Pixel History。 |
| Compute | 原生直接/间接 dispatch、当次 GPU 参数、barrier/fence 与资源绑定。 | shader 运算由 Native 执行；必需地址与依赖仍须恢复。 |
| 纹理与缓冲区 | Shared/Private 初态、buffer/texture 拷贝与 view、mip/array/cube/volume，以及已支持格式族。 | 验证布局、范围和预算；未认证所有压缩或平台专有格式。 |
| Heap 与动态绑定 | Placement/backing、typed descriptor/argument 绑定、指针重定位、生命周期与提交跟踪。 | 不透明地址需要真实身份与布局信息；新捕获的背景 alias 恢复仍有已知缺口。 |
| 高级几何 | 原生 tessellation、object/mesh 阶段的捕获、重放与检查路径，配有定向样例。 | 受设备与特性约束，不等于所有 UE Nanite 场景均已认证。 |
| Compute 光追 / ray query | AS build/初态、refit/copy/compaction、几何与实例输入、函数表、compute 阶段光追及转换后的 inline query 路径。 | 能力开关跟随原生设备；新候选的整帧输出验收单独进行。 |
| 调试界面 | 纹理、buffer、资源、Pipeline State、mesh、usage/绑定与 shader 检查。 | 可选 shader 访问展示允许 partial/unknown。 |
| Shader 工具 | 捕获的 MSL/AIR、受约束的 Edit/Apply/Remove，以及打包的 MSL/HLSL/GLSL 重建预览。 | Apple 编译依赖 Xcode；重建和编辑有已记录的 ABI/子集限制。 |

**当前不提供：** render 阶段光追（`supportsRaytracingFromRender` 保持 false）、AS 内部查看、shader 单步调试或 Pixel History。详见 [shader 工具限制](util/shader_tools/README.md)、[光追开启范围](docs/metal-replay/RAYTRACING_ENABLEMENT.md)与 [测试记录](docs/metal-replay/TEST_MATRIX.md)。

## Unreal Engine 集成

项目级 **RenderDoc Metal Capture** 插件提供路径设置，以及视口 **Capture Metal Frame** 按钮 / Tools 菜单入口。

1. 在 [Release 页面](https://github.com/crossous/renderdoc-metal/releases/tag/v1.46-metal.1)下载 UE 插件，或将 [`util/ue/RenderDocMetalCapture`](util/ue/RenderDocMetalCapture)复制到 `<项目>/Plugins/RenderDocMetalCapture`。
2. 在 Plugins 中启用 **RenderDoc Metal Capture**。源代码插件应使用对应 UE 版本重新构建；本次集成按 **UE 5.8.3 / macOS arm64** 构建。
3. 打开 **Project Settings → Plugins → RenderDoc Metal**。填写 **RenderDoc application (.app)**，或填写 **RenderDoc library (.dylib)**。两者同时填写时，优先使用 dylib。
4. 开启 **Automatically attach on editor startup**，重启 UE。设置按用户和项目保存；默认不对 commandlet 或 Null RHI 运行进行附加。
5. 点击视口 **Capture Metal Frame**，或选择 **Tools → Capture Metal Frame**。文件保存在 `<项目>/Saved/RenderDocMetalCaptures`，可在 RenderDoc Metal 中打开。

| 设置项 | 示例 |
| --- | --- |
| Application | `/Applications/RenderDocMetal.app` |
| Library，可选 | `/Applications/RenderDocMetal.app/Contents/lib/librenderdoc.dylib` |
| Matching MetalRHI provider，高级选项 | 基于完全相同 UE 版本构建、带资源地址发布信息的 `libUnrealEditor-MetalRHI.dylib`。 |

**为什么需要启动时重新执行？** 当前后端依赖 dyld interposition。独立原生实验确认，`dlopen` 虽能取得 `RENDERDOC_GetAPI`，但不会 hook `MTLCreateSystemDefaultDevice`；随后加载 Metal 调用模块也一样。因此插件检查路径后，会在 **RHI 初始化前自动重新执行同一编辑器进程一次**，设置启动注入。原启动参数和已有注入库均被保留，不会额外打开第二个编辑器，也不修改引擎安装；单次保护防止重复重启。

普通直接 Metal 捕获与 UE bindless/转换光追负载需要的元数据不同。后者可能需要完全匹配 UE 版本的 [MetalRHI descriptor/address provider](util/ue/metal_provider/RenderDocMetalDescriptorProvider.h)及 [准备工具](util/ue/prepare_ue_metal_provider.py)。插件不会伪造缺失的 GPU 指针布局，单独安装插件也不意味着任意 UE/Lumen 截帧已经可用。构建、自动附加验证与剩余集成范围见 [插件指南](util/ue/RenderDocMetalCapture/README.md)。

## 构建与验证

开发环境需要 Xcode/Metal tools、CMake/Ninja、Qt 5、Python 开发头文件和固定版本 shader tools。使用者可以直接下载应用。

```sh
# 开发构建；可将中间文件定向到外置硬盘。
bash util/buildscripts/scripts/build_metal_dev_macos.sh

# 定向原生光追验证：先关闭 UE/qrenderdoc，GPU 测试串行执行。
bash util/buildscripts/scripts/test_metal_ray_basics_macos.sh
```

构建配置、哈希和真实测试范围见 [Metal 开发文档](docs/metal-replay/README.md)与 [Release 记录](docs/metal-replay/RELEASE_2026-10-08.md)。构建成功、GPU 执行完成、输出比较、整体验收分别记录，历史版本通过不能直接计为新二进制通过。

## 致谢与许可证

RenderDoc 与本 fork 的代码采用 [MIT](LICENSE.md)许可证。感谢 **Baldur Karlsson 和上游贡献者**。打包的 Qt、Python 与 shader processors 保留各自许可证；处理器源码与许可证随应用提供。发布包不包含 UE，引擎仍遵循 Epic 的许可证。

Metal/macOS 问题请提交到 [本仓库 Issues](https://github.com/crossous/renderdoc-metal/issues)，附上版本、设备/系统、复现步骤和可以共享的截帧。通用调试器操作可参考 [上游 RenderDoc 文档](https://renderdoc.org/docs/)。
