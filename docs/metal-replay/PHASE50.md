# PHASE50：Texture CPU 读回与 Managed 子资源同步

T50 / `Metal_Texture_Readback`。终端自动通过，GUI L4待验，阶段开放。
接通2个bridge、3个旧chunk：getBytes 1072/1073、synchronizeTexture 1205。
不追加chunk，Max1264；剩余158 bridge / 93实际未处理chunk。

## 行为与边界

- 两种getBytes原本已透传原生CPU读回。本批保留原生调用/返回内容，补充帧内捕获与
  resource read引用；后台读取不追加初始化历史。记录Texture、region、mip、row pitch，
  slice重载另记image pitch和slice。没有应用指针、读回payload或未初始化的host padding。
- 离线只校验/显示CPU读取元数据，不分配原应用目的缓冲、不再次执行应用CPU逻辑。
  CPU读取后产生的GPU输入由已有CPU buffer更新捕获；T49的提交前快照和事件回退路径
  在T50直接复验。getBytes不是GPU copy action，不新增虚构的copy/CPU usage分类。
- 回放校验真实纹理类型、Shared/Managed存储、非MSAA/非framebuffer-only、mip/slice、
  非空region、子资源边界、行距、跨行/跨图像地址计算溢出。3D多层要求有效image pitch。
  支持校验普通非packed、非compressed、非depth线性格式；本批实际验证RGBA8的
  2D/2DArray/3D。1D特殊pitch、压缩/打包/depth格式另做，不能把本批计为全格式支持。
- synchronizeTexture现在真正向native replay encoder编码，保留frame resource引用。
  校验当前活跃blit encoder、资源身份、mip/slice及Managed存储。原生验证层明确拒绝
  Shared同步，因此回放同样拒绝；不是Shared静默no-op。共享subresource helper也加了
  texture资源类型检查。已有synchronizeResource空回放不在本批范围。
- M2 Pro已运行Managed路径，但不声称验证了独显VRAM/CPU缓存一致性。一般纹理初始内容、
  GPU-only捕获前历史、texture views、外部CPU副作用不因本批自动支持。
- 3D getBytes原生/capture两层内容已验证，离线保留资源/参数与已捕获的CPU派生输入；
  现有Replay API `ReadTextureSubresource`/Texture Viewer尚不支持3D展示，未在本批扩展。

## Fixture

1. 11×7 Managed 2D和18×10、两slice、两mip的Managed数组纹理，GPU分别clear
   mip1/slice0为RGBA43/43/43/255、mip1/slice1为79/79/79/255，之后同步。
2. 7×5 Shared纹理在帧内仅由CPU getBytes引用，内容113/113/113/255；19×3 Managed
   纹理仅由synchronizeTexture引用，内容197/197/197/255。两种独立资源均保留并逐像素复验。
3. 8×4×2 Shared 3D仅CPU读引用，两个z plane分别151/152。捕获前按depth=1逐plane
   replaceRegion，明确不依赖旧multi-image upload布局；读取depth2，row32/image128。
4. 四次读回都是局部region，row pitch20/28/16/32，slice重载image pitch84/128。
   native和capture均逐字节检查有效像素、行padding、图像间隙、首尾哨兵。
5. 读回值写入68-byte Shared参数：uint32前五项43/79/113/151/152，其余48bytes为零。
   第二command buffer的draw输出RGBA43/79/113/255。XML核对17-byte CPU差异payload；
   API在同步/getBytes/draw之间往返，验证提交前参数全零、后续提交数据和回退正确，
   并核对纹理目标mip/slice、未改动子资源、fragment资源usage及最终像素。

一键入口、176异常+2合法变体、联合结果及版本见BATCH50。UI只验新增元数据/资源链接、
参数回退与2D画面，已并入QA_CONSOLIDATED；不要求原应用CPU内存或3D viewer功能。
