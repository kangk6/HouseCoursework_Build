# 行走动作来源与重建

真人动作来自 [CMU Graphics Lab Motion Capture Database](https://mocap.cs.cmu.edu/) 的 [Subject 7 / Trial 1：walk](https://mocap.cs.cmu.edu/search.php?motion=%25&subjectnumber=7)。原始 BVH 采用 [Bruce Hahne 转换的镜像](https://github.com/una-dinosauria/cmu-mocap/blob/master/data/007/07_01.bvh)，对应数据见 cmu_07_01.bvh.txt。CMU 官网声明数据可自由用于各种用途，原数据库由 NSF Grant #0196217 资助。本目录保留来源，未将其宣称为本组采集。

保留原 Harper 人物网格和衣着；将第100至230帧的一个完整步态循环重定向到原骨架。原始素材为120fps，重采样至60fps、66个关键帧。完整周期采用六阶傅里叶轨迹拟合，首尾姿态与速度连续，不再将最后六帧强行拉回起点。拟合后重建相连的腿部骨骼，以稳定的解剖方向确定膝盖弯曲侧，避免腿接近伸直时求解方向翻转。以脚跟和前掌约束高度，并采用全周期统一的接触与伸展余量。头部保持正常前视，并保持原模型携带物隐藏。这是针对本人物的离线重定向，不是运行时通用足部IK。

walk_tracks.json 是已校准的 UE 坐标骨骼采样，包含派生姿态，不包含角色模型或贴图；ue_ref.json 为坐标核对用的现有人物参考骨架。普通协作只需运行团队资源导入脚本，无需重建动画。

需要重新烘焙时，先编译 Editor 目标，再用 UE 5.3 的 Python 命令行运行本目录 bake.py。它创建或更新 /Game/Coursework/ResidentMocap 内的行走动画、行走混合空间与两个起步转身动画。

需要从原始动作重新重定向时，使用装有 numpy、scipy 的 Python，执行 `python retarget_walk.py <Harper原始harper-brisk-linear.glb路径>`。目标 GLB 来自现有角色资源，源文件获取记录见旧工程 SourceAssets/ResidentBrisk/RESOURCE.md。该步骤会重写本目录 walk_tracks.json；重新烘焙并实际预览后才可更新团队资源包。

姿态校准扣除了来源骨架与目标骨架之间的固定躯干倾斜，保留小幅周期摆动。手部根据当前西服躯干蒙皮的截面外廓确定全周期避让余量，再以稳定的肘部方向和统一手腕角度约定重建手臂，避免逐帧启停修正。`audit_tracks.py <同一GLB路径>`检查最终采样（包括循环衔接帧）的躯干方向、手部外廓间距、各肢体相邻帧转角与角加速度，结果见 clearance_report.json。该外廓检查不包含所有衣物自碰撞，不能替代原生多角度预览。

原生 `-HouseTest=gait_preview -HouseGaitFrameStep` 诊断按1/60秒推进骨骼采样，每个角度渲染三个以上循环，图片位于 Saved/Screenshots/GaitContinuity。按60fps编码这些图片只代表动画采样速度，不代表设备能以60fps运行。一般行走资产修订可在烘焙命令增加 `-HouseWalkOnly`，避免重新保存未改动的转身动画。

`python turn_steps.py <Harper原始harper-idle-linear.glb路径>`生成两个一秒钟的90度起步转身动作。两脚先后抬落，世界空间的支撑脚位置固定；运行时角色朝向必须与同一条三次平滑曲线同步。它服务于当前固定疏散路线，不是任意角度、任意地形的通用转身系统。到达集合点使用速度驱动的减速和待机混合，没有另行声称制作专用停步动画。
