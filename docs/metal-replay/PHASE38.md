# 阶段 38：T37 blit transfer

状态：实现与终端自动验证完成，GUI L4 待验，阶段开放。

## 纵向切片

T37 两次 buffer→texture 上传使用相同 3×2 RGBA8 内容，分别写入 array 的不同 slice
与 origin；整纹理复制后，再把 source slice 1/mip 1 复制到 destination slice 0/mip 1。
两次 texture→buffer 读回分别经过无 options 和显式 None 的 ObjC 入口。随后 fragment
shader 读 destination 的 mip 1/slice 0 并输出到 backbuffer。

- 输入 buffer 4096 bytes；source offset 256；row pitch 256；image pitch 512。
- 输出 buffer 4096 bytes，先 fill `0xa5`；两个区域分别从 512/1536 开始。
- 纹理 16×8、2 slices、2 mips；实际传输 `(3,2,1)`，非零 origin。
- Replay API 断言六个 Copy actions 的资源/subresource/usage，检查原始字节、未触及
  padding/texels、前后 seek 和最终像素。whole/range 分别有独立 native encoder 调用。

## 输入防御

调用 native 前检查纹理/encoder/buffer、slice/mip、非空 region、extent、格式/采样数、
pixel alignment、row pitch 与 buffer bounds；算术使用剩余容量和除法避免溢出。
whole/range 还检查格式、类型、尺寸、slice/mip counts 与重叠 self-copy。
异常脚本 43 cases 要求正常非零退出，signal/crash/timeout 都视为失败。

## 验证范围与关闭条件

- 必跑：T37 native/capture/XML/API/CLI/异常/lifecycle，T10_debug marker 回归。
- 共享改动覆盖：T36 visibility/store/D32S8，与 T07 depth/stencil、T08 resolve、
  T09 subresources、T10 blit、T11 compute、T16/T17 texture binding。
- 本批进一步在最终库验证 T01–T37 + T10_debug 的 API/CLI/lifecycle，结果见 BATCH38。
- 原始函数桥接已存在，四项新增是实际 replay，不按四项新 bridge 重复计数。
- 人工 GUI 按 `QA_BATCH38.md` 验收；不等待 GUI 才能推进后续独立功能，但不得自动关闭。
