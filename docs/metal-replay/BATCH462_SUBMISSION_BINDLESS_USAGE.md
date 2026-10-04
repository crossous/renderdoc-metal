# B462：同一 UE 的 GBufferA Usage 与提交时刻对齐

2026-10-04。继续使用原捕获 `build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`，SHA-256 `c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`。没有重新截帧，没有提交或推送。保留此前工作树改动。

## 根因与修复

用户指出的 4036 CS image/SSBO 确实是错误 Usage，不是 Metal API 的限制。该事件是 `FilterTranslucentVolume`，实际 shader 输入/输出是 3D 半透明光照体积，没有 GBufferA。

发现一个直接根因和一条同类误报路径：

1. `AddValidatedSourcedComputeUsage` 将 descriptor 验证中的资源闭包记成 CS_RW。这个集合证明资源有效、驻留及生命周期安全，不能证明 shader 实际访问。移除这条 Usage 路径，保留 preflight 验证集合和 gate。仅移除它后，4036 的 GBufferA 条目仍在；因此没有把这一步当作问题已解决。
2. B460–B461 在 `AddAction` 编码时解析 uniform bindless 索引。UE 的 Shared 上传缓冲区会在编码后、提交前更新，因此加载阶段读取了旧值。4036 的 buffer 6154、offset 55568/55576/55584：编码时为 1934/1936/1949，实际提交/选中事件为 1297/1298/1299；输入对应 1166/1197。旧索引错误指向 GBufferA 等资源。

现在保存每个 action 的 pipeline 和 inline pointer 快照，按 command buffer 收集待解析事件。加载 `commit` 时，在现有 CPU submission snapshot 恢复之后、原来的 native commit 之前，CPU-only 解析该提交的 uniform binding。随后原有 submission bake 统一重排 EID。不增加 GPU 提交、读回、等待或改变 native 执行顺序；GPU-dependent 未知值仍不从旧 CPU 数据猜测。

这与 D3D12/Vulkan 区分资源声明、shader binding 和实际提交的原则一致，复用现有 Metal 提交快照。不是按事件编号删除 4036，也不读取整帧结束时的一份 global buffer 去冒充所有事件。

## 同一 UE 的 Usage

尚未访问任何事件时，GBufferA（ResourceId 12323）已有以下九个条目：

| EID | Usage | 所属工作 |
| --- | --- | --- |
| 3415 | Clear | GBuffer pass load clear |
| 3509 | FB Color | Basepass |
| 3596 | CS image/SSBO | Nanite compute 写入 |
| 3612 | CS image/SSBO | Nanite compute 写入 |
| 3739 | FS Resource | ScreenSpace AO Setup |
| 3824 | FS Resource | ScreenSpace AO |
| 3928 | FS Resource | BatchedLights |
| 4096 | FS Resource | SkyLightDiffuse / DistanceFieldLighting |
| 4482 | FS Resource | ReflectionEnvironment |

逐个选择七个 shader 事件，公共 Pipeline State 中相应阶段 RO/RW 描述符都包含 GBufferA。选择 4036 后，RO/RW 描述符和 Usage 都没有 GBufferA；访问事件前后九个条目完全相同、排序唯一。证据 `build-macos-debug/metal-usage-audit/ue-usage-consistency.log`。

B460–B461 报告中的 4036 CS_RW 条目以及“3928 必须选中才补入”的解释被本批结果纠正。历史日志保留，不能继续把那次 Usage 列表当正确基线。

## 4036 的纯色体积

实际输入：Ambient 13994、Directional 13995、BlackVolumeTexture3D 64；输出：Ambient 13998、Directional 13999，以及 shader 的 SpecularDirection placeholder 14000。前两对均为 64×64×64 RGBA16Float。

全部 262144 个 voxel 的原生 GPU 数据检查，而不是只看 Slice 0：

- Ambient 全体为 `(0.50830078125, 0.424072265625, 0.3623046875, 1)`。
- Directional 全体为 `(-0.0880126953125, 0.576171875, 0.484130859375, 0)`。
- 无 NaN/Inf；对应滤波输入/输出逐字节一致；两次 EID0 reset 一致。

UE `TranslucentLighting.cpp` 的 `FFilterTranslucentVolumeCS::FParameters` 没有 GBufferA。`TranslucentLightingShaders.usf` 的 spatial filter 对八个邻域采样求平均，常量输入应保持常量输出。`TranslucencyVolumeInjectionCommon.ush::AccumulateSHLighting` 存储球谐系数，Directional 的负数不是非法法线。

因此，这次滤波的纯色结果符合捕获中的常量输入，不支持“4036 错误修改 GBufferA”的判断。这不是所有场景体积都应纯色的声明，也不是独立证明 UE 更早的光照注入与运行中原帧逐像素一致。

另修正 direct dispatch 的 action 名称：原先深度写死 1；4036/4046 现在显示 `dispatchThreadgroups(16x16x16, 4x4x4)`。实际 native dispatch 和 action 数值此前已经是这六个正确维度。

## DX12/Vulkan 对照与 barrier

本地官方 API 实现对照：

- D3D12 `d3d12_command_list_wrap.cpp` 对 transition/aliasing/UAV barrier 中明确列出的资源记录 Barrier。
- Vulkan `vk_cmd_funcs.cpp` 的 legacy/sync2 barrier 对明确的 buffer/image barrier 记录 Barrier，不把无资源 ID 的全局 memory barrier 枚举成每个纹理的使用。
- Metal compute/render `memoryBarrierWithResources` 原先漏记 Usage，现对每个明确列出的资源在准确 API EID 记录 Barrier。T42/T43 从结构化 chunk 的资源 ID 和 event.chunkIndex 独立建立 oracle，验证 scope barrier 不给无关驻留资源虚构 Usage。

