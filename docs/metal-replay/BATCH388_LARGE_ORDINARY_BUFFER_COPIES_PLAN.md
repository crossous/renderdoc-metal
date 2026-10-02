# B388：实际 UE 普通 buffer 复制（定向与容量端点通过）

UE763817d8有129次buffer→buffer blit，总2955720B，最大786432B，均不触碰已声明typed table backing。
另有两次buffer→texture blit，不把这两条混入buffer复制统计或放宽其原规则。
旧普通复制最多16条/64KiB总量/64KiB单条。v47候选单条1MiB、256条、16MiB累计，
保持显式ResourceId/offset/size、源/目的不同、overflow-free范围、frame birth先后、
别名生存期和Nativeusage检查，typed descriptor copy仍24B/source association。
TrackDescriptorGPUCopy普通buffer分支和preflight使用统一限额，不改capture二进制版本。

夹具复用B381：Shared/Private frame placement128KiB、Standalone Shared4MiB；
先完整128KiB/1MiB upload，再127次尾部12B copy、typed reader54/80，最后尾部覆盖90/120，
共129条。Shared目的先填0xaa的复制区域，完整读回应为0xee，避免原初始化内容掩盖漏复制；
Private目的完整GPU上传，API检查每个byte。两个capture独立NativeVA，四轮partial/full/EID0。
候选script util/buildscripts/scripts/test_metal_descriptor_large_plain_copy_macos.sh。
反例新增旧v46/zero-size/单条上限/UINT64_MAX/257count/16MiB累计等。

当前运行库仍466b749c v46，其精确全量308/7786/3080已通过，growth8880128B。
v47源尚未构建，不把旧库结果归给候选。UE heap64自动重截先运行，待退出后串行测试候选。
完整UE GPU replay、MRT/pass与人工UI尚未验收；UI锁屏。目标active，不提交/推送。

精确库c5caf39075aaa056b46e1c558001144c8eb675f33cd860f099d10aad94a9c609，metal-large-plain-copy.2vSSSB：六捕获/24 seek cycles、Private及Shared全字节、GPU54/80/覆盖/EID0均通过；27/29/27共83 API+CLI negatives通过。256条Native容量端点进一步复核中；没有重跑同一全量来代替定向验证。

相同精确c5caf390…库，metal-large-plain-copy.k4e8AR：Native每帧255次upload/echo加1次overwrite=256条，两类heap+standalone六捕获/24 seek cycles/83 negatives全部通过。日志末尾旧固定echo129描述已修正为实际env计数；捕获XML计数为256，不依据文案推断。
