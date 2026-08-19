# 第 11 步：战斗节奏优化 —— 核心文件改动

> **只改 3 个地方，每处 2~3 行。** 真正的逻辑全在新文件 `CustomSpeed.cpp` 里。
> 所有改动都用 `[CUSTOM SPEED]` 标记包裹，以后合并官方更新一眼能认出来。

---

## 改动 1/3 —— Spell.cpp：GCD 缩放

**文件**：`D:\TrinityCore\src\server\game\Spells\Spell.cpp`
**位置**：`Spell::TriggerGlobalCooldown()` 函数末尾，约 **8654 行**

### 找到这段原文

```cpp
    // Apply haste rating
    if (m_spellInfo->StartRecoveryCategory == 133 && m_spellInfo->StartRecoveryTime == 1500 &&
        m_spellInfo->DmgClass != SPELL_DAMAGE_CLASS_MELEE && m_spellInfo->DmgClass != SPELL_DAMAGE_CLASS_RANGED &&
        !m_spellInfo->HasAttribute(SPELL_ATTR0_REQ_AMMO) && !m_spellInfo->HasAttribute(SPELL_ATTR0_ABILITY))
    {
        gcd = int32(float(gcd) * m_caster->GetFloatValue(UNIT_MOD_CAST_SPEED));
        RoundToInterval<int32>(gcd, MIN_GCD, MAX_GCD);
    }

    if (gcd)
        m_caster->ToUnit()->GetSpellHistory()->AddGlobalCooldown(m_spellInfo, gcd);
}
```

### 改成

```cpp
    // Apply haste rating
    if (m_spellInfo->StartRecoveryCategory == 133 && m_spellInfo->StartRecoveryTime == 1500 &&
        m_spellInfo->DmgClass != SPELL_DAMAGE_CLASS_MELEE && m_spellInfo->DmgClass != SPELL_DAMAGE_CLASS_RANGED &&
        !m_spellInfo->HasAttribute(SPELL_ATTR0_REQ_AMMO) && !m_spellInfo->HasAttribute(SPELL_ATTR0_ABILITY))
    {
        gcd = int32(float(gcd) * m_caster->GetFloatValue(UNIT_MOD_CAST_SPEED));
        RoundToInterval<int32>(gcd, MIN_GCD, MAX_GCD);
    }
    // ===== [CUSTOM SPEED] 急速影响近战 GCD（修复原版缺陷）begin =====
    else if (sCustomSpeed->HasteAffectsMelee() && m_spellInfo->StartRecoveryTime == 1500)
    {
        gcd = int32(float(gcd) * m_caster->GetFloatValue(UNIT_MOD_CAST_SPEED));
        RoundToInterval<int32>(gcd, MIN_GCD, MAX_GCD);
    }
    // ===== [CUSTOM SPEED] end =====

    // ===== [CUSTOM SPEED] GCD 缩放 begin =====
    gcd = sCustomSpeed->ScaleGcd(m_caster->ToPlayer(), gcd);
    // ===== [CUSTOM SPEED] end =====

    if (gcd)
        m_caster->ToUnit()->GetSpellHistory()->AddGlobalCooldown(m_spellInfo, gcd);
}
```

> **注意那个 `else if`**：它必须紧跟在原有 `if` 之后。
> 原版的 `if` 只对法系生效（`DmgClass != MELEE && != RANGED`），
> 走不到的就是物理职业 —— `else` 分支正好接住它们。

---

## 改动 2/3 —— Spell.cpp：读条缩放

**文件**：同上
**位置**：`Spell::prepare()` 里，约 **3310 行**

### 找到这段原文

```cpp
    if (Player* player = m_caster->ToPlayer())
    {
        if (!player->GetCommandStatus(CHEAT_CASTTIME))
        {
            // calculate cast time (calculated after first CheckCast check to prevent charge counting for first CheckCast fail)
            m_casttime = m_spellInfo->CalcCastTime(this);
        }
        else
            m_casttime = 0; // Set cast time to 0 if .cheat casttime is enabled.
    }
    else
        m_casttime = m_spellInfo->CalcCastTime(this);
```

### 改成

