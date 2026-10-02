# B381：较大帧内buffer来源（定向验证通过，持续适配UE）

新实际UE bfa3f117…CPU导出：7 frame placement buffers超过64KiB，5×66560B、2×131072B，
全部Private tracked options544；另在36444/36445有两个standalone Shared4MiB buffers，
旧只统计placement的计数不可当成全部frame buffer内存。都超过当前v39 frame64KiB上限。

v40候选沿用D3D12 placed buffer的heap+offset和现有Metal frame ResourceId/Native重建，
区分placement最大128KiB、standalone Shared最大8MiB；typed表backing的64KiB限制暂不放宽。
Nativeheap尺寸/对齐/range/overlap和tracked lifetime、Private GPU初始化、普通blit16条/64KiB
总量边界保持。源地址必须匹配captured base+memberOffset，回放按每次Native VA重编码。
CPU buffer修改预检补future/live/Shared storage以及start/size范围，避免CPU越界延迟到GPU编码后。

新增test_metal_descriptor_large_frame_buffer_macos.sh与原生/回放/gate夹具已静态编译：
Private/Shared placement128KiB、standalone Shared4MiB；read末尾有效8B GPU54/80，
Private仅12B局部upload、读后12Boverwrite；Shared检查全部非尾部0xee字节。
两个捕获使用Native query独立heap范围；typed frame slot、背景scalar13、普通root metadata
共同验证，EID0释放frame source并检查table归零。反例涵盖容量/options/Native range、
VA/member/identity、birth次序、copy范围、inline声明及CPU范围。
尚未执行这组Native/GPU/反例；等待当前全量结束，保持GPUserial。

当前全量script启动时会自动构建：它包含v39和最早v40容量候选，精确库
958d133db85805ef5de88b96ab6aec4ff1ce50402cd6367779b90167c55a5572。
不能记成7b4e0848库的精确全量；之后CPU范围候选只编辑源文件，未替换运行中的958d库。
全量是已知极小T01–T312语料，不包含未验证的大buffer来源或实际UE整帧提交。
B380库7b4e0848旧21类674 API+CLI兼容本轮已通过；新CPU范围需要后续定向验证。
持续目标active，人工UI锁屏未验，实际UE无完整coverage/尚未GPU replay，未提交或推送。

958d133d…全量已完成，308/7786/3080通过，resident growth12632064B。之后才启动
含新CPU范围检查的大buffer定向构建；目前Native/API套件运行中，等待结果。

最终定向库150e4d323ec1343139960cf7f7f98ce7355a40ae87da58e6877010e54d9860e6：
metal-large-frame-buffer.D5BVSL，6 captures / 24 repeated seeks / 67 API+CLI negative groups
（Private22、heap Shared24、standalone Shared21）全部通过；Native/GPU54/80、
Shared全字节、Private初始化尾部、overwrite、EID0归零通过。
首轮1fe350…正确Native但前导retirement scan误把背景staging上传按future→future copy解释；
v40对背景端copy结束retirement-prefix证明，交回普通copy预检继续验证，不跳过range/alias。
958d组合全量308/7786/3080属于更早v40容量候选；150e4最终库尚无精确全量。

UE临时堆Init注解按Native buffer的rounded1024B而非逻辑72B计数，导致3个有效槽
声明成42个，并读到27个旧普通数据非零槽。修改provider准备脚本以Engine自身
HeapSize/sizeof(IRDescriptorTableEntry)作为Count；新隔离模块编译、重新截帧验证进行中。
