# PHASE51：异步 Library / Pipeline 创建

T51 / `Metal_Async_Creation`。终端自动通过，GUI L4 待验，阶段开放。
接通六个 bridge；追加六个 chunk 1264–1269，Max 从 1264 变为 1270。

## 实现与边界

- 六入口：source library；render descriptor 的普通/options 两种；compute function 的
  普通/options 两种；compute descriptor + options，均为 completionHandler 重载。
- 保留原生异步 API 和回调线程，不改为同步编译。成功结果先包装并登记资源/父依赖，再
  调用应用 block；error/reflection 原样传回，失败 nil 结果不创建资源。离线只重建成功的
  资源，不序列化 block/reflection/error 对象，不运行应用回调或模拟回调时序。
- source 和 descriptor 在调用时复制。Native descriptor 使用真实 function，capture
  snapshot 保留包装 function；应用随后修改 descriptor 或释放 library/function 不改变快照。
- completion 参数是借用对象：包装层额外保留 native 引用，回调返回后释放临时 proxy
  引用，应用仍须按原生规则 retain 自己要留下的结果。PSO 与 Event 的 proxy 改为独立
  拥有 native 引用，沿用已有 Library/Function 策略；不是所有 wrapper 生命周期的通用修复。
- **异步 source library 仅支持 options=nil 的离线重建**。非 nil compile options 原生
  仍正常传递，但 capture 标为 unsupported 并在 replay 明确拒绝，不悄悄用默认选项编译。
  Pipeline descriptor 的支持范围与 PHASE47 相同，binary archives/preloaded libraries
  等未支持特性拒绝。未覆盖动态库、函数拼接或任意异步资源类型。
- 共享 source serializer 加入 device 身份/类型及 library 非零唯一性校验；补上
  WrappedMTLDevice 的 eResDevice 初始化。旧 source chunk 格式不变。

## Fixture / 自动断言

原生验证层与注入 capture 各 12 帧：七个 completion 各调用一次（六成功、一非法 MSL
失败），检查包装 device、错误与反射。三 compute pipeline 和两 render pipeline 均实际使用。
提交异步请求后修改 render format 与 compute maxThreads，并提前释放 library/functions，
创建后排空 autorelease pool；之后绘制/dispatch 及显式释放 PSO 仍通过。

XML 六个新 chunk 各一条，options 为 0/3，snapshot 为 BGRA8 和 maxThreads=64；失败
library 无资源记录。API 验证 1 library / 3 shaders / 5 PSOs、3 dispatch / 2 draw，428-byte
output 的三段各 32 uint 起值 31/83/127、尾 44 bytes 全零；offset 为 0/128/256。
首/末 dispatch 回退、反射/binding/usage、先左半后全屏与最终 RGBA31/83/127/255 均通过。

104 类异常覆盖资源 ID/类型/重复、非法 MSL、unsupported options、function stage、
pipeline options/descriptor 字段和 dispatch threads；每例 30 秒超时，必须干净非零退出。
另两种合法 options=1/2 变体各 3-loop replay 通过。没有把失败探索算作通过结果。

## 联合回归与 UI

共享 pipeline/source/proxy 所有权影响旧场景：T01 triangle、T12 argument buffer、T47
pipeline variants 必跑；三者还额外重新录制兼容 capture。与 PHASE52 共用最终全量
T01–T52 + T10 marker 的 53 份现有 capture，不重录旧正式文件；结果见 BATCH51-52。
后续改公共资源/序列化/部分 replay 时继续跑共享联合脚本，不只测新样例。

GUI 只查初始化记录、pipeline/shader 资源链接、逐事件 buffer/左右半屏；无专用 async
或 reflection 面板要求，不要求离线触发回调。最小步骤已合入 QA_CONSOLIDATED。
