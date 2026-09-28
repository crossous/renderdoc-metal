# PHASE42：T41 blit descriptor 与 texture optimization

状态：实现与最终联合终端 batch 通过；GUI L4 待验，阶段开放。

接通 `blitCommandEncoderWithDescriptor`，重用真实encoder包装/替换/命令归属与pass action；
支持无counter sample buffer的descriptor。有sample attachment时显式记录并在replay拒绝，
不默默丢弃计数器；counter API不在本阶段范围。

接通 `optimizeContentsForGPUAccess` / `optimizeContentsForCPUAccess` 各自whole和slice/level
四种native replay。补frame resource引用和null/子资源界限检查；修复CPU slice/level版本
原先误写whole chunk的缺陷（现在ID1223，whole仍1222）。旧capture IDs不变。

T41复用Metal_Blit_Transfer，设置 `RENDERDOC_METAL_BLIT_HINTS=1`；默认T37不改变行为，
旧capture保留。原六种pitched copy、readback padding、slice/mip与最终RGBA64/128/192/255
均继续断言。额外13×7纹理只被GPU optimization引用、不参与任何copy/draw：其初始内容
17/34/51/255仍必须进入capture，并在多个事件读取/回退时逐字节保留。

必跑：Metal API Validation native、capture/XML四种chunk ID/参数、T37既有API断言+
仅hint引用资源初始内容、3-loopCLI、23类异常（缺resource/encoder、slice/level/整数溢出、
descriptor缺失身份、unsupported counter attachments）。四种hint有真实native调用，
但不宣称M2上的性能收益；Managed同步、counter sample buffers仍不在覆盖内。

共有blit encoder生命周期经全42份capture和420次open/close联合验证。
GUI不加一套重复T37检查，只验新创建入口、四条参数和hint-only纹理，见集中总单。
