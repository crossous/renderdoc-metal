# BATCH135：静态三角形加速结构 build 与压缩尺寸回读

## 已接通范围

- `MTLDevice::newAccelerationStructureWithSize:`：独立包装资源、创建 chunk、Replay
  资源类型和所有权。离线重复 replay/seek 时重建真实对象并保留同一资源 ID。
- `MTLCommandBuffer::accelerationStructureCommandEncoder` 及 encoder 的
  `buildAccelerationStructure:descriptor:scratchBuffer:scratchBufferOffset:`、
  `writeCompactedAccelerationStructureSize:toBuffer:offset:sizeDataType:`（也支持默认
  UInt 重载）、`endEncoding`：真实 GPU build、资源父子记录、事件和回放命令。
- 描述符当前**只支持一个无索引、无 motion、默认 usage/几何选项、packed Float3、
  stride12、零 vertex/scratch offset 的静态三角形 geometry**。容量、buffer类型、
  数量、输出写回范围和类型均在回放前验证；其余 AS selector 不静默透传。

T135 的 36-byte 顶点 buffer 生成一个 AS，原生与注入运行均经 Metal API Validation，
每帧 AS 分配1536字节，GPU回写的 compacted size 为1280。捕获后 API/CLI 回放，
API 检查 AS 资源类型、五段事件/资源 ID、write→build→write seek 后的8-byte GPU
读回为1280→0→1280；18个畸形目标通过非崩溃拒绝。原生/捕获/定向重跑入口：

```sh
bash util/buildscripts/scripts/test_metal_as_build_macos.sh
```

旧 AS 命令 encoder chunk 已处理，新增创建/build/write/end 四个 chunk。最新库与
app内嵌库 SHA 均为 `55f87e6705d0…`，T135 capture为 `dff173e98bae…`。全量
134份成功capture的API/CLI、2745个畸形用例、1340次生命周期打开均通过；resident
growth 5,554,176 bytes，日志 `/tmp/metal-batch135-final.log`。T70/T133仍按预期
明确拒绝。旧捕获 schema 版本未改，chunk 编号只从尾部追加。

## 明确未支持

`supportsRaytracing` 仍为 false：尚缺 `newAccelerationStructureWithDescriptor:`、
instance/top-level AS、索引/box/motion build、refit/copy/compact、AS 的 shader绑定和
intersection function table。T135 仅证明 bottom-level 单三角形 build 与 GPU 尺寸写回，
不声称完整 ray tracing。AS 分配和 build scratch 大小目前在回放设备重新校验；捕获
设备与回放设备所需大小不一致时安全拒绝，尚未做跨 GPU 可移植承诺。T133 heap
同偏移别名复用和 T70 GPU 生成 ICB range 仍各自保持预期拒绝。

## 后续集中 UI QA（未执行）

打开 `t135_capture.rdc`，核对 AS 资源类型和1536字节分配、Event Browser 的 begin →
build → write compacted size → end 顺序，以及 API Inspector 的同一 AS/vertex/scratch/
output buffer ID。切到 write、回退 build、再回 write：Shared 输出 buffer 的8字节
应为十进制1280（小端 `00 05 00 00 00 00 00 00`）；build处为零。AS 内部结构暂不要求可视化；
只核对事件、资源链接与回读。最终普通帧画面沿用 T39 的验证项。
