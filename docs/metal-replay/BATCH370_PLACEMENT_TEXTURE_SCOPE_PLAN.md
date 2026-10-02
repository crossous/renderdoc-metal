# B370：实际placement纹理范围

实际734378ff…CPU审计/Native query发现BC3/BC6/UInt/UNorm、Shared BGRA及cube-array来源。
当前普通Serialise_newTextureWithOffset仍仅Private、旧format白名单、2D/array/cube/3D，
与B367已经验证的standalone initial/readback范围不一致。因此不能仅打开descriptor coverage
就期待完整UE；须以小placement原生例证明Native storage、heap offset/size/format和回跳，
再扩大普通资源创建范围。Native查询报告并不证明GPU复制、资源生命周期或实际shader可用。

后续复用GetTextureDataBlockShape与D32S8 aspect布局，但保留Native heapTextureSizeAndAlign、
对齐/范围、tracked alias及完整mip几何校验；sample=1，限制budget，Typed buffer view另测。
NativeHeap->storage与descriptor options必须一致；Shared内容用B369初始快照恢复。
同时保持sourced GPU contract v29的tiny输入限制，先验证普通资源创建。

本文件仅列当前实测范围缺口；没有实现/通过声明，不提交/推送。

实现复用GetTextureDataBlockShape白名单，保留已有DepthStencil/RG8Uint格式；加入cube-array形状
和Shared storage。要求descriptor storage与heap一致，options只能对应storage/default或tracked，
所有BC initial要求0x10以上；Native布局对齐/范围、overlap/lifetime规则保留。
未扩大sourced GPU预算/纹理限制，仍最多v29，实际UE不能据此直接提交。

精确e4d05e4fbc512646318df2ef745883e4ddf8284e43b1074fb22dfe6aa5e26de8：
Private35格式/158 texture（metal-texture-subresources.r46jnh）两捕获/12312分子资源检查/八seek/
110 API+CLI反例通过，新增R16Unorm及RGBA8Uint PickPixel；5个Heap-storage-mismatch
反例在Native对象创建前拒绝。Shared20 texture（metal-texture-subresources.vkdCRb）
两捕获/1536检查/八seek/80反例通过。早期Private148纹理同样通过（VpRgul）。

旧21类/674反例兼容通过（metal-descriptors，placement-texture-old21-compat.log），精确e4d05e4f…全量308捕获/7786反例/3080生命周期通过，resident增长9928704B；人工UI与完整UE未验收，持续目标active。
