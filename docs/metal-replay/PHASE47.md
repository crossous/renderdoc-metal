# PHASE47：同步 Pipeline 创建变体

T47 / `Metal_Pipeline_Variants`。实现和最终完整联合验证通过（48 captures、513类异常、
480次lifecycle），详情BATCH47；GUI L4按用户要求延后，阶段保持开放。

## 接通范围

- Render `newRenderPipelineStateWithDescriptor:options:reflection:error:`，原chunk1022。
- Compute `newComputePipelineStateWithFunction:options:reflection:error:`，原chunk1024。
- Compute `newComputePipelineStateWithDescriptor:options:reflection:error:`，原chunk1025。

移除三个bridge标记、接通三个旧chunk，未增加/重排chunk编号，Max仍1264。
native创建复制调用方descriptor并解包shader/linked functions，保留原descriptor；返回真实
autoreleased reflection/error，失败时返回NULL，不建立空wrapper。资源记录保存shader父依赖。
Replay重建pipeline与反射、shader绑定和compute线程组限制，保留各次创建的不同资源ID。

支持options 0/ArgumentInfo(1)/BufferTypeInfo(2)/两者(3)。Replay总是请求ArgumentInfo，
不要求用户原调用请求reflection。Compute descriptor覆盖label、compute function、buffer
mutability、maxTotalThreadsPerThreadgroup、threadGroupSizeIsMultipleOfThreadExecutionWidth。
整倍数编译承诺在dispatch验证，包括dispatchThreads的非整齐尾组；旧function入口默认无承诺。

## T47 与自动断言

六个pipeline：四compute（function/descriptor各options3和0），两个render（options3和0）。
native检查返回reflection及Params结构体成员，descriptor function身份不被修改；descriptor
maxThreads=64、整倍数=true、slot0 Mutable/slot1 Immutable。前三dispatch为threadgroups，
最后dispatchThreads，均32线程。实际使用全部六个pipeline，不只做创建API空跑。

532-byte output按uint32：四段各32项，分别17+i、41+i、73+i、109+i；尾部20 bytes保持0。
四dispatch分别填一段，前后seek核对全部buffer与padding。两个draw分别画左右半屏，最终
RGBA17/41/73/255；第一draw右半黑。API验证pipeline身份、CS/VS/FS reflection和资源usage。

异常输入覆盖空/错误类型/未知/重复ID、shader stage、options位、descriptor支持标记、数组
上限、mutability、线程数/栈深/高级状态、linked functions、采样数，以及违反整倍数/尾组承诺。
信号退出不算拒绝；每个子命令30秒超时。具体数量和最终全套结果见BATCH47。

## 边界与待验

仅同步创建。异步completion-handler入口未接通；不支持binary archive/cache-miss policy、
dynamic/preloaded library、linked/binary functions的replay。Compute stage-in、ICB kernel、
非默认call-stack depth也明确拒绝；不将支持普通descriptor等同任意高级pipeline/真实UE兼容。
没有新增pipeline参数专用UI面板；创建参数在资源初始化API/structured data中查看。
GUI最小差异检查并入QA_CONSOLIDATED，保留所有旧T待验，不要求逐元素重复手算buffer。
