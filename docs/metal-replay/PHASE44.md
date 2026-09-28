# PHASE44：T43 Render 分阶段资源声明与 barrier

状态：实现及定向终端验证通过；GUI L4 未执行，阶段开放。最终联合结果见 BATCH43-44。

接通五个现有 bridge/chunk 缺口：带 stages 的单资源声明、普通/staged 批量声明，
scope/resource 两种 barrier。支持 Vertex/Fragment stage 及 RenderTargets/Buffers/Textures
scope；Tile/Object/Mesh 等尚无 replay 的 stage 明确拒绝。buffer/texture 身份及 usage 校验
与 compute 共用，旧单资源 render 声明也补上同样校验，保留其原有序列化布局。

T43 为 `Metal_Resource_Barriers` 设置 `RENDERDOC_METAL_RENDER_BARRIERS=1`：
前两次 draw 关闭 rasterization，vertex shader 向112-byte buffer 写入 uint32(20,40,60)，
再各加7；scope barrier 分隔 VS→VS，resource barrier 分隔 VS→FS，第三次 draw 读取最终
数据输出 RGBA27/47/67/255。其余 buffer padding 保持0。52-byte 独立声明 buffer 全0x6d，
12×9 独立声明纹理 RGBA34/68/102/255，须被保留且在事件回退后不丢内容。

关联修复：

- 原生 `newRenderPipelineState` 失败时返回NULL，不包装空对象；native/注入两路径都由
  故意无效 pipeline 验证NULL和NSError，XML不得出现失败创建chunk。
- vertex RW buffer 正确进入 storage bindings、ReadWriteBuffer descriptor 和
  VS_RWResource usage，不误作 IA 输入。pipeline 切换后不残留旧 RW descriptor。
- Pipeline Viewer 现有 VS Storage Buffers 表接收 RW buffer；仅完成构建与 API 数据验证，
  行展示、链接和过滤仍待用户 GUI QA。

必跑：Metal验证层 native5帧/capture8帧/XML/3-loop CLI，三 draw 完整buffer与padding、
descriptor/usage、前后seek及最终像素，68类畸形capture无crash拒绝；replay验证层。
共享绑定路径必跑旧T19 VS只读存储、T34 inline/batch；资源声明必跑T12。批末 T01–T43+
T10_debug API/CLI、10轮lifecycle，顺带回归旧T21 indirect与通用缩略图。
测试识别改用实际 API signature，不再只按52-byte buffer或任意setVertexBytes猜fixture；
通用缩略图选择真正的最后draw，避免把 raster-disabled 的第一个draw误当最终图像。

不宣称支持 vertex RW texture、fragment RW buffer 完整UI或高级stage。UI增量与所有旧待验
项合并到 QA_CONSOLIDATED，不自动关闭本阶段或此前批次。
