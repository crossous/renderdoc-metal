# B382：实际UE描述符表容量与注解范围

v41沿用D3D12逻辑descriptor shadow、Metal显式ResourceId/source binding、逐Native VA/GPU ID重编码。
表容量与Native分配器rounded容量分离；背景typed table最大32MiB，逻辑count仍全局786432上限，
其他frame buffer/heap/aggregate/dispatch/draw边界不变。ReadSlot和PrepareShadow使用相同表容量界限；
每个未声明槽仍必须GPU身份字段为零。sampler schema2仅首字段为GPU ID，bias/metadata属于普通数据。

最终库4322af8c513e11fa373227b3fe5afc855ac5f2e7b33c785c31cf7b86e2504392，
metal-large-table.G9hgDZ：Native与2 captures全部通过；786432 resource slots(18MiB)、
4096 sampler slots(98304B)，单线程读尾部typed buffer/texture/sampler，GPU122→161，
释放/重分配同槽，16 dispatch seeks+8 EID0恢复，检查全部空槽普通metadata与sampler bias保存。
21 API+CLI negatives通过，涵盖旧v40、表容量/stride/range/schema、unknown非零GPU字段、
live raw-byte mismatch、slot overlap/generation/source、inline与dispatch。所有异常无replay wait。
没有扩大GPU工作量，没有用原始VA猜源，没有实际UE整帧GPU提交。

UE重新截帧0b2dbec924c83099354a3a72f7d5edcb1866833df9c56c117fa1cac0600a2b27，
111761594B；原bfa3/b2b截帧保留。provider b89d1b…按HeapSize/24而非Native1024B/24声明，
临时堆从42缩到10槽，但UE FlushPendingDescriptorUpdates的256B堆只分配三个handle，
其余7槽仍是未初始化数据。CPU统计四个unknown非零，不放宽unknown检查。
新HeapTableCountScope(3)仅包围这段精确三handle初始化，并保留256B实际分配/allocator行为；
注解声明前三个槽，scope退出恢复。准备脚本严格匹配UE5.8.3源码；原引擎不修改。
隔离模块7b160b2f6fe10ad542325c767dc0ce0ae36ebdb042b498e9206421f7e78bac35，
98/98导出、原库ace97360…不变，重新截帧进行中。

0b2dbec9 CPU当前帧lifetime/value/inline/producer问题0，frame-start1925 live全初始字节一致。
历史4758 value epoch缺失源身份为帧前已退役历史，不能当全历史无问题；主表1801 live、
526 known-freed nonzero须时序验证、784105 unknown零；sampler109 live、3987 unknown零。
658151256B texture initials/493 records，其他资源与别名继续审计。

精确4322af8全量尚未运行，最近958d组合308/7786/3080不等同于此库。人工UI锁屏未验。
持续目标active；没有提交或推送。

三槽注解重新截帧763817d815be83d95a4bbc46b26977e11cebc80bddbfb7f802373c2a020a1986，
106715099B，provider7b160b…/capture库4322af8…；临时heap16833前三槽live且全部raw-byte一致，
主表1677live/665known-freed/784090unknown零，sampler109live/3987unknown零，
所有typed tables unknown非零0，68producer expected匹配。
当前帧lifetime/value/inline/producer issues0；4875历史value epochs缺失已退役源身份保留限制。
439textureInitial/589902470B、effective842sources均有initial或精确Free；10prefix frees。
Native query-only1704placements/range issues0/112历史overlaps；typed backing三对
16833→17389、16833→17437、17389→17437全部逻辑retired、prior committed、no later refs。
commit不等于GPU completion，此CPU审计不授权整帧提交。4322af8精确全量进行中。

4322af8c…精确全量已完成：308 captures/7786 malformed cases/3080 lifecycle opens通过，resident growth11632640B；日志large-table-combined-regression.log、hash记录large-table-combined-regression.sha256。随后才构建v42候选，GPU串行。
