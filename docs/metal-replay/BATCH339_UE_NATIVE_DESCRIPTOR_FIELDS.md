# BATCH339：UE 原生字段、纹理上传与来源对象生命周期

2026-10-01；持续目标 active，未提交或推送，未执行完整 UE GPU replay。

coverage v7 保留 v4–v6 合成标记兼容，使用显式 ResourceId/member offset
重编码 buffer VA、texture ID、sampler ID；普通 metadata、LOD/bias 和 inline
常量不参与地址查找。Sampler 必须支持 argument buffer；纹理限 Shared 2D、
≤2×2、单 mip/sample/array、RGBA8/BGRA8。其余静态/compute 小预算继续生效。

官方 UE 5.8.3 RHIDefinitions 枚举为 Buffer0/1、typed buffer2/3、Texture4/5、
CBV6、Sampler7。但 MetalUAV.cpp 的 SRV/UAV 构造统一分配 Texture4/5 句柄，
实际 view 可以是 FBufferView 或 FTextureBufferBacked。因此 4/5 的字段由
IR factory 的实际 packet 与显式来源共同验证：VA 位于 word0，texture ID
位于 word1，均可为空；不能仅凭 RHI 枚举要求 word0 必须为零。
安装源码与已验证的官方 5.8.3 tag 对齐，未将 Epic 文件加入公开仓库。

原生混合条目两捕获均采样 122/DEADBEEF，buffer VA 和 texture ID 在 replay
进程中改变，四轮 seek、clear 像素及普通字节不变。24 API+CLI 错误组拒绝；
Buffer1/CBV6/Texture5 以及 Texture4/5-buffer-view 正例均通过。

通用修复对照 D3D12 GetCopyableFootprints 与 Vulkan GetByteSize staging：
replaceRegion 复用 Metal 已有 block footprint，验证 storage/region/pitch/payload，
仅保存最后占用字节，3D 包含所有 image；此前 slice 重载仅保存首个 image，
BC 捕获错误按 pixel rows 读取 padding。新 padded 3D 两 image 与 BC1 原生 /
capture / GPU407/DEADBEEF /四 seek /12 API+CLI 负例通过，日志
metal-cpu-upload.Aeg701；脚本 test_metal_cpu_texture_upload_macos.sh。

另修复已有 reflection 下 GetComputeTextureForAccess 的槽位回退：当明确没有
write texture 时返回空（与同文件 buffer 查询一致），不把槽1只读纹理假定为
输出。修复前上述 texture→buffer shader 被错误拒绝；修复后 GPU 校验通过。
旧 t01/t02/t09/t11/t12/t35 API+CLI 回归通过。

UE provider 的 CreatedDescriptor 在 factory→RHI lambda/context queue→payload/
GPUValue binding 期间短期 retain 已知 native objects，防止释放后指针重用误绑
另一个 ResourceId；绑定结束即释放，未永久持有全表资源。隔离模块 SHA256
27949e1c2403a7b89769dbbaf4c030fc943a63be5b8827ba500856dc74c13165，98 导出不变。
原 Engine 文件未替换，当前进程 DYLD override。会话20261001-071808 End=1，
owned PID14694 SIGTERM，launcher exit0；仍只有旧基线13 AutomationTest错误。

UE 新帧 11,272,241bytes，SHA256
b341dea42973b795872e32f2951e375fbc127d5bab3eabe7c636134661e40f43，
Saved/RenderDocMetalCaptures/UE58_NewMap_retained_b341dea4.rdc 已保护。
CPU 审计38,609chunks/scope25,252、1,477/1,477帧首槽字节匹配、31 producer /
31 expected/source匹配；lifetime、frame value epoch、inline、producer问题均0。
4,127历史来源身份未保留，不能当作执行时 liveness 或 alias 证明。
本次 capture 库322eb0f6…；最终定向库与扩展 suite 见 B340。

全量未跑，新增人工 UI 未验；当前 UE 完整画面、MRT、pass scope replay 尚未通过。
