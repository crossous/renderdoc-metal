# B364：帧内 texture view 的 sourced GPU ID

本批定向验证及旧21类兼容通过；当前实际 UE 整帧正确 replay 仍是持续目标，
本批没有重复执行全量，人工UI未验收；未经要求不提交或推送。

## 实际源依据

0649fac8…捕获有14个帧内 texture view，各有一个显式 Texture slot source：
15366→十个R8 view，父纹理256×128、8mip；15399→两个BGRA8 sRGB转线性
view；15514/15516→两个Depth32Float_Stencil8的X32_Stencil8 view。后两组
父纹理为2744×1696；320×240 scene viewport并没有将整个编辑器渲染限制到
同样尺寸。ignored `ue-heap-references.future-view-profile.json`保存API字段。

UE源码`MetalUAV.cpp`使用Native parent、目标format/type、MipRange和ArrayRange
创建SRV，然后`InitAsTextureView`传给descriptor工厂。对照RenderDoc Vulkan
`Serialise_vkCreateImageView`保存视图身份/创建参数/父image；D3D12
`CreateShaderResourceView`更新逻辑descriptor shadow并记录源resource引用。
继续沿用这类依赖/身份机制，Metal部分只编码Native GPU texture ID。

## 最小实现范围

v26先验证frame Private placement 2D RGBA/BGRA、2×2以内、same-format单mip
subset view。Metadata记录显式view→parent ResourceId，投影描述不新分配heap；
API顺序预检要求parent出生后再创建view，view出生后才绑定其source。Native
创建继续调用已有`Serialise_newTextureView`，回跳沿用已有frame-view优先释放/
重建顺序。Slot source重新读取view当前Native GPU ID。

父纹理明确makeAliasable后，已有Native view继承capture逻辑退役，避免通过
保守GPU-identity引用把旧分配带入下一帧initial section。保留Native对象的
GPU生命周期，不对view调用新的Native退役方法。

小用例扩展frame MRT：第一pass写parent，第二pass通过view descriptor采样，
serial/parallel × two/five MRT；覆盖回跳、Native parent/view ID和像素。
追加17组view出生/父依赖/format/type/range/swizzle反例。尚未运行新版GPU：
正在完成精确246a9d5c…上一批全量回归，不能在过程中替换动态库。

真实R8多mip、sRGB转换、stencil和大纹理范围仍待后续扩展，不能以本批
same-format tiny view验证代替实际UE接受。

## 定向终端结果

精确库a167612e9aab5a5253d2a583a01667d6a1732d2976b3a885c14b2c1fa10cf32b，
`metal-frame-mrt.UF0tLK`：8捕获/32seek/387反例通过。Native parent/view GPU ID
确实分别为4/5和7/8，回跳时view ID重新生成，像素122/186与186/122一致。
第一份AFXzBH首捕获已经通过四seek；第二份因native ID数值偶合被夹具误拒绝，
新增16个无GPU工作的tiny texture padding固定分配顺序，驱动未因该测试修复改变。

新增窗口控制plugin8f4bf566e2ca67c1914b5a94a5172fe6aacd0bc8413989edfc4fa8b74649f808，
隔离cached clang编译/link通过，保存原installed源码和ec8301be…dylib到
`build-macos-debug/ue-plugin-window-mode`。捕获期间转Windowed并解除最大化，
结束恢复先前模式/尺寸/最大化状态；日志记录请求/实际尺寸。引擎原MetalRHI
仍为ace97360…，未运行UBT。正在自动重截实际UE。

本批精确库的旧21类/全量和人工UI仍待核对；不以tiny view通过替代整帧接受。


## 实际UE重截与尺寸修正

8f4bf566…插件/a167612e…库先重截d30a6abe…，50,753,382B，294draw/88pass，
GPU producer68、inline1868全部匹配，frame value/lifetime错误0。窗口解除最大化
生效，UE最小客户区把640×480钳制到900×640，但GBuffer仍2744×1696。
对照UE `SceneTextures.cpp::FSceneTextureExtentState::Compute`，编辑器默认Grow，
除非`r.SceneRenderTargetResizeMethodForceOverride=1`，否则忽略resizeMethod。

自动添加`r.SceneRenderTargetResizeMethod 0,r.SceneRenderTargetResizeMethodForceOverride 1`
后session20261001-192540重截9738816a9412462ceafb6d8cedb8a216f09ee9a11d173e3eab7cf96f37a87eb6，
46,815,289B，328draw/168pass、峰值27未提交CB、32signal/0GPUwait；GBuffer明确
创建320×240，半尺寸160×120。材质、阴影、历史logical references保留其尺寸，
不能把全帧视为所有资源均320×240。1917初始活slot及bytes、114producer全部匹配，
lifetime/producer/inline/frame-value问题0。1956Native placement，279历史overlap，
其中4typed backing对完成CPU归属审计；这不是GPU完成证明。没有完整UE GPU replay。
ignored `ue-requested-size.*`与工程Saved中的独立SHA文件保留证据。

精确a167612e…旧21类/674反例通过，`frame-view-old21-compat.log`。本批没有重复
执行全量；上一批246a9d5c…308/7786/3080通过。继续普通Private纹理初始内容。
