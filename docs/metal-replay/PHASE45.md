# PHASE45：T44 跨 encoder Fence 同步

状态：实现、native/capture/XML/API/CLI及最终联合回归通过；GUI L4待验，阶段开放。

实现MTLFence真实包装、Objective-C bridge、资源ID序列化、Sync资源登记、引用和释放；
接通newFence以及Blit/Compute/Render的update/wait共七个入口。Compute新增chunk1262/1263，
Max1264；其余使用原chunk ID，MetalResourceType仅尾部追加eResFence，不重编号旧capture。

Replay执行真实GPU fence调用，校验对象类型、active encoder身份和render stages；等待
必须有本次replay中另一个encoder的先行update。epoch在完整/WithoutDraw回放开始更新，
OnlyDraw配对阶段保留，防止seek沿用上次回放的更新状态。空/未知/类型错误/已结束encoder、
未更新/未来fence、同encoder依赖明确拒绝，避免提交无效GPU等待。

T44使用 `Metal_Fence_Present` 默认模式，两个Untracked shared buffers（304/308 bytes）：

1. Blit清零两buffer并更新fence0；Compute等待后写data[i]=i+3，更新fence1。
2. Render等待Vertex阶段，关闭rasterization，VS将data前三项各加7；更新fence2。
3. Blit等待fence2，复制304 bytes到readback，更新fence3。
4. Compute等待fence3，将readback前76项各加5；复用fence0做更新。
5. 最终Render在Fragment阶段等待fence0并读取前三项，输出RGBA15/16/17/255。

必跑：native Metal验证层5帧、capture8帧、XML四个fence/十条update-wait/stage与复用关系、
3-loop CLI（带验证层）、完整buffer/padding、两dispatch/两draw/copy及present前后seek、
精确停在十条fence API事件处的partial replay、资源usage。56类畸形capture无crash/hang拒绝。

新包装类型影响通用序列化/释放，共享epoch与present路径影响旧回放，触发批末联合
T01–T46+T10_debug API/CLI、470次lifecycle；保留原旧场景与异常检查。另定向T00空帧。
范围为当前受控同队列、帧内先行update场景；不包含跨队列、外部/捕获前fence状态、
CPU可等待event/shared event或Tile/Object/Mesh stages。仅完成终端验证，非GUI验收。
