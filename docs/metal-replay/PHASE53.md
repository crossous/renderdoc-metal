# PHASE53：ICB GPU 操作与单命令 reset

T53 / Metal_ICB_Operations。接通 blit reset/copy/optimize 三个旧 chunk（1224–1226），
以及 indirect render command reset bridge。仅 CPU 初始化的 Shared render ICB；不扩展
GPU shader 生成命令、compute/Private ICB 或任意跨队列调度。终端自动验证通过；
GUI L4 待验，阶段开放。

## 验证清单

- Native 验证层 → capture/XML → API/CLI，验证非零范围复制、重置不影响邻居、优化后
  输出、空命令、复制资源依赖、重复回放/后退。新增错误资源/类型/范围/descriptor/encoder
  负例。必须恢复 replay 开始时的 ICB 内容，不能残留上一轮 GPU copy/reset 结果。
- 必跑旧 T20/T22–T27 ICB、T13/T14/T21 draw、T44–T46 同步、T49/T52 epoch 回退；
  共享 replay/资源路径修改时，批末统一 T01–T53 + T10 marker API/CLI/lifecycle 与全部
  已接入异常脚本。旧 T24 的空命令变体若成为合法输入，移入正例而非删除验证。
- UI 留至集中 QA：事件参数/ICB 身份、空命令与有效 draw 的区分、复制后的 pipeline/
  buffers、像素及事件回退；不要求 ICB 原生二进制解码面板。

## 实现

- 三项 blit 调用真实编码 native reset/copy/optimize；记录源/目标/范围/encoder，保留
  引用，并更新 replay 的命令元数据。Copy 支持非零范围、不同 ICB 和同 ICB 不重叠区间；
  要求命令类型、继承标志和绑定容量一致。错误资源/范围/类型/结束后的 encoder 先拒绝。
- 每个 replay epoch 第一次使用 ICB 时保存/恢复 CPU 初始命令，原地 reset/re-encode，
  保留既有 command wrapper 指向；OnlyDraw 延续 WithoutDraw 的 epoch。优化记录也按
  epoch 清除，同提交的重叠优化范围在 native 之前拒绝；不是忽略 hint 的空回放。
- 单命令 reset 追加 chunk1270，清除该 slot 的 pipeline/buffers/draw shadow，支持
  初始化时清空或重新编码。现有 CPU ICB 编辑的初始化记录模型未改，不声称支持帧内
  CPU 重编码与 GPU 提交交错。两个 slot 绑定均可随已编码命令复制，包括非 indexed draw。
- 空命令显示 `ICB[n] empty command`，保留 execute 子项和 EID，不创建虚假的 draw/
  shader usage；依然调用原生 execute。T22/T24 两个旧“空命令负例”转为三轮正例，未删覆盖。
- 增加 capture epoch 和 GPU 改写标记。跨捕获边界的 GPU 初始内容无法从 CPU 记录恢复
  时，原生调用继续，离线通过新 chunk1271 `unavailableInitialContents` 明确拒绝；完整
  CPU reset/re-encode 可重新建立可重建状态。Max1272；该拒绝标记不是新增功能入口。
  仅帧前 GPU 内容不可用时不保证能靠捕获内局部/完整 GPU reset 绕过，保持保守拒绝。
- Replay 创建阶段拒绝非 Shared ICB，避免恢复初值时向 Private ICB 做 CPU 编码。
  旧 inheritance 负例暴露的缺失 render pipeline 原生崩溃已加资源/类型/encoder 检查修复。

## Fixture 与验收范围

源 ICB 五项、目标六项；源包含普通/indexed 命令、清空的黄命令和 reset 后重新编码的蓝
命令。每帧四次 execute：初始全屏洋红 → 非零范围复制后的红/绿/蓝三带 → 中段 GPU
reset 后红/黑/蓝 → 同 ICB disjoint copy 后同样红/黑/蓝。两次 GPU reset、两次 copy、
三次不重叠 optimize，14 个 execute 子项中 9 draw / 5 empty。帧前全 CPU reset 建立
固定初始状态；独立变体加入帧前 GPU copy，验证原生仍正确、离线明确拒绝。

每个 draw 的 PSO、buffer offset16/0、indexed 类型与 offset4/size12、80-byte 参数包
及 padding、旧黄命令无 draw usage、四阶段及 GPU API EID 的往返均自动核对。Native
验证层逐阶段检查三带像素，不只检查最终图。194 类新异常 + 三种合法变体；旧 ICB
脚本统一追加 30 秒超时/禁止信号退出。批末精确结果、版本与一键入口见 BATCH53。

后续 UI 只验事件/资源链接、空项无 Draw 标记、复制状态和四阶段画面，不重复手算数据；
已并入 QA_CONSOLIDATED。已验旧 T20–T27 结论保留，本批新行为由 T53 集中覆盖。
