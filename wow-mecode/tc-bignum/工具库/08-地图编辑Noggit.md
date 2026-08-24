# Noggit —— 地图/地形编辑器

> **【勘误 2026-07-31】本篇初版写的仓库地址 `github.com/wowdev/noggit-red`
> 是 404，不存在。正确地址在 GitLab，用户指出的：**
>
> ## https://gitlab.com/prophecy-rp/noggit-red
>
> 老版本：Noggit 3 / Noggit SDL（均已停更）
> 用途：编辑 3.3.5 的地形、放置模型、改水面

---

## 零、项目现状（2026-07-31 实查）

| 项 | 情况 |
|---|---|
| 主仓库 | **GitLab** `prophecy-rp/noggit-red`，不是 GitHub |
| 最近提交 | **1 个月前**（README 更新）|
| 代码最近改动 | **2 个月前**（`src/` 修复 alphamap 批量导入）|
| 分支 | `master` + `noggit-pipeline-rework` |
| 许可 | GPL3 |
| 维护者 | T1ti |
| **预编译版下载** | **只在 Discord 发**：https://discord.gg/NqvM3xE5uS |

**结论：项目活着，而且在动。** 但发布方式很社区化——
GitLab 只放源码，**要现成 exe 得进 Discord**。

### 近期提交里能看出的功能方向

| 提交 | 说明 |
|---|---|
| Render map occluders + 批量 alphamap 导入优化 | 渲染和批处理性能 |
| AreaTrigger Tool | 可视化编辑区域触发器 |
| port from mysql connector to QTSQL | **直连数据库**，改用 Qt SQL 模块 |
| project system | 工程化管理 |

**注意那条 MySQL 的**：Noggit Red 能直连你的 world 库
（用于 GUID 存储同步）。CMake 里 `mysql required to build`，
也就是**编译时 MySQL 是必需依赖**。

---

## 零点五、自己编译的依赖清单（README 实录）

如果 Discord 拿不到预编译版，或者你想自己编：

```
OpenGL
StormLib   (Ladislav Zezula -- 就是 MPQ Editor 作者那个库)
CascLib    (同作者)
Qt5
Lua 5.x
LibMySQL + MySQLCPPConn   (MySQL GUID 存储需要)
```

**Windows 上只需自己装 Qt5**，其余依赖 CMake 的 FetchContent 自动拉。

关键 CMake 变量：

| 变量 | 填什么 |
|---|---|
| `CMAKE_PREFIX_PATH` | Qt 路径，如 `C:/Qt/5.15/msvc2019_64` |
| `MYSQL_ROOT` | 如 `C:\Program Files\MySQL\MySQL Server 8.0` |
| `MYSQLCPPCONN_ROOT` | MySQL Connector C++ 目录 |
| `CMAKE_INSTALL_PREFIX` | 空目录 |

> **坑**：新版 MySQL Connector 安装时默认**不含开发组件**，
> 要在安装器里手动勾 `Legacy JDBC API -> Development Components`，
> 否则找不到 `cppconn/driver.h`。

编完把 Qt DLL（Qt5Core / Qt5OpenGL / Qt5Widgets / Qt5Gui）
从 `C:/Qt/X.X/msvcXXXX/bin` 拷到 noggit.exe 旁边。

> 你有 VS2022 + CMake GUI，环境是齐的。但这是**一整套新依赖链**
> （Qt5 光下载就几个 G），不建议在剧情工具主线没完成前碰。

---

## 一、它能做什么

| 功能 | 说明 |
|---|---|
| **地形塑形** | 抬高/降低/平滑地面 |
| **地表纹理** | 刷草地、石头、雪地等贴图 |
| **放置模型** | 摆树、建筑、石头（M2 和 WMO）|
| **水面编辑** | 加水、改水位 |
| **顶点着色** | 给地面染色 |
| **区域划分** | 配合 AreaTable.dbc 分区 |
| **新建地图** | 从零做一张自定义地图 |

**这是做"自定义大陆/副本"的核心工具。**

---

## 二、Noggit Red vs 老版本

| | Noggit Red | Noggit 3 |
|---|---|---|
| 仓库 | **gitlab.com/prophecy-rp/noggit-red** | 各种停更 fork |
| 维护状态 | **活跃**（2 个月内有代码提交）| 停更 |
| 界面 | Qt5，现代 | 老旧 |
| 功能 | 更全，有脚本系统 + MySQL 直连 | 基础 |
| 稳定性 | 较好 | 容易崩 |
| 推荐 | [OK] | 仅作备选 |

