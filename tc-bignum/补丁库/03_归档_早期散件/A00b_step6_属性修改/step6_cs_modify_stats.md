# 第 6 步补丁：五维直改指令（cs_modify.cpp）

本文件是**手工改代码指南**。因为 `cs_modify.cpp` 用的是**旧框架**
（`std::vector<ChatCommand>`），网上教程多是新框架写法，抄了编译不过。

改动文件：`D:\TrinityCore\src\server\scripts\Commands\cs_modify.cpp`

---

## 改动 1：在命令表里注册新指令

找到 `modifyCommandTable`（约第 58 行），在 `{ "arenapoints", ... }` **之前**插入两行：

```cpp
        static std::vector<ChatCommand> modifyCommandTable =
        {
            { "allstats",     rbac::RBAC_PERM_COMMAND_MODIFY_ALLSTATS,     false, &HandleModifyAllStatsCommand,      "" },   // 新增
            { "stat",         rbac::RBAC_PERM_COMMAND_MODIFY_STAT,         false, &HandleModifyStatCommand,          "" },   // 新增
            { "arenapoints",  rbac::RBAC_PERM_COMMAND_MODIFY_ARENAPOINTS,  false, &HandleModifyArenaCommand,         "" },
```

> 注意旧框架字段顺序是：
> `{ 命令名, 权限, 是否允许控制台, 函数指针, 帮助文本 }`
> 和新框架 `{ 命令名, 函数, 权限, Console::No }` 完全不同。

---

## 改动 2：添加两个 handler 函数

在文件里找到 `HandleModifyScaleCommand`（任意一个 handler 都行），
在它**上面或下面**贴入以下完整代码：

```cpp
    // ================= 自定义：五维直改 =================
    // .modify stat <str|agi|sta|int|spi|reset> <数值>
    // .modify allstats <数值>
    //
    // 说明：用 TOTAL_VALUE 而非 BASE_VALUE，这样不会污染角色的基础成长，
    //       reset 时能干净地清零，也不影响升级时的属性重算。

    static bool ModifyOneStat(ChatHandler* handler, Player* target, Stats stat, int32 amount)
    {
        UnitMods unitMod = UnitMods(UNIT_MOD_STAT_START + AsUnderlyingType(stat));

        // 先清掉之前用本指令加的量（存在 TOTAL_VALUE 里）
        target->SetStatFlatModifier(unitMod, TOTAL_VALUE, float(amount));
        target->UpdateStats(stat);
        return true;
    }

    static bool HandleModifyStatCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .modify stat <str|agi|sta|int|spi|all|reset> <数值>");
            handler->SendSysMessage("示例: .modify stat sta 100000000   (耐力1亿)");
            handler->SendSysMessage("注意: 耐力安全上限约 4.2 亿，超过会导致血量溢出归零");
            handler->SetSentErrorMessage(true);
            return false;
        }

        char* statStr = strtok((char*)args, " ");
        char* valStr  = strtok(nullptr, " ");

        if (!statStr)
        {
            handler->SendSysMessage("用法: .modify stat <str|agi|sta|int|spi|all|reset> <数值>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (handler->HasLowerSecurity(target, ObjectGuid::Empty))
            return false;

        std::string statName = statStr;
        std::transform(statName.begin(), statName.end(), statName.begin(), ::tolower);

        // reset：清空所有五维的自定义加成
        if (statName == "reset")
        {
            for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
            {
                UnitMods um = UnitMods(UNIT_MOD_STAT_START + i);
                target->SetStatFlatModifier(um, TOTAL_VALUE, 0.0f);
                target->UpdateStats(Stats(i));
            }
            target->UpdateAllStats();
            handler->PSendSysMessage("已重置 %s 的五维加成。", handler->GetNameLink(target).c_str());
            return true;
        }

        if (!valStr)
        {
            handler->SendSysMessage("缺少数值参数。用法: .modify stat sta 100000000");
            handler->SetSentErrorMessage(true);
            return false;
        }

        int32 amount = atoi(valStr);

        // 安全提示：耐力过大导致血量溢出
        if ((statName == "sta" || statName == "all") && amount > 420000000)
        {
            handler->PSendSysMessage("|cffff0000警告|r: 耐力 %d 超过安全上限 4.2 亿，", amount);
            handler->SendSysMessage("|cffff0000血量会超过 uint32(42亿) 上限而回绕成 0，角色将暴毙。|r");
            handler->SendSysMessage("已拒绝执行。请使用 420000000 以下的值。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        struct { char const* key; Stats stat; char const* cn; } const statMap[] =
        {
            { "str", STAT_STRENGTH,  "力量" },
            { "agi", STAT_AGILITY,   "敏捷" },
            { "sta", STAT_STAMINA,   "耐力" },
            { "int", STAT_INTELLECT, "智力" },
            { "spi", STAT_SPIRIT,    "精神" },
        };

        if (statName == "all")
        {
            for (auto const& m : statMap)
                ModifyOneStat(handler, target, m.stat, amount);
            target->UpdateAllStats();
            handler->PSendSysMessage("已将 %s 的全部五维设为 +%d。",
                handler->GetNameLink(target).c_str(), amount);
            return true;
        }

        for (auto const& m : statMap)
        {
            if (statName == m.key)
            {
                ModifyOneStat(handler, target, m.stat, amount);
                handler->PSendSysMessage("已将 %s 的%s设为 +%d。",
                    handler->GetNameLink(target).c_str(), m.cn, amount);

                if (m.stat == STAT_STAMINA)
                    handler->PSendSysMessage("当前最大生命: %u", target->GetMaxHealth());

                return true;
            }
        }

        handler->SendSysMessage("未知属性。可用: str agi sta int spi all reset");
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool HandleModifyAllStatsCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .modify allstats <数值>");
            handler->SendSysMessage("示例: .modify allstats 100000000");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 复用 stat all 的逻辑
        std::string forward = "all ";
        forward += args;
        return HandleModifyStatCommand(handler, forward.c_str());
    }
    // ================= 自定义结束 =================
```

