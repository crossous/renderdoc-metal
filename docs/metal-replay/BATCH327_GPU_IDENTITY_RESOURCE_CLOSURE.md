# BATCH327：间接资源捕获遗漏修复，UE 自动重截与重定位原生验证

2026-09-30，本地 M2 Pro / 16 GiB，HEAD 仍为 c4be68bb7。
全部改动保留在工作树；未提交、未推送。用户已完成 BATCH326 要求的重截，
本批没有再要求用户点按钮。

## 用户帧与实际遗漏

用户会话 `20260930-193008`，库 c7cb40b786ff…，捕获 end=1，UE 正常退出。
`UE58_capture.rdc` 20,020,428 bytes，SHA256
`1f6b40106fb22680d81a6a446ef9fe8bd7851a23483a4cc41c7464de2ce35b23`。
已另存 `UE58_NewMap_identity_1f6b4010.rdc`，原件哈希核对相同。

CPU XML 有 8659 chunks、180 draw、81 pass，heap 声明容量 6 GiB。
身份 chunk 899（buffer 313、texture 586、sampler 0）。UE 已知的资源表24
帧首 800 个非零 VA，仅210个有唯一范围候选；2013个texture ID仅300个有
唯一候选。sampler表25有125条非零项，却没有任何sampler创建或身份记录。
表24/25确实在vertex、fragment、compute绑定到UE ABI的0/1槽位。

最小原生复现只有一个线程、1×1纹理、4-byte输入、24-byte手写argument
packet与64 KiB Shared heap：通过useHeap声明input/texture驻留，sampler
只经packet间接使用，没有显式setTexture/setSamplerState/useResource。
原生与注入都算出105，**旧库文件中三类资源及全部身份metadata被遗漏**。
未对该错误旧文件执行GPU replay。这证明是捕获依赖过滤问题，不是猜测的
完整UE watchdog唯一原因。

## 修复与横向对照

对照D3D12 `WrappedID3D12Resource::RefBuffers` 对已查询GPU地址的live
buffers保守标记，以及Vulkan `CreateAddressRange`/address tracker的资源+offset
关系。Metal没有CPU可见的任意raw descriptor引用列表，sampler也不属于heap。

- `MetalResourceManager::RefGPUIdentityResources` 在capture开始时于资源管理器
  锁内遍历live map，标记所有应用查询过原生GPU身份的对象为Read。保留创建、
  身份chunk、父依赖与已有初始状态机制，不持有额外native对象或历史ID集合。
  已销毁对象不在live map；可能额外捕获未实际使用的live资源，这是保守代价。
- 活跃capture中的缓存getter也标记frame引用；原子去重只去掉重复metadata，
  不能去掉资源依赖。1397格式、旧编号、section version与GPU前拒绝不变。
- 新CPU工具 `util/ue/audit_ue_metal_gpu_identities.py` 要求显式指定已确认的
  resource/sampler表ID，按24-byte UE IRDescriptorTableEntry布局核对候选。
  重建已捕获CPU增量写入，报告offset、缺项、歧义与sampler相同描述符别名。
  不扫描任意64-bit字段，不改capture，不模拟GPU写入，不把候选当安全重定位。

## 助手自动重截后的证据

小夹具及旧六帧通过后，使用插件已有的45秒自动受控viewport捕获入口：

```bash
UE_METAL_AUTO_CAPTURE_DELAY_SECONDS=45 bash "/Users/crossous/Documents/Unreal Projects/Testproj/Saved/RenderDocMetalLocalM2.command"
```

会话 `20260930-195104`，仍为NewMap、15FPS、50%、Nanite off，2744×1690。
end=1，文件保存后仅向本次启动的editor PID发送SIGTERM，编辑器清理完成、
launcher退出0；当前没有遗留UE进程。没有修改Engine、项目Config或Content。

新文件12,154,879 bytes，SHA256
`1e4a2f80f87d7846077a0a652b43f87db0c9fca960797bf50d1f528646e78e27`，另存
`UE58_NewMap_retained_1e4a2f80.rdc`，本地产物 `retained.rdc`。不同会话的帧内容
和启动资源不同，文件大小变化不能作为修复性能结论。

