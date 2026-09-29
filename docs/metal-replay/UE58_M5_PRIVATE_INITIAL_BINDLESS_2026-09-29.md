# UE58 frame833：Private 初始状态恢复与 bindless 旧帧阻塞（2026-09-29）

## 工作树与输入

- 只在独立 `renderdoc-metal-t312` 工作树修改，基础 HEAD
  `e0a26f7e65e22a3890aa38a318bde51d24babf6b`。原 `renderdoc-metal`
  的 4 项 Git 变动未碰；没有 reset/stash/提交/推送。
- 原始 `UE58_frame833.rdc` SHA256
  `472dfa48a56cbaa43b6cc97d5f82d0471263de0c0fdd47049dde166e1df6f163`；
  这是含 Nanite/VSM 的项目帧，不是非 Nanite 最小场景。
- 本批新增逻辑：`metal_core.cpp` / `metal_device.h` 的 Private buffer 初始内容
  GPU blit 恢复；`metal_indirect_draw.cpp` 的帧前 Private 参数夹具；
  `metal_private_initial_invalid.py` 的未知资源负例；UE 启动脚本可用
  `UE_METAL_EDITOR_ARGS` 传递可记录的编辑器参数。其余工作树变动是之前
  的 UE/Metal 接入成果，不能以本批 `git diff` 总量计算新增工作。

## 横向排查和静态证据

- D3D12 `d3d12_initstate.cpp`、Vulkan `vk_initstate.cpp` 和
  `ResourceManager::ApplyInitialContents()` 都有 GPU 本地资源初始状态应用。
  Metal 先前 `Serialise_InitialState` 只缓存 buffer 字节；每次帧前
  `ResetReplayCPUUpdatedBuffers()` 只写 Shared，未恢复 Private。
- `frame833` 静态导出有 436 份 buffer 初始状态，其中 420 个是 Private，
  共 421,200,384 字节。卡住的 command buffer `503251` 的间接 dispatch
  读取 `6984`、`6987`、`4153`、`4237` 等帧前 Private buffer。新上传逻辑
  使用 16 MiB Shared staging 分批 blit，保留快照用于事件回跳；该帧的
  420 个 Private heap buffer 在静态区间检查中互不重叠。
- UE 5.8.3 的 `MetalBindlessDescriptors.cpp` 用
  `IRDescriptorTableSetBuffer(..., Buffer->GetGPUAddress(), ...)` 写 descriptor。
  Shader Converter 的 `IRDescriptorTableEntry` 为三个 64 位字段：
  `gpuVA`、`textureViewID`、`metadata`。UE `METAL_SM6` 默认
  `BindlessConfiguration=All`。这张旧帧的 Shared buffer `24` 长
  18,874,368 字节（恰为 786,432 个 24 字节表项），其初始字节中有
  844 个非零 GPU VA 字段、677 个不同取值；compute encoder `503285`
  等将 `24` 绑在 slot 0、`25` 绑在 slot 1，符合 UE 源码绑定点。
  旧 `.rdc` 的资源创建 chunk 没有记录 capture 进程的 `gpuAddress`/
  `gpuResourceID` 映射；现有 Metal replay 也只重编码已知 argument
  encoder packet，没有通用的 Shader Converter descriptor 重定位。
  D3D12 的 GPU 地址跟踪和 `ExecuteIndirect` patching 提供相近方案，
  但不能直接猜测 Metal 的表项/资源身份。**这高度怀疑是旧帧卡住的
  后续原因，尚未以最小夹具证明它是唯一原因。**
- 新增 `metal_gpu_address_probe.mm`，在本机 Metal Validation 下原生编译
  并运行：shader 从 Shared 表里的 `MTLBuffer.gpuAddress` 反解 GPU
  指针，读入 41 写出 42，退出 0。它证明本机 GPU VA 表项有实际解引用
  语义；尚未注入捕获、API/CLI 或验证完整 UE descriptor 三字段，
  因此没有据此放行 bindless 回放。

## 本批定向终端结果

完整构建使用无空格源码别名 `/tmp/rdm-t312-src` 和
`/tmp/rdm-t312-build`（它们指向此独立工作树），因为 CMake/Ninja 的
`-force_load` 路径在原目录名含空格时拆词。命令：

```sh
cmake -S /tmp/rdm-t312-src -B /tmp/rdm-t312-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_METAL=ON -DENABLE_QRENDERDOC=ON \
  -DENABLE_RENDERDOCCMD=ON -DENABLE_PYRENDERDOC=OFF -DENABLE_VULKAN=OFF \
  -DENABLE_GL=OFF -DENABLE_GLES=OFF -DENABLE_EGL=OFF \
  -DPCRE_HEADER=1 \
  -DRENDERDOC_SWIG_PACKAGE='/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312/build-qrenderdoc/qrenderdoc/custom_swig-prefix/src/renderdoc-modified-7.zip'
cmake --build /tmp/rdm-t312-build --target renderdoc renderdoccmd build-qrenderdoc -j 8
cmake --build build-metal-demos -j 8
clang++ -std=c++17 -fobjc-arc util/test/metal/metal_gpu_address_probe.mm \
  -framework Foundation -framework Metal -o /tmp/rdm-t312-gpu-address-probe
MTL_DEBUG_LAYER=1 /tmp/rdm-t312-gpu-address-probe
```

