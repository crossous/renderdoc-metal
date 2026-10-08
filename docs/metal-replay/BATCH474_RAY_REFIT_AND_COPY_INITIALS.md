# B474：refittable、refit 和普通 copy 的 AS 初态

2026-10-05，接续 [B473](BATCH473_RAY_INDEXED_INITIALS.md)。按本仓库 Vulkan
`vk_acceleration_structure.cpp` 的输入记录/重建及 copy 共享不可变快照方法，
以及 DX12 `d3d12_initstate.cpp` 的恢复时清除 PERFORM_UPDATE、重新 build 方法实现。
光追保持黑盒，不增加 AS 内部查看、shader 单步或光追 Pixel History。

在已有 schema3 输入记录中支持 usage Refit，恢复对应的 refittable 原生描述符，
并恢复 kind4/kind7、三角形数量、格式、索引、duplicate 等已有 refit 校验元数据。
完成的原地/异地 refit 使用提交时最新 Shared 输入形成新快照；要求前序可重建
记录完成且描述符兼容。普通 copy 保留不可变源 recipe，复制完成后仍可独立于
源的后续 refit 恢复。Metal `destination=nil` 归一到同一源/目标 ID，沿用现有
序列化 refit 接口，不新增 driver chunk。

逐事件测试发现异地 refit 的首次重放失败：帧内目标创建记录只加载一次，上一
执行残留的已构建状态导致拒绝。每次完整回放先清除全部 AS 的逻辑构建/尺寸
查询状态，再重建存在初态的结构；帧内目标重新作为未构建对象接受 refit。
原地/异地、Float3/Float4、stride20、UInt16/UInt32、索引 offset、duplicate=false、
nil 写法及 copy 后源更新分别验证。

## 本批证据

候选 backend/bundle SHA256：
`37dc58da15b12ebd8065aa4e13127e1d7988b75623a91978d2852f5f06a7d143`。
日志：`build-macos-debug/metal-ray-b474-gate.log` 和 `build-macos-debug/metal-ray-b474/`；
捕获：`captures/metal-ray-b474/`。入口：`util/buildscripts/scripts/test_metal_ray_refit_macos.sh`。

| 检查 | 结果 |
| --- | --- |
| 新增19组 native/capture/API/CLI | PASS；AS 初态与当前顶点数据故意不同，帧内 refit 后再次查询，射线结果为1/0/0或0/1/1 |
| 新例事件导航 | 每帧38事件，正/反/正三方向，每次先EID0；2,166选择 PASS；检查输出哨兵前缀和实际 AS 绑定 |
| 新增损坏捕获 | 非索引初态26、索引初态15、refit身份/描述符/禁用Refit/关闭encoder49，共90 PASS |
| 既有基础 gate | 16新+30旧正例、1,785选择、177负例及3语义边界、schema1/2兼容 PASS |
| 生命周期 | 基础18×10增长622,592 bytes；refit/copy19×10增长360,448 bytes；合计370次 PASS |
| 构建 | renderdoccmd/app PASS，backend/bundle一致 |

合计65份正例、3,951次选择、267损坏捕获、3语义边界。首轮异地 refit 失败日志
留在 `metal-ray-b474-before-reset-gate.log`，不计 PASS。两份新捕获 hash：

- initial-refit：`367a192f342bfa200e6694599e84da76aa7dc81ed9fd41f041cd6dc46504c841`
- initial-refit-indexed-copy：`287e2f533bbf0e22f4746b3b3ac1154041da68619ac5706c5378bd022b388197`

本批 copy 测试为编码后即时提交，再更新源的情形。后续复查发现“编码后先在
另一提交更新源、再提交copy”的窗口，需要在 commit 检查源 recipe 版本；
该硬化及 compact 初态接续在 B475。Private/同CB GPU输入、boxes/motion/多geometry
仍未闭环；设备光追能力仍false。未跑全量308帧/GUI/真实UE光追，未提交/推送。
