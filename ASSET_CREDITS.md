# 人物资源来源

## 当前版本 v0.5.0：Harper 待机住户

发布方：Innerscene，基于 MakeHuman/MPFB CC0 身体、骨架与皮肤资源制作。资源页：https://www.innerscene.com/tools/library/3d-parts/idle-standing-harper-3d-person-animated-ba9bc23f 。授权 CC0，https://creativecommons.org/publicdomain/zero/1.0/ 。原始和转换 GLB、授权记录在 SourceAssets/ResidentIdle。

带骨架的成年真人角色 Harper，六秒原地待机循环。为 UE 5.3 转换原始三次插值为 30 fps 线性采样，并加入资源原有公共父骨架节点以保持左右脚和躯干分支完整。使用真正的骨骼动画，未新增自动行走、疏散或 AI。

## 历史版本 v0.4.0：Avery 静态住户

发布方：Innerscene，基于 MakeHuman CC0 资源制作。
资源：Relaxed stance - 3D Person, Standing，standing-relaxed.glb。
官方页面：https://www.innerscene.com/tools/library/3d-parts/relaxed-stance-3d-person-standing-139755ea
下载地址：https://www.innerscene.com/api/library/relaxed-stance-3d-person-standing-139755ea/download
授权：CC0，https://creativecommons.org/publicdomain/zero/1.0/。

2026-10-13 下载。完整穿着的成年人，蓝衬衫、牛仔裤、鞋子，九个静态网格部件，带皮肤、服装、头发、眼睛贴图。身高 176 厘米。发布方说明约 121k 三角形；没有骨架或动画，本工程也没有把静态模型描述为带待机动作。

导入资源在 Content/Coursework/ResidentRefined；原始 GLB 在 D:/House_assets/ResidentRefined/standing-relaxed.glb。原先的 Quaternius 模型资源保留用于回滚，新版日景和夜景关卡已替换为 Avery。

## 历史版本：Quaternius 低模住户

作者：Quaternius。
资源包：Background Posed Humans Pack。
官方页面：https://quaternius.com/packs/backgroundposedhumans.html
授权：CC0 1.0 Universal，https://creativecommons.org/publicdomain/zero/1.0/。

使用模型：Female_Standing.fbx、Female_Hairstyle_1.fbx。
下载来自作者官方页面所链接的公开 Google Drive 文件夹。原始 FBX 和作者 License.txt 保存在 D:\House_assets。

这是一名低多边形、固定姿态的人类居民，用于房屋场景中的人物展示；尚未提供行走动画，也未改变原第一人称控制角色的操作方式。人物按约 170 厘米身高缩放，放入日景和夜景关卡。

原有 HouseFurniture、ModernHouse 等房屋资源的授权信息应由原工程提供者保留。本文件只说明本次新增模型的来源。

## v0.8.0 Tab 面板字体

中文使用 Adobe 思源黑体简体中文子集 Source Han Sans CN，Regular 与 Medium 字重。
官方仓库：https://github.com/adobe-fonts/source-han-sans/tree/release/SubsetOTF/CN 。
授权：SIL Open Font License 1.1；完整许可随字体保存在 Content/Slate/Fonts/SourceHanSans-LICENSE.txt，独立运行包也包含该许可。
数字使用 Unreal Engine 自带的 Roboto Light，与引擎字体资源一同打包。

## 2026-10-15 字体预览分支

霞鹜新晰黑 LXGW Neo XiHei v1.305，官方来源 https://github.com/lxgw/LxgwNeoXiHei/releases/tag/v1.305 ，按 IPA Font License Agreement v1.0 提供，原始字体未修改；许可证为 Content/Slate/Fonts/LXGWNeoXiHei-LICENSE.txt。

得意黑 Smiley Sans v2.0.1，官方来源 https://github.com/atelier-anchor/smiley-sans/releases/tag/v2.0.1 ，SIL OFL 1.1；原始字体未修改，许可证为 Content/Slate/Fonts/SmileySans-LICENSE.txt。预览仅将该展示字体用于大标题，小字继续使用思源黑体。

以上为字体对比预览，不属于已经发布的 v0.8.0 运行包。

2026-10-15 第二轮书写感预览增加 LXGW WenKai Light v1.522（https://github.com/lxgw/LxgwWenKai/releases/tag/v1.522）和 Long Cang（https://github.com/google/fonts/tree/main/ofl/longcang），两者均按 SIL OFL 1.1 提供，原字体文件未修改。许可证为 Content/Slate/Fonts/LXGWWenKai-LICENSE.txt 与 LongCang-LICENSE.txt。龙藏体仅用于大标题，正文使用文楷细字重；仍为预览分支内容。

正文对照预览增加 Source Han Serif CN Light（https://github.com/adobe-fonts/source-han-serif/tree/release/SubsetOTF/CN），SIL OFL 1.1，原文件未修改；许可证为 Content/Slate/Fonts/SourceHanSerif-LICENSE.txt。此字体仅为候选预览，尚未发布至现有运行包。

## v0.8.1 默认字体定稿

用户选择正文方案 A，发布默认使用 Long Cang 龙藏体标题 + LXGW WenKai Light 文楷正文，数字使用引擎自带 Roboto Light。两款新增字体的原文件和完整许可证均随独立运行包提供；原文件未修改。其余候选字体保留为设计对比材料，不作为默认正文。


## v0.12.0 Harper 跑步动画
Innerscene Run - Harper，https://www.innerscene.com/tools/library/3d-parts/run-harper-3d-person-animated-a726b25c ，发布页标注 CC0。复用现有 Harper 网格与骨架；来源、转换及绑定检查见 SourceAssets/ResidentRun/RESOURCE.md 和 Docs/Evacuation/run_import.json。


## v0.12.1 快步与厨房火焰
Harper 快步动画来源 Innerscene（CC0）：https://www.innerscene.com/tools/library/3d-parts/brisk-walk-harper-3d-person-animated-0efea8f3 。详细来源及转换见 SourceAssets/ResidentBrisk/RESOURCE.md。火焰使用项目已有 Unreal StarterContent P_Fire，另制关闭粒子灯光模块的厨房预设；原 StarterContent 不修改。


Ventilation v0.16: separated window parts and leaf material are derived from the project's existing HouseFurniture assets; original assets remain unchanged. The quiet wind loop is synthesized locally from seeded filtered noise by Tools/prepare_ventilation_assets.py; no external recording is used.

Desk lamps v0.16.1: independent emissive material copies retain the existing HouseFurniture textures. The small added living-room bulb uses a copy of Unreal Engine's BasicShapes/Sphere; its simple warm emissive material is created locally. Original furniture and engine assets are unchanged.

Hall lamps v0.16.2: the landing lamp uses independent copies of the existing HouseFurniture C1 furniture material/textures with a locally generated shade/bulb emission mask. Sconces reuse the source E1 illumination material on runtime instances. All original HouseFurniture assets remain unchanged.
