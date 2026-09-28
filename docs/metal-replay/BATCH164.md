# BATCH164：带默认descriptor的AS encoder创建

接通`MTLCommandBuffer::accelerationStructureCommandEncoderWithDescriptor` bridge，
新增独立chunk并只接受没有counter sample attachment的默认
`MTLAccelerationStructurePassDescriptor`。T164在两个command buffer中均使用该重载
分别构建BLAS/TLAS；原生Metal Validation及注入捕获、API/CLI回放的compute射线
结果为`0,1`，事件回退为`0,9`，说明并非只重放了encoder创建调用。
回放校验command buffer类型、当前编码状态、encoder ID和descriptor attachment
标志；两次创建各5个畸形输入（含错误command buffer/encoder ID）均干净拒绝。

19份跨族哨兵API/CLI通过；T35/T142/T163/T164共40次生命周期打开通过，
resident growth 1,064,960 bytes。此入口原本是bridge标记，但**并非原有未处理
chunk case**：本批新添一个chunk，原有未处理chunk数不变。原始bridge 55→54，
原始`METAL_CHUNK_NOT_HANDLED`宏匹配仍18处（含定义1处），`supportsRaytracing`
仍false。06:26 kernel panic后完整GPU压力回归仍未执行，GUI/Computer Use未运行。
该批库/app内嵌SHA `0ace9b0c4581…`，T164 capture SHA `581fdf7ca5f9…`；
集中UI待验126份。

定向复验：

```sh
bash util/buildscripts/scripts/test_metal_replay_targeted_macos.sh --sentinel t36 t142 t148 t156 t159 t160 t161 t162 t163 t164
python3 util/test/metal/metal_as_pass_descriptor_invalid.py build-macos-debug/bin/renderdoccmd captures/metal-smoke/t164_capture.rdc
```

完整累计回归入口已纳入T164，但本批未执行：
`RENDERDOC_METAL_LAST_TEST=164 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`。
