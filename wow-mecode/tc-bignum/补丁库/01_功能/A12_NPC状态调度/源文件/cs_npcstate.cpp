/*
 * ============================================================================
 *  NPC 状态调度器 —— cs_npcstate.cpp   (step26)
 * ============================================================================
 *
 *   .nst                          查看当前目标状态 / 用法
 *   .nst <档位>                   改选中的 NPC（临时）
 *   .nst <档位> save              改选中的 NPC 并写库（永久）
 *   .nst <档位> r <半径>          改周围所有 NPC（临时）
 *   .nst <档位> r <半径> save     改周围所有 NPC 并写库
 *   .nst <档位> entry <ID>        改全世界该 entry 的 NPC（自动写库）
 *   .nst list                     列出所有档位说明
 *
 *  六个档位：
 *    invuln   无敌   谁都打不了（像原版小孩）
 *    story    剧情   打不了 + 不可选中 + 不参与战斗（纯背景板）
 *    pconly   仅怪   玩家打不了，怪能打（护送任务的被护送者）
 *    normal   普通   可攻击、会还手（默认怪）
 *    neutral  中立   可攻击、不主动打你（黄名）
 *    friend   友好   绿名，要按 Ctrl 才能打
 *
 * ----------------------------------------------------------------------------
 *  为什么做这个（用户原话）
 *
 *    「怎么让创造出来的npc和原版改版的npc完全不会被攻击？
 *      做一个可以完全改变npc类型的指令，可以让他们变成像原版里的孩子一样无敌，
 *      也可以改成像普通npc一样可以攻击，或者是改成中立，
 *      可以对所有生物生效，包括玩家和boss，有了这个功能，
 *      以后做剧情的角色就能更方便了」
 *
 * ----------------------------------------------------------------------------
 *  攻击判定链（WorldObject::IsValidAttackTarget，Object.cpp:2991）
 *  —— 按顺序，越靠前优先级越高：
 *
 *    :3004  HasUnitState(UNIT_STATE_UNATTACKABLE)          <- 【不能用！见下】
 *    :3008  target->ToPlayer()->IsGameMaster()
 *    :3033  UNIT_FLAG_UNINTERACTIBLE                       <- 不可选中
 *    :3043  UNIT_FLAG_NON_ATTACKABLE | ON_TAXI |
 *           NOT_ATTACKABLE_1 | NON_ATTACKABLE_2            <- 不可攻击
 *    :3071  IsImmuneToNPC() / IsImmuneToPC()               <- 分别屏蔽怪/玩家
 *
 *  【重要】UNIT_STATE_UNATTACKABLE 不能用：
 *      Unit.h:259  UNIT_STATE_UNATTACKABLE = UNIT_STATE_IN_FLIGHT
 *    它是「飞行中」的别名！设了会让 NPC 被当成在坐飞行点，
 *    SpellEffects.cpp 里十几处 IsInFlight() 判断会全部误判。
 *    所以本实现【只用 flag，不碰 UnitState】。
 *
 *  为什么优先用 SetImmuneToPC/NPC 而不是裸 SetUnitFlag：
 *      Unit.cpp:8663  SetImmuneToPC(bool apply, bool keepCombat)
 *    它除了设 flag，还会：
 *      · ValidateAttackersAndOwnTarget()  把正在打它的人踢掉
 *      · 遍历 CombatRefs 结束战斗
 *      · 【NPCBot 分支已处理】(pair.second->GetOther(this)->IsNPCBotOrPet())
 *    裸设 flag 不会断当前战斗，改了也还在挨打。
 *
 * ----------------------------------------------------------------------------
 *  已核实 API（全 public）
 *
 *   Chat.h:104          Creature* getSelectedCreature();
 *   Creature.h:98       ObjectGuid::LowType GetSpawnId() const
 *   Creature.h:134      void SetReactState(ReactStates st)
 *   Unit.h:955/956      SetUnitFlag / RemoveUnitFlag
 *   Unit.h:975          void SetFaction(uint32 faction)
 *   Unit.h:1096         NPCFlags GetNpcFlags() const
 *   Unit.h:1135-1141    IsImmuneToAll / IsImmuneToPC / IsImmuneToNPC
 *   Unit.h:1139/1142    SetImmuneToPC / SetImmuneToNPC (bool apply, bool keepCombat)
 *   Unit.h:869          CombatStop(bool includingCast, bool mutualPvP)
 *   UnitDefines.h:136   UNIT_FLAG_NON_ATTACKABLE   = 0x00000002
 *   UnitDefines.h:143   UNIT_FLAG_IMMUNE_TO_PC     = 0x00000100
 *   UnitDefines.h:144   UNIT_FLAG_IMMUNE_TO_NPC    = 0x00000200
 *   UnitDefines.h:151   UNIT_FLAG_NON_ATTACKABLE_2 = 0x00010000
 *   UnitDefines.h:160   UNIT_FLAG_UNINTERACTIBLE   = 0x02000000
 *   UnitDefines.h:408   REACT_PASSIVE / REACT_DEFENSIVE / REACT_AGGRESSIVE
 *   SharedDefines.h:242 FACTION_MONSTER  = 14
 *   SharedDefines.h:245 FACTION_PREY     = 31
 *   SharedDefines.h:247 FACTION_FRIENDLY = 35
 *
 *  持久化（参考 cs_npc.cpp:404 HandleNpcSetFactionCommand 的写法）
 *   ObjectMgr.cpp:2227  data.unit_flags = fields[20].GetUInt32();
 *                       -> creature 表【有】unit_flags 列，单只可持久化
 *   wdb creature:342    `unit_flags` int unsigned
 *   wdb creature_template:652 `unit_flags` int unsigned
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DatabaseEnv.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "SharedDefines.h"
#include "Unit.h"
#include "UnitDefines.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

// ============================================================================
//  档位定义
// ============================================================================

enum NpcStateId
{
    NST_INVULN = 0,     // 无敌：谁都打不了
    NST_STORY,          // 剧情：打不了+不可选中+不参战
    NST_PCONLY,         // 仅怪：玩家打不了，怪能打
    NST_NORMAL,         // 普通：可攻击会还手
    NST_NEUTRAL,        // 中立：可攻击不主动
    NST_FRIEND,         // 友好：绿名
    NST_MAX
};

struct StateDef
{
    char const* key;        // 英文名
    char const* alias;      // 中文别名
    char const* cn;         // 显示名
    char const* desc;       // 说明
};

StateDef const g_states[NST_MAX] =
{
    { "invuln",  "无敌", "无敌", "谁都打不了，像原版小孩" },
    { "story",   "剧情", "剧情", "打不了+不可选中+不参战，纯背景板" },
    { "pconly",  "仅怪", "仅怪", "玩家打不了，怪能打（护送目标）" },
    { "normal",  "普通", "普通", "可攻击、会还手（默认怪）" },
    { "neutral", "中立", "中立", "可攻击、不主动打你（黄名）" },
    { "friend",  "友好", "友好", "绿名，要按 Ctrl 才能打" },
};

// 六个档位分别要设成什么。
// 注意 faction 用 0 表示「不动，保持原样」——
// 剧情 NPC 常常需要保留原阵营（比如联盟守卫仍是联盟），
// 只有明确要改敌友关系的档位才写具体值。
struct StateConfig
{
    bool     immuneToPC;      // 玩家打不了
    bool     immuneToNPC;     // 怪打不了
    bool     uninteractible;  // 不可选中
    bool     nonAttackable;   // 不可攻击（客户端不显示攻击图标）
    uint32   faction;         // 0 = 不改
    ReactStates react;
};

StateConfig const g_cfg[NST_MAX] =
{
    /* invuln  */ { true,  true,  false, true,  0,                REACT_PASSIVE    },
    /* story   */ { true,  true,  true,  true,  0,                REACT_PASSIVE    },
    /* pconly  */ { true,  false, false, false, 0,                REACT_DEFENSIVE  },
    /* normal  */ { false, false, false, false, FACTION_MONSTER,  REACT_AGGRESSIVE },
    /* neutral */ { false, false, false, false, FACTION_PREY,     REACT_DEFENSIVE  },
    /* friend  */ { false, false, false, false, FACTION_FRIENDLY, REACT_PASSIVE    },
};

