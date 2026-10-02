# B390：实际 UE 间接命令的剩余工作（CPU分析）

UE763817d8的CPU工作量清单：direct compute84/1237551threads/最大262144；
producer4/6/6/52；普通buffer copies129/2955720B/最大786432B、不触碰声明typed backing；
direct draw277（260 indexed+17nonindexed），total vertex×instance20148、max11904，
Triangle273/TriangleStrip6；另31 indirect dispatch、15 indirect draw、2 indexed-indirect draw。

31/15 indirect参数分布在8个buffer，含同一13724buffer的15个offset。
不能用frame-end一次快照代替每次GPU使用时的内容，也不能用CPU调用commit当完成证明。
现有Metal indirect runtime读取Native Private参数，action tree保留unknown count；
compute RegisterComputeIndirectAction在加载后解析，参考RenderDoc D3D12 ExecuteIndirect/
Vulkan DrawIndirect的Native GPU命令和事后动作数据机制。
当前sourced preflight仍禁止indirect，直接draw只3顶点/2次；本次未提交实际整帧GPU工作。

下步：先低负载heap64重截/完整CPU初始和Nativelayout审计，再普通copy定向。
之后分别扩充初始化index来源、IR draw constants/triangle-strip、批量bounded direct draws，
最后证明GPU生成indirect args的生产→消费和回跳，不以dummy output/旧captured VA绕过source闭包。
将实际UE compiled graphics shader放入独立Native小用例，不能把自制consumer通过当完整UE图像通过。
人工UI仍锁屏待验证。持续目标active；没有提交/推送。

分类修正：旧indexed indirect stringify缺少后缀，不能只按名字统计；最新CPU工具按indirectBuffer字段分类，总48 indirect、9个argument buffers。新heap64截帧212direct draws+48indirect（31compute+17draw），typed backing/cross-kind overlaps均0。
