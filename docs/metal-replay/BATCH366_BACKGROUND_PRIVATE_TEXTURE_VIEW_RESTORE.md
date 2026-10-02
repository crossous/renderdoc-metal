# B366：背景Private texture view起点恢复

持续开发中，不以tiny pass代替实际UE整帧。精确21411d96…的全量正在执行，
只编辑源码，不在运行过程中重新编译/替换库。

## 问题与通用机制

新增普通Private纹理初始数据后，发现`SnapshotTextureViewSources`在初始像素
上传之前创建Native baseline copy；随后上传captured data后，已有ResetView
会把未初始化副本复制回parent。这会让纹理view第一次读取及后续回跳错误。

沿用RenderDoc Vulkan imageView / D3D12 SRV 的显式parent resource依赖：view
自身没有独立像素初始状态，只通过parent检查完整初始数据。Metal只需为新
Native view重新编码gpuResourceID，并在parent初始上传之后建立baseline。
`GetReplay()->SnapshotTextureViewSources`移到首次Shared/Private初始恢复之后，
每次seek仍按parent initial→view baseline reset顺序执行。

新增明确view→parent ResourceId map，由已有`Serialise_newTextureView`登记。
v28 tiny背景Private sourced view只接受有完整parent初始数据的单层、同format
1mip RGBA/BGRA；不猜测GPU ID和父对象。Frame view继续沿用v26出生顺序。
Native baseline copy只用ShaderRead/PixelFormatView，不申请blit不需要的
ShaderWrite/RenderTarget，避免给采样格式增加不支持的usage要求。

## 验证设计

给B365资源夹具加入Private subset view：descriptor指向view，frame clear写
parent，视图采样和原始readback应随parent变化；起点restore之后恢复原始颜色。
连续两捕获×RGBA/BGRA、16seek、三mip逐字节检查；追加5组view依赖/范围/
format/丢失创建/错绑parent反例，总76 API/CLI组。版本27拒绝背景Privateview。

尚未运行新版GPU，因为21411d96…全量正在执行；定向/兼容/全量/UI待记录。


## 定向终端

精确0eb096fd697311129fd96a19c02f98e4552da0fbae6f57e47160046b942d19b0，`metal-private-texture.ybcNUI`：四捕获/16seek/76 API/CLI反例通过，
Native view GPU ID在重建后为4、captured为2，descriptor使用view身份，parent
被覆写后的颜色和回跳恢复颜色/三mip逐字节全部一致。Native阶段/捕获和
replay均启用Metal Validation。每份颜色纹理initial capture/restore增加局部
autorelease pool，避免完成的Native CB持续保持staging allocations到外层pool。
正在复验direct Private初始像素及frame view MRT兼容。没有重复全量；上一批
21411d96…组合308/7786/3080通过，growth8536064B。人工UI未验收。


同一0eb096fd…库：`metal-private-texture.jCKn2z` direct兼容四捕获/16seek/56反例；
`metal-frame-mrt.YBO0h8` frame Private view serial/parallel two/five MRT兼容
八捕获/32seek/387反例通过。继续统一扩展实际array/cube/3D和格式初始状态。
