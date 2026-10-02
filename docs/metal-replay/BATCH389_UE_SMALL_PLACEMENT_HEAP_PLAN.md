# B389：本地 M2 的隔离 UE placement heap 配置（重截与CPU审计通过）

当前UE763817d8的Mac allocator默认每block512MiB，八个placement heap接近4GiB，
小viewport不能改变这个编译常量。为持续本地验证准备隔离diagnostic block64MiB；
遵循原UE FMetalResourceHeap allocator的NewBlockSize=max(resourceSize,defaultBlockSize)，
超过64MiB的单资源仍得到完整空间，不缩资源、不中途搬移offset、不是重放时伪造heap大小。

prepare_ue_metal_provider.py新增显式--placement-heap-size-mib可选参数，默认保持原引擎常量。
只精确替换UE5.8.3 Mac那一行；iOS路径不变，所有输出拒绝installed-engine子目录。
隔离heap64 source与已通过显式三槽版本逐文件比较，仅Private/MetalBuffer.cpp不同。
provider元数据、UE draw constants/inline/source/lifetime hooks保留，无coverage声明。

source: build-macos-debug/local-ue-module-probe/provider-source-heap64
build: build-macos-debug/local-ue-module-probe/provider-build-heap64
module SHA256 0d648a90952eda8f8e1d619e89c62b9597c86959dc76b8db288ec43b1dd20792。
98/98 exports匹配，missing/added均空，三cached clang unity顺序编译，不调用UBT、不安装模块。
原Engine MetalRHI SHA256仍ace97360be37784ff3ec909537aaf105d5266eb3a1b24ca5d64fac72304928d0。

全量GPU回归运行时只做了CPU构建。重截需要等待GPU串行队列空闲，保留之前RDC；
后续CPU审计新heap的实际数量/大小/布局/alias和来源，不把更小默认值当内存压力验证成功。
当前实际UE完整replay与人工UI未验收；UI锁屏，持续目标active，无提交/推送。

新UE捕获12230757c9a41febcc1e10e6f3fed3bf8dc564570720c0cf0fd76a1736f51677，101939588B，session20261002-050736，owned editor19914退出0。旧763817d8保留，新文件UE58_heap64_12230757.rdc保存。仍无complete coverage声明，未GPU replay整帧。
38heaps总2625896448B（36个64MiB/75.76MB/128MiB见workload-audit精确列表；不能把预设64当全部heap大小），相较4GiB降低约39%；原引擎不变。Native1748placements/range问题0，127历史overlaps=15BG buffer→frame buffer+112frame buffer→frame buffer，无本帧cross-kind重叠；这不是GPU完成证明。
frame-start1937 live全部initial字节匹配；当前frame lifetime/value/inline/producer问题0，62producer精确匹配。5054历史value epoch缺失退休源身份保留限制。所有typed table未知非零GPU字段0；主表1813live/531known-freed nonzero/784088unknown zero。490texture initial/609985688B、packed layout问题0；913有效source=550textureInitial+363bufferInitial，19源明确首CB前释放。
81direct compute/1237422threads/max262144；120buffer copies/2901178B/max786432；212direct draws（195indexed+17nonindexed）、31indirect compute/15indirect draw/2indexed-indirect draw仍待适配。CPU报告前缀ue-heap64；人工UI锁屏。

补充：typed table backing overlap数0。旧Metal stringify将indexed indirect也写成drawIndexedPrimitives（没有后缀）；workload audit按实际indirectBuffer字段分类，旧统计214里有2个indexed indirect，现更正212direct/48indirect commands。
