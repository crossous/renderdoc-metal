# Agent 交接规范

## 2026-09-24 BATCH29-30 关闭交接

用户确认 T28/T29 右侧缩略图和 T29 `$action()` 筛选正常；此前其余 L4
及 T28 DDS 已确认，T22/T25 事件树改动的最短复验也已完成。T28/T29
自动与用户 L4 均通过，BATCH29-30 已关闭；`QA_PENDING.md` 当前无待验项。
本次提交是向 `crossous/renderdoc-metal` 备份，并非发布，用户明确要求
不运行 T00–T29 L3。最终定向证据与 app/capture 标识见 `STATUS.md` 顶部。
下一批为 `BATCH31-32.md`：从 `PHASE31.md` P31.1 开始 T30 compute
sampler，再推进 `PHASE32.md` T31 批量绑定。T30/T31 尚未实现。

## 2026-09-24 历史交接补充

用户最新确认 T25 的 indexed instanced 多实例文字、ICB 子 draw 收在 execute 下，
点 execute 到最后子 draw；并确认 `setBuffer` 等状态 API 有 EID，T27/T28 可见结果。
因此 T25 新行为复验已通过。余项缩为 T28 10×7 UI DDS、T29 `$action()`
筛选、T22 指定父行画面。`BATCH31-32.md`、`PHASE31.md`、`PHASE32.md` 已写
下一批 T30/T31 计划与定向验证清单，尚未开始代码；当前 BATCH29-30 仍未关闭。

本批 T28/T29 自动通过，用户确认 T29 Buffer 20 变化与 API Inspector 绑定可见。
最新事件树修复使非 action 调用有 EID，ICB execute 可展开并在父行回放整个范围；
范围显示 location/length，indexed instanced 子 draw 显示实例数。正式 T28/T29
captures 已重生成。当前 GUI 和 EID 见 `STATUS.md` 顶部及 `QA_BATCH29-30.md`
顶部。待人工 QA 共四项：T28 的 10×7 Texture 20 DDS、T29 的 EID 10/11 与
`$action()`、T22 ICB 父子/父行输出、T25 ICB 父子/父行输出/实例数。
旧已验 T22/T25 仅新显示行为待复验；T28/T29 与 BATCH29-30 未关闭。
全部未提交改动保留。L3 条件未触发。

## 默认工作周期

一个 agent 默认连续负责一个已定边界的相邻阶段批次；当前计划批次为 `BATCH31-32.md`：从
`PHASE31.md` P31.1 推进至 `PHASE32.md` P32.4。批次以 2–3 个相关 T 为宜；改变边界前先更新
批次计划与验证清单，不因单条切片的自动验证通过就停下。只有以下情况可以在批次关闭前停下：

1. 需要用户选择会显著改变范围、兼容策略或用户可见行为。
2. 外部条件不可用，且安全的替代检查已穷尽。
3. 上下文已明显膨胀，需要先写恢复检查点再 compact。
4. 最终联合自动验证已完成，按 [QA_GUIDE.md](QA_GUIDE.md) 交付一次性 GUI 验收单，
   正等待用户完成 L4 并反馈。此时批次保持未关闭，待验功能持续登记在
   [QA_PENDING.md](QA_PENDING.md)；可继续后续功能开发，但不能把未验批次写成关闭。

每条切片按以下顺序推进：

1. **Fixture/native**：建立最小、确定性输入，先证明未注入运行正确。
2. **Capture/data**：补齐 capture/chunk/initial contents，并用 XML 或结构化检查锁定参数。
3. **Replay/state**：完成 GPU replay、event seek、readback 与 pipeline/descriptor 状态。
4. **自动验证**：立即跑当前 fixture 与实际受影响旧路径的必要 L0/L1/L2，证明数据与功能闭环；
   将 UI 操作记为批末待验，不把未验 UI 标为通过。标准 Viewer、跳转和 export 仍在实现范围内。
