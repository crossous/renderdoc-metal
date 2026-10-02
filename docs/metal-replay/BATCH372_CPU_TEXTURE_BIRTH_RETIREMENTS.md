# B372：退役前导中的buffer view与heap texture创建

实际b2b56a6c…CPU审计严格区分现行v30和扩展候选：v30于31573 buffer texture birth停止，
只覆盖66冻结槽；增加两类CPU创建后的候选于31665 CPU内容更新停止，包含122冻结槽退役/
121失效texture来源，五条已知frame buffer复制15200B。两类纹理创建本身不提交GPU，
但CPU候选不等于完整replay合法性；192×104 RG11B10 heap texture仍超现行sourced小尺寸范围。

对照Vulkan vkCreateImageView的AddParent/baseResource/baseResourceMem以及D3D12
独立descriptor shadow，v31在v30未提交前导内允许buffer texture birth、heap texture birth。
buffer view沿用ValidateMetalBufferTexture的Native alignment、逻辑尺寸、parent storage及range
校验；不得使用未知、尚未出生或已alias的parent。将view backing加入提交/alias依赖，
preflight使用捕获GPU ID、runtime使用新Native GPU ID，回跳按既有frame view释放/重建。
heap texture仍使用已有严格sourced创建/Native footprint检查；不扩大shader、draw、heap预算。

定向夹具扩展四捕获/80退役：frame Shared buffer view前穿插创建，普通blit复制byte1..16；
额外typed table绑定frame view以检查其GPU ID重定位，每次seek读取view pixel byte1..4。
另创建Private tracked placement 2×2纹理，检查合法CPU创建可穿插；两者均在最后退役及首次提交前。
新增v30降级、parent/range/storage/width/order及heap offset/width反例。精确a6c6c93e7362a8676978ec7085e472ca5d8634ff076e8377415e11b69550ec98，
metal-retirement.76BP68四捕获/16seek/128 API+CLI反例通过。
首轮ZtGyXn已通过计算和view像素，但测试helper以2×2尺寸误选新增heap纹理作为drawable；
改用捕获的CAMetalLayer明确Texture ResourceId后完整通过，driver未为此改动。

前版978f9a40…全量308/7786/3080通过，growth12058624B；期间仅编辑源文件，
未重编译/替换测试所用dylib，v31定向已通过，旧21类/674 API+CLI反例兼容通过；未重复新版全量。
当前实际UE仍无完整descriptor coverage，尚未GPU replay；人工UI未验收，持续目标active，未提交/推送。