```cpp
    if (Player* player = m_caster->ToPlayer())
    {
        if (!player->GetCommandStatus(CHEAT_CASTTIME))
        {
            // calculate cast time (calculated after first CheckCast check to prevent charge counting for first CheckCast fail)
            m_casttime = m_spellInfo->CalcCastTime(this);
            // ===== [CUSTOM SPEED] 读条缩放 begin =====
            m_casttime = sCustomSpeed->ScaleCastTime(player, m_spellInfo, m_casttime);
            // ===== [CUSTOM SPEED] end =====
        }
        else
            m_casttime = 0; // Set cast time to 0 if .cheat casttime is enabled.
    }
    else
        m_casttime = m_spellInfo->CalcCastTime(this);
```

> 只加了 3 行（含两行注释标记）。`player` 变量现成的，直接用。

---

## 改动 3/3 —— SpellHistory.cpp：单技能 CD 缩放

**文件**：`D:\TrinityCore\src\server\game\Spells\SpellHistory.cpp`
**位置**：`SpellHistory::StartCooldown()` 里，约 **320 行**

### 找到这段原文

```cpp
        if (int32 cooldownMod = _owner->GetTotalAuraModifier(SPELL_AURA_MOD_COOLDOWN))
        {
            // Apply SPELL_AURA_MOD_COOLDOWN only to own spells
            Player* playerOwner = GetPlayerOwner();
            if (!playerOwner || playerOwner->HasSpell(spellInfo->Id))
            {
                needsCooldownPacket = true;
                cooldown += cooldownMod * IN_MILLISECONDS;   // SPELL_AURA_MOD_COOLDOWN does not affect category cooldows, verified with shaman shocks
            }
        }

        // replace negative cooldowns by 0
        if (cooldown < 0)
            cooldown = 0;
```

### 改成

```cpp
        if (int32 cooldownMod = _owner->GetTotalAuraModifier(SPELL_AURA_MOD_COOLDOWN))
        {
            // Apply SPELL_AURA_MOD_COOLDOWN only to own spells
            Player* playerOwner = GetPlayerOwner();
            if (!playerOwner || playerOwner->HasSpell(spellInfo->Id))
            {
                needsCooldownPacket = true;
                cooldown += cooldownMod * IN_MILLISECONDS;   // SPELL_AURA_MOD_COOLDOWN does not affect category cooldows, verified with shaman shocks
            }
        }

        // ===== [CUSTOM SPEED] 单技能CD 缩放 begin =====
        sCustomSpeed->ScaleCooldown(GetPlayerOwner(), spellInfo, cooldown, categoryCooldown);
        // ===== [CUSTOM SPEED] end =====

        // replace negative cooldowns by 0
        if (cooldown < 0)
            cooldown = 0;
```

---

## 改动 4/3（附加）—— 补 include

三个文件都要加 `#include "CustomSpeed.h"`：

### Spell.cpp

在文件顶部的 include 区，找到 `#include "SpellHistory.h"` 那一行，在它后面加：

```cpp
#include "CustomSpeed.h"
```

### SpellHistory.cpp

同样在 include 区加：

```cpp
#include "CustomSpeed.h"
```

---

## 改动 5/3（附加）—— .reload config 时重新加载

**文件**：`D:\TrinityCore\src\server\game\World\World.cpp`
**位置**：`World::LoadConfigSettings()` 函数**末尾**（`return;` 或函数最后一行 `}` 之前）

加一行：

```cpp
    // ===== [CUSTOM SPEED] 让 .reload config 能刷新战斗节奏设置 begin =====
    sCustomSpeed->LoadConfig();
    // ===== [CUSTOM SPEED] end =====
```

同样要在 World.cpp 顶部加 `#include "CustomSpeed.h"`。

> 这一步是可选的。不加也能用，只是改 conf 后必须**重启**而不能热重载。
> 建议加上，调手感时能省很多时间。

---

## 安装步骤汇总

### 1. 放两个新文件

```
D:\TrinityCore\src\server\game\Spells\CustomSpeed.h
D:\TrinityCore\src\server\game\Spells\CustomSpeed.cpp
```

