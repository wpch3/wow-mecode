# 第 7 步：智能添加指令（.add / .spawn / .clean）

## 先说一个你可能不知道的

`.additem` **本来就支持中文名**，只是要求**精确完全匹配**：

```
.additem [埃辛诺斯战刃]        <- 必须一字不差，且要加方括号
```

源码里 `HandleAddItemCommand` 会先查 `item_template.name`，
查不到再查 `item_template_locale.Name`（中文表）。

**但两个硬伤**：
1. `WHERE name = ?` 是精确匹配，差一个字就找不到
2. `.npc add` **完全不支持名字**，只认数字 ID

所以本次做的增强版解决这两点，并加上批量。

---

## 新增指令一览

### 物品

| 指令 | 效果 |
|---|---|
| `.add 埃辛诺斯` | 模糊搜索，唯一匹配→直接给；多个→列可点击候选 |
| `.add 埃辛诺斯战刃 5` | 带数量 |
| `.add 火焰之击, 霜之哀伤, 灰烬使者` | **逗号分隔批量** |
| `.add! 战刃` | **强制模式**：所有匹配项全给 |
| `.add last` | 重复上次添加 |

### 生物

| 指令 | 效果 |
|---|---|
| `.spawn 石爪豺狼人` | 模糊搜索并召唤 |
| `.spawn 石爪豺狼人 x5` | **一次刷 5 只**（上限 50，环绕站位） |
| `.spawn 熊, 狼, 豺狼人` | **逗号分隔批量** |
| `.spawn! 豺狼人` | **强制模式**：所有匹配项各刷一只 |
| `.spawn last` | 重复上次召唤 |
| `.clean [半径]` | **只清理你自己刷的**，默认 30 码 |

> `.clean` 会记录你召唤过的 GUID，**不会误删原生 NPC**。

---

## 已验证的边界情况

我把三个解析函数单独编译测试过，全部正确：

```
=== 逗号拆分 ===
  "火焰之击, 霜之哀伤, 灰烬使者"  -> 3项 [火焰之击][霜之哀伤][灰烬使者]
  "熊,狼,  豺狼人"                -> 3项（多余空格已清理）
  "带空格 的名字, 另一个"          -> 2项（名字内部空格保留）

=== .add 数量解析 ===
  "埃辛诺斯战刃 5"      -> 名称=[埃辛诺斯战刃] 数量=5
  "物品名带 2 个空格"    -> 名称=[物品名带 2 个空格] 数量=1   <- 不误判
  "测试 0"              -> 数量=1                            <- 0修正为1

=== .spawn xN 解析 ===
  "石爪豺狼人 x5"   -> 数量=5
  "熊 X10"          -> 数量=10        <- 大小写都认
  "怪物 x999"       -> 数量=50        <- 上限保护
  "名字里有x 的怪"   -> 数量=1         <- 不误判
```

---

## 安装步骤

### 1. 添加源文件

把 **`step7_cs_smartadd.cpp`** 复制到：

```
D:\TrinityCore\src\server\scripts\Commands\cs_smartadd.cpp
```

**注意改名**：去掉 `step7_` 前缀，文件名必须是 `cs_smartadd.cpp`。

### 2. 注册脚本

打开 `D:\TrinityCore\src\server\scripts\Commands\cs_script_loader.cpp`

找到这一行（约第 47 行）：

```cpp
void AddSC_modify_commandscript();
```

在它**下面**加一行：

```cpp
void AddSC_smartadd_commandscript();
```

然后找到（约第 93 行）：

```cpp
    AddSC_modify_commandscript();
```

在它**下面**加一行：

```cpp
    AddSC_smartadd_commandscript();
```

### 3. 添加 RBAC 权限

打开 `D:\TrinityCore\src\server\game\Accounts\RBAC.h`

在你上次加的两行**后面**追加两行（注意**不要**重复 `RBAC_PERM_MAX` 和 `};`）：

```cpp
    // 自定义指令权限 71001+
    RBAC_PERM_COMMAND_MODIFY_ALLSTATS                        = 71001,
    RBAC_PERM_COMMAND_MODIFY_STAT                            = 71002,
    RBAC_PERM_COMMAND_SMART_ADD                              = 71003,
    RBAC_PERM_COMMAND_SMART_SPAWN                            = 71004,

    RBAC_PERM_MAX
};
```

**改完自查**（两条都必须输出 1）：

```bash
grep -c "RBAC_PERM_MAX" /d/TrinityCore/src/server/game/Accounts/RBAC.h
grep -c "enum RBACCommandResult" /d/TrinityCore/src/server/game/Accounts/RBAC.h
```

### 4. 重新运行 CMake（重要）

**新增了源文件，必须重跑 CMake**，否则 VS 工程里没有这个文件。

原因和第 2 步一样：源文件是 `file(GLOB ...)` 扫描的，列表在生成时固定。

```
CMake GUI -> Configure -> Generate
```

（路径不用改，直接点两个按钮即可）

### 5. 编译

```
VS 2022 -> RelWithDebInfo + x64 -> 生成解决方案
```

### 6. 注册权限到数据库

逐条执行（你的客户端只跑光标所在语句）：

```sql
INSERT INTO auth.rbac_permissions (id, name) VALUES (71003, 'Command: smart add');
```

```sql
INSERT INTO auth.rbac_permissions (id, name) VALUES (71004, 'Command: smart spawn');
```

```sql
INSERT INTO auth.rbac_linked_permissions (id, linkedId) VALUES (192, 71003);
```

```sql
INSERT INTO auth.rbac_linked_permissions (id, linkedId) VALUES (192, 71004);
```

### 7. 重启 worldserver

RBAC 权限是启动时加载的，必须重启（`.reload` 无效）。

---

## 测试清单

```
.add 埃辛诺斯                     应列出候选或直接给
.add 埃辛诺斯战刃 3               带数量
.add 火焰, 霜之, 灰烬             批量，逐项显示结果
.add! 战刃                        全部匹配项都给
.add last                         重复上次

.spawn 豺狼人                     列候选或直接刷
.spawn 石爪豺狼人 x5              刷5只，环绕站位
.spawn 熊, 狼                     批量刷
.clean                            清30码内自己刷的
.clean 100                        清100码内
```

---

## 设计说明

**为什么 `.clean` 只清自己刷的**

脚本用 `s_mySummons[accountId]` 记录你召唤过的每个 GUID，
清理时只遍历这个列表。原生 NPC、别人刷的怪都不受影响。

> 注意：记录存在内存里，**重启服务端后清空**。
> 重启前刷的怪需要手动 `.die` 或用原版 `.npc delete`。

**安全上限**

| 项 | 上限 | 原因 |
|---|---|---|
| 单次给物品数量 | 1000 | 防手滑 |
| 单次召唤数量 | 50 | 防卡服 |
| 搜索结果条数 | 30 | 防刷屏 |

---

## 完成后告诉我

测试通过后，我们做**第 2 项：套装系统**。

按你的想法会做成：
```
.gearset 战士 264              给自己整套战士装
.gearset bot 圣骑士 264 tank   给选中的bot整套坦克装
.gearset save 我的配装          保存当前全身装备为方案
.gearset load 我的配装          一键还原
```

有编译错误把**第一条 error** 发我。
