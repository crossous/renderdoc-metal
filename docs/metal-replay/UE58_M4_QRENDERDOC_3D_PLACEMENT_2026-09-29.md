# UE 5.8.3 新截帧、qrenderdoc 构建与首个回放阻塞（2026-09-29）

> 后续 Private 间接绘制及 UE 帧超时见
> [本轮记录](UE58_M4_PRIVATE_INDIRECT_2026-09-29.md)；下方为先前停点。

## 12:22 后续定位与安全停点

原先的 3D placement 拒绝已在本工作树的定向实现中越过；230 个截帧
placement descriptor 经原生 Metal Validation 布局探针检查，2D、array、
3D、cube 各有实际创建正例。Private buffer texture 的 97 个 descriptor
亦经过格式/对齐探针。它们是原生能力检查和逐步 CLI 定位，**不是本帧
完整回放通过**；注入正例、畸形输入和旧帧定向回归尚缺。

最后一次 GPU 回放命令与现场：

```sh
MTL_DEBUG_LAYER=1 build-qrenderdoc/bin/renderdoccmd replay --loops 1 \
  '/Users/kurogames/Documents/Unreal Projects/SocoTestProj/Saved/RenderDocMetalCaptures/UE58_frame833.rdc'
```

12:22 退出 250；`/tmp/renderdoc-metal-t312-ue833-indirect-dispatch-replay.log`
记录 `MTLResourceStorageModePrivate, which is not CPU accessible`。系统报告
`~/Library/Logs/DiagnosticReports/renderdoccmd-2026-09-29-122228.ips` 的栈为
`MTL::Buffer::contents()` →
`WrappedMTLRenderCommandEncoder::Serialise_drawPrimitives<ReadSerialiser>`，
原第 4543 行。失败点是 `MTLRenderCommandEncoder::drawPrimitives(indirect)`；
不是已证实的 compute indirect 问题，更不是 `MTLHeap::newTexture(offset)`
仍是首个阻塞。XML 静态转换文件
`/tmp/renderdoc-metal-t312-ue833.zip.xml` 含多个同类 draw chunk。

按 [跨 API 排查顺序](CROSS_API_TRIAGE.md)检查：UE 5.8.3 的
`MetalCommands.cpp::RHIDrawPrimitiveIndirect` 在 Metal 支持间接 buffer
时直接发出 `drawPrimitives(..., buffer, offset)`；未见 RenderDoc 专门适配。
RenderDoc Vulkan 的 `FetchIndirectData` 为 GPU 顺序中的间接参数安排
readback；D3D12 `ExecuteIndirect` 同样按 GPU command list 处理。
Metal 现有实现却在 CPU 编码时调用 `contents()`，对 Private buffer 非法，
且对 GPU 写入的 Shared 参数也可能读到旧值。下一步完整功能族应保留
执行点参数、输出 buffer 和事件元数据的一致性，先做原生 Metal Validation、
注入捕获、API/CLI、必要负例及旧 T13/T21 等定向回归；不能仅移除
`contents()` 并声称 Private 间接绘制已支持。

本批先在 `metal_render_command_encoder.cpp` 的普通和 indexed 间接绘制
CPU 读取之前拒绝 Private buffer，避免同类 Validation 断言；撤回本轮
未独立验证的 direct/indirect compute buffer dispatch 放行。因此当前构建
可能在该间接绘制 chunk 之前安全拒绝；停测后没有测出新的第一阻塞点。
`qrenderdoc/Code/pyrenderdoc/PythonContext.cpp` 的首次 stubs 生成错误处理
已编译，但 GUI 启动修复仍未人工复验。最终静态校验 `git diff --check`
与 `cmake --build build-qrenderdoc --target build-qrenderdoc renderdoccmd -j 8`
通过；本次 app 内嵌和 CLI 同源回放库 SHA256 均为
`319fd504dece47de6b8279bacbcba02d8ad401f6d6635a9e4723dc759d33bfba`，
qrenderdoc 可执行文件 `023ad5ba4fb1759936d1bc88c63b0e821a28a4b5feb1c2b2bc724ba5171371de`，
renderdoccmd `797afe7af3a19ede3fad26445f6fdd97fe75dbcd5469fa4504bc252fd766d2b5`。
**这次 Validation
断言之后没有再运行 UE 帧、旧帧或任何 GPU 回放。** 未运行全量压力回归、
人工 UI 或 Computer Use，累计 UI QA 增量仍为 **0**。

## 工作树与实物

- 独立工作树 `/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312`，
  基础 HEAD `e0a26f7e65e22a3890aa38a318bde51d24babf6b`；保留全部现有
  未提交改动。原 `renderdoc-metal` 目录的 4 项改动未动；本批未提交、未推送。
- 用户 UE 会话 `SocoTestProj/Saved/RenderDocMetalSessions/20260929-064750/`
  的 manifest 确认注入库为 `build-ue-debug/lib/librenderdoc.dylib`，SHA256
  `1ab4448a93b2c8f08211087bb40f2afbf72a9231d98c978918ea5425fe3b6ef9`。
  插件日志记录按钮请求及保存。
- 新帧 `SocoTestProj/Saved/RenderDocMetalCaptures/UE58_frame833.rdc`，
  14,660,781 字节，SHA256
  `472dfa48a56cbaa43b6cc97d5f82d0471263de0c0fdd47049dde166e1df6f163`。
  用户此次成功截帧，未重现先前 `EndFrameCapture` 崩溃。

## Viewer 构建

在无空格路径别名 `/tmp/renderdoc-metal-t312` 上配置 CMake/Ninja，Qt 5.15.19：

