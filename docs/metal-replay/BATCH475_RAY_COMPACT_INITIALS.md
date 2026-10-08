# B475：compact 初态、尺寸查询恢复与提交前版本校验

2026-10-05，接续 [B474](BATCH474_RAY_REFIT_AND_COPY_INITIALS.md)。先对照本仓库
Vulkan `vk_acceleration_structure.cpp` 的 copy 共享输入与重新 build，再对照
DX12 `d3d12_initstate.cpp` 的全尺寸重建、compact copy。没有复制 opaque Metal
AS 字节，也没有添加光追内部 viewer、单步或 Pixel History。

## 改动与发现

- compact copy 保留源的不可变输入及 compact 标志。普通 copy 再复制 compact
  结构时保留该标志；源后续重建不会改写已复制的 recipe。
- copy/refit 编码时锁定源版本，commit 前验证仍为同一 recipe。另一个提交先更新
  源再提交 copy 的合法原生样例执行正确，但当前无法证明执行点快照，离线明确
  拒绝；不能把编码时的旧输入当作实际复制的新结构。版本比较使用本次候选处理
  前的快照，避免原地 refit 的占位失效记录干扰验证。
- schema4 保存 compact 标志及可选的已完成尺寸查询 source/offset/type。读取保留
  schema1/2/3；旧 buffer/texture 字段及 driver chunk ID 不变。
- 恢复 compact 初态时先用冻结输入构建全尺寸临时 AS、写入临时 Shared 尺寸，
  等待原生提交完成，验证本机真实 compact size 不大于捕获分配，再执行 compact
  copy。所有临时原生资源在完成后释放，不把 full build 塞进较小的 compact 分配。
- 首轮帧内 compact 失败：AS 初态此前没有恢复尺寸查询的 producer 身份。现在
  保存符合既有“查询和构建来自同一已完成提交”规则的记录，恢复后重新查询本机
  尺寸，并将 producer 映射到初态提交标识。查询使用临时输出，不改写捕获尺寸
  缓冲的帧首字节；实际 compact copy 再检查本机容量，避免设备差异导致越界。
- 静态索引 AS 的 triangle-count 元数据保持既有 kind2=0 约定；refittable 索引
  kind7 恢复真实数量，以同时满足原有 compact 与 refit 校验。

首轮草稿候选 `dfd6e115...` 未通过帧内 compact，日志保留在
`build-macos-debug/metal-ray-b475-before-query-gate.log`，不计本批最终 PASS。

## 最终候选与验收

backend/bundle SHA256：
`b5451e7946e66b8e3536985ac3abe129abfcfe6c78cc2fd43ac3b0d9056efa6a`。
入口 `util/buildscripts/scripts/test_metal_ray_compact_macos.sh` 串行运行嵌套的 basics、
refit/copy gate，再验 compact、旧 AS 兼容及生命周期；每步有超时。
总日志 `build-macos-debug/metal-ray-b475-gate.log`，分日志 `build-macos-debug/metal-ray-b475/`，
捕获 `captures/metal-ray-b475/`。此 hash 重新验证此前正例，历史 PASS 不自动继承。

| 检查 | 结果 |
| --- | --- |
| 12组 compact native/capture/API/CLI | 非索引/索引/Float4 BLAS、TLAS、refittable索引、compact后普通copy、源重建、帧内compact、TLAS引用compact子结构与重复子引用：PASS |
| 12组事件导航 | 1,242次选择，正/反/正，每次先EID0；射线0/1/0/1、当前顶点/索引字节、AS绑定和输出哨兵前缀 PASS |
| 基础与refit/copy复验 | 16+19组新正例分别1,791和2,166次选择；30份原定向旧帧 PASS |
| 扩展旧捕获兼容 | 额外141份旧AS/refit/函数表相关捕获 API/CLI PASS，显式排除上述30份，非全量308帧 |
| 损坏捕获 | 基础177 + refit/copy90 + compact容量/丢失标志6 + 初态尺寸查询身份/参数8 = 281 PASS |
| 语义边界 | 基础3项 + 编码后另提交更新源再copy1项；原生正确、离线明确拒绝，无signal/hang |
| 格式兼容 | schema1/2/3非compact初态各3 CLI轮 PASS；原B474两份schema3 refit/indexed-copy历史文件各3轮另行PASS，见legacy-schema3日志 |
| 生命周期 | 基础18×10增长376,832 bytes；refit19×10增长393,216 bytes；compact12×10增长229,376 bytes；共490次 PASS |
| 构建与静态检查 | renderdoccmd/app PASS；backend/bundle一致；shell/Python语法、diff whitespace PASS |

总计47份新正例+171份旧正例=218份；新例共5,199次逐事件选择。历史B474 schema3
两份文件另做兼容复验，不混入上面的218份定向数量。两份最终 compact 捕获 hash：

- background-compact：`ca40cdc9c851bd92cefd8967468112e602e971c72c8aa95e2612eab9f248cbbe`
- background-tlas-indexed-repeated-child-compact：`51d9f93c797d44562ac2465ff6b9fd993045d3fec0300afe68a6c41468aa8a6a`

Shared 输入下受控三角形 AS 的 build/refit/copy/compact 初态与反复离线回放基本
链路已闭环。Private、同CB GPU输入、boxes/multi-geometry/motion初态、复杂函数表
提交快照及真实工程光追仍有缺口，两项设备能力继续false。未跑完整308帧/累计负例/
3080生命周期、GUI或UE真光追验收。未提交/推送。阶段与后续门槛见 [PHASE56](PHASE56.md)。
