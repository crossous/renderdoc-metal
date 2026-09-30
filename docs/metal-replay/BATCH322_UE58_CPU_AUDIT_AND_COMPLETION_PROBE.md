# BATCH322：UE 真帧无 GPU 审计与完成边界探针

2026-09-29，工作树为 `renderdoc-metal-t312`，基础提交
`a7dd2bdae28adaa5a0a623663ea2557d4af0a600`。原 `renderdoc-metal`
目录的四项未提交修改保留。本批没有启动 UE、qrenderdoc 或任何 GPU
回放，也没有提交或推送。

## 可复现的真帧事实

`UE58_capture.rdc` SHA256
`a96e685f726608bb30a84f2ae0c7485d2edaaad82553b8e95da1df44e91915f0`。
T321 的帧内 `Empty/Volatile` 预扫描原先也阻止了只读 XML 导出。
现在仅在实际 replay 模式执行该安全预扫描；structured export 只解码
chunk，不提交帧 GPU 工作。用新库的 `renderdoccmd convert` 在 1.08 秒内
导出 15 MB XML，进程最大 RSS 约 855 MiB。原 `.rdc` 未修改。

[`evidence/ue58-t322-capture-audit.json`](evidence/ue58-t322-capture-audit.json)
由 `util/ue/audit_ue_metal_xml.py` 从 XML 逐 chunk 流式生成：11214 个
chunk、278 个 render draw、89 个 render pass，其中 10 个有至少两个
颜色附件；存在 `NaniteBasePass`、`MainPass` 等 UE scope。这张帧确实含
场景渲染，但含 Nanite，并非目标非 Nanite 最小帧。旧
`UE58_frame1770.rdc` 的相同审计在
[`evidence/ue58-t322-frame1770-audit.json`](evidence/ue58-t322-frame1770-audit.json)：
4177 chunk、280 draw、仅一个 render pass、零 MRT，有 SlateUI scope。
两帧的内容不能混为一谈；旧帧的黑 RT 仍是人工 UI 内容失败。

真帧 `251031` 与 `251032` 在 chunk 9248/9249 被同一个 blit encoder
`251045` 读取，所属 command buffer `251044` 于 11163 提交；
11174/11175 对两 buffer 调用 `PurgeableStateEmpty`。前一刻 UE 还提交了
完成信号 command buffer `251104`（11173）。帧内没有
`MTLCommandBuffer::waitUntilCompleted` chunk；其 `addCompletedHandler`
只记录注册身份，回放没有重现回调边界。UE 5.8 `MetalSubmission.cpp`
通过 completion fence/handler 驱动回收，`MetalTempAllocator.cpp` 的
`DeferredDelete` 路径会设置 `Empty`。D3D12/Vulkan 在等待队列/栅栏
完成后回收资源；Metal 必须先重建等价完成边界，再执行此转换。
**不能仅按 `commit` 顺序假定 GPU 已完成，也不能移除安全守卫。**

## 稳定性边界与下一个验证

本机 14:30 与 20:43 两份 panic 均写明 `WindowServer` 在 120 秒内
未成功 checkin，随后 watchdog 重启；20:41–20:42 的报告具体指出
WindowServer 主线程无响应。它们不是 `IOGPUResource::free` 断言，
但现有记录不足以证明或排除 RenderDoc/Metal 回放诱发了窗口服务故障。
本批只做 CPU 导出、静态审计、编译；未继续升级 GPU 负载。

新增 `metal_purgeable_completion_probe.mm`：在独立测试 Mac 上以原生
Metal Validation 一次性执行 4 KB blit、队列后继完成信号及 callback，
确认前序工作已完成后才 `Empty`，再恢复 `NonVolatile`、填充并验证
第二次 blit。`run_purgeable_completion_probe_macos.sh` 默认**只编译**；
显式 `--run` 才执行，进程限时 10 秒并保存日志。当前机仅编译通过，
没有原生运行结果。获得原生正例后，仍需注入截帧、API/CLI 单次回放、
GPU 字节与 seek、必要负例和受影响旧帧定向验证，方可实施完整功能族。

## 本批命令与结果

- `cmake --build /private/tmp/rdm-t312-build --target renderdoc renderdoccmd -j 4`：
  通过。库 SHA256 `30b75f7ab51b69d04684900ebe9fb537d6268154b5a8bb5ef3af`；
  CLI `c49dd50da2119eab1788b3bcb782bd26a94b93f0388bac9e77e63058220e161a`。
- `/private/tmp/rdm-t312-build/bin/renderdoccmd convert -f
  "/Users/kurogames/Documents/Unreal Projects/SocoTestProj/Saved/RenderDocMetalCaptures/UE58_capture.rdc"
  -o /private/tmp/rdm-t322/UE58_capture.xml`：
  通过，**只读导出**；先前同命令被 T321 的安全扫描拒绝。
- `/private/tmp/rdm-t312-build/bin/renderdoccmd convert -f
  "/Users/kurogames/Documents/Unreal Projects/SocoTestProj/Saved/RenderDocMetalCaptures/UE58_frame1770.rdc"
  -o /private/tmp/rdm-t322/UE58_frame1770.xml`：通过。
- `python3 util/ue/audit_ue_metal_xml.py /private/tmp/rdm-t322/UE58_capture.xml
  > docs/metal-replay/evidence/ue58-t322-capture-audit.json`；同命令对
  `UE58_frame1770.xml` 输出 `ue58-t322-frame1770-audit.json`：通过。
- `bash util/test/metal/run_purgeable_completion_probe_macos.sh`：仅编译通过；
  源码 SHA256 `e49a5bd82f9c29ccaa6e7a9bfb3c7192c4165896d64a5752cb9a8700098c1456`。
- `python3 -m py_compile util/ue/audit_ue_metal_xml.py`、`bash -n` 探针脚本、
  `git diff --check`：通过。

本批**仅 CPU 定向审计和编译通过**。没有原生 GPU 正例、注入、API/CLI
回放、全量回归或人工 UI 验收；累计成功 UI QA 增量 **0**。真帧仍被安全
拒绝，首个未解决功能点是帧内 purgeable 与 GPU 完成边界。越过它之后，
还需验证场景输出与 Shader Converter GPU 地址/资源身份。