当前原 UE XML 有 82 次 `memoryBarrierWithScope`，没有 `memoryBarrierWithResources`，这些 scope barrier 不指名 GBufferA。不能补出 Windows 截图中的 D3D12 resource-state transition 行。Tracked Metal 资源的隐式 hazard 处理也不是 D3D12 layout/state 转换事件。

Windows `Downloads/testrdc.txt` 和当前 Metal 实际 pass graph 不同。Windows GBufferA 多次读取包括 StochasticLighting、VSM GeneratePageFlagsFromPixels、Lumen Reflections GenerateRays/TraceScreen、ScreenProbeGather、BRDF_PDF、VirtualShadowMapProjection；当前 Metal action tree 没有 Lumen/ScreenProbeGather/VSM 这些 pass，使用 SSAO、DistanceFieldLighting、ReflectionEnvironment 路径。分辨率和 indirect light instance count 也不同。

进一步找到当前候选 original 的实际捕获 session：`Testproj/Saved/RenderDocMetalSessions/20261003-085847/manifest.txt` 指向 `testproj-20261003-085846-931499`，即本候选 results.json 中的 original；`ue-editor.log:1894–1907` 在捕获之前确认 `r.Shadow.Virtual.Enable=0`、`r.Lumen.DiffuseIndirect.Allow=0`、`r.Lumen.Reflections.Allow=0`、`r.ShadowQuality=0`、`r.VolumetricFog=0`、`r.SSR.Quality=0` 已执行。随后 2083/2093 行记录同次捕获开始/保存。这是之前给本机低负载测试采用的关闭项，不是由 Metal API 能力限制推断出来。项目 DefaultEngine.ini 虽然设置 Lumen method=1，但这些运行时覆盖仍关闭了相关 pass。不能把这份低负载捕获当作与 Windows 全功能帧等价的验收，也不能给已关闭的 pass 虚构 Usage。

## 验证与复跑

证据总目录：`build-macos-debug/metal-usage-audit/`。首次 probe 的输出目录未创建而 exit 9，保留为 `previsit-only-exit9.log`；建立目录后完整重跑通过，以下 UE 结果引用子目录 `submission-ue/events.log`，不把该失败尝试算成通过。

定向终端已通过：

- 新增 after-encoding/before-commit Shared uniform 更新场景；旧冻结库 `6362ccc0` 在 pre-visit image 0 Usage 错归到第二个 dispatch，exit 4；修复库正确归属两个 dispatch，六次 reset 和 exact GPU float bytes 通过，未访问第三张驻留纹理零 Usage。
- CS/Task/Mesh、8 个 malformed 输入、已有 mesh/ray AS 定向脚本通过。
- T42 compute / T43 render resource Barrier 的精确 EID、scope exclusion、两轮重置通过。新断言加入全量脚本。
- 最终候选重新验证 late-uniform、Task/Mesh 和两种资源 barrier 通过。

真实 UE 已通过：3612/3928/4036/4046 各两次 reset；GBufferA 3612/3928/4036/4046 SHA-256 保持 `33d636f6cc68eebcce5813669fe0a81f45d60d595c4770087b856706a459d55f`。4036/4046 全体积 readback 一致。九个 Usage 的七个 shader 事件逐项与选中资源吻合，4036 排除验证通过。

最终候选库、bundle 库、冻结全量库 SHA-256 均为 `a881aab32910a531631781845b908ee58ed6680d8129f34310baa5f4fa3ec857`。

本候选固定库全量通过：308 captures、7786 malformed cases、3080 lifecycle opens，resident growth 0 bytes，exit 0。`full-build/start-hash.log` / `end-hash.log` 完全一致；日志 `full-regression.log`。全量后同一原 UE 的九个 Usage / 七个 shader 事件再次逐项一致，4036 排除通过，访问前后列表相同。正常 OpenCapture 和两次整帧 EID0 reset 的三份 900×640 呈现图 SHA-256 都保持 `fc5f3afe33b3ec7a523d447d59d0b517337d4c8f8867f6d863f99f0ab5eb4f61`；证据 `post-full-usage.log`、`post-full-images/events.log` 和 `hashes.log`。实际UI通过4036命名体积输入/输出、正确3D dispatch维度、3928 GBufferA场景法线；ResourceInspector显示8分组/9事件且无4036，双击3739跳转ScreenSpace AO Setup成功。缩略图右键/外点击关闭未验收：CUA右键AX无菜单，坐标报windowNotFoundAtPosition。

GUI留在GBufferA资源检查器的Usage列表。右键未验收不影响以上公共接口和资源检查器列表/跳转通过，但不能记录成菜单鼠标验收通过。

复跑定向（先退出 qrenderdoc/UE，GPU 串行）：

```sh
bash util/buildscripts/scripts/build_metal_dev_macos.sh
bash util/buildscripts/scripts/test_metal_bindless_stages_macos.sh
clang++ -std=c++17 -DRENDERDOC_PLATFORM_APPLE -I. \
  util/test/metal/metal_usage_consistency_replay.cpp -Lbuild-macos-debug/lib -lrenderdoc \
  -Wl,-rpath,"$PWD/build-macos-debug/lib" -o build-macos-debug/metal-usage-audit/usage-consistency
build-macos-debug/metal-usage-audit/usage-consistency \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc 12323 4036
```

边界仍是可证明的 uniform AIR bindless 解析；没有宣称任意 per-invocation 动态索引反馈已与 D3D12/Vulkan 全面平齐。
