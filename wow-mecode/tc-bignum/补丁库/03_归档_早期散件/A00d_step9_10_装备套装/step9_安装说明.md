# 第 9 步 v2：套装系统（已修复闪退）

## 一、闪退根因 —— 我的错误

你遇到的三个问题**是同一个原因**：

```cpp
// ❌ 我写的（printf 风格）
CharacterDatabase.PQuery("... WHERE owner_guid = %u AND set_name = '%s'", guid, name);

// ✅ 实际需要（fmt 风格）
CharacterDatabase.PQuery("... WHERE owner_guid = {} AND set_name = '{}'", guid, name);
```

源码里数据库接口用的是 **fmt 库**：

```cpp
// StringFormat.h:28
using FormatString = fmt::format_string<Args...>;
```

`fmt::format` 遇到 `%u` **无法识别为占位符** → 参数数量不匹配 → **抛异常 → 崩服**。

这解释了：
| 现象 | 原因 |
|---|---|
| `.gearset list` 闪退 | PQuery 用了 `%u` |
| `.gearset load` 无菜单 | 同一个 PQuery 崩了 |
| `.gearset save` 看似成功 | PExecute 也崩，但在异步线程 |

**已修复全部 5 处 + 1 处 StringFormat。**

---

## 二、装等改进（按你的要求）

### 从 8 档扩展到 17 档，覆盖 1 级到超凡内容

```
装等 <= 20     (1-20级新手)
装等 <= 40     (20-40级)
装等 <= 60     (40-60级)
装等 <= 90     (60级 T1-T2)
装等 <= 120    (60级 T3)
装等 <= 140    (TBC 前期)
装等 <= 164    (TBC T5)
装等 <= 200    (WLK 入门)
装等 <= 219    (纳克萨玛斯)
装等 <= 232    (奥杜尔)
装等 <= 245    (十字军)
装等 <= 264    (ICC 25H)
装等 <= 284    (ICC 传说)
装等 <= 500    (自定义内容)
装等 <= 999    (自定义高级)
装等 <= 9999   (超凡内容)
不限装等       (含大数值装备)
```

**向下兼容**：`装等 <= N` 是上限，低于 N 的装备也会参与筛选，
所以低级角色能正常拿到低级装备。

---

## 三、新功能：按副本自动配装 ⭐

这是你提的想法，我做进去了。

### 用法

```
.gearset auto              按当前位置自动配装
.gearset auto tank         同上，指定定位
.gearset here              auto 的别名
```

主菜单第一项也是它：
```
[推荐] 按当前位置自动配装  (团本(英雄) / 装等264)
```

### 实测：副本 → 推荐装等

```
野外(80级)      -> 装等 232
冰冠堡垒 25H    -> 装等 264
冰冠堡垒 普通   -> 装等 251
奥杜尔 英雄     -> 装等 232
纳克萨玛斯      -> 装等 213
太阳井          -> 装等 164
熔火之心        -> 装等 71
魔枢 英雄       -> 装等 200
魔枢 普通       -> 装等 175
未知副本        -> 按等级推算
```

### 等级推算（副本未收录时的兜底）

```
20级 -> 25     40级 -> 45     60级 -> 92
70级 -> 146    80级 -> 232
```

**等级 255 改造后自动适配**：
```cpp
// 80级以上线性放大
return uint32(232 + (lvl - 80) * 20);
// 100级 -> 632    255级 -> 3732
```

不用改代码就能支持你以后的高等级内容。

---

## 一、指令一览

### 生成套装

```
.gearset                        打开可点击主菜单（推荐）
.gearset auto                   ★按当前副本/等级自动配装
.gearset auto tank              ★同上，指定定位
.gearset 战士 264               直接生成：战士 / 装等<=264
.gearset 圣骑士 264 tank        指定定位：坦克
.gearset 法师 999 heal          治疗向
.gearset dk 0 dps               装等 0 = 不限（会给到你的大数值装备）
```

**职业名支持中英文和简称**：
```
战士/warrior      圣骑士/paladin/骑士    猎人/hunter
潜行者/rogue/盗贼  牧师/priest           死亡骑士/dk
萨满/shaman       法师/mage              术士/warlock
德鲁伊/druid/小德
```

**定位**：`tank/坦克` `dps/输出` `heal/治疗`，不填=通用

### 方案管理

```
.gearset save 我的坦克配装      保存当前全身装备为方案
.gearset load                   弹菜单选择方案（支持分页）
.gearset load 我的坦克配装      直接加载
.gearset list                   列出所有方案
.gearset del 我的坦克配装       删除方案
.gearset strip                  卸下全身装备到背包
```

---

## 二、你最关心的：不会覆盖手动装备

**设计上做了保护**：

```cpp
// 只发放到背包，不自动穿戴
handler->SendSysMessage("装备已放入背包，请手动穿戴（避免覆盖你现有装备）");
```

生成的套装**全部进背包**，你自己决定穿哪件。
`.gearset strip` 也是**卸到背包**而非销毁。

---

## 三、智能选装逻辑

### 按职业自动选护甲类型（已实测）

```
战士/圣骑士/DK    -> 板甲
猎人/萨满         -> 锁甲
潜行者/德鲁伊     -> 皮甲
牧师/法师/术士    -> 布甲
```

首饰类（项链/戒指/饰品/披风）不受限制。

### 按定位加权评分

| 定位 | 优先属性 |
|---|---|
| **坦克** | 耐力×3、防御/闪避/招架/格挡×2.5、护甲×0.5 |
| **治疗** | 智力×3、精神×2.5、法强×2、法力回复×2 |
| **输出** | 主属性×2.5、爆击/急速×2、攻强/法强×1.5 |

