# 阶段 19 细化计划：T18 fragment storage buffer 直接绑定

T17 texture/sampler 批量绑定已关闭。下一步补 M4.7 的 fragment storage buffer：在受控
source-created MSL 中让 fragment shader 读取结构化 buffer，并通过标准 Pipeline/Buffer Viewer
核对真实字节、物理 slot、reflection、usage 和事件状态。

当前状态：P19.1-P19.4 已关闭；T00-T18 L3 与最新 qrenderdoc L4 通过。下一项见 `PHASE20.md`。

## 本阶段证据（2026-09-24）

- P19.1：新增 `Metal_Fragment_Storage_Buffer`。640-byte shared buffer 在 offset 256 存放四组
  `float4`，前后有品红哨兵；fragment `device const float4 *` 绑定物理 slot 3，另声明未绑定
  slot 5。原生四象限 BGRA 像素与 CPU 参考一致。
- P19.2：capture XML 记录 `setFragmentBuffer` 的 slot 3、offset 256 和 640-byte 初始数据。
  Metal 反射对此指针报告 `Float4`，结构体常量/argument buffer 报告 `Struct`；现按数据类型
  将非结构体 buffer 纳入只读 storage resource。通用 descriptor 为 `Buffer`、范围 256+384，
  draw usage 为 `PS_Resource`，不进入 constant block。clear/draw/回退、原始字节、哨兵和越界读取
  均由自动 smoke 核对；T04/T12 定向通过。T10 的 `constant uchar4 *` 同样按 storage resource
  记录使用，旧 usage 断言已更新并定向通过。
- P19.3：FS 页面增加 Storage Buffers 表，通用资源 descriptor 和标准 Buffer Viewer 跳转使用
  同一 buffer ID、slot 3、offset 256 与 384-byte 可见范围；HTML export 包含该表。自动 smoke
  核对未绑定 slot 5 不产生 descriptor，raw export 为 640 bytes。
- P19.4：`/tmp/t18-final-regression.log` 中 T00-T18 全量回归、CLI replay 与 19×10 lifecycle
  全部通过（最新 resident growth 1556480 bytes）；`git diff --check` 通过。首次全量揭示 T10
  usage 断言过窄，定向修复后全量通过。最新 qrenderdoc 加载了与正式 capture SHA-256 相同的
  `/tmp/t18-ui-final.rdc`，核对 EID 2 的 FS Storage Buffers slot 3/offset 256/size 384、
  标准 Buffer Viewer 四组值、Resource Inspector 的 `FS - Resource`、HTML 与 UI CSV 保存和
  `No problems detected`。UI CSV 前四行与自动 raw export 的 640-byte 文件逐字节对应；
  产物为 `captures/metal-smoke/t18_pipeline_state_standard.html`、`t18_storage_ui.csv` 和
  `t18_storage.bin`。UI 二进制菜单项未单独点击；raw 字节及保存内容由自动 smoke 覆盖。

## P19.1：fixture 与 native 语义

- 建立独立 T18，用非零 fragment buffer slot 读取确定性颜色或变换参数，保留越界哨兵。
- 固定原生像素、buffer 字节、layout、slot 和 clear/draw 边界；确保错误 offset/slot 可被像素识别。

验收：未注入 native readback 与 CPU 参考一致。

## P19.2：capture/replay/state

- 复用已有 `setFragmentBuffer`/offset 路径，补齐 storage buffer 的类型、范围、reflection/descriptor、
  `PS_Resource` usage 与事件 snapshot；不得把它误报为 constant buffer。
- 定向核对 XML、raw bytes、clear/draw/回退，以及空资源/越界 offset 的明确失败。

验收：T18 capture/replay 和原生输出一致；T04 constant buffer 与 T12 argument buffer 语义保持正确。

## P19.3：标准 Viewer

- 在 FS Pipeline、通用 descriptor/reflection、标准 Buffer Viewer 和 Resource Inspector 中核对
  storage buffer；验证资源跳转、used/unused 与 HTML/raw export。

验收：自动 smoke 与最新 qrenderdoc 对同一 EID、buffer 范围和输出一致。

## P19.4：阶段收口

- 接手只读 README/STATUS/PHASE19 与 HANDOFF 当前检查点；PLAN 按需查阅，历史阶段文档按需追查。
- 开发中只跑 T18 与受影响的 T04/T12 定向验证；阶段末一次 T00-T18 全量回归和一次最新
  qrenderdoc 实机验收。若公共路径修复影响全量结果，修复后再重跑一次。
- 同步阶段文档，下一任务提示保持短小并引用 STATUS/PHASE 文档，不复制历史证据。

验收：全量回归通过，qrenderdoc 状态栏为 `No problems detected`。
