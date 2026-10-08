# B514：UE 帧内 R16Float 多 mip 与原生地址绑定分析

接续 B513 的实际 UE Lumen HWRT 捕获，补已证实的 R16Float 256×128、8 mip、usage3 帧内 placement 纹理。最终 backend/bundle SHA256 `cc91bb098cea214a6eba858996149a95517883a4aaf185a9040bcbbfed3e8cb3`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。证据根 `build-macos-debug/metal-ray-b514/`，最终库保留 `final-binary/librenderdoc.dylib`，详细 hashes 在 manifest.json。无提交/推送。

## 实现与跨 API 对照

DX12 `d3d12_initstate.cpp:415–527` 按 MipLevels/array/plane 保存 subresources；Vulkan `vk_initstate.cpp:2249` 遍历 image mipLevels。Metal 已有逐 mip 数据上传、初态预算和读回，本批补对应帧内 texture 的许可范围：coverage65、Private/Tracked、2D、R16Float、RW usage3、单样本/单层/depth1、宽高≤512、完整链长度与总字节预算，保持原 native placement size/alignment、heap范围、birth/alias/生命周期证据。没有加新 chunk 或全局放宽格式。多 mip parent 的 subset view 仍拒绝，防止把完整 parent descriptor 误当单 mip view。

测试暴露 plain computeCommandEncoder 只有两个ResourceId、16byte，却按 WithDispatchType 读取额外字段。预检现在按三种真实接口格式读取，plain implicit serial，WithDispatchType/WithDescriptor 保留 dispatchType 和 counter 约束。

参照 DX12 typed root/descriptor 及 Vulkan typed descriptor 的资源关联，AIR分析接受 air.buffer 和 air.indirect_buffer，跟踪已有 typed 来源的 ulong load/inttoptr、纹理句柄 ulong 转换；不扫描任意标量地址，未声明整数及未知动态表达式保持Unknown，不将驻留闭包全部误报为UAV。CPU回归覆盖带引号 struct layout、标量伪指针/伪句柄拒绝。动态循环texture phi仍未解析。fixture 使用 immutable local texture handle，准确记录该 shader 的调用；未承诺任意 AIR 路径均能识别。

## 验证

`python3 util/test/metal/metal_descriptor_frame_mip_gate.py --work-dir build-macos-debug/metal-ray-b514/final-validation`。生成每场景MSL，用Xcode metal/metallib编译，native/capture共用确切二进制。8/9 mip为256×128，10 mip为512×512；两个连续capture分别使用同heap不同offset，3个dispatch依次写、读、覆盖写，各 mip 值不同。native读最后mip输出120/128/136，capture一致。最终 `final-usage-oracle/` 用强化后的helper重跑14份capture（包含旧四类八份）：OpenCapture时两次CS_RW和第二dispatch只读usage、所有像素字节和每mip首尾PickPixel、四次末事件→读事件→EID0往返、descriptor table EID0归零、texture native ID强制变化均通过。EID0未初始化纹理像素没有golden，不记为像素归零证明。

| 范围 | 结果 |
| --- | --- |
| 新8/9/10 mip | 3次native、6份capture/API/CLI3 PASS；全部mip精确half值、各mipPickPixel、72事件选择/EID0 |
| 旧frame family | array、volume、volume32、extended2D：4次native、8份capture/API/CLI3 PASS；单mip及三维/数组行为保持 |
| 最终usage oracle | 14份、168事件选择/EID0 PASS；使用确切captured ID、读写EID区分，非泛化UAV闭包 |
| 新坏capture | 18组，API4+CLI1共36次正确拒绝；shape/mip/usage/storage/heap/重复birth/旧coverage |
| 间接旧回归 | compute8 + render8、112 seek/reset PASS；84组证据API+CLI168拒绝，2执行错配4拒绝、2独立拒绝控制 |
| 旧IR | frame multi64 API与CLI3 PASS，本批未重跑78组native或全量IR反例 |
| 官方MIT Apple sample | 两scene native/capture/API三方向270事件/CLI3、10尺寸查询 PASS；原zip/license hash核验；本批未跑官方13坏capture |
| 能力及CPU | 6能力查询、32 Python tests、AIR独立断言测试、语法/diff PASS；公开RT能力仍false |
| 实际UE | 只CPU65预检，原capture未修改；越过R16 mip纹理，下一12050 RGB10A2二维320×240 usage3拒绝，未执行UE GPU回放 |
| 完整78IR/75RT/308、Qt原崩溃、ARC早释、UE最终图像/事件 | NOT RUN / NOT VALIDATED；不继承B512/B513旧库结果 |

构建和所有GPU测试串行、统一互斥；所有完成进程已终止。最终日志扫描统计与诊断命中见manifest，包含正确拒绝日志。`first-mips/` 越界、`final-mips/` 无usage、`binary-mips/`、`verified-mips/` 的中间失败保留。MSL-only capture不能凭typed闭包提供shader读写证据，因此fixture增加精确metallib模式。中间regex raw-string定界编译失败、错误qrenderdoc目标、Python导入路径及旧compute helper参数错误已纠正，未当产品通过。`regressions/manifest.json`保留helper调用失败，四类真实GPU通过由最终usage oracle再次确认。

## 下一批

原真实UE capture哈希仍为B513的401db9a1…，本批没有重新运行UE或声称输出通过。下一补RGB10A2二维RW实际范围，随后继续typed inline-query资源包/AS header与producer关联。production两能力false，AS内部查看/RT shader单步/RT Pixel History不在范围。系统重启根因、原Qt崩溃未闭环，不称已修；持续任务继续。
