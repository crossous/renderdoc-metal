# BATCH71–72：自动分配 Private Heap 的 Buffer 与 Texture

T71 建立 `MTLHeap` 包装、`MTLDevice::newHeapWithDescriptor` 旧 chunk 回放和
`MTLHeap::newBuffer` 新 chunk；T72 补 `MTLHeap::newTexture`。当前只承诺
**Private + Automatic + Tracked hazard** 的 heap；buffer 仅长度516等合理范围、
无 placement offset，texture 仅自动分配的普通 2D RGBA8/BGRA8 单层单采样；
placement、sparse、aliasable、heap 内其它子资源及 untracked hazard 不支持。
原生创建保持应用传入的 descriptor；超出上述范围的 capture 可以记录，但离线
重放明确拒绝。Heap 子资源在 capture/replay 中均保留父资源关系，`buffer.heap` 和
`texture.heap` 在注入期间返回对应包装对象，而不是泄漏原生对象。

T71 复用 Private buffer 测试：heap 的 516-byte buffer 经 blit/compute 写入、
readback 和 fragment 读取；默认 hazard 曾出现 `0xA5` 填充值偶发未被 compute
更新，故测试与回放范围明确采用 tracked hazard。5次 Metal Validation 原生重复、
捕获、结构字段、CLI三轮、API 数据/画面/seek、13个畸形输入拒绝通过；与T39输出图
逐字节一致。入口 `bash util/buildscripts/scripts/test_metal_capture_batch71_macos.sh`。

T72 复用三阶段 Private texture 测试：同一 heap 子纹理反复 GPU clear，再由
fragment 采样到 backbuffer。5次 Metal Validation 原生重复、捕获、结构字段、
CLI三轮、API 三阶段画面与前后 seek、17个畸形输入拒绝通过；T64/T71定向回放通过。
入口 `bash util/buildscripts/scripts/test_metal_capture_batch72_macos.sh`。

T70 的 GPU 间接 ICB 范围仍是单独的预期拒绝负例，使用新构建重录复验通过；
不要把它计入成功回放。原始标记目前 **96 bridge / 59旧chunk**，Max1286；
bridge 计数相对 BATCH70 暂增1，因为以前没有 Heap 包装，新增后把仍未支持的
offset/placement buffer 与 texture 入口显式标出来了。此数字并不意味着
新回归或功能倒退；真实新增的是 heap 创建和两类子资源回放。

联合终端回归通过：`RENDERDOC_METAL_LAST_TEST=72 bash
util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`，**72份成功捕获
API/CLI、1947个畸形输入、720次 lifecycle 打开**，resident growth 1556480 bytes，
日志 `/tmp/metal-batch72-full.log`。T70负例单独通过，不计入72份。
`git diff --check` 与批次脚本语法检查通过。库与app内嵌库SHA-256均为
`c36b553dcd149396152c8c17bd0366937235da2472e63a14523bf5305f500acb`；
T71 capture `5d4f73a88a97edbad6f262ad464431f5904a76216d99050224fe26640df6c343`；
T72 capture `958442028c01414b6ad6a9b234c38832f9ab41f7674a3fec6e64b7e8903a0f19`。
本批未运行 GUI/Computer Use；T71/T72 需集中人工 QA，T70 不可离线打开。
