# B526：inline query 的有界 Shared AS header 范围

完成。参照DX12 typed AS SRV的buffer+offset及VK明确AS descriptor重定位。根VA必须唯一归属声明header buffer；按捕获base计算偏移，8对齐、剩余至少64bytes，schema4及schema0恰好覆盖header的AS-ID和贡献VA。只解释该64-byte header的保留字，不将padding猜作身份。声明/replay从exact64放宽为至少64；仍Shared standalone、whole backing≤16KiB、无alias/heap/GPU修改且完整权威初态。旧chunk无需变更。动态更新/heap/nested UE闭包没有被放行。

八native→capture→API/CLI3场景：static32、Private64-instances4096、frame32、新目标64-instances4096、新目标16304（whole16384）、两几何32、64几何+64实例4096、初始empty64-frame32。176事件/EID0，真实四uint输出及changedASID/roots VA/usage保持，header整个prefix及最后16byte suffix逐字节0xa5不变；新目标beforeHeader亦独立验证。不是只验证CPU偏移。

两形状236坏组/472 API4-CLI1 GPU前拒绝、无wait；包含header前8bytes、错对齐、只剩32bytes、UINT64溢出、指其他buffer、错AS/VA typed offset及whole backing>16KiB。其余AS/frame/geometry坏输入也按有offset的实际header修补。五合法UserID/child-ID/几何逆序控制API/CLI3及24-event oracle通过。

当前库旧query28：B524八份强oracle仅API，B525四/B523六/B522八/B521两共20份API/CLI3；四B512 TraceRay API/CLI3；官方Apple MIT两scene/270事件/EID0/10查询/CLI3及六能力通过。未fresh native旧capture。当前未跑旧50texture/view/heap、完整78IR/75RT/308、旧84坏间接证据、32CPU/AIR、Qt/ARC及官方13坏sample。无UE新GPU运行/截帧/完整重放，不继承旧hash。

构建/样例/GPU串行共享锁，有限超时，语法和diff检查通过。此批最终矩阵无构建/GPU失败；probe通过。原Qt/AS ForceCrash与系统冻结仍未闭环。

产物：build-macos-debug/metal-ray-b526/manifest.json、matrix-manifest.json、negative-matrix-manifest.json、final八形状、final-invalid两形状、regressions/manifest.json及official/gate-results。最终库快照librenderdoc-final.dylib，工具/源码/样例/capture/依赖哈希见各manifest。

backend/bundle SHA256：e231e2bd394f1781d7093ecad2da5318764f11d7c814ad4e9bcea578c3a14706；GUI：3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91。

实际UE还需要Shared动态/大backing/typed producer及完整shader/UB/resourceHeap闭包。下一先生成隔离的小场景（不改用户工程）并有限截帧核验真实Lumen HW query与成本，再由capture补缺。两个生产RT flags仍false；持续任务未完成，无提交推送。

最终严格1734完成日志无断言/overrun/streamseek/资源表或几何快照诊断。
