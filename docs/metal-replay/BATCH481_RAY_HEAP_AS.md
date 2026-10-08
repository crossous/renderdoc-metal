# B481：UE heap 加速结构创建与生命周期

接续 PHASE58。最终 backend/bundle SHA256：
9122ac7a269a2a1321bb39d6ea4875044f632b18816806dfdf629b2e595b61f1

## 实际问题与实现

B480 首轮 UE 拒绝位于 AS encoder 的对象类型检查，不是 usage/geometry 检查。
追加实际类型日志证明结构为原生 AGX AS，scratch 已包装。UE MetalBuffer.cpp:2143
通过 heap->newAccelerationStructure(size, offset) 创建 AS；此前 heap bridge 原生转发
使对象逃逸。对照 Vulkan 的 AS backing allocation 和 DX12 placed resource/AS 地址
依赖方式，新增 MTLHeap::newAccelerationStructure chunk（旧编号不变），自动及
placement 两种创建、资源身份与 heap 父依赖、帧内创建重放与 EID0 释放/重建。
AS heap getter 返回包装 heap，heapOffset 返回原生偏移。

重放按原生 heapAccelerationStructureSizeAndAlign 查询实际物理占用；核验设备、
Private storage、heap type、身份、对齐和实际 native heap 容量；placement 重叠拒绝。
未支持 descriptor 分配变体、AS aliasable 退役或 sparse；没有提供 AS 内部查看。

## 结果

- 六例 heap-placement/heap-auto/heap-frame-placement/heap-frame-auto/
  background-heap-placement/background-heap-auto：native/capture ray0/1/0/1，
  API全部846事件选择含往返/EID0，CLI各3loops PASS。
- 六例78坏捕获 clean拒绝；18旧定向API/CLI PASS。
- 七捕获各10生命周期 PASS，resident growth278528bytes。
- Apple 官方两scene新截帧/原生/离线像素逐字节一致，44/46事件各3方向及
  CLI各3loops PASS；41坏输入拒绝，6项能力查询 PASS。
- 实际UE再次运行：heap AS 包装修复生效，进入下一个两几何段构建边界；
  usage=0/motionKeyframes=1，两个段各28三角形、stride12，UInt16 index offsets
  0/168，共享Private输入。进程退出1；没有可验收RT捕获，RT dispatch/回放未验。
- 本库全量、GUI和设备提前释放ARC问题未验/未修；两项公开能力仍false。

build-macos-debug/metal-ray-b481/heap-manifest.json、regression-manifest.json、
sample-manifest.json；captures/metal-ray-b481 六捕获。官方当前 gate-results
为9122ac7a，e0e4df24已归档gate-results-b480-e0e4df24。UE结果
build-macos-debug/ue-ray-diagnostic/b481-9122ac7a/manifest.json，session20261005-160948。

## 保留的失败

第一次构建漏更新 Max 静态断言，build-heap-as.log FAIL，second.log通过。
坏输入脚本先错误假定AS逻辑size与heap物理占用线性，后又假定请求heap大小等于
native大小；驱动允许这些合法变化，不能记后端失败。改用原生query约束和
保证超出分配域的对齐偏移；offset-assumption-failure日志/manifest保留。
官方坏输入调度首次用了错误捕获文件名，File not found，path-failure保留；
修正文件路径后41份通过，不重复已通过sample oracle。未提交/推送。
