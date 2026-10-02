# B385：实际 UE 帧内整数、单通道和打包纹理（定向通过）

UE763817d8 frame heap 包含 R32Uint(53) 二维和数组、R8Unorm(10) 二维、RGB10A2Unorm(90) 3D。
v44 沿用 B380 sourced closure、Native footprint 和公共 initial-size 解码，增加这些格式。
限定 Private/Tracked、恒等 swizzle、mip/sample1、width/height512、array8、depth16；
R32Uint 仅2D/Array、R8仅2D、RGB10A2仅3D。usage3/7；R32Uint2D额外允许usage35。
其它 buffer、shader线程数、绘制和aggregate预算不变。

fixture 复用 frame-family：R32Uint三层数组、128×1二维、128×1真正texture原子操作、
R8Unorm512²、RGB10A2Unorm1³。原子路径按 Apple Metal Shading Language Specification
Metal3.1的atomic_exchange和atomic_fetch_add，先置零再加64/192，每个texel明确初始化。
https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
不把创建 ShaderAtomic 标志当作执行原子操作的验证。

检查完整 raw texel、PickPixel、typed Native GPU ID重定位、源GetUsage、EID0和四次重复seek。
UInt的未使用alpha遵循RenderDoc公共DecodePixelData默认0；没有修改公共解码器以迎合fixture。
script util/buildscripts/scripts/test_metal_descriptor_frame_numeric_macos.sh；GPU串行运行中。
旧精确cf1bc8af v43全量308/7786/3080已经通过，growth13123584B。
实际UE完整GPU replay及人工UI未完成；UI仍锁屏。保留改动，不提交/推送，目标active。

最终库及结果见 metal-frame-family-numeric.LdYd1i/library-hash.log：10 captures/40 seeks/160 API+CLI negatives全部通过，包括真实atomic_exchange/fetch_add。首轮H1ojx5的显式MTLLanguageVersion3_1选项被现有source-library白名单拒绝（GPU之前），没有修改supported标记或伪造成功；默认语言选项编译并执行同一原子函数通过。非默认source编译选项完整序列化仍是独立未实现范围，真实UE使用newLibraryWithData二进制路径。

精确v44库4d69dbb0c749895540600d014ec005c0fb6677ebcb0ba34693e7bc2e0543f568。
