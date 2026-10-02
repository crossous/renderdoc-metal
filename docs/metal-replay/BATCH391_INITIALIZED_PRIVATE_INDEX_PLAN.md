# B391：初始化 Private 索引缓冲（定向通过）

v48 允许已经捕获完整 Initial Contents 的背景 Private 索引，复用原始 ResourceId、
类型/offset/range 校验和通用初始内容恢复。没有读取 Private contents()，没有使用捕获 VA。
依旧拒绝帧内创建、别名、GPU 写入和声明的 typed backing；旧 coverage 规则保持。

精确库 257c43039dd9a511c537fa6fed8118d559a576490c03495086e0345ebc57a333。
metal-private-index.DjeblM：UInt16/UInt32、各两份捕获，16 seek cycles、116 API+CLI
negative groups，通过 Native 预帧上传、GPU producer/consumer、IR constants、2×2 全像素和 EID0。
Private 初始字节缺失/截断、旧 v47、错资源/类型/范围/常量/写入等均在帧内 GPU 提交前拒绝。

全量最近精确 466b749c v46：308 captures/7786 negatives/3080 lifecycle，
growth 8880128B。尚无 v48 精确全量；后续与绘制参数变更合并验证。
当前 UE12230757 尚未整帧 GPU replay，人工 UI 锁屏未验；持续目标 active，无提交/推送。
