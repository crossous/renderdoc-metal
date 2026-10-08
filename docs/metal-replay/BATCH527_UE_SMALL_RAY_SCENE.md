# B527：隔离低工作集 UE Lumen 场景

完成本批native capture/AIR证据；UE整帧输出和GPU重放仍未验收。新增prepare_ue_metal_ray_scene.py，独立Blueprint工程，仅三个Engine BasicShapes mesh和两个light；复制匹配RenderDocMetalCapture plugin，保留原用户Testproj/installed engine。NullRHI命令120秒有限、无注入，验证地图保存/角色/资源及哈希。SM6/Lumen HWRT/Inline/必需SkinCache配置；禁用无关默认engine plugins。已有输出目录拒绝覆盖。

scene和scene-minimal两次NullRHI保存关卡通过，不计GPU或RT通过。首次UE-capture启动漏SkinCache，被UE RenderUtils.cpp:1045 Fatal主动中止；按源码补r.SkinCache.CompileShaders=True，失败保留。第二UE-capture-skincache在120秒startup边界被owned cleanup(-9)，尚未ready，日志1次native PSO编译完成，不计通过。随后隔离scene-minimal关闭默认engine plugins，UE-capture-minimal有限启动/capture成功；保留启动sample显示模块加载路径。capture保存后进程owned cleanup(-9)，不能计自行正常退出。全程同GPU锁串行、无全局能力开关。

最终真实UE5.8.3CL58210709捕获1b19090e…（56,406,168bytes）：3间接TLAS、99直接+56间接compute、7heap AS/identity，163 native compile完成且无pending。60原metallib Lumen条目全部CPU反汇编，0未验证条目；实际Main_0000a274_46403057 [2,72,1]和Main_0000c358_ed865861 [786,1,1]两次非零间接调度，通过原library/function/PSO绑定和per-use frozen groups证明包含hardware query生命周期。逐invocation分支、命中/阴影输出、图像/完整GPU重放/事件与资源绑定未验，不将shader AIR或cvar请求计这些通过。

45heaps共3,029,598,208bytes，原B513为70heaps/5,266,571,264；初态Contents blob1,227,522,505，原4,362,319,907。只是资源inventory，CPU预检尚在纹理创建处拒绝，未达总预算判断，不能宣称预算通过。normal-open API4 invalid frame-born identity，CPU65 mandatory-no-GPU API4因Depth16Unorm 8192x2048x1/mip1/usage5/Private frame placement texture；两个检查均无replay wait/断言/overrun/资源表诊断，没有UE GPU replay。

产物build-macos-debug/metal-ray-b527：manifest.json、scene(-minimal)/manifest.json/scene-manifest.json/NullRHI-generation.log及生成project、UE-capture(-skincache/-minimal)保留所有失败/成功日志；最终UE-capture-minimal/original.rdc/original.zip.xml/AIR-proof/manifest.json/ray-workload-inventory.json；small-scene-memory.json及preflight-manifest.json/normal-open.log/CPU65.log。capture SHA256：1b19090ef789e7ab1af01a37f90ab225649063f2b7797a809054eaf1f90b3c6d。

backend/bundle仍e231e2bd394f1781d7093ecad2da5318764f11d7c814ad4e9bcea578c3a14706，GUI3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91；隔离provider仍9d6349a655a5fb93c1fbeb1dd657b4c3eccce1167f07e8220e6a877e96a7e225。项目/地图/源码/plugin/engine及完整querylibrary hash见各manifest。没有改backend，B526回归属于该库既有证据，本批未再跑官方/IR/Qt/ARC/full旧回归。

下一B528按真实Shadow CSM配置缩小Depth16资源并重新核验RayQuery与实际预算；随后补Shared动态AS header、frame-born/heap/nested roots与可靠producer。不得用CPU覆盖覆盖原capture或启用全局能力。系统冻结与用户Qt/AS ForceCrash原根因未闭环。两RT flagsfalse，持续任务active，未提交推送。
