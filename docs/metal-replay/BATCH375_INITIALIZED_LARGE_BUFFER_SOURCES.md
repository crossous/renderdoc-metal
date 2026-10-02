# B375：完整初始内容的大 buffer 来源

v34背景buffer来源上限128MiB，但须存在与Native length相等的完整initial；
Standalone Private沿用通用Private initial staging/upload恢复，不要求heap。Shared及Tracked Private heap规则保留，
精确captured GPUVA+memberOffset、overflow、range、alias、frame-live校验不变。
CPU allocation预检收集background large buffer声明及initial sizes，匹配后才允许Native materialization；
单资源128MiB、sourced aggregate256MiB。frame birth仍64KiB、heap仍1MiB，整帧coverage未开放。

首轮vXLD5h Native独立计算122正确，replay被旧allocation预检64KiB拒绝，没有GPU replay提交。
统一CPU预算后metal-buffer-texture.JjSyAT：Private独立input65552B、memberOffset65540、
两捕获/八回跳GPU122/DEADBEEF、TextureBuffer offset256 overwrite/reset/PickPixel通过；
每捕获26组API+CLI反例，共52组，缺initial/短长initial/声明length mismatch/越界offset/旧版本均在
实际Private upload/wait前拒绝。精确库9e97baf51e5da5b765f2836e8070b071a18319d02b7c45f1dbc0c7421d8fbc33。

全量最近精确a9a106aa…308/7786/3080通过，growth7045120B；新库全量待组合适配后执行。
人工UI锁屏受限，实际UE完整GPU replay尚未提交；继续真实2D纹理尺寸/格式范围，目标active，无提交/推送。