**用 Noggit Red。**

> 网上还能搜到 wowmodding.net 上一个 "Noggit Red" 归档版，
> 那个是**私有版本的存档快照**（2021），页面自己写着
> "Use at your own risk. It may break your map"。
> **不要用那个**，去 GitLab 拿主线。

---

## 三、前置条件（重要）

Noggit 编辑的是 **ADT 文件**（地形分块），它需要：

1. **解包好的客户端数据** —— 不能直接读 MPQ，要先用 MPQ Editor
   把 `World\Maps\` 解出来
2. **完整的 listfile**
3. **DBC 文件** —— `Map.dbc`、`AreaTable.dbc` 等

### 目录结构

Noggit 要求一个"工作目录"，结构和客户端一致：

```
<工作目录>/
├── World/
│   └── Maps/
│       └── Azeroth/
│           ├── Azeroth_32_48.adt
│           └── ...
├── DBFilesClient/
│   ├── Map.dbc
│   └── AreaTable.dbc
└── ...（模型、贴图等）
```

---

## 四、基本流程

### 改现有地图

1. MPQ Editor 解出 `World\Maps\<地图名>\` 到工作目录
2. 解出需要的 DBC 到 `DBFilesClient\`
3. Noggit 打开工作目录，选地图
4. 编辑
5. 保存（生成新的 ADT）
6. 打包 ADT 进 `patch-4.MPQ`
7. **服务端也要更新** —— 重新提取 maps/vmaps/mmaps（见下）

### 【关键】服务端必须同步

改了地形，服务端的碰撞数据也要重新生成，否则：
- 玩家会浮空 / 陷地
- 寻路失效
- BOSS 走不动

**重新提取流程**：

```
1. 把改好的 patch-4.MPQ 放进客户端 Data
2. 用 TrinityCore 的提取工具：
   mapextractor.exe      → maps/
   vmap4extractor.exe    → Buildings/
   vmap4assembler.exe    → vmaps/
   mmaps_generator.exe   → mmaps/    （很慢，几小时）
3. 覆盖服务端的 maps/vmaps/mmaps 目录
4. 重启服务端
```

**mmaps 生成非常慢**，全地图要几小时。只改一小块的话可以只生成那张图。

---

## 五、学习曲线警告

Noggit **是这批工具里最难的**：

- 界面复杂，快捷键多
- 容易改坏（改坏了整块地形要重来）
- 崩溃频率高，**要经常保存**
- 服务端同步麻烦

**建议**：
1. 先在**测试地图**上练手，别动主城
2. 每次编辑前**备份 ADT**
3. 先做小改动（放几棵树），熟悉了再做大的

---

## 六、常见用途排序（按难度）

| 难度 | 用途 | 说明 |
|---|---|---|
| 易 | 放置模型 | 摆点建筑装饰，改动小 |
| 易 | 改地表贴图 | 换草地为雪地之类 |
| 中 | 地形塑形 | 挖坑、造山 |
| 中 | 水面编辑 | 加个湖 |
| **难** | **新建地图** | 要配 Map.dbc、WDT、全套 ADT |
| **难** | 大规模改造 | 服务端同步工作量大 |

---

## 七、对你项目的建议

你现在的重心是**服务端玩法**（指令、bot、剧情工具），
地图编辑属于**内容创作**层面。

**我的判断**：

| 场景 | 建议 |
|---|---|
| 想做自定义副本 | **暂缓** —— 用现成副本改配置更划算 |
| 想做剧情场景 | **用 `.scene` + `.nst`** —— 你已经有这两个工具了 |
| 想加装饰物 | 可以考虑，但 `.gobject add` 更简单 |
| 想做全新大陆 | 长期项目，等主线稳定后再说 |

**关键点**：你做的 `.scene` 场景快照，
其实覆盖了**大部分"摆场景"的需求**，而且不用改客户端文件、不用重提 mmaps。

真要用 Noggit，我建议等**客户端改造批次**统一做——
那时候反正要改客户端，一起处理。

---

## 八、替代方案

| 需求 | 不用 Noggit 的做法 |
|---|---|
| 摆 NPC/物件 | `.npc add` / `.gobject add` + `.scene` 保存 |
| 做剧情场景 | `.scene` + `.nst` + 后续的 `.emote`/`.say` |
| 自定义副本 | 复用现成副本地图，改 `instance_template` |
| 加建筑 | `.gobject add` 放 WMO（不改地形）|

**这些都不需要改客户端**，成本低得多。