5. **批次收口**：最终构建上执行去重后的联合自动清单与 CLI/lifecycle，准备正式
   captures 和 [QA_GUIDE.md](QA_GUIDE.md) 规定的一次性 GUI 验收单。用户在同一轮
   qrenderdoc 中依次验收批内 captures；收到结果后完成文档同步。全量回归按下述触发条件执行。

## 新 agent 的最短接手路径

1. 阅读 `README.md` 入口、`STATUS.md` 顶部当前批次与最近恢复检查点、当前 BATCH 文档
   （目前为 `BATCH29-30.md`）、其中两份 PHASE 文档，以及本文件、`QA_GUIDE.md` 和
   `QA_PENDING.md` 的接手/验证规则与待验项；`PLAN.md` 和历史阶段文档按需追查。
2. 执行 `git status --short --branch`，把工作区视为可能包含前任未提交的有效修改，不得清理、覆盖或
   回退未知改动。
3. 读取 `STATUS.md` 的“当前任务”“下一步”和“恢复检查点”；从第一项未完成工作继续，不重做已有
   明确验证证据的步骤。
4. 用增量构建或当前 fixture 的最短定向测试确认环境可用；新 agent 接手时默认**不**先跑全部历史
   capture 的完整回归。
5. 连续完成当前批次。开始时只需在 `STATUS.md` 留一条简短状态；切片转接时记录自动验证结果、
   UI 待验与下一项。每个微小修复不要求逐次改文档，架构决策、影响范围和中断检查点除外。

## 分层验证与额度控制

验证按风险从低到高执行，避免每个修改都重复读取长日志：

- L0：增量编译受影响 target、`git diff --check`、必要的静态检查。
- L1：每条切片立即运行 native/capture/XML/replay/readback/state 的必要断言；批末在最终代码上
  核对两条 T 的 L1、CLI replay 与本场景 lifecycle。已通过且未受后续修改影响的结果可复用。
- L2：每条切片按清单运行实际受影响旧 fixture；批末取各清单的并集，相同 T 只跑一次。
- L3：只在较大里程碑、发布/合并前，或有具体跨场景风险且 L1/L2 无法充分覆盖时，运行一次
  当前全部 T00-Txx 的一键回归、lifecycle 和 CLI replay。当前两阶段批次本身不构成 L3 触发条件。
- L4：批末由用户用最新成功构建，在同一轮 qrenderdoc 中依次验收批内 T 的 Event、
  Viewer、Pipeline、资源跳转/保存/export 和 `No problems detected`。agent 先完成命令行
  可判定项并交付准确步骤与预期，按 [QA_GUIDE.md](QA_GUIDE.md) 收集反馈；L3 不是前置条件。
  反馈不符时先分析、修复并给最短复验步骤；仅在沟通仍无法确认或用户明确要求时，
  agent 才使用 Computer Use 定向诊断。
  未收到用户针对相应 T/步骤的明确通过反馈时，L4 持续为待验证；部分反馈只关闭
  已确认的项。每轮结果和交接都列出 `QA_PENDING.md` 中仍待人工 QA 的功能。

关闭当前批次并编写下一份 `PHASEx.md` 时，必须在新阶段文档中列出验证清单：L1 的当前 Txx、
L2 必跑的旧 T 编号、仅在特定代码路径变动时才跑的旧 T 编号及触发条件、L4 的 UI 检查项，
以及 L3 是“计划执行”还是“默认不执行”。若计划执行 L3，写明覆盖的完整 T 编号范围、
CLI/lifecycle 范围和原因；若默认不执行，写明升级触发条件及触发后的完整范围。
批次文档还须列出去重后的 L1/L2/L4 联合清单。开发时发现新的受影响路径，先在相关 PHASE
及批次文档补上对应 T 编号和原因，再运行测试；批次收口在 `STATUS.md` 只记录实际执行的清单
和结果。不要把“受影响旧路径”留作无编号的验收要求。

