# BATCH39–40：动态 sampler LOD、private buffer 读回

2026-09-26。实现与终端自动验证通过；GUI L4 延后，PHASE39/40 和本批保持开放。
本轮没有使用 Computer Use，没有启动 qrenderdoc，没有提交代码。此前修改全部保留。

## 本批实际接通

- T38：VS/FS/CS 各自的 single/batch sampler LOD clamp，共六个入口。捕获包含 sampler、
  null/bound、slot/range、min/max；native replay 应用真实参数。按事件保存 LOD override，
  通用 sampler descriptor 返回有效范围，普通 sampler rebind 清掉 override；不修改共享的
  immutable sampler 对象。覆盖 nil、batch 空槽、覆盖与事件前后跳转。
- 两个 compute LOD 入口此前甚至不在 bridge/chunk 标记清单中，而是落入 ObjC forwarding。
  本批新增显式 bridge 和尾部 chunk IDs 1250/1251；不改变原有 ID，旧 capture 继续可读。
- T39：`GetBufferData` 支持 private buffer，通过临时 shared staging + blit + GPU completion
  同步读回，支持非对齐 byte ranges、len=0、超长截断；先校验资源类型，避免把 texture
  当 buffer 解引用。Managed 同步路径也补齐，但本机 Apple Silicon 未专项验证 Managed。
- 新样例暴露此前 compute 校验绑定于纹理滤镜样例：direct dispatch 必须有成对同尺寸 2D
  纹理。现增加 output-buffer 分支，支持 T38 的 texture→buffer 和 T39 的 buffer-only kernel。
  使用 Metal reflection 检查 active buffer 的最小大小/对齐、必要 texture/sampler 的存在。
  旧 texture-output 与 indirect 的约束保留，不声称已泛化全部 kernel/dispatch。

## 终端测试 batch

入口：`bash util/buildscripts/scripts/test_metal_capture_batch39_40_macos.sh`。

1. 构建 app/renderdoccmd/demos（不启动 GUI）；T38/T39 各 5 帧 native + 8 帧注入 capture。
2. XML 精确核对六种 LOD 调用、bound/null、range/min/max；T39 两个 516-byte buffers 的
   Private/Shared storage flags；新 capture 各 3-loop CLI。
3. Replay API：T38 三次 CS 输出 green/blue/red，VS/FS 三次 draw 输出黄/黑/蓝；descriptor
   LOD 逐事件、同一 sampler object、nil slot、plain rebind、回退/前进均断言。
4. T39：516-byte private buffer 的 fill `a5` → kernel `(i*7+3)&255`，完整与
   `offset=3,len=9`、`255,17`、`515,0`、`512,UINT64_MAX` 读回逐字节断言；两次往返 seek，
   越界/空资源/texture-as-buffer 返回空；最终 framebuffer RGBA `3,3,24,255`。
5. 新异常脚本最终 **72 cases**：slot/range、null mismatch、短数组、NaN/Inf、负/反向 LOD，
   encoder/buffer/texture 缺失、active sampler=nil、buffer 大小/对齐及 grid 异常。信号崩溃、
   超时均算失败，而非“成功拒绝”。
6. 共享最终回归入口复用 `test_metal_replay_batch35_38_macos.sh`，设置
   `RENDERDOC_METAL_LAST_TEST=39`：T01–T39 + T10_debug 共 **40 captures** 的 API/CLI；
   原 71、新 72、旧 compute 32，共 **175 类异常检查**；40 × 10 lifecycle。
   默认不设变量仍只跑先前 38 captures，避免旧 batch 命令含义漂移。

旧 compute 异常回归包括 T28/T29 10、T30 2、T31 9、T32 5、T33 6。没有重录旧 fixtures，
也没有重做全部历史 native 或 GUI。第一次整批（加最后三项异常前）40×10 resident growth
589,824 bytes；最终增强staging探测的40×10为 **557,056 bytes**（门限64MiB），
库与app内嵌库同为 `35150e09a506…`。脚本语法、Python编译和 `git diff --check` 通过。

## 覆盖与后续

- bridge/chunk 从 186/128 降到 **182/124**；本会话基线 216/165，累计减少34/41。
  六个 LOD 入口只有四个原有标记，因此计数不能当成全部功能覆盖率。
- sampler LOD 暂覆盖单采样 2D 三 mip、三 stage、非零 slot/range；sampler arrays、其他纹理
  类型、GPU family 差异需独立扩展，不从本测试推导。
- compute 新分支仍依赖当前受支持的 reflection 和资源模型；不能静态证明任意 shader
  内的数据相关索引安全。旧 2D/depth=1 等限制没有在本次整体移除。
- Private buffer 读回是同步 staging，先保证数据正确；批量异步读回与性能优化留后续。
- 为集中 QA 已建立 `QA_CONSOLIDATED.md`：共七份待验 capture 在同一个 app 中检查，
  公共操作只做一次，各功能只核对差异。当前所有 GUI 均未执行，用户要求时再指导。
