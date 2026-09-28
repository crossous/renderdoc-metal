# PHASE48：预编译 Metal Library 加载

T48 / `Metal_Binary_Library`。新增四入口和最终native/capture/offline/完整联合验证通过，
49 captures、577类异常、490次lifecycle，另三份源码路径新录复验；详情BATCH48。
GUI L4未执行，阶段保持开放。

## 接通及修复

- `newDefaultLibraryWithBundle:error:`，原chunk1015。
- `newLibraryWithFile:error:`，原chunk1016。
- `newLibraryWithURL:error:`，原chunk1017（本批覆盖本地file URL）。
- `newLibraryWithData:error:`，原chunk1018。

移除4个bridge标记、接通4个旧chunk。无新chunk、无编号变动，Max仍1264。
File/URL/bundle记录原始metallib字节及origin诊断信息，dispatch data用create_map处理
分段输入。C++/ObjC++的dispatch_data_t ABI不同，bridge边界使用void*，内部按平台类型转换。
Replay只使用capture内字节，不重新打开origin。身份、重复ID、基础payload头检查先行，
Metal编译失败干净报错；不将任意未知二进制格式宣称为受支持。

原default library路径也纳入测试：直接从NSData拷贝，不将dispatch_data强转为NSData，
不手动release autoreleased字符串；旧chunk1014格式不变。库/函数创建失败返回NULL，
保留NSError，不记录空资源；newFunction反序列化增加library/函数ID与名称校验。

## 提前释放修复

T48首次注入捕获暴露了真实堆损坏：多个相同native library/function实例可能被缓存复用，
app释放function wrapper后，native对象上的旧retaining association仍指向已释放内存；
wrapper地址复用时可能在新对象构造中释放错误对象。终端crash report/LLDB定位后修复。
Library/Function的new*返回值改为独立proxy拥有native +1，proxy析构先清理ObjC实例，再
释放C++记录/对象，最后释放native对象；不再为这两种对象建立反向retaining association。
这不是全Metal对象所有权重构；其他对象的历史association策略需后续独立审计。

fixture保留立即release compute function的行为，并在首帧前释放所有library以及VS/FS。
Pipeline资源记录对function/library的父依赖必须继续保留，正式capture仍包含五库七shader。

## Fixture / 断言

独立app bundle内编译default.metallib，额外Test.bundle与Empty.bundle。五库分别经file、
URL、分为17bytes+其余bytes的dispatch data、指定bundle、main bundle加载；都真实执行。
native验证missing file/URL、空bundle、非法data/source、缺失function返回值及NSError。

五dispatch写676-byte buffer：五段各32个uint32，起值29/61/97/137/173，各段依次递增；
末尾36bytes保持0。一个draw输出RGBA29/61/97/255。自动逐元素、padding、回退、shader
身份/父依赖、CS/VS/FS reflection与usage核对。二进制库无原始MSL源码，rawBytes/files为空
是预期，不伪造源文件；本批不增加metallib反汇编/源码恢复能力。

脚本在回放前临时移走整个生成目录，使全部原始路径失效，退出时恢复；XML/zip检查五份
payload与原文件逐字节相同、failed创建无chunk。64类畸形输入及改origin后仍成功的正例，
子命令30秒超时，信号退出不算拒绝。完整终端回归与版本见BATCH48。

边界：Metal工具链/硬件能接受的普通预编译库；动态库、stitching、function constants、
跨OS/GPU二进制兼容性与真实引擎完整支持不在本批承诺。GUI差异并入QA_CONSOLIDATED。
