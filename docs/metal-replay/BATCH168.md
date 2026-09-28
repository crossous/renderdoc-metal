# BATCH168：UInt32 indexed opaque 正向闭环

T167 的 indexed opaque chunk 同时支持 UInt16/UInt32，但先前只用 UInt16 做了
GPU 正向实证。本批用 UInt32 索引 `{0,1,2}` 独立录制 T168：原生 Metal Validation、
注入捕获、API/CLI 回放均命中，fragment Shared 值 1、中央绿色。两种索引宽度现在
均有正向 capture 和各 12 个畸形输入拒绝测试；未扩展其它 AS 描述符子集。

24 份跨族哨兵 API/CLI 通过，T35/T136/T165/T167/T168 共 50 次生命周期打开
通过，resident growth 983,040 bytes。库/app 内嵌库 SHA-256 `9a7f700ace5c…`
（驱动未变），T168 capture SHA-256 `fdba55218a1d…`。当前仍为 55 处 bridge
调用（另有宏定义 1）/18 处未处理 chunk 宏匹配（含定义 1），GUI 待验 130 份。
未启动 qrenderdoc/Computer Use；06:26 IOGPUFamily panic 后完整 GPU 压力回归
仍暂停，不能以本批定向通过宣称已排除 panic 根因。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t138 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165 t166 t167 t168
python3 util/test/metal/metal_indexed_opaque_triangle_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t168_capture.rdc
```

完整累计回归入口已纳入 T168，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=168 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
