# WoW Model Viewer —— 项目分析与教程

> 项目地址：https://github.com/wowmodelviewer/wowmodelviewer
> 采集日期：2026-07-31

---

## 一、项目现状（重要）

| 项 | 值 |
|---|---|
| Star / Fork | 65 / 26 |
| 主分支 | `develop` |
| 提交数 | 70 commits |
| 最后更新 | **2026-07-23（上周）** |
| **正式版本** | **V.0.11.0，2026-07-05 发布** |
| 基础 | 基于 0.10.0.909 classic 重制 |

> **Releases 页**：https://github.com/wowmodelviewer/wowmodelviewer/releases
> 有编译好的 0.11.0，不用自己 build。

### 0.11.0 新增（2026-07-05，实查 changelog）

| 功能 | 说明 |
|---|---|
| **FBX Component 模式** | 导出装备部件的原始贴图 + UV2，配套 Blender 插件自动重建材质节点 |
| **图像序列导出** | PNG/JPG/EXR，带透明通道，可导入 AE / Premiere / DaVinci |
| 装备槽一键移除 | 每个槽位加 X 按钮 + Clear all |
| File -> Restart | `Ctrl+Shift+R` 一键重启，保留设置 |
| 相机自动适配 | 改用完整 3D 边界（旧版只按高度算，宽模型会被裁）|
| M2 材质渲染改进 | 加法辉光不再被压暗，alpha 抠图阈值贴近游戏 |
| 金属武器高光 | Fresnel 反射 + 随视角移动的高光 |
| FBX 导出不再卡界面 | 后台任务 + 进度窗 |

**这是一个"复活项目"** —— 老牌 WMV 停更多年，这个仓库在 2026 年重新维护，
基于 wxWidgets 版本用 VS2022 重新构建。

### 对你最关键的一点：它支持 MPQ

从提交记录看到（`ThirdParty` 目录，2026-07-23）：

> **Add StormLib-backed MPQ file provider**

以及一条专门针对 MPQ 的修复：

> **Fix legacy MPQ geoset visibility for old M2 models**
>
> Character-based creature/NPC models in a legacy MPQ client rendered with parts missing
> — e.g. `creature/band/*` (the L70ETC band: bandorcmale was bald and had no microphone;
> bandtaurenmale was missing its entire drum kit) and `creature/akama`.

**这意味着它能直接读你的 3.3.5 客户端 MPQ**，不用先解包。
而且作者专门修了老 M2 模型的显示问题 —— 对 WotLK 用户是好消息。

> **注意**：这条修复只对 `storage == MPQ` 且路径在 `creature/` 下的非角色模型生效。
> 原文：*"for storage == MPQ, non-character models under `creature/`,
> force every submesh visible"*。角色模型（`character/<race>/<sex>/*`）**不受影响**，
> 因为它们的多个 geoset 是互斥的（不同发型等），全显示会糊成一团。

### 目录结构

| 目录 | 内容 |
|---|---|
| `Source/` | 主程序源码 |
| `ThirdParty/` | CascLib、StormLib（MPQ 支持）等依赖 |
| `Installers/` | 安装包构建脚本 |
| `armory-proxy/` | 从暴雪官网导入角色的代理服务 |
| `bin_support/` | 运行时支持文件（DBC 定义等）|

### 版本 0.2.0 的更新要点（2026-06-17）

- 启动大幅加速（SQLite 索引 + 单次 CASC 枚举）
- 物品装备/搜索不再卡死
- 支持从 URL 导入 NPC（Character 菜单）
- 修复生物无贴图问题
- 修复 WMO 堆损坏崩溃
- 支持 retail 12.x 的 WMO 查看

---

## 二、它能做什么

| 功能 | 说明 |
|---|---|
| **看模型 + 动画** | 3D 预览所有生物/角色/物品模型，播放动画 |
| **找 displayid** | 这是**对服务端魔改最有用的功能** |
| **角色装扮预览** | 换装、换发型、试装备外观 |
| **导出 OBJ** | 导出到 Blender 等 3D 软件 |
| **截图** | 出图做展示 |
| **从 Armory 导入** | 输入角色 URL 直接加载该角色外观 |

