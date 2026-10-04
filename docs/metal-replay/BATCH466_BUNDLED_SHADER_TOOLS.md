# B466：fork 内置 Metal shader processors

2026-10-05。按用户要求把处理器作为本 fork 的功能随 macOS app 分发，自动接入公共
Shader Viewer / Pipeline State Edit / Compiler 选择，不要求 release 用户配置外部路径。
保留 B440–B464 和其他任务的工作树改动，本轮没有提交或推送。

## 已实现

- `Contents/Resources/shader-tools/` 内置 metal2vulkan、SPIRV-Cross、spirv-val 和 Python
  适配器。工具使用 app 相对位置，已有用户处理器不被覆盖；内置项不写入持久配置，
  搬动 app 后重新计算路径。全新配置自动出现七个内置处理器。
- Apple AIR 反汇编、AIR/MSL 编译依赖 Xcode Metal 工具链，符合用户允许的 Xcode 例外。
  Rust、Cargo、Homebrew 和独立 LLVM 只属于开发环境，release 用户不需要安装或配置。
- View 提供 AIR，以及重建的 MSL/HLSL/GLSL 预览。Edit 有源码时优先捕获 debug MSL；
  没有源码时提供 AIR、受限重建 MSL、MSL replacement 模板。Compiler 保留 Builtin，
  同时自动提供 Apple MSL/AIR → MetalLib 编译器。
- 高层语言预览工具不进入 Edit 菜单。当前没有 HLSL/GLSL → Metal 编译链，不能把它们
  作为可应用替换来宣传。AIR 是 LLVM 中间表示，也不是 Apple GPU 机器 ISA。
- 所有原生工具来自固定官方源；校验下载源包、Cargo.lock、安装文件 hash、架构和动态
  库依赖。三工具只依赖 Apple 系统 dylib。release 的 CMake 配置在缺少工具分发时失败，
  打包步骤即使 GUI 没重新链接也执行。许可证、对应源码包、Rust 依赖和运行库 notices
  随 app 附带；metal2vulkan 保持独立的 LGPL 可执行程序，可按附带说明重建/替换。
- 每次调用使用私有 scratch、直接 argv；24 秒总 deadline、翻译单步最多20秒、采样
  进程组500MiB RSS guard。总 deadline 在公共 UI 30 秒等待前结束，finally 清理子进程
  与临时文件。失败先移除旧输出，不会将过期 shader 当成成功结果。

