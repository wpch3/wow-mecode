# 内容管线 —— 加大量地图/建模/图标之前必读

> 你问「加大量内容需要优化吗」。答案是**需要，而且有些必须在加内容之前做**。
> 这份文档是我核实过真实数据后的结论。

---

## 一、性能：三个会成为瓶颈的配置

我扫了你的 conf，这几项现在感觉不到，但内容多了就是第一批瓶颈：

| 配置 | 你的值 | 问题 |
|---|---|---|
| **`MapUpdate.Threads`** | **1** | **单线程跑所有地图更新**。加大量自定义地图后第一个撑不住 |
| `WorldDatabase.WorkerThreads` | **1** | 加载 DBC/物品/生物单线程 |
| `CharacterDatabase.WorkerThreads` | **1** | 玩家数据读写单线程 |
| `ThreadPool` | 2 | 网络 IO |

**已做成 `conf/worldserver.conf.d/performance.conf`**，丢进去就行。
里面有按 CPU 核数的对照表。

**为什么现在就要改**：等你加了 20 张地图再改，到时候卡顿你会怀疑是地图做错了，
其实是配置问题 —— 排查方向就跑偏了。

另外顺手调了两个：
- `Instance.UnloadDelay` 1小时 → 15分钟（副本多了内存吃不消）
- `PlayerSaveInterval` 15分钟 → 5分钟（崩溃少丢数据）

---

## 二、加内容前必须知道的四个坑

### 坑 1：MPQ 默认文件数上限只有 4000

社区资料里建 MPQ 时要手动设 max files，**默认就是 4000**。
你要加大量图标+模型+地图，一张自定义地图光 ADT 就上百个文件，很容易超。

**做法**：
- 建 MPQ 时把上限设到 **65536**
- 按类型拆包：`patch-A.MPQ`(地图) / `patch-B.MPQ`(图标) / `patch-C.MPQ`(模型)
- 加载顺序按字母排，后面覆盖前面

用 `check_patch_content.py` 打包前先数一遍。

### 坑 2：DBC 双端必须完全一致 ← 最常见的翻车点

改了客户端 DBC 忘了同步服务端，症状：
- 能进地图但坐标错乱
- 服务端认为你会某技能，客户端不显示
- 登录后直接掉线

**做法**：每次改完跑 `check_dbc.py`，它会告诉你哪个文件不一致，
以及是记录数不同还是字段数不同（后者更严重，说明结构版本都不一样）。

### 坑 3：自定义地图要提取三种数据

加进 `Map.dbc` 只是第一步，服务端还需要：

```
mapextractor.exe    → maps/     高度图（必须）
vmap4extractor.exe  → vmaps/    视线判定（必须）
mmaps_generator.exe → mmaps/    寻路，NPC走路要用
```

**mmaps 生成极慢** —— 全大陆要跑几小时到一天。

**做法**：加地图时**只提取那一张**，别每次全量重跑。
`mmaps_generator.exe <mapId>` 可以指定单张图。

### 坑 4：图标有硬格式要求

- 必须 **BLP**（不是 PNG/DDS）
- 技能/物品图标必须 **64×64**
- 放 `Interface\Icons\`
- **文件名不能有中文和空格** ← 这个最容易犯

`check_patch_content.py` 会全部帮你检出来。

---

## 三、工具用法

### check_dbc.py — DBC 双端校验

```bash
python check_dbc.py "D:/TC-Build/bin/RelWithDebInfo/dbc" "D:/临时解压的客户端dbc"
```

客户端 DBC 在 MPQ 里，要先用 MPQ Editor 解到一个临时目录。

输出示例：

```
  [不一致] 2 个文件内容不同  <<< 这是最危险的

  文件                    服务端(记录/字段)   客户端(记录/字段)
  chrraces.dbc            12 / 69            12 / 70    <<< 字段数不同！结构不兼容
  spell.dbc               50000 / 234        49999 / 234  <<< 记录数不同
```

### check_patch_content.py — 打包前预检

```bash
python check_patch_content.py "D:/MyPatch"
```

检查 7 项：文件总数、类型分布、图标格式尺寸、文件名合法性、
DBC 清单、超大文件、目录结构。

---

## 四、我建议的内容管线（重要）

你现在的做法是「想到什么做什么」。要加**大量**内容的话，
建议先把工具链跑通再批量做：

```
第 1 步  加 1 张最小的自定义地图（哪怕就是块空地）
         ├─ 跑通 Map.dbc / AreaTable.dbc 编辑
         ├─ 跑通 maps/vmaps/mmaps 提取
         ├─ 跑通 MPQ 打包
         └─ 跑通 DBC 双端同步

第 2 步  确认整个流程没问题

第 3 步  再批量加内容
```

**先跑通一次完整流程，比一次加 10 张图然后调试三天要快得多。**

---

## 五、其他建议

### 版本管理

你改动越来越多（3个patch + 5个指令 + 战斗节奏 + conf + 模块）。
建议每个功能一个分支：

```
bignum-mod              主线
├── feat/gearset        套装
├── feat/speed          战斗节奏
├── feat/aoe-loot       群体拾取
└── feat/maps           自定义地图
```

出问题能精确回滚，而不是「不知道哪个改动导致的」。

### 数据库备份

你 characters 库现在有玩家数据、套装解锁进度、刷本记录、每日奖励。
建议加个 Windows 计划任务：

```bat
mysqldump -u root -p123456 characters > D:\backup\characters_%date%.sql
mysqldump -u root -p123456 world      > D:\backup\world_%date%.sql
```

### 建包脚本化

你以后会反复打包 MPQ，手动做容易漏。建议做个 bat：

```
收集文件 → 跑 check_patch_content.py → 同步 DBC 到服务端
→ 跑 check_dbc.py → 打包 MPQ
```

需要的话我可以写。

---

## 六、加内容前建议先装的模块

| 模块 | 为什么在加内容前就要装 |
|---|---|
| **AIO**（Eluna） | **服务端推 UI 到客户端，不用打客户端补丁**。世界之魂进度条、和解度面板、剧情对话框全靠它。先装能省掉大量客户端补丁工作 |
| **GOMove** | 游戏内摆放/移动物件。做新地图时手动填 SQL 坐标会疯，这个所见即所得 |
| **`.reload item_template`** | 原版没有。调装备数值要反复改，没这个每次都得重启 |

前两个尤其重要 —— **它们是"做内容的工具"，不是"内容本身"**。
先有趁手的工具，后面效率差好几倍。