优先用命令行、脚本和 Replay API 完成可直接判定的截帧、载入、事件切换、资源/像素
断言、导出内容和日志检查。需要 qrenderdoc 可见交互的部分预先合并成一次用户 L4；
验收单必须列明步骤、预期和简短反馈格式，不能仅给宽泛的检查项目。用户的观察
不能由自动测试冒充；“验收单已交付”也不算 L4 通过。具体分工见 [QA_GUIDE.md](QA_GUIDE.md)。

命令输出应优先重定向到日志文件；成功时只读取摘要，失败时只读取相关错误和末尾日志。修复后只重跑
受影响的 L1/L2 和必要的 L4。后一个 T 修改了前一个 T 的公共路径时，重跑受影响的前一个 T
自动断言。修改公共 replay、序列化、资源所有权、事件语义或通用 UI 路径时，
先列出可能受影响的旧 fixture 并做定向验证；只有具体风险跨越这些 fixture、无法合理界定范围时
才升级到 L3。不要仅凭“改了公共文件”就运行全量回归。批内仍须实现和测试必要功能，不能
为省 QA 删掉功能或测试；确实不必要的功能要说明依据并更新范围。UI 批末实机验收不可省略，
默认由用户按验收单完成。

完整回归命令（仅在上述 L3 条件成立时运行）：

```sh
./util/buildscripts/scripts/test_metal_capture_macos.sh
```

该脚本构建 qrenderdoc/renderdoccmd 与 Metal demos，并自动运行当前全部 T00-Txx 的 native、
capture/XML、replay/output、CLI replay 和 lifecycle。产物位于 `captures/metal-smoke/`；历史阶段
证据按需查看对应 PHASE 文档，不在本文件重复列出逐场景命令。

以下为 BATCH29-30 进行时的历史交接记录，当前状态以本文件顶部为准。
当时最新关闭的是 T00-T27。BATCH29-30 的 T28/T29 功能与最终联合自动均通过，
`/tmp/batch29-30-final.log` 记录 11×10 lifecycle、定向场景与 CLI；L3 未触发。两份正式
capture 已准备，`QA_BATCH29-30.md` 是合并用户 L4 验收单；`QA_PENDING.md` 中 T28/T29
均部分待复验。用户确认 T28 其余 GUI 项和 EID 1 Texture 20 通过，首次 ⌘O
重复弹窗也已解决；T28 当前 EID 7 重新保存的 10×7 DDS 已与自动参考核对通过。T29 的 Buffer Viewer
回退/前进、画面与状态栏已通过，HTML/CSV/Save Bytes raw 已生成，agent 核对 raw
与自动参考完全一致。T29 API Inspector 原未显示无 EID 状态调用，已补列该 action
前的 structured chunks；最新 GUI SHA-256 `c1a773a05d56…`，需用户 ⌘Q 重开后在
EID 5 确认两条 setBuffer 可见，并在完整 Buffer 20 Viewer 确认 EID 2 全 `a5`。
用户还确认 T22 父行 EID 5 可展开为 EID 6/7，绘制正常。此后 Texture Viewer
右侧缩略图空白已定位为 Metal 缺 Headless output；修复后 T01/T03/T09/T11/
T12/T16/T17/T22/T25/T28/T29 的 Replay API 和逐份 CLI replay、12×10 lifecycle
通过，库 SHA-256 `81738a1bcbde…`。当前待用户最短 GUI 复验：T28/T29 右侧
Input/Output 小图，以及 T29 `$action()` 筛选。未收到余项通过反馈前两阶段和
批次不关闭；可继续开发后续功能，但每次结果和交接保留两项待验提醒。
不清理工作区；最近完整 L3 仍为 T00-T18 的 `/tmp/t18-final-regression.log`。

## Compact、继续与新任务边界

### 继续当前任务

只要当前批次尚未完成、上下文仍清晰且没有用户决策阻塞，agent 应自行继续下一项 Pxx.y；T28
自动验证通过后直接进入 T29，不等待用户再次发送“继续”。一次失败或修复循环不是切换任务的理由。

### 建议 compact

