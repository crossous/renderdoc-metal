# B415：重复heap residency声明

真实UE pre-submit65的新增精确诊断定位到encoder17678，useHeaps数组4项、unique3，重复heap4160；Native object/type/device/real/hazardTracked全部正确。先前未输出encoder的streamOffset273408不能用来将失败直接映射到第一条useHeaps。新的trace才是具体失败证据。

只对coverage>=65允许重复有效heap，保留32项上限、scalar/array形状、live compute/render encoder、resource身份/type/同设备/Native tracked以及render stages检查。Native Serialise_declareHeaps原样转交已有数组，不去重或创建替代资源/命令。重复声明不增加写权限或ownership。

本地官方UE MetalCommandEncoder.cpp::UseHeaps直接传入Heaps.GetData()/Heaps.Num()；既有Metal wrapper也已接受重复。通用资源声明沿用已有RenderDoc记录/回放顺序；这里仅修正Metal CPU预检额外去重要求。

新增极小fixture：两个64KiB Private Tracked placement heaps，compute和render原生useHeaps为[A,B,A,B]，正常Native计算/MRT/普通metadata不变。测试Shared、undeclared Private间接参数、parallel Private，reset seek与malformed身份/形状/encoder生命周期/stages继续检查。测试正在执行，不能先宣称通过。未提交完整UE GPU回放；未提交或推送。

6b7ef2bc/metal-duplicate-heaps.zAnTaz：6captures/24reset-seeks/108indirect+24fresh+24mixed+30heap rejection groups全部通过，库前后哈希一致。真实UE candidate65推进至setVertexBuffer，背景全局descriptor table buffer24/index0；不是nil解绑。无UE GPU wait/submission。