（从 `patches/step11_CustomSpeed.h` 和 `.cpp` 复制过去，**去掉 step11_ 前缀**）

### 2. 改 3 处核心代码 + 3 处 include

见上面。

### 3. 放 conf

把 `conf/worldserver.conf.d/speed.conf` 复制到

```
D:\TC-Build\bin\RelWithDebInfo\worldserver.conf.d\speed.conf
```

> **speed.conf 不用改后缀**。它和档位文件（epic.conf 等）是不同功能，
> 同时生效互不干扰。切档位时也不要把它关掉。

### 4. 重跑 CMake

**这次是新增源文件，必须重跑 CMake**（`file(GLOB)` 的文件列表在生成时固定）。

```
CMake GUI -> Configure -> Generate
```

### 5. 编译 + 重启

启动时应该看到：

```
>> 战斗节奏优化已启用：GCD 60% / 读条 75% / CD 100% (地板 300ms/500ms/3000ms，急速影响近战=是，PVP恢复原版=是)
```

---

## 测试清单

| # | 操作 | 预期 |
|---|---|---|
| 1 | 看启动日志 | 出现「战斗节奏优化已启用」那行 |
| 2 | 战士放技能 | GCD 明显变快（1.5s → 0.9s） |
| 3 | 法师念火球术 | 读条 3.5s → 2.6s |
| 4 | 用冲锋 | CD 15s → 12.75s |
| 5 | 用寒冰屏障 | CD 5min → 4min30s（长CD少压） |
| 6 | 用炉石 | **CD 不变**（白名单保护） |
| 7 | 战复队友 | **CD 不变**（白名单保护） |
| 8 | 进战场放技能 | **GCD 恢复 1.5s**（PVP 保护） |
| 9 | 战士堆急速 | GCD 跟着变短（原版不会） |
| 10 | 改 conf 后 `.reload config` | 立刻生效，不用重启 |

---

## 数值验证（已跑过）

```
【GCD 60%】
  战士/猎人 1500 -> 900 ms    1.67x
  法师      1153 -> 691 ms    1.67x
  盗贼      1000 -> 600 ms    1.67x

【读条 75%】
  火球术   3500 -> 2625 ms
  强效治疗 3000 -> 2250 ms
  极短读条  600 ->  500 ms    <- 地板保护生效

【CD 分段】
  冲锋 15秒     -> 12.75秒  (85% 档)  省 2.2 秒
  盾墙 30秒     -> 24秒     (80% 档)  省 6.0 秒
  寒冰屏障 5分钟 -> 4分30秒  (90% 档)  省 30 秒
  法术反射 2秒  -> 不变      (低于 MinScale)

【平衡性 —— 本次改动的核心目的】
  职业           原循环  只压GCD  GCD+读条  提速
  战士 致死打击   1500    900      900      1.67x
  法师 火球术     3500    3500     2625     1.33x
  牧师 强效治疗   3000    3000     2250     1.33x

  >>> 只压 GCD：近战 1.67x 但法系 1.00x（完全没提速）
  >>> 都压之后：近战 1.67x 法系 1.33x，差距明显收窄
```

---

## 已做的验证

- `g++ -std=c++17 -Wall -Wextra -Wunused-parameter -Wshadow` **零错误零警告**
- 数值验算 4 组全部符合设计
- 桩文件严格对照真实源码（上次就是栽在桩写错上）：
  - `Player::GetClass()` — Unit.h:896
  - `Player::InBattleground()` — Player.h:1965
  - `Player::InArena()` — Player.h:1966
  - `SPELL_EFFECT_RESURRECT=18 / RESURRECT_NEW=113 / SELF_RESURRECT=94` — SharedDefines.h
  - `SPELL_EFFECT_TELEPORT_UNITS=5 / BIND=36` — SharedDefines.h
- 三个插入点都对照了你仓库的真实源码

---

## 出错怎么办

编译报错**只发第一条**给我，带文件名和行号。

最可能的问题是 include 路径 —— 如果报 `CustomSpeed.h: No such file`，
检查文件是不是放在 `src\server\game\Spells\` 下，
以及有没有重跑 CMake。
