# B512：IR帧内多几何BLAS

接续B511/PHASE59，能力仍false，构建与GPU串行，不提交或推送。

## 对照与实现

DX12 d3d12_command_list4_wrap.cpp:990–1127与Vulkan wrappers/vk_cmd_funcs.cpp:8688–8770捕获typed几何数组并在build事件重放。本批复用已有Metal kind8多索引几何验证/descriptor与encoder-end Private整buffer快照，新增append buildFrozenMultiIndexed（1435，Max1436）；保留原chunk及其他Shared/heap/refit路线。typed资源、2..64条十字段descriptor、原offset/stride/Float3或Float4、UInt16或UInt32、IFT offset/flags、scratchOffset、整VBO/IBO快照在native完成后补写。只在完整Private/Tracked/standalone/无alias与usageNone入口使用新路。current配方保持帧前初态独立、EID0恢复。预检拒绝opcode/kind错配和越界，有限值只检查实际索引引用的xyz，整buffer中未引用padding不当成顶点。回放独立staging持有到native完成，不捕获wrapper。

六个新mode覆盖2与64 geometries、existing与new BLAS、heap-only、global+heap+local六sampler。前n-1个Float4/stride32/UInt16/offset16及index4三角形远离射线，最后Float3/stride16/UInt32/offset160及index24平移100或-100，IFT槽0与1。closesthit写GeometryIndex*17，TraceRay geometry multiplier0保证单hit记录；Private 256/64bytes上传后建AS并擦零。未引用padding放NaN验证精确边界。实际TraceRay前后输出、事件usage/往返/EID0、逆序合法控制须通过。

## 最终固定库验收

backend/bundle SHA256 `ad0a0248f5a1f60df425d81e188aaec44a37814a56053f2596e8b071ec514bf2`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。构建与GPU串行，起止hash一致。证据目录 `build-macos-debug/metal-ray-b512/final/`，capture目录 `captures/metal-ray-b512/final/<mode>/ir-runtime_capture.rdc`；manifest、validated-modes、remaining-manifest、source-binary-hashes、shutdown-log-audit和CPU-final-checks记录实际范围。wrapper集成78场景及反例，本批逐组件运行，未把wrapper整体调用计作PASS。

| 范围 | 实际结果 |
| --- | --- |
| 78主IR native/capture/API/CLI三循环 | PASS，8268事件选择及EID0；新六multi场景1446次 |
| 多几何真实TraceRay | PASS，2/64 geometries、existing/new目标、heap/local/global组合；before146/11，after11/164或11/1218，GeometryIndex分别1/63，其他组合实际输出见validated-modes；Private256/64bytes建AS后清零 |
| 损坏IR/合法控制 | PASS，总2088拒绝/135控制；新multi 579/30，含opcode/kind错配、数组范围、mixed格式/索引、IFT越界、最后descriptor、引用NaN、未知typed资源、scratch、缺build；合法逆序GeometryIndex0/原坐标/UserID75/child重编号/同command buffer |
| typed冻结及事件 | PASS，原geometry顺序、两种格式/stride/offset/index/IFT槽、未引用padding NaN保存；新BLAS只在after、旧只在before调度被读，EID0恢复初态 |
| 初始列表 | PASS，1坏上限/1往返；外层旧错误分类未变 |
| 旧targeted/argument/packet/IR | PASS，18旧targeted、41旧argument、6旧packet、B501/B502 IR，未注解IR继续拒绝 |
| 官方MIT Apple sample/能力 | PASS，两scene/270事件、10查询、13坏capture、6能力入口；原生支持单独判断，默认生产RT false |
| 正常关闭/重开 | PASS，4captures×10，包含global+heap+local多几何，resident growth 1785856bytes |
| 失败关闭/CPU导出/seek-reset | PASS，30失败+10成功、10CPU导出/30seek-reset，包含新multi referenced NaN反例，growth 638976bytes |
| 日志/CPU/hash | PASS，6504最终日志无断言/overrun/未知resource/device关闭诊断；gate互斥、Python/Bash/diff、起止binary hash |
| 实际UE/完整75RT/完整308/原Qt/ARC早释/独立旧table | NOT RUN或NOT VALIDATED，不继承旧库结果 |

官方缓存ZIP SHA256 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`，来源/许可/版本与构建由official gate保存。新fixture使用本机UE5.8.3 DXC→Apple IR转换，明确typed注解，不能当成自动UE producer接入。两IFT槽使用同一typed native intersector，字段与边界已验证，未声称不同函数语义覆盖。系统报告库存比较仅filename/stat证据，未证明旧冻结根因。

## 保留的中间失败与修复

首轮转换的SM6.3不支持GeometryIndex，`first-multi`完整保留；仅multi variant选择SM6.6，其余旧variant不变。`shader-model-fixed` native/capture/API/CLI与`first-multi-invalid`96坏/5合法通过，均先验，不计最终固定库通过。64几何首轮固定e58e库主API/native通过，但后续反例export暴露补写chunk固定budget没有包含640-word参数数组，实际5600bytes超过声明1344；全部中间final产物移至before-multi-chunk-size-fix，不能算通过。已把参数bytes计入budget，并将sample gate的每一步backend日志单独保留且拒绝断言/overrun，防止只看exit0漏报。POSIX日志在最后引用退出时会删除，因此另加父进程共享锁保留每一步文件；缺少逐步导出/API/CLI日志的场景在同ad0a库补跑，中间产物在before-step-log-retention-fix保留。size-fixed-64及其97坏/5合法控制为修后先验，最终全部78场景及相关组件在新库重新运行。

## 尚未完成

UE实际光追与可靠producer布局关联、IR refit/copy、异buffer多几何和更广资源布局仍缺；完整75RT/308、Qt原崩溃、任意ARC早释、独立旧table本批未运行。系统强制重启报告只能证明WindowServer watchdog/forceReset，未证明测试或GPU具体根因。RT黑盒边界保持，无AS内部查看/shader单步/RT Pixel History承诺。

用户最新要求：B512验收后先回本机UE工程，以有限命令行/隔离配置开启Lumen HWRT并截帧，核验真实调度和支持水平，再按捕获暴露缺口修复。B512收尾时B513入口已准备但尚未启动；后续真实UE截帧、实际Lumen inline查询证据和流读取修复见[B513](BATCH513_UE_LUMEN_QUERY_AND_STREAM_SCAN.md)，与B512构建/GPU验收未重叠。
