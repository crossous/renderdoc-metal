# B365：普通Private颜色纹理的初始像素

持续目标为当前UE整帧正确replay，本批仍在验证，不提交/推送。

## 通用机制与本地源依据

RenderDoc D3D12 `d3d12_initstate.cpp`保存各subresource的copyable footprint，
分别处理行数据和对齐行pitch；Vulkan `vk_initstate.cpp`逐slice/mip执行image↔
buffer staging copy，并在CPU读取前处理同步。Metal沿用这种逻辑像素/子资源
机制，不能把Native heapTextureSizeAndAlign的物理footprint直接当像素复制。
Apple MTLBlitCommandEncoder官方文档记录单slice/mip texture↔buffer接口：
https://developer.apple.com/documentation/metal/mtlblitcommandencoder

## 实现范围

把原BC-only initial state扩为有界2D Private颜色纹理：同一逻辑mip列表描述
width/height、block rows、紧密rowBytes、Native对齐pitch、逻辑字节offset。
压缩BC1/BC5不调用linear texture alignment query；普通颜色格式通过设备查询
Native alignment，序列化只保留实际像素行，去除对齐padding。≤4096每维、≤13mip、
总数据和单staging均≤32MiB；拒绝array/3D/MSAA/view/buffer-backed/depth格式。
格式使用现有GetTextureBlockShape，新增初始状态格式范围与读取/viewer范围
不能混为一谈。

普通Private standalone/placement创建时加入dirty tracking；在捕获起点已等完
应用提交，再GPU→Shared snapshot。明确退休的Native资源即使由旧CB保持引用，
也不能在下一捕获读取其失效分配。重放加载及每次回跳在新提交前恢复初始mips；
不调用Private contents，也不猜测地址/texture GPU ID。

v27仅放开已完整捕获初始数据的tiny背景Private RGBA/BGRA sourced texture；
同类背景辅助纹理≤8×8、≤4mip且计入tiny预算。Private颜色初始数据缺失需失败。
其余sourced预算/帧内GPU范围仍受原有预检约束，不能提交完整UE。

夹具新增Private RGBA/BGRA：GPU上传起点红64，frame采样得122，随后清除成
红192，验证全帧/dispatch/起点回跳。附加3×5/三mip不同像素行，验证packing。
连续两捕获对每格式检查Native身份重定位、普通字段、像素和四seek。

首轮RGBA小用例两捕获八seek通过。扩大夹具最初被tiny尺寸预算拒绝，随后仅
放开≤8×8/四mip辅助背景Private纹理，并要求完整initial data。反例损坏mip数
曾触发Native assert；补ValidTextureMipCount并复用placement形状校验，在Native
对象创建前拒绝。最终定向/兼容/全量/UI结果待完成后记录。


## 定向终端结果

精确21411d96e5b578486eb0ec22e7e7d7f8e3b8b8898414b94a4ffa8b96a3b24af0，`metal-private-texture.mHqfMf`：Private RGBA/BGRA四捕获、16seek、三mip
72字节逐像素验证、覆盖后复原、56 API/CLI反例通过。首轮T J827U因损坏mip
触发Nativeassert，已按上述预创建校验修复；最终反例无GPU上传/等待日志。
正在重截真实UE并验证初始颜色数据；全量和人工UI结果尚未核对。


## 实际UE重截

21411d96…库、原8f4bf566…插件自动重截49e66f85b360402a66be8ab5f22040d27006238934495139207472ca84659adc，
69,840,625B，42729chunk/294draw/88pass/peak26CB。Native起点363纹理的
186,669,370B初始数据已序列化，覆盖RGBA16Float40份、RG11B10Float35份、
R16Float11份及RGBA/BGRA等；当前320×240GBuffer各颜色格式数据明确存在。
这只是捕获数据验证，尚未把这些普通格式全部放开sourced GPU回放及viewer。
68GPUproducer全部匹配，lifetime/inline/frame-value/producer错误0。
`ue-private-color.initial-texture-profile.json`保存逻辑descriptor/字节数量。
完整UE没有GPU提交；正在执行精确库308份既有帧组合回归。


## 全量与人工UI

精确21411d96…组合回归308捕获/7786畸形输入/3080 lifecycle通过，
resident增长8,536,064B；`private-color-combined-regression.log`。
这是既有captured fixtures回归，新增普通Private初始数据另由上述新夹具验证。
人工UI未验收，实际UE整帧未GPU replay；继续B366 Private view恢复顺序。
