# B405：保持原PSO的零附件UAV pass

真实eb9386b7的12次zero-attachment render indirect在root encoder17705/17713，原pass尺寸320x240、sample1，原PSO rasterizationEnabled=true且fragment存在；不是rasterization disabled或depth-only。

微型Native MTL_DEBUG_LAYER fixture已验证原配置：2x2、all color/depth/stencil invalid、rasterization enabled、fragment存在，vertex实际UAV atomic计数9/sum27、sentinels正确、原Private indirect3/6后source归零。Legacy capture/replay七次reset/seek取得first3/sum6与final9/sum27、无targets且fragment存在。

coverage59候选只允许explicit nonzero <=512尺寸、原样sample1/rasterization enabled/vertex+fragment存在的attachmentless sourced indirect；原PSO所有color/depth/stencil格式必须与空附件一致。复用v58 per-use参数、完整pass writes/Native或future physical footprint、buffer snapshots/typed descriptor source closure与预算。不改原PSO/原draw，不假设Nested UVA只读，requires至少一个完整声明的write resource。

sourced fixture使用top-level readonly root指向24-byte typed descriptor，descriptor再指向实际Writable Counters，Native直接输出与capture relocation必须都保持9/27；测试shader的top-level readonly与实际UAV写区分由useResource Write完整声明完成，符合UE现有路径。当前sourced capture/replay验证进行中，未验证实际UE/UI，目标active，无提交或推送。

2026-10-02 最终2bcf940376fec6f76e210eea562bbae00a2c82741161af476f6caad656b2f980，0PzKr3定向6captures/42reset-seeks/252API+CLI反例通过，2x2/320x240/512x512及parallel/unretained均真实UAV9/27、首draw3/6、sentinels正确、Native PSO/F存在、source末尾0。显式sample1和array1必需。空参数fragment reflection是已查询零参数集合，AddShaderBindings不再把nil数组视为未知；原生reflection对象必须存在，保持原fragment/PSO。
同最终库交叉B404 22/88/404，B403 8/56/248，B402与B401全部通过。组合308旧夹具精确全量进行中，不替真实UE覆盖授权。
最新真实UE eb9386b7 CPU Native budget审计：34heap2357460992B，独立buffer Native23068672B，保留initial1282587160B，资源+initial3663116824B；heap-child buffer664615760B是共享heap而非额外Native分配。前文38heap/2625896448B属于旧12230757样本，不用于最新预算。largestInitial134217728B，texture row-pitch staging与descriptor shadows仍需计量。查询不创建heap/resource/queue，GPU提交0。

最终2bcf9403组合全量308captures/7786malformed/3080lifecycle通过，resident growth9306112B，start/end hash不变。log：build-macos-debug/local-m2-descriptor-replay/attachmentless-combined-regression.log。已补计最新UE的8drawable独立纹理：Native2393702400B、initial1282587160B、最大texture staging75759616B、snapshots10776825B；CPU保守含2份initial+192MiB余量共5170980137B。实际UE/UI仍未验收，继续allocation candidate。