---

## 改动 3：添加 RBAC 权限号

文件：`D:\TrinityCore\src\server\game\Accounts\RBAC.h`

找到 NPCBot 权限的最后一行（约第 792 行）：

```cpp
    RBAC_PERM_COMMAND_NPCBOT_SEND                            = 70037,
    //End NPCBot
```

在 `//End NPCBot` **之后**、`RBAC_PERM_MAX` **之前**插入：

```cpp
    //End NPCBot

    // 自定义指令权限 71001+
    // （官方用到 885，NPCBot 占 70001-70037，这里从 71001 开始避免冲突）
    RBAC_PERM_COMMAND_MODIFY_ALLSTATS                        = 71001,
    RBAC_PERM_COMMAND_MODIFY_STAT                            = 71002,

    RBAC_PERM_MAX
};
```

---

## 改动 4：确认头文件

`cs_modify.cpp` 顶部应已包含所需头文件。如果编译报
`'transform' is not a member of 'std'`，在文件顶部 include 区加一行：

```cpp
#include <algorithm>
```

---

## 编译与测试

```
1. VS 2022 打开 D:\TC-Build\TrinityCore.sln
2. 配置选 RelWithDebInfo + x64
3. 生成 -> 生成解决方案
4. 重启 worldserver
```

### 游戏内测试

```
.modify stat sta 100000000      耐力设为1亿，应显示最大生命约10亿
.modify stat str 500000000      力量5亿
.modify allstats 50000000       五维全部5000万
.modify stat reset              全部清零
.modify stat sta 500000000      应被拒绝（超过4.2亿安全上限）
```

### 权限说明

新指令用的是 71001/71002，**这两个权限号不在 auth 库的
`rbac_permissions` 表里**。TrinityCore 对未注册的权限，
默认只有**最高权限账号（SEC_CONSOLE / 3级GM）**能用。

如果你的 GM 账号用不了，在 auth 库执行：

```sql
INSERT INTO auth.rbac_permissions (id, name) VALUES
 (71001, 'Command: modify allstats'),
 (71002, 'Command: modify stat');

INSERT INTO auth.rbac_linked_permissions (id, linkedId) VALUES
 (192, 71001),
 (192, 71002);
```

> 192 = 3级GM角色组。改完重启 authserver 或执行 `.account set gmlevel`。

---

## 为什么用 TOTAL_VALUE 而不是 BASE_VALUE

`_ApplyItemMods()` 里装备属性走的是 `BASE_VALUE`（`Player.cpp:7341`）。
如果我们也写 `BASE_VALUE`，会和装备属性冲突，脱装备时被一起清掉。

用 `TOTAL_VALUE` 的好处：
- 和装备属性互不干扰
- `reset` 时设为 0 就能干净还原
- 升级时不影响基础属性重算
