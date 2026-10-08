# B528：UE 小场景的有界 raster shadow 配置

完成配置/真实截帧/CPU预检，本批不是UE replay验收。run_testproj_metal_ray_macos.py新增可选--small-shadows，将MaxResolution/MaxCSMResolution设256、MaxCascades1，并在原生UE日志打印cvar。来自本地Renderer MaxCSMShadowResolution/SceneRendering MaxCascades语义。仅isolated诊断命令，无installed engine或用户Testproj修改，两个生产能力false，backend未改。

新capture成功9fa35417b120748c80eb64ddbc7564c0a75ab3c5511ae6aacccf4c84778a7135、62,399,341bytes：6间接TLAS、186直接+112间接compute，60原AIR模块全审（0 unvalidated）。Main_0000a274_46403057两调度[2,72,1]；Main_0000c358_ed865861两调度[908,1,1]/[794,1,1]。精确library/function/PSO绑定及实际frozen间接参数证明包含hardware query生命周期，未证明每invocation分支、命中输出、图像/UE GPU replay/事件回退与绑定。

原生日志MaxResolution256/MaxCSM256/MaxCascades1均LastSetBy SystemSettingsIni，原frame Depth16 8192x2048变为256x256；XML所有Depth16 extent≤512。捕获保存后owned cleanup -9，不计UE自行正常关闭。启动120秒/延迟6/停滞12/保存后20、outer160，共享GPU锁串行；当前无新的UE启动或capture失败。

43heap共2,962,489,344bytes，初态Contents blob1,608,781,321；heap比B527低、初态反而增加，capture包含更多scene调度，不能声称单调降内存或总预算通过。normal-open API4仍frame-born/invalid identity，CPU65 mandatory-GPU-disabled API4下一R32Uint3D192x48x48/mip1/usage3/Private frame placement texture拒绝；已越过Depth16 guard。两检查均无GPUwait/断言/overrun/资源表诊断。没有改变原capture覆盖或忽略错误。

产物build-macos-debug/metal-ray-b528/manifest.json、UE-capture/original.rdc/original.zip.xml/AIR-proof/manifest.json、ray-workload-inventory、small-scene-memory.json/preflight-manifest.json及两预检log。原生session在B527 scene-minimal/project/Saved/RenderDocMetalSessions/20261006-191201，cvar和过程日志保留。Small-scene memory首版查询接口名误用newTextureWithDescriptor导致depth列表空，已按实际MTLHeap::newTexture(offset)与enum string字段纠正，再核验捕获的实际256尺寸；生产与capture未变。

最终backend/bundle SHA256 e231e2bd394f1781d7093ecad2da5318764f11d7c814ad4e9bcea578c3a14706；GUI3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91；isolated provider9d6349a655a5fb93c1fbeb1dd657b4c3eccce1167f07e8220e6a877e96a7e225。原metallib及tool/engine/plugin等hash见各manifest。本批仅syntax/diff与UE阶段，未重跑sample/IR/full/Qt/ARC；B526旧回归仍为同库的历史证据。

下一B529补真实R32Uint3D frame extent（参照VK/DX12 volume typed resource/初态），sample验证后再CPU查UE下一缺口；typed Shared动态AS header/frame-born heap/nested roots/producer及总预算仍未闭合。原Qt/AS ForceCrash与系统冻结根因未修，持续active，无提交推送。
