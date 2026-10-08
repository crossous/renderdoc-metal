# B469：AS 标记与交点函数表清空/恢复

2026-10-05，接续 B468。第一轮候选 backend/bundle 均为
`d6ec05401e05d46982270f0cb02ec49fd55246ac5705f9e50467cbcec894cc5c`。
后续 B470 修改不能继承该 hash 的结果，必须重验。无提交/推送。

## 支持方式与实现

按 [PHASE54](PHASE54.md) 和 [D058](DECISIONS.md) 的 Vulkan/DX12 对照准则。
Vulkan DebugUtils labels、D3D12 Begin/EndEvent/SetMarker 已使用公共事件树；
Metal AS 编码器复用相同 ActionFlags 与已有 submission EID 模型。新增
insertDebugSignpost/pushDebugGroup/popDebugGroup 三个接口及末尾 chunk1415–1417，
Max1418，旧 chunk 编号/格式不变。校验 encoder 类型、native 对象、当前所有者，
错误/已结束 encoder 在处理前拒绝。标记是元数据，不改变 GPU 工作。

IntersectionFunctionTable 的单槽/range setter 允许 nil，清空后可恢复。Apple
协议的 handle 为 optional；这只是必要执行状态重建，不新增光追调试 UI。
新负例暴露“未知非空 ID 反序列化成 NULL”的漏洞：visible/intersection 两种表
现在直接保留序列化资源 ID，再检查存在性和 handle 类型/pipeline/stage。
只有真正 ID0 允许清空，未知非空 ID 不再被当作空槽。二进制字段和旧捕获兼容。

## 测试结果

一键入口：`bash util/buildscripts/scripts/test_metal_ray_basics_macos.sh`。
初轮记录：`build-macos-debug/metal-ray-b469/`；捕获
`captures/metal-ray-b469/table_capture.rdc`，SHA256
`8aaf5b4cfe80bb93c2731b40d2ded7158f1ead4bc5ef3204173a70da83b3dc9b`。

- 原生/注入均启用 Metal API Validation。四种单槽/range 清空和恢复得到
  GPU ray `0/1/0/1`，AS compacted size1280。
- 17 个 table 更新中包含5个 nil；零长度 range 无副作用。三层/同层 marker
  各自归属正确，无 synthetic continuation。54个 API events 去重且可选，
  前进/后退/前进3轮（162次选择），核对射线前缀、尺寸写回和现有 AS 绑定。
  GPU事件按 submission EID，CPU创建/快照按物理记录边界验证，不混淆两种顺序。
- 新19例 malformed 通过；visible 19例（含新增未知 handle）、render IFT20例、
  compute IFT18例通过；合计76例。无信号退出；每个测试有限时。
- 29份既有捕获 API/CLI回放通过：9份基础/同步/ICB哨兵及20份函数表/AS/
  render、compute、box、refit、TLAS代表帧。首次清单错误包含不支持的计数器
  T154，该项无capture，整个首轮gate不计PASS；核对B156记录后移除非本族项，
  完整修正gate通过，没有跳过实际光追测试。
- 新capture CLI3轮；T35/新帧/T300各10次打开关闭，总30次，resident
  增长1,212,416 bytes。库和bundle hash一致。

未运行全量308帧/累计负例，未进行新帧GUI验收，不能继承之前全量或人工结论。
只证明本批受控执行链可用，raytracing两项设备查询仍false；剩余初态、GPU实例
数据、帧内CPU表变更等门槛见PHASE54。
