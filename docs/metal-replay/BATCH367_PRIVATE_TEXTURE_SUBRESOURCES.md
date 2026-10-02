# B367：Private纹理分子资源、深度/模板初始内容与公共像素解码

持续目标仍是实际UE整帧正确replay；本批定向成功不能代表整帧成功，不提交/推送。

## 通用方案

沿用本地RenderDoc D3D12的copyable-footprint行复制和Vulkan的layer/mip/aspect复制。
3D GetTextureData返回整个mip体积，与D3D12/Vulkan处理一致；PickPixel的slice选择z。
格式转换复用maths/formatpacking.cpp DecodePixelData，UInt/SInt必须保留PixelValue
union的位模式，不能强制转float。Metal特有部分仅为Native blit、alignment和D32S8 aspect options。

## 实现

InitialTextureLayout统一2D、2DArray、cube、cube-array、3D，逐slice/mip保存紧密行及
深度切片；同一Native CB复制一个纹理的所有子资源。逻辑字节去掉Native row padding。
D32S8分别复制float depth和uint8 stencil，紧密序列化为每像素5字节的两plane；
公共GetTextureData仍按RenderDoc惯例合并为8字节D32S8，尾部3字节为0。

复用既有format block shape，增加实际UE使用的UInt/UNorm、BC3/BC6/BC7等格式。
边界为各二维维度8192、深度256、arrayLength128、mip14且符合完整mip几何；
单纹理逻辑/staging各128MiB，拒绝MSAA、buffer-backed/view自身initial、framebufferOnly。
捕获dirty parent及已有view->parent恢复顺序保持；所有initial record先验证长度/重复/归属，
完成整个loading/preflight后才上传。此改变未扩大sourced descriptor GPU coverage v28。

ReadTextureSubresource同步扩展上述格式/类型，保留tight rows/block bytes，体积返回所有z。
PickPixel使用公共Decoder，覆盖整数/浮点、UNorm和depth/stencil；压缩像素仍不逐像素解码。
RenderTexture viewer的UInt/volume/cube pipeline仍需后续适配，不能把API读回当成UI验收。

## 定向终端结果

精确库eda11f6411b614c880017f2ef6aceee197f3359af892f0bc8d702b6453d2c94c，
metal-texture-subresources.UE7RYh：33格式/148个3×5或3×3小纹理，原生Metal与注入截帧GPU
135/DEADBEEF通过。连续两份捕获分别5772次分子资源byte检查、四次覆盖/dispatch/起点
回跳通过；共11544次byte检查，UInt/float/Depth16/Depth32/D32S8 PickPixel验证通过。

Depth16原生clear将0.25量化为0x3fff，初版测试预期0x4000失败。改为原生blit分别读回
depth/stencil独立核对，所有slice/mip Native clear值通过，未修改驱动掩盖这个测试差异。

两份捕获各45组API+CLI反例，共90组：2D/array/cube/cube-array/3D/D32S8/BC3 mip数据
缩短/延长、重复、错误ResourceId、不匹配mip、RGBA改Shared，全部在initial GPU上传/等待
前拒绝。没有对不受Metal支持的Shared depth/BC组合制造Native assert。

运行：bash util/buildscripts/scripts/test_metal_texture_initial_subresources_macos.sh
负例：python3 util/test/metal/metal_texture_initial_subresources_gate.py CLI OPEN_PROBE CAPTURE OUTPUT_DIR

## 真实UE、全量及人工UI

精确eda11f64…Private view兼容四捕获/16seek/76反例通过（metal-private-texture.WAti6A）。

实际734378ff5976bac8396f52972dcd46957333415f2f7ef136c374500c03e8467a重截保存，
91,542,057B；485纹理/622,624,824B initial，独立CPU layout字节计算零错误。
955 frozen texture source中546有纹理initial、357有buffer initial、49在首个CB创建
前精确generation free、2为Shared texture、1为Drawable。Shared 3D695已有replaceRegion，
Shared320×24015322没有background数据；继续检查Shared初始像素捕获。

49个stale texture来源不应被认为可直接重定位；CPU证明其49个slot全部在30905首个
CB创建前释放，但heap buffer birth穿插导致v28连续free prefix只能证明13条。
下一批按无GPU编码的严格CPU前导规则适配，保留原版限制和完整generation/初始字节验证。
1952起点slot backing值匹配，68producer全部匹配，帧内value/inline/lifetime/producer错误0。
精确eda11f64…全量308捕获/7786反例/3080生命周期通过，resident增长11,403,264B；
texture-subresources-combined-regression.log。人工UI与实际UE整帧GPU尚未验收。
上一精确21411d96…库全量308捕获/7786反例/3080生命周期已通过；不冒充当前库结果。
实际UE无完整DeclareDescriptorCoverage，仍必须先补资源来源、同步/alias、预算及间接参数
验证，未执行整帧GPU replay。人工UI尚未验收，持续目标active。
