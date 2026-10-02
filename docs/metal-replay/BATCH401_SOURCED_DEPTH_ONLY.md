# B401：sourced depth-only / absent fragment

真实UE12230757存在12个零颜色pass，部分有depth，部分完全无附件；Native原始PSO5190/5111/5148验证fragmentFunction不存在。仅开放带有效深度附件的零颜色pass，完全无附件UAV路径仍不受支持。

v56复用现有ValidateGraphicsTargets/深度clear与load provenance、Native PSO和部分回放ResolveDeferredStoreActions。fragment stage仅在depth-only、Native原始PSO fragment不存在、vertex存在且fragment bindings/bytes均空时跳过反射要求；有fragment的depth-only继续执行原有反射检查。

极小MRT夹具增加先行depth-only draw，实际descriptor pointer vertex、无fragment、所有颜色格式Invalid。既检查独立depth-only seek的depth/stencil像素，又验证后续两pass的MRT像素和descriptor relocation。

缺失vertex函数负例发现旧无options PSO创建路径Native断言；已复用options路径的函数类型/有效对象检查。无需修改实际Metal pipeline创建行为。

2026-10-02精确库e8f991a23edd3ad8af9fce372872c97a694a0df4f201eca15dd3d847aeca69f7：
`metal-depth-only.154ozH`，7组合/14 captures/56 reset-seek cycles及56次depth-only seek、80 API+CLI负例组全通过。D16/D32/D32S8、background/frame Private heap、serial/parallel、deferred stores覆盖。

全量回归待与后续sourced compute indirect合并执行。人工UI未验收，实际UE全帧尚未开启，持续目标active，无提交推送。

2026-10-02 后续v57精确26a25b7f…组合全量包含以上实现：308 captures/7786 malformed/3080 lifecycle passed，growth7520256B；结束库hash一致。日志sourced-indirect-depth-only-combined-regression.log。人工UI与真实UE整帧尚未验收，目标active。
