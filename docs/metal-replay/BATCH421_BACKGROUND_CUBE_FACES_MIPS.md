# B421：背景cubemap面与mip附件

实际UE encoder17737绑定BG cubemap3499 RG11B10Float128x128 face4/mip0，完整initial contents已捕获；此前只允许slice0/level0和2x2 cube目标。不是缺初始化内容。

候选65允许Private Tracked RenderTarget RG11B10Cube完整初始目标，维持8192尺寸、sample1、无resolve，检查face<6、mip<实际mipmapLevelCount及显式pass尺寸<=所选mip尺寸。传递原生attachment slice/level，不创建替代2D view或改clear/draw。旧coverage及其他format/shape仍保持范围。

a201df3a/metal-cube-color-faces.hA24Cb：2captures/8reset-seeks/10 API+CLI背景initial/format/face/mip/store反例组通过。8x8四mipCube初始化全部faces/mips，frame分别clear face4/mip0与face5/mip1，GetTextureData选定subresource完整像素与原Native readback精确字节一致，face0全0保持，EID0恢复所选初始像素。Native mip/face尺寸与RDC metadata正确；没有shader或PSO替代。尚需真实UE下一预检及最终全量，未提交推送。