CPU XML：10909 chunks、191 draw、96 pass、24 MRT pass、49 commit、7 heaps
声明3.5 GiB；没有显式waitUntilCompleted或purgeable Empty chunk。
1763身份chunk（buffer626、texture1069、sampler68），对应626/910/41种值。
有68份sampler创建记录；109个非零sampler表条目全部有候选，其中33条唯一，
76条对应相同创建描述符的多个对象。12组重复sampler原生ID的描述符全部相等，
不把这类别名误报为不同状态，也不随意选择重播对象。

资源表24帧首684非零VA有484唯一候选，1411个texture ID有977唯一候选；
重建两次CPU快照后596个VA有564候选，1496个texture ID有1373候选。
仍缺32个VA和123个texture ID（帧首缺项更多）。UE `FreeDescriptor`只交还
allocator，未清表槽；这些缺项可能包含旧槽，当前数据不能证明每一项无shader
访问，也不能把帧尾CPU状态用于替代帧首/帧内状态。

UE `FlushPendingDescriptorUpdates` 使用临时entries/indices buffer和临时heap，
通过 `UpdateDescriptorHandle.usf` 把三字段descriptor写到标准heap，再插入
compute memory barrier。这要求同时处理显式表schema、临时payload范围、
资源生命周期、buffer offset/typed view，以及GPU写入/复制与CPU快照的执行点。
单纯重写标准表帧首或删除guard都不构成正确实现。

## 验证结果

- **定向终端：通过。** renderdoc/renderdoccmd/viewer以-j4构建，最终库与
  app内嵌库都为 `6c0e5d6f8cfe08aa3be98cb92e01f43ad568ca02f29d99777945bfe477326059`。
  CLI hash仍为ffcebc3677ce…；Qt SDK26警告是已有构建限制。
- 同源105用例：显式绑定pre-frame、显式绑定active-query、仅间接/useHeap
  三变体各两次捕获；创建+metadata+packet原值、输入41初始字节与纹理
  (64,128,192,255)上传均一致。间接变体额外创建并
  查询后销毁一个buffer，其metadata未泄漏到capture。每份原文件与把metadata
  移到帧尾的变体，API/CLI都在捕获资源分配、initial GPU upload与frame
  submission前明确拒绝。这是**预期拒绝**，不是replay成功。
- 用户1f6b…帧与自动1e4a…帧的API退出4、CLI退出1，具体错误为descriptor
  relocation unsupported；无replay wait/private initial upload trace。没有
  提交完整UE帧GPU工作，也没有再次触发已知挂死路径。
- `test_metal_replay_targeted_macos.sh t01 t09 t35 t49 t52 t62`，最终库六帧
  API与CLI全部通过；日志 `retention-targeted.log` / `metal-targeted.R8ey2n`。
- 新 `metal_gpu_identity_relocation_probe.mm` 在两个独立原生进程间传递32-byte
  packet。输入buffer的+4 offset、texture、sampler由MTLArgumentEncoder重编码，
  8-byte普通常量/哨兵原样保留；旧VA90194411524变为90195492868，两进程均
  得到122且Metal Validation无错误。只提交重编码后的packet。**这是原生算法
  验证，不是RenderDoc或UE descriptor replay已经实现。**
- 两份UE CPU审计、Python AST与diff check通过；本轮未发现新panic报告。
- **全量回归：未运行。人工qrenderdoc UI：未运行。UE正常打开与replay：
  尚未达到。** 旧人工待验全部保留，成功UI QA增量0。

日志与CPU结果集中于 `build-macos-debug/local-m2-ue-identities`。
本批修复了可独立复现的capture资源遗漏；已有新帧足够继续开发，不要求用户
再次重截。下一步必须把明确schema与资源+offset关联接入真正的replay路径，
处理GPU descriptor更新及生命周期后，再逐级扩大用例和UE负载。
