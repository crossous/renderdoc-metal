# UE58 frame833：Private 间接绘制与后续超时（2026-09-29）

## 来源与改动

- 独立工作树 `renderdoc-metal-t312`，HEAD
  `e0a26f7e65e22a3890aa38a318bde51d24babf6b`；保留原
  `renderdoc-metal` 目录 4 项 Git 改动，未 reset/stash/提交/推送。
- 现有 UE 帧 `SocoTestProj/Saved/RenderDocMetalCaptures/UE58_frame833.rdc`，
  SHA256 `472dfa48a56cbaa43b6cc97d5f82d0471263de0c0fdd47049dde166e1df6f163`。
  静态 XML 有 27 个 `drawPrimitives(indirect)` 和 59 个含 Nanite 的
  debug marker；它不是非 Nanite 最小场景。
- 本轮改动在 `metal_render_command_encoder.cpp`、
  `metal_compute_command_encoder.cpp`、两个原有 indirect draw demo、
  `metal_replay_output_smoke.mm` 与本状态/证据文档。Private indirect draw
  不再调用 `contents()`，原生 GPU 绘制保留；事件树参数计数显式为
  GPU-defined/未知。直接和间接 compute dispatch 对有 slot-0 buffer、
  无纹理且绑定通过验证的路径接通；buffer usage 保守记录。

## 定向验证

命令均在 `renderdoc-metal-t312` 下运行，GPU 命令设置
`MTL_DEBUG_LAYER=1` 并以 Python 子进程限定 30–60 秒：

```sh
cmake --build build-metal-demos -j 8
cmake --build build-qrenderdoc --target build-qrenderdoc renderdoccmd -j 8
# 原生、注入捕获：bin/demos_x64 Metal_Indirect_Draw / Metal_Indexed_Indirect_Draw
# --frames 1 或 3；RENDERDOC_METAL_TEST_PRIVATE_INDIRECT=1；注入时设置
# DYLD_INSERT_LIBRARIES=build-qrenderdoc/lib/librenderdoc.dylib 和
# RENDERDOC_METAL_CAPTURE_PATH=/tmp/renderdoc-metal-t312-<fixture>
MTL_DEBUG_LAYER=1 build-qrenderdoc/bin/renderdoccmd replay --loops 1 <capture.rdc>
# 同轮编译并运行 util/test/metal/metal_replay_output_smoke.mm
```

- 原生 Private 普通和 indexed 间接绘制各单次 Validation/pixel 正例通过；
  两者注入捕获成功。普通夹具的扩展版还执行了 GPU buffer writer 和
  buffer-only indirect compute dispatch，捕获成功；该扩展版没有再次独立
  原生像素验收。
- 两份 Private 绘制捕获及普通扩展版的 CLI/API 单次回放退出 0；API
  核对了参数 buffer、事件 seek 与两个输出像素。扩展版还未对 compute
  输出 buffer 做 API 比对。
- 旧 `t13_capture.rdc`、`t21_capture.rdc` 的 API/CLI 各单次退出 0；
  `t29_capture.rdc` API 单次退出 0。没有运行本族畸形输入、全量回归、
  长时生命周期或人工 UI。累计 UI QA 增量 **0**。

## UE 帧当前停点

第一轮受控 CLI 在 14:15 安全拒绝于
`MTLComputeCommandEncoder::dispatchThreadgroups`，日志为
`/tmp/renderdoc-metal-t312-ue833-after-private-indirect.log`；
1×1×1 dispatch、无纹理、slot-0 buffer 1024 字节且绑定验证通过。
补该 compute 子集并验证旧 T29 后，14:18 单次 CLI 安全拒绝于
`dispatchThreadgroups(indirect)`，日志
`/tmp/renderdoc-metal-t312-ue833-after-compute-buffer.log`。新普通间接
compute 夹具取得捕获并 API/CLI 单次通过后，14:21 再对 UE 帧仅运行
一次：

```sh
MTL_DEBUG_LAYER=1 build-qrenderdoc/bin/renderdoccmd replay --loops 1 \
  '/Users/kurogames/Documents/Unreal Projects/SocoTestProj/Saved/RenderDocMetalCaptures/UE58_frame833.rdc'
```

60 秒超时退出 124，已终止进程；日志
`/tmp/renderdoc-metal-t312-ue833-after-indirect-compute.log` 只有回放开始和
Validation 启用，没有新的断言或失败 chunk。终止前进程约 881 MiB RSS；
检查时无残留回放进程或新 `renderdoccmd` 诊断报告。**不能宣称 UE 帧
可打开或 GPU 结果正确。** 按黑盒门槛暂停重复 UE 回放和增加负载。
随后只做一次 20 秒的定向等待 trace；设置
`RENDERDOC_METAL_TRACE_REPLAY_WAITS=1`，日志
`/tmp/renderdoc-metal-t312-ue833-trace-waits.log`。此前 12 个等待返回
status 4；最后 `ResourceId::503251` 于 status 2 开始等待，没有完成日志，
到时退出 124。静态 XML 中它创建于 chunk 3921、提交于 chunk 10180；
前一 `ResourceId::503248` 已完成。没有新 Validation 断言，也没有新
`renderdoccmd` 系统诊断报告。首个仍未解决的可观察阻塞因此缩小为
`503251` 的 GPU command buffer 完成等待；尚不知它自身工作、提交顺序、
依赖或驱动调度中的哪一项造成等待。下一步先静态审计该 buffer 的
依赖/提交图，再做小型夹具，不继续反复回放 UE 大帧。
静态展开此 buffer 的 9 个 encoder 有 184 个子 chunk，其中 10 次
`dispatchThreadgroups(indirect)`、11 次直接 dispatch、6 次 buffer blit、
24 次 compute memory barrier；没有捕获的 event/fence wait/signal。
因此目前证据不能把等待归因于某个同步 API；也不能把单一小夹具通过
外推到这条大型 GPU 工作链。

最终回放库与 app 内嵌库 SHA256 均为
`42ada08ec64cfc599808907fd8ec94b7ba19679147ed1e9b25f08e3da15682e5`；
`qrenderdoc` 可执行文件为
`023ad5ba4fb1759936d1bc88c63b0e821a28a4b5feb1c2b2bc724ba5171371de`。
`git diff --check` 通过。该 app 未做人工 UI 验收。