// ============================================================================
//  工具
// ============================================================================

std::vector<std::string> Tok(char const* args)
{
    std::vector<std::string> t;
    if (!args)
        return t;
    std::string a = args;
    size_t pos = 0;
    while (pos < a.size())
    {
        size_t sp = a.find(' ', pos);
        if (sp == std::string::npos)
            sp = a.size();
        if (sp > pos)
            t.push_back(a.substr(pos, sp - pos));
        pos = sp + 1;
    }
    return t;
}

int ParseState(std::string const& s)
{
    for (int i = 0; i < NST_MAX; ++i)
        if (s == g_states[i].key || s == g_states[i].alias)
            return i;
    return -1;
}

/*
 * 把一个档位应用到一只 Creature。
 *
 * 顺序有讲究：
 *   1. 先 SetImmuneToXX（它内部会断战斗、踢掉正在打它的人）
 *   2. 再设/清 flag
 *   3. 最后 SetReactState + faction
 * 反过来的话，刚设完 flag 又被战斗逻辑改回去。
 */
void ApplyState(Creature* c, int st)
{
    StateConfig const& cfg = g_cfg[st];

    // --- 1. 免疫（Unit.h:1139/1142，第二参数 keepCombat=false 表示要断战斗）---
    c->SetImmuneToPC(cfg.immuneToPC, false);
    c->SetImmuneToNPC(cfg.immuneToNPC, false);

    // --- 2. 可选中 / 可攻击 flag ---
    if (cfg.uninteractible)
        c->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);       // UnitDefines.h:160
    else
        c->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);

    if (cfg.nonAttackable)
        c->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE_2);     // UnitDefines.h:151
    else
        c->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE_2);

    /*
     * UNIT_FLAG_NON_ATTACKABLE(0x02) 故意【不用】：
     * UnitDefines.h:136 注释说明它是 SPAWNING 用的，
     * 法术命中时会被自动清掉，拿它做长期状态不可靠。
     * 用 NON_ATTACKABLE_2(0x10000) 才是「移除攻击图标」的正确位。
     */

    // --- 3. 行为 ---
    c->SetReactState(cfg.react);                        // Creature.h:134

    if (cfg.faction)                                    // 0 = 保持原阵营
        c->SetFaction(cfg.faction);                     // Unit.h:975

    // 无敌/剧情档顺手把当前战斗清干净
    if (cfg.immuneToPC && cfg.immuneToNPC)
        c->CombatStop(true);                            // Unit.h:869
}