出现自动上下文压缩提示、关键输出反复截断、已经历多轮大范围排查，或 agent 难以可靠保留早期实现
细节时，应在安全检查点建议 compact。建议前必须先在 `STATUS.md` 的“恢复检查点”写明：

- 当前批次、各 T 的自动/UI 状态和第一项未完成任务；
- 已修改文件及不可回退的已有改动；
- 最后成功命令与结果；
- 当前失败命令、最短关键日志和已排除原因；
- 下一条安全操作；
- L1/L2/L4 哪些已完成或待批末验收；L3 是否有触发条件、若有是否已执行。

compact 只是压缩当前任务的聊天历史，不改变批次目标。compact 后先读上述文档和 `git diff`，直接从
检查点继续；不重新做未受后续修改影响的测试，也不假定 dirty worktree 可以清理。

### 建议新建任务

完整批次关闭后先判断当前对话能否可靠承接下一批：上下文清楚、工具可用且下一批边界明确时
继续当前对话；上下文已膨胀或需要独立的新目标时建议新建对话，并交付可直接复制的提示词。
当前批内两条切片的自动验证、受影响旧路径联合清单、两份 capture 的用户 L4、文档同步和下一批拆分全部
完成，才可说“批次完成”；若触发 L3，也须完成 L3。等待用户 L4 时，在 `STATUS.md`
及 `QA_PENDING.md` 记录最终构建、captures、自动结果、验收单和第一项待办，
回复中交付准确步骤并列出全部跨批次待验 T；未回复或漏看结果时状态原样保留。
不把自动通过写成阶段关闭。若中途因其他原因切换任务，也留下同等完整的恢复检查点。
agent 的最终回复说明批次是否关闭、下一项是什么，以及继续当前对话还是建议新对话；如建议
新对话，附可直接复制的提示词。上下文已明显变长而批次未关闭时，先写 STATUS 检查点再建议
compact，而非提前声称完成。

下一任务通用提示模板：

```text
继续 RenderDoc Metal replay 当前批次。保留全部未提交改动。接手只读 README 入口、
STATUS 当前批次/最近恢复检查点、QA_PENDING 全部待验项、当前 BATCH 与其中 PHASE 文档、
HANDOFF/QA_GUIDE 验证规则；
PLAN 与历史阶段按需查阅。从 STATUS 第一项未完成工作连续推进到批次关闭。每个 T 完成
必要的 native/capture/replay 自动验证后记录“批末 UI 待验”，继续下一 T；最终代码上按
批次清单去重运行定向验证、CLI/lifecycle。命令行可判定项由你完成；对 GUI 剩余项
按 QA_GUIDE 准备合并两份 capture 的一次性用户验收单，写明步骤、预期与反馈格式，
等待用户 L4 结果。未收到明确反馈或只有部分反馈时，未验功能持续登记在 QA_PENDING，
每次结果都提示全部待人工 QA 的 T；可以继续后续开发，不关闭未验批次。若用户反馈
不符，先修复并给最短复验；仅在沟通仍无法确认或用户明确要求时使用 Computer Use。
收到对应 T 全部步骤通过反馈后再关闭该批次。
影响范围变化时先更新 PHASE 与 BATCH 清单和原因。全量 L3 仅在列明条件触发时执行。
同步阶段与索引文档；完成后判断能否在当前对话继续，若建议新对话则给可复制提示词。
若上下文先变长，先写 STATUS 检查点再建议 compact。
```

T19 已关闭后的下一批提示：

```text
继续 RenderDoc Metal replay 的 BATCH21-22：PHASE21/T20 单命令 ICB 和 PHASE22/T21 indexed
indirect，从 P21.1 连续推进到 P22.4。保留全部未提交改动。接手只读 README、STATUS 当前批次
与最新检查点、BATCH21-22、PHASE21、PHASE22 及 HANDOFF 验证规则；历史按需追查。每个 T
立即完成必要的 native/capture/replay 自动断言，T20 记“批末 UI 待验”后继续 T21。最终构建
按批次联合清单一次性去重执行定向、CLI/lifecycle，并用最新 qrenderdoc 同一轮验收两份 capture。
影响范围变化先更新清单和原因；L3 仅在清单条件触发时覆盖 T00–T21。全部通过再关闭两阶段、
同步文档，并判断是否需要新对话；若需要，给下一批可复制提示词。
```

