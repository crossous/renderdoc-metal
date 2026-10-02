# B429：UE 帧内 Shared placement 缓冲区容量

新 UE 截帧 ef026832e5f8c3591e6234b86298b8452d1c53b0c6e9bba9a45652ebf21a6415 在 chunk30654 新建 Shared/Tracked heap22 buffer16193，length786432、offset26770688。旧帧内 heap buffer 上限128KiB 拒绝；后续 chunk30665 原 Native 全768KiB copy、两个完整 CPU 快照无需新图形API操作。

coverage65 heap frame buffer 上限改为1MiB；历史coverage仍128KiB，standalone仍8MiB。已有 Native heap layout/alignment/range、alias、source identity、copy range/1MiB单次/16MiB累计、CPU snapshot ownership和等待路径共用。按官方 D3D12 Serialise_CopyBufferRegion 和 Vulkan Serialise_vkCmdCopyBuffer 保留原长度/offset并执行原Native copy，没有缩小、CPU模拟或跳过GPU命令。

精确4145ea66ff2403d8e9589254909d6faf2990a5039d317f1404356d2b5e6d9069、metal-extended-heap.OIhPcW：768KiB 与1MiB边界两个尺寸，4captures/16resetseeks，全部Shared bytes、GPU54/80、末尾覆盖90/120、NativeVA重定位/EID0恢复通过。124 API+CLI非法容量/heap范围/CPU范围/copy范围/累计预算/legacy64反例全部GPU前拒绝。

实际UE预提交推进到背景初始内容校验，三个未保留初始数据的Shared旧分配另见B430。无UE整帧GPU回放、无提交推送。
