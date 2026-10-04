> 后续纠正（2026-10-04）：本批记录的 4036 CS_RW 是错误 Usage，3928 并非必须依赖选中后的 GPU 索引解析。B462 发现加载期间误读了提交前尚未更新的 Shared uniform，并移除 residency 闭包的误报；修复后打开原 UE 即有九个正确事件。历史日志保留；当前结论见 [B462](BATCH462_SUBMISSION_BINDLESS_USAGE.md)。

# B460–461：Nanite UAV Usage 与 Metal shader 阶段检查

当前候选（2026-10-04）在原 Testproj 截帧上修复 Nanite compute 对 GBuffer 的写入被遗漏的问题，并扩展现有 uniform AIR bindless 解析到 Compute、Object/Task、Mesh。没有修改原截帧、提交或推送。

## 问题与对照

3509 Basepass 的 GBufferA 与 3928 BatchedLights 输入不同并非顺序错误：3596、3612 NaniteBasepass 的 compute dispatch 在中间修改了 GBuffer。旧解析只遍历 VS/FS，并只识别 sample/read，漏掉 compute 与 texture write，因此 Usage 与 Texture Viewer Outputs 都缺少这两次写入。

本地官方对照源码：

- `renderdoc/driver/d3d12/d3d12_replay.cpp`：GetDescriptorAccess 合并静态访问与 shader feedback。
- `renderdoc/driver/d3d12/d3d12_shader_feedback.cpp`：Dispatch、MeshDispatch、Drawcall；覆盖 CS、VS、HS、DS、GS、PS、AS/MS。
- `renderdoc/driver/vulkan/vk_shader_feedback.cpp`：同样覆盖 compute、传统 graphics、Task/Mesh。
- 两个 feedback 入口均不包含 RayDispatch，不能把普通 RT dispatch 描述成已经具有动态 bindless feedback。Metal 的 compute ray kernel 仍应显示可确认的 shader 输入，包括加速结构。
- Apple `MTLRenderPipelineReflection.meshBindings/objectBindings` 使用现代 `MTLBinding.isUsed`，不是旧式 `meshArguments/objectArguments`；数组长度仅在支持该 selector 的 binding 上读取。
- Metal 原生 tessellation 的控制计算走 CS、后细分 vertex function 走 VS；不添加不存在的原生 HS/DS/GS 标签。Metal Shader Converter 的几何/曲面细分若映射为 Object/Mesh，则走对应真实阶段。

