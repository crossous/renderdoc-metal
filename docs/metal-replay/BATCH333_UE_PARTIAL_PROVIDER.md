# BATCH333：官方局部源码、独立MetalRHI编译与槽位诊断provider

2026-10-01，用户要求持续适配→截帧→再适配，直到UE真实replay完成；目标仍active。
保留工作树，未提交/推送。全量回归未运行，完整UE replay未通过。

## 源码与独立构建

GitHub账号可读取Epic官方私有仓库。局部取`5.8.3-release`，解析到
`396c9f059903aed5fec78ecd3d437a40c6415368`，本地保存于
`build-macos-debug/ue-official-reference/5.8.3`，manifest记录路径/hash。
MetalBindlessDescriptors、MetalStateCache、RHIDescriptorAllocator及RulesCompiler等
六份源码与安装版逐字节一致；Build.version的源码tag CL0与安装CL58210709分别记录。
官方源码只保存在ignored本地目录，不纳入此公共仓库。

`util/ue/build_ue_metal_module_macos.py`读取已存在的三份compile和一份link response，
不运行UBT；显式把unity include、MetalRHI Private/Public include及所有产物重定向
到独立source/output。移除缺失的shared PCH，顺序编译三个unity，链接仍依赖原版
Engine DLL。安装版98个导出，baseline/provider也都是98个，缺失/新增均0；
MetalRHI类/头文件布局未变，跟踪状态放于本provider的文件级map。

当前baseline SHA `baa825ff0058d7bf7af25ceece9eaf091f0a3d49c0cfa2334263e8d37d617f1a`，
provider SHA `79a0a4921c0b9e5fc9e93c9c870ac03e58d58ac118fb4ada8158fc7f71da63f7`。
原安装MetalRHI保持`ace97360be37784ff3ec909537aaf105d5266eb3a1b24ca5d64fac72304928d0`。
`prepare_ue_metal_provider.py`仅允许新建隔离副本，复制自有provider/API头并加执行点
hook，不改写安装Engine源码或库。无需整套UE源码编译，也无需用户另提供源码版。

启动通过`UE_METAL_RHI_OVERRIDE`选择经build-manifest校验的独立DLL，进程级
DYLD_LIBRARY_PATH依次查provider、安装Engine/Binaries/Mac、MetalShaderConverter。
首次正常Metal启动缺第三方rpath而失败；CPU dlopen明确指出libmetalirconverter，
已补依赖搜索路径。NullRHI `-run=DumpMaterialExpressions -help`加载独立模块、
插件成功，exit0、0error/0warning；这只是CPU启动，不是GPU/replay验收。
后续正常Metal启动已看到NewMap及Capture按钮；首次自动截帧因计时失误丢弃，
正在重截。`PresentDeadline`现在在StartFrameCapture返回后设置5秒，避免元数据
准备时间提前耗尽两秒预算；项目插件已独立编译并保留原源码/DLL备份。

## 描述符来源与通用API参照

参考D3D12 `CopyDescriptors[Simple]` 的明确shadow copy/资源引用，以及Vulkan
`vk_descriptor_funcs` 在capture/idle都维护DescriptorSetSlot的行为。UE FreeDescriptor
只在deferred callback把slot交回allocator，旧bytes仍在；GetAllocatedRange的包围
区间不能证明全部槽位有效。为此hook实际Allocate、deferred Free（交回pool前，
避免另一线程新Allocate先于free记录）、Immediate CPU write，以及GPU update
dispatch后的expected destination值。事件和GPU值都为诊断事实，不跳过GPU等待。