固定来源：
[metal2vulkan](https://github.com/steelbrain/metal2vulkan/tree/43c46ac8a24adf1a6e872b8a52c706ec9614fad0)、
[SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross/tree/vulkan-sdk-1.4.357.0)、
[SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools/tree/vulkan-sdk-1.4.357.0)。
未修改上游 Rust 转换器；构建缓存位于外置盘，app 工具资源约42MiB，包含源码与 notices。

## 明确的功能限制

链路是 Apple metal-objdump → LLVM AIR → metal2vulkan → Vulkan SPIR-V → SPIRV-Cross。
SPIRV-Cross 本身不读取 AIR。上游 alpha 转换器可能改变 binding、argument buffer ABI、
function constants、阶段接口和 compute dispatch payload；SPIR-V 验证成功不保证 Metal
shader 替换等价。生成预览头部明确写 VIEW ONLY。

实测带 function constant 的 fs 原值0.25，在重建 MSL 中变成默认值0。因此重建 MSL 的
Edit 入口严格限于无资源、无 function constants、无 input varyings、无 imageblock
附件的 fragment shader；其他情况拒绝，并指导使用 AIR 或捕获源码。已验证一个纯
literal fragment 的 MSL 重建、编译、真实替换与恢复。这个门禁不是任意 shader 等价证明。

**当前 UE 的 EID3928 光照 FS、3612 Nanite CS 尚不能高层反编译**：typed emitter 分别
拒绝 LLVM `fptoui`、`fptosi`，返回 FALLBACK、exit1、不产生输出。没有替换这两个
shader 为错误的重建代码。AIR 编辑与 Apple AIR 编译可用，真实 UE 验证见下。

## 定向终端验证

证据：`build-macos-debug/metal-shader-processors/`。

`processor-directed-final.log` PASS：真实 Apple 编译的微型 shader，AIR/MSL/HLSL/GLSL
输出、重建 MSL/AIR 再编译、原入口保留、function constant 的预览提示与 Edit 拒绝、
无效源码错误和 stale-output 清理；把整套工具搬到含空格路径并设置 PATH=/usr/bin:/bin
后输出一致；验证 app manifest 全部文件 hash；单步及总 deadline 均杀掉 sleep worker。
日志 RSS 为采样值，短进程的0不等于实际没有分配内存。

`literal-replay.log` 在本轮最初06ae2318后端通过；后续匹配当前 API 的固定后端验证记录
在 `literal-final-current-replay.log`。检查实际 reflection 已切换为新 shader，读回全部
颜色附件和该阶段可写纹理，以及可写 buffer 前64B；两轮 EID0 reset、应用、删除替换，字节与原基线一致。

`ue-3928-preview.log` / `ue-3612-preview.log` 验证实际 UE 原 MetalLib 的高层转换失败，
只记预期拒绝，不记高层反编译成功。`ue-3928-final-compile.log` /
`ue-3612-final-compile.log` 验证最终打包适配器的 AIR 编译通过。
Python py_compile、shell bash -n、git diff --check 通过；GUI 构建与打包通过。
release-missing-tools.log 验证 release 配置缺少工具时按预期失败，避免发布缺工具的 app。
release-with-tools.log 验证同一 release 配置指定完整工具分发后成功生成构建文件。

## 同一真实 UE 验证

原捕获：
`build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`，
SHA256 `c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`，原文件未改。

本轮最初固定06ae2318的 `ue-3928-replay.log`、`ue-3612-replay.log` 通过：分别读回
614400B、1536000B 的写资源数据；AIR 经外部 Apple compiler 生成 MetalLib，再由公共
BuildTargetShader/ReplaceResource 应用；两轮实际 reflection 切换、EID0 重置后逐字节
一致，RemoveReplacement 后恢复一致，无 fatal error。这不是只证明 compiler exit0。

期间“分析 Metal 独有调试功能”任务更新了共享 API/后端。不能用新头文件编译的测试
客户端搭配旧06ae库：追加尝试 `replay-pinned` 无有效事件结果且退出139，保留其日志和
sample，不计 PASS。后续固定一份与当前 API 匹配的后端，单独记录在 `current-abi/`；
最终复核结果另列于本文结尾。旧验收结果不冒充新组合的结果。

## 全量回归

本轮处理器和打包改动不修改 GPU backend，不默认重复全量。B464 的冻结06ae2318全量
`build-macos-debug/metal-shader-edit/full-final-regression.log` 已通过308 captures、
7786 malformed、3080 lifecycle，growth1343488B。该结果仅属于其记录的冻结版本。
其他任务随后改变的 Metal backend 是否完成全量，见该任务的独立记录；本轮不宣称
当前共享库因工具定向测试通过而自动获得全量验收。

## 实际 UI 验证

实际启动更新 app，原用户工具保留。公开 PersistentConfig.Load 从新文件路径创建配置，
自动获得七个内置工具；Save 后 JSON 中 bundled 项为0，不保存安装路径。
`ui-auto-tools.json` 记录原配置与新配置的工具名称。

微型 air_capture EID11 FS：实际点 Edit 打开 AIR；Compiler 下拉显示
Apple Metal compiler (AIR) 和 Builtin，点 Apply 后 Edited Shader Active；实际点击
Remove changes 后 Original Shader Active。退出选择 No，不保存临时编辑。

公共 View 页的下拉实际列出三种高层预览。用现有 MiniQtHelper.SelectComboOption
在 UI 线程选择 MSL、HLSL、GLSL，等待实际查看器异步加载后，逐个读取 AX 和屏幕，
确认对应选择状态与带 VIEW ONLY 提示的真实源码。最初在构造 ViewShader 的同一 REPL
语句中选项尚未载入（GetComboCount=0）；待载入后切换成功。鼠标选择下拉的自动化未
完成，不把公开 Qt 选择 API 的验证描述为鼠标操作。实际 Apply/Remove 按钮操作已通过。
没有代替用户人工验收，也没有重试此前挂起的原生 Save 面板。

## 开发与复测入口

完整说明见 [Shader Tools](../../util/shader_tools/README.md)。

```sh
# 开发者构建 native 工具；release 用户不执行这些命令。
bash util/buildscripts/scripts/build_metal_shader_tools_macos.sh /path/to/tool-cache
RENDERDOC_METAL_SHADER_TOOLS_ROOT=/path/to/tool-cache \
  bash util/buildscripts/scripts/build_metal_dev_macos.sh
# 关闭 UE/qrenderdoc 后，串行工具/GPU定向。
bash util/buildscripts/scripts/test_metal_shader_processors_macos.sh
# 同一UE外部AIR编译产物的实际替换；replay使用同批API头文件和库编译。
build-macos-debug/metal-shader-processors/replay \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc \
  build-macos-debug/metal-shader-processors/ue-3928-final.metallib Main_0000aee4_f632e2ef 3928
build-macos-debug/metal-shader-processors/replay \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc \
  build-macos-debug/metal-shader-processors/ue-3612-final.metallib Main_00012ae4_d2df2e04 3612 cs
```

## 最终匹配 API 的复核

固定库 SHA256 `48afd5d3fdfba858f0dc4a145b2746eae8b649774f691ccb8ccd16744806328b`，
`current-abi/start-hash.log` 与 `end-hash.log` 一致。
`literal-final-current-replay.log`、`ue-3928-final-current-replay.log`、
`ue-3612-final-current-replay.log` 全部 PASS：三者均实际应用新 reflection、两轮
EID0 reset 后写资源字节不变，删除替换后恢复，无 fatal。当前共享库/bundle 也为这个
SHA；该库包含另一任务的新增 backend，不能沿用06ae的全量结果。
三个本轮 probe 顺序执行；另一任务在后半段启动其微型 UI 验证，不能称机器全局 GPU
独占。最终 UI 保留另一任务的页面，未抢占或重新载入 UE。

编号使用 B466，避开另一任务已经使用的 metal-features-b465。
