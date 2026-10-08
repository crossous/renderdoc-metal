# B483：无输入 TLAS 查询与 UI header 回调生命周期

最终 backend/bundle SHA256：
32333bb6491f52936758acdcb42ade8a3e5ee79d367db43fd5a04cef44612e87
GUI binary：3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。

## 查询接口

实际 UE RHICalcRayTracingSceneSize 只设置instanceCount就查询。参考Vulkan
vk_get_funcs.cpp:1345 的GetAccelerationStructureBuildSizes转发和DX12
 d3d12_device_wrap5.cpp:333 的PrebuildInfo转发，分离query-only描述与实际build。
复制并unwrap静态default/indirect实例描述；查询可没有buffer/children，有buffer时
允许Private及合法offset。支持None/Refit、固定合法stride和有界count；motion等
未扩展。macOS14/iOS17 indirect API有可用性守卫，旧OS不会引用新枚举。
不因查询成功放宽实际AS创建/build、GPU身份重定位或生产能力。

现有五组primitive/Shared/Managed/Private查询和新增五组unbound default/
indirect count1/64、Private indirect offset64：AS size/build/refit及heap size/align
逐项native与injected一致，原descriptor/对象引用未修改。Apple gate现集成两组。

官方两个scene再次native/capture/offline逐字节一致，44/46事件各三方向、CLI×3、
41坏输入、6项能力查询PASS。当前gate-results为32333bb6，cad38602已归档。
结果build-macos-debug/metal-ray-b483/sample-manifest.json。

真实UE session20261005-171524/manifest在ue-ray-diagnostic/b483-32333bb6：
查询修复生效，下一失败Metal acceleration-structure selector gpuResourceID未捕获。
尚无可验收RT capture，RT dispatch/离线未验。下一项按VK/DX12 AS GPU地址资源
关联/实例重定位补AS资源身份、indirect TLAS与IR header，不扫描任意整数来猜地址。

## 用户 UI 崩溃与已验证修复

用户栈QtWidgets+0x81e3c→QCommonStylePrivate::viewItemSize→RichTextViewDelegate。
本机Qt5.15.19相同偏移为text layout函数prologue保存栈指令，支持“疑似栈耗尽”
判断；只有顶部9帧，没有足够证据认定完整原因。已询问面板/操作/capture，
当前不能宣称原崩溃已复现或修复。

同一路径发现独立真实问题：RDHeaderView的dataChanged functor未绑定this寿命，
切换model未断开，旧model仍触发当前view尺寸测量；tree expanded/collapsed及
scroll回调同样没有context。添加this context，切换model断开dataChanged。
Qt独立CPU对照测试：修复前旧model更新触发2次当前header测量，exit2；修复后
旧model不触发、当前model仍触发、header销毁后model/展开/滚动更新无崩溃，exit0。
保留before/current日志和manifest；未以此替代用户原场景或GUI视觉验收。

入口python3 util/test/qt/qt_header_model_lifetime.py；编译实际RDHeaderView.cpp，
仅将未测试的GUIInvoke::defer队列依赖用Qt寿命安全singleShot接入。Qt5.15.19，
当前manifest build-macos-debug/qt-header-lifetime-current/manifest.json PASS；
修复前对照build-macos-debug/qt-header-lifetime/manifest.json PASS（预期before失败）。

## 集中关口

B481–483三族后固定32333bb6集中回归PASS，产物
build-macos-debug/frozen-validation-32333bb6/full-regression.log与manifest.json。
十份B482功能capture在本库API回放复验全部通过；全量308正例、7784坏输入、
3080生命周期打开通过，resident growth0bytes、exit0，frozen与工作库起止哈希均一致。
ARC提前释放仍未修，公开两项RT能力false；不提交/推送。

第一次query构建因macOS14间接类型缺可用性守卫FAIL（build.log），修复后second.log
PASS。Qt独立手工首构建缺Qt flat header include path，second build及正式qmake入口
PASS；全部失败日志保留。
