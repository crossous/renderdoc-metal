# Metal 黑盒回放验收与稳定性门槛

2026-09-28 方向校准：不再以清零 bridge 宏、遍历全部光追描述符组合
作为近期目标。参照本仓库 DX12/Vulkan 对 AS build 与绑定的回放思路，
Metal 光追的最低验收是**必要资源和状态被安全捕获、离线重建、GPU结果
可观察且一致**；事件树能定位 build/bind/dispatch。暂不承诺 AS 内部
节点、交点过程、shader 单步或像素历史的调试。新案例只在增加语义、
风险或真实应用阻塞证据时加入。

## 已验证能力

- 受控 Metal 样例的基础图形、compute、资源、同步及若干 AS build/refit、
  copy/compact、TLAS 实例、渲染/计算阶段绑定，已有原生、截帧和终端
  回放证据；最新TLAS复用两个BLAS见[BATCH299–300](BATCH299-300.md)。
- 逐事件回退通过 GPU readback 验证，而非仅检查 chunk 存在。
- 09-28最终库`ac97012d33b5…`：21份代表截帧API/CLI一次回放、
  T299/T300各20例畸形输入、T35+T299+T300共3×10生命周期通过。
  小阶梯复跑T300 CLI×5、T299 CLI×10、T35/T294/T298/T299/T300
  共5×10生命周期，也未出现新panic；这些都不是全量压力回归。

## 明确拒绝范围

- 无法完整重建的调用必须在捕获或回放时明确拒绝，不可静默原生透传
  后声称支持。当前不同BLAS子资源数组上限4、实例数上限65536；
  未支持的descriptor类型、非Shared实例描述符、CPU/GPU内容无法
  重建的路径仍拒绝。受控样例之外的任意Metal帧不在已验证范围。
- 本机 counter sampling 对应能力不可用，不能用该机完成相关旧chunk
  的GPU闭环；M4专属/硬件加速路径必须到目标机验证。

## 未解决的架构问题

- T61 GPU执行点的 ICB range 不能从编码时CPU值可靠推断，见
  [PLAN](PLAN.md)原门槛；没有GPU时序证据前不得把它当作已接通。
- GPU写入的实例描述符、间接参数或其他执行期资源内容，与编码时
  Shared buffer快照可能不同。现有TLAS路径要求可验证的CPU侧
  Shared描述符，不代表一般GPU生成TLAS实例已支持。
- 第三方应用注入、预编译shader可见性、复杂heap/alias、跨队列同步及
  真实UE资源链须由首个实际失败点决定优先级，不能以受控样例数量推断。

## 06:26 panic 与渐进复测

唯一查到的panic为`2026-09-28 06:26:38`，断言在
`IOGPUResource::free`：资源仍由`IOGPUDevice`持有。快照含
`renderdoccmd`进程，但panicked thread属于`kernel_task`/
`IOGPUFamily`。测试可能是诱因；仅凭该日志无法定位RenderDoc资源
生命周期错误还是系统驱动缺陷。后续短测至BATCH311–312收口未复现；
这不能排除长时负载问题。

恢复顺序：先对固定少量捕获做构建/单次API与CLI、畸形输入、10次
生命周期；再选2–5份异族代表分别做5/10次CLI重放并检查新panic；
再扩大到一个小批次。任一步出现GPU异常、系统重启或资源增长异常，
停在该阶段，记录具体capture、循环数、日志和panic时间，不升级负载。
未经稳定阶段证据，不自动运行`LAST_TEST=300`累计全量回归。
295份正常截帧的分组复测、各至少10次生命周期打开，以及3453例
分组畸形输入结果见[稳定性记录](STABILITY_2026-09-28.md)；尚未覆盖
全量累计负例或单进程长时压力。

## 集中UI QA及真实应用的进入门槛

- 受控截帧集中UI QA：库与app内嵌库hash相同；代表性API/CLI和
  畸形输入通过；短阶梯无新panic；[QA_CONSOLIDATED](QA_CONSOLIDATED.md)
  统一列出capture与差异项。满足这些后可先按代表帧分组启动人工QA，
  不必先打开全部274份，也不要求UE首帧已成功。当前Qt 5.15.19构建对
  macOS 26 SDK给出兼容性警告，app只编译未运行，须在此轮UI QA验证。
  首轮建议同一进程打开T01/T09/T35/T53/T144/T300六份：基础画面、
  资源/compute、空帧、ICB、双BLAS及实例复用各一份。只有该组稳定后
  再按差异项扩展，不要求一次人工遍历全部历史capture。
- M4：先记录机器型号、RAM、macOS/SDK/GPU family，再跑同一小组
  native/capture/replay对照。当前M2 Pro成功只证明API路径，不证明M4
  专属行为或性能。
- UE普通帧：先固定现有UE 5.6.1最小非Nanite场景与注入方式；只有
  截到可打开的一帧、能定位第一个不支持调用、关键GPU输出与原生对照
  一致，才称普通帧里程碑达成。Nanite/VSM、光追内部调试另列目标。

09-28只读核查：本机为M2 Pro/16GB/macOS 26.1；
`/Volumes/CauseUseMac/UE_5.6`仍有UE 5.6.1的arm64
`UnrealEditor.app`。主app可执行文件为ad-hoc签名，签名flags仅`adhoc`，
未见hardened runtime位；这降低了已知注入门槛，但不证明
`DYLD_INSERT_LIBRARIES`在实际编辑器进程和其子进程中有效。
未启动编辑器、未改签名、未生成UE捕获；M4机器未连接。
