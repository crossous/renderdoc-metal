# B497：函数表 GPU 标识关联及有限光追复验

2026-10-06，PHASE58。完成 VFT/IFT `gpuResourceID` 原生转发和有类型的资源关联；新增四种真实 ray query 场景，在最终一致库上复验官方 Apple Metal2 sample。UE 未重启，系统死机根因仍未解决，公开能力保持 false。

## 来源、参照及支持边界

实际 UE `MetalRayTracing.cpp` 的 `IRDispatchRaysArgument` 会查询 VisibleFunctionTable 和 IntersectionFunctionTable 的 gpuResourceID。仓库 DX12 `d3d12_resources.cpp` 的 shader export database 将原生 `GetShaderIdentifier` 结果关联到 state object/export，并记录解包信息；Vulkan `wrappers/vk_get_funcs.cpp:1403` 直接转发原生 `vkGetRayTracingShaderGroupHandlesKHR`。本批按查询结果与 typed 资源关联的方式补齐 getter，承接 B484 AS 身份机制。

新增显式 Objective-C getter，精确返回原生 `MTLResourceID`；只在捕获记录 `MTLFunctionTable::CaptureGPUIdentity(resource, kind, value)`，kind0=VFT、kind1=IFT。查询本身将资源标为 frame reference，即便未绑定也保留创建依赖；重复查询去重。新 chunk 追加到已有枚举尾部，旧 chunk 编号不变，名称计数同步更新。重放验证资源存在、创建顺序、类型、设备、非零值、同资源稳定值及同类型唯一所有权，允许完全相同的重复元数据。

VFT、IFT、AS 使用不同的有类型命名空间。实际 native fixture 出现 AS 与 VFT 数值同为1，不能按任意整数建立全局唯一表。本批没有实现 UE raw IR dispatch packet 的地址/函数表重定位，没有扫描任意 buffer 整数，没有宣称可重放完整 UE RT。GRS/ResDescHeap/SmpDescHeap 等 typed ABI 字段仍须独立证明。RT 保持黑盒：不增加 AS 内部查看、RT shader 单步或 RT Pixel History。

## 实际验证

串行构建 renderdoccmd/qrenderdoc app，最终 exit0；构建日志 `build-macos-debug/metal-ray-b497/build.log`。测试均为有限小场景，Metal API Validation 开启，没有与 UE 或其他 GPU 测试重叠。

| 场景 | Native/capture 输出 | API事件 ×3方向 | CLI | 标识坏例/合法副本 |
| --- | --- | --- | --- | --- |
| table-identity-large-visible-33 | 0/3/0/3，compact1280 | 69×3 PASS | 3 loops PASS | 19/2 PASS |
| background-table-identity-large-visible-33 | 0/3/0/3 | 46×3 PASS | 3 loops PASS | 19/2 PASS |
| table-identity-frame-born-large-visible-33 | 0/3/0/3，compact1280 | 77×3 PASS | 3 loops PASS | 19/2 PASS |
| background-table-identity-unused-large-visible-33 | 0/3/0/3，额外未绑定VFT/IFT仍捕获 | 46×3 PASS | 3 loops PASS | 20/2 PASS |

共714次新增事件选择，每次先 EID0 reset；帧内场景还核验创建前输出资源不可见、创建后初始7/7/7/7及四次 dispatch 后结果，往返不跳过任何事件。每次真实 dispatch 验证 AS 资源绑定，保留17次IFT更新/5次清空及33项VFT末槽调用。getter重复稳定且与 `real.gpuResourceID` 精确相同。共77坏capture拒绝、8合法重复控制，包含零/未知/错误类型/跨表类型资源、invalid kind、零标识、冲突标识、提前于创建及重复所有权；逐项核验后台具体错误，不只接受任意 CLI 失败。

旧定向复验：B484 AS身份35×3、B494同TLAS Private重建50×3、帧内placement alias/GPU擦零51×3，均PASS；函数表sentinel t44/t120/t126/t130/t135/t140/t141/t144/t148由既有targeted脚本展开18份capture，API/CLI PASS。生命周期基础t35与四个新capture共5×10，growth589824bytes，exit0、库起止hash一致。此次不是75份RT全套或集中308份回归。

