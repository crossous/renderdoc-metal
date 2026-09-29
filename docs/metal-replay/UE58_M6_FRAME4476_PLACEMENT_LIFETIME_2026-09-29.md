# UE58 frame4476：placement heap 复用的首个回放阻塞（2026-09-29）

## 输入与单次终端结果

- 工作树：`/Users/kurogames/Documents/Unreal Projects/renderdoc-metal-t312`；
  HEAD `e0a26f7e65e22a3890aa38a318bde51d24babf6b`，含本地未提交的
  M4–M6 改动。原 `renderdoc-metal` 目录未修改。
- 截帧 session：`SocoTestProj/Saved/RenderDocMetalSessions/20260929-165339`。
  `UE58_frame4476.rdc` 149,219,170 字节，SHA256
  `ab0e5a2918af5568a0257eb6f316140f68954f26d6c09e0b4f17606434b5bcbb`。
  注入库 SHA256 `19d491f029980e7b40f2f5f88c1fedcf833b8ad46fb78c60f3684bdc1843ed8f`。
  UE 5.8.3 / macOS 26.6 / Apple M4 Max；编辑器运行后由用户正常关闭。
- `renderdoccmd thumb --out=/tmp/rdm-ue4476-thumb.png <capture>` 退出 0；
  缩略图及 UE 日志确认 `Lvl_FirstPerson` 编辑器视口，非 Empty 关卡。
- 使用当前库编译 `/tmp/rdm-ue4476-open.mm` API 探针，SHA256
  `6556542dfb81355ffa66a8da6506a27eff9ed1900871d93356d90d2617de2224`。
  运行一次 `MTL_DEBUG_LAYER=1 RENDERDOC_METAL_TRACE_REPLAY_WAITS=1
  /tmp/rdm-ue4476-open <capture>`，75 秒上限、4 GiB RSS 上限；约 2 秒
  退出码 4。日志 `/tmp/rdm-ue4476-api-once.log`：
  `Metal placement buffer overlap: offset=11457536 end=11524096
  existing=11459584..11526144`，随后 `OpenCapture failed`。
  没有进入帧内 GPU command buffer 回放，也没有新 Metal Validation 错误。
- `renderdoccmd convert -f <capture> -o /tmp/rdm-ue4476.xml -c xml`
  退出 0；仅用于静态审计，未再调用 GPU。CLI `replay`、全量回归、
  人工 UI 均未运行；累计 UI QA 增量 **0**。

## 对照与资源身份

静态 XML 的 `MTLHeap::newBuffer(offset)` chunk 显示：

| chunk | heap | buffer | offset | length | 区间 |
| --- | --- | --- | ---: | ---: | --- |
| 1914 | 6047 | 1373003 | 11,459,584 | 66,560 | [11,459,584, 11,526,144) |
| 2179 | 6047 | 1374505 | 11,457,536 | 66,560 | [11,457,536, 11,524,096) |

heap `6047` 是 512 MiB、Private、placement 类型；`1373003` 还出现在
帧前 `Internal::Initial Contents` chunk 2328。静态检查 694 个 placement
buffer 创建记录，仅发现 2 对按逻辑长度重叠的 buffer，均位于 heap `6047`。
另一对是 `1373074` 与 `1374537`。这份 XML 没有可用于判断旧对象释放
时间的 chunk，也没有这两对对象的 `makeAliasable` chunk。

UE 5.8.3 `MetalBuffer.cpp` 的 `FMetalResourceHeap::CreateBuffer` 使用
`Block.Heap->newBuffer(Size, Options, Block.Offset)`；`ReleaseBuffer` 调用
`FreeBlock`，将块归还 free list 并释放 MTLBuffer。现有 Metal replay 的
`WrappedMTLHeap::Serialise_newBufferWithOffset` 只累积 `m_PlacementRanges`，
从不移除已释放对象的区间，因此把捕获期间的空间复用当作同时存活的重叠，
在 native `newBuffer` 前安全拒绝。D3D12 的 placed resource 创建与资源
生命周期由资源包装/释放处理；不能把它的具体释放规则直接用于 Metal。
这与 [BATCH131–132](BATCH131-132.md) 留下的 T133 边界一致：原生 Metal
Validation 及注入已证明同 offset alias/reuse 正例，回放明确拒绝；当时还
发现创建顺序与事件 seek 的堆状态重建问题。UE 此帧的复用路径使用释放和
free list，不是可见的 `makeAliasable` chunk，因此 T133 也不是充分覆盖。

## 下一步与边界

需要建立可验证的完整 placement 资源生命周期：记录捕获时释放/重用的
身份和时间，回放时在正确执行点解除旧资源占用，兼顾帧前初始内容、
GPU 在途引用、纹理/缓冲交叉复用和错误输入。先用原生 Metal Validation
正例确认释放后重用，再做注入截帧、API/CLI 回放、数据/像素及必要负例，
最后跑受影响的旧 T116/T131/T132 和新 UE 截帧定向回归。
现有新帧缺释放时间证据，不能仅凭两个 offset 删除重叠守卫或猜测释放点。
即使此阻塞解决，默认 bindless 的 GPU 地址重定位仍待验证；不能宣称
这张帧已能打开或回放正确。
