# BATCH57：Argument buffer 数据与地址重定位

起点140 bridge / 82旧chunk，Max1274。只做终端开发验证；保留所有旧修改和UE路线。

| 功能族 | 范围/边界 | Fixture/旧T | 验证 | 状态 |
| --- | --- | --- | --- | --- |
| Argument packet | setBuffer/setBuffers、constantDataAtIndex、arrayElement选择；顶层Shared目的buffer、只读成员、scalar/vector常量、FS多packet | T57；T12/T49/T56及sentinel | native/capture、偏移/数组/空绑定、初值与两提交CPU修改、重定位/回退/描述符/usage；异常/真实不支持变体；最终集中回归 | 集中自动通过，GUI L4待验 |

不把GPU地址原始字节当可移植数据：恢复initial/CPU更新后按packet重新编码资源引用。
资源初始化布局保持不变，frame内只允许常量CPU数据修改；资源重新编码明确拒绝。
嵌套、GPU生成、非Shared目的buffer、可写buffer指针不在本批承诺内。
这是共享初值路径改动，本族即触发集中关口；没有混入下一族，也没有启动GUI。

## 实现与边界

- 接通4 bridge：单个/批量buffer成员、constantDataAtIndex、arrayElement选择。
  **140→136 bridge；旧chunk仍82**。追加1274选择、1275 buffer、1276常量getter、
  1277帧内资源重编码诊断，Max1278；批量buffer规范化成逐成员1275，空range无写入。
- packet按`(buffer ID, offset)`登记；支持一个encoder的多个不重叠packet，拒绝重叠与
  同位置换encoder解释。选择检查乘加溢出、alignment、Shared存储和encodedLength范围。
  只读、非嵌套buffer pointer验证类型、最小长度与alignment；支持顶层pointer数组，
  包括Metal将`metal::array`反射成透明单array结构的形式，不依赖编译器生成的成员名。
- 常量getter不保存CPU指针，只登记成员并标记目标buffer dirty；初值和提交时CPU快照
  携带实际bytes。支持scalar/vector（含vec3存储对齐），不承诺矩阵/任意嵌套结构。
- 初值恢复、loading CPU update、active replay提交更新三个入口复制后均重编码当前
  replay资源地址，保留常量和padding。补齐**无帧内CPU更新的Shared初值**恢复，避免
  只通过contents初始化的成员buffer读到零、以及GPU结果在seek时残留。
- 新buffer成员通过私有packet registry接入FS通用descriptor/reflection/usage，未改
  公共MetalPipe序列化布局。qrenderdoc仅补FS buffer成员槽位地址映射，已构建未运行。
- 成员id限0–31；瞬时nil/部分覆盖/重绑后使用可支持，draw前要求声明的资源成员齐全。
  帧内调用buffer/texture/sampler资源setter保留native行为，capture记录明确不支持诊断，
  离线拒绝。帧内常量CPU修改和普通member buffer CPU数据修改可支持。
  GPU生成packet、nested argument encoder、非Shared目的buffer、可写pointer成员、
  VS/CS argument packet和任意资源重编码时间线不在本批承诺内；不声称全部均已自动检测。

## T57与自动证据

`Metal_Argument_Data`使用621-byte目的buffer，packet起点256/352、encodedLength96。
每packet含buffer id0/2/3、texture id4/5、sampler id6/7、uint4 bias id8、uint delta id9。
三个成员buffer长度117/181/213，packet0 offsets16/32/48、packet1 offsets32/48/64。
两packet交换texture/sampler身份；两次提交各两个draw，中间更新delta和第一个成员的数据。

| draw顺序（0起） | 左半屏RGB | 右半屏RGB |
| --- | --- | --- |
| 0 | 17/27/37 | 0/0/0 |
| 1 | 17/27/37 | 57/67/77 |
| 2 | 23/33/43 | 0/0/0 |
| 3 | 23/33/43 | 68/78/88 |

alpha均255；API按3→0→1→2→0→3检查像素、descriptor身份/offset/size、反射名称、usage、
成员数据、外层bias/delta和padding。97-byte CPU更新跨过另一个packet的资源地址区域，
因此覆盖真正的提交更新重定位，不只验证初始化。

- native Metal验证层/capture各6帧；CLI 3 loops；15份定向API验证层及CLI通过。
- 新增**161异常**：身份/类型、array溢出/重叠、buffer成员slot/offset/size、常量类型、
  缺失选择/必需成员、fragment offset/encoder状态等；异常runner有30秒超时并拒绝crash。
- 独立合法变体清零capture初值和两份CPU update里的GPU资源地址，保留常量；API在
  Metal验证层下仍通过全部四阶段/回退断言，CLI 3 loops通过。
- 真正帧内重绑资源变体native/capture各3帧成功；离线明确报`unsupportedEncoding`。
- 最终集中关口：**58 captures API/CLI、1639异常、580 lifecycle opens**全部通过；
  30份正式capture的API使用Metal验证层，resident growth **2,555,904 bytes**。
  范围T01–T57+T10 marker；不是所有历史独立脚本/T00都已重跑。
- 额外T01/T12/T47三份源码路径新录兼容API/CLI通过；T35 native/capture各6帧、
  新录API验证层/CLI通过，未覆盖旧正式capture。T35旧测试曾在第一次dispatch就期望
  `17,17`，与原生帧前清零不符；现检查第一次`17,0`、第二次`17,17`并往返，未放宽断言。
- bash语法、Python编译、`git diff --check`通过。没有GUI/Computer Use、提交或push。

主要日志：`/tmp/metal-batch57-final.log`（集中）、`/tmp/metal-batch57-targeted.log`、
`/tmp/metal-batch57-invalid.log`、`/tmp/metal-batch57-source-compat.log`、
`/tmp/metal-batch57-t35-{native,capture,replay}.log`、`/tmp/metal-batch57-app.log`。
真实拒绝变体日志`/tmp/metal-batch57-reencode-{native,capture}.log`。

## 重现与产物

```sh
# 本族完整入口；本次各组成步骤分段运行通过，不声称整条入口一次运行过
bash util/buildscripts/scripts/test_metal_capture_batch57_macos.sh
# 只重放现有正式capture，不重新录制
RENDERDOC_METAL_LAST_TEST=57 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

| 产物 | SHA-256 |
| --- | --- |
| `build-macos-debug/lib/librenderdoc.dylib` 与app `Contents/lib`副本 | `8e84e249d68438a83b2720f318a24a740ce16d374a640596e20425859313130b` |
| `build-macos-debug/bin/qrenderdoc.app/Contents/MacOS/qrenderdoc` | `fe8bcf852b683bc463a3be883e6c54208b7bd45054a24f5dbace58c912346d74` |
| `captures/metal-smoke/t57_capture.rdc` | `c852c41b973ca90efd228ec188fcf924019c37b98f622f7eef74bd849bb52cc8` |

## 后续

无本批计划内自动回归欠项。T57界面槽位/资源跳转/packet切换与T35初值回退差异并入
`QA_CONSOLIDATED.md`；累计**T34–T57+T10 marker，共25份**待人工，阶段仍开放。
下一编号T58；候选texture views/共享存储别名，须先审查初值与双向写入可见性，不能只做
native转发。用户UE路线及原有dirty修改保留；本批不等于真实UE一帧已跑通。
