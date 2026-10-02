# B379：帧内大于2px的color placement来源（进行中）

实际新UE bfa3f117…初始来源完整，但第29909 frame heap texture仍是192×104 RG11B10、
Private tracked、usage3。19个frame texture涵盖R8/R16/3D/array等，259 frame buffers
含7个大于64KiB(max131072)，81 direct+31 indirect dispatch、229 draws/86passes；
这些还不具备完整coverage，不能打开整帧GPU。

对照D3D12 CreatePlacedResource的heap+offset重建、Vulkan image view的AddParent/
baseResource/baseResourceMem，沿用现有Metal frame placement生命周期和Native尺寸对齐查询。
v38候选只扩frame2D单mip/slice/sample1，512×512以下，RGBA/BGRA/R16Float/RG11B10Float，
Private tracked/default cache/identity swizzle，usage read|RT/read|write/read|write|RT。
frame view仍必须exact parent/format/mip/slice，源GPUID按出生后的Native对象重定位。
新增large frame color clear附件要求RT usage和Clear load；Nativefootprint、范围、alias、
heap1MiB和aggregate256MiB等边界不变，其他3D/array/indirect尚未放行。

新增 test_metal_descriptor_frame_color_macos.sh：RG11/R16×direct/view的192×104，
同heap额外创建usage3精确UE类型（不作GPU读取），可采样source以GPU clear定义初始像素，
再经typed packet访问右下角GPU64/128/192、覆盖clear、回跳逐像素及PickPixel三位置；
EID0释放frame source并检查table清零。新增畸形输入在GPU wait前拒绝。
目前仅源文件、ObjC++夹具/回放helper和Python/shell静态编译完成；尚未运行Native/GPU。
等待B378精确7ef3fa13…全量结束后再重编译driver，避免改换运行中库。
持续目标active，人工UI仍锁屏待验，未提交/推送。

B378精确7ef3fa13…全量完成后开始v38库74f7ef239d918f48a08739e97ebdafb4cfd44c22a0341a6f10c50dc08b865651。
首轮aFUKv7 Native和两次捕获GPU64/128/192正确，但夹具只有future slot、无背景live slot，
不符合已有PrepareDescriptorSlotShadow非空contract，OpenCapture在初始检查拒绝。
修正夹具加入GPU实际读取的背景scalar13 typed source/metadata，并保留完整inline来源声明；
不为测试放宽driver已有contract。qj5Zgh OpenCapture成功，helper因label未序列化而找不到table/output；
改用实际buffer尺寸和typed普通metadata识别，保留缺失诊断，不改driver资源命名。
继续运行直接/同format view和反例，尚无完整新用例PASS声明。

第三轮6BFCcb第一捕获四seek/19968完整像素/三PickPixel/GPU值和新ID通过；第二捕获
被frame heap创建preflight拒绝。CPU导出确认上一捕获texture20仍由背景历史引用保留，
新texture28复用offset0形成未证明的texture overlap；这是夹具重复范围问题，不能为此
放开alias规则。两个捕获使用独立Native query对齐的frame范围，继续保留heap1MiB上限。

最终metal-frame-color.GLmceD，精确74f7ef239d918f48a08739e97ebdafb4cfd44c22a0341a6f10c50dc08b865651：
RG11/R16×direct/view八捕获/32seek通过，GPU64/128/192（R16的GB为0）和背景scalar13
共同校验typed relocation；完整19968px clear→overwrite→rewind、三PickPixel位置以及
EID0释放frame来源/table全零通过。26direct×2+31view×2=114 API+CLI反例组，均无GPU wait。
原生和捕获两个路径分别独立得到GPU预期值；usage3精确UE资源创建通过但没有GPU读取它，
已验证GPU source为usage7+全纹理clear定义的附件。这不等于真实usage3计算UAV已验证。
回放helper最初按label识别失败、空背景live slot问题均在夹具修正；没有放宽driver契约。
B378库7ef3fa13…全量308/7786/3080已通过；当前74f7ef23…精确全量尚未运行，人工UI锁屏未验。
实际UE仍无完整coverage/尚未GPU提交，继续array/3D/frame buffer/indirect适配，目标active。

精确74f7ef23…旧21类typed descriptor兼容（frame-color-old21-compat.log）674 API+CLI反例通过。
后续v39 array/3D写入来源候选开始，尚未构建替换；GPUserial无UE全帧提交。
