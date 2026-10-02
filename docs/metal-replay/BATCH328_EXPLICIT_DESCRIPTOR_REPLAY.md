# BATCH328：显式 descriptor 重定位与 GPU/CPU 混合更新

2026-09-30，本地 M2 Pro / 16 GiB。HEAD c4be68bb7；工作树保留，未提交或推送。
本批把 BATCH327 的原生算法接入真正 RenderDoc replay，但支持范围仍是有完整
应用声明的极小 compute 用例，不代表完整 UE bindless/render 已支持。

## 捕获与回放契约

新增 `metal_descriptor_tables.cpp`，在 1397 身份诊断后追加四个 chunk：1398
表布局、1399 完整覆盖声明、1400 GPU 写入声明、1401 显式 CPU 写入。
旧编号与 section version 16 不变。通过已有 RenderDoc API 1.7 annotation 接入。

- `metal.descriptorTable`：UInt64×4 `{schema, offset, count, stride}`。schema 0 为
  buffer VA，1 为 UE/IR 三字段 resource entry，2 为 sampler entry，3 为
  VA/texture/sampler 三身份测试布局。只接收 Shared、8-byte 对齐、非重叠、
  合法 native 范围；count 上限 786432、stride 上限 4096、总长上限 32 MiB。
  相同声明重复调用幂等，不靠 label、资源号或扫描任意 64-bit 内容推断类型。
- `metal.descriptorCoverage`：device 上 UInt32 scalar，v1 为 CPU 更新只读表；
  v2 明确承诺嵌套表及 GPU 更新的完整布局/来源覆盖。版本不可变；没有完整
  来源证明的真实应用不能为了越过门控而声明。
- `metal.descriptorGPUWrites`：v2 已声明表 buffer 上 UInt32 scalar 1；此类 buffer
  不生成自动 Shared CPU diff，避免把 GPU 输出误录为 CPU 写入。
- `metal.descriptorCPUWrite`：活跃捕获中 UInt64×2 `{start,size}`，仅完整条目，
  将应用明确执行的 CPU 写入保存在执行点。回放只更新该 native 范围，保留
  同一表中其它已被 GPU 更新的条目。

重定位使用捕获 ResourceId 与原生身份：VA 必须唯一落入 buffer range，保持
内部 offset；texture ID 必须唯一；sampler 相同原生 ID 只允许已核对相同状态的
等价对象。零值保持零，普通字段和 padding 原样保留；缺项、冲突、歧义拒绝。
CPU-only 表的部分 byte diff 先合并到捕获字节 shadow，再重编码。每次事件
回跳恢复初始表后重播；不把帧尾表替代帧首，也不把 GPU 结果写回 shadow。

完整 stream 先扫描 metadata，再验证所有表初始值与帧内 CPU 更新。当前拒绝
帧内新资源/身份、render draw、表的 blit/ICB 用途、未支持 dispatch/binding
形式、alias/purge 等路径。失败发生在完整帧提交前；没有删除完成等待或旧守卫。
这些限制仍阻止 UE 真帧，详见 BATCH329。

## 定向结果与命令

```bash
bash /Users/crossous/Developer/renderdoc-metal/util/buildscripts/scripts/test_metal_descriptor_relocation_macos.sh
```

脚本构建 renderdoc/renderdoccmd（-j4），运行两个极小原生用例、各两份注入
capture、真实 GPU 字节/像素/事件回跳和 API+CLI 负例。不会运行完整 UE 帧或
全量回归。最终日志目录 `build-macos-debug/metal-descriptors.o2dyO1`，总日志
`build-macos-debug/local-m2-descriptor-replay/final-descriptor-suite.log`。

- **定向终端通过**：两个独立 capture/replay 进程；replay 先分配 1 MiB padding，
  确认 VA 不同。CPU 更新用例保留 +4/+8 offset、普通常量、哨兵与 texture/
  sampler，GPU 结果 122→161，12 次双向 seek、BGRA 像素全部一致。
- GPU 更新用例按 UE 的 IR entry 形式，由 GPU copy 更新条目0，CPU 明确更新
  条目1，再 GPU 覆盖条目0；输出 41→121→160，15 次 seek 与哨兵一致。
  XML 恰有一次 offset24/size24 显式 CPU 写入，没有 GPU 目的表自动 CPU diff。
- 13 个负例 API+CLI 通过预期 GPU 前拒绝：无 coverage、未知 schema、重叠、
  late identity、未声明 writable table、frame allocation、后续非法指针、
  非法初始内容；旧 coverage、未对齐/不完整 CPU entry、伪造自动快照、缺 GPU flag。
- 另以最终库跑 T01/T09/T35/T49/T52/T62，六帧 API+CLI 通过，日志
  `local-m2-descriptor-replay/lifetime-final-targeted.log`。
- 最终库与 viewer 内嵌库同 SHA256：
  `bd255367cb6030fea2b1d05b4e4894a3401539d55d7f553b22a3c85c799da600`。
- **全量回归未运行；人工 UI 未完成；UE 正确打开与 replay 未达到。**

小表用例还包含每份 capture 六次 2×2 drawable present，释放应用引用并退出
局部 autoreleasepool 后结束捕获，防止缩略图保留修复耗尽 drawable pool。
该生命周期修复及实际 UE 重截结果见 [BATCH329](BATCH329_UE_DESCRIPTOR_LAYOUTS_AND_DRAWABLE_LIFETIME.md)。