```sh
cmake -S /tmp/renderdoc-metal-t312 -B /tmp/renderdoc-metal-t312/build-qrenderdoc -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-Wno-nontrivial-memcall \
  -DQMAKE_QT5_COMMAND=/opt/homebrew/opt/qt@5/bin/qmake \
  -DENABLE_METAL=ON -DENABLE_GL=OFF -DENABLE_GLES=OFF -DENABLE_EGL=OFF \
  -DENABLE_VULKAN=OFF -DENABLE_PYRENDERDOC=OFF \
  -DENABLE_RENDERDOCCMD=ON -DENABLE_QRENDERDOC=ON
cmake --build build-qrenderdoc --target build-qrenderdoc renderdoccmd -j 8
```

首次链接 app 成功，但 qmake post-link 的 `set_plist_version.sh` 因源码路径含
空格而失败。`qrenderdoc/qrenderdoc.pro` 对脚本路径加 `shell_quote` 后重建成功。
为补齐首次失败的 post-link 元数据，对已生成的 app 执行一次
`sh util/set_plist_version.sh 1.46.0 build-qrenderdoc/bin/qrenderdoc.app/Contents/Info.plist`；
`CFBundleShortVersionString=1.46.0`、`CFBundleIdentifier=org.renderdoc.qrenderdoc`，
`Info.plist` lint 通过，app executable 为 arm64。构建日志：
`/tmp/renderdoc-metal-t312-qrenderdoc-build2.log`、
`/tmp/renderdoc-metal-t312-qviewer-rebuild.log`。

- app：`build-qrenderdoc/bin/qrenderdoc.app`
- app executable SHA256：`1e6421e816556d13f9f88e2262f01080dc13a92ffc66e6bfe70897c1c94ae3fc`
- app 内嵌 `Contents/lib/librenderdoc.dylib` 与同次构建
  `build-qrenderdoc/lib/librenderdoc.dylib` SHA256 均为
  `8f79ed3345fdceb75c8170869576c3e607eab3ff688ee6885ff7f89233db3cb5`。
  它们与用户截帧时注入的旧构建字节不同；源码来自同一工作树。

## 单次回放与首个阻塞

```sh
MTL_DEBUG_LAYER=1 build-qrenderdoc/bin/renderdoccmd replay --loops 1 \
  '/Users/kurogames/Documents/Unreal Projects/SocoTestProj/Saved/RenderDocMetalCaptures/UE58_frame833.rdc'
build-qrenderdoc/bin/renderdoccmd convert -f '<上述帧>' \
  -o /tmp/renderdoc-metal-t312-ue833.zip.xml -c zip.xml
```

CLI 退出 1，Metal Validation 已启用，安全拒绝 `MTLHeap::newTexture(offset)`。
补充只在拒绝分支执行的描述符诊断后重建并单次重放，明确首个失败资源是
`MTLTextureType3D`（type 7）的 Private 1×1×1 BGRA8Unorm placement 纹理，
offset 512；options 544、hazard 2、usage 1 均在现有允许范围。之前仅打印
format/options/hazard 容易误判。转换出的 XML 表明新帧含 230 个 placement
纹理创建：2D 150、2D array 44、3D 32、cube 4；还有多种像素格式、
非单层 mip、真实 3D 深度。现有 T117 只验证 2D 单 mip、窄格式子集。

按 [跨 API 排查顺序](CROSS_API_TRIAGE.md)查看了 D3D12 placed resource
重建路径及 UE 5.8 MetalRHI 对纹理 descriptor 的设置。UE 的 3D 类型是
合法的实际资源请求；不能只删除 Metal 回放守卫或强制按 2D 创建。后续应
先建立该纹理族的原生 Metal Validation 正例，再做注入捕获、API/CLI、
必要负例和旧 T117 等定向回归。本批未实现 3D 及其它 placement 纹理类型。

同一 `build-qrenderdoc/bin/renderdoccmd` 在 Metal Validation 下单次回放旧 UE
`UE58_frame99.rdc` 退出 0；该帧 SHA256
`8a55f0e3738d8b10255dc855e13933ef6e9ab1055f7f59831fe3cfdba4a51022`。
它可作为用户手动打开 viewer 的首个检查对象，但本批没有运行人工 UI。

本批修改：`qrenderdoc/qrenderdoc.pro` 的路径引用、
`renderdoc/driver/metal/metal_heap.cpp` 的拒绝诊断，以及本状态/接入文档。
**定向终端通过**仅指 viewer 构建、真实 UE 截帧保存证据和首个阻塞定位；
新 UE 帧回放失败。没有运行完整功能族正例/负例、旧帧回归、全量压力回归、
GUI/Computer Use 或人工 UI 验收。累计 UI QA 增量 **0**。

## 用户首次启动 viewer 的闪退

`~/Library/Logs/DiagnosticReports/qrenderdoc-2026-09-29-115009.ips` 记录
11:50:00.518 启动、11:50:01.719 捕获的 `EXC_BAD_ACCESS`，主线程栈为
`PyDict_SetItemString` → `PythonContext::GlobalInit` 的第 532 行 → `main`。
进程链接 Homebrew Python 3.14.4；栈中没有 Metal 回放调用。
`2026-09-28-213518.ips` 也在同一行崩溃。首次启动期间，
`~/Library/Application Support/qrenderdoc/pystubs/{latest,v1_46}/version.txt`
于 11:50:01 被写入，但目录里没有实际生成的 stub 文件；第二次启动的
qrenderdoc 进程于 11:50:04 正常运行。`GenerateStubs` 当前在生成前写入
版本标记，所以下次启动会跳过该步骤。证据强烈指向 Python 3.14 的
自动 stub 生成或其失败后的初始化路径，但现有报告不能单独判定具体
Python/SWIG 失效点。没有为了复现而启动 GUI，也没有清理该用户缓存。