构建退出 0；`qrenderdoc.app` 已编译，**没有启动 GUI**。新库与 app 内嵌库
SHA256 均为 `19d491f029980e7b40f2f5f88c1fedcf833b8ad46fb78c60f3684bdc1843ed8f`；
app 主可执行文件为 `6140aba9fa4e8e46b35b8a11fbde1c347dea7c2c9005c3a36c1247ac46ca581f`。
可供用户手动打开的复制版位于
`build-private-initial-viewer/bin/qrenderdoc.app`，该路径内 lib hash 与构建一致。
此前给用户的 `build-qrenderdoc/bin/qrenderdoc.app` 也已同步为同一构建与
同一内嵌库 SHA256；两处 app 都未做人工 UI 验收。

- `RENDERDOC_METAL_TEST_PRIVATE_INITIAL_INDIRECT=1` 下，
  `MTL_DEBUG_LAYER=1 bin/demos_x64 Metal_Indirect_Draw --frames 3 --debug`
  原生退出 0；同命令设置 `DYLD_INSERT_LIBRARIES=/tmp/rdm-t312-build/lib/librenderdoc.dylib`
  和 `RENDERDOC_METAL_CAPTURE_PATH=/tmp/rdm-t312-private-initial-capture`，
  注入退出 0 并保存 48 字节 Private 初始状态。截帧 SHA256
  `099a2f29ec3285da5842829951f8f8f06fe3f8a7bc5b46bb6c65fd6312b24b87`。
- 旧库用该新夹具做 API 回放，在 Metal Validation 下于 Private `contents()`
  断言，退出 -6；新库 `renderdoccmd replay --loops 1` 退出 0，
  API helper 核对间接参数、事件回跳和两侧像素后退出 0。
- `MTL_DEBUG_LAYER=1 python3 util/test/metal/metal_private_initial_invalid.py
  /tmp/rdm-t312-build/bin/renderdoccmd /tmp/rdm-t312-private-initial-capture_capture.rdc`
  退出 0，未知 Private 初始资源安全拒绝。旧 T13/T21/T29 的 API 与 CLI
  各单次退出 0。最终库下旧 UE `UE58_frame99.rdc` 的 CLI 单次退出 0，
  API 正向/回跳 pipeline 身份探针退出 0，报告 165 个 draw；它仍不是
  原生画面逐像素对照。`git diff --check`、启动脚本 `bash -n` 与 Python
  `py_compile` 通过。没有全量回归、长时压力或人工 UI；累计 UI QA 增量 **0**。
- 对 `frame833` 仅一次 `MTL_DEBUG_LAYER=1`、
  `RENDERDOC_METAL_TRACE_INITIAL_PRIVATE=1`、
  `RENDERDOC_METAL_TRACE_REPLAY_WAITS=1` 的 CLI 回放，Python 子进程
  `timeout=75`。该次诊断使用的库 SHA256 为
  `9f48d708fbddb9b3633db9c5be0d9a0aefe9449c1686fb5218956a729add0fca`；
  随后只加了 upload 完成状态检查，未再次跑整帧。日志
  `/tmp/rdm-t312-ue833-private-initial-once.log`：
  421,200,384 字节分 26 批上传成功，但仍在 `ResourceId::503251`
  status 2 等待，75.012 秒超时退出 124，进程已终止；无新的 Metal
  Validation 断言，观察到约 880 MiB RSS。**旧帧仍不可打开。**

## 第一个仍未解决的阻塞

`503251` 的 GPU 工作没有完成；最强静态嫌疑是 UE SM6 bindless 表项保存
了旧进程的 GPU VA / texture resource ID，而旧 `.rdc` 缺少建立原地址到
回放资源身份映射所需的数据。下一步需要完整功能族：捕获时记录原资源
GPU VA/ID，识别并重编码表项，覆盖 Shared 初始和帧内 CPU 更新，以及
GPU 写入/复制 descriptor 的执行点语义；先做原生 Validation 小夹具、
注入、API/CLI、像素/数据和必要负例，再用**新截帧**验证。不能对旧帧的
任意 64 位值盲目替换，也不能借 `-BindlessOff` 声称该族已支持。

后续实测否定了当前项目使用 `-BindlessOff` 的启动路径：UE 5.8.3
`METAL_SM6` 全局着色器编译报告 3157 个错误，窗口未创建；见
[启动失败证据](UE58_M6_BINDLESSOFF_STARTUP_2026-09-29.md)。不要据此重复等待或
声称 bindless 已隔离验证。恢复默认配置取得新普通帧；如有新 GPU 错误或
资源异常增长，按 `BLACKBOX_GATE.md` 停止升级负载并保留 session 日志。
