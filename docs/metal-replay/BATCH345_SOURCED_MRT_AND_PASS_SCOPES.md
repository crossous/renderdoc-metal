# BATCH345：描述符重定位后的 MRT 与跨 pass 输出

2026-10-01，持续目标 active，未提交或推送。

v9 受限图形预检扩展到两个 2×2 单采样 RGBA/BGRA 目标。按渲染 pipeline 的
每个 color attachment pixelFormat 核对 pass 目标，拒绝缺失/额外/格式不符的
附件与重复资源；限制 array length / raster sample count / target width/height，
仍拒绝 depth、resolve、tile、rate-map、counter、vertex fetch 和大型绘制。

小用例：计算 shader 更新 Texture4 描述符，第一图形 pass 输出 drawable 与
独立 RGBA intermediate，两目标均为 red=186/122；第二 pass 经明确来源的
Texture4 descriptor 采样 intermediate，输出 drawable。两个实际 draw、三个
compute pass 的 BeginPass/EndPass 配对，action.outputs 和 MetalPipe.colorTargets
正确，第一 pass 两目标、第二 pass 单目标；inline storage/fragment size 为 48。

native、两捕获、各四轮 compute/draw/EID0 跳转、所有 MRT 与最终像素通过。
16 组 API+CLI 附件/pipeline/bounds 反例及 27 组图形顺序/阶段反例通过。
描述符整套十四类、284 组 API+CLI 反例通过，日志 metal-descriptors.f8X6yX，
库 c55700493fb2a0532149f254c3aa1aab8766d6ce218b1f288ad6283bd5adde65。
脚本 test_metal_descriptor_relocation_macos.sh，fixture metal_descriptor_mrt_*。

这些是小输入验证，不代表完整 UE MRT/pass scope 已完成；B344 真实更新 shader
链路另已通过。全量未跑，新增 UI 未验；继续多提交/CPU 完成等待/资源复用。
