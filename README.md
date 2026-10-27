# HouseCoursework：住宅交互与智慧家居

Unreal Engine 5.3.2 计算机图形学课程项目。当前团队起点为 v0.17.1。

公开仓库保存 C++ 源码、配置、工具和资源版本清单。完整 `Content` 资源由组内网盘分发，包括住宅模型、贴图、地图、角色、字体及生成的 UE 资源。

雨效打磨版本需要额外的 `assets-v0.22.0-rain-polish-delta.zip`。已有基础资源的人只需把这个小包放进同一资源包文件夹，关闭UE后重新运行原来的导入脚本，不用重下四个基础ZIP。详见[雨效与湿润](Docs/雨效与湿润.md)。

> 省流：让codex帮你配

## 第一次加入

1. 安装 Git for Windows、UE **5.3.2**。 VS 2022 的“使用 C++ 的游戏开发”、MSVC **14.38** 工具集及 Windows SDK。

2. 下载仓库：

   ```powershell
   git clone https://github.com/kangk6/HouseCoursework_Build.git
   cd HouseCoursework_Build
   ```

3. GitHub 工程放在：`D:\HouseCoursework_Build`，

   网盘里的四个 ZIP 放在：`D:\HouseCoursework_Assets`，**不要解压**。

   然后关闭 UE，打开  `powershell`粘贴下面的命令：

   ```
   cd D:\HouseCoursework_Build
   .\Team\Import-Assets.ps1 -PackageDirectory "D:\HouseCoursework_Assets"
   ```

   看到 **`Import complete`** 就表示资源导入成功。

4. 右键 `HouseCoursework.uproject` → Generate Visual Studio project files，打开生成的解决方案，选择 **Development Editor / Win64**，编译 `HouseCoursework`，再打开 `.uproject`。如提示缺少引擎插件，按项目声明启用 UE 安装中对应组件，不随意升级引擎。

5. 首次打开会扫描资源、编译着色器。默认住宅地图为 `/Game/HouseFurniture/Maps/Demo_01_Night`。运行、Tab 智慧页、E 交互等见 [操作说明](Docs/操作说明.md)。

## 怎么协作

- 开工前关闭 UE，确认自己的修改已提交，然后 `git switch main`、`git pull --ff-only`。若 `Team/asset-manifest.json` 变化，从组内下载对应新资源包并再次运行导入脚本。
- 每项任务开一个分支，例如 `git switch -c feature/kitchen-lighting`。先在群里说明负责模块和将修改的地图/蓝图。
- 保存并验证代码后提交自己的文件、推送分支，创建 Pull Request，由我检查和合并。`main` 保留可运行版本。
- **同一地图或蓝图同一时间只由一人修改。** 两个人分别改 C++ 文件通常可以合并，二进制资源不能这样处理。
- `Content` 被 Git 忽略，因此 UE 资源改动不会自动出现在 Git 提交中，交付资源必须走下面的增量流程。

## 改了蓝图、地图、模型或材质怎么办

1. 保存并关闭 UE。
2. 导出资源增量，版本名使用新的唯一名称：

   ```powershell
   .\Team\Export-Assets.ps1 -Version "v0.18.0-kitchen" -OutputDirectory "D:\待分享\厨房更新" -EngineRoot "D:\epic\UE_5.3"
   ```

3. 把生成的新 ZIP 放到组内网盘，保留之前清单仍引用的基础包。脚本同步更新 `Team/asset-manifest.json`；将它与对应源码一起提交 PR，并在 PR 中说明资源版本及组内获取方式，不贴公开下载密码。
4. 负责人确认新包可获取、代码与资源匹配后合并。其他人拉取代码后导入新包。已有正确基础资源的人只需新增量；新组员需要清单中全部包。
5. 同时改资源清单可能冲突，由负责人整合后重新导出，不手工拼接资源哈希。清单删除记录不会自动删除磁盘旧资源；需要实际删除时由负责人确认，避免误删本地文件。

本地导入脚本会保护未分享的资源修改。遇到“Local asset edit protected”时先导出或备份本地资源，请勿直接删改清单来绕过检查。

## 打包与检查

```powershell
.\Team\Check-Assets.ps1 -EngineRoot "D:\epic\UE_5.3"
.\build_windows.ps1 -EngineRoot "D:\epic\UE_5.3"
```

运行包默认生成到本目录的 `Artifacts`，通过组内网盘分发。`Tools` 中还保留维护者的高级验收脚本，其中部分默认路径沿用维护机目录，运行前检查参数；首次使用按本 README 操作即可。

## 公开仓库边界

- 提交前执行 `python Team/check_public_files.py`，检查已跟踪文件中是否误入资源、压缩包、二进制或明显凭据。
- 原始住宅素材的授权信息由钟子杰保留，新增人物和字体的来源见 [ASSET_CREDITS.md](ASSET_CREDITS.md)。