// 算出应用该档位后，unit_flags 应该是什么（用于写库）
uint32 CalcUnitFlags(Creature* c, int st)
{
    StateConfig const& cfg = g_cfg[st];
    uint32 f = c->GetUInt32Value(UNIT_FIELD_FLAGS);

    auto setbit = [&f](uint32 bit, bool on)
    {
        if (on)  f |= bit;
        else     f &= ~bit;
    };

    setbit(UNIT_FLAG_IMMUNE_TO_PC,     cfg.immuneToPC);
    setbit(UNIT_FLAG_IMMUNE_TO_NPC,    cfg.immuneToNPC);
    setbit(UNIT_FLAG_UNINTERACTIBLE,   cfg.uninteractible);
    setbit(UNIT_FLAG_NON_ATTACKABLE_2, cfg.nonAttackable);

    return f;
}

// 写库：单只（creature 表，按 guid）
void SaveOne(Creature* c, int st)
{
    ObjectGuid::LowType spawnId = c->GetSpawnId();      // Creature.h:98
    if (!spawnId)
        return;                                          // 临时召唤物没有 spawnId，存不了

    uint32 flags = CalcUnitFlags(c, st);

    WorldDatabase.DirectPExecute(
        "UPDATE `creature` SET `unit_flags` = %u WHERE `guid` = %u",
        flags, spawnId);

    if (g_cfg[st].faction)
    {
        // faction 只在 creature_template 里，没有 per-guid 覆盖
        WorldDatabase.DirectPExecute(
            "UPDATE `creature_template` SET `faction` = %u WHERE `entry` = %u",
            g_cfg[st].faction, c->GetEntry());
    }
}

// 写库：按 entry（creature_template）
void SaveEntry(uint32 entry, int st, uint32 sampleFlags)
{
    WorldDatabase.DirectPExecute(
        "UPDATE `creature_template` SET `unit_flags` = %u WHERE `entry` = %u",
        sampleFlags, entry);

    if (g_cfg[st].faction)
        WorldDatabase.DirectPExecute(
            "UPDATE `creature_template` SET `faction` = %u WHERE `entry` = %u",
            g_cfg[st].faction, entry);
}

// 收集半径内的 Creature
void CollectNear(Player* player, float radius, std::vector<Creature*>& out)
{
    std::list<Creature*> found;
    Trinity::AnyUnitInObjectRangeCheck check(player, radius);
    Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck>
        searcher(player, found, check);
    Cell::VisitAllObjects(player, searcher, radius);

    for (Creature* c : found)
    {
        if (!c || !c->IsInWorld())
            continue;
        // NPCBot 绝不误伤（Creature.h:394）
        if (c->IsNPCBotOrPet())
            continue;
        // 玩家自己的宠物也跳过
        if (c->IsPet() || c->IsTotem())
            continue;
        out.push_back(c);
    }
}

