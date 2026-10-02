# B407：按buffer范围支持真实UE大TextureBuffer

真实UE在背景加载失败于6个TextureBuffer宽度超过旧1114112上限。保持descriptor、format、Private storage、alignment、pitch和parent range，限制改为min(128MiB,device.maxBufferLength)/pixelBytes；不分割view或替换原shader。参考官方Vulkan Serialise_vkCreateBufferView继续按原CreateInfo/native buffer backing创建。

402984b1微型Native/capture/replay通过3cases：R32Uint2621440与4587520texels、Read及Read|Write usage，最大18350080B；GPU只有3 invocations，首中尾sentinels准确，4次reset seeks/每case。16API+CLI非法width0/overflow/limit、offset/range/pitch、storage/type/mips、initial-byte组在任何Private upload/wait前拒绝。原小format gate的width-limit同步改为33554433，避免保留旧夹具宽度当设备上限。

真实UE原文件pre-submit402984b1已越过所有背景resource materialization及PrepareDescriptorTables，下一阻塞在frame MTLBuffer::DescriptorSlotEvent；没有initial GPU upload或frame GPU submission。正补齐event日志定位具体epoch/source。整帧UE/UI仍未验收，目标active，无提交推送。

正式脚本 test_metal_large_buffer_texture_macos.sh 在 a201df3a 精确库通过 3 captures / 12 reset seeks / 48 API+CLI negative groups，库前后 hash 一致。日志 local-m2-descriptor-replay/large-buffer-texture-final-test.log、metal-large-buffer-texture.fAti1V。ReadWrite usage 用例仍只有 GPU 读，未声称 UAV 写验证。
