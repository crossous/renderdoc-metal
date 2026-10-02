# B374：cube视图复用父初始状态

实际b2b56a6c…有61个活parent texture view：RG11B10 cube->array10/cube->cube7、
RG11B10 2D4、R16Float2D32、BGRA sRGB->linear4、Depth32S8->stencil4。
视图本身不拥有像素。对照Vulkan image view baseResource/D3D12 resource initial恢复，
SnapshotTextureViewSources发现已捕获parent完整initial时不再分配Private copy或Shared额外缓存，
每次seek先RestoreReplayTextureInitialContents，再让view直接观察父数据；旧无initial的CPU历史源保持原baseline。

Private cube->cube新Native view允许完整6slice及有效mip范围。v33给已知完整initial的Private cube
source重定位Native GPU ID，维持8192方形/array1/depth1/sample1/14mip和common受支持格式范围。
RG11B10 tiny cube clear target沿用render pass/source initial校验；未扩大整帧shader/draw预算。

首轮YF06hh合法GPU/seek已过，但cube错误宽高反例触发Native断言退出；在Device Native创建前
补cube/cube-array square/depth/array/sample检查后拒绝，不再交Native断言处理。
55a572b5…metal-private-texture.cbchRw RGBA/BGRA cube view四捕获/16seek/76反例通过。
RG11B10第一夹具PLuA7w误仍使用RGBA格式，捕获shader结果巧合122但packed API验证失败；
纠正实际Native创建format并加入显式format断言。gI4Y2T的RG11 clear pass被sourced原RGBA格式检查
在GPU上传前拒绝，接通有初始状态的极小RG11 cube target后Yvhd4A两捕获/八seek/38反例通过，
GPU122/DEADBEEF、packed clear/restore与PickPixel .25/.5/.75一致。

Yvhd4A库精确c217f4a2dbbab17f194111032ede076a52ada3c6e336f7fe3f2e7b50acfe6a04，含当时尚未完成验证的v34大buffer source；新版全量未重复。
前a9a106aa…全量308/7786/3080、growth7045120B通过。真实UE未完整GPU replay，人工UI锁屏受限，
持续目标active，无提交/推送。
