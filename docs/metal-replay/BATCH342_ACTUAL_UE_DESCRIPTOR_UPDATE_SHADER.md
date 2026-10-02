# BATCH342：真实 UE UpdateDescriptorHandle shader 的小输入 GPU/replay

2026-10-01；持续目标 active，未提交、推送或完整 UE 帧 GPU回放。

从B341 a5907a66… zip.xml通过CPU提取第一个受限producer group：compiled
metallib4492bytes、function Main_00000f34_3a6a7053、pipeline479、无function
constants、1×1×1线程；原始library SHA256
 ae5ce0cf0d2abb57b3c26830db08c5a78a192acd424d5da544250deb48a9a6eb。
对照官方MetalBindlessDescriptors/UpdateDescriptorHandle：uniform顺序为
NumUpdates、IndicesHandle、EntriesHandle、DestinationHandle。提取时重建
Shared upload buffer在该CB commit前的CPU快照，核对indices、uniform与每个
source24bytes/producer目标offset一致，拒绝>8updates或>64KiB目标范围。

重建紧凑native buffers与三个IR buffer条目，使用该实际编译shader（不是
替代MSL shader）执行。payload内部的原UE VA/texture/metadata在此pass只是
opaque copied bytes，没有任何目标descriptor consumer；这项不宣称完整UE
重定位或纹理输出正确。root buffer VAs在小捕获中由已有v8显式来源重编码。

单条更新：encoder14223，目标offset1440，destination1464bytes；四条更新：
encoder15042，offsets17424/16176/36216/32376，destination36240bytes。
两种原生Metal、各两份capture、每份四轮dispatch/EID0 seek、API GPU字节与
CLI replay全部通过，目标packet完全一致，未写区域0xCD及EID0恢复保持不变。
日志metal-ue-scatter（见build-macos-debug/ue-actual-scatter-final.log指向目录），
脚本util/buildscripts/scripts/test_metal_ue_descriptor_scatter_macos.sh；提取
util/ue/extract_ue_metal_descriptor_scatter.py。Epic shader二进制仅在ignored
build目录保存，未加入公开仓库。库仍37063340…，没有新增driver修改。

真实UE更新内核现在已有受限GPU/replay证据；前批v7/v8的实际texture descriptor
消费另有122/186/seek证明。二者尚未串为完整UE帧。下一项vertex/fragment
来源inline、反射最小绑定尺寸与draw preflight，随后alias/multi-submit。

全量回归未跑，新增人工UI未验，完整UE图像、MRT、pass scope未验收，继续推进。
