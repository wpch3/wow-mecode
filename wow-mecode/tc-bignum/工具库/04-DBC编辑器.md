# WDBX Editor —— DBC 数据表编辑

> 作者：barncastle
> 下载：https://www.wowmodding.net/files/file/74-wdbx-editor/
> 源码：WoWDevTools GitHub
> 版本：1.1.9a
> 依赖：**.NET 4.6.1**

---

## 一、DBC 是什么，为什么必须改它

**DBC = 客户端的数据表**，等价于服务端的 MySQL 表。

关键点：**很多东西服务端和客户端都要有一份，两边必须对上**。

| 你想做的事 | 服务端改哪 | 客户端改哪（DBC）|
|---|---|---|
| 加自定义物品 | `item_template` | `Item.dbc` + `ItemDisplayInfo.dbc` |
| 自定义物品外观 | — | `ItemDisplayInfo.dbc` |
| 加自定义生物模型 | `creature_template.modelid1` | `CreatureDisplayInfo.dbc` + `CreatureModelData.dbc` |
| 自定义法术 | `spell_dbc`（TC 的表）| `Spell.dbc` |
| 换坐骑模型 | — | `CreatureDisplayInfo.dbc` |
| 加地图/区域 | — | `Map.dbc` / `AreaTable.dbc` |

**这就是为什么你之前"服务端造了装备但游戏里显示红问号"** ——
`item_template` 有了，但客户端 `Item.dbc` 没有这个 entry。

---

## 二、WDBX Editor 的能力

官方特性列表：

- 支持**所有正式版本**的 DBC / DB2 / WDB / ADB
- 可设为默认文件关联
- **同时打开多个文件**，不限类型和版本
- **能直接从 MPQ 归档和 CASC 目录打开 DBC**（不用先解包）
- 保存单个 / 批量保存
- 标准增删改查 + 跳转、复制行、粘贴行、撤销、重做
- 隐藏/显示/排序列
- 较强的列过滤系统（类似布尔搜索）
- 十六进制显示和编辑（数值列）
- **导出到 SQL 数据库 / SQL 文件 / CSV / MPQ 归档**
- **从 SQL 数据库和 CSV 导入**
- Excel 风格的查找替换

### 自带工具

- **Definition editor** —— 维护列定义
- **WotLK Item Import** —— **专门解决自定义物品的红问号问题**
- WDB5 Parser

> **WotLK Item Import 对你特别有用**。你做了自造装备 entry 800000-899999，
> 如果游戏里显示红问号，用这个工具能从服务端 `item_template` 批量
> 导入到 `Item.dbc`，一次性解决。

---

## 三、安装

1. 装 **.NET Framework 4.6.1**
2. 下载解压 WDBX Editor
3. 直接运行

---

## 四、基本操作

### 打开 DBC

**方式一：从服务端 dbc 目录**（推荐）

TrinityCore 有一份提取好的 DBC，通常在：
```
D:\TC-Build\bin\RelWithDebInfo\dbc\
```

**方式二：从客户端 MPQ 直接开**

File → Open from MPQ → 选 `patch-3.MPQ` 等 → 找 `DBFilesClient\xxx.dbc`

### 【必做】选对 Definition

打开 DBC 后会让你选版本定义，**必须选 `WOTLK 3.3.5 (12340)`**。

选错了列会全部错位，改出来的数据是废的。

### 加一行数据

1. 右键 → **Insert Line**
2. 填 ID（**必须唯一**，建议用大号段避免冲突）
3. 逐列填值
4. File → Save

### 查找

Ctrl+F，Excel 风格。也可以用列过滤器做复杂筛选。

---

## 五、实战：给自定义武器加外观

以论坛实战帖的流程为例（`ItemDisplayInfo.dbc`）：

1. 打开 `ItemDisplayInfo.dbc`，选 WOTLK 定义
2. 右键 → Insert Line
3. 填：

| 列 | 值 | 说明 |
|---|---|---|
| `ID` | 100050 | 唯一，自己定 |
| `ModelName_1` | `sword_2h_xxx.mdx` | **注意是 .mdx 不是 .m2** |
| `ModelTexture_1` | `sword_2h_xxx_red` | 贴图名，**不带扩展名** |
| `InventoryIcon` | `inv_sword_2h_xxx` | 图标名，不带扩展名 |
| 其余 | 0 或留空 | |

4. 保存到**服务端 dbc 目录**
5. 同时打包进 `patch-4.MPQ/DBFilesClient/`
6. HeidiSQL 里改 `item_template.displayid = 100050`
7. 重启服务端 + 删客户端 Cache

### 关键细节（很多人栽这）

- **`ModelName` 要写 `.mdx`**，即使你的文件是 `.m2`。这是历史遗留。
- **贴图名不带扩展名**，不要写 `.blp`
- **服务端和客户端的 DBC 必须是同一份**。改完要同时更新两边。

---

## 六、DBC 必须两边同步

这是最容易出错的地方：

```
服务端 dbc 目录          客户端 patch-4.MPQ/DBFilesClient/
D:\TC-Build\...\dbc\  ←→  ItemDisplayInfo.dbc
```

**两边不一致的后果**：
- 服务端有、客户端没有 → 红问号 / 看不见 / 崩溃
- 客户端有、服务端没有 → 服务端不认这个 ID

**建议做法**：改完直接从服务端 dbc 目录**拖进 MPQ**，保证是同一份文件。

---

## 七、常用 DBC 速查

| 文件 | 内容 | 常见用途 |
|---|---|---|
| `Item.dbc` | 物品基础（3.3.5 里较简单）| 自定义物品必需 |
| `ItemDisplayInfo.dbc` | 物品外观 | 自定义武器/装备外观 |
| `CreatureDisplayInfo.dbc` | 生物外观 | **查/改 displayid** |
| `CreatureModelData.dbc` | 生物模型文件路径 | 加自定义模型 |
| `Spell.dbc` | 法术 | 自定义法术、换视觉 |
| `SpellIcon.dbc` | 法术图标 | |
| `SpellVisual*.dbc` | 法术视觉效果 | 换特效 |
| `Map.dbc` | 地图 | 自定义地图 |
| `AreaTable.dbc` | 区域 | 自定义区域 |
| `ChrRaces.dbc` | 种族 | 自定义种族 |
| `CharStartOutfit.dbc` | 初始装备 | |
| `GameObjectDisplayInfo.dbc` | 物件外观 | |

---

## 八、替代工具

| 工具 | 评价 |
|---|---|
| **MyDBCEditor** | 老牌，界面简单，搜索功能弱。论坛老教程常用它 |
| **wow.export 的 Data 标签** | **只读**，查数据方便，不能改 |
| **TSWoW** | 用 TypeScript 写代码生成 DBC，见 `09-一体化框架.md` |

**建议用 WDBX Editor**，功能最全，特别是 SQL 导入导出和 WotLK Item Import。

---

## 九、注意事项

- 所有数据**存在内存**，一次开太多文件会崩（作者自己写在说明里）
- 撤销/重做历史在切换文件时丢失
- SQL 导入要求列**完全匹配**定义
- 改之前**备份**，DBC 改坏了游戏会崩溃或黑屏
