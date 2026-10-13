# 当前阶段（2026-10-13，metal.3）

2026-10-13 已发布v1.46-metal.3：tag/source 5178f08d04c5abdb9f2ce384d0fac55a65294474，packaged dylib e5cc44b8495204e61cef6f368a1a605cc6d32b00275371ed89120bcac2960ee6。远端标签/四附件digest及重下载全部1788文件/link/签名核对通过。仅本次阶段交付完成，完整UE仍FAIL/根因UNKNOWN；后续生产基线来自该tag，不自动提交或发布。

本次发布范围固定为 metal.2 源码加合法 Native compute 线程组误拒修复及两个相关测试文件。转换器默认 tg_size 不再冻结实际 Native shape；设备/编译器限制、线程组内存和资源/地址/寿命恢复检查保留。默认关闭 CPU 纹理身份观察、Clear 读回和未完成材料列表/bitcode/AS 试验不纳入。原目录 dirty 与外盘证据完整保留。

最终源码、实际签名 dylib/bundle、通过/失败/未运行范围、GUI及可比性能见[metal.3发布记录](RELEASE_METAL_3_2026-10-13.md)和 Release 附件。较大 UE Native/replay 差异仍为已知问题，不能计作修复；不扩大 supportsRaytracing、render-stage RT 或 Metal3 声明。

发布后回固定1cb8及实际保错3fed子集，沿真实 material/list producer、physical 版本和原提交定位最早恢复违规。材料列表审查仅取得来源 locator，没有 viewport 因果结论；不重复已关闭的 root/SH/count/Clear/world/AS-format/读回假设。串行锁、原时间/RSS/故障保护与外盘管理不变；后续不自动提交或发布。
