# BATCH344：真实 UE 更新 shader 接入图形描述符消费者

2026-10-01，持续推进，无提交或推送。

复用 B342 从 UE a5907a66… 捕获导出的实际 compiled metallib：
Main_00000f34_3a6a7053，SHA256 ae5ce0cf0d2abb57b3c26830db08c5a78a192acd424d5da544250deb48a9a6eb。
在独立小用例重建 Shared source/indices/uniform/static-sampler 和三个 IR buffer
条目；uniform {1,1,0,2}，目标索引 0，单线程更新一个 24-byte Texture4 目标槽。
source packet 的 native texture ID 有明确 ResourceId 来源，由回放重编码。
真实 UE shader 在 GPU 复制这个新 packet 后，计算与顶点/片元消费者读取它，
得到预期 122→186 / 186→122 和最终图像。消费者仍为受限验证 MSL，并非完整 UE
图形 shader；Epic binary 只保存在 ignored build 目录。

原生执行、两捕获、每份四轮事件跳转、GPU 字节、普通常量、最终像素、CLI replay、
27 组 API+CLI 图形反例全部通过。独立脚本 test_metal_ue_descriptor_graphics_macos.sh，
日志 build-macos-debug/metal-ue-graphics.MoQXyQ；库与 B343 相同 490bf140…。

这次已验证真实更新内核写出的重定位描述符被后续图形阶段实际使用，超出 B342
opaque-copy 的证据。但完整 UE frame 多提交/alias/vertex-fetch/MRT/pass scope
尚未接通。全量未跑，新增 UI 未验，下一项多目标与跨 render-pass 消费。