// 读当前状态，返回档位（认不出来返回 -1）
int DetectState(Creature* c)
{
    bool ipc  = c->IsImmuneToPC();                      // Unit.h:1138
    bool inpc = c->IsImmuneToNPC();                     // Unit.h:1141
    bool unin = c->HasUnitFlag(UNIT_FLAG_UNINTERACTIBLE);

    if (ipc && inpc && unin)  return NST_STORY;
    if (ipc && inpc)          return NST_INVULN;
    if (ipc && !inpc)         return NST_PCONLY;

    uint32 fac = c->GetFaction();
    if (fac == FACTION_FRIENDLY) return NST_FRIEND;
    if (fac == FACTION_PREY)     return NST_NEUTRAL;
    return NST_NORMAL;
}

} // namespace

// ============================================================================
//  指令
// ============================================================================

class npcstate_commandscript : public CommandScript
{
public:
    npcstate_commandscript() : CommandScript("npcstate_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "nst", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleNst, "" },
        };
        return commandTable;
    }

    static bool HandleNst(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = Tok(args);

        // ---------- 无参数：看当前目标状态 ----------
        if (tok.empty())
            return ShowUsage(handler, player);

        if (tok[0] == "list" || tok[0] == "列表")
            return ShowList(handler);

        int st = ParseState(tok[0]);
        if (st < 0)
        {
            handler->PSendSysMessage("|cffff0000未知档位|r：%s", tok[0].c_str());
            handler->PSendSysMessage("用 |cffffff00.nst list|r 看全部档位");
            return true;
        }

        // ---------- 解析后续参数 ----------
        bool  save     = false;
        float radius   = 0.0f;
        uint32 byEntry = 0;

        for (size_t i = 1; i < tok.size(); ++i)
        {
            if (tok[i] == "save" || tok[i] == "保存")
                save = true;
            else if ((tok[i] == "r" || tok[i] == "半径") && i + 1 < tok.size())
                radius = float(atof(tok[++i].c_str()));
            else if ((tok[i] == "entry" || tok[i] == "编号") && i + 1 < tok.size())
                byEntry = uint32(atoi(tok[++i].c_str()));
        }

        // ---------- 模式三：按 entry 批量 ----------
        if (byEntry)
            return ApplyByEntry(handler, player, st, byEntry);

        // ---------- 模式二：半径批量 ----------
        if (radius > 0.0f)
            return ApplyByRadius(handler, player, st, radius, save);

        // ---------- 模式一：选中目标 ----------
        Creature* target = handler->getSelectedCreature();   // Chat.h:104
        if (!target)
        {
            handler->PSendSysMessage("|cffff0000请先选中一个 NPC|r");
            handler->PSendSysMessage("或用 |cffffff00.nst %s r <半径>|r 批量修改",
                g_states[st].key);
            return true;
        }

        if (target->IsNPCBotOrPet())
        {
            handler->PSendSysMessage("|cffff0000这是 NPCBot，不能改|r");
            return true;
        }

        ApplyState(target, st);
        if (save)
            SaveOne(target, st);

        handler->PSendSysMessage("|cff00ff00[%s]|r %s -> |cffffff00%s|r%s",
            save ? "已保存" : "已修改",
            target->GetName().c_str(),
            g_states[st].cn,
            save ? "" : "  |cff888888(临时，重刷失效)|r");
        handler->PSendSysMessage("|cff888888%s|r", g_states[st].desc);
        return true;
    }

