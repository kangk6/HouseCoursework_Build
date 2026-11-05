# 行走动作来源与重建

真人动作来自 [CMU Graphics Lab Motion Capture Database](https://mocap.cs.cmu.edu/) 的 [Subject 7 / Trial 1：walk](https://mocap.cs.cmu.edu/search.php?motion=%25&subjectnumber=7)。原始 BVH 采用 [Bruce Hahne 转换的镜像](https://github.com/una-dinosauria/cmu-mocap/blob/master/data/007/07_01.bvh)，对应数据见 cmu_07_01.bvh.txt。CMU 官网声明数据可自由用于各种用途，原数据库由 NSF Grant #0196217 资助。本目录保留来源，未将其宣称为本组采集。

保留原 Harper 人物网格和衣着；将第100至230帧的一个完整步态循环重定向到原骨架。原始素材为120fps，重采样至60fps、66个关键帧，最后六帧作循环衔接。按人物腿长做两段腿部求解，校正脚掌相对中立姿态的翻转，以脚跟和前掌接触点约束高度；步态周期共同高度漂移被扣除。头部保持正常前视，并保持原模型携带物隐藏。这是针对本人物的离线重定向，不是运行时通用足部IK。

walk_tracks.json 是已校准的 UE 坐标骨骼采样，包含派生姿态，不包含角色模型或贴图；ue_ref.json 为坐标核对用的现有人物参考骨架。普通协作只需运行团队资源导入脚本，无需重建动画。

需要重新烘焙时，先编译 Editor 目标，再用 UE 5.3 的 Python 命令行运行本目录 bake.py。它创建或更新 /Game/Coursework/ResidentMocap 内的 A_ResidentWalk 与 BS_ResidentWalk。

需要从原始动作重新重定向时，使用装有 numpy、scipy 的 Python，执行 `python retarget_walk.py <Harper原始harper-brisk-linear.glb路径>`。目标 GLB 来自现有角色资源，源文件获取记录见旧工程 SourceAssets/ResidentBrisk/RESOURCE.md。该步骤会重写本目录 walk_tracks.json；重新烘焙并实际预览后才可更新团队资源包。
