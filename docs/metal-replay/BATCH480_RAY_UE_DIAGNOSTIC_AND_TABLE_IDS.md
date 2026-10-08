# B480：UE 诊断能力入口与 compute 函数表身份

2026-10-05，接续 PHASE57/B479。backend/bundle 已构建为同一
e0e4df240356b81d045f5271e6700af11a8c0bf692f3631a653d492b7b51f3b8。
B479 a35028ba 固定库全量通过结果不归于本批次。

## 支持边界与实现

按 [启用门槛](RAYTRACING_ENABLEMENT.md)，UE 必须先得到 compute ray tracing
支持才能进入真实调度。capture 进程设置 RENDERDOC_METAL_RAYTRACING_PROBE=1
时，supportsRaytracing 使用原生设备返回值；默认、空值和其它字符串仍 false。
supportsRaytracingFromRender 保持 false。警告说明生产验收尚未完成；不因环境
变量宣称设备具有硬件 RT。测试记录原生返回值，再分别核对默认及诊断状态。

UE 入口 util/ue/run_testproj_metal_ray_macos.py 使用本机 Testproj 和已校验的
隔离 MetalRHI provider，进程内配置 r.RayTracing/Lumen HardwareRT。启动地图
设置写入独立 INI，校验原编辑器配置未变化；旧捕获按哈希保留，新捕获复制至
诊断结果目录。记录实际 library 哈希、查询警告、API chunk 数量和 session 日志。
CAPTURED 只表明生成捕获，RT 调度/回放另行验证，不能以普通 draw/dispatch
数量作为光追通过。原 UE 引擎文件不修改。

参照 Vulkan/DX12 公共资源 ID 与句柄重建，补齐 compute VFT 单表/数组及
IFT 数组的非零未知 ID 拒绝；沿用 B479 单 IFT 路径。显式 ResourceId 保存
区分 nil 与未知对象，序列化字段及二进制布局不变。nil 清空、稍后恢复绑定
合法；最终 pipeline 兼容性仍在 dispatch 前检查。未扩展内部 RT 调试。

## 测试结果

| 检查 | 状态 |
| --- | --- |
| backend/bundle 一致哈希 | e0e4df24，一致 |
| native/default/probe/非法环境值/空值能力 | 6项 PASS：native1/1、default0/0、probe1/0，其它0/0 |
| compute 单 VFT、数组 VFT、单 IFT、数组 IFT identity/nil 恢复 | 8坏 ID拒绝；各 nil 清空/恢复3loops PASS |
| 新数组 IFT native/capture/API seek/EID0/CLI3 loops | 帧内/帧前 AS 两例 PASS，ray0/1/0/1 |
| 官方 triangle/procedural 新捕获离线像素与事件定位 | 两 scene PASS：native各2次、44/46事件各3方向、CLI各3loops；41坏输入拒绝 |
| 相关旧函数表/AS 例、坏捕获、生命周期 | basics定向与21旧例 PASS；2,070命名事件选择；20×10生命周期 PASS，growth540,672bytes |
| 实际 UE RT 启动/构建/dispatch 与离线回放 | FAIL：UE初始化后实际进入RHIBuildAccelerationStructures，在AS对象类型检查拒绝（B481证明是未包装heap AS）；无可验收新RT捕获，dispatch/回放未验 |
| ARC 父设备先释放 | 未修复，启用前仍需闭环 |

检查入口 util/test/metal/metal_ray_capability_gate.py、
metal_compute_table_binding_invalid.py、metal_ray_table_capture.mm 的
array-bindings 及 background-array-bindings 模式。

结果：build-macos-debug/metal-ray-b480/suite.log、suite-manifest.json、
sample-manifest.json，build-macos-debug/ray-capability-gate/manifest.json。
官方当前 replay-manifest.json 为 e0e4df24；B479 的官方证据保存在
gate-results-b479-a35028ba 中。未重跑本版本全量或 GUI；未提交/推送。

UE首轮：build-macos-debug/ue-ray-diagnostic/b480-e0e4df24/manifest.json。
进程实际查询诊断compute（警告已观察），退出1；session20261005-154424 的
renderdoc.log 指向AS command encoder bridge第220行，ue-editor.log含
RHIBuildAccelerationStructures与RayTracingGeometry breadcrumb。新版本B481
将先记录usage/geometryCount/motion等实际参数，不把诊断构建当功能修复。
