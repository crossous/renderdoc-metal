# B387：实际 UE 直接计算几何（定向与全量通过）

UE763817d8实际84个direct dispatch，合计1237551threads，单次最多262144；
典型volume是16³ groups×4³ threads，320×240二维是40×30 groups×8×8 threads。
旧sourced contract只有4次、每次单线程，因此即使地址完整也不能实际运行该帧。

v46保留source/inline/epoch/native/allocation完整验证，direct dispatch允许128次、
每次262144threads、全帧8Mi threads预算。用除法逐轴检查避免乘法溢出和零维。
动态threadgroup memory仍拒绝；没有扩展indirect或draw路径。
MetalReplay::ValidateComputeThreadgroupSnapshot从现有runtime校验抽取：原runtime调用同一函数，
CPU preflight使用pipeline快照和空dynamic-memory，保留设备最大维、pipeline最大总线程、
threadExecutionWidth编译承诺、静态/动态threadgroup内存和最小绑定规则。
不是另建一套放宽校验；遵循D3D12/Vulkan先保留管线/资源快照再执行Native命令的结构。

复用frame-family夹具，work writer为每线程唯一texel，明确边界和写入坐标，避免原单线程
遍历全纹理代码被262144threads重复执行。读consumer仍1线程；往返回放检查完整raw数据、
首末slice PickPixel、typed source GetUsage、NativeID重编码、EID0。

精确库466b749c7370513f71bf0d088e5dd1d9c0b26ab478ba37c0e3436817ffa0185a：
metal-frame-family-work.nu7sc5八捕获/32 seek cycles通过：RGBA16Float64³、R8Unorm512²、
R32Uint128×1并行writer及128次compute encoder。40/40/39/39共158 API+CLI negatives，
包括旧v45、零维、Native最大threadgroup、单次work上限、整帧work预算、129次count、
UINT64_MAX溢出、资源/inline/source/format/heap范围，全部无replay wait拒绝。
script util/buildscripts/scripts/test_metal_descriptor_direct_work_macos.sh。
精确本库308/7786/3080组合全量进行中，GPU串行，不替换运行库。
实际UE整帧和人工UI未验收；UI锁屏。继续真实复制和indirect参数，目标active，不提交/推送。

精确466b749c…全量308 captures/7786 malformed/3080 lifecycle通过，resident growth8880128B；日志direct-work-combined-regression.log。运行期间未替换库，包含最终v44/v45/v46源。v47候选源尚未编译。
