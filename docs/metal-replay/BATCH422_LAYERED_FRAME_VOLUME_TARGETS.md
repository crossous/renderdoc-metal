# B422：保持原生 3D 分层 MRT 通道

实际 UE pass 17755 绑定两个 frame-born Private/Tracked RGBA16Float 64×64×64 volume（17499、17505），Load、deferred Store、arrayLength64。背景 initial uploads 和整帧 GPU 执行前的预检原先仅允许 arrayLength≤1。沿用官方 Vulkan Serialise_vkCmdBeginRenderPass：保存原生 pass、attachment、subresource 和每 command buffer 状态；Metal 原生 descriptor 已序列化 array length、slice、depthPlane，不创建替代 2D view。

coverage65 增加有限分层 volume：既有 ValidDescriptorFrameTexture，3D RGBA16Float≤64³、mip1/sample1/array1、Private/Tracked/RT+RW；pass layers1..depth，所有 attachment 必须支持相同 layer range，无 depth/stencil、非零 slice/level/depthPlane、resolve 仍拒绝。Load 必须有既有 compute typed-UAV predecessor 或原生 clear。其他背景/2D 分层仍拒绝。

极小 4³ Native fixture：第一 volume 原始 Native compute 全体写入，第二 volume 原始 layered clear，然后两个 Load target MRT，原始 vertex instance_id/render_target_array_index 四层 draw。Fragment typed table 输入13，每层 first.r/second.g=1..4。全128voxels 的原始 half bytes 与 Native 精确一致，2captures/8 reset seeks，20 API+CLI predecessor/zero-work/UAV/layer overflow/subresource/mixed-shape rejection groups，全部 GPU 前拒绝。第二 capture 包含历史 background volumes，helper 按 EID0 frame resource removal 筛选当前 frame births，不能混用历史相同形状纹理。库 8c40e6d04a108946c552c6ce84330fb96bd525559d68341ce7f9bdd2272b5777 前后一致。日志 metal-layered-volume.EGKuV0。

真实 UE pre-submit 已越过 layered MRT、推进到后续 descriptor free（B423）。未提交整帧 UE GPU，也未验收 UI。无提交推送。
