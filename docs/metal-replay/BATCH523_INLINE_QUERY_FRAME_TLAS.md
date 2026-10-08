# B523：inline query 的帧内 TLAS 当前配方

参考DX12/Vulkan build前后AS状态，复用Metal B509–512既有Private encoder-end冻结typed间接输入与当前配方。query沿普通compute，不生成SBT/IFT。放开同一已初始化TLAS的当前配方；保留Shared immutable roots/header/output、1D≤4096线程、≤64实例与子BLAS初态约束。帧内冻结配方独立于EID0初态，顺序和队列检查在GPU提交前完成；新AS目标、动态BLAS、refit/copy仍未扩到query。

样例两次query：初始UserID73/空/屏蔽；GPU blit上传Private instance，AS encoder-end冻结并重建UserID74，source构建后清零；第二query。普通结果2241→2258，64实例仅最后可命中且UserID136→137，3312→3329；初始空/全屏蔽0→2258或3329。两个Shared输出和两个不同roots声明使用同PSO，四次前后往返含EID0每场景24选择，防止读到最终配方冒充之前事件。

最终backend/bundle SHA256 `f71629a0a4c87b151827fea9b890f9e7e2c16e8a94b844da8932c48f48b2e66d`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。最终库/manifest/固定SDK版本与runtime headers hash在`build-macos-debug/metal-ray-b523/`。

| 实际范围 | 结果 |
| --- | --- |
| 6 native/capture/API/CLI3 | active/empty/masked各1实例root0及64实例root8 PASS；原两次非零dispatch、实际metallib SHA/AIR query与原typed frame冻结recipe参数/bytes核验 |
| 144事件/EID0 | PASS；两个输出前后独立、root与AS ID强制变化、两个roots GPU VA不同且共享header、两组保护字、before输出不出现after写usage、initial空/屏蔽不提前出现BLAS CS read |
| Private source | native/capture清零88/5128bytes PASS；`source-zero-verification`最终强化API六份/144选择，GetBufferData全部0及EID0恢复 PASS |
| 239损坏组 | `final-invalid` active85/empty75/masked64 root8 79；478 API/CLI预提交拒绝，无GPU wait，覆盖初态/ABI以及frame source/target/child/raw/encoder/queue/commit/参数写入 |
| 合法控制 | `final-legal-controls`各两份共六组API/CLI3 PASS；frame UserID75、child GPU-ID一致重编号，创建bytes/初态/commit CPU快照/冻结recipe均同步；24事件、最终source零值/EID0/usage/地址验证 |
| 未声明frame query | native/capture PASS，API4/CLI1预期拒绝 |
| 旧query/TraceRay | B521两+B522八query原capture、四份B512 Private/multi/local/heap TraceRay API/CLI3 PASS；未重跑其native |
| 官方固定Apple MIT sample | 两scene native/capture/API270事件/EID0、10查询、CLI3 PASS；13坏sample NOT RUN |
| 六能力/语法/diff | PASS，两公开RT能力false |
| 旧50texture/full78IR/75RT/308/旧84/32CPU-AIR/Qt/ARC | 当前库NOT RUN，不继承旧hash |
| 实际UE | 本轮无新UE capture/GPU提交/输出；此前total预算阻塞不变，未无效重跑 |

1744完成日志严格扫描无断言/overrun/非法stream seek/未知Metal类型/资源表未空/缺geometry snapshot。所有构建/GPU互斥串行、有限超时，最终backend/bundle哈希一致。

失败保留：强化oracle误用SDObject children成员导致编译失败，改NumChildren/GetChild；`regressions-oracle-compile-fail`。合法UserID控制先改creation/frozen而漏commit-time CPU snapshot，真实immutability guard正确拒绝；第一次修正又使用错误chunk显示名，仍被拒绝，实际名为Internal_MTLBufferModifyCPUContents。两失败目录`invalid-empty`、`empty-legal-controls`保留；修为创建/初态/commit快照一致修改，重新239坏组及六合法控制全部通过，未放松生产immutable guard，不把失败计通过。早期`invalid-active`结果被最终复验替代，旧日志留存。

下一B524补query对Private帧内BLAS几何当前配方的消费，复用B511/B512冻结范围/几何顺序；之后补Private/GPU/suballocated header、heap/nested根参数、可靠UE producer与小工作集UE。当前仅Shared ABI、已有TLAS目标、静态子BLAS、独立compute，无通用UE成功证据。原用户Qt/AS ForceCrash及系统冻结未闭环，生产RT能力保持false，持续任务未完成、未提交推送。