新增chunk1403 `MTLBuffer::DescriptorSlotEvent`（Max1404）：buffer、byte offset、
generation、event、descriptor type、完整24-byte payload。event0 allocate/1 free/
2 CPUwrite/3 GPUexpected；event3 generation0表示根据ordered槽位记录审计，
不能独立作为generation覆盖证明。背景记录保留在真实buffer record，frame记录
加入有序frame record并引用buffer。绝不写入/清空原表或替换实际UE GPU dispatch。
任何带这些记录的普通replay当前在metadata gate拒绝，CPU XML export可用。
这不是live-slot replay支持，也不是完整provider coverage；临时slice、render inline、
alias及GPU写入的完整契约仍待实现，禁止给UE添加coverage声明。

最小48-byte表/2x2 clear：四项非法annotation拒绝，两代slot六条记录的offset/type/
generation/CPU与GPUexpected payload逐字节通过，scope前后顺序正确；API+CLI
在分配/提交GPU前拒绝。六个typed tiny变体及56组负例也通过，当前日志
`build-macos-debug/metal-descriptors.h6ILpf`，库SHA
`1ee8ba5ecd9177992fe9c905809963fb644dc7a1e813ecd7ecb23197d90883a6`。
这批没有新增Viewer人工验收；UE启动观察不等同于UE replay人工验收。

## 构建探测事故与完整恢复

早期误以为UBT `-WriteOutdatedActions`是只读；实际BuildMode先执行
DeleteOutdatedProducedItems，再导出actions。它删除了123个原安装DLL
（852,581,488bytes）；没有执行C++编译或UE GPU replay。这是本agent对选项行为的
判断失误，已向用户说明。unsafe helper已从源码移除，原脚本/记录仅留ignored目录。

依据原Epic安装manifest从官方CDN恢复全部123个DLL，每个验证manifest SHA1和
恢复记录SHA256，独立再校验123/123一致；完整原件和chunks均保留。
`build-macos-debug/local-ue-module-probe/recovery/restoration.json`及recovery.log为证据。
安装标记、MetalRHI.Build.cs原内容/只读模式、四个原Rules DLL/manifest均恢复一致。
generated Intermediate曾受UBT影响，未声称整个Engine每个文件从未变动。
恢复后的NullRHI启动先以不存在的Help commandlet exit1（加载过程无缺库）；
之后选已核实的DumpMaterialExpressions -help，成功exit0。后续一律直接隔离编译，
不得再对安装Engine运行该UBT导出动作。

## 复现命令

```sh
python3 util/ue/prepare_ue_metal_provider.py \
  --engine '/Users/Shared/Epic Games/UE_5.8/Engine' \
  --source build-macos-debug/ue-partial-module/provider-source-new
python3 util/ue/build_ue_metal_module_macos.py \
  --engine '/Users/Shared/Epic Games/UE_5.8/Engine' \
  --source build-macos-debug/ue-partial-module/provider-source-new \
  --output build-macos-debug/ue-partial-module/provider-build-new
bash util/buildscripts/scripts/test_metal_descriptor_relocation_macos.sh
```

构建脚本不安装Engine库。当前实际UE诊断session为
`Testproj/Saved/RenderDocMetalSessions/20261001-032329`，只截帧/CPU审计，暂不整帧GPU回放。

## 首次实际slot帧审计

session 20261001-032329，end=1、正常退出。新帧15,260,576bytes，SHA256
`7845c9a4ca29a1d67fdf7c84167fdad4abf01125135744190be9bf39fb688506`；
保护副本`Saved/RenderDocMetalCaptures/UE58_NewMap_slots_7845c9a4.rdc`。CPU导出
21784chunks，9514slot事件，3个layout，8个slot buffer；lifetime audit零冲突。
frame-start活跃1462，frame-end活跃2022；所有已记录非零字段有identity候选，
但部分raw VA/drawable ID有多个候选。此候选统计包含未来身份，不证明执行期
唯一性；76个sampler多候选还未在本audit中比较等价state。完整UE的API仍exit4，
GPU前拒绝。下一步按D3D12 GetRefIDs记录factory已知source对象/offset与异步队列
的逐entry关系，不以raw候选猜测source。
