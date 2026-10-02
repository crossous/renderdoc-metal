# B433：沿用通用切片规则显示 3D MRT

Metal raw volume读取和MRT原Native replay已在B422验证，但RenderTextureInternal此前拒绝TextureType3D。实际UE两个64³ RGBA16Float MRT需要TextureViewer显示切片。

对照官方D3D12 d3d12_rendertexture.cpp::RenderTextureInternal中3D Slice和 hlsl_texsample.h，以及Vulkan vk_rendertexture.cpp / vk_texsample.h。保留既有Metal Native output pipeline，用texture3d<float>采样，z坐标为(slice+0.5)/当前mipdepth，nearest sampling；slice验证按当前mipdepth，先检查mip避免非法移位。64B显示uniform只使用已有末尾padding；原2D/array/cube绑定与显示流程保留。未改原capture纹理或生产shader。

59d97cf09846785dd28c7014ee786097be74275281c003e460af91c4aa78b75a：原B422的两份真实Native截帧，两个4³ MRT各四个不同值切片，两轮EID0恢复后共32次headless Native display，全体16pixels/readback每次均匹配原r或g=1/2/3/4，独立线性range0..4的RGB8量化验证(允许≤1unit)，NativeMetalValidation无错误。日志volume-display-first.log/second.log，helper metal_volume_display_replay.cpp。

人工qrenderdoc UI及当前59d97cf最新全量待完成，5366f35c的功能全量见B432。实际UE完整GPU回放仍待逐步推进。未提交推送。