官方 Apple `AcceleratingRayTracingUsingMetal.zip` 原始source逐文件与zip比对，SHA256仍为 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`，MIT许可，构建方法由 `metal_apple_ray_sample_gate.py --stage replay` 保存。两种Metal2 scene（triangles/procedural）固定seed1/64×64/4frames，原生重复输出、capture输出及65536-byte RGBA32Float离线像素 oracle、44/46事件×3方向/EID0/CLI PASS；10组AS尺寸/heap查询native一致，41坏sample反例与6组原生/default/probe/无效开关能力查询PASS。native compute可用，默认两能力仍false，精确probe仅compute可用、render仍false。

## 编译取证修正

B496的 `thread` 实际为 pthread 指针，不能直接当成系统 sample/spindump 的 numeric thread ID。本批新增 `pthread_threadid_np` 的 `native_thread`，保留旧字段；审计器保持旧trace兼容，保存 begin/end native ID，异步可分别记录调用及callback线程。

四个新capture实际证明 descriptor-sync 各一次，linked_functions=2、函数trace、native success；耗时0.364–0.470ms，begin/end native thread相同、非零，无pending。其余五入口已构建但本批未作真实native验收，不以CPU协议测试替代。10项编译trace审计CPU测试及21项监督/INI CPU测试PASS。

正常退出日志曾消失：RenderDoc POSIX `logfile_close` 无其它shared holder时会unlink日志。监督器和新fixture gate在启动前持有普通共享文件锁，退出/清理/审计后关闭，保存诊断日志。新增CPU控制先证明无holder日志会删除，再证明被监督进程正常退出后日志保留。不是修改native logger语义，也不是用强制logger初始化掩盖问题。

## 保留的失败

- 首次新增chunk忘记同步stringise Maxguard，构建失败，修正1426后通过；原日志 `build-before-chunk-name-count-fix.log`。
- 首次capture输出正确，但正常退出删除trace日志，gate失败。`before-trace-log-bootstrap/`保留；调查中临时logger bootstrap随后撤回，最终源码无bootstrap/全局logger变更。
- CLI仅返回通用chunk失败，原坏例脚本未看到具体错误；父进程保留每个变体的debug log后验证后台拒绝原因，原失败 `before-diagnostic-log-retention-fix/`。
- conflict反例原用XOR1，在值1时得到0，触发正确的零值拒绝而非预期conflict；改为非零old+1，原失败 `before-nonzero-conflict-oracle-fix/`。
- 帧内输出尚未创建时旧API oracle要求16bytes，原FAIL EID1。依据原CPU创建chunk fileOffset核验absence/birth/initial/dispatch，补每次EID0 absence，原失败 `before-frame-born-output-oracle-fix/`。
- 日志调查中的LLDB启动停滞，未获得native调用栈；仅清理本次拥有的lldb/target/debugserver，保留trace-debug/lldb-sample日志，不计成功证据。没有改用户LLDB配置。

## 产物及最终哈希

`captures/metal-ray-b497/`：四份新capture。
`build-macos-debug/metal-ray-b497/`：gate.log/table-identity-manifest.json、各native/capture/replay/CLI/invalid日志、4份原生compile日志与audit、native-thread-manifest.json、lifecycle.log、CPU测试、preflight、supplemental-manifest.json及binary-source-hashes.json。官方独立产物位于 `apple-sample/gate-results/`；B496 acd0c0d6库保存在 `baseline/lib/`。

最终backend/bundle SHA256：`eea2539fb069dae9d59d2c32ad962c562ee83aecb09d57be195e50538f123c57`。
GUI SHA256：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。
所有验收对应同一backend/bundle，最终再次核验一致。Python/shell语法、UE preflight、git diff --check PASS。

**未运行/未解决**：实际UE重截/RT dispatch/输出/离线重放、75份完整RT、新库集中308份回归、GUI原用户Qt crash和ARC父对象生命周期验收。系统死机根因未定位；旧WindowServer/MetalCompiler日志不能指定具体失败函数。下一推进UE IR typed packet支持及有限六入口编译取证，禁止直接重跑旧长UE/全量followup，先具备对应停止和诊断证据。公开能力false，任务仍未达启用终点，未提交/推送。

B498接续：其余五种创建入口连同descriptor-sync已全部用六个实际query管线验证native/capture/replay，32×3事件/EID0/23坏创建及生命周期通过；参见[B498](BATCH498_SIX_NATIVE_RAY_COMPILE_APIS.md)。库未变，不等于UE真实编译停滞已修复。
