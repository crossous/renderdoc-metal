# B395：部分 replay 恢复实际 copy submission 前缀（定向通过）

B394增强夹具已证明：只停在draw编码offset，会遗漏之后编码、之前GPU提交的上传；
Private heap残留会掩盖问题。参考D3D12 Serialise_ExecuteCommandLists/baked command lists、
Vulkan ReplayQueueSubmit/command-buffer action replay，v52保留捕获提交顺序与原始chunk offsets。

当前范围是已完整preflight的普通buffer-copy-only submission：所有Shared源复制字节必须由
完整initial/creation或该submission的CPU快照证明，原offset/range/每条与累计预算保留。
排除render/compute、typed copies、signal、enqueue与未知GPU产物。计划包含原command
creation/blit birth/copy/end、CPU snapshot与commit，以及相关future buffer birth依赖。
partial tail提交consumer之前，以原始ResourceId和原始API chunks重放缺失producer；
先按原出生顺序创建必要future buffers，再恢复原owner快照并提交原producer。
不替换index binding、不修改输出，不把final index bytes当initial；GPU等待仍在已提交
prefix和所有tail提交完成之后进行。现有command context在处理dependency后恢复。

精确库5b329da94168b409f881e736bd4340087ebb41ac7f10fb6c83cf142fe51f9da3。
- metal-frame-index.x1t174：同/跨submission四捕获/16 seek cycles/146 API+CLI反例。
- metal-submission-index.4ArE4M：后编码先提交四捕获/16 cycles/16 first-draw seeks/144反例。
- metal-submission-index.85OIet：staging在draw之后出生；同样4/16/16/144，通过missing prefix births=1。
- metal-submission-index.CrNUsa：11条copy、late staging与10个额外Private destination births；
  同样4/16/16/144，覆盖完整多copy计划与多个后出生资源。

后两轮helper额外确认完整frame末index实际全零，再seek firstDraw读取Native index bytes和
2×2全像素；确实恢复5/3/4的UInt16（baseVertex=-3）与UInt32 TriangleStrip，双实例/base7。
各capture256 draw、四轮compute producer/consumer/EID0和draw pipeline metadata也正确。
总16 captures/64 seek cycles、48额外first-draw seeks、578 API+CLI negative groups。
所有GPU任务串行，没有完整实际UE GPU提交。

实际UE12230757 CPU核查195 direct indexed、135 frame-index reads、9 future buffers问题0；
4个先编码consumer依赖GPU先提交17909，后者11条copy/6240B，含indices和vertex data。
新路径为其提供对应提交前缀机制；不宣称完整UE/MRT/画面/UI通过。
全量最新精确v50 008ca84b：308/7786/3080、growth12681216B；v52精确全量待下一轮。
UI再次CUA检查仍locked，持续目标active；未提交/推送。

2026-10-02 07:31：上述精确 v52 库全量通过，308 captures / 7786 malformed cases /
3080 lifecycle opens，resident growth 10403840 B。日志为
`build-macos-debug/local-m2-descriptor-replay/submission-index-combined-regression.log`。
运行结束哈希仍为5b329da9；运行期间仅编译下一候选对象，未链接替换库。
