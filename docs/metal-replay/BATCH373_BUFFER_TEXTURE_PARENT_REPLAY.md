# B373：Private TextureBuffer的父buffer恢复与读回

b2b56a6c…实际UE有359个活background TextureBuffer，16格式，parent initial bytes均存在，
最大parent18350080B。此前Native创建支持部分格式，但typed sourced GPU ID只允许极小2D；
API纹理读回也不支持type9。对照D3D12 buffer SRV/Vulkan view base resource，使用明确parent
ResourceId/Native buffer pointer/offset，不重复保存view像素。恢复沿用Private buffer initial路径。

v32 buffer view在创建时登记parent；source relocation验证完整parent初始bytes或frame出生。
API GetTextureData/PickPixel type9从父buffer精确view range读回，复用GetBufferData完成等待和
Shared staging；不对TextureBuffer发texture blit。common format shape补充R32Sint/RGBA8Snorm/
RGBA16Snorm，Native buffer view validator补R8Unorm/R16Uint/RGBA8Uint/RG16Uint，保持Native alignment、
Private storage、options、height1、mip1、range限制。

小用例的覆盖需要在compute后buffer blit覆盖view数据。v32允许已知live Native/frame buffer
普通copy（最多16/64KiB累计），必须有效range、不同ResourceId且双方非descriptor table，
复用encoder->CB映射、未提交检查、noteResource/modifiedBuffers和提交等待；typed24B copy旧规则保留。
未扩大shader/draw/heap预算，无整帧coverage绕过。

精确a9a106aa333100768288955b446ed56419e40adc4228b977ccd83e6690317e4b，
metal-buffer-texture.HeDH9g两捕获/八回跳，Private RGBA8 TextureBuffer offset256，GPU通过重定位后
bindless texture_buffer<float>读到122/DEADBEEF；compute后普通copy覆盖，再回跳恢复64/128/192/255，
GetTextureData/PickPixel一致；40 API+CLI parent initial/view range/storage/source/order/copy反例拒绝，
未触发初始GPU上传或GPU等待。Native shader第一轮read(0)有uint/ushort歧义，明确uint(0)后通过。

精确35b6237ef58575222f1fd7472633f573b7bc3e171422a7b3cb76b4f0e65be981，
metal-buffer-formats.Q2i5vF：16实际格式独立Native float/uint/sint shader decode通过，两捕获/八回跳/
128 raw+PickPixel检查，负整数-17、offset及overwrite恢复正确，28反例拒绝。首轮Ae8ZFa在
CPU Native创建时拒绝遗漏RG16Uint，补validator后通过；未向GPU提交该失败重放。
同库metal-buffer-texture.piSMR7 typed parent dependency兼容两捕获/八回跳/40反例通过。精确a9a106aa…全量308/7786/3080通过，resident growth7045120B；
后续扩展background view parent提交/alias依赖，尚未以该后续库重复全量，旧a6c6c93e…21类674反例已经通过。人工UI受锁屏限制，
实际UE仍无完整descriptor coverage，未GPU replay，持续目标active，未提交/推送。

CPU内存布局审计ue-shared-texture.memory-layout-audit.json：八heap声明总4GiB，
Native引用range span约2.86GiB、range union约1.24GiB；初始buffer673124816B加texture642042264B，
总1315167080B。以上不是驻留/同时活跃度，未调整heap大小，未提交GPU。

2026-10-02重新核对GitHub默认分支仍c4be68bb…，HTTP2读失败后HTTP/1.1重试成功，
本地HEAD一致，所有dirty/untracked保留，未提交/推送。
