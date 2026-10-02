# BATCH340：GPU 写纹理描述符后的实际采样

2026-10-01；继续持续目标，未提交、推送或完整 UE GPU 回放。

在 v7 显式字段 shadow 上复用 B338 的计算 producer/payload/expected/source
验证路径：真正的 compute shader 复制三个 qword 后，由下一 shader 通过
目标表中的新 texture ID 采样。期望值注解只验证，不 CPU 覆盖 GPU 目标。

两次捕获分别 122→186、186→122（第二次以第一次 GPU 写入作为初始状态），
每份四轮 first/producer/consumer/EID0 seek，GPU 字节、普通常量、DEADBEEF 和
clear 像素通过；capture纹理ID1/2重建为2/3，采样像素64/128正确对应。

复用 sourced GPU compute gate 的24个 API+CLI 错误组；初始数据变异兼容
Initial Contents 与 newBufferWithBytes initialData 两种来源。十一类 tiny /
171 API+CLI 负例全通过，日志 metal-descriptors.eunQa9，脚本
util/buildscripts/scripts/test_metal_descriptor_relocation_macos.sh。12组 CPU
texture upload 负例另独立通过。旧六帧定向回归通过，日志
build-macos-debug/targeted-reflected-texture-final.log。

最终库 SHA256
a0f35fed80074ec2da42a879d287bc828adb99868c5a4cb53fc68c6baff78e44。
原始数据与 GPU 更新均已有实际小例证明；真实 UE b341dea4…仍仅 CPU 审计。

下一项：帧内创建临时 buffer/table、已知来源到新 ResourceId 的创建顺序、
多提交/alias与各 render stage。复用已有 placement 事件回跳和 GPU 完成等待。
继续保持小预算直到能够证明整帧资源与提交契约；不直接扩大到 UE 6 GiB heaps。

全量回归未跑，新增人工 UI 未验，UE 正确图像/MRT/pass scope 全部仍待验。
目标 active；这些 tiny 验证不构成完整 UE 验收。
