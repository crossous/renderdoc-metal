# BATCH70：GPU 写入 ICB 间接执行范围的安全边界

原先 `executeCommandsInBuffer:indirectBuffer:indirectBufferOffset:` 在 ObjC bridge
触发 `METAL_NOT_HOOKED` 致命中断，旧 chunk 也无处理分支。现在该调用可按原应用语义
提交给 Metal，并序列化 ICB、range buffer 和 offset；离线 replay 在专属 chunk
明确失败，不用编码时 CPU 范围伪造事件树。原始标记由 **96/61 降至 95/60**，
但这是**安全降级，不是功能接通**：正确 GPU 执行点范围、子事件、seek 仍缺失，
等效待解决范围仍为 **96/61**。实施门槛和剩余技术问题见 [PLAN.md](PLAN.md)。

T70 复用 T53 fixture 的可选模式：同一 command buffer 内，blit 更新 ICB，
compute 将 `(0,6)` 写入初始为哨兵的 range buffer，随后 render encoder 用该
GPU 范围执行六个 ICB slot。测试入口：

```sh
bash util/buildscripts/scripts/test_metal_capture_batch70_macos.sh
```

该脚本在 Metal Validation 下跑原生和捕获各3帧，检查像素、唯一的 id1185 chunk、
非空 range buffer 身份及 offset0，并确认 CLI replay 非零退出且定位到
`executeCommandsInBuffer (indirect range)`。T70 capture **不可用于 UI 打开/QA**，
待真正回放接通后才加入集中 GUI 清单。T53/T67 旧 capture 定向 API/CLI 回放通过。

本批不改 chunk 编号或已有 capture 格式；常规 T01–T69 + T10 marker 的全量
终端回归通过：**70份 capture API/CLI、1917个畸形输入、700次 lifecycle 打开**，
resident growth 1802240 bytes，日志 `/tmp/metal-batch70-full.log`。T70 单独作为
预期失败负例，**不计入**70份成功回放。`git diff --check` 通过；库与 app 内嵌库
SHA-256 均为 `6ba534c557b2a613a86431f06703dc55ea3e834bbc9687ae5bb23ca4343e215c`；
T70 capture 为 `2900e5512245598a5feccbc1792561154fab0945847a6a7e0884a92a9855168d`。
未运行 Computer Use；未提交/推送。
