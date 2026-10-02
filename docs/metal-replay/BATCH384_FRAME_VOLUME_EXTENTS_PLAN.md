# B384：实际帧内体纹理尺寸和二维浮点格式（定向与全量通过）

UE763817d8实际frame heap textures包含六个64×64×64 RGBA16Float(115) usage7；
v39只允许depth≤16，二维RGBA16Float64×64与RGBA32Float16×2也尚未在frame表范围。

v43候选沿用DescriptorTextureInitialSize通用mip/shape逻辑和B380 sourced texture闭包，
3D最大depth64（width/height64、array8、mip/sample1不变），二维RGBA16/32Float/R32Float
增加既有ShaderRead|Write及可含RenderTarget的tracked Private范围。原二维RGBA/BGRA/RG11
路径保留。typed source必须当前NativeGPU ID/明确ResourceId匹配；input-only texture writer
仍必须通过完整preflight与source usage闭包，不引入dummy output。

极小工作量仍只有单线程：写完整体积，读取一点，覆盖完整体积；框架test复用B380。
两个捕获独立Native footprint范围，检查每一texel、首末depth与角点/中点PickPixel，
GPU64/128/192/metadata、32 seek及EID0归零。Native heap预算最多16MiB（每heap，
aggregate仍256MiB），原≤42 heap预算1MiB；没有扩大实际UE整帧heap/residency预算。
案例为RGBA16/32Float64³及二维两格式；旧v42/heap容量/格式/options/Native范围/source/inline
反例候选31组/格式。原native/reflection/initial/upload逻辑复用，编译静态检查通过。

当前script test_metal_descriptor_frame_family_extent_macos.sh运行中，等待GPU/API结果。
此前a9ffdb1a v42定向16 captures/64seeks/208 negatives与旧21类674兼容通过；
最近精确全量4322af8 v41的308/7786/3080通过，growth11632640B。
实际UE整帧未GPU提交，current coverage未完成；人工UI锁屏未验。持续目标active。
没有提交或推送。

最终库cf1bc8af7e0404d3afd02928401a699e5ccf00c7a076242e6d45be14eb9fa6fd，metal-frame-family-extent.b060xp：8 captures/32 seeks/124 API+CLI negative groups全部通过。64³每个262144texel完整GPU写/覆盖及逐texelAPI比对，首末depth角点/中点PickPixel正确；二格式二维图像全像素通过。新库精确308份组合全量运行中，GPU串行，不替换运行库。

精确 cf1bc8af… 组合全量已完成：308 captures、7786 malformed cases、3080 lifecycle opens；resident growth 13123584 bytes。日志 build-macos-debug/local-m2-descriptor-replay/frame-family-extent-combined-regression.log。全量运行期间未替换库。
