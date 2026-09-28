# BATCH60：Device独立ArgumentEncoder（纹理/采样器）

承接BATCH58–59的130 bridge / 78旧chunk、Max1278。本批接通
`MTLDevice::newArgumentEncoderWithArguments`的bridge和旧chunk1028；当前**129/77**，
Max1278不变。入口把原生返回值包装为MTLArgumentEncoder，序列化六个公开descriptor字段，
回放重建真实Metal encoder，登记资源及成员类型。严格限定1–16个descriptor、32个成员槽，
只支持只读2D sampled texture与sampler；重叠ID、空数组、越界、其他类型/权限及常量块
明确拒绝。buffer pointer、嵌套argument buffer、可写纹理和任意descriptor布局不在此承诺。

T60 `Metal_Device_Argument_Encoder` 用两个texture/sampler packet实际驱动fragment shader，
原生Metal验证4帧、抓取4帧、一键整轮脚本通过。API按第二→第一→第二draw检查编码成员、
Shader/FS descriptor、资源usage、buffer偏移及左右RGB`20/40/60`和`70/80/90`；
325-byte存储的前缀/后缀哨兵保留。29个畸形RDC身份/descriptor变体安全拒绝。

完整入口`bash util/buildscripts/scripts/test_metal_capture_batch60_macos.sh`，最终日志
`/tmp/metal-batch60-final.log`：最新构建上**61份capture API/CLI、1759个畸形样本、
610次lifecycle打开**通过，resident growth1130496bytes。`bash -n`及`git diff --check`
通过。T60 SHA-256 `847db07d39f505f5aa38df44592d967a961e8512d7703b4b2cc20abad1166c25`；
库/app内嵌库一致为`ad4e54209464459eaf4783890ed31d5e5ba6ac7ae2a33765df34de0d683a736e`；
GUI executable未变，`fe8bcf852b683bc463a3be883e6c54208b7bd45054a24f5dbace58c912346d74`。
GUI/Computer Use未运行，T60加入集中QA，共28份待验。无提交/推送、历史dirty工作树保留。
