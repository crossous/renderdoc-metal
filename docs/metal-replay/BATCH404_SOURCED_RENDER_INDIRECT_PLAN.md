# B404：有界 sourced render indirect

B403 已从当前 UE 真帧取得全部 17 次 per-use render 参数，另有 31 次 compute 参数；捕获保持原 Native draw，pass-end 读取借用 Vulkan 方案。新 candidate coverage58 不改变 section version/Chunk enum。

在 ValidateDescriptorSlotFrame 的原 direct draw 分支解析原 indirect call，匹配 command/encoder/root pass/ordinal/source/offset/kind；从已经过完整性审计的捕获证据取得参数。继续复用 bounded draw work、index bytes/Private initial/frame upload、queue ownership/submission prefix、shader stage buffer snapshot、MRT/depth targets 和 typed descriptor source closure。零工作量仅允许已证明的 indirect。

完整写集合含附件及全部 child 的 useResource(s) Write。root pass end 检查当前 Native physical allocation footprint；future placement 用现有 device sizeAndAlign 与创建 metadata 计算同一 Native heap 范围，无 Native resource/queue 创建。解析 TextureBuffer/texture view parent，ResourceId/current Native owner/same-heap allocation/GPU address 重叠均拒绝。捕获声明缺失、source alias history、参数越界/overflow/work 超限均拒绝，保持原 Native indirect 参数和 draw。

夹具准备：Shared/Private/placement 参数、nonindex/index、parallel parent、future MRT texture、D32S8、零 draw；两次真实捕获分别经历 descriptor producer 122→186/186→122，每份四次 reset cycles、所有 MRT/DS 像素/普通 metadata、3/6/0 action 与帧末源清零。indexed 越界与负 effective baseVertex 使用原索引校验器。末尾清零使用已有普通 buffer copy 支持，不引入 fillBuffer 特例。

对象/fixture 编译进行中；等待 B403 a0f8b74a 精确全量完成后才能链接/串行 GPU。当前没有 coverage58 GPU 验证，provider 仍 diagnostic-only，没有为实际 UE 伪造整体覆盖声明。还需要零附件 UAV pass 和真实 heap/retirement/source closure 等，实际 UE/UI 未验收。目标 active，无提交推送。

2026-10-02 最终4c177a50/r4SosW定向22captures/88reset cycles/404API+CLI negative groups通过（7×36+4×38）。3/6/0真实Native original draws、Shared/Private/placement source、indexed/parallel/future MRT/DS每像素及ordinary metadata正常；末尾全128B源清零。第一版shell在macOS Bash3 empty array/nounset报错，改为非空replay_args；之后因tail-zero blit多1pass，validator按实际scope修正，OpenCapture一直正常。实际UE未整帧GPU提交，继续零附件UAV。新版本全量待与后续candidate合并验证，最近精确全量a0f8b74a。
