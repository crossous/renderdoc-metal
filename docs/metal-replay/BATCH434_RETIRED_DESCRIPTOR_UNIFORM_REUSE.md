# B434：描述符退休后保留复用范围内的普通常量

实际 UE ef026832 在前36次 Native 提交全部完成后，间接参数与 capture 不一致。执行前原生 GPU 初始读回与原文件一致，逐次计算读回将偏差缩小到参数生成。CPU审计定位到 Shared arena ResourceId477：历史已经释放的 type6 条目32784/33288占用的字节，当前已被普通常量覆盖；例如 captured 32784为(0,9)、33288为(7,11)，旧 OverlayDescriptorSlotBuffer 会再次清零这些有效常量。

按 D3D12/Vulkan 中独立的逻辑 descriptor shadow 与 Native GPU 内存生命周期处理：coverage65 的帧内 overlay 对全部已经退休的逻辑条目保留原生字节，不只限于 GPU-written table。背景历史不拥有该物理范围的未来用途。初始恢复仍清除旧身份；新代 descriptor 分配/绑定、地址迁移、提交拥有的 CPU 快照和完成等待继续按原验证执行。没有替换间接参数，没有跳过计算/绘制，没有修改原 UE capture。

极小实际 Native 用例使用一个24B Shared arena，先分配/绑定/释放 descriptor，截帧内同地址写入六个普通常量13/17/19/23/29/31，原生计算 shader逐字检验，并通过真实 GPU输出 marker。两份截帧各四次定位/重置：GPU输出、六字节域及 EID0 全零恢复均通过。旧冻结库5366f35c对相同 capture 的输出为00000bad，返回9，复现清零问题。当前库275d6a211d6b90e38656fe6f127f0a724a88d4707443d12b2e7217ef04cf0227通过，日志 metal-retired-uniform.Zb80xk。测试脚本 util/buildscripts/scripts/test_metal_retired_uniform_arena_macos.sh；可设置 RENDERDOC_METAL_BASELINE_LIBRARY_DIR 对照旧库。

附加诊断均由显式环境变量开启：RENDERDOC_METAL_TRACE_INDIRECT_REPLAY 在初始恢复后和逐次间接使用读回参数；RENDERDOC_METAL_TRACE_COMPUTE_ARGUMENT_PRODUCERS 为拥有相应间接源的 command buffer 中的直接计算保存执行点读回，不改 Action 参数、生产shader或原命令。诊断的 Native 调度/屏障开销存在，默认关闭。

修复后的实际 UE prefix36通过：全部 Native 提交完成，17 render indirect 原参数逐字一致；compute原先1954的16/34/0恢复8/1/1，3502/3521恢复350/189，4083恢复66。原 capture sha256不变。日志 ue-acquired-drawable.prefix36-retired-uniform-fix.log/json。

后续加强 compute indirect 校验：按 encoder/ordinal逐次匹配原捕获证据，读回后比较三字；不同就拒绝正常打开，不以抓取证据替代实际调度。诊断 producer 读回不消耗 ordinal。精确库0d0725ffa339237e33a612132c5f412bbdaa7fe58c0e68abe393d440d6f210f3：metal-indirect-evidence.qIUTqg，8captures/56seeks/176API+CLI格式反例通过；另2组合法形态但预期1或3被改成2的证据，原调度不改，实际GPU完成后API/CLI均准确拒绝。helper metal_compute_indirect_execution_match_gate.py。最新UE prefix48正在验证；当前最新全量及人工UI尚未验证。未提交或推送。

实际 UE 结果（2026-10-02 18:36）：同一份 ef026832 原文件的 prefix48/63/75 均通过，31 compute indirect 与17 render indirect 逐次执行点证据匹配。只新增 coverage65 声明的候选副本 bd34d0b7 正常 OpenCapture，通过最后EID9669、229draw/112dispatch/313marker/1present；两次EID0→完整帧 Native BGRA8 readback 2,304,000字节与首次打开完全一致，SHA256 1e7f9e791cdabde129856f23e9a13e3463c0d311324dc95e8eb25b8045e74e34。按官方 core.cpp 的896×637整数缩放和原 jpge quality90 编码，JPEG与原捕获缩略图逐字节相等，SHA256 15d15618c45e04b63bec4f642205c2facad0a048ac3d1af8a5f41630756de6b9。原始GPU命令/资源/缩略图payload均经转换往返审计保持不变。

全量独立记录：0d0725ff 冻结验证在308份正常捕获重放后，T49畸形Private CPU更新反例触发Metal验证断言，未完成全量。调用栈定位 ResetReplayCPUUpdatedBuffers 的拒绝日志无条件调用 Private contents()；短路验证本身已正确拒绝。改为仅Shared打印contents，库 f7a662f75dbf9d5055fc001c71a85dd4078e5d44290dc6eb8ee1c20c9aa2d327。定向 MTL_DEBUG_LAYER=1 metal_command_handlers_invalid.py 全部通过，日志 private-reset-diagnostic-targeted.log。本次日志修复不改资源恢复/提交/生命周期，无立即重复全量，继续同一UE真实回放局部事件及UI。
