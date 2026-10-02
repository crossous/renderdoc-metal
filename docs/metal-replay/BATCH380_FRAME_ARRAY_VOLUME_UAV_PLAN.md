# B380：帧内array/3D typed texture写入来源（进行中）

新UE bfa3f117…19 frame textures里6个RGBA16Float3D usage7、1个3D usage3、
3个R32Float2DArray usage3，以及RGBA32Float2D等；B379仅2D颜色，不能据此放行。
复用DescriptorTextureInitialSize的common format/shape逻辑验证logical bytes，并保留
placement Native heapTextureSizeAndAlign、storage/options/offset、tracked alias和出生顺序。
v39候选接受frame2DArray/3D Private tracked、usageRead|Write，width/height≤64，
depth≤16、arrayLength≤8、singlemip/sample；格式R32Float/RGBA16Float/RGBA32Float。
目前没有扩array/3D frame view、render/depth附件或间接dispatch、aggregate/heap预算。

新增test_metal_descriptor_frame_family_macos.sh：R32Float array两slice及RGBA16/32Float
3D两depth层，全部3×5×2=30texel。typed GPUID→Native单线程write/read/write，
root以背景scalar13共同验证buffer来源；回跳核对完整数据和两slice/depth的三PickPixel位置。
Native/capture已独立通过array64/0/0；初版LWJWT2 heap1024B小于旧replay4096B门槛，
夹具使用至少4096，不为此放宽driver。2wFHzw CPU preflight在setBytes拒绝，
原因root声明count2×stride8却含phase共24B；改用count2×stride16的32B root，
两个GPUpointer与普通phase/padding各自精确布局，沿用已有inline契约，无driver放宽。
当前精确b71144320507e5910167aedb039326afaad659fbe593cf682d14c146bc7ca6d6，
定向套件运行中，尚无完整Native/API/CLI通过声明；B378全量、B379定向及旧兼容已记录。
实际UE无完整coverage且没有提交GPU replay，人工UI锁屏，持续目标active，未提交/推送。

第三轮oevY6z CPU preflight通过，运行时writer dispatch被旧texture-pair/output-buffer限定拒绝。
对照D3D12 Serialise_Dispatch和Vulkan Serialise_vkCmdDispatch，它们按pipeline/命令参数执行，
不要求每个compute必须有输出buffer或同格式2D源/目标。这是通用dispatch建模限制。
v39保留旧路径，增加经过完整sourced CPU preflight验证的encoder资源闭包；Native运行仍需
ValidateComputeBufferBindings、threadgroup limits和slot0 inline bytes有效，直接texture bindings
必须为空；普通无coverage捕获不能使用该分支，间接dispatch也未扩展。
没有给writer加假输出buffer来掩盖此限制，使用无输出buffer的真实texture写入kernel验证。

noteResource把typed source加入encoderResources闭包，包含view父依赖；成功预检后保留每encoder
已验证闭包，失败清空。新的inline dispatch在loading按闭包记录保守CS_RW usage，复用已有
RenderDoc资源使用系统。array/3D usage3及usage7允许source读写，但color clear限定2D，
不因为source合法就放开未测array/3D render attachment。

精确7b4e0848352ada30725871ae0d34cc7bdaf9bc9d32ded861145a0e194540e070，
metal-frame-family.chfdYJ：R32Float array usage3、RGBA16Float3D usage7、RGBA32Float3D usage3，
三组原生/捕获独立GPU64/0/0或64/128/192通过，六捕获/24seek、30完整texel和六PickPixel位置、
typedID及EID0清零通过。基础26反例×3=78；额外missing inline layout/binding/bytes证明组
重跑29×3=87 API+CLI组均在GPU wait前拒绝。三个独立source-usage helper追加12seek通过，
每资源三条CS_RW event usage存在。库未改变，帮助测试新增usage断言；精确全量待运行。
旧21类674兼容正在串行执行，人工UI仍锁屏，实际UE未GPU replay，目标active。

7b4e0848…旧21类/674 API+CLI兼容通过。后续含早期v40容量的958d133d…全量
308捕获/7786反例/3080 lifecycle通过，resident growth12632064B；不能记成7b4库的精确全量。
