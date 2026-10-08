# B499：参数缓冲的 AS / IFT / VFT 资源恢复

2026-10-06，PHASE58。补齐 `MTLArgumentEncoder` 的 AS、交叉函数表编码及可见函数表初态恢复；真实 query 同时消费三种成员，Shared/Managed、function/device encoder、标量/函数表数组入口已验。生产 `supportsRaytracing` 与 `supportsRaytracingFromRender` 仍 false，未启动 UE。

## 参照与实现

先核对仓库 Vulkan `wrappers/vk_descriptor_funcs.cpp::Serialise_vkUpdateDescriptorSets` / `ReplayDescriptorSetWrite` 的 typed AS descriptor 以及 DX12 `d3d12_device_wrap.cpp::CreateShaderResourceView` 的 descriptor 资源关联。两者恢复的是已声明资源及原生 descriptor，不扫描任意字节猜地址。函数表采用既有 Metal pipeline/function 关联；实际编码由 native argument encoder 执行，Metal 专有 handle 布局不仿造。

新增 AS/IFT scalar 序列化 chunk，追加在旧 chunk 末尾，旧 ID 不变；IFT 数组入口检查范围和每个资源类型后归一化到 scalar。既有 VFT 数组入口保留。device argument descriptor 允许三种 ray 类型（AS 区分 primitive/instance），沿用只读、32成员、无重叠及 alignment 范围约束。

参数缓冲按 `(buffer ResourceId, packet offset, member index)` 保存 AS/IFT/VFT typed shadow、明确 null 及资源依赖。初态/CPU 数据恢复后重新编码这些资源字段，避免 capture 进程 GPU identity 覆盖 replay handle，保留常量与填充字节。三种 compute dispatch 入口核验成员已声明、AS 已构建且种类匹配、函数表属于当前 PSO；dispatch 记录三种资源的 CS Resource usage。无公共 MetalPipe ABI 变化。

帧内 resource 重编码仍走 `CheckCaptureMutation` 并以 `MTLArgumentEncoder::unsupportedEncoding` 拒绝离线加载。本批只支持帧前编码，不把新增接口当成任意动态参数缓冲支持。

## 实际验证

串行 gate：`util/buildscripts/scripts/test_metal_ray_packet_macos.sh`。所有 GPU 子进程有限时间、Metal validation 开启，无 UE/qrenderdoc 同时运行。固定非 opaque triangle BLAS、两套 IFT（accept/reject）、两套 VFT（3/9），另建 foreign PSO 表作反例。四次实际 ray 调度分别输出 **3 / 0 / 0 / 9**；另有第五个显式全 null packet，不调度它。packet 前48 bytes填充0xa5。

| 场景 | native → capture → API/CLI | 事件选择 | 坏输入 |
| --- | --- | --- | --- |
| function | PASS | 22 × 3 | 20 PASS |
| device | PASS | 22 × 3 | 20 PASS |
| function-managed | PASS | 22 × 3 | 20 PASS |
| device-managed | PASS | 22 × 3 | 20 PASS |
| function-arrays | PASS | 22 × 3 | 20 PASS |
| device-arrays-managed | PASS | 22 × 3 | 20 PASS |

六例共 **396** 次正/反/再正事件选择；每次先 EID0 验证输出全7，按 GPU 调度前缀及 CPU 原 chunk fileOffset 核对已执行结果，dispatch 上参数 buffer/offset 与 AS/IFT/VFT usage 一致。null packet 恢复及 prefix 填充不变；CLI 每例3 loops。这里数组表示函数表 setter 数组入口，未声称多元素 ray 成员数组已验。

120 坏输入涵盖未知资源、buffer 冒充 ray 资源、错误成员/32边界、未知 encoder、buffer 冒充 encoder；IFT/VFT 各含另一 PSO 的同类表。前六类失败于具体 setter chunk，foreign 表在 dispatch 前失败；拒绝信号、超时及无关失败。显式 null 合法，不作坏输入。

`function-frame` / `device-managed-frame` 两例 native/capture 输出仍正确，离线分别明确拒绝 `unsupportedEncoding`。不计入六例成功 replay。

