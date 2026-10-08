# B524：inline query 的 Private 帧内 BLAS 几何

完成。参照已有DX12/Vulkan的typed AS构建输入、当前资源依赖和捕获后重定位，复用B511/B512的Private encoder-end冻结与kind1/2/8配方。只撤销query对「child必须仍为初态配方」的限制；范围、顶点/索引/finite、整个多几何buffer、顺序及队列约束仍由原验证器保障。TLAS本批仍要求存在初态，roots/header/output仍Shared静态。RT保持黑盒，两生产能力false。

## 实际验收

八份native→capture→API/CLI3，192次事件选择/前后往返/EID0：triangles、indexed、两几何均root0/8；64几何root0；64几何+64实例root8。初始z0→GPU上传Private z0.25，BLAS scratchOffset256、几何构建后清零，再TLAS UserID74。CommittedGeometryIndex*13及InstanceID*17参与真实结果：单几何2508，两几何2521，64几何3327，64几何+64实例4398。Float4/Float3、UInt16/UInt32、原offset/stride及几何顺序均保留；只有最后几何/实例在命中位置。ForceOpaque query无需SBT/VFT/IFT。

最终独立oracle重跑八份API，检查两输出、前后不同roots VA及保护字、共享header重定位、分配干扰后的不同AS-ID、子AS usage。实例source在所有选择含EID0为零；几何source仅在第二query之后断言为零，不承诺未使用资源在初始事件的内容。multi未引用NaN允许；引用NaN拒绝。

四形状反例：114/121/132/132组，共499组、998次API/CLI拒绝，无GPU wait。十个合法控制：四形状的UserID75和coherent child GPU-ID重编号共八个，加两种多几何逆序，全部API/CLI及24事件通过。逆序CommittedGeometryIndex0，移除13*(count-1)贡献。真正合法65-instance typed间接TLAS native/capture通过（输出3329），API4/CLI1在GPU前按64上限拒绝；未声明multi64 native/capture通过但重放拒绝。

当前库旧query16份、B512旧TraceRay四份API/CLI3通过；固定官方Apple MIT样例两scene、270事件/EID0、10查询及CLI3通过，六能力检查通过且两flagsfalse。旧capture回归没有重新native。3358个完成日志严格扫描无断言、overrun、非法streamseek、未知Metal类型、资源表未清理或缺几何快照诊断。

## 失败和限制

首轮multi64 native/capture成功，API4在预检拒绝：读回buffer使用newBufferWithLength，缺权威初态，CPU提交快照不能证明它的来源。测试改为newBufferWithBytes显式零初态，verified-multi64及最终八场景完整重验通过；生产immutable guard未放宽。原失败保留。

当前hash未跑旧50texture/view/heap、完整78IR/75RT/308、旧84间接反例、32CPU/AIR、Qt/ARC、官方13损坏sample。未重新UE运行/截帧/GPU重放；原UE预算、Private/GPU header/producer仍缺。原Qt/AS ForceCrash、系统冻结根因未闭环；普通同CB BLAS→TLAS及AS标签仍有限制。下一B525新TLAS query目标，再实际UE typed header闭包与低工作集验收。

## 产物与哈希

根目录：`build-macos-debug/metal-ray-b524/`。最终manifest.json、matrix-manifest.json、negative-matrix-manifest.json、regressions/manifest.json；final八场景，final-invalid四形状，valid65-control与untyped-control；multi64保留首轮失败。最终oracle为regressions/query-replay。最终库快照librenderdoc-final.dylib。

backend及bundle：`4d350b8979d90c065e3f67e1e2170ba2f5a8a562b442175f081ce016b2aaf004`；GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。最终oracle：`38df66556c50728759d50b13ef86e3f10b5d70afaaec5d68bc344dd7b509d28e`。源码/编译sample/工具/依赖/capture哈希见各manifest；固定官方来源、许可和archive哈希见regressions/official/gate-results/manifest.json。构建/GPU全部共享互斥锁串行，未提交推送。