T21 已关闭后的下一批提示：

```text
继续 RenderDoc Metal replay 的 BATCH23-24：PHASE23/T22 多命令 ICB 与非零 execute range，
PHASE24/T23 indexed ICB，从 P23.1 连续推进到 P24.4。保留全部未提交改动。接手只读
README、STATUS 当前批次与最新检查点、BATCH23-24、PHASE23、PHASE24 和 HANDOFF 验证规则；
历史按需查阅。每个 T 立即完成必要 native/capture/XML/replay/readback/state 自动断言，
T22 自动通过后标记“批末 UI 待验”，继续 T23。最终代码按批次联合清单去重执行旧场景、
CLI replay 与 lifecycle，并用最新 qrenderdoc 同一轮验收两份 capture。影响范围变化先更新
PHASE 与 BATCH 清单和原因；L3 默认不跑，仅在清单条件触发时覆盖 T00–T23。全部通过后
同步阶段和索引文档，再判断是否需要新对话；若需要，留下 STATUS 检查点。
```

T23 已关闭后的下一批提示：

```text
继续 RenderDoc Metal replay 的 BATCH25-26：PHASE25/T24 ICB reset 后重编码，
PHASE26/T25 同一 ICB 中混合非索引与 indexed command，从 P25.1 连续推进到 P26.4。
保留全部未提交改动。接手只读 README、STATUS 当前批次与最新检查点、
BATCH25-26、PHASE25、PHASE26 和 HANDOFF 验证规则；历史按需查阅。每个 T 立即完成
必要 native/capture/XML/replay/readback/state 自动断言；T24 自动通过后标记“批末 UI 待验”，
继续 T25。最终代码按批次联合清单去重执行旧场景、逐份 CLI replay 与 lifecycle，
并用最新 qrenderdoc 同一轮验收两份 capture。影响范围变化先更新 PHASE 与 BATCH
清单和原因；L3 默认不跑，仅在清单条件触发时覆盖 T00–T25。全部通过后同步阶段和
索引文档，再判断是否需要新对话；若需要，留下 STATUS 检查点。
```

T25 已关闭后的下一批提示：

```text
继续 RenderDoc Metal replay 的 BATCH27-28：PHASE27/T26 ICB `inheritPipelineState`，
PHASE28/T27 ICB `inheritBuffers`，从 P27.1 连续推进到 P28.4。保留全部未提交改动。
接手只读 README、STATUS 当前批次与最新检查点、BATCH27-28、PHASE27、PHASE28 和
HANDOFF 验证规则；历史按需查阅。每个 T 立即完成必要 native/capture/XML/replay/readback/state
自动断言；T26 自动通过后标记“批末 UI 待验”，继续 T27。最终代码按批次联合清单去重执行
旧场景、逐份 CLI replay 与 lifecycle，并用最新 qrenderdoc 同一轮验收两份 capture。
影响范围变化先更新 PHASE 与 BATCH 清单和原因；L3 默认不跑，仅在清单条件触发时覆盖
T00–T27。全部通过后同步阶段和索引文档，再判断是否需要新对话。
```

T27 已关闭后的下一批提示：

