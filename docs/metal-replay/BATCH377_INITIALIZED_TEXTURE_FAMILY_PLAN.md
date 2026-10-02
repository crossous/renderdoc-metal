# B377：背景初始纹理家族与 aspect view

v36扩展CPU初始布局预算至已有initial-state支持的2D/array/cube/cube-array/3D和Depth16/32/D32S8，
逐slice/mip/volume计算逻辑字节，D32S8 wire为depth4+stencil1；保持128MiB单资源/256MiB总预算，
frame-born尺寸/heap预算未扩大。来源GPU ID只从已验证完整initial的Native资源取得，aspect view引用父initial。
父view依赖纳入submission resource集合，并拒绝已aliasable的base/view；color attachment clear记录资源依赖。

新定向fixture复用已有158纹理/35格式Native上传与API子资源检查：159个typed texture descriptor（含X32Stencil8 view），
Native shader通过table访问2D/array/cube/cube-array/3D/depth/stencil，独立预期309/DEADBEEF（旧四类135+cube-array第2 cube的3×3面中心93+depth64+stencil17）。
Stencil API从父D32S8公共读回提取1B平面，PickPixel沿用公共S8 decode（G通道）；不将Native aspect view blit猜成线性像素。
metal-texture-subresources.6aoXer两捕获/八回跳/12312子资源检查、159个typed texture slot元数据及七类GPU309/DEADBEEF、Stencil三mip原始平面/PickPixel通过；55+10组反例每捕获，共130组API+CLI反例在GPU上传/wait前拒绝。精确ada72925792b136bf6106de0f5dee10e43ca1781152a65fbf6ae645b03db0c87。

实际b2b56a6c…冻结drawable17291是上一帧900×640 BGRA、Managed(1)、初始字节0。
slot(buffer24,offset3336,generation6456)在frame34896才Free，晚于首次GPU dispatch31810，
不能按未提交前导退役规则忽略。WrapDrawableTexture从未MarkDirty，initial layout只支持Private/Shared；
后续需极小背景drawable initial验证及Managed GPU快照，不可默认为“先写后读”。

整帧/UI未验收，目标active；没有提交/推送。

首轮gI1Ro9 MSL depth.sample返回scalar，错误.r在Native编译时拒绝；已修正。twryMR Native结果309/DEADBEEF，原期望299误将cube-array面中心当作左上角；独立手工pattern确认中心x1/y1/face6为93，期望修正309。

ugW10b/3HaihG发现Stencil原始平面正确但PickPixel0：公共DecodePixelData 8-bit Depth未归一化，对照已有16-bit Depth分支及Vulkan S8的CompType::Depth，补同一公共8-bit归一化规则。新S8 Depth/UInt测试8断言、完整[format]四test cases/141027断言通过；保存common-format-unit.log。全量最近精确e3c11635…308/7786/3080通过、growth0B，ada72925…新版全量待组合后运行。