private:
    // ------------------------------------------------------------------
    static bool ShowUsage(ChatHandler* handler, Player* player)
    {
        handler->PSendSysMessage("|cff00ff00===== NPC 状态调度 =====|r");

        if (Creature* c = handler->getSelectedCreature())
        {
            int cur = DetectState(c);
            handler->PSendSysMessage("当前目标：|cffffff00%s|r (entry %u)",
                c->GetName().c_str(), c->GetEntry());
            handler->PSendSysMessage("当前状态：|cff00ccff%s|r",
                cur >= 0 ? g_states[cur].cn : "未知");
            handler->PSendSysMessage("|cff888888阵营 %u  免疫玩家 %s  免疫怪 %s  可选中 %s|r",
                c->GetFaction(),
                c->IsImmuneToPC()  ? "是" : "否",
                c->IsImmuneToNPC() ? "是" : "否",
                c->HasUnitFlag(UNIT_FLAG_UNINTERACTIBLE) ? "否" : "是");
        }
        else
            handler->PSendSysMessage("|cff888888（未选中目标）|r");

        (void)player;
        handler->PSendSysMessage("|cffffff00.nst <档位>|r              改选中的");
        handler->PSendSysMessage("|cffffff00.nst <档位> save|r         改并写库（永久）");
        handler->PSendSysMessage("|cffffff00.nst <档位> r <半径>|r     改周围所有");
        handler->PSendSysMessage("|cffffff00.nst <档位> entry <ID>|r   改全世界该 entry");
        handler->PSendSysMessage("|cffffff00.nst list|r                看档位说明");
        return true;
    }

    // ------------------------------------------------------------------
    static bool ShowList(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ff00===== 档位说明 =====|r");
        for (int i = 0; i < NST_MAX; ++i)
            handler->PSendSysMessage("|cffffff00%-8s|r %s  |cff888888%s|r",
                g_states[i].key, g_states[i].cn, g_states[i].desc);
        handler->PSendSysMessage("|cff888888档位名可用中文：无敌/剧情/仅怪/普通/中立/友好|r");
        return true;
    }

    // ------------------------------------------------------------------
    static bool ApplyByRadius(ChatHandler* handler, Player* player,
                              int st, float radius, bool save)
    {
        if (radius > 500.0f)
        {
            handler->PSendSysMessage("|cffffff00半径已钳到 500 码|r");
            radius = 500.0f;
        }

        std::vector<Creature*> list;
        CollectNear(player, radius, list);

        if (list.empty())
        {
            handler->PSendSysMessage("|cffffff00半径 %.0f 码内没有可改的 NPC|r", radius);
            return true;
        }

        uint32 done = 0, saved = 0;
        for (Creature* c : list)
        {
            ApplyState(c, st);
            ++done;
            if (save && c->GetSpawnId())
            {
                SaveOne(c, st);
                ++saved;
            }
        }

        handler->PSendSysMessage("|cff00ff00[完成]|r %u 个 NPC -> |cffffff00%s|r",
            done, g_states[st].cn);
        if (save)
            handler->PSendSysMessage("|cff888888其中 %u 个已写库（%u 个是召唤物存不了）|r",
                saved, done - saved);
        return true;
    }

    // ------------------------------------------------------------------
    static bool ApplyByEntry(ChatHandler* handler, Player* player,
                             int st, uint32 entry)
    {
        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(entry);
        if (!ct)
        {
            handler->PSendSysMessage("|cffff0000entry %u 不存在|r", entry);
            return true;
        }

        // 先改世界里已存在的实例（同地图可见范围内）
        std::vector<Creature*> list;
        CollectNear(player, 500.0f, list);

        uint32 live = 0;
        uint32 sampleFlags = ct->unit_flags;

        for (Creature* c : list)
        {
            if (c->GetEntry() != entry)
                continue;
            ApplyState(c, st);
            sampleFlags = CalcUnitFlags(c, st);
            ++live;
        }

        // 没有在场实例，就按模板算一份 flags
        if (!live)
        {
            StateConfig const& cfg = g_cfg[st];
            uint32 f = ct->unit_flags;
            auto setbit = [&f](uint32 bit, bool on) { if (on) f |= bit; else f &= ~bit; };
            setbit(UNIT_FLAG_IMMUNE_TO_PC,     cfg.immuneToPC);
            setbit(UNIT_FLAG_IMMUNE_TO_NPC,    cfg.immuneToNPC);
            setbit(UNIT_FLAG_UNINTERACTIBLE,   cfg.uninteractible);
            setbit(UNIT_FLAG_NON_ATTACKABLE_2, cfg.nonAttackable);
            sampleFlags = f;
        }

        // 同步内存里的模板（参考 cs_npc.cpp:401 的做法）
        const_cast<CreatureTemplate*>(ct)->unit_flags = sampleFlags;
        if (g_cfg[st].faction)
            const_cast<CreatureTemplate*>(ct)->faction = g_cfg[st].faction;

        SaveEntry(entry, st, sampleFlags);

        handler->PSendSysMessage("|cff00ff00[已保存]|r entry |cffffff00%u|r (%s) -> |cffffff00%s|r",
            entry, ct->Name.c_str(), g_states[st].cn);
        handler->PSendSysMessage("|cff888888附近 %u 只已即时生效；其余重刷后生效|r", live);
        return true;
    }
};

void AddSC_npcstate_commandscript()
{
    new npcstate_commandscript();
}
