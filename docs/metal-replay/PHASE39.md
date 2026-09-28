# PHASE39：T38 sampler LOD override

状态：终端自动完成，GUI L4 待验，阶段开放。

32×8 RGBA8 texture 的三层 mip 分别是红/绿/蓝，同一个 sampler 静态范围0..2。
VS/FS/CS 都使用 slot2；显式 clamp 1..1 或2..2 选 mip，batch range2+2 包含 slot3=nil，
再切回普通绑定恢复0..2。single nil 与不同 stage 同一 sampler 的独立 override 都有覆盖。

计算输出48 bytes（3个float4）：green、blue、red。渲染三个横向区域：黄、黑、蓝。
API 验证有效 descriptor min/max、资源身份、移除空槽和前后seek；native验证GPU输出。
新增compute chunks只追加ID；render沿用已有LOD chunk ID。

必跑旧路径：T16/T17 vertex/fragment binding，T30/T31 compute samplers/batch，T32/T33
dispatch；因事件快照与 compute 共享逻辑变化，本批扩大到40份已存capture回归。
测试与限制见 `BATCH39-40.md`，人工验收并入 `QA_CONSOLIDATED.md`。
