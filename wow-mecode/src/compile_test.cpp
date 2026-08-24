// 模拟 TrinityCore 环境，验证第6步代码的语法与类型正确性
#include <cstdio>
#include <cstdint>
#include <string>
#include <algorithm>
#include <cstring>
#include <vector>

typedef uint8_t uint8; typedef int32_t int32; typedef uint32_t uint32;

enum Stats { STAT_STRENGTH=0, STAT_AGILITY=1, STAT_STAMINA=2, STAT_INTELLECT=3, STAT_SPIRIT=4, MAX_STATS=5 };
enum UnitMods { UNIT_MOD_STAT_STRENGTH=0, UNIT_MOD_STAT_AGILITY, UNIT_MOD_STAT_STAMINA,
                UNIT_MOD_STAT_INTELLECT, UNIT_MOD_STAT_SPIRIT, UNIT_MOD_STAT_START=UNIT_MOD_STAT_STRENGTH };
enum UnitModifierFlatType { BASE_VALUE=0, TOTAL_VALUE=1 };
enum { LANG_NO_CHAR_SELECTED = 1 };

template<typename E> constexpr auto AsUnderlyingType(E e){ return static_cast<int>(e); }

struct ObjectGuid { static ObjectGuid Empty; };
ObjectGuid ObjectGuid::Empty;

struct Player {
    void SetStatFlatModifier(UnitMods, UnitModifierFlatType, float){}
    bool UpdateStats(Stats){ return true; }
    bool UpdateAllStats(){ return true; }
    uint32 GetMaxHealth() const { return 1000000000; }
};
struct ChatHandler {
    void SendSysMessage(char const* s){ printf("  %s\n", s); }
    void SendSysMessage(uint32){ printf("  <lang>\n"); }
    template<typename... A> void PSendSysMessage(char const* f, A... a){ printf("  "); printf(f, a...); printf("\n"); }
    void SetSentErrorMessage(bool){}
    Player* getSelectedPlayerOrSelf(){ static Player p; return &p; }
    bool HasLowerSecurity(Player*, ObjectGuid){ return false; }
    std::string GetNameLink(Player*){ return "测试角色"; }
};

// ============ 以下为交付给用户的实际代码 ============
    static bool ModifyOneStat(ChatHandler* handler, Player* target, Stats stat, int32 amount)
    {
        UnitMods unitMod = UnitMods(UNIT_MOD_STAT_START + AsUnderlyingType(stat));
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
        std::string forward = "all ";
        forward += args;
        return HandleModifyStatCommand(handler, forward.c_str());
    }
// ============ 代码结束 ============

int main(){
    ChatHandler h;
    char buf[128];
    printf("=== .modify stat sta 100000000 ===\n");
    strcpy(buf, "sta 100000000"); HandleModifyStatCommand(&h, buf);
    printf("=== .modify stat str 500000000 ===\n");
    strcpy(buf, "str 500000000"); HandleModifyStatCommand(&h, buf);
    printf("=== .modify stat sta 500000000 (应拒绝) ===\n");
    strcpy(buf, "sta 500000000"); HandleModifyStatCommand(&h, buf);
    printf("=== .modify stat reset ===\n");
    strcpy(buf, "reset"); HandleModifyStatCommand(&h, buf);
    printf("=== .modify allstats 50000000 ===\n");
    strcpy(buf, "50000000"); HandleModifyAllStatsCommand(&h, buf);
    printf("=== .modify stat xxx (未知属性) ===\n");
    strcpy(buf, "xxx 100"); HandleModifyStatCommand(&h, buf);
    return 0;
}
