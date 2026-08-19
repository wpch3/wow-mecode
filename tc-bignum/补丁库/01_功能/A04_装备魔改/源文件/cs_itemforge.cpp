/*
 * ============================================================================
 *  装备锻造 —— cs_itemforge.cpp  （第 1 层：.item clone）
 * ============================================================================
 *
 *  解决的痛点：WDE 手填 137 列，其中 ItemEffect 就有 7 字段 x 5 组 = 35 格，
 *  TriggerType 填错（1=装备时 / 2=命中触发）装备毫无反应且不报错。
 *
 *  本层思路：站在暴雪肩膀上 —— 抄一件现成装备再改。
 *  模型、图标、音效、特效全是官方做好的，客户端认得，不用发补丁。
 *
 *  ── 指令 ──────────────────────────────────────────────────────────────
 *   .item clone <源ID> [选项...]        克隆装备（默认只预览）
 *   .item del <entry>                   删除自造装备（仅限 800000+）
 *   .item list [页]                     列出所有自造装备
 *   .item info <entry>                  查看装备详情
 *
 *  ── 克隆选项（可组合）─────────────────────────────────────────────────
 *   装等300 / ilvl300      按装等曲线重算属性、伤害、护甲
 *   x10                    所有数值乘 10
 *   名字 <新名字>          改名（不写则自动加后缀）
 *   品质 橙                改品质
 *   entry 800123           指定 entry（不写则自动分配）
 *   -y                     跳过预览直接写入
 *
 *  例：
 *   .item clone 49623                        预览克隆霜之哀伤
 *   .item clone 49623 装等300 -y             装等拉到300并写入
 *   .item clone 49623 x10 名字 真龙之牙 -y
 *
 *  ── 两个必须处理的坑（已处理）─────────────────────────────────────────
 *   1. 新物品写库后必须 LoadItemTemplates() + InitializeQueryData()，
 *      否则客户端鼠标悬停看到空白/旧数据（conf 里 CacheDataQueries = 1）
 *   2. item_template 有 137 列，手写 INSERT 必漏字段。
 *      -> 用 INSERT ... SELECT 让 MySQL 复制全部列，再 UPDATE 覆盖要改的
 *
 *  放置：D:\TrinityCore\src\server\scripts\Commands\cs_itemforge.cpp
 *  RBAC：71010
 *  新增源文件必须重跑 CMake
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace ItemForge
{
    // 自造装备的 entry 起点。
    // 3.3.5 官方物品最大约 56000，从 800000 起留足安全间隔，
    // 也避开自定义法术用的 900000+ 段。
    static constexpr uint32 ENTRY_BASE = 800000;
    static constexpr uint32 ENTRY_MAX  = 899999;

    // ---------------------------------------------------------------
    //  装等 -> 属性总点数的近似曲线
    //
    //  暴雪的真实公式在 ItemLevel/RandPropPoints.dbc 里，很复杂。
    //  这里用一条拟合曲线，抓大放小：
    //      80级紫装(ilvl 200) 约 100 点
    //      80级顶级(ilvl 264) 约 165 点
    //  超过 300 后线性外推，保证魔改端能拉到任意高度。
    // ---------------------------------------------------------------
    inline double IlvlToPoints(uint32 ilvl)
    {
        if (ilvl == 0)
            return 1.0;
        // 经验拟合：point ≈ ilvl^1.25 / 8
        return std::pow(double(ilvl), 1.25) / 8.0;
    }

    // 计算缩放倍率：目标装等相对源装等
    inline double IlvlScale(uint32 fromIlvl, uint32 toIlvl)
    {
        double a = IlvlToPoints(fromIlvl ? fromIlvl : 1);
        double b = IlvlToPoints(toIlvl);
        if (a <= 0.0)
            return 1.0;
        double s = b / a;
        // 保护：不允许缩小到 0 或放大到离谱
        return std::clamp(s, 0.01, 100000.0);
    }

    // int32 安全放大（本服已做大数值改造，上限是 int32）
    inline int32 ScaleI(int32 v, double s)
    {
        if (v == 0)
            return 0;
        double r = double(v) * s;
        if (r > 2100000000.0)  return 2100000000;
        if (r < -2100000000.0) return -2100000000;
        return int32(r);
    }

    inline uint32 ScaleU(uint32 v, double s)
    {
        if (v == 0)
            return 0;
        double r = double(v) * s;
        if (r > 2100000000.0) return 2100000000u;
        if (r < 0.0)          return 0u;
        return uint32(r);
    }

    inline float ScaleF(float v, double s)
    {
        double r = double(v) * s;
        if (r > 2100000000.0) return 2100000000.0f;
        if (r < 0.0)          return 0.0f;
        return float(r);
    }

    // 品质名 -> 值（SharedDefines.h:375）
    inline int32 QualityFromName(std::string const& s)
    {
        std::string t = s;
        std::transform(t.begin(), t.end(), t.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (t == "灰" || t == "poor")                       return 0;
        if (t == "白" || t == "common")                     return 1;
        if (t == "绿" || t == "uncommon")                   return 2;
        if (t == "蓝" || t == "rare")                       return 3;
        if (t == "紫" || t == "史诗" || t == "epic")        return 4;
        if (t == "橙" || t == "传说" || t == "legendary")   return 5;
        if (t == "神器" || t == "artifact")                 return 6;
        return -1;
    }

    inline char const* QualityColor(uint32 q)
    {
        switch (q)
        {
            case 0:  return "ff9d9d9d";
            case 1:  return "ffffffff";
            case 2:  return "ff1eff00";
            case 3:  return "ff0070dd";
            case 4:  return "ffa335ee";
            case 5:  return "ffff8000";
            case 6:  return "ffe6cc80";
            default: return "ffffffff";
        }
    }

    inline char const* QualityName(uint32 q)
    {
        switch (q)
        {
            case 0:  return "灰";
            case 1:  return "白";
            case 2:  return "绿";
            case 3:  return "蓝";
            case 4:  return "紫";
            case 5:  return "橙";
            case 6:  return "神器";
            default: return "?";
        }
    }

    // ITEM_MOD_* -> 中文名（ItemTemplate.h:28-73）
    inline char const* StatName(uint32 type)
    {
        switch (type)
        {
            case 0:  return "法力";
            case 1:  return "生命";
            case 3:  return "敏捷";
            case 4:  return "力量";
            case 5:  return "智力";
            case 6:  return "精神";
            case 7:  return "耐力";
            case 12: return "防御";
            case 13: return "躲闪";
            case 14: return "招架";
            case 15: return "格挡";
            case 31: return "命中";
            case 32: return "暴击";
            case 35: return "韧性";
            case 36: return "急速";
            case 37: return "精准";
            case 38: return "攻强";
            case 39: return "远程攻强";
            case 43: return "回蓝";
            case 44: return "护甲穿透";
            case 45: return "法术强度";
            case 46: return "生命回复";
            case 47: return "法术穿透";
            case 48: return "格挡值";
            default: return "未知属性";
        }
    }

    // TriggerType -> 中文（ItemTemplate.h:78-92）
    inline char const* TriggerName(uint32 t)
    {
        switch (t)
        {
            case 0: return "使用时";
            case 1: return "装备时";
            case 2: return "命中触发";
            case 4: return "灵魂石";
            case 5: return "使用(无CD)";
            case 6: return "教学";
            default: return "未知";
        }
    }

    // 找下一个可用 entry
    inline uint32 FindFreeEntry()
    {
        ItemTemplateContainer const& store = sObjectMgr->GetItemTemplateStore();
        for (uint32 e = ENTRY_BASE; e <= ENTRY_MAX; ++e)
        {
            if (store.find(e) == store.end())
                return e;
        }
        return 0;
    }

    // 物品链接
    inline std::string Link(ItemTemplate const* p, std::string const& nameOverride = "")
    {
        if (!p)
            return "|cffff0000[未知]|r";
        std::ostringstream ss;
        ss << "|c" << QualityColor(p->Quality)
           << "|Hitem:" << p->ItemId << ":0:0:0:0:0:0:0:0|h["
           << (nameOverride.empty() ? p->Name1 : nameOverride) << "]|h|r";
        return ss.str();
    }

    // 转义单引号，防 SQL 注入
    inline std::string Esc(std::string s)
    {
        std::string safe = s;
        WorldDatabase.EscapeString(safe);
        return safe;
    }


    // ---------- 职业掩码（SharedDefines.h:136-150）----------
    // 注意 CLASS 10 空缺，德鲁伊是 11
    struct ClassInfo { char const* cn; char const* en; uint32 cls; };
    static ClassInfo const g_classes[] =
    {
        { "战士",   "warrior",     1  },
        { "圣骑士", "paladin",     2  },
        { "猎人",   "hunter",      3  },
        { "盗贼",   "rogue",       4  },
        { "牧师",   "priest",      5  },
        { "死骑",   "dk",          6  },
        { "萨满",   "shaman",      7  },
        { "法师",   "mage",        8  },
        { "术士",   "warlock",     9  },
        { "德鲁伊", "druid",       11 },
    };
    static constexpr uint32 CLASSMASK_ALL = 1535;   // 全部 10 个职业

    // 中文/英文职业名 -> 掩码位。返回 0 表示不认识
    inline uint32 ClassMaskFromName(std::string const& s)
    {
        std::string t = s;
        std::transform(t.begin(), t.end(), t.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (t == "全部" || t == "all" || t == "通用")
            return CLASSMASK_ALL;

        for (auto const& ci : g_classes)
        {
            if (t == ci.cn || t == ci.en)
                return 1u << (ci.cls - 1);
        }
        return 0;
    }

    // 掩码 -> 可读文字
    inline std::string ClassMaskToText(uint32 mask)
    {
        if (mask == 0 || mask == CLASSMASK_ALL || mask == uint32(-1))
            return "全职业";

        std::string out;
        for (auto const& ci : g_classes)
        {
            if (mask & (1u << (ci.cls - 1)))
            {
                if (!out.empty())
                    out += "/";
                out += ci.cn;
            }
        }
        return out.empty() ? "无(没人能穿)" : out;
    }

    // ---------- 属性名 -> ITEM_MOD_*（ItemTemplate.h:28-73）----------
    struct StatAlias { char const* name; uint32 mod; };
    static StatAlias const g_statAlias[] =
    {
        // 五维
        { "力量",   4  }, { "str",       4  }, { "strength",  4  },
        { "敏捷",   3  }, { "agi",       3  }, { "agility",   3  },
        { "耐力",   7  }, { "sta",       7  }, { "stamina",   7  },
        { "智力",   5  }, { "int",       5  }, { "intellect", 5  },
        { "精神",   6  }, { "spi",       6  }, { "spirit",    6  },
        // 基础
        { "生命",   1  }, { "hp",        1  }, { "血量",      1  },
        { "法力",   0  }, { "mana",      0  }, { "蓝量",      0  },
        // 战斗子属性
        { "攻强",   38 }, { "攻击强度",  38 }, { "ap",        38 },
        { "远程攻强", 39 }, { "rap",     39 },
        { "法强",   45 }, { "法术强度",  45 }, { "sp",        45 },
        { "暴击",   32 }, { "crit",      32 }, { "暴击等级",  32 },
        { "命中",   31 }, { "hit",       31 }, { "命中等级",  31 },
        { "急速",   36 }, { "haste",     36 }, { "急速等级",  36 },
        { "精准",   37 }, { "expertise", 37 },
        { "韧性",   35 }, { "resilience",35 },
        { "护甲穿透", 44 }, { "arp",     44 }, { "穿甲",      44 },
        { "法术穿透", 47 }, { "spellpen",47 },
        // 防御子属性
        { "防御",   12 }, { "defense",   12 }, { "防御等级",  12 },
        { "躲闪",   13 }, { "dodge",     13 },
        { "招架",   14 }, { "parry",     14 },
        { "格挡",   15 }, { "block",     15 },
        { "格挡值", 48 }, { "blockvalue",48 },
        // 回复
        { "回蓝",   43 }, { "mp5",       43 }, { "法力回复",  43 },
        { "回血",   46 }, { "hp5",       46 }, { "生命回复",  46 },
    };

    // 返回 0xFFFFFFFF 表示不认识（0 是合法值 ITEM_MOD_MANA）
    static constexpr uint32 STAT_INVALID = 0xFFFFFFFF;

    inline uint32 StatModFromName(std::string const& s)
    {
        std::string t = s;
        std::transform(t.begin(), t.end(), t.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        for (auto const& sa : g_statAlias)
        {
            std::string a = sa.name;
            std::transform(a.begin(), a.end(), a.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (t == a)
                return sa.mod;
        }
        return STAT_INVALID;
    }

    // ---------- 武器/护甲子类 ----------
    struct SubClassAlias { char const* name; uint32 cls; uint32 sub; uint32 invType; };
    static SubClassAlias const g_subAlias[] =
    {
        // 名字, ItemClass(2=武器 4=护甲), SubClass, 建议 InventoryType
        { "单手剑",   2, 7,  13 }, { "sword",     2, 7,  13 },
        { "双手剑",   2, 8,  17 }, { "sword2",    2, 8,  17 },
        { "单手斧",   2, 0,  13 }, { "axe",       2, 0,  13 },
        { "双手斧",   2, 1,  17 }, { "axe2",      2, 1,  17 },
        { "单手锤",   2, 4,  13 }, { "mace",      2, 4,  13 },
        { "双手锤",   2, 5,  17 }, { "mace2",     2, 5,  17 },
        { "长柄",     2, 6,  17 }, { "polearm",   2, 6,  17 },
        { "法杖",     2, 10, 17 }, { "staff",     2, 10, 17 },
        { "匕首",     2, 15, 13 }, { "dagger",    2, 15, 13 },
        { "拳套",     2, 13, 13 }, { "fist",      2, 13, 13 },
        { "弓",       2, 2,  15 }, { "bow",       2, 2,  15 },
        { "枪",       2, 3,  26 }, { "gun",       2, 3,  26 },
        { "弩",       2, 18, 26 }, { "crossbow",  2, 18, 26 },
        { "魔杖",     2, 19, 26 }, { "wand",      2, 19, 26 },
        { "投掷",     2, 16, 25 }, { "thrown",    2, 16, 25 },
        { "鱼竿",     2, 20, 17 },
        // 护甲
        { "布甲",     4, 1,  0  }, { "cloth",     4, 1,  0  },
        { "皮甲",     4, 2,  0  }, { "leather",   4, 2,  0  },
        { "锁甲",     4, 3,  0  }, { "mail",      4, 3,  0  },
        { "板甲",     4, 4,  0  }, { "plate",     4, 4,  0  },
        { "盾牌",     4, 6,  14 }, { "shield",    4, 6,  14 },
        { "圣契",     4, 7,  23 }, { "libram",    4, 7,  23 },
        { "神像",     4, 8,  23 }, { "idol",      4, 8,  23 },
        { "图腾",     4, 9,  23 }, { "totem",     4, 9,  23 },
        { "魔印",     4, 10, 23 }, { "sigil",     4, 10, 23 },
        { "饰品",     4, 0,  12 }, { "trinket",   4, 0,  12 },
    };

    inline SubClassAlias const* SubClassFromName(std::string const& s)
    {
        std::string t = s;
        std::transform(t.begin(), t.end(), t.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (auto const& sc : g_subAlias)
        {
            std::string a = sc.name;
            std::transform(a.begin(), a.end(), a.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (t == a)
                return &sc;
        }
        return nullptr;
    }

    // ---------- 部位名 -> InventoryType ----------
    struct SlotAlias2 { char const* name; uint32 invType; };
    static SlotAlias2 const g_invAlias[] =
    {
        { "头",   1  }, { "头盔",   1  }, { "head",      1  },
        { "颈",   2  }, { "项链",   2  }, { "neck",      2  },
        { "肩",   3  }, { "护肩",   3  }, { "shoulder",  3  },
        { "衬衣", 4  }, { "shirt",  4  },
        { "胸",   5  }, { "胸甲",   5  }, { "chest",     5  },
        { "腰",   6  }, { "腰带",   6  }, { "waist",     6  },
        { "腿",   7  }, { "护腿",   7  }, { "legs",      7  },
        { "脚",   8  }, { "靴子",   8  }, { "feet",      8  },
        { "腕",   9  }, { "护腕",   9  }, { "wrist",     9  },
        { "手",   10 }, { "手套",   10 }, { "hands",     10 },
        { "戒指", 11 }, { "finger", 11 },
        { "饰品", 12 }, { "trinket",12 },
        { "单手", 13 }, { "主手",   21 }, { "副手",      22 },
        { "盾",   14 }, { "shield", 14 },
        { "披风", 16 }, { "背",     16 }, { "cloak",     16 },
        { "双手", 17 }, { "2h",     17 },
        { "战袍", 19 }, { "tabard", 19 },
        { "长袍", 20 }, { "robe",   20 },
        { "副手物品", 23 }, { "holdable", 23 },
        { "远程", 26 }, { "ranged", 26 },
    };

    inline uint32 InvTypeFromName(std::string const& s)
    {
        std::string t = s;
        std::transform(t.begin(), t.end(), t.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (auto const& ia : g_invAlias)
        {
            std::string a = ia.name;
            std::transform(a.begin(), a.end(), a.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (t == a)
                return ia.invType;
        }
        return 0;
    }

    // ---------- 触发方式名 -> TriggerType（ItemTemplate.h:78-92）----------
    inline int32 TriggerFromName(std::string const& s)
    {
        std::string t = s;
        std::transform(t.begin(), t.end(), t.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (t == "使用" || t == "use" || t == "点击")          return 0;
        if (t == "装备" || t == "equip" || t == "被动")        return 1;
        if (t == "命中" || t == "hit" || t == "触发" || t == "proc") return 2;
        if (t == "使用无cd" || t == "usenocd")                 return 5;
        if (t == "教学" || t == "learn")                       return 6;
        return -1;
    }

    inline char const* InvTypeName(uint32 t)
    {
        switch (t)
        {
            case 1:  return "头部";      case 2:  return "颈部";
            case 3:  return "肩部";      case 4:  return "衬衣";
            case 5:  return "胸甲";      case 6:  return "腰带";
            case 7:  return "腿部";      case 8:  return "靴子";
            case 9:  return "护腕";      case 10: return "手套";
            case 11: return "戒指";      case 12: return "饰品";
            case 13: return "单手";      case 14: return "盾牌";
            case 15: return "弓";        case 16: return "披风";
            case 17: return "双手";      case 19: return "战袍";
            case 20: return "长袍";      case 21: return "主手";
            case 22: return "副手";      case 23: return "副手物品";
            case 25: return "投掷";      case 26: return "远程";
            default: return "其他";
        }
    }

    inline char const* SubClassName(uint32 cls, uint32 sub)
    {
        if (cls == 2)   // 武器
        {
            switch (sub)
            {
                case 0:  return "单手斧"; case 1:  return "双手斧";
                case 2:  return "弓";     case 3:  return "枪";
                case 4:  return "单手锤"; case 5:  return "双手锤";
                case 6:  return "长柄";   case 7:  return "单手剑";
                case 8:  return "双手剑"; case 10: return "法杖";
                case 13: return "拳套";   case 15: return "匕首";
                case 16: return "投掷";   case 18: return "弩";
                case 19: return "魔杖";   case 20: return "鱼竿";
                default: return "其他武器";
            }
        }
        if (cls == 4)   // 护甲
        {
            switch (sub)
            {
                case 0:  return "杂项";   case 1:  return "布甲";
                case 2:  return "皮甲";   case 3:  return "锁甲";
                case 4:  return "板甲";   case 5:  return "小盾";
                case 6:  return "盾牌";   case 7:  return "圣契";
                case 8:  return "神像";   case 9:  return "图腾";
                case 10: return "魔印";
                default: return "其他护甲";
            }
        }
        return "非装备";
    }

    // 克隆参数
    struct CloneArgs
    {
        uint32 srcEntry   = 0;
        uint32 newEntry   = 0;      // 0 = 自动分配
        uint32 targetIlvl = 0;      // 0 = 不改
        double multiply   = 0.0;    // 0 = 不用倍数
        int32  quality    = -1;     // -1 = 不改
        std::string newName;
        bool   confirm    = false;  // -y
    };
}

class itemforge_commandscript : public CommandScript
{
public:
    itemforge_commandscript() : CommandScript("itemforge_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "item", rbac::RBAC_PERM_COMMAND_ITEMFORGE, false, &HandleItemCommand, "" },
        };
        return commandTable;
    }

    static bool HandleItemCommand(ChatHandler* handler, char const* args)
    {
        std::vector<std::string> tok = Tokenize(args);

        if (tok.empty())
        {
            ShowHelp(handler);
            return true;
        }

        std::string const& sub = tok[0];

        if (sub == "clone")  return HandleClone(handler, tok);
        if (sub == "del" || sub == "delete")
                             return HandleDel(handler, tok);
        if (sub == "list")   return HandleList(handler, tok);
        if (sub == "info")   return HandleInfo(handler, tok);
        if (sub == "type")   return HandleType(handler, tok);
        if (sub == "name")   return HandleName(handler, tok);
        if (sub == "stat")   return HandleStat(handler, tok);
        if (sub == "spell")  return HandleSpell(handler, tok);
        if (sub == "check")  return HandleCheck(handler, tok);
        if (sub == "why")    return HandleWhy(handler, tok);
        if (sub == "fix")    return HandleFix(handler, tok);
        if (sub == "diff")   return HandleDiff(handler, tok);
        if (sub == "raw")    return HandleRaw(handler, tok);
        if (sub == "set")    return HandleSet(handler, tok);
        if (sub == "combat") return HandleCombat(handler, tok);

        ShowHelp(handler);
        return true;
    }

private:

    // ==================================================================
    //  工具
    // ==================================================================
    static std::vector<std::string> Tokenize(char const* args)
    {
        std::vector<std::string> tok;
        if (!args)
            return tok;
        std::string a = args;
        size_t pos = 0;
        while (pos < a.size())
        {
            size_t sp = a.find(' ', pos);
            if (sp == std::string::npos)
                sp = a.size();
            if (sp > pos)
                tok.push_back(a.substr(pos, sp - pos));
            pos = sp + 1;
        }
        return tok;
    }

    static bool IsNumber(std::string const& s)
    {
        return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
    }

    static std::string ToLowerStr(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    // 从 "装等300" / "ilvl300" 里抠数字，失败返回 0
    static uint32 ParsePrefixedNumber(std::string const& s, std::vector<std::string> const& prefixes)
    {
        for (std::string const& p : prefixes)
        {
            if (s.size() > p.size() && s.compare(0, p.size(), p) == 0)
            {
                std::string num = s.substr(p.size());
                if (IsNumber(num))
                    return uint32(atoi(num.c_str()));
            }
        }
        return 0;
    }

    static void ShowHelp(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ff00===== 装备锻造 =====|r");
        handler->PSendSysMessage("|cffffff00.item clone <源ID> [选项]|r  克隆现有装备");
        handler->PSendSysMessage("|cffffff00.item list [页]|r            列出自造装备");
        handler->PSendSysMessage("|cffffff00.item info <entry>|r         查看详情");
        handler->PSendSysMessage("|cffffff00.item del <entry>|r          删除自造装备");
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00--- 编辑（只能改自造装备）---|r");
        handler->PSendSysMessage("|cffffff00.item type <entry> ...|r      改职业限制/类型/部位/等级");
        handler->PSendSysMessage("|cffffff00.item name <entry> <新名>|r   改名字（或 desc 改描述）");
        handler->PSendSysMessage("|cffffff00.item stat <entry> ...|r      加属性（五维+全部子属性）");
        handler->PSendSysMessage("|cffffff00.item spell <entry> ...|r     加技能（5槽位，触发方式独立）");
        handler->PSendSysMessage("|cffffff00.item combat <entry> ...|r    改伤害/速度/护甲/耐久");
        handler->PSendSysMessage("|cffffff00.item check <法术ID>|r        查法术要什么武器 |cffff8000(重要)|r");
        handler->PSendSysMessage("|cffffff00.item set ...|r               |cff00ff00套装系统|r list/info/clone/bind/new");
        handler->PSendSysMessage("|cffffff00.item why <法术ID>|r          |cffff8000诊断：为什么技能识别不到我的武器|r");
        handler->PSendSysMessage("|cffffff00.item fix [entry]|r          修复耐久归零的装备");
        handler->PSendSysMessage("|cffffff00.item diff <A> <B>|r         |cffff8000对比两件装备，找出差异|r");
        handler->PSendSysMessage("|cffffff00.item raw <entry>|r           |cffff8000数据库 vs 内存，定位问题层|r");
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00--- 克隆选项（可组合）---|r");
        handler->PSendSysMessage("  |cff00ccff装等300|r      按装等曲线重算属性/伤害/护甲");
        handler->PSendSysMessage("  |cff00ccffx10|r          所有数值乘 10");
        handler->PSendSysMessage("  |cff00ccff名字 <新名>|r  改名");
        handler->PSendSysMessage("  |cff00ccff品质 橙|r      改品质");
        handler->PSendSysMessage("  |cff00ccffentry 800123|r 指定 entry（默认自动分配）");
        handler->PSendSysMessage("  |cff00ccff-y|r           跳过预览直接写入");
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("例：|cffffff00.item clone 49623 装等300 -y|r");
        handler->PSendSysMessage("|cff888888自造装备 entry 段：%u - %u|r",
            ItemForge::ENTRY_BASE, ItemForge::ENTRY_MAX);
    }

    // ==================================================================
    //  克隆
    // ==================================================================
    static bool HandleClone(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item clone <源物品ID> [选项]|r");
            handler->PSendSysMessage("用 |cffffff00.item|r 看全部选项。");
            return true;
        }

        ItemForge::CloneArgs ca;
        ca.srcEntry = uint32(atoi(tok[1].c_str()));

        // ---- 解析选项 ----
        for (size_t i = 2; i < tok.size(); ++i)
        {
            std::string const& t = tok[i];

            if (t == "-y" || t == "确认")
            {
                ca.confirm = true;
                continue;
            }

            if (t == "名字" || t == "name")
            {
                if (i + 1 < tok.size())
                {
                    // 名字取到下一个已知关键字之前
                    std::string nm;
                    size_t j = i + 1;
                    for (; j < tok.size(); ++j)
                    {
                        std::string const& w = tok[j];
                        if (w == "-y" || w == "确认" || w == "品质" || w == "quality" ||
                            w == "entry" || w.rfind("装等", 0) == 0 ||
                            w.rfind("ilvl", 0) == 0 || (w.size() > 1 && w[0] == 'x' && IsNumber(w.substr(1))))
                            break;
                        if (!nm.empty())
                            nm += " ";
                        nm += w;
                    }
                    ca.newName = nm;
                    i = j - 1;
                }
                continue;
            }

            if (t == "品质" || t == "quality")
            {
                if (i + 1 < tok.size())
                {
                    int32 q = ItemForge::QualityFromName(tok[i + 1]);
                    if (q >= 0)
                        ca.quality = q;
                    ++i;
                }
                continue;
            }

            if (t == "entry")
            {
                if (i + 1 < tok.size() && IsNumber(tok[i + 1]))
                {
                    ca.newEntry = uint32(atoi(tok[i + 1].c_str()));
                    ++i;
                }
                continue;
            }

            // 装等300 / ilvl300
            if (uint32 il = ParsePrefixedNumber(t, { "装等", "ilvl", "iLvl", "ILVL" }))
            {
                ca.targetIlvl = il;
                continue;
            }

            // x10 / X10
            if (t.size() > 1 && (t[0] == 'x' || t[0] == 'X') && IsNumber(t.substr(1)))
            {
                ca.multiply = double(atoi(t.substr(1).c_str()));
                continue;
            }

            // 光写品质名也认（.item clone 49623 橙）
            int32 q = ItemForge::QualityFromName(t);
            if (q >= 0)
            {
                ca.quality = q;
                continue;
            }

            handler->PSendSysMessage("|cffff8000忽略无法识别的选项：%s|r", t.c_str());
        }

        // ---- 校验源物品 ----
        ItemTemplate const* src = sObjectMgr->GetItemTemplate(ca.srcEntry);
        if (!src)
        {
            handler->PSendSysMessage("|cffff0000源物品 %u 不存在。|r", ca.srcEntry);
            return true;
        }

        // ---- 确定新 entry ----
        if (ca.newEntry)
        {
            if (ca.newEntry < ItemForge::ENTRY_BASE || ca.newEntry > ItemForge::ENTRY_MAX)
            {
                handler->PSendSysMessage("|cffff0000entry 必须在 %u - %u 之间。|r",
                    ItemForge::ENTRY_BASE, ItemForge::ENTRY_MAX);
                handler->PSendSysMessage("|cff888888这是自造装备专用段，避免和官方物品撞号。|r");
                return true;
            }
            if (sObjectMgr->GetItemTemplate(ca.newEntry))
            {
                handler->PSendSysMessage("|cffff0000entry %u 已被占用。|r", ca.newEntry);
                handler->PSendSysMessage("用 |cffffff00.item info %u|r 看是什么，或换一个号。", ca.newEntry);
                return true;
            }
        }
        else
        {
            ca.newEntry = ItemForge::FindFreeEntry();
            if (!ca.newEntry)
            {
                handler->PSendSysMessage("|cffff0000%u-%u 段已满，请先删除一些自造装备。|r",
                    ItemForge::ENTRY_BASE, ItemForge::ENTRY_MAX);
                return true;
            }
        }

        // ---- 计算缩放倍率 ----
        double scale = 1.0;
        std::string scaleDesc = "不变";

        if (ca.targetIlvl)
        {
            scale = ItemForge::IlvlScale(src->ItemLevel, ca.targetIlvl);
            std::ostringstream ss;
            ss << "装等 " << src->ItemLevel << " -> " << ca.targetIlvl
               << "（x" << std::fixed;
            ss.precision(2);
            ss << scale << "）";
            scaleDesc = ss.str();
        }

        if (ca.multiply > 0.0)
        {
            scale *= ca.multiply;
            std::ostringstream ss;
            if (ca.targetIlvl)
                ss << scaleDesc << " 再 x" << int32(ca.multiply);
            else
            {
                ss << "x" << int32(ca.multiply);
            }
            scaleDesc = ss.str();
        }

        // ---- 新名字 ----
        std::string newName = ca.newName;
        if (newName.empty())
            newName = src->Name1 + " (改)";

        uint32 newQuality = (ca.quality >= 0) ? uint32(ca.quality) : src->Quality;
        uint32 newIlvl    = ca.targetIlvl ? ca.targetIlvl : src->ItemLevel;

        // ---- 预览 ----
        handler->PSendSysMessage("|cff00ff00===== 克隆预览 =====|r");
        handler->PSendSysMessage("源：  %s |cff888888(ID %u, 装等%u, %s)|r",
            ItemForge::Link(src).c_str(), src->ItemId, src->ItemLevel,
            ItemForge::QualityName(src->Quality));
        handler->PSendSysMessage("新：  |c%s[%s]|r |cff888888(ID %u, 装等%u, %s)|r",
            ItemForge::QualityColor(newQuality), newName.c_str(),
            ca.newEntry, newIlvl, ItemForge::QualityName(newQuality));
        handler->PSendSysMessage("缩放：|cffffff00%s|r", scaleDesc.c_str());

        // 属性对比
        if (src->StatsCount)
        {
            handler->PSendSysMessage("|cff00ff00--- 属性 ---|r");
            for (uint32 i = 0; i < src->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
            {
                int32 oldV = src->ItemStat[i].ItemStatValue;
                if (!oldV)
                    continue;
                int32 newV = ItemForge::ScaleI(oldV, scale);
                handler->PSendSysMessage("  %-10s %d |cff00ff00-> %d|r",
                    ItemForge::StatName(src->ItemStat[i].ItemStatType), oldV, newV);
            }
        }

        // 武器伤害
        if (src->Damage[0].DamageMax > 0)
        {
            handler->PSendSysMessage("|cff00ff00--- 伤害 ---|r");
            handler->PSendSysMessage("  %.0f-%.0f |cff00ff00-> %.0f-%.0f|r",
                src->Damage[0].DamageMin, src->Damage[0].DamageMax,
                double(ItemForge::ScaleF(src->Damage[0].DamageMin, scale)),
                double(ItemForge::ScaleF(src->Damage[0].DamageMax, scale)));
        }

        if (src->Armor)
            handler->PSendSysMessage("|cff00ff00--- 护甲 ---|r  %u |cff00ff00-> %u|r",
                src->Armor, ItemForge::ScaleU(src->Armor, scale));

        // 特效（这是 WDE 最难填的部分，直接原样继承）
        bool anyEffect = false;
        for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (src->Effects[i].SpellID <= 0)
                continue;
            if (!anyEffect)
            {
                handler->PSendSysMessage("|cff00ff00--- 特效（原样继承）---|r");
                anyEffect = true;
            }
            handler->PSendSysMessage("  法术 |cffffff00%d|r  %s  PPM %.1f  CD %dms",
                src->Effects[i].SpellID,
                ItemForge::TriggerName(src->Effects[i].TriggerType),
                double(src->Effects[i].SpellPPMRate),
                src->Effects[i].CoolDownMSec);
        }

        if (!ca.confirm)
        {
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cffff8000以上仅为预览，尚未写入。|r");
            handler->PSendSysMessage("确认请加 |cffffff00-y|r：");
            handler->PSendSysMessage("|cff888888.item clone %u ... -y|r", ca.srcEntry);
            return true;
        }

        // ---- 真正写入 ----
        return DoClone(handler, ca, src, scale, newName, newQuality, newIlvl);
    }

    // ==================================================================
    //  执行克隆（写库 + 重载）
    // ==================================================================
    static bool DoClone(ChatHandler* handler, ItemForge::CloneArgs const& ca,
                        ItemTemplate const* /*src*/, double scale,
                        std::string const& newName, uint32 newQuality, uint32 newIlvl)
    {
        /*
         * item_template 有 137 列，手写 INSERT 必漏字段。
         * 用 INSERT ... SELECT 让 MySQL 自己复制所有列，
         * 再用 UPDATE 覆盖要改的几列 —— 永远不会漏。
         */
        // 先清掉可能的残留（比如上次写了一半）
        WorldDatabase.DirectPExecute("DELETE FROM item_template WHERE entry = {}", ca.newEntry);

        /*
         * 复制全部列，但不写死列名。
         *
         * 为什么不能写死：item_template 的列数在不同整合包里不同
         * （本仓库 LoadItemTemplates 读到 fields[137]，注释只标到 136，
         *  说明有未被注释覆盖的列，例如 VerifiedBuild）。
         * 写死列名一旦对不上，INSERT 直接失败。
         *
         * 为什么不用临时表：DatabaseWorkerPool 是连接池，
         * DirectExecute 之间不保证同一条连接，TEMPORARY TABLE 会丢。
         *
         * 解法：先从 information_schema 查出真实列名，
         * 在 C++ 里拼出 "INSERT INTO ... SELECT <新entry>, col2, col3, ..."。
         * 一条语句完成，零连接依赖。
         */
        std::string colList;
        {
            QueryResult colRes = WorldDatabase.Query(
                "SELECT COLUMN_NAME FROM information_schema.COLUMNS "
                "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_template' "
                "ORDER BY ORDINAL_POSITION");

            if (!colRes)
            {
                handler->PSendSysMessage("|cffff0000读取 item_template 表结构失败。|r");
                return true;
            }

            bool first = true;
            do
            {
                std::string col = (*colRes)[0].GetString();
                if (first)
                {
                    // 第一列是 entry，用新 entry 替换
                    colList = std::to_string(ca.newEntry);
                    first = false;
                }
                else
                {
                    colList += ", `";
                    colList += col;
                    colList += "`";
                }
            }
            while (colRes->NextRow());
        }

        WorldDatabase.DirectPExecute(
            "INSERT INTO item_template SELECT {} FROM item_template WHERE entry = {}",
            colList, ca.srcEntry);

        /*
         * 立刻回查确认 INSERT 真的成功了。
         *
         * 这里必须用 DirectPExecute（同步）而不是 PExecute ——
         * DatabaseWorkerPool.h:76 的 PExecute 注释写明是
         * "executed asynchronously"（异步排队），
         * SQL 还在队列里时代码就跑到 LoadItemTemplates() 了，
         * 读到的当然是空的，报「重载后找不到 entry」。
         */
        {
            QueryResult chk = WorldDatabase.PQuery(
                "SELECT COUNT(*) FROM item_template WHERE entry = {}", ca.newEntry);
            if (!chk || (*chk)[0].GetUInt64() == 0)
            {
                handler->PSendSysMessage("|cffff0000写入失败：INSERT 没有生效。|r");
                handler->PSendSysMessage("|cff888888可能原因：|r");
                handler->PSendSysMessage("|cff888888  · item_template 有触发器或外键约束|r");
                handler->PSendSysMessage("|cff888888  · 源物品 %u 在数据库里不存在（只在内存里）|r", ca.srcEntry);
                handler->PSendSysMessage("|cff888888  · 数据库账号没有 INSERT 权限|r");
                handler->PSendSysMessage("|cff888888查服务端日志看具体 SQL 错误。|r");
                return true;
            }
        }

        // 改名字、品质、装等
        WorldDatabase.DirectPExecute(
            "UPDATE item_template SET name = '{}', Quality = {}, ItemLevel = {} WHERE entry = {}",
            ItemForge::Esc(newName), newQuality, newIlvl, ca.newEntry);

        // ---- 按倍率更新数值 ----
        if (scale != 1.0)
        {
            // 属性
            for (uint32 i = 1; i <= MAX_ITEM_PROTO_STATS; ++i)
            {
                WorldDatabase.DirectPExecute(
                    "UPDATE item_template SET stat_value{} = LEAST(2100000000, GREATEST(-2100000000, ROUND(stat_value{} * {}))) "
                    "WHERE entry = {}", i, i, scale, ca.newEntry);
            }

            // 伤害
            WorldDatabase.DirectPExecute(
                "UPDATE item_template SET dmg_min1 = dmg_min1 * {}, dmg_max1 = dmg_max1 * {}, "
                "dmg_min2 = dmg_min2 * {}, dmg_max2 = dmg_max2 * {} WHERE entry = {}",
                scale, scale, scale, scale, ca.newEntry);

            // 护甲与抗性
            WorldDatabase.DirectPExecute(
                "UPDATE item_template SET armor = LEAST(2100000000, ROUND(armor * {})), "
                "holy_res = ROUND(holy_res * {}), fire_res = ROUND(fire_res * {}), "
                "nature_res = ROUND(nature_res * {}), frost_res = ROUND(frost_res * {}), "
                "shadow_res = ROUND(shadow_res * {}), arcane_res = ROUND(arcane_res * {}) "
                "WHERE entry = {}",
                scale, scale, scale, scale, scale, scale, scale, ca.newEntry);

            /*
             * 耐久 —— 必须钳制在 65535 以内，不能跟着放大。
             *
             * 根因（实测踩到）：Item.cpp:365 存耐久用的是
             *     stmt->setUInt16(++index, GetUInt32Value(ITEM_FIELD_DURABILITY));
             * 是 uint16，上限 65535。
             * MaxDurability 放大到超过 65535 后，存库时溢出回绕成 0，
             * 于是 Item.h:107 的 IsBroken() 判定为真（MaxDur>0 且 Dur==0），
             * 而 Spell.cpp:7423 对带 SPELL_ATTR3_MAIN_HAND 的法术会检查：
             *     if (!item || item->IsBroken()) return SPELL_FAILED_EQUIPPED_ITEM_CLASS;
             * 结果就是「原版武器能放技能，克隆的不行」。
             *
             * 耐久放大本来也没意义（100 和 100 万都是「不会坏」），
             * 所以直接钳到安全值。
             */
            WorldDatabase.DirectPExecute(
                "UPDATE item_template SET MaxDurability = LEAST(60000, ROUND(MaxDurability * {})) "
                "WHERE entry = {} AND MaxDurability > 0", scale, ca.newEntry);
        }

        /*
         * 重载模板 + 重建 QueryData。
         * 两个都必须做：
         *   LoadItemTemplates()      把新物品读进内存
         *   InitializeQueryData()    conf 里 CacheDataQueries=1，
         *                            不重建客户端鼠标悬停看到的是空白
         */
        sObjectMgr->LoadItemTemplates();

        ItemTemplate const* created = sObjectMgr->GetItemTemplate(ca.newEntry);
        if (!created)
        {
            handler->PSendSysMessage("|cffff0000写入失败：重载后找不到 entry %u。|r", ca.newEntry);
            handler->PSendSysMessage("|cff888888请检查 world.item_template 表，或看服务端日志。|r");
            return true;
        }

        const_cast<ItemTemplate*>(created)->InitializeQueryData();

        handler->PSendSysMessage("|cff00ff00[锻造成功]|r %s", ItemForge::Link(created).c_str());
        handler->PSendSysMessage("entry = |cffffff00%u|r   装等 %u   %s",
            created->ItemId, created->ItemLevel, ItemForge::QualityName(created->Quality));
        handler->PSendSysMessage("拿到手：|cffffff00.additem %u|r", created->ItemId);
        return true;
    }

    // ==================================================================
    //  删除
    // ==================================================================
    static bool HandleDel(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item del <entry>|r");
            return true;
        }

        uint32 entry = uint32(atoi(tok[1].c_str()));

        // 只允许删自造段，防手滑删掉官方物品
        if (entry < ItemForge::ENTRY_BASE || entry > ItemForge::ENTRY_MAX)
        {
            handler->PSendSysMessage("|cffff0000只能删除自造装备（entry %u-%u）。|r",
                ItemForge::ENTRY_BASE, ItemForge::ENTRY_MAX);
            handler->PSendSysMessage("|cff888888这是防手滑保护，官方物品不能用本命令删。|r");
            return true;
        }

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
        if (!proto)
        {
            handler->PSendSysMessage("|cffff0000entry %u 不存在。|r", entry);
            return true;
        }

        std::string name = proto->Name1;
        WorldDatabase.DirectPExecute("DELETE FROM item_template WHERE entry = {}", entry);

        handler->PSendSysMessage("|cff00ff00[已删除]|r %s (entry %u)", name.c_str(), entry);
        handler->PSendSysMessage("|cffff8000注意：LoadItemTemplates() 不清空内存，|r");
        handler->PSendSysMessage("|cffff8000该物品仍在内存中，重启服务器后才真正消失。|r");
        handler->PSendSysMessage("|cff888888已发出去的实例会变成未知物品，记得回收。|r");
        return true;
    }

    // ==================================================================
    //  列出自造装备
    // ==================================================================
    static bool HandleList(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        uint32 page = 0;
        if (tok.size() >= 2 && IsNumber(tok[1]))
        {
            uint32 n = uint32(atoi(tok[1].c_str()));
            if (n >= 1)
                page = n - 1;
        }

        std::vector<uint32> mine;
        ItemTemplateContainer const& store = sObjectMgr->GetItemTemplateStore();
        for (auto const& kv : store)
        {
            if (kv.first >= ItemForge::ENTRY_BASE && kv.first <= ItemForge::ENTRY_MAX)
                mine.push_back(kv.first);
        }

        if (mine.empty())
        {
            handler->PSendSysMessage("|cffff8000还没有自造装备。|r");
            handler->PSendSysMessage("用 |cffffff00.item clone <源ID>|r 开始锻造。");
            return true;
        }

        std::sort(mine.begin(), mine.end());

        uint32 const perPage = 15;
        uint32 total = uint32(mine.size());
        uint32 maxPg = total ? ((total - 1) / perPage) : 0;
        if (page > maxPg)
            page = maxPg;

        uint32 begin = page * perPage;
        uint32 end   = std::min(begin + perPage, total);

        handler->PSendSysMessage("|cff00ff00===== 自造装备 第 %u/%u 页，共 %u 件 =====|r",
            page + 1, maxPg + 1, total);

        for (uint32 i = begin; i < end; ++i)
        {
            ItemTemplate const* p = sObjectMgr->GetItemTemplate(mine[i]);
            if (!p)
                continue;
            handler->PSendSysMessage("%s |cff888888ID:%u  装等%u|r",
                ItemForge::Link(p).c_str(), p->ItemId, p->ItemLevel);
        }

        if (maxPg > 0)
            handler->PSendSysMessage("|cff888888翻页：.item list %u|r",
                page + 2 > maxPg + 1 ? 1 : page + 2);
        return true;
    }

    // ==================================================================
    //  查看详情
    // ==================================================================
    static bool HandleInfo(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item info <entry>|r");
            return true;
        }

        uint32 entry = uint32(atoi(tok[1].c_str()));
        ItemTemplate const* p = sObjectMgr->GetItemTemplate(entry);
        if (!p)
        {
            handler->PSendSysMessage("|cffff0000entry %u 不存在。|r", entry);
            return true;
        }

        handler->PSendSysMessage("|cff00ff00===== %s =====|r", ItemForge::Link(p).c_str());
        handler->PSendSysMessage("entry %u   装等 %u   品质 %s   需求等级 %u",
            p->ItemId, p->ItemLevel, ItemForge::QualityName(p->Quality), p->RequiredLevel);
        handler->PSendSysMessage("class %u/%u   InventoryType %u   模型 %u",
            p->Class, p->SubClass, p->InventoryType, p->DisplayInfoID);

        if (p->StatsCount)
        {
            handler->PSendSysMessage("|cff00ff00--- 属性 ---|r");
            for (uint32 i = 0; i < p->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
            {
                if (!p->ItemStat[i].ItemStatValue)
                    continue;
                handler->PSendSysMessage("  %-10s %d",
                    ItemForge::StatName(p->ItemStat[i].ItemStatType),
                    p->ItemStat[i].ItemStatValue);
            }
        }

        if (p->Damage[0].DamageMax > 0)
            handler->PSendSysMessage("|cff00ff00--- 伤害 ---|r  %.0f-%.0f  速度 %.2f",
                double(p->Damage[0].DamageMin), double(p->Damage[0].DamageMax),
                double(p->Delay) / 1000.0);

        if (p->Armor)
            handler->PSendSysMessage("|cff00ff00--- 护甲 ---|r  %u", p->Armor);

        bool anyEffect = false;
        for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (p->Effects[i].SpellID <= 0)
                continue;
            if (!anyEffect)
            {
                handler->PSendSysMessage("|cff00ff00--- 特效 ---|r");
                anyEffect = true;
            }
            handler->PSendSysMessage("  法术 |cffffff00%d|r  %s  PPM %.1f  CD %dms  次数 %d",
                p->Effects[i].SpellID,
                ItemForge::TriggerName(p->Effects[i].TriggerType),
                double(p->Effects[i].SpellPPMRate),
                p->Effects[i].CoolDownMSec,
                p->Effects[i].Charges);
        }

        if (entry >= ItemForge::ENTRY_BASE && entry <= ItemForge::ENTRY_MAX)
            handler->PSendSysMessage("|cff888888（自造装备，可用 .item del %u 删除）|r", entry);

        return true;
    }

    // ==================================================================
    //  改装备类型
    //  .item type <entry> <选项...>
    // ==================================================================
    static bool HandleType(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 3 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item type <entry> <选项...>|r");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff00ff00--- 可用选项 ---|r");
            handler->PSendSysMessage("  |cff00ccff职业 圣骑士|r        限定单职业（可多个：职业 战士 圣骑士）");
            handler->PSendSysMessage("  |cff00ccff职业 全部|r          解除职业限制");
            handler->PSendSysMessage("  |cff00ccff类型 单手锤|r        改武器/护甲类型");
            handler->PSendSysMessage("  |cff00ccff部位 主手|r          改装备部位");
            handler->PSendSysMessage("  |cff00ccff等级 80|r            改需求等级");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("例：|cffffff00.item type 800000 职业 圣骑士 类型 单手锤 部位 主手|r");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cffff8000注意：法术自身也可能限制武器类型。|r");
            handler->PSendSysMessage("造之前先用 |cffffff00.item check <法术ID>|r 查它要什么武器。");
            return true;
        }

        uint32 entry = uint32(atoi(tok[1].c_str()));
        ItemTemplate const* p = sObjectMgr->GetItemTemplate(entry);
        if (!p)
        {
            handler->PSendSysMessage("|cffff0000entry %u 不存在。|r", entry);
            return true;
        }
        if (!InForgeRange(handler, entry))
            return true;

        uint32 newClassMask = 0;
        bool   setClassMask = false;
        bool   autoFixed    = false;
        int32  newCls = -1, newSub = -1, newInv = -1, newLvl = -1;

        for (size_t i = 2; i < tok.size(); ++i)
        {
            std::string const& t = tok[i];

            if (t == "职业" || t == "class")
            {
                setClassMask = true;
                // 后面可以跟多个职业名
                size_t j = i + 1;
                for (; j < tok.size(); ++j)
                {
                    uint32 m = ItemForge::ClassMaskFromName(tok[j]);
                    if (!m)
                        break;
                    newClassMask |= m;
                }
                if (j == i + 1)
                {
                    handler->PSendSysMessage("|cffff0000「职业」后面要跟职业名，如：职业 圣骑士|r");
                    return true;
                }
                i = j - 1;
                continue;
            }

            if (t == "类型" || t == "type")
            {
                if (i + 1 < tok.size())
                {
                    ItemForge::SubClassAlias const* sc = ItemForge::SubClassFromName(tok[i + 1]);
                    if (!sc)
                    {
                        handler->PSendSysMessage("|cffff0000不认识的类型「%s」。|r", tok[i + 1].c_str());
                        handler->PSendSysMessage("武器：单手剑 双手剑 单手斧 双手斧 单手锤 双手锤 长柄 法杖 匕首 拳套 弓 枪 弩 魔杖 投掷");
                        handler->PSendSysMessage("护甲：布甲 皮甲 锁甲 板甲 盾牌 圣契 神像 图腾 魔印 饰品");
                        return true;
                    }
                    newCls = int32(sc->cls);
                    newSub = int32(sc->sub);
                    // 部位没显式指定时，用类型的建议值
                    if (newInv < 0 && sc->invType)
                        newInv = int32(sc->invType);
                    ++i;
                }
                continue;
            }

            if (t == "部位" || t == "slot")
            {
                if (i + 1 < tok.size())
                {
                    uint32 iv = ItemForge::InvTypeFromName(tok[i + 1]);
                    if (!iv)
                    {
                        handler->PSendSysMessage("|cffff0000不认识的部位「%s」。|r", tok[i + 1].c_str());
                        handler->PSendSysMessage("头 肩 胸 腰 腿 脚 腕 手 披风 主手 副手 双手 单手 盾 远程 战袍 戒指 饰品 项链");
                        return true;
                    }
                    newInv = int32(iv);
                    ++i;
                }
                continue;
            }

            if (t == "等级" || t == "level" || t == "需求等级")
            {
                if (i + 1 < tok.size() && IsNumber(tok[i + 1]))
                {
                    newLvl = int32(atoi(tok[i + 1].c_str()));
                    ++i;
                }
                continue;
            }

            handler->PSendSysMessage("|cffff8000忽略无法识别的选项：%s|r", t.c_str());
        }

        // ---- 拼 UPDATE ----
        std::vector<std::string> sets;
        if (setClassMask)
            sets.push_back("AllowableClass = " + std::to_string(newClassMask));
        if (newCls >= 0)
            sets.push_back("class = " + std::to_string(newCls));
        if (newSub >= 0)
            sets.push_back("subclass = " + std::to_string(newSub));
        if (newInv >= 0)
            sets.push_back("InventoryType = " + std::to_string(newInv));

        /*
         * 改武器类型时自动补齐配套字段。
         *
         * 单手/双手不只是 subclass 的区别，还牵动：
         *   Delay   双手通常 3000-3800，单手 1500-2800
         *   Sheath  背在身上的位置（1=双手背后 3=单手腰侧）
         * 不改这些会出现「模型背在背上但判定是单手」这类怪现象。
         */
        if (newSub >= 0 && newCls == 2)
        {
            bool isTwoHand = (newSub == 1 || newSub == 5 || newSub == 6 ||
                              newSub == 8 || newSub == 10);   // 双手斧/锤/长柄/剑/法杖
            bool isRanged  = (newSub == 2 || newSub == 3 || newSub == 18 ||
                              newSub == 16 || newSub == 19);  // 弓/枪/弩/投掷/魔杖

            if (!isRanged)
            {
                // Sheath: 1=双手武器 3=单手武器
                sets.push_back(std::string("sheath = ") + (isTwoHand ? "1" : "3"));
                autoFixed = true;
            }
        }
        if (newLvl >= 0)
            sets.push_back("RequiredLevel = " + std::to_string(newLvl));

        if (sets.empty())
        {
            handler->PSendSysMessage("|cffff8000没有指定任何要改的东西。|r");
            return true;
        }

        std::string setClause;
        for (size_t i = 0; i < sets.size(); ++i)
        {
            if (i)
                setClause += ", ";
            setClause += sets[i];
        }

        WorldDatabase.DirectPExecute("UPDATE item_template SET {} WHERE entry = {}", setClause, entry);

        if (!ReloadOne(handler, entry))
            return true;

        ItemTemplate const* np = sObjectMgr->GetItemTemplate(entry);
        handler->PSendSysMessage("|cff00ff00[已修改]|r %s", ItemForge::Link(np).c_str());
        handler->PSendSysMessage("  职业限制：%s", ItemForge::ClassMaskToText(np->AllowableClass).c_str());
        handler->PSendSysMessage("  类型：%s (class %u/%u)",
            ItemForge::SubClassName(np->Class, np->SubClass), np->Class, np->SubClass);
        handler->PSendSysMessage("  部位：%s (%u)",
            ItemForge::InvTypeName(np->InventoryType), np->InventoryType);
        handler->PSendSysMessage("  需求等级：%u", np->RequiredLevel);

        if (autoFixed)
            handler->PSendSysMessage("|cff888888（已自动调整 sheath 以匹配单手/双手）|r");

        HintReequip(handler, entry);
        HintClientCache(handler);
        return true;
    }

    // ==================================================================
    //  改名字 / 描述
    //  .item name <entry> <新名字>
    //  .item name <entry> desc <描述文字>
    // ==================================================================
    static bool HandleName(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 3 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item name <entry> <新名字>|r");
            handler->PSendSysMessage("     |cffffff00.item name <entry> desc <描述>|r  改绿色描述文字");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("例：|cffffff00.item name 800001 真龙之牙|r");
            handler->PSendSysMessage("例：|cffffff00.item name 800001 desc 龙族的獠牙铸成|r");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff888888名字可以带空格，会自动拼接。|r");
            return true;
        }

        uint32 entry = uint32(atoi(tok[1].c_str()));
        if (!sObjectMgr->GetItemTemplate(entry))
        {
            handler->PSendSysMessage("|cffff0000entry %u 不存在。|r", entry);
            return true;
        }
        if (!InForgeRange(handler, entry))
            return true;

        bool isDesc = (tok[2] == "desc" || tok[2] == "描述");
        size_t from = isDesc ? 3 : 2;

        if (from >= tok.size())
        {
            handler->PSendSysMessage("|cffff0000请给出内容。|r");
            return true;
        }

        // 拼接剩余所有词（名字可以带空格）
        std::string text;
        for (size_t i = from; i < tok.size(); ++i)
        {
            if (!text.empty())
                text += " ";
            text += tok[i];
        }

        if (text.length() > 250)
        {
            handler->PSendSysMessage("|cffff0000太长了（上限 250 字符）。|r");
            return true;
        }

        WorldDatabase.DirectPExecute("UPDATE item_template SET {} = '{}' WHERE entry = {}",
            isDesc ? "description" : "name", ItemForge::Esc(text), entry);

        if (!ReloadOne(handler, entry))
            return true;

        ItemTemplate const* np = sObjectMgr->GetItemTemplate(entry);
        handler->PSendSysMessage("|cff00ff00[已改名]|r %s", ItemForge::Link(np).c_str());
        if (isDesc)
            handler->PSendSysMessage("  描述：|cff00ff00%s|r", text.c_str());
        HintClientCache(handler);
        return true;
    }

    // ==================================================================
    //  加/改属性
    //  .item stat <entry> <属性名> <数值> [属性名 数值 ...]
    //  .item stat <entry> clear
    // ==================================================================
    static bool HandleStat(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 3 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item stat <entry> <属性> <数值> [属性 数值...]|r");
            handler->PSendSysMessage("     |cffffff00.item stat <entry> clear|r   清空全部属性");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff00ff00--- 五维 ---|r  力量 敏捷 耐力 智力 精神");
            handler->PSendSysMessage("|cff00ff00--- 输出 ---|r  攻强 法强 暴击 命中 急速 精准 护甲穿透 法术穿透");
            handler->PSendSysMessage("|cff00ff00--- 防御 ---|r  防御 躲闪 招架 格挡 格挡值 韧性");
            handler->PSendSysMessage("|cff00ff00--- 回复 ---|r  回蓝 回血 生命 法力");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("例：|cffffff00.item stat 800000 力量 5000 暴击 2000 吸血 0|r");
            handler->PSendSysMessage("|cff888888最多 10 条属性（3.3.5 硬上限）。数值设 0 = 删除该属性。|r");
            return true;
        }

        uint32 entry = uint32(atoi(tok[1].c_str()));
        ItemTemplate const* p = sObjectMgr->GetItemTemplate(entry);
        if (!p)
        {
            handler->PSendSysMessage("|cffff0000entry %u 不存在。|r", entry);
            return true;
        }
        if (!InForgeRange(handler, entry))
            return true;

        // clear
        if (tok[2] == "clear" || tok[2] == "清空")
        {
            std::string sets = "StatsCount = 0";
            for (uint32 i = 1; i <= MAX_ITEM_PROTO_STATS; ++i)
            {
                sets += ", stat_type" + std::to_string(i) + " = 0";
                sets += ", stat_value" + std::to_string(i) + " = 0";
            }
            WorldDatabase.DirectPExecute("UPDATE item_template SET {} WHERE entry = {}", sets, entry);
            if (!ReloadOne(handler, entry))
                return true;
            handler->PSendSysMessage("|cff00ff00[已清空]|r 所有属性已移除。");
            return true;
        }

        // 读出现有属性到 map（保留原有的，除非被覆盖）
        std::vector<std::pair<uint32, int32>> stats;
        for (uint32 i = 0; i < p->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (p->ItemStat[i].ItemStatValue != 0)
                stats.emplace_back(p->ItemStat[i].ItemStatType, p->ItemStat[i].ItemStatValue);
        }

        // 解析 属性名 数值 对
        for (size_t i = 2; i + 1 < tok.size(); i += 2)
        {
            uint32 mod = ItemForge::StatModFromName(tok[i]);
            if (mod == ItemForge::STAT_INVALID)
            {
                handler->PSendSysMessage("|cffff0000不认识的属性「%s」。|r", tok[i].c_str());
                handler->PSendSysMessage("用 |cffffff00.item stat|r 看全部可用属性名。");
                return true;
            }

            std::string const& vs = tok[i + 1];
            bool neg = (!vs.empty() && vs[0] == '-');
            std::string digits = neg ? vs.substr(1) : vs;
            if (!IsNumber(digits))
            {
                handler->PSendSysMessage("|cffff0000「%s」不是有效数值。|r", vs.c_str());
                return true;
            }
            int64 v64 = atoll(digits.c_str());
            if (v64 > 2100000000LL)
                v64 = 2100000000LL;
            int32 val = int32(neg ? -v64 : v64);

            // 覆盖同名属性
            bool found = false;
            for (auto& kv : stats)
            {
                if (kv.first == mod)
                {
                    kv.second = val;
                    found = true;
                    break;
                }
            }
            if (!found && val != 0)
                stats.emplace_back(mod, val);
        }

        // 剔除值为 0 的
        stats.erase(std::remove_if(stats.begin(), stats.end(),
            [](std::pair<uint32, int32> const& kv) { return kv.second == 0; }), stats.end());

        if (stats.size() > MAX_ITEM_PROTO_STATS)
        {
            handler->PSendSysMessage("|cffff0000属性最多 %u 条（3.3.5 硬上限），当前 %u 条。|r",
                uint32(MAX_ITEM_PROTO_STATS), uint32(stats.size()));
            handler->PSendSysMessage("|cff888888先用 .item stat %u clear 清空再重设。|r", entry);
            return true;
        }

        // 写库
        std::string sets = "StatsCount = " + std::to_string(stats.size());
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            uint32 ty = (i < stats.size()) ? stats[i].first  : 0;
            int32  vl = (i < stats.size()) ? stats[i].second : 0;
            sets += ", stat_type"  + std::to_string(i + 1) + " = " + std::to_string(ty);
            sets += ", stat_value" + std::to_string(i + 1) + " = " + std::to_string(vl);
        }

        WorldDatabase.DirectPExecute("UPDATE item_template SET {} WHERE entry = {}", sets, entry);
        if (!ReloadOne(handler, entry))
            return true;

        ItemTemplate const* np = sObjectMgr->GetItemTemplate(entry);
        handler->PSendSysMessage("|cff00ff00[已修改]|r %s", ItemForge::Link(np).c_str());
        for (uint32 i = 0; i < np->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (!np->ItemStat[i].ItemStatValue)
                continue;
            handler->PSendSysMessage("  %-10s %d",
                ItemForge::StatName(np->ItemStat[i].ItemStatType),
                np->ItemStat[i].ItemStatValue);
        }
        HintReequip(handler, entry);
        return true;
    }

    // ==================================================================
    //  加/改技能
    //  .item spell <entry> <槽位1-5> <法术ID> [触发方式] [PPM] [冷却ms] [次数]
    //  .item spell <entry> <槽位> clear
    // ==================================================================
    static bool HandleSpell(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 4 || !IsNumber(tok[1]) || !IsNumber(tok[2]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item spell <entry> <槽位1-5> <法术ID> [触发] [PPM] [冷却ms] [次数]|r");
            handler->PSendSysMessage("     |cffffff00.item spell <entry> <槽位> clear|r  清除该槽位");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff00ff00--- 触发方式（每个槽位独立）---|r");
            handler->PSendSysMessage("  |cff00ccff装备|r    穿上就生效（被动光环用这个）");
            handler->PSendSysMessage("  |cff00ccff命中|r    命中时几率触发（**必须配 PPM**）");
            handler->PSendSysMessage("  |cff00ccff使用|r    右键点击发动");
            handler->PSendSysMessage("  |cff00ccff使用无cd|r 消耗品用");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("例：|cffffff00.item spell 800000 1 71903 命中 2.0|r");
            handler->PSendSysMessage("例：|cffffff00.item spell 800000 2 20375 装备|r");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff8888885 个槽位互不干扰，可以同时挂 5 个不同触发方式的技能。|r");
            handler->PSendSysMessage("|cffff8000造之前先 |r|cffffff00.item check <法术ID>|r|cffff8000 查它要什么武器。|r");
            return true;
        }

        uint32 entry = uint32(atoi(tok[1].c_str()));
        uint32 slot  = uint32(atoi(tok[2].c_str()));

        if (slot < 1 || slot > MAX_ITEM_PROTO_SPELLS)
        {
            handler->PSendSysMessage("|cffff0000槽位必须是 1-%u。|r", uint32(MAX_ITEM_PROTO_SPELLS));
            return true;
        }

        ItemTemplate const* p = sObjectMgr->GetItemTemplate(entry);
        if (!p)
        {
            handler->PSendSysMessage("|cffff0000entry %u 不存在。|r", entry);
            return true;
        }
        if (!InForgeRange(handler, entry))
            return true;

        // clear
        if (tok[3] == "clear" || tok[3] == "清除")
        {
            WorldDatabase.DirectPExecute(
                "UPDATE item_template SET spellid_{} = 0, spelltrigger_{} = 0, spellcharges_{} = 0, "
                "spellppmRate_{} = 0, spellcooldown_{} = -1, spellcategory_{} = 0, "
                "spellcategorycooldown_{} = -1 WHERE entry = {}",
                slot, slot, slot, slot, slot, slot, slot, entry);
            if (!ReloadOne(handler, entry))
                return true;
            handler->PSendSysMessage("|cff00ff00[已清除]|r 槽位 %u 的技能。", slot);
            return true;
        }

        if (!IsNumber(tok[3]))
        {
            handler->PSendSysMessage("|cffff0000法术ID 必须是数字。|r");
            return true;
        }
        uint32 spellId = uint32(atoi(tok[3].c_str()));

        // 校验法术存在
        SpellInfo const* si = sSpellMgr->GetSpellInfo(spellId);
        if (!si)
        {
            handler->PSendSysMessage("|cffff0000法术 %u 不存在。|r", spellId);
            return true;
        }

        // 默认值
        int32 trigger = 1;          // 装备时
        float ppm     = 0.0f;
        int32 cd      = -1;
        int32 charges = 0;

        // 解析可选参数
        size_t argi = 4;
        if (argi < tok.size())
        {
            int32 tr = ItemForge::TriggerFromName(tok[argi]);
            if (tr >= 0)
            {
                trigger = tr;
                ++argi;
            }
            else if (IsNumber(tok[argi]))
            {
                trigger = atoi(tok[argi].c_str());
                ++argi;
            }
        }
        if (argi < tok.size())
        {
            ppm = float(atof(tok[argi].c_str()));
            ++argi;
        }
        if (argi < tok.size() && IsNumber(tok[argi]))
        {
            cd = atoi(tok[argi].c_str());
            ++argi;
        }
        if (argi < tok.size())
        {
            charges = atoi(tok[argi].c_str());
            ++argi;
        }

        // 常见错误提醒：命中触发没配 PPM
        if (trigger == 2 && ppm <= 0.0f)
        {
            handler->PSendSysMessage("|cffff8000提醒：触发方式是「命中触发」但 PPM 是 0，|r");
            handler->PSendSysMessage("|cffff8000这样永远不会触发。已自动设为 1.0。|r");
            ppm = 1.0f;
        }

        WorldDatabase.DirectPExecute(
            "UPDATE item_template SET spellid_{} = {}, spelltrigger_{} = {}, spellcharges_{} = {}, "
            "spellppmRate_{} = {}, spellcooldown_{} = {} WHERE entry = {}",
            slot, spellId, slot, trigger, slot, charges, slot, ppm, slot, cd, entry);

        if (!ReloadOne(handler, entry))
            return true;

        handler->PSendSysMessage("|cff00ff00[已设置]|r 槽位 %u", slot);
        handler->PSendSysMessage("  法术：|cffffff00%u|r  %s", spellId,
            si->SpellName[handler->GetSessionDbcLocale()] ? si->SpellName[handler->GetSessionDbcLocale()] : "");
        handler->PSendSysMessage("  触发：%s   PPM %.1f   CD %dms   次数 %d",
            ItemForge::TriggerName(uint32(trigger)), double(ppm), cd, charges);

        // 武器要求提示
        if (si->EquippedItemClass >= 0)
        {
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cffff8000该法术对武器有要求：|r");
            ShowSpellItemReq(handler, si);
        }
        HintReequip(handler, entry);
        return true;
    }

    // ==================================================================
    //  查法术的武器要求
    //  .item check <法术ID>
    // ==================================================================
    static bool HandleCheck(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item check <法术ID>|r");
            handler->PSendSysMessage("查这个法术要求什么武器/装备才能放出来。");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff888888为什么需要这个：装备的 AllowableClass 只管「谁能穿」，|r");
            handler->PSendSysMessage("|cff888888法术自己还有 EquippedItemClass 管「要什么武器」。|r");
            handler->PSendSysMessage("|cff888888两层都过了技能才放得出来。|r");
            return true;
        }

        uint32 spellId = uint32(atoi(tok[1].c_str()));
        SpellInfo const* si = sSpellMgr->GetSpellInfo(spellId);
        if (!si)
        {
            handler->PSendSysMessage("|cffff0000法术 %u 不存在。|r", spellId);
            return true;
        }

        LocaleConstant loc = handler->GetSessionDbcLocale();
        handler->PSendSysMessage("|cff00ff00===== 法术 %u =====|r", spellId);
        handler->PSendSysMessage("名称：%s", si->SpellName[loc] ? si->SpellName[loc] : "(无名)");

        if (si->EquippedItemClass < 0)
        {
            handler->PSendSysMessage("|cff00ff00对武器无要求|r —— 任何装备都能挂这个法术。");
            return true;
        }

        ShowSpellItemReq(handler, si);
        return true;
    }

    static void ShowSpellItemReq(ChatHandler* handler, SpellInfo const* si)
    {
        handler->PSendSysMessage("  要求 ItemClass = |cffffff00%d|r %s",
            si->EquippedItemClass,
            si->EquippedItemClass == 2 ? "(武器)" :
            si->EquippedItemClass == 4 ? "(护甲)" : "");

        if (si->EquippedItemSubClassMask)
        {
            std::string subs;
            for (uint32 i = 0; i < 21; ++i)
            {
                if (!(si->EquippedItemSubClassMask & (1 << i)))
                    continue;
                if (!subs.empty())
                    subs += " ";
                subs += ItemForge::SubClassName(uint32(si->EquippedItemClass), i);
            }
            handler->PSendSysMessage("  允许的类型：|cff00ccff%s|r", subs.c_str());
            handler->PSendSysMessage("  |cff888888.item type <entry> 类型 %s|r",
                subs.substr(0, subs.find(' ')).c_str());
        }

        if (si->EquippedItemInventoryTypeMask)
        {
            std::string invs;
            for (uint32 i = 0; i < 28; ++i)
            {
                if (!(si->EquippedItemInventoryTypeMask & (1 << i)))
                    continue;
                if (!invs.empty())
                    invs += " ";
                invs += ItemForge::InvTypeName(i);
            }
            handler->PSendSysMessage("  允许的部位：|cff00ccff%s|r", invs.c_str());
        }
    }

    // ==================================================================
    //  直查数据库原始值，并与内存对比
    //  .item raw <entry>
    //
    //  用途：分清「数据库没写进去」还是「内存没重载」还是「客户端缓存」。
    // ==================================================================
    static bool HandleRaw(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item raw <entry>|r");
            handler->PSendSysMessage("直接查数据库，和服务端内存对比，定位问题在哪一层。");
            return true;
        }

        uint32 entry = uint32(atoi(tok[1].c_str()));

        QueryResult r = WorldDatabase.PQuery(
            "SELECT name, class, subclass, InventoryType, delay, sheath, "
            "AllowableClass, ItemLevel, RequiredLevel, MaxDurability, dmg_min1, dmg_max1 "
            "FROM item_template WHERE entry = {}", entry);

        if (!r)
        {
            handler->PSendSysMessage("|cffff0000数据库里没有 entry %u。|r", entry);
            handler->PSendSysMessage("|cffff8000-> 说明写库根本没成功。|r");
            return true;
        }

        Field* f = r->Fetch();
        std::string dbName = f[0].GetString();
        uint32 dbClass  = f[1].GetUInt32();
        uint32 dbSub    = f[2].GetUInt32();
        uint32 dbInv    = f[3].GetUInt32();
        uint32 dbDelay  = f[4].GetUInt32();
        uint32 dbSheath = f[5].GetUInt32();

        ItemTemplate const* mem = sObjectMgr->GetItemTemplate(entry);

        handler->PSendSysMessage("|cff00ff00===== entry %u =====|r", entry);
        handler->PSendSysMessage("|cff00ccff字段          数据库          服务端内存|r");

        auto row = [&](char const* n, std::string const& db, std::string const& m)
        {
            bool same = (db == m);
            handler->PSendSysMessage("%-12s %-14s %-14s %s", n, db.c_str(), m.c_str(),
                same ? "|cff00ff00一致|r" : "|cffff0000不一致！|r");
        };

        row("name",    dbName, mem ? mem->Name1 : "(内存无)");
        row("class",   std::to_string(dbClass),  mem ? std::to_string(mem->Class) : "-");
        row("subclass",std::to_string(dbSub),    mem ? std::to_string(mem->SubClass) : "-");
        row("InvType", std::to_string(dbInv),    mem ? std::to_string(mem->InventoryType) : "-");
        row("delay",   std::to_string(dbDelay),  mem ? std::to_string(mem->Delay) : "-");
        row("sheath",  std::to_string(dbSheath), mem ? std::to_string(mem->Sheath) : "-");

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00--- 怎么看 ---|r");
        handler->PSendSysMessage("|cff888888数据库=新值 内存=新值|r -> 服务端OK，是|cffff8000客户端缓存|r");
        handler->PSendSysMessage("|cff888888数据库=新值 内存=旧值|r -> |cffff8000重载失败|r，重启服务端");
        handler->PSendSysMessage("|cff888888数据库=旧值|r          -> |cffff8000写库失败|r，看服务端日志");
        return true;
    }

    // ==================================================================
    //  逐字段对比两件装备
    //  .item diff <entryA> <entryB>
    //
    //  用途：「原版武器能放技能，克隆的不行」这类问题，
    //  直接对比两者所有影响施法判定的字段，差异一目了然。
    // ==================================================================
    static bool HandleDiff(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 3 || !IsNumber(tok[1]) || !IsNumber(tok[2]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item diff <原版entry> <克隆entry>|r");
            handler->PSendSysMessage("逐字段对比，找出为什么一个能用一个不能用。");
            handler->PSendSysMessage("例：|cffffff00.item diff 40395 800000|r");
            return true;
        }

        uint32 ea = uint32(atoi(tok[1].c_str()));
        uint32 eb = uint32(atoi(tok[2].c_str()));

        ItemTemplate const* a = sObjectMgr->GetItemTemplate(ea);
        ItemTemplate const* b = sObjectMgr->GetItemTemplate(eb);
        if (!a) { handler->PSendSysMessage("|cffff0000entry %u 不存在。|r", ea); return true; }
        if (!b) { handler->PSendSysMessage("|cffff0000entry %u 不存在。|r", eb); return true; }

        handler->PSendSysMessage("|cff00ff00===== 对比 %u vs %u =====|r", ea, eb);
        handler->PSendSysMessage("A: %s", ItemForge::Link(a).c_str());
        handler->PSendSysMessage("B: %s", ItemForge::Link(b).c_str());
        handler->PSendSysMessage(" ");

        uint32 diffs = 0;

        // 宏：只在不同时输出，红色标注
        auto cmpU = [&](char const* name, uint32 va, uint32 vb, char const* hint)
        {
            if (va == vb)
                return;
            ++diffs;
            handler->PSendSysMessage("|cffff0000[差异]|r %-18s A=%u  B=%u", name, va, vb);
            if (hint && *hint)
                handler->PSendSysMessage("        |cff888888%s|r", hint);
        };

        // ---- 影响「能不能放技能」的字段 ----
        cmpU("class",          a->Class,          b->Class,
             "决定 IsFitToSpellRequirements 第一关（Item.cpp:864）");
        cmpU("subclass",       a->SubClass,       b->SubClass,
             "决定 SubClassMask 匹配（Item.cpp:869）");
        cmpU("InventoryType",  a->InventoryType,  b->InventoryType,
             "决定装到哪个槽位。主手技能要求装在 MAINHAND(15) 槽");
        cmpU("AllowableClass", a->AllowableClass, b->AllowableClass,
             "谁能装备。0 或 -1 都表示全职业");
        cmpU("RequiredLevel",  a->RequiredLevel,  b->RequiredLevel, "需求等级");
        cmpU("MaxDurability",  a->MaxDurability,  b->MaxDurability,
             "为 0 = 不会损坏；>0 但实例耐久为 0 = IsBroken 判定损坏");
        cmpU("Delay",          a->Delay,          b->Delay, "武器速度(毫秒)，0 会导致伤害计算异常");
        cmpU("Quality",        a->Quality,        b->Quality, "");
        cmpU("ItemLevel",      a->ItemLevel,      b->ItemLevel, "");
        cmpU("Sheath",         a->Sheath,         b->Sheath, "武器背在身上的位置，不影响施法");
        cmpU("Bonding",        a->Bonding,        b->Bonding, "");
        cmpU("Material",       uint32(a->Material), uint32(b->Material), "");
        cmpU("Flags[0]",       a->Flags[0],       b->Flags[0],
             "物品标志位。某些位会影响可用性");
        cmpU("Flags[1]",       a->Flags[1],       b->Flags[1], "");
        cmpU("FlagsCu",        a->FlagsCu,        b->FlagsCu, "TrinityCore 自定义标志");
        cmpU("ItemSet",        a->ItemSet,        b->ItemSet, "套装ID");
        cmpU("ScalingStatDistribution", a->ScalingStatDistribution, b->ScalingStatDistribution,
             "|cffff8000非0会让装备走「随等级缩放」逻辑，属性可能显示异常|r");
        cmpU("ScalingStatValue", a->ScalingStatValue, b->ScalingStatValue, "同上");

        // ---- 武器伤害 ----
        if (a->Damage[0].DamageMin != b->Damage[0].DamageMin ||
            a->Damage[0].DamageMax != b->Damage[0].DamageMax)
        {
            ++diffs;
            handler->PSendSysMessage("|cffff0000[差异]|r %-18s A=%.0f-%.0f  B=%.0f-%.0f",
                "伤害",
                double(a->Damage[0].DamageMin), double(a->Damage[0].DamageMax),
                double(b->Damage[0].DamageMin), double(b->Damage[0].DamageMax));
        }

        // ---- 技能槽 ----
        for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (a->Effects[i].SpellID == b->Effects[i].SpellID &&
                a->Effects[i].TriggerType == b->Effects[i].TriggerType)
                continue;
            ++diffs;
            handler->PSendSysMessage("|cffff0000[差异]|r 技能槽%u  A=%d(%s)  B=%d(%s)",
                i + 1,
                a->Effects[i].SpellID, ItemForge::TriggerName(a->Effects[i].TriggerType),
                b->Effects[i].SpellID, ItemForge::TriggerName(b->Effects[i].TriggerType));
        }

        handler->PSendSysMessage(" ");
        if (!diffs)
        {
            handler->PSendSysMessage("|cff00ff00两者模板完全一致。|r");
            handler->PSendSysMessage("|cff888888既然模板一样，问题在【物品实例】不在模板：|r");
            handler->PSendSysMessage("|cff888888  · 实例耐久为 0（用 .item fix 修）|r");
            handler->PSendSysMessage("|cff888888  · 没真正装到主手槽（脱下重穿）|r");
        }
        else
            handler->PSendSysMessage("|cffff8000共 %u 处差异，红色项即可能的原因。|r", diffs);

        return true;
    }

    // ==================================================================
    //  修复损坏装备（耐久归零）
    //  .item fix [entry]     不写 entry = 修全身
    // ==================================================================
    static bool HandleFix(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        uint32 onlyEntry = 0;
        if (tok.size() >= 2 && IsNumber(tok[1]))
            onlyEntry = uint32(atoi(tok[1].c_str()));

        uint32 fixed = 0;

        // 装备栏 + 背包全扫
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!it)
                continue;
            if (onlyEntry && it->GetEntry() != onlyEntry)
                continue;

            uint32 maxDur = it->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
            uint32 curDur = it->GetUInt32Value(ITEM_FIELD_DURABILITY);
            if (maxDur == 0 || curDur >= maxDur)
                continue;

            it->SetUInt32Value(ITEM_FIELD_DURABILITY, maxDur);
            it->SetState(ITEM_CHANGED, player);
            ++fixed;

            ItemTemplate const* pr = it->GetTemplate();
            handler->PSendSysMessage("|cff00ff00[已修复]|r %s  耐久 %u -> %u",
                ItemForge::Link(pr).c_str(), curDur, maxDur);
        }

        if (!fixed)
        {
            handler->PSendSysMessage("|cffff8000没有需要修复的装备。|r");
            handler->PSendSysMessage("|cff888888（只扫描身上穿着的，背包里的请先穿上）|r");
        }
        else
            handler->PSendSysMessage("|cff00ff00共修复 %u 件。|r", fixed);

        return true;
    }

    // ==================================================================
    //  诊断：为什么这个法术识别不到我的武器
    //  .item why <法术ID>
    // ==================================================================
    static bool HandleWhy(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        if (tok.size() < 2 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item why <法术ID>|r");
            handler->PSendSysMessage("诊断：为什么这个法术识别不到你手上的武器。");
            handler->PSendSysMessage("例：|cffffff00.item why 35395|r  （十字军打击）");
            return true;
        }

        uint32 spellId = uint32(atoi(tok[1].c_str()));
        SpellInfo const* si = sSpellMgr->GetSpellInfo(spellId);
        if (!si)
        {
            handler->PSendSysMessage("|cffff0000法术 %u 不存在。|r", spellId);
            return true;
        }

        LocaleConstant loc = handler->GetSessionDbcLocale();
        handler->PSendSysMessage("|cff00ff00===== 诊断 %s (%u) =====|r",
            si->SpellName[loc] ? si->SpellName[loc] : "?", spellId);

        // ---- 法术要求 ----
        if (si->EquippedItemClass < 0)
        {
            handler->PSendSysMessage("|cff00ff00该法术对武器无要求|r —— 放不出来是别的原因");
            handler->PSendSysMessage("|cff888888（可能是职业不符/等级不够/天赋没点/资源不足）|r");
            return true;
        }

        handler->PSendSysMessage("法术要求：class=|cffffff00%d|r  subMask=|cffffff00%d|r",
            si->EquippedItemClass, si->EquippedItemSubClassMask);

        if (si->EquippedItemSubClassMask)
        {
            std::string subs;
            for (uint32 i = 0; i < 21; ++i)
            {
                if (!(si->EquippedItemSubClassMask & (1 << i)))
                    continue;
                if (!subs.empty())
                    subs += " ";
                subs += ItemForge::SubClassName(uint32(si->EquippedItemClass), i);
            }
            handler->PSendSysMessage("允许类型：|cff00ccff%s|r", subs.c_str());
        }

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00--- 你身上的装备 ---|r");

        // ---- 逐槽检查（复刻 Player::HasItemFitToSpellRequirements 的扫描范围）----
        bool anyFit = false;
        uint8 from = (si->EquippedItemClass == 2) ? EQUIPMENT_SLOT_MAINHAND : EQUIPMENT_SLOT_START;
        uint8 to   = (si->EquippedItemClass == 2) ? EQUIPMENT_SLOT_TABARD   : EQUIPMENT_SLOT_MAINHAND;

        for (uint8 slot = from; slot < to; ++slot)
        {
            Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!it)
                continue;

            ItemTemplate const* pr = it->GetTemplate();
            if (!pr)
                continue;

            char const* slotName =
                slot == EQUIPMENT_SLOT_MAINHAND ? "主手" :
                slot == EQUIPMENT_SLOT_OFFHAND  ? "副手" :
                slot == EQUIPMENT_SLOT_RANGED   ? "远程" : "其他";

            bool clsOk = (si->EquippedItemClass == int32(pr->Class));
            bool subOk = (si->EquippedItemSubClassMask == 0) ||
                         ((si->EquippedItemSubClassMask & (1 << pr->SubClass)) != 0);

            // 这一步是关键：GetUseableItemByPos 会先过 CanUseAttackType
            Item* useable = player->GetUseableItemByPos(INVENTORY_SLOT_BAG_0, slot);
            bool  usable  = (useable != nullptr);

            bool fit = it->IsFitToSpellRequirements(si);

            /*
             * 关键：耐久检查。
             * Spell.cpp:7423 对带 SPELL_ATTR3_MAIN_HAND 的法术有独立检查：
             *     if (!item || item->IsBroken()) return SPELL_FAILED_EQUIPPED_ITEM_CLASS;
             * Item.h:107  IsBroken() = MaxDurability > 0 && Durability == 0
             * 这条不在 IsFitToSpellRequirements 里，所以必须单独查。
             */
            bool broken = it->IsBroken();

            if (fit && usable && !broken)
                anyFit = true;

            handler->PSendSysMessage("|cff00ccff[%s]|r %s", slotName, ItemForge::Link(pr).c_str());
            handler->PSendSysMessage("   class=%u %s   subclass=%u(%s) %s",
                pr->Class,   clsOk ? "|cff00ff00OK|r" : "|cffff0000不符|r",
                pr->SubClass, ItemForge::SubClassName(pr->Class, pr->SubClass),
                subOk ? "|cff00ff00OK|r" : "|cffff0000不在允许列表|r");
            handler->PSendSysMessage("   InventoryType=%u(%s)   %s",
                pr->InventoryType, ItemForge::InvTypeName(pr->InventoryType),
                usable ? "|cff00ff00可用|r" : "|cffff0000被缴械/不可用|r");
            handler->PSendSysMessage("   耐久 %u/%u   %s",
                it->GetUInt32Value(ITEM_FIELD_DURABILITY),
                it->GetUInt32Value(ITEM_FIELD_MAXDURABILITY),
                broken ? "|cffff0000已损坏！法术会被拒绝|r" : "|cff00ff00完好|r");

            if (broken)
            {
                handler->PSendSysMessage("   |cffff0000-> 这就是原因：耐久为 0 的武器视为「损坏」|r");
                handler->PSendSysMessage("   |cffffff00.item fix %u|r |cff888888修好它|r", pr->ItemId);
            }

            if (!fit)
            {
                if (!clsOk)
                    handler->PSendSysMessage("   |cffff0000-> class 不符，改：|r|cffffff00.item type %u 类型 <正确类型>|r",
                        pr->ItemId);
                else if (!subOk)
                    handler->PSendSysMessage("   |cffff0000-> subclass 不在允许列表|r");
            }
        }

        handler->PSendSysMessage(" ");
        if (anyFit)
        {
            handler->PSendSysMessage("|cff00ff00结论：武器符合要求，法术应该能放出来。|r");
            handler->PSendSysMessage("|cff888888还是放不出？检查这些：|r");
            handler->PSendSysMessage("|cff888888  1. 你的职业能不能学这个法术|r");
            handler->PSendSysMessage("|cff888888  2. 是不是没学会（.learn %u）|r", spellId);
            handler->PSendSysMessage("|cff888888  3. 天赋要求 / 等级要求 / 资源不足|r");
        }
        else
        {
            handler->PSendSysMessage("|cffff0000结论：没有任何装备满足要求。|r");
            handler->PSendSysMessage("|cffff8000注意：改了 .item type 之后必须【脱下再穿上】。|r");
            handler->PSendSysMessage("|cff888888装备的属性和类型在穿上那一刻结算，改模板不会自动重算。|r");
        }
        return true;
    }

    // ==================================================================
    //  改武器伤害 / 护甲 / 速度
    //  .item combat <entry> [伤害 100 200] [速度 2.6] [护甲 5000]
    // ==================================================================
    static bool HandleCombat(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 3 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item combat <entry> [选项...]|r");
            handler->PSendSysMessage("  |cff00ccff伤害 100 200|r   武器最小/最大伤害");
            handler->PSendSysMessage("  |cff00ccff速度 2.6|r       攻击速度（秒）");
            handler->PSendSysMessage("  |cff00ccff护甲 5000|r      护甲值");
            handler->PSendSysMessage("  |cff00ccff耐久 200|r       最大耐久");
            handler->PSendSysMessage("例：|cffffff00.item combat 800000 伤害 5000 8000 速度 2.6|r");
            return true;
        }

        uint32 entry = uint32(atoi(tok[1].c_str()));
        if (!sObjectMgr->GetItemTemplate(entry))
        {
            handler->PSendSysMessage("|cffff0000entry %u 不存在。|r", entry);
            return true;
        }
        if (!InForgeRange(handler, entry))
            return true;

        std::vector<std::string> sets;

        for (size_t i = 2; i < tok.size(); ++i)
        {
            std::string const& t = tok[i];

            if ((t == "伤害" || t == "dmg") && i + 2 < tok.size())
            {
                sets.push_back("dmg_min1 = " + tok[i + 1]);
                sets.push_back("dmg_max1 = " + tok[i + 2]);
                i += 2;
                continue;
            }
            if ((t == "速度" || t == "speed") && i + 1 < tok.size())
            {
                double sec = atof(tok[i + 1].c_str());
                if (sec > 0.0)
                    sets.push_back("delay = " + std::to_string(uint32(sec * 1000.0)));
                ++i;
                continue;
            }
            if ((t == "护甲" || t == "armor") && i + 1 < tok.size() && IsNumber(tok[i + 1]))
            {
                sets.push_back("armor = " + tok[i + 1]);
                ++i;
                continue;
            }
            if ((t == "耐久" || t == "durability") && i + 1 < tok.size() && IsNumber(tok[i + 1]))
            {
                // 耐久存库是 uint16（Item.cpp:365 的 setUInt16），超 65535 会回绕成 0
                // 导致武器被判定为「已损坏」，主手类技能直接放不出来
                int64 d = atoll(tok[i + 1].c_str());
                if (d > 60000)
                {
                    d = 60000;
                    handler->PSendSysMessage("|cffff8000耐久上限 60000（存库是 uint16），已自动钳制。|r");
                }
                sets.push_back("MaxDurability = " + std::to_string(d));
                ++i;
                continue;
            }
            handler->PSendSysMessage("|cffff8000忽略：%s|r", t.c_str());
        }

        if (sets.empty())
        {
            handler->PSendSysMessage("|cffff8000没有指定任何要改的东西。|r");
            return true;
        }

        std::string setClause;
        for (size_t i = 0; i < sets.size(); ++i)
        {
            if (i)
                setClause += ", ";
            setClause += sets[i];
        }

        WorldDatabase.DirectPExecute("UPDATE item_template SET {} WHERE entry = {}", setClause, entry);
        if (!ReloadOne(handler, entry))
            return true;

        ItemTemplate const* np = sObjectMgr->GetItemTemplate(entry);
        handler->PSendSysMessage("|cff00ff00[已修改]|r %s", ItemForge::Link(np).c_str());
        if (np->Damage[0].DamageMax > 0)
            handler->PSendSysMessage("  伤害 %.0f-%.0f   速度 %.2f",
                double(np->Damage[0].DamageMin), double(np->Damage[0].DamageMax),
                double(np->Delay) / 1000.0);
        if (np->Armor)
            handler->PSendSysMessage("  护甲 %u", np->Armor);
        HintReequip(handler, entry);
        return true;
    }

    // ==================================================================
    //  共用：范围检查 + 单个重载
    // ==================================================================
    static bool InForgeRange(ChatHandler* handler, uint32 entry)
    {
        if (entry < ItemForge::ENTRY_BASE || entry > ItemForge::ENTRY_MAX)
        {
            handler->PSendSysMessage("|cffff0000只能改自造装备（entry %u-%u）。|r",
                ItemForge::ENTRY_BASE, ItemForge::ENTRY_MAX);
            handler->PSendSysMessage("|cff888888防手滑保护：不允许直接改官方物品。|r");
            handler->PSendSysMessage("|cff888888想改官方装备，先 |r|cffffff00.item clone %u -y|r|cff888888 复制一份。|r", entry);
            return false;
        }
        return true;
    }

    /*
     * 提示玩家「脱了再穿」。
     *
     * 为什么需要：Item::GetTemplate()（Item.cpp:540）是每次实时查表，
     * 所以改模板后 tooltip 立刻变。
     * 但 Player::_ApplyItemBonuses()（Player.h:1925）是在【装备的那一刻】
     * 把属性算进人物面板的，之后不会重算 ——
     * 结果就是 tooltip 显示新属性，角色面板却还是旧数值。
     *
     * 只在玩家真的穿着这件装备时才提示，避免刷屏。
     */
    static void HintReequip(ChatHandler* handler, uint32 entry)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (it && it->GetEntry() == entry)
            {
                handler->PSendSysMessage(" ");
                handler->PSendSysMessage("|cffff8000你正穿着这件装备 —— 脱下再穿上才会生效。|r");
                handler->PSendSysMessage("|cff888888（属性加成在装备瞬间结算，改模板不会自动重算）|r");
                return;
            }
        }
    }

    /*
     * 提示客户端缓存问题。
     *
     * 服务端 InitializeQueryData() 只是重建了「要发给客户端的包」，
     * 但客户端自己在 WDB\ItemCache.wdb 里也存了一份。
     * 客户端优先用本地缓存，不会主动重新请求 ——
     * 于是出现「服务端数据已改，客户端还按旧数据判断」。
     *
     * 典型症状：改了 subclass 成单手剑，客户端仍认为是双手剑，
     * 于是「必须装备单手近战武器」的技能放不出来。
     */
    static void HintClientCache(ChatHandler* handler)
    {
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cffff8000改了类型/名字后，客户端可能还在用旧缓存。|r");
        handler->PSendSysMessage("|cff888888如果技能仍识别不到武器，删掉客户端的|r |cffffff00WDB|r |cff888888文件夹再重登。|r");
    }

    // 重载单个物品并重建客户端缓存
    static bool ReloadOne(ChatHandler* handler, uint32 entry)
    {
        sObjectMgr->LoadItemTemplates();
        ItemTemplate const* p = sObjectMgr->GetItemTemplate(entry);
        if (!p)
        {
            handler->PSendSysMessage("|cffff0000重载后找不到 entry %u，写入可能失败。|r", entry);
            return false;
        }
        // conf 里 CacheDataQueries=1，不重建客户端看到的是旧数据
        const_cast<ItemTemplate*>(p)->InitializeQueryData();
        return true;
    }

    // ==================================================================
    //  套装总入口
    // ==================================================================
    static bool HandleSet(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            ShowSetHelp(handler);
            return true;
        }

        std::string const& op = tok[1];

        if (op == "list")   return HandleSetList(handler, tok);
        if (op == "info")   return HandleSetInfo(handler, tok);
        if (op == "clone")  return HandleSetClone(handler, tok);
        if (op == "bind")   return HandleSetBind(handler, tok);
        if (op == "unbind") return HandleSetUnbind(handler, tok);
        if (op == "new")    return HandleSetNew(handler, tok);

        ShowSetHelp(handler);
        return true;
    }

    static void ShowSetHelp(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ff00===== 套装系统 =====|r");
        handler->PSendSysMessage("|cffffff00.item set list [关键词]|r     浏览现成套装");
        handler->PSendSysMessage("|cffffff00.item set info <setId>|r      看套装效果");
        handler->PSendSysMessage("|cffffff00.item set clone <setId>|r     克隆整套");
        handler->PSendSysMessage("|cffffff00.item set bind <entry> <setId>|r  单件挂套装");
        handler->PSendSysMessage("|cffffff00.item set unbind <entry>|r    解除绑定");
        handler->PSendSysMessage("|cffffff00.item set new <名字> <法术ID:件数>...|r  自定义套装(要补丁)");
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00--- 两条路 ---|r");
        handler->PSendSysMessage("|cff00ccff借壳|r  复用暴雪现成套装效果，|cff00ff00零补丁立刻能用|r");
        handler->PSendSysMessage("|cff00ccff新建|r  自定义效果，要发客户端 DBC 补丁");
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("例：|cffffff00.item set clone 831 装等300 -y|r  克隆T10死骑套");
    }

    // ==================================================================
    //  浏览现成套装
    // ==================================================================
    static bool HandleSetList(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        std::string kw;
        uint32 page = 0;

        for (size_t i = 2; i < tok.size(); ++i)
        {
            if (IsNumber(tok[i]))
            {
                uint32 n = uint32(atoi(tok[i].c_str()));
                if (n >= 1)
                    page = n - 1;
            }
            else
            {
                if (!kw.empty())
                    kw += " ";
                kw += tok[i];
            }
        }

        uint32 loc = uint32(handler->GetSessionDbcLocale());
        std::string kwLower = ToLowerStr(kw);

        struct Hit { uint32 id; std::string name; uint32 pieces; uint32 spells; };
        std::vector<Hit> hits;

        for (uint32 i = 0; i < sItemSetStore.GetNumRows(); ++i)
        {
            ItemSetEntry const* se = sItemSetStore.LookupEntry(i);
            if (!se)
                continue;

            char const* nm = se->Name[loc] ? se->Name[loc] : se->Name[0];
            if (!nm || !*nm)
                continue;

            std::string name = nm;
            if (!kwLower.empty() && ToLowerStr(name).find(kwLower) == std::string::npos)
                continue;

            uint32 pieces = 0;
            for (uint32 k = 0; k < MAX_ITEM_SET_ITEMS; ++k)
                if (se->ItemID[k])
                    ++pieces;

            uint32 spells = 0;
            for (uint32 k = 0; k < MAX_ITEM_SET_SPELLS; ++k)
                if (se->SetSpellID[k])
                    ++spells;

            if (!spells)      // 没有套装效果的跳过，借壳没意义
                continue;

            Hit h;
            h.id = i; h.name = name; h.pieces = pieces; h.spells = spells;
            hits.push_back(h);
        }

        if (hits.empty())
        {
            handler->PSendSysMessage("|cffff8000没找到含「%s」的套装。|r", kw.c_str());
            return true;
        }

        uint32 const perPage = 15;
        uint32 total = uint32(hits.size());
        uint32 maxPg = total ? ((total - 1) / perPage) : 0;
        if (page > maxPg)
            page = maxPg;

        uint32 begin = page * perPage;
        uint32 end   = std::min(begin + perPage, total);

        handler->PSendSysMessage("|cff00ff00===== 套装%s 第 %u/%u 页，共 %u 个 =====|r",
            kw.empty() ? "" : ("「" + kw + "」").c_str(), page + 1, maxPg + 1, total);

        for (uint32 i = begin; i < end; ++i)
        {
            Hit const& h = hits[i];
            handler->PSendSysMessage("|cffa335ee%s|r  |cff888888ID:|r|cffffff00%u|r  %u件 %u效果",
                h.name.c_str(), h.id, h.pieces, h.spells);
        }

        handler->PSendSysMessage(" ");
        if (maxPg > 0)
            handler->PSendSysMessage("|cff888888翻页：.item set list %s %u|r",
                kw.c_str(), page + 2 > maxPg + 1 ? 1 : page + 2);
        handler->PSendSysMessage("|cff888888看效果：|r|cffffff00.item set info <ID>|r"
                                 "   |cff888888克隆：|r|cffffff00.item set clone <ID> -y|r");
        return true;
    }

    // ==================================================================
    //  查看套装详情
    // ==================================================================
    static bool HandleSetInfo(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 3 || !IsNumber(tok[2]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item set info <setId>|r");
            return true;
        }

        uint32 sid = uint32(atoi(tok[2].c_str()));
        ItemSetEntry const* se = sItemSetStore.LookupEntry(sid);
        if (!se)
        {
            handler->PSendSysMessage("|cffff0000套装 %u 不存在。|r", sid);
            return true;
        }

        uint32 loc = uint32(handler->GetSessionDbcLocale());
        LocaleConstant lc = handler->GetSessionDbcLocale();

        handler->PSendSysMessage("|cff00ff00===== %s (ID %u) =====|r",
            se->Name[loc] ? se->Name[loc] : "未知", sid);

        // 组成部件
        handler->PSendSysMessage("|cff00ff00--- 组成 ---|r");
        uint32 pieces = 0;
        for (uint32 k = 0; k < MAX_ITEM_SET_ITEMS; ++k)
        {
            if (!se->ItemID[k])
                continue;
            ++pieces;
            ItemTemplate const* p = sObjectMgr->GetItemTemplate(se->ItemID[k]);
            if (p)
                handler->PSendSysMessage("  %s |cff888888(%u)|r",
                    ItemForge::Link(p).c_str(), se->ItemID[k]);
            else
                handler->PSendSysMessage("  |cffff8000物品 %u（数据库里没有）|r", se->ItemID[k]);
        }
        handler->PSendSysMessage("  |cff888888共 %u 件|r", pieces);

        // 套装效果
        handler->PSendSysMessage("|cff00ff00--- 套装效果 ---|r");
        bool any = false;
        for (uint32 k = 0; k < MAX_ITEM_SET_SPELLS; ++k)
        {
            if (!se->SetSpellID[k])
                continue;
            any = true;
            SpellInfo const* si = sSpellMgr->GetSpellInfo(se->SetSpellID[k]);
            handler->PSendSysMessage("  |cff00ccff%u件|r -> 法术 |cffffff00%u|r  %s",
                se->SetThreshold[k], se->SetSpellID[k],
                (si && si->SpellName[lc]) ? si->SpellName[lc] : "");
        }
        if (!any)
            handler->PSendSysMessage("  |cffff8000这个套装没有效果（借壳没意义）|r");

        if (se->RequiredSkill)
            handler->PSendSysMessage("|cff888888需要技能 %u 等级 %u|r",
                se->RequiredSkill, se->RequiredSkillRank);

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff888888借壳用法：|r|cffffff00.item set bind <你的装备entry> %u|r", sid);
        return true;
    }

    // ==================================================================
    //  克隆整套
    //  .item set clone <setId> [装等300] [x10] [-y]
    // ==================================================================
    static bool HandleSetClone(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 3 || !IsNumber(tok[2]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item set clone <setId> [装等300] [x10] [-y]|r");
            handler->PSendSysMessage("把整套装备一次性克隆，并自动挂到同一个套装上。");
            handler->PSendSysMessage("例：|cffffff00.item set clone 831 装等300 -y|r");
            return true;
        }

        uint32 sid = uint32(atoi(tok[2].c_str()));
        ItemSetEntry const* se = sItemSetStore.LookupEntry(sid);
        if (!se)
        {
            handler->PSendSysMessage("|cffff0000套装 %u 不存在。|r", sid);
            return true;
        }

        // 解析选项
        uint32 targetIlvl = 0;
        double multiply   = 0.0;
        bool   confirm    = false;

        for (size_t i = 3; i < tok.size(); ++i)
        {
            std::string const& t = tok[i];
            if (t == "-y" || t == "确认") { confirm = true; continue; }
            if (uint32 il = ParsePrefixedNumber(t, { "装等", "ilvl" })) { targetIlvl = il; continue; }
            if (t.size() > 1 && (t[0] == 'x' || t[0] == 'X') && IsNumber(t.substr(1)))
            { multiply = double(atoi(t.substr(1).c_str())); continue; }
        }

        // 收集有效部件
        std::vector<uint32> srcItems;
        for (uint32 k = 0; k < MAX_ITEM_SET_ITEMS; ++k)
        {
            if (!se->ItemID[k])
                continue;
            if (sObjectMgr->GetItemTemplate(se->ItemID[k]))
                srcItems.push_back(se->ItemID[k]);
        }

        if (srcItems.empty())
        {
            handler->PSendSysMessage("|cffff0000这个套装没有可用部件。|r");
            return true;
        }

        // 预留连续 entry
        uint32 baseEntry = FindFreeRange(uint32(srcItems.size()));
        if (!baseEntry)
        {
            handler->PSendSysMessage("|cffff0000找不到 %u 个连续空位。|r", uint32(srcItems.size()));
            return true;
        }

        uint32 loc = uint32(handler->GetSessionDbcLocale());
        std::string setName = se->Name[loc] ? se->Name[loc] : "套装";

        handler->PSendSysMessage("|cff00ff00===== 克隆套装预览 =====|r");
        handler->PSendSysMessage("源：|cffa335ee%s|r (ID %u)  %u 件",
            setName.c_str(), sid, uint32(srcItems.size()));
        handler->PSendSysMessage("新 entry：|cffffff00%u ~ %u|r",
            baseEntry, baseEntry + uint32(srcItems.size()) - 1);

        if (targetIlvl)
            handler->PSendSysMessage("装等 -> %u", targetIlvl);
        if (multiply > 0.0)
            handler->PSendSysMessage("属性 x%d", int32(multiply));

        handler->PSendSysMessage(" ");
        for (size_t i = 0; i < srcItems.size(); ++i)
        {
            ItemTemplate const* p = sObjectMgr->GetItemTemplate(srcItems[i]);
            handler->PSendSysMessage("  %u  %s",
                baseEntry + uint32(i), ItemForge::Link(p).c_str());
        }

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00套装效果会自动继承（itemset = %u）|r", sid);

        if (!confirm)
        {
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cffff8000以上仅为预览。确认请加 |r|cffffff00-y|r");
            return true;
        }

        // ---- 逐件克隆 ----
        uint32 done = 0;
        for (size_t i = 0; i < srcItems.size(); ++i)
        {
            uint32 newEntry = baseEntry + uint32(i);
            if (CloneOne(handler, srcItems[i], newEntry, targetIlvl, multiply, sid))
                ++done;
        }

        sObjectMgr->LoadItemTemplates();
        for (size_t i = 0; i < srcItems.size(); ++i)
        {
            if (ItemTemplate const* p = sObjectMgr->GetItemTemplate(baseEntry + uint32(i)))
                const_cast<ItemTemplate*>(p)->InitializeQueryData();
        }

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00[完成]|r 克隆 %u/%u 件，entry %u ~ %u",
            done, uint32(srcItems.size()), baseEntry, baseEntry + uint32(srcItems.size()) - 1);
        handler->PSendSysMessage("|cff888888一键全拿：|r");
        for (size_t i = 0; i < srcItems.size(); ++i)
            handler->PSendSysMessage("|cffffff00.additem %u|r", baseEntry + uint32(i));

        HintClientCache(handler);
        return true;
    }

    // 找连续 n 个空 entry
    static uint32 FindFreeRange(uint32 n)
    {
        ItemTemplateContainer const& store = sObjectMgr->GetItemTemplateStore();
        for (uint32 base = ItemForge::ENTRY_BASE; base + n <= ItemForge::ENTRY_MAX; ++base)
        {
            bool ok = true;
            for (uint32 k = 0; k < n; ++k)
            {
                if (store.find(base + k) != store.end()) { ok = false; break; }
            }
            if (ok)
                return base;
        }
        return 0;
    }

    // 克隆单件（供套装克隆复用）
    static bool CloneOne(ChatHandler* handler, uint32 srcEntry, uint32 newEntry,
                         uint32 targetIlvl, double multiply, uint32 forceSetId)
    {
        ItemTemplate const* src = sObjectMgr->GetItemTemplate(srcEntry);
        if (!src)
            return false;

        double scale = 1.0;
        if (targetIlvl)
            scale = ItemForge::IlvlScale(src->ItemLevel, targetIlvl);
        if (multiply > 0.0)
            scale *= multiply;

        // 取列名
        std::string colList;
        {
            QueryResult colRes = WorldDatabase.Query(
                "SELECT COLUMN_NAME FROM information_schema.COLUMNS "
                "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'item_template' "
                "ORDER BY ORDINAL_POSITION");
            if (!colRes)
                return false;

            bool first = true;
            do
            {
                std::string col = (*colRes)[0].GetString();
                if (first) { colList = std::to_string(newEntry); first = false; }
                else { colList += ", `"; colList += col; colList += "`"; }
            }
            while (colRes->NextRow());
        }

        WorldDatabase.DirectPExecute("DELETE FROM item_template WHERE entry = {}", newEntry);
        WorldDatabase.DirectPExecute(
            "INSERT INTO item_template SELECT {} FROM item_template WHERE entry = {}",
            colList, srcEntry);

        // 改名 + 套装绑定
        std::string newName = src->Name1 + " (改)";
        WorldDatabase.DirectPExecute(
            "UPDATE item_template SET name = '{}', itemset = {}{} WHERE entry = {}",
            ItemForge::Esc(newName), forceSetId,
            targetIlvl ? (", ItemLevel = " + std::to_string(targetIlvl)) : "",
            newEntry);

        if (scale != 1.0)
        {
            for (uint32 i = 1; i <= MAX_ITEM_PROTO_STATS; ++i)
                WorldDatabase.DirectPExecute(
                    "UPDATE item_template SET stat_value{} = LEAST(2100000000, GREATEST(-2100000000, "
                    "ROUND(stat_value{} * {}))) WHERE entry = {}", i, i, scale, newEntry);

            WorldDatabase.DirectPExecute(
                "UPDATE item_template SET dmg_min1 = dmg_min1 * {}, dmg_max1 = dmg_max1 * {}, "
                "armor = LEAST(2100000000, ROUND(armor * {})) WHERE entry = {}",
                scale, scale, scale, newEntry);

            // 耐久必须钳制：Item.cpp:365 存库用 setUInt16，上限 65535
            WorldDatabase.DirectPExecute(
                "UPDATE item_template SET MaxDurability = LEAST(60000, ROUND(MaxDurability * {})) "
                "WHERE entry = {} AND MaxDurability > 0", scale, newEntry);
        }

        (void)handler;
        return true;
    }

    // ==================================================================
    //  单件挂套装 / 解绑
    // ==================================================================
    static bool HandleSetBind(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 4 || !IsNumber(tok[2]) || !IsNumber(tok[3]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item set bind <装备entry> <setId>|r");
            handler->PSendSysMessage("把你的装备挂到某个现成套装上，直接继承它的套装效果。");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff888888原理：Item.cpp:85 只数「穿了几件带这个 setid 的装备」，|r");
            handler->PSendSysMessage("|cff888888不校验 ItemID 列表。所以填对 itemset 就能触发效果。|r");
            return true;
        }

        uint32 entry = uint32(atoi(tok[2].c_str()));
        uint32 sid   = uint32(atoi(tok[3].c_str()));

        if (!sObjectMgr->GetItemTemplate(entry))
        {
            handler->PSendSysMessage("|cffff0000entry %u 不存在。|r", entry);
            return true;
        }
        if (!InForgeRange(handler, entry))
            return true;

        ItemSetEntry const* se = sItemSetStore.LookupEntry(sid);
        if (!se)
        {
            handler->PSendSysMessage("|cffff0000套装 %u 不存在。|r", sid);
            handler->PSendSysMessage("用 |cffffff00.item set list|r 浏览可用套装。");
            return true;
        }

        WorldDatabase.DirectPExecute("UPDATE item_template SET itemset = {} WHERE entry = {}",
            sid, entry);
        if (!ReloadOne(handler, entry))
            return true;

        uint32 loc = uint32(handler->GetSessionDbcLocale());
        ItemTemplate const* np = sObjectMgr->GetItemTemplate(entry);

        handler->PSendSysMessage("|cff00ff00[已绑定]|r %s -> |cffa335ee%s|r",
            ItemForge::Link(np).c_str(), se->Name[loc] ? se->Name[loc] : "?");

        // 显示效果门槛
        LocaleConstant lc = handler->GetSessionDbcLocale();
        for (uint32 k = 0; k < MAX_ITEM_SET_SPELLS; ++k)
        {
            if (!se->SetSpellID[k])
                continue;
            SpellInfo const* si = sSpellMgr->GetSpellInfo(se->SetSpellID[k]);
            handler->PSendSysMessage("  |cff00ccff%u件|r -> %s",
                se->SetThreshold[k],
                (si && si->SpellName[lc]) ? si->SpellName[lc] : "?");
        }

        HintReequip(handler, entry);
        return true;
    }

    static bool HandleSetUnbind(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 3 || !IsNumber(tok[2]))
        {
            handler->PSendSysMessage("用法：|cffffff00.item set unbind <装备entry>|r");
            return true;
        }

        uint32 entry = uint32(atoi(tok[2].c_str()));
        if (!sObjectMgr->GetItemTemplate(entry))
        {
            handler->PSendSysMessage("|cffff0000entry %u 不存在。|r", entry);
            return true;
        }
        if (!InForgeRange(handler, entry))
            return true;

        WorldDatabase.DirectPExecute("UPDATE item_template SET itemset = 0 WHERE entry = {}", entry);
        if (!ReloadOne(handler, entry))
            return true;

        handler->PSendSysMessage("|cff00ff00[已解绑]|r entry %u 不再属于任何套装。", entry);
        HintReequip(handler, entry);
        return true;
    }

    // ==================================================================
    //  生成新 ItemSet 的 DBC 清单
    //  .item set new <名字> <法术ID:件数> [法术ID:件数]...
    // ==================================================================
    static bool HandleSetNew(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        if (tok.size() < 4)
        {
            handler->PSendSysMessage("用法：|cffffff00.item set new <名字> <法术ID:件数> [更多...]|r");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("例：|cffffff00.item set new 真龙套 71903:2 48819:4 54428:6|r");
            handler->PSendSysMessage("    2件触发71903，4件触发48819，6件触发54428");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cffff8000这条只生成 DBC 清单，需要打客户端补丁才生效。|r");
            handler->PSendSysMessage("|cff888888生成后用 tools/export_itemset_dbc.py 打包。|r");
            return true;
        }

        std::string name = tok[2];

        struct Eff { uint32 spell; uint32 threshold; };
        std::vector<Eff> effs;

        for (size_t i = 3; i < tok.size(); ++i)
        {
            std::string const& t = tok[i];
            size_t colon = t.find(':');
            if (colon == std::string::npos)
            {
                handler->PSendSysMessage("|cffff0000格式错误：%s|r（应为 法术ID:件数）", t.c_str());
                return true;
            }
            std::string sp = t.substr(0, colon);
            std::string th = t.substr(colon + 1);
            if (!IsNumber(sp) || !IsNumber(th))
            {
                handler->PSendSysMessage("|cffff0000格式错误：%s|r", t.c_str());
                return true;
            }

            uint32 spellId = uint32(atoi(sp.c_str()));
            if (!sSpellMgr->GetSpellInfo(spellId))
            {
                handler->PSendSysMessage("|cffff0000法术 %u 不存在。|r", spellId);
                return true;
            }

            Eff e;
            e.spell = spellId;
            e.threshold = uint32(atoi(th.c_str()));
            effs.push_back(e);

            if (effs.size() >= MAX_ITEM_SET_SPELLS)
                break;
        }

        // 找一个空的 setId
        uint32 newSetId = 0;
        for (uint32 i = 2000; i < 5000; ++i)
        {
            if (!sItemSetStore.LookupEntry(i)) { newSetId = i; break; }
        }
        if (!newSetId)
        {
            handler->PSendSysMessage("|cffff0000找不到空的 setId。|r");
            return true;
        }

        // 写进自定义表，供工具导出
        WorldDatabase.DirectPExecute(
            "REPLACE INTO custom_itemset (setId, name) VALUES ({}, '{}')",
            newSetId, ItemForge::Esc(name));

        WorldDatabase.DirectPExecute("DELETE FROM custom_itemset_spell WHERE setId = {}", newSetId);
        for (size_t i = 0; i < effs.size(); ++i)
        {
            WorldDatabase.DirectPExecute(
                "INSERT INTO custom_itemset_spell (setId, idx, spellId, threshold) "
                "VALUES ({}, {}, {}, {})",
                newSetId, uint32(i), effs[i].spell, effs[i].threshold);
        }

        LocaleConstant lc = handler->GetSessionDbcLocale();

        handler->PSendSysMessage("|cff00ff00===== 新套装已登记 =====|r");
        handler->PSendSysMessage("名字：|cffa335ee%s|r", name.c_str());
        handler->PSendSysMessage("setId：|cffffff00%u|r", newSetId);
        handler->PSendSysMessage("|cff00ff00--- 效果 ---|r");
        for (Eff const& e : effs)
        {
            SpellInfo const* si = sSpellMgr->GetSpellInfo(e.spell);
            handler->PSendSysMessage("  |cff00ccff%u件|r -> %u  %s",
                e.threshold, e.spell, (si && si->SpellName[lc]) ? si->SpellName[lc] : "");
        }
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cffff8000还要两步才能生效：|r");
        handler->PSendSysMessage("|cff8888881. 用装备挂上：|r|cffffff00.item set bind <entry> %u|r", newSetId);
        handler->PSendSysMessage("|cff8888882. 打 DBC 补丁：|r|cffffff00python3 tools/export_itemset_dbc.py|r");
        return true;
    }
};

void AddSC_itemforge_commandscript()
{
    new itemforge_commandscript();
}
