# B411：同一主表的CPU新槽位与GPU其他槽位写入

实际UE buffer24/off24384的新allocation没有任何先前producer写入该offset。整表modified/GPUWritten阻止初始化，尽管原Native命令只更新其他明确槽位。

候选64按逻辑槽位记录此前materialized读者，以及每个已编码producer的明确目标槽位；一般不明GPU写者另行记录opaqueTableWrites。仅未物化、未借用、无先前producer写入、无opaque写者的完整新槽位允许CPU初始化。GPUWritten表还必须有typed layout范围，preflight保存chunkOffset授权loading/active读取，保留原Native Shared更新和提交完成等待。此前已写/已读槽位不会获得这一新路径。

a45dc5e0/metal-mixed-fresh-cpu-slot.pEbAwW：6captures/24reset seeks/108indirect +24fresh CPU +24mixed API+CLI反例组通过。夹具在同一CB先producer写slot0，随后CPU写新slot24；原shader读取slot0、输出和MRT各像素正确，检查普通字段与多次reset。反例覆盖coverage63、此前GPU目标重叠、opaque写者、短/错offset/gen、缺/未知source。

实际UE pre-submit64越过混合CPU槽位，下一拒绝为frame中MTLBuffer::newTextureWithDescriptor，stream240320。正在核对其是否来自background buffer；无UE GPU上传/提交。此后补充direct-binding opaque写者检查，最终组合定向/全量待跑，UI未验，未提交推送。