```text
继续 RenderDoc Metal replay 的 BATCH29-30：PHASE29/T28 compute dispatchThreads，
PHASE30/T29 compute buffer binding，从 P29.1 连续推进到 P30.4。保留全部未提交改动。
接手只读 README、STATUS 当前批次与最新检查点、BATCH29-30、PHASE29、PHASE30 和
HANDOFF/QA_GUIDE/QA_PENDING 验证规则及待验清单；历史按需查阅。每个 T 立即完成必要 native/capture/XML/replay/readback/state
自动断言；T28 自动通过后标记“批末 UI 待验”，继续 T29。最终代码按批次联合清单去重执行
旧场景、逐份 CLI replay 与 lifecycle。终端可判定的 QA 均由你完成；根据最终正式
captures 按 QA_GUIDE 输出一次性 T28/T29 GUI 验收单，列明打开路径、按 EID 合并的
步骤、预期和反馈格式，由用户在同一轮 qrenderdoc 完成 L4。若反馈不符，先指导
最短复验或修复；仅在沟通仍无法确认或用户明确要求时用 Computer Use。等待反馈时
标为“自动验证通过，等待用户 L4”，登记 QA_PENDING，每次结果列出未验 T；
用户漏掉结果或仅部分反馈时持续待验，可以继续后续开发，但不要关闭未验批次。
影响范围变化先更新 PHASE 与 BATCH
清单和原因；L3 默认不跑，仅在清单条件触发时覆盖 T00–T29。L4 通过后同步文档并关闭。
```

历史详细验收记录已移至 [HANDOFF_HISTORY.md](HANDOFF_HISTORY.md)，仅按需追查。

## 每次工作必须更新的内容

- `STATUS.md`：当前批次、各 T 自动/UI 状态、实际验证命令/结果、阻塞项、下一步。
- `QA_PENDING.md`：每个 T 的人工 L4 状态、验收单、用户反馈及跨批次未验项。
- `TEST_MATRIX.md`：只要 API 覆盖或样例状态变化，就同步修改对应行。
- `DECISIONS.md`：出现影响架构、capture 格式、兼容范围或用户可见行为的选择时新增记录。
- `PLAN.md`：阶段范围发生变化时更新；禁止只在聊天中改变计划。
- 当前 `BATCH` 与 `PHASEx.md`：影响范围变化时更新清单；批次关闭时写明下一批及每个 T 的
  编号级验证清单、L3 决策和触发条件。

更新节奏默认是“批次开始一次、切片转接/架构决策/中断时一次、批次收口一次”。不要仅为了记录
每个小修复而反复重写长文档；测试覆盖或用户可见能力实际变化时，仍必须在收口前完整同步。

## 批次与阶段关闭规则

批内前一个阶段在功能自动验证通过后只能标为“批末 UI 待验”。批次中的阶段只有在以下事项全部
完成后才能一起标为完成：

1. 阶段验收条件全部通过，或未通过项得到用户明确接受并记录。
2. 最终构建的联合 L1/L2/CLI/lifecycle、批末两份 capture 的用户 L4，以及实际触发的 L3
   均已完成；没有回复或仅完成部分步骤时，相应 T 在 `QA_PENDING.md` 持续待验证；
   验证命令、产物路径和结果已写入 `STATUS.md`。
3. 新增/变更能力已反映到 `TEST_MATRIX.md`。
4. 已知限制和遗留问题有明确任务 ID。
5. 下一批已拆成文件级或接口级任务，并在 `STATUS.md` 中指定第一项。

## 修改与验证约定

- 实现优先放在 RenderDoc 原有架构位置；不要另建绕开 replay API 的独立查看器。
- 构建产物统一放在 `build-*` 目录，不提交二进制和本机绝对配置。
- 第三方测试样例固定 commit 和许可证；未经核验不复制源码。
- 每个 Metal chunk 的支持应包含 capture/序列化、replay、状态更新和测试四方面检查。
- 暂不支持的接口应稳定返回错误/unsupported，并写清楚日志，不留下 silent success。
- 遇到新 SDK 兼容补丁时，将纯兼容修改与 replay 功能修改尽量分开。

## 建议的任务记录格式

在 `STATUS.md` 工作日志中追加：

```text
| YYYY-MM-DD HH:mm | Mx.y | owner | 做了什么 | 验证命令与结果 | 下一步/阻塞 |
```

若任务中断，必须留下：已修改文件、最后成功命令、当前失败命令、关键日志摘要和安全的下一操作。