### 覆盖 14 个部位

```
头 颈 肩 背 胸 腕 手 腰 腿 脚 + 戒指×2 + 饰品×2
```

戒指饰品会挑**两件不同的**，不会给重复。

---

## 四、实测结果

```
=========== 战士/坦克/装等<=260 ===========
  [头部] 板甲头盔_v2 (装等260)      <- 正确选板甲
  [颈部] 项链_v2 (装等260)
  [背部] 披风_v2 (装等260)
  [胸甲] 板甲胸铠_v2 (装等260)
  [腿部] 板甲腿铠_v2 (装等260)
  [戒指] 戒指_v2 (装等260)
  [戒指] 戒指_v1 (装等230)          <- 两件不同
  [饰品] 饰品_v2 / 饰品_v1
完成：获得 9 件

=========== 法师/治疗/不限装等 ===========
  [头部] 布甲兜帽_v2                 <- 正确选布甲
  [胸甲] 布甲长袍_v2
...

=========== 职业名解析 ===========
  战士 -> 战士        dk   -> 死亡骑士
  小德 -> 德鲁伊      不存在 -> 未知

=========== 护甲类型映射 ===========
  战士 -> 板甲    猎人 -> 锁甲    潜行者 -> 皮甲    法师 -> 布甲
```

> 测试里的"跳过"是因为我只造了部分部位的测试数据，
> 你的库有 38000+ 件物品，实际不会缺。

---

## 五、安装（5 步）

### 1. 添加源文件

`step9_cs_gearset.cpp` → `D:\TrinityCore\src\server\scripts\Commands\cs_gearset.cpp`

（改名去掉 `step9_` 前缀）

### 2. 注册脚本

`cs_script_loader.cpp` 里，在 `AddSC_smartadd_commandscript();` 附近加：

**声明区**（约47行，和其他 void 声明放一起）：
```cpp
void AddSC_gearset_commandscript();
```

**调用区**（约93行）：
```cpp
    AddSC_gearset_commandscript();
```

### 3. 添加 RBAC 权限

`RBAC.h` 里，在你已有的 71004 后面加一行：

```cpp
    RBAC_PERM_COMMAND_MODIFY_ALLSTATS                        = 71001,
    RBAC_PERM_COMMAND_MODIFY_STAT                            = 71002,
    RBAC_PERM_COMMAND_SMART_ADD                              = 71003,
    RBAC_PERM_COMMAND_SMART_SPAWN                            = 71004,
    RBAC_PERM_COMMAND_GEARSET                                = 71005,

    RBAC_PERM_MAX
};
```

⚠️ **老规矩：别重复 `RBAC_PERM_MAX` 和 `};`**

自查：
```bash
grep -c "RBAC_PERM_MAX" /d/TrinityCore/src/server/game/Accounts/RBAC.h
grep -c "enum RBACCommandResult" /d/TrinityCore/src/server/game/Accounts/RBAC.h
```
两条都必须是 **1**。

### 4. 建表 + 注册权限（4 条 SQL，逐条执行）

**第1条 — 建表**（`sql/04_gearset_table.sql`）
```sql
CREATE TABLE IF NOT EXISTS characters.custom_gearset (owner_guid int unsigned NOT NULL, set_name varchar(64) NOT NULL, slot tinyint unsigned NOT NULL, item_entry int unsigned NOT NULL, PRIMARY KEY (owner_guid, set_name, slot), KEY idx_owner (owner_guid)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

**第2条**
```sql
INSERT INTO auth.rbac_permissions (id, name) VALUES (71005, 'Command: gearset');
```

**第3条**
```sql
INSERT INTO auth.rbac_linked_permissions (id, linkedId) VALUES (192, 71005);
```

**第4条 — 验证**
```sql
SELECT id, name FROM auth.rbac_permissions WHERE id = 71005;
```

### 5. 重新运行 CMake（重要）

**新增了源文件，必须重跑 CMake**：

```
CMake GUI → Configure → Generate
```

然后 VS 编译，**重启 worldserver**（RBAC 权限需要重启加载）。

---

## 六、测试清单

```
.gearset                        打开菜单，逐级点击：职业→定位→装等
.gearset 战士 264               直接生成，应全是板甲
.gearset 法师 264               应全是布甲
.gearset 圣骑士 264 tank        坦克向，耐力优先
.gearset 牧师 264 heal          治疗向，智力精神优先
.gearset 0 0                    应提示用法（职业名无效）

.gearset save 测试方案          保存当前装备
.gearset list                   应列出「测试方案」
.gearset load                   弹菜单
.gearset load 测试方案          直接加载
.gearset del 测试方案           删除
.gearset strip                  卸下全身
```

**重点验证**：
- 生成的装备**进背包而不是自动穿上**
- 职业护甲类型正确
- `.gearset 战士 0`（不限装等）能拿到你的大数值装备

---

## 七、已知限制

| 项 | 说明 |
|---|---|
| 不含武器 | 武器种类太多（剑/斧/法杖/双持），单独做更合理 |
| 不自动穿戴 | 有意设计，保护你手动配的装备 |
| 遍历全表 | 38000 件物品，生成一次约几十毫秒，可接受 |
| 方案按角色存 | 不同角色的方案独立（owner_guid 区分） |

---

## 八、下一步可以做

- `.gearset weapon 战士 264` — 武器专项
- `.gearset bot 战士 264` — 给选中的 NPCBot 配装
- `.gearset equip` — 生成后自动穿戴（可选开关）

有编译错误发我**第一条 error**。