Apple 主参考：[Mesh bindings](https://developer.apple.com/documentation/metal/mtlrenderpipelinereflection/meshbindings)、[Metal Tessellation](https://developer.apple.com/library/archive/documentation/Miscellaneous/Conceptual/MetalProgrammingGuide/Tessellation/Tessellation.html)。

## 实现

- AIR 识别 read/sample/write/texture atomic；同一阶段、表与槽的 SRV/UAV 合并为一个可写访问，不把驻留当访问。
- 支持 UE byte GEP 与原生 MSL scalar/pointer/struct GEP；每个 metallib function 的 AIR module 独立解析，避免重用其他模块的 metadata ID。
- Compute、VS、FS、Object、Mesh 保存各阶段 inline 源指针、buffer offset 和每个 EID 的绑定快照。公共 PipeState 增加 Task/Mesh shader、buffers/textures/samplers，以及 VS/FS/CS acceleration structure 输入。
- 全帧 Usage 在实际 draw/dispatch 处加入能够由 CPU 元数据证明的访问，再按已有提交事件映射重排；加载期间不提交 GPU readback。GPU 写出的索引不在 CPU 上猜测，选中事件后再验证 native table handle。
- coverage 66 只扩展 sourced Object/Mesh 绑定与有界 direct mesh dispatch；保留资源 closure、inline layout、native handle、提交、alias、生命周期和工作量检查。该版本必须包含 mesh dispatch；旧 coverage 的接受范围没有自动扩展。
- Mesh PSO 恢复 color attachment format 和完整 buffer 类型反射；记录现代 Task/Mesh 直接绑定。VS/FS 的 direct writable texture/buffer 与多路 CS 资源按实际 SRV/UAV 分类。
- Pipeline State 增加 Task/Mesh 页面、正确 shader 查看入口、资源/inline constants/sampler、AS 输入、VS/FS writable 列表与 HTML 导出。Buffer 行打开真实 offset/size，AS 不再误放入 texture 列表。

这里的 bindless 仍是 **可证明的 uniform AIR 地址解析**，不是 DX12/Vulkan 的逐 invocation shader instrumentation。非 uniform 索引、未知表达式、未捕获源指针、未知结构布局及 linked intersection/callable 内部的访问不冒充已解析；这一限制仍需单独实现 GPU feedback 才能消除。

## 定向终端

证据根目录：`build-macos-debug/metal-nanite-usage/`。

- `directed-script/`：可重复脚本通过；CS 两个 dispatch 改变 inline 源指针 offset，验证两个精确 RW 输入/输出、第三张仅驻留纹理零 Usage、六次 EID0 回退和 exact RGBA32Float GPU bytes。
- 原生 Object/Mesh：先 MS 写 image0，再 TS 写 image1 / MS 读取 image0；加载后、尚未访问事件时已分别产生 MS_RW / TS_RW / MS_Resource Usage，第三张驻留 image2 没有 Usage。两个 EID 各两次 reset；纹理 exact float 和 2×2 framebuffer bytes 均吻合。
- 8 个新异常输入（coverage、阶段、layout、源资源、offset、grid）均拒绝且无崩溃。
- T79/T80/T83/T85 的公共 Task/Mesh shader、buffer、texture、sampler 检查通过。
- T144/T244/T247/T250 的 compute ray kernel AS 描述符与对应 CS Usage 检查通过；原有 ray hit/miss/native buffer oracle 也通过。
- T28/T29/T47/T48/T66 与上述 mesh/ray 捕获的原有 output smoke 通过，包含原生 tessellation 回退。
- Parser 保持未知动态索引未知；新增 write、atomic、module isolation 与实际 compiled MSL struct GEP 的检查通过。

复跑命令（先关闭 UE/qrenderdoc；使用已构建库，GPU 串行）：

```sh
bash util/buildscripts/scripts/build_metal_dev_macos.sh
bash util/buildscripts/scripts/test_metal_bindless_stages_macos.sh
# 加末尾参数可再检查这份 UE 捕获的固定 EID/GBufferA：
bash util/buildscripts/scripts/test_metal_bindless_stages_macos.sh \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc
```

## 真实 UE

原 capture SHA-256：`c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`。

`final-ue/` 检查 3509、3596、3612、3928 各两次 reset；`acceptance-ue/` 在最后候选再次检查 3612、3928。全部通过。

全量后重新编译公共接口 probe，再用同一原 capture 检查 3509、3596、3612、3928 各两次 EID0 reset；`post-full-ue/events.log` 通过，逐项 GBufferA hash 与下面基线一致。3596/3612 的 CS_RW Usage 与选中 3928 后的 FS Resource 已确认，action tree chronological / usage sorted_unique 均通过。`post-full-images/` 的正常打开与两次整帧 reset 通过，三份 900×640 呈现图 SHA-256 均为 `fc5f3afe33b3ec7a523d447d59d0b517337d4c8f8867f6d863f99f0ab5eb4f61`。证据汇总 `post-full-hashes.log`；原 capture、库和 bundle 库 hash 保持不变（`post-full-candidate-hash.log`）。

- pre-visit GBufferA Usage：3415 Clear、3509 FB Color、3596 CS_RW、3612 CS_RW、4036 CS_RW、4096 FS Resource。3928 的部分 descriptor 索引需要选中后完成 GPU 依赖，因此其 FS Resource 在实际选中后补入；不从未完成的 private/GPU 数据猜值。
- 3612 具有四个真实 writable textures，其中包含 `GBufferA` ResourceId 12323，Texture Viewer Outputs 通过公共接口获得。
- GBufferA 3509：`6fcd78b5894f358974dda294829a4e94d0f4f22a3e22dfb9e4f59d44441ca53f`。
- 3596：`002a5b0290b05cc937e83be1cf651afbf3cf79355d45fe493bba7a1b67065c61`。
- 3612 与 3928：`33d636f6cc68eebcce5813669fe0a81f45d60d595c4770087b856706a459d55f`。
- 相对修复前的同 EID GPU bytes 不变；改动恢复的是实际访问、输出和 Usage 的可见性。`ue-acceptance-hashes.log` 保存逐事件证据。

最后候选库与 bundle 库 SHA-256 均为 `6362ccc012f8e8358429457258ecd8ffa45de447f26188bf4854830f7b431212`。

## 全量与 UI

验收候选固定库全量通过：308 captures、7786 malformed cases、3080 lifecycle opens，resident growth 2392064 bytes，exit 0；库 start/end hash 一致。证据为 `full-regression.log`、`full-build/start-hash.log` 和 `full-build/end-hash.log`。使用 `RENDERDOC_METAL_LAST_TEST=312` / `RENDERDOC_METAL_SKIP_BUILD=1` 运行 `test_metal_replay_batch35_38_macos.sh`，关闭 GUI，GPU 串行。B459 的历史全量通过不作为本候选通过证据。

助手实际 UI 已正常打开原 UE 捕获，选择 3612 后 Compute Pipeline State 显示四个 Read-Write resources，Texture Viewer Outputs 显示 SceneColor、GBufferA/B/C，选择 GBufferA 后显示场景法线；选择 3928 后 Inputs 显示六个命名纹理。无 replay fatal error。原生 Task/Mesh 小捕获也正常打开，选择 30 后识别 Object/Mesh Pipeline State 和 fragment entry point。

Task/Mesh 自定义阶段按钮的鼠标点击及资源右键菜单未完成 UI 验收：CUA 坐标操作返回 `noWindowsAvailable` / `windowNotFoundAtPosition`，AX 与事件搜索仍可操作；未以终端公共接口通过替代这两项 UI 通过。Task/Mesh shader、entry point、RO/RW/sampler/AS 公共接口的终端验证单独记在定向结果中。完成 UI 读取后已正常退出 qrenderdoc，再串行运行全量。

全量与 UE 终端复核之后再次正常打开原 UE capture，选择3612、Texture Viewer Outputs、GBufferA；实屏可见场景法线及 Timeline 中新增的 CS read/write 标记，状态栏为 `loaded. No problems detected.`，Overlay None / RGB。最后将界面留在这个输出，方便用户核对；上述未验收鼠标操作仍单独保留，不以此次正常打开覆盖其状态。