旧 sentinel + t54/t55/t60/t119/t120/t126/t127/t128/t129，合计 **18** 份 API/CLI PASS；T60 device 参数 encoder **27** 反例、t126/t127 VFT 参数各 **7** 反例 PASS。t35 与六份新 capture 共 **7 × 10** 生命周期 PASS，resident growth **409600 bytes**、exit0、backend 起止 hash 一致。不是 ARC 父对象提前销毁验收。

## 当前库的官方样例复验

Apple 官方 MIT sample ZIP 固定来源 `https://docs-assets.developer.apple.com/published/ade36d76f1bb/AcceleratingRayTracingUsingMetal.zip`，SHA256 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`。复用已下载 ZIP，在独立 B499 目录逐文件核对原包，未修改上游；`metal_apple_ray_sample_gate.py --stage replay --work-dir build-macos-debug/metal-ray-b499/apple-sample` 重建 Metal2 scheme 与固定 runner。

triangles/procedural 两 scene（18/27实例，seed1、64²、4帧），两次 native 可重复，capture 与 native 的65536-byte RGBA32Float输出逐字节一致；44/46事件各三方向及 EID0、TLAS 绑定、CLI3 loops PASS，共 **270** 次选择。**10** 尺寸查询、triangles13/procedural28共 **41** 坏 sample PASS；**6** 能力查询 PASS：native compute/render=1/1、默认0/0、精确 probe=1仅 compute1/render0，true/0/空仍0/0。没有提前打开生产能力，也未覆盖 Metal3/UE raw IR。

## 失败、哈希与产物

初次 bridge 编辑重复已有 VFT 数组方法，构建失败；移除新增重复声明、保留原实现后 final build PASS。日志 `build-before-existing-VFT-array-merge.log` 保留。初次 replay helper 使用不存在的 `rdcinflexiblestr.endsWith`，改成确切名称比较，失败日志 `replay-build-before-name-comparison-fix.log` 保留。不计这些失败为通过。正式 gate 的旧 VFT 反例命令在执行前修正为脚本实际支持的 t126/t127 参数。

- backend/bundle SHA256：`03fc7b32c31d0c31c07655df462d2814c984086a646e14982d924b17333832b1`。
- GUI SHA256：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。
- `build-macos-debug/metal-ray-b499/manifest.json`、`packet-manifest.json`、`followup-manifest.json`、`source-binary-hashes.json`；build/gate/各模式 native/capture/API/CLI/invalid/lifecycle/官方/capability 日志。
- 新 capture：`captures/metal-ray-b499/`，官方产物：`build-macos-debug/metal-ray-b499/apple-sample/gate-results/`。baseline eea 库独立保留。

shell/Python 语法、diff whitespace、最终源/二进制哈希核对 PASS。UE 实际 dispatch、旧75份 RT、集中308份、原 Qt sizeHint 崩溃、ARC父生命周期均 **NOT RUN**；旧 hash 全量结果不归于03fc。

## 接续与限制

primitive AS 参数已实测，instance AS 参数布局已允许但 TLAS packet GPU 路径未验；嵌套 ray 参数、render ray 参数、Private/GPU 改写及帧内重编码未验/未支持。UE raw `IRDispatchRaysArgument` 及 SBT/GRS/static sampler/AS header 地址依赖尚未重定位；本批不代表 UE 光追可开启。下一优先验 typed TLAS packet 与缺失成员/错误 AS kind 拒绝，再推进完整 UE IR 声明和重定位。

原 AS build ForceCrash 暴露的 Private/placement 输入已在 B494 小例修复，但 UE 回到 dispatch/输出闭环仍缺证据。系统死机取证见 B495：WindowServer watchdog 与 UE 等待高CPU MTLCompilerService 同期，未确认GPU kernel panic或具体待编译函数，根因未修。本批有限 sample 通过不能证明该问题修复，继续不直接重跑旧长 UE/full followup。RT 保持黑盒范围，不支持 AS 内部查看、RT shader 单步或 RT Pixel History；未提交/推送，持续目标未完成。
