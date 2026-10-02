# BATCH335：明确来源的静态 slot shadow 实际重放

## 当前契约

coverage v4 的首个执行范围为静态 Shared buffer 描述符和 compute inline VA。
复用已有 slot allocation/write/free/source chunks 构造逻辑 shadow，冻结其初始状态，
根据明确 ResourceId 和成员 offset 重定位，普通字段原样保留。退休 slot 的 VA 清零。
逐次 CPU 更新接入已有 submission-owned CPU snapshot，每次 seek 恢复初始 slot 状态。

在整个 frame CPU preflight 完成后才进入加载执行：检查槽位世代、完整来源与 raw VA
一致、inline 声明/绑定顺序、encoder 生命周期、pipeline reflection 和 buffer 大小。
首阶段仅允许一张 command buffer、1×1×1 compute dispatch、render clear；buffer
不超过 64KiB，总 buffer/heap 预算不超过 1MiB，texture 不超过 2×2。GPU-authored
slot、帧内出生/alias reuse、draw、其他 shader stage 尚未放行。UE provider 不声明 v4。

这采用其他后端的 logical descriptor shadow 和完整帧预检，再做 Metal 原始 VA 字段
替换；不是重新按地址扫描常量或绕过 GPU 完成等待。

## 实测

`metal_descriptor_shadow_capture.mm/replay.mm`：native、两张捕获与 replay 均通过。
GPU 返回 80/DEADBEEF；GPU 输出的 inline table VA 与实际读取的 source VA 均不同于
捕获地址，且 table 内替换地址一致；每张帧 4 次 dispatch/帧首回跳，初始世代与退休
指针恢复正确；普通 64 位字段与 2×2 BGRA 像素正确。初始 placement overlap 原型
仍被旧 heap 安全边界拒绝，没有放宽其判断；最终用例使用非重叠静态 placement。

七类定向 native/两捕获/GPU/seek（table、GPU update、frame Shared/Private/view、
inline、shadow）及原 56 + 新 22 组 API/CLI 负例通过；新增负例含遗漏来源、错源/
越界、世代、GPU expected、先写后声明、inline after end、第二 command buffer、
遗漏 pipeline/output 和预算/dispatch 范围。全部负例在 GPU wait/Private upload 前拒绝。
七类日志 `build-macos-debug/metal-descriptors.lxjerP`；22 组补充检查
`build-macos-debug/metal-descriptor-shadow/gate-bindings-final`。
旧路径 t01/t02/t09/t11/t12/t35 的 API 和 CLI 通过，日志
`build-macos-debug/metal-targeted.IqKgn6`。

完成补充检查时 library SHA256
`795a470a7c0fda22a3f623696b270dab4e930c9d3ff48e10339ac361d6fe004d`。
全量回归未跑，新库人工 Viewer 未验，完整 UE replay 未完成。持续目标仍 active，
下一步是已重定位 payload 的 GPU 更新与多提交生命周期，然后扩展 render/stage 和
UE frame-born 资源。没有提交或推送。
