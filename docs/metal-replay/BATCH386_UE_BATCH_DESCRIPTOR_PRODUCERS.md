# B386：真实 UE shader 的批量描述符更新（定向通过）

UE763817d8的producer分为4/6/6/52项，共68项。原sourced preflight全帧最多2项，
无法表达这个合法单线程scatter。v45只把explicit DescriptorSlotProducer计数放到256，
每个24B仍必须CPU源slot、精确GPU预期字节、相同descriptor类型、Native源ResourceId和成员偏移闭合。
普通blit仍2项、非descriptor blit16项/64KiB；没有扩线程数、draw数或资源预算。
producer总数与普通blit计数分开，避免此前producer占用blit计数导致合法混合路径被拒绝。

复用B344真实UpdateDescriptorHandle metallib和原源/根表构建；从当前UE763817d8 XML提取实际shader，
以52和256个Native纹理ID代替旧captured IDs，全部payload/indices/destination字段显式标注。
消费者读取最后一项，GPU前后122→186和下一捕获186→122，四轮往返；每个produced slot都检查
NativeID一致和普通metadata，最后顶点/像素consumer渲染2×2全像素正确。
不把复制opaque captured地址当作descriptor消费者验证。

精确库6ddd8e91ee9e11033074210bff618f32a1c64db520f7276a2b3367398d57d4cd：
metal-ue-batch-graphics.iypzjr(52)、x9A70b(256)，四捕获/16 seek cycles、全部槽位和图像通过。
39+39 API/CLI negatives通过；256追加完整第257条producer/value/binding三元组后40组全部通过，
覆盖旧v44、源/目的offset和范围、producer/value/binding缺失/顺序/字节不匹配和计数上限。
异常均无Metal replay wait。最后一个slot memberOffset按表长度验证，不猜captured VA。
script util/buildscripts/scripts/test_metal_ue_descriptor_batch_graphics_macos.sh。

全量最近cf1bc8af v43的308/7786/3080通过，growth13123584B；本v45精确全量待组合执行。
完整UE GPU replay/实际MRT与人工UI尚未验收；UI锁屏。继续直接dispatch范围，目标active。
没有提交或推送。
