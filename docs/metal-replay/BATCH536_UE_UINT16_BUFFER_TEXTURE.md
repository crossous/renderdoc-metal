# B536：实际UE RGBA16Uint TextureBuffer

参照VK Serialise_vkCreateBufferView typed format/parent/offset/range与DX12 CreateShaderResourceView Buffer typed descriptor及初态，补Metal格式1138bytes，父Private buffer全量初态、对齐/范围/身份/资源预算保持。样例17格式，新增uint16高位四通道，7/8192宽度、零/非零offset、RO与真实RW调度、首中末/写入前后/EID0恢复。

初次验证 verified-formats：native/capture通过，API失败于格式113 rawreadback长度0，未计通过，完整日志与原capture保留。补GetTextureDataBlockShape白名单RGBA16Uint，复用已有8byte footprint、原始parent范围读回、共享unsigned decoder/PickPixel。新库构建与fixed-formats验证接续；没有提前启用生产能力。

第二轮fixed-formats：tiny及wide RO原生/捕获/API/CLI和36坏格式检查通过；wide RW native/capture通过但纯write dispatchThreads因旧2D copy pair要求API4，失败保留。第三轮verified-rw：实际两capture完整uint16写读/事件/EID0通过；writer-readonly反例API4正确拒绝，但已经发生初态GPU上传，原oracle要求所有拒绝GPU前导致FAIL（不计通过）。新路径限定authoritative shader reflection、write-only UInt4/16-bit TextureBuffer、已验证Private parent、ShaderReadWrite用途、合法inline/buffer绑定和tiny1D grid；无src/output要求与VK storage texel buffer/DX12 typedUAV一致。其他dispatch路径保持。最终final-formats重跑完整三shape及反例，writer反例明确加载期/提交writer前拒绝，保留初态上传范围。


## 最终验收

2026-10-06 [B536](BATCH536_UE_UINT16_BUFFER_TEXTURE.md)：补RGBA16Uint TextureBuffer8byte创建/读回/PickPixel及authoritative reflection的纯write typedUAV路径；backend/bundle5bbd0636、GUI3ba30e36。3shape六native/capture/API/CLI、408raw/pixel、24seek cycles/88事件选择、真实高位RGBA与RO/RW、零/非零offset、usage及EID0输出清零通过。72格式/范围/初态坏组144上传前拒绝，3writer组6加载期拒绝（发生初态上传，不计GPU前）。旧16格式两份/旧large R32三份API/CLI、新163初态纹理两capture12696subresource、八query/IR64、官方两scene270事件10查询及六能力通过，630完成日志无严格诊断。实际B535UE仅pre-submit65已越过buffer-view创建，下一slot25:27432/gen5508/type4无typedsource，API4且无GPUwait/初态上传；未验UE frameGPU/output/EID0。首三失败/oracle误判保留，生产flagsfalse、guard不放宽；下一B537显式AS-header factory/producer与typed动态header闭包，持续active、未提交推送。

最终 `util/test/metal/metal_buffer_texture_formats_sample_gate.py` 可直接复现；构建脚本现在使用同一共享GPU锁、有限超时、owned进程组清理与逐进程保留日志，避免旧脚本无锁构建/GPU重叠。命令：

    python3 util/test/metal/metal_buffer_texture_formats_sample_gate.py --build --work-dir build-macos-debug/metal-buffer-formats-new-run

| 输入/检查 | 实际结果 |
| --- | --- |
| format113宽7，RO非零offset | Native/两capture/API/CLI PASS |
| format113宽8192，RO零offset、row65536 | Native/两capture/API/CLI PASS |
| format113宽8192，RW非零offset、row65536 | Native/两capture/API/CLI PASS；三个GPU位置四通道写后重新读 |
| 每capture四次last/读/写/写后/0 | 六capture408raw/pixel检查，88事件选择，CS_Resource/CS_RWResource及输出清零通过 |
| 三新格式+旧第一format各18坏组 | 72组144 API+CLI拒绝，全在初态上传前 |
| writer用途变RO、缺texture、grid uint64溢出 | 3组6 API+CLI加载阶段拒绝；初态上传发生，writer dispatch未执行，非GPU前拒绝 |
| 旧Q2i5vF16格式两capture、large R32小/读/写三capture | 当前强oracle兼容显式legacy16；API+CLI PASS |
| 新RGBA16Uint普通Private二维/数组/cube/cube-array/3D，加旧158格式/type | Native/两capture/API/CLI PASS，163textures、两份共12696subresource checks、全raw bytes和uint16 PickPixel；此处新格式仅驻留/初态读取，真实新格式shader消费由上面的TextureBuffer保证 |
| 旧B526八query及B512 multi64 TraceRay | 当前库API+CLI PASS |
| 官方Apple两scene/六原生能力 | PASS；270事件/EID0、10queries、CLI3。ZIP4ee961f8/license1579cf42核验；本批fresh metallib74ea7a8b，详见manifest，不引用旧metallibhash |
| 实际B535UE pre-submit65 | API4于descriptor slot25:27432/gen5508/type4 typed source缺失；没有GPUwait/初态上传/frame replay |

公开pure write路径限定UInt4/16-bit TextureBuffer、ShaderReadWrite用途、已验证Private parent、authoritative shader reflection、全部required buffer/texture绑定及1D合法grid≤width；旧2D copy与其他dispatch分支保持。纹理读取使用已有parent offset/length readback和unsigned共享decoder，不创建额外AS内部查看功能。

## 失败、产物与边界

首轮rawreadback0、第二轮write旧copy guard，以及第三轮writer-readonly oracle过严FAIL全部保留在verified-formats/fixed-formats/verified-rw；未记为通过。新final-formats已完整通过，最后永久sample-gate入口在同一最终库上fresh capture再验，包含更强usage/EID0清零oracle。

证据：`build-macos-debug/metal-ray-b536/sample-gate/manifest.json`、regressions/manifest.json、pre-submit-manifest.json、next-slot-source.json、final-manifest.json和librenderdoc-final.dylib。最终backend/bundle5bbd06360e0d972c02cc67c67ad4e34ff814bf4b8f0b9704b76e14556caee1f7、GUI3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91；起止库hash一致。630最终有效日志严格扫描无断言/overrun/资源表/非法seek，失败目录独立，不混入通过扫描。

B535 original.rdc SHA fbe1b7ac不改，未新启动UE；pre-submit推进到缺typed producer的24byte row（first word90194739456，second0），没有DescriptorSlotBinding，不能猜buffer/AS资源。AS factory CreateDescriptor(FMetalAccelerationStructure*) 调IRDescriptorTableSetAccelerationStructure，当前provider尚未Created typed语义；匹配地址仅用于定位，不能作为接入证据。下一B537须明确记录header buffer/offset/64byte ABI、对应TLAS与contribution来源，并可靠关联query shader/PSO/per-use roots，不能只补地址然后宣称内部AS-ID已重定位。

UE图像/整帧重放/typed query binding/事件及EID0、full78IR/75RT/308/QtARC、官方13坏sample/旧78frame纹理全库未跑；B534的旧hash验收不冒充当前库。原ForceCrash/系统重启/Qt崩溃根因未修。生产supportsRaytracing/FromRender false，持续任务active、无提交推送。
