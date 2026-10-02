# BATCH361：Private heap buffer 的显式地址来源与物理共享

2026-10-01，持续目标 active，未提交或推送。

新 UE 0649fac8…中238次 frame buffer→frame buffer复用、99次背景buffer→frame
buffer复用、3次背景Private buffer→frame texture复用。实际地址来源审计完整，
但 v22 的Shared来源及此前CB必须commit限制无法支持这些Private资源。

对照本地RenderDoc D3D12 d3d12_device_rescreate_wrap.cpp::Serialise_CreateResource
和Vulkan vk_resource_funcs.cpp::Serialise_vkBindBufferMemory：继续用原heap/offset，
在对应ResourceId上读取新的native GPU地址。Private只影响CPU映射，不能因此
把已知GPU地址判为无效。沿用Metal既有16MiB staging初始buffer上传，并在seek
前等此前GPU完成。帧内Private buffer在实际出生时重建，不能预先填假对象。

v23定向通过：仅增加<=64KiB、Private tracked placement buffer来源/共享，
背景Private来源必须有完整初始内容；descriptor backing继续Shared且CPU可写。
heapstorage必须与buffer options匹配，提前拒绝非法Native组合。旧v22仍拒绝
Private sourced frame buffer。显式makeAliasable仍保留原完成/退役条件。

小用例保持A descriptor不变：帧内Private A由GPU初始化41，背景Private A
通过捕获初始内容恢复41；GPU再写同heap/offset的B[1]=80，读A得到225/161。
retained/unretained、背景/帧内、两次捕获和事件回跳；仅末尾一次CPU wait。
新增CPU证据校验初始内容或GPU初始化seed绑定，以及B没有CPU写入。
精确库9905558b275503aa0669f9b54ee877dcdb2e810a5476464c4491c319c130696c。
metal-async-alias.ONkKm6八捕获/32seek/216 API+CLI反例全部通过；Private
初始与frameGPU初始化路径、GPU数值122→225及186→161、实际画面一致。
Shared兼容的Z4jEY3八捕获/32seek/200反例通过；新增CPU证据断言修正了
两处过严假设：背景Shared A可通过完整initial bytes而不是重复提交快照
提供41，Shared B的保守提交快照只要仍为41就是正确，GPU producer写80。
没有给B增加应用CPU写入。系统Bash3.2空数组nounset问题已修正。
此前精确863f5998…全量308/7786/3080通过，增长13942784B；本次精确库
全量尚未运行。人工UI仍待解锁、完整UE尚未GPU replay；继续未提交CB与
跨buffer/texture的tracked heap共享。
