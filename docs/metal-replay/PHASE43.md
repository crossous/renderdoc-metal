# PHASE43：T42 Compute 资源声明、barrier 与 marker

状态：实现及定向终端验证通过；GUI L4 未执行，阶段开放。最终联合结果见 BATCH43-44。

接通七个此前隐式转发的 compute 入口：single/batch `useResource(s)`、scope/resource
两种 `memoryBarrier`、push/signpost/pop 三条 debug marker。新 chunk1255–1261 追加，
Max1262；旧 capture 编号不变。声明/资源 barrier 保留 frame 引用，含只被声明的资源。
marker 保存为 structured API 事件，不在部分 replay 中重建原生 debug 栈，不新增嵌套 action。

当前支持直接包装的 buffer/texture，usage 为 Read/Write/Sample，compute scope 为
Buffers/Textures。空声明数组保留 chunk、无 native 调用。resource registry 检查拒绝空、
未解析及类型不匹配的 ID；不宣称支持 heap、ICB、acceleration structure 或 argument-buffer
内间接资源图。barrier 是真实 native 命令；声明不是 GPU 实际访问，不伪造读写 action。

T42 使用 `Metal_Resource_Barriers` 默认模式：Concurrent encoder 的三次 dispatch，
同一272-byte buffer 第 i 个 uint 依次为 i+1、3i+10、3i+21，两类 barrier 分隔依赖。
44-byte 独立 buffer 全0x6d，11×9 独立纹理 RGBA34/68/102/255，仅声明、不参与 draw/copy。
最终 backbuffer 为 RGBA21/117/222/255。

必跑：带 Metal API Validation 的 native 5帧、注入8帧 capture、XML ID/参数/marker、
三 dispatch 的完整数据/descriptor/usage、1→2→3→1→3→2 seek、最终像素、3-loop CLI，
35类畸形 capture 无信号退出拒绝。replay 本身也开启验证层。
新资源保留逻辑影响共享路径，必跑旧T12声明、T40 compute 参数；批末 T01–T43+T10_debug
联合 API/CLI 与10轮 lifecycle，列表去重见共享脚本。UI 只登记到 QA_CONSOLIDATED。
