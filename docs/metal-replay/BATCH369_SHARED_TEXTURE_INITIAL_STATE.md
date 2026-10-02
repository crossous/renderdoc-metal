# B369：Shared纹理GPU初始内容

734378ff…实际UE包含Shared16³BGRA695和Shared320×240BGRA15322；前者有background
replaceRegion，后者没有任何background像素数据。Shared可CPU访问，不代表GPU写入前帧
数据已完整保存。对照D3D12/Vulkan的initial image机制，同样需要起点快照及回跳恢复。

统一B367子资源layout允许Private/Shared，Standalone及heap自动/placement纹理dirty跟踪
覆盖两类storage，IOSurface/framebufferOnly/view/buffer-backed自身继续排除。Native已提交
GPU完成后统一texture->Shared staging，initial record紧密byte保存与完整长度校验；回跳
buffer->texture完成后再执行捕获提交。日志明确输出storage，原trace环境变量保留兼容。

精确04a4de2d4e0bed02a2b846902d8d291ae3b1566e7e3413d8cf6e87b20a581bce，
metal-texture-subresources.YWtv4L Shared四格式×五类型20纹理原生GPU135/DEADBEEF、
连续两捕获、768×2子资源检查、八次seek、整数/浮点PickPixel、40×2 API/CLI反例通过。
完整Shared initial现在合法，原“Shared即失败”负例改为明确short-shared byte mismatch。
没有继续把合法Shared layout当负例，也没有撤销byte/归属/重复/mip校验。

运行：RENDERDOC_METAL_SHARED_TEXTURE_INITIAL=1 bash util/buildscripts/scripts/test_metal_texture_initial_subresources_macos.sh

Private148纹理兼容两捕获/11544子资源检查/八回跳/55×2反例通过（metal-texture-subresources.JnyHmC）。
Private view兼容四捕获/16seek/76反例通过（metal-private-texture.7LsbnK）。
CPU replaceRegion兼容GPU407/DEADBEEF/四seek/12反例通过（metal-cpu-upload.Qcqufz）。
实际b2b56a6c0f734ce85378885a0df81281621aa141f28476ebecd779d4050535c0，96,713,237B
自动重截并正常退出：489纹理/642042264B初始数据，独立packed byte计算零错误。
两个Shared来源695(16³/16384B)与15808(320×240/307200B)均有完整initial。
2030起点slot backing匹配、68producer全部匹配，frame-value/inline/lifetime/producer错误0。
另发现121个失效texture来源：75在首个CB出生前释放，46穿插首个未提交buffer blit的编码，
全部在首个shader工作前释放；v29严格CB边界/64slot上限仍会拒绝，下一批需独立验证。
最新旧库eda11f64…全量308/7786/3080已通过，本库未跑全量；人工UI/完整UE未验收。
持续目标active，不提交/推送。
