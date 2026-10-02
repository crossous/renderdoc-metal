# B424：带帧内 staging 的原生 buffer→texture 上传

真实 UE frame chunk38396/38397：Shared frame buffers17868/17869（各4MiB）上传 BG Private/Tracked targets4914 BGRA8_sRGB1024²、4681 R8Unorm2048²，原生 offsets0、row4096/2048、image4MiB、完整region。旧预检未覆盖 chunk1208，日志 copy-proof 没有普通buffer-copy诊断，因为这个同名 chunk 是 texture upload。

沿用官方 Vulkan Serialise_vkCmdCopyBufferToImage 原样 native replay 和原生 copy action/resource usage。抽出现有 Metal ValidLinearTextureCopy 的 Native target + bufferLength 校验，既有 Native serializer 与 descriptor预检共用，避免两套 pitch/format/subresource规则。coverage65 限定既有 BG initial contents 的 Private/Tracked 2D R8/BGRA8_sRGB≤2048、Shared合法 frame/background staging、live encoder、非descriptor表/alias；原有 pixel/range/row/image/options校验原样。与普通 buffer copy共用256调用/16MiB合计footprint预算；partial纯buffer-copyplan排除含texture upload的提交。GPU前检查通过后才保留原生copy，CPU数据仍经已有 submission-owned Shared snapshot。

精确cdf6b4fccca14c362adf88d3839aa4c469dccfb8ef9242879e93fa403827dab4：metal-linear-upload.Bx59i7，两格式4 captures/16 reset cycles。两个独立 frame staging buffers：offset32、row256，首次完整8×4上传，第二次2×2局部覆盖origin1/1。初始/首copy/最后copy每个pixel原始bytes与Native一致（sRGB没有伪CPU转换），第二帧初始内容也精确恢复。38 API+CLI malformed groups（19×2）在GPU前拒绝：source/target identity/type/range/row/slice/mip/options/unknown或endedencoder/缺initial。库前后一致。

真实 UE cdf6b4fc pre-submit 已越过两个 4MiB 上传，下一处是已提交旧generation之后的slot reuse（B425）。未提交整帧 UE GPU、未验收人工UI，无提交推送。
