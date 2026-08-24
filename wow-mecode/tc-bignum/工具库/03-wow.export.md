# wow.export —— 项目分析与教程

> 项目地址：https://github.com/Kruithne/wow.export
> 官网下载：https://www.kruithne.net/wow.export/
> 当前版本：**0.2.19**（2026-06-22）
> 采集日期：2026-07-31

---

## 一、项目现状

| 项 | 值 |
|---|---|
| Star / Fork | **508 / 103** |
| 提交数 | 3066 commits |
| 许可 | MIT |
| 最后更新 | 2026-06-22 |
| 技术栈 | Node.js (NW.js) |

作者自称 *"the number one export toolkit for World of Warcraft"*，
从活跃度和 star 数看，这话不夸张。**有自动更新**，装了就不用管版本。

---

## 二、【重大发现】它完整支持 3.3.5 MPQ

我原本以为它只能读新版客户端（CASC），查了 CHANGELOG 发现**不是**：

```
Implemented support for legacy MPQ-based installations (0.X - 3.X)
```

后续版本持续加强 MPQ 支持：

| 版本 | MPQ 相关更新 |
|---|---|
| 0.2.18 | "Maps" 标签支持 MPQ Legacy，可导小地图和地形 |
| — | 支持 MPQ 的**地形导出**（含 pre-baked 和 alpha map）|
| — | **"Data" 标签支持 legacy MPQ，可查看/导出 DBC 表** |
| — | 支持从 legacy MPQ 预览/导出 M2 和 WMO 模型 |
| — | "Fonts" 标签支持 CASC + MPQ |
| — | 修复 MPQ 归档导出 WMO 首个贴图丢失的问题 |

**这意味着你可以用一个工具搞定绝大部分提取工作**，不用为 3.3.5 单独找老工具。

---

## 三、功能清单（官方 README）

- 导出模型（M2 / WMO），支持 OBJ / glTF
- 导出贴图（BLP → PNG）
- **预览和导出所有音频文件**
- **预览和导出所有视频（过场动画）**
- 支持客户端全部 13 种语言的本地化
- **拖拽本地 M2/BLP 文件到程序上直接转换**
- 查看和导出游戏内区域地图
- **DB2/DBC 数据库查看器**
- 查看所有游戏内文本、界面、脚本
- 预览和导出字体文件

---

## 四、对你项目最有用的三个功能

### 1. 提取模型做 retroport（把新版模型搬到 3.3.5）

这是它的主战场。流程见 `07-模型格式转换.md`。

### 2. 直接查 DBC（不用另装 DBC 编辑器）

"Data" 标签能直接看 3.3.5 客户端里的 DBC 表。
**查数据用它，改数据还是得用 WDBX Editor**（wow.export 是只读的）。

不过查东西很方便，比如：
- 查 `CreatureDisplayInfo.dbc` 确认某个 displayid 对应什么模型
- 查 `ItemDisplayInfo.dbc` 找现成的物品外观
- 查 `Spell.dbc` 找法术视觉效果

### 3. 批量导出图标

你做自定义物品需要图标。`Interface\Icons\` 下有全部游戏图标，
wow.export 可以**批量导出成 PNG**，挑一个改改就能用。

---

## 五、安装

1. 去 https://www.kruithne.net/wow.export/ 下载
2. 解压运行，**不需要安装**
3. 有更新时程序内会提示，点一下自动完成

> GitHub Releases 页面（你给的 0.2.19 链接）也可以下，
> 但官网版本带自动更新，推荐用官网的。

---

## 六、使用教程

### 首次配置

1. 启动 → 选 "Local Installation"（本地客户端）
2. 指向你的 **3.3.5 客户端根目录**（含 `Data` 文件夹那层）
3. 它会自动识别为 **MPQ Legacy 模式**
4. 首次会建索引，等一会

> 也可以选 "Online"（从暴雪 CDN 直接拉），但那是新版客户端数据，
> 做 3.3.5 魔改时用不上，除非你要 retroport。

### 界面标签说明

| 标签 | 内容 |
|---|---|
| **Models** | M2 模型（生物、物品、法术特效）|
| **Textures** | BLP 贴图 |
| **Audio** | 音效、音乐 |
| **Video** | 过场动画 |
| **Maps** | 地图、小地图、地形 |
| **Data** | DBC/DB2 数据表 |
| **Text** | 游戏文本、Lua 脚本、XML |
| **Fonts** | 字体 |
| **Raw** | 原始文件浏览器 |
| **Install** | 安装文件清单 |

### 导出模型

1. Models 标签
2. 左侧搜索框输入名字（如 `trainingdummy`）
3. 点模型名，右侧 3D 预览
4. 底部选导出格式：
   - **OBJ** —— 通用，Blender/3DMax 都能开
   - **glTF** —— 带材质信息，更现代
   - **RAW (M2)** —— 原始文件，做 retroport 用这个
5. 点 "Export"

**导出设置**（右上角齿轮）里可以配：
- 导出目录
- 是否一并导出贴图
- 贴图格式（PNG / BLP）
- 是否用 FileDataID 命名

### 导出贴图

1. Textures 标签
2. 搜索（如 `inv_sword`）
3. 选中 → Export
4. 默认转成 **PNG**，可以直接用 Photoshop 编辑

### 拖拽转换（很方便的隐藏功能）

**把本地的 .m2 或 .blp 文件直接拖到程序窗口上**，
它会自动转换。改贴图时不用来回找菜单。

---

## 七、与 WoW Model Viewer 的分工

两个都支持 MPQ 了，分工变成：

| 需求 | 用哪个 |
|---|---|
| 找 displayid、看模型长啥样 | **WMV**（模型树按分类组织，好找）|
| 角色装扮预览、试装备 | **WMV**（专门做了装扮系统）|
| 批量导出文件 | **wow.export**（导出功能强得多）|
| 导出音频/视频/字体 | **wow.export**（WMV 没有）|
| 查 DBC 数据 | **wow.export**（有 Data 标签）|
| retroport 新版模型 | **wow.export**（能连 CDN 拉新版数据）|
| 导出地形做地图 | **wow.export**（0.2.18 起支持 MPQ 地形）|

**建议两个都装**，各有各的强项。

---

## 八、Blender 插件

项目自带 Blender 插件：

```
addons/blender/io_scene_wowobj/
```

装了之后 Blender 能直接导入 wow.export 导出的 OBJ，**自动带材质**。
比手动配材质省事很多。

> 0.2.18 修复了 "alpha map import/export for MPQ builds"，
> 3.3.5 用户导地形时会用到。

---

## 九、已知限制

- **只读**，不能改文件（改要用 MPQ Editor + WDBX Editor）
- 导出的 OBJ 不含骨骼动画
- MPQ 模式下部分新功能不可用（那些是为 CASC 设计的）
- 首次索引大客户端会比较慢
