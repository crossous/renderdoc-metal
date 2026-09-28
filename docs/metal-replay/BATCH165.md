# BATCH165：显式opaque三角形AS几何

原来的三角形AS build wrapper仅接受描述符默认`opaque=false`，显式
`triangle.opaque=true`会在bridge被拒绝。T165扩展单个无索引三角形build，新增
`buildOpaqueTriangle` chunk，并在回放按chunk重建`opaque=true`描述符；旧默认/
非opaque/refit chunk保留原有语义。原生Metal Validation下，同一个自定义
`reject_triangle`函数对T148非opaque几何返回未命中0，对T165显式opaque几何
返回命中1，中央由红转绿。注入捕获和API/CLI回放复现Shared值、颜色与事件seek。

T165的11个畸形AS build输入干净拒绝；20份跨族哨兵API/CLI通过，
T35/T148/T156/T164/T165共50次生命周期打开通过，resident growth
1,556,480 bytes。原始bridge宏匹配仍54、旧未处理chunk宏匹配仍18（含定义1），
因为本批扩展了既有AS bridge的支持子集而非删除整个拒绝分支；新增chunk不算
旧chunk消除。完整GPU压力回归在06:26 kernel panic后仍暂停，未使用GUI/Computer Use。
库/app内嵌SHA `19ae3d8f9332…`，T165 capture SHA `ae98acfea13e…`；
集中UI待验127份。`supportsRaytracing`仍false。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t142 t148 t156 t159 t160 t161 t162 t163 t164 t165
python3 util/test/metal/metal_opaque_triangle_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t165_capture.rdc
```

完整累计回归入口已纳入T165，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=165 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