---

## 三、对你项目最实用的用法：查 displayid

你之前踩过坑：

> step21 随手填 displayid 15294 → 无效 ID → 客户端静默失败 → 看不到 NPC
> step23 木桩连续三次是矮人，因为不敢猜模型 ID

**WMV 就是解决这个问题的工具。**

### 步骤

1. 打开 WMV，指向你的 3.3.5 客户端目录（`D:\WoW335\`）
2. 左侧模型树选 `Creature` 分类
3. 找到想要的模型（比如训练假人 `TrainingDummy`）
4. 界面上会显示 **Model / DisplayID** 信息
5. 把这个 ID 填进 `creature_template.modelid1`

**这样就不用再猜了** —— 看到什么样子，就是游戏里什么样子。

### 反查：已知 displayid，想看长什么样

WMV 支持按 ID 搜索。你现在库里那些 NPCBot 模板用的 displayid
（3343 / 3399 / 3431 / 1300 / 1578 / 3053 / 27541）都可以逐个看一遍，
挑合适的用。

---

## 四、安装

### 方式一：下载现成版本（推荐）

项目 Releases 页面有编译好的版本。注意选对应你客户端版本的。

### 方式二：自己编译

从提交记录看到编译要求：

> Builds on **VS2022 Build Tools** as a **32-bit (Win32)** target with **Qt 5.13.2**

**注意是 32 位**。作者还记录了两个坑：

> - CMakeLists: force `CASC_UNICODE/SHARED_LIB/TESTS` via `CACHE...FORCE`
>   (option() 2nd arg is help text, so CASC_UNICODE stayed OFF and CascLib 3.0
>   failed a UNICODE-mismatch check)
> - `particle.h`: add `#include <ostream>` (VS2022 no longer pulls it in transitively)

如果你自己编译遇到这两个错误，照着改就行。

---

## 五、使用教程

### 首次配置

1. 启动后会让你选客户端路径
2. 选到 **World of Warcraft 根目录**（含 `Data` 文件夹那层）
3. 它会自动识别是 MPQ（3.3.5）还是 CASC（新版）
4. 首次加载会建索引，耐心等

### 界面布局

```
左侧：模型树（Character / Creature / Item / Spell / ...）
中间：3D 预览窗口
右侧：属性面板（geoset 开关、贴图、动画列表）
底部：动画播放控制
```

### 常用操作

| 操作 | 方法 |
|---|---|
| 旋转 | 左键拖动 |
| 缩放 | 滚轮（0.2.0 起是距离缩放，更顺手）|
| 平移 | 右键拖动 |
| 播放动画 | 底部动画列表双击 |
| 显示/隐藏部件 | 右侧 geoset 面板打勾 |
| 导出 OBJ | File → Export → Wavefront OBJ |
| 截图 | File → Save Screenshot |

### 导出模型到 Blender

1. File → Export → Wavefront OBJ
2. 在 Blender 里 Import → Wavefront (.obj)
3. 贴图需要单独处理（BLP 要先转 PNG，见 `05-贴图与纹理.md`）

---

## 六、与 wow.export 的分工

两个工具**不冲突，是互补的**：

| | WoW Model Viewer | wow.export |
|---|---|---|
| 主要用途 | **查看** + 找 ID | **提取** 文件 |
| 客户端支持 | MPQ（3.3.5）+ CASC | 主要 CASC（新版）|
| 动画预览 | 强 | 有但较弱 |
| 批量导出 | 弱 | **强** |
| 导出格式 | OBJ | OBJ / glTF / 原始 m2/blp |
| 角色装扮 | **强** | 一般 |

**实际用法**：
- 想知道"这个 displayid 长什么样" → **WMV**
- 想从正式服搬一个模型到 3.3.5 → **wow.export**

---

## 七、已知限制

- 32 位程序，超大模型可能吃内存
- 老版本对某些 WotLK 模型有 geoset 显示问题（2026-07-23 已修，用新版）
- 导出的 OBJ 不带骨骼动画（要动画得用其他方案）
