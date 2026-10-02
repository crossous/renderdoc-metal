# B394：实际 UE index producer 后编码、先提交（进行中）

CPU index provenance audit 发现实际 UE12230757 的 4 次 indexed draw（38218/38300/
38385/38426，buffer17688/17694/17699/17701）在文件中早于其 staging upload。
按已捕获 command buffer commit 顺序模拟 GPU 操作后，195 direct indexed draws中
135次frame-index读、9个future index buffer都具有完整已知字节，effective范围合法，问题0。
工具 util/ue/audit_ue_metal_index_uploads.py，输出 ue-heap64.index-upload-audit.json；
仅CPU provenance，未证明 Native shader/residency/alias/completion 或整帧 replay。

v51候选：Future index read/copy按所属command buffer记录，在preflight commit点按该
submission内部顺序模拟，复用现有D3D12/Vulkan-style提交顺序，保持所有range/source checks。
v50规则仍保留兼容。Native库仍008ca84b v50，全量运行期间不替换。

新增微型late-upload fixture：consumer先编码，producer后编码但先commit；Native已知
shader使用index%3/%4保护小数组。需要分别证明整帧及draw event partial seek。
现有core partial tail可能在target draw文件offset停下而缺少后编码producer；尚未放宽
实际UE整帧或绕过source闭包。当前尚未构建/运行候选，持续目标active。

间接命令新增CPU事实：48 indirect归属19 encoders，有5个encoder各6次调用，
12个单次/1个4次/1个2次；部分arg buffer在同一encoder被explicit Write引用。
因此不能只在encoder end/frame end读一次当每个dispatch的参数。
官方Vulkan FetchIndirectData/ExecuteIndirectReadback逐使用点复制、绘制延至renderpass end；
Metal需要对应的逐使用点GPU快照，不能任意split/CPUwait引入queue reservation死锁。


当前v51库1d6998a72769619026f4f24f629f474a3b78414a3b17f82922ce10b8edde03fe。
首次late原生capture成功，Replay loading因Snapshot仍按最后selected CB误归属而失败；
core现在使用preflight建立的chunkOffset→commit owner，选择已有未提交owner再应用快照。
小帧整帧Native/replay以及compute seek通过。Helper必须在draw事件检查pipeline，
Present/EndPass时状态已清空；不要把该测试失误当Metal pipeline修复。

最初partial也通过，但加Tail index poison后正确复现缺口：metal-submission-index.EgVy5f，
firstDraw EID73/cycle0得到128/64/32/255清屏像素，期望128/64/186/255。
当前v51完整源上传顺序与snapshot ownership已接通；部分replay在encoded draw offset
停下，丢失文件后方、GPU提交前缀中的producer。没有把原生heap残留当恢复证明。

下一步对照D3D12 baked command list/ExecuteCommandLists与Vulkan ReplayQueueSubmit的
提交前缀计划，重放已验证的实际producer和其资源出生/CPU快照依赖，再部分提交consumer。
不以直接改输出、旧GPU VA、帧末index快照或未证明的提前alias覆盖代替真实队列依赖。
目标active，UI再次CUA检查仍locked，所有GPU任务已退出；未提交/推送。
