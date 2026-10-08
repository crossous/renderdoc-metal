# B522：inline query 的 typed 间接 TLAS 初态

接续B521；参考DX12 `d3d12_command_list4_wrap.cpp` 的TLAS构建输入与typed AS依赖、Vulkan AS descriptor/构建重放，复用Metal B507–512既有kind9/10/11、72-byte间接instance→68-byte UserID重建。query不读取SBT/IFT；保留Shared standalone roots/header/output的静态ABI和全局预算。TLAS与所有子BLAS配方必须仍是初始对象，不允许帧内变更偷用旧状态；空TLAS非空贡献pointer仍需要至少4-byte有效Shared backing。

新增HLSL `CommittedInstanceID` 可见证据。普通实例结果1000/1000/0/0；间接UserID73结果2241/2241/0/0；64实例前63个移到x100，只最后UserID136可被ray命中，结果3312/3312/0/0；空和全屏蔽结果全0。Private实例source通过GPU blit上传，AS完成后清零并readback证明88或5128bytes全0，capture保存独立frozen recipe；Shared roots/header仍未GPU authored。SDK反射、精确metallib SHA、原非零调度、AIR query调用证据及原schema7/kind9、schema8/kind10、schema9/kind11参数保存并断言。

最终backend/bundle SHA256 `6f8bab506dd596155aed4396092f0dca4bd5a891dc00b03f7d9b6de2d1ec77d8`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。产物、固定依赖/源码SHA、最终库及manifest在`build-macos-debug/metal-ray-b522/`。

| 实际验证 | 结果 |
| --- | --- |
| 8 native/capture/API/CLI3 | PASS；普通root0/8、Shared/Private indirect1、Shared/Private indirect64 root8、Private empty0、Private masked64 root8 |
| 输出/状态/地址 | PASS；128事件/EID0、不同VA-ASID、root8保护字、读写usage、UserID73/136与空/全屏蔽输出 |
| 损坏 active indirect | 55组/110 API+CLI拒绝；B521声明/绑定/ABI范围及新count/type/stride/offset/child/kind/raw未知ID/NaN/options/贡献对齐 |
| 损坏 masked64 root8 | 49组/98拒绝；包含非零AS-ID、活跃mask、计数/配方/NaN/options；反例工具已适配非零root offset |
| 损坏 empty | 45组/90拒绝；包含count/type/reserved/child/offset/stride |
| 未声明Private indirect query | native/capture PASS，API4/CLI1预期拒绝 |
| B521两份旧query capture | 当前库API/CLI3 PASS，非重新nativecapture |
| 四份旧TraceRay | B512 multi64/global-local-sampler-anyhit/global-heap-Private-frame-multi/新目标heap-Private-frame-multi API/CLI3 PASS |
| 官方Apple固定MIT sample | 两scene native/capture/API270事件/EID0、10查询、CLI3 PASS；13坏sample NOT RUN |
| 六能力/语法/diff | PASS；supportsRaytracing及supportsRaytracingFromRender保持false |
| B521五十旧纹理/view/heap、full78IR/75RT/308、旧84间接证据、32CPU/AIR、Qt/ARC | 本新库 NOT RUN，不继承B521 |
| 实际UE | 本轮无新UE capture/GPU回放；此前原帧总预算阻塞不变，未无效重跑CPU失败 |

全部构建/GPU按同一互斥锁串行、有有限超时。1108完成日志严格扫描无断言/overrun/非法stream seek/未知Metal类型/资源表未空/缺geometry snapshot。149坏组全部没有GPU wait标记。最终hash一致，原B521失败完整保留；本批验收未出现新生产失败。

当前仍static Shared ABI；query的frame TLAS更新、GPU/Private/suballocated AS header、resource heap/nested UB/通用UE shader producer绑定、render混合调度缺失。下一B523按已支持的Private帧内冻结间接TLAS配方，验证两次query在重建前后UserID输出改变、前后事件回退与EID0；之后补实际UE header/producer及小工作集场景。旧用户Qt/AS ForceCrash和系统冻结仍未证明修复，RT能力保持false；持续任务未完成、未提交推送。
