# B420：原Native compute之后的frame color Load

实际UE17722 Load帧内R8Unorm512x512纹理17468。它通过table24 offset24432 type5 TextureUAV绑定，之前同command17716已有compute dispatch5371/5384；没有explicit useResource(texture)声明，Native heap residency使bindless UAV可访问。预检仅允许新R8目标Clear，漏掉原compute→render Load依赖。

候选65保存live、materialized TextureUAV source闭包在原validated非零compute dispatch的potential write commands。必须有Native active buffer binding并属于该encoder访问的typed table；descriptor-copy producer与zero-work/zero-argument shader不提供纹理写前驱。Load需同command或已提交同queue的前驱，保留typed slot/source identity、birth、retirement、usage和所有Native dispatch限制。并记录合法Clear前驱。此闭包是写权限/可能依赖，不能证明任意shader覆盖全部texel；实际执行原Native shader及原load/store，不生成替代clear或推断CPU像素。对照RenderDoc Vulkan Serialise_vkCmdBeginRenderPass沿原attachment/subresource与command状态回放。

新增fixture在frame创建Private Tracked R8纹理，以type5 typed UAV/heap residency供Native compute写入全部8x4，再原Native render Load保留数据。2captures/8reset-seeks/12 API+CLI前驱/UAV/非零工作/command提交反例组通过：replay全32pixels=64，EID0 native frame resource移除。first helper fixture仅有空BG table，触发现有初始slotShadow必须非空约束；补同既有fixture的BG scalar slot后通过。

真实UE candidate65已越过R8 Load，下一项为BG RG11B10 cubemap3499的slice4（已完整initial contents），需B421。无UE GPU提交；待最终全量与UI。

精确微测库：dee749113c1b2826ba765fbbae07357cb276cf782c066314c80a41ebabb57cf5，Native/replay/gates日志metal-frame-color-load.4DJRpq。
