/*
 * ============================================================================
 *  场景快照 —— cs_scene.cpp   (step28)
 * ============================================================================
 *
 *   .scene save <名字> [半径]     把周围 NPC 的完整状态存成快照
 *   .scene load <名字>            还原快照
 *   .scene list                   列出所有快照
 *   .scene info <名字>            看快照里有什么
 *   .scene del <名字>             删除快照
 *   .scene rebuild <名字>         还原，且【重建】已被删掉的 NPC
 *
 * ----------------------------------------------------------------------------
 *  为什么先做这个（用户选的）
 *
 *  做剧情要反复调 NPC 位置、朝向、状态。没有快照的话，
 *  每次改坏了就得从头摆一遍。有了它，摆好先存一份，
 *  随便怎么试，一条 .scene load 就回到原样。
 *
 *  它是其他剧情工具（.emote / .say / .follow / .freeze）的【地基】——
 *  有了后悔药，后面所有调试都不怕改坏。
 *
 * ----------------------------------------------------------------------------
 *  存了什么（用户要求"全存"）
 *
 *    位置 x/y/z + 朝向        摆位
 *    unit_flags               .nst 的六档状态（免疫位/可选中位）
 *    faction                  阵营，和 unit_flags 一起决定能不能打
 *    react                    REACT_PASSIVE/DEFENSIVE/AGGRESSIVE
 *    display + scale          模型和缩放
 *    stand + emote            站姿和情绪动作
 *    npcflag                  商人/任务给予者等功能位
 *    entry + guid             身份，guid=0 表示临时召唤物
 *
 * ----------------------------------------------------------------------------
 *  已核实 API（全 public）
 *
 *   Position.h:68        void Relocate(float x, float y, float z, float o)
 *   Unit.h:1200/1201     void NearTeleportTo(Position const&, bool casting)
 *   Unit.h:968           void SetEmoteState(Emote emote)
 *   Unit.h:1002          UnitStandStateType GetStandState() const
 *   Unit.h:1005          void SetStandState(UnitStandStateType state)
 *   Unit.h:1594/1596     GetDisplayId / GetNativeDisplayId
 *   Unit.h:1595          virtual void SetDisplayId(uint32 modelId)
 *   Unit.h:1598          void SetNativeDisplayId(uint32 displayId)
 *   Unit.h:974/975       GetFaction / SetFaction
 *   Unit.h:1096          NPCFlags GetNpcFlags() const
 *   Unit.h:1100          void ReplaceAllNpcFlags(NPCFlags flags)
 *   Object.h:93/94       GetObjectScale / SetObjectScale
 *   Creature.h:98        ObjectGuid::LowType GetSpawnId() const
 *   Creature.h:134/135   SetReactState / GetReactState
 *   Creature.h:394       bool IsNPCBotOrPet() const
 *   WorldObject.h:476    SummonCreature(...)
 *   UpdateFields.h:117   UNIT_FIELD_FLAGS
 *   UpdateFields.h:140   UNIT_NPC_EMOTESTATE
 *
 *  数据库：sql/41_scene_tables.sql
 *   world.custom_scene       快照主表
 *   world.custom_scene_npc   快照内容
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "SharedDefines.h"
#include "TemporarySummon.h"
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

constexpr float  SCENE_DEFAULT_RADIUS = 50.0f;
constexpr float  SCENE_MAX_RADIUS     = 500.0f;
constexpr uint32 SCENE_MAX_NPC        = 2000;   // 单个快照上限，防手滑存整张地图

// 快照里一个 NPC 的完整状态
struct SceneNpc
{
    uint32 guid    = 0;      // creature.guid，0 = 临时召唤物
    uint32 entry   = 0;
    float  x = 0.f, y = 0.f, z = 0.f, o = 0.f;
    uint32 unitFlags = 0;
    uint32 faction   = 0;
    uint8  react     = 1;
    uint32 display   = 0;
    float  scale     = 1.f;
    uint8  stand     = 0;
    uint32 emote     = 0;
    uint32 npcflag   = 0;
    std::string name;
};

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

// SQL 字符串转义（名字可能含引号）
std::string Esc(std::string const& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s)
    {
        if (c == '\'' || c == '\\')
            out += '\\';
        out += c;
    }
    return out;
}

// 收集半径内可用于快照的 Creature
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
        if (c->IsNPCBotOrPet())     // Creature.h:394  NPCBot 不进快照
            continue;
        if (c->IsPet() || c->IsTotem())
            continue;
        out.push_back(c);
    }
}

// 从活着的 Creature 读出完整状态
SceneNpc Capture(Creature* c)
{
    SceneNpc n;
    n.guid      = uint32(c->GetSpawnId());              // Creature.h:98
    n.entry     = c->GetEntry();
    n.x         = c->GetPositionX();
    n.y         = c->GetPositionY();
    n.z         = c->GetPositionZ();
    n.o         = c->GetOrientation();
    n.unitFlags = c->GetUInt32Value(UNIT_FIELD_FLAGS);  // UpdateFields.h:117
    n.faction   = c->GetFaction();                      // Unit.h:974
    n.react     = uint8(c->GetReactState());            // Creature.h:135
    n.display   = c->GetDisplayId();                    // Unit.h:1594
    n.scale     = c->GetObjectScale();                  // Object.h:93
    n.stand     = uint8(c->GetStandState());            // Unit.h:1002
    n.emote     = c->GetUInt32Value(UNIT_NPC_EMOTESTATE); // UpdateFields.h:140
    n.npcflag   = uint32(c->GetNpcFlags());             // Unit.h:1096
    n.name      = c->GetName();
    return n;
}

// 把状态套回一只 Creature
void Restore(Creature* c, SceneNpc const& n)
{
    // --- 位置：用 NearTeleportTo，会正确同步给客户端 ---
    if (c->GetPositionX() != n.x || c->GetPositionY() != n.y ||
        c->GetPositionZ() != n.z || c->GetOrientation() != n.o)
    {
        c->NearTeleportTo(n.x, n.y, n.z, n.o);          // Unit.h:1201
    }

    // --- 状态位（.nst 的六档就在这里）---
    c->SetUInt32Value(UNIT_FIELD_FLAGS, n.unitFlags);
    c->SetFaction(n.faction);                           // Unit.h:975
    c->SetReactState(ReactStates(n.react));             // Creature.h:134

    // --- 外观 ---
    if (n.display && c->GetDisplayId() != n.display)
    {
        c->SetDisplayId(n.display);                     // Unit.h:1595
        c->SetNativeDisplayId(n.display);               // Unit.h:1598  两个都设防回退
    }
    if (n.scale > 0.f)
        c->SetObjectScale(n.scale);                     // Object.h:94

    // --- 姿态 ---
    c->SetStandState(UnitStandStateType(n.stand));      // Unit.h:1005
    c->SetEmoteState(Emote(n.emote));                   // Unit.h:968

    // --- 功能位 ---
    c->ReplaceAllNpcFlags(NPCFlags(n.npcflag));         // Unit.h:1100
}

// 按名字查快照 id，找不到返回 0
uint32 FindSceneId(std::string const& name, std::string* outName = nullptr)
{
    QueryResult r = WorldDatabase.PQuery(
        "SELECT `id`,`name` FROM `custom_scene` WHERE `name` = '{}'",
        Esc(name));
    if (!r)
        return 0;
    Field* f = r->Fetch();
    if (outName)
        *outName = f[1].GetString();
    return f[0].GetUInt32();
}

} // namespace

// ============================================================================

class scene_commandscript : public CommandScript
{
public:
    scene_commandscript() : CommandScript("scene_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "scene", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleScene, "" },
        };
        return commandTable;
    }

    static bool HandleScene(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = Tok(args);
        if (tok.empty())
            return ShowUsage(handler);

        std::string const& sub = tok[0];

        if (sub == "list" || sub == "列表")
            return DoList(handler);

        if (sub == "save" || sub == "保存")
        {
            if (tok.size() < 2)
            {
                handler->PSendSysMessage("|cffff0000用法|r：.scene save <名字> [半径]");
                return true;
            }
            float radius = (tok.size() > 2) ? float(atof(tok[2].c_str())) : SCENE_DEFAULT_RADIUS;
            return DoSave(handler, player, tok[1], radius);
        }

        if (sub == "load" || sub == "还原")
        {
            if (tok.size() < 2) { handler->PSendSysMessage("|cffff0000用法|r：.scene load <名字>"); return true; }
            return DoLoad(handler, player, tok[1], false);
        }

        if (sub == "rebuild" || sub == "重建")
        {
            if (tok.size() < 2) { handler->PSendSysMessage("|cffff0000用法|r：.scene rebuild <名字>"); return true; }
            return DoLoad(handler, player, tok[1], true);
        }

        if (sub == "info" || sub == "详情")
        {
            if (tok.size() < 2) { handler->PSendSysMessage("|cffff0000用法|r：.scene info <名字>"); return true; }
            return DoInfo(handler, tok[1]);
        }

        if (sub == "del" || sub == "delete" || sub == "删除")
        {
            if (tok.size() < 2) { handler->PSendSysMessage("|cffff0000用法|r：.scene del <名字>"); return true; }
            return DoDel(handler, tok[1]);
        }

        return ShowUsage(handler);
    }

private:
    // ------------------------------------------------------------------
    static bool ShowUsage(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ff00===== 场景快照 =====|r");
        handler->PSendSysMessage("|cffffff00.scene save <名字> [半径]|r  存快照（默认50码）");
        handler->PSendSysMessage("|cffffff00.scene load <名字>|r         还原");
        handler->PSendSysMessage("|cffffff00.scene rebuild <名字>|r      还原+重建被删的NPC");
        handler->PSendSysMessage("|cffffff00.scene list|r                所有快照");
        handler->PSendSysMessage("|cffffff00.scene info <名字>|r         看内容");
        handler->PSendSysMessage("|cffffff00.scene del <名字>|r          删除");
        handler->PSendSysMessage("|cff888888存：位置/朝向/状态/阵营/模型/缩放/站姿/动作/功能位|r");
        return true;
    }

    // ------------------------------------------------------------------
    static bool DoSave(ChatHandler* handler, Player* player,
                       std::string const& name, float radius)
    {
        if (radius <= 0.f)
            radius = SCENE_DEFAULT_RADIUS;
        if (radius > SCENE_MAX_RADIUS)
        {
            radius = SCENE_MAX_RADIUS;
            handler->PSendSysMessage("|cffffff00半径已钳到 %.0f 码|r", SCENE_MAX_RADIUS);
        }

        std::vector<Creature*> list;
        CollectNear(player, radius, list);

        if (list.empty())
        {
            handler->PSendSysMessage("|cffffff00半径 %.0f 码内没有可存的 NPC|r", radius);
            return true;
        }
        if (list.size() > SCENE_MAX_NPC)
        {
            handler->PSendSysMessage("|cffff0000NPC 太多（%zu 个），上限 %u|r",
                list.size(), SCENE_MAX_NPC);
            handler->PSendSysMessage("|cff888888把半径调小一点|r");
            return true;
        }

        // 同名快照先删掉（覆盖语义）
        if (uint32 old = FindSceneId(name))
        {
            WorldDatabase.DirectPExecute("DELETE FROM `custom_scene_npc` WHERE `scene_id` = {}", old);
            WorldDatabase.DirectPExecute("DELETE FROM `custom_scene` WHERE `id` = {}", old);
            handler->PSendSysMessage("|cffffff00已覆盖同名快照|r");
        }

        // 建主表记录
        WorldDatabase.DirectPExecute(
            "INSERT INTO `custom_scene` "
            "(`name`,`creator`,`map`,`center_x`,`center_y`,`center_z`,`radius`,`npc_count`,`created`) "
            "VALUES ('{}','{}',{},{},{},{},{},{},{})",
            Esc(name), Esc(player->GetName()),
            player->GetMapId(),
            player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(),
            radius, uint32(list.size()), uint32(GameTime::GetGameTime()));

        uint32 sceneId = FindSceneId(name);
        if (!sceneId)
        {
            handler->PSendSysMessage("|cffff0000保存失败|r，请确认已执行 |cffffff00sql/41_scene_tables.sql|r");
            return true;
        }

        uint32 idx = 0, temp = 0;
        for (Creature* c : list)
        {
            SceneNpc n = Capture(c);
            if (!n.guid)
                ++temp;

            WorldDatabase.DirectPExecute(
                "INSERT INTO `custom_scene_npc` "
                "(`scene_id`,`idx`,`guid`,`entry`,`pos_x`,`pos_y`,`pos_z`,`orientation`,"
                " `unit_flags`,`faction`,`react`,`display`,`scale`,`stand`,`emote`,`npcflag`,`name_cache`) "
                "VALUES ({},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},'{}')",
                sceneId, idx++, n.guid, n.entry,
                n.x, n.y, n.z, n.o,
                n.unitFlags, n.faction, uint32(n.react), n.display, n.scale,
                uint32(n.stand), n.emote, n.npcflag, Esc(n.name));
        }

        handler->PSendSysMessage("|cff00ff00[已保存]|r 快照 |cffffff00%s|r  %u 个 NPC  半径 %.0f 码",
            name.c_str(), idx, radius);
        if (temp)
            handler->PSendSysMessage("|cff888888其中 %u 个是临时召唤物，"
                "还原时需要用 |cffffff00rebuild|r|cff888888 才能重新召唤|r", temp);
        return true;
    }

    // ------------------------------------------------------------------
    static bool DoLoad(ChatHandler* handler, Player* player,
                       std::string const& name, bool rebuild)
    {
        uint32 sceneId = FindSceneId(name);
        if (!sceneId)
        {
            handler->PSendSysMessage("|cffff0000找不到快照|r：%s", name.c_str());
            handler->PSendSysMessage("用 |cffffff00.scene list|r 看有哪些");
            return true;
        }

        QueryResult r = WorldDatabase.PQuery(
            "SELECT `guid`,`entry`,`pos_x`,`pos_y`,`pos_z`,`orientation`,"
            "`unit_flags`,`faction`,`react`,`display`,`scale`,`stand`,`emote`,`npcflag`,`name_cache` "
            "FROM `custom_scene_npc` WHERE `scene_id` = {} ORDER BY `idx`", sceneId);

        if (!r)
        {
            handler->PSendSysMessage("|cffff0000快照是空的|r");
            return true;
        }

        uint32 done = 0, missing = 0, rebuilt = 0;

        do
        {
            Field* f = r->Fetch();
            SceneNpc n;
            n.guid      = f[0].GetUInt32();
            n.entry     = f[1].GetUInt32();
            n.x         = f[2].GetFloat();
            n.y         = f[3].GetFloat();
            n.z         = f[4].GetFloat();
            n.o         = f[5].GetFloat();
            n.unitFlags = f[6].GetUInt32();
            n.faction   = f[7].GetUInt32();
            n.react     = uint8(f[8].GetUInt32());
            n.display   = f[9].GetUInt32();
            n.scale     = f[10].GetFloat();
            n.stand     = uint8(f[11].GetUInt32());
            n.emote     = f[12].GetUInt32();
            n.npcflag   = f[13].GetUInt32();
            n.name      = f[14].GetString();

            Creature* c = nullptr;

            // 有 spawnId 的，按 guid 精确找回原来那只
            if (n.guid)
            {
                // ObjectAccessor 没有按 spawnId 直接查的接口，
                // 用地图的 creature 存储找（Map.h GetCreatureBySpawnId）
                c = player->GetMap()->GetCreatureBySpawnId(n.guid);
            }

            if (c && c->IsInWorld())
            {
                Restore(c, n);
                ++done;
                continue;
            }

            // 找不到了
            ++missing;
            if (!rebuild || !n.entry)
                continue;

            // rebuild 模式：按 entry 重新召唤一只
            if (!sObjectMgr->GetCreatureTemplate(n.entry))
                continue;

            if (TempSummon* ts = player->SummonCreature(n.entry, n.x, n.y, n.z, n.o,
                    TEMPSUMMON_MANUAL_DESPAWN, Milliseconds(0)))
            {
                Restore(ts, n);
                ++rebuilt;
            }
        }
        while (r->NextRow());

        handler->PSendSysMessage("|cff00ff00[已还原]|r 快照 |cffffff00%s|r", name.c_str());
        handler->PSendSysMessage("  就位 |cffffffff%u|r 个", done);
        if (missing)
        {
            if (rebuild)
                handler->PSendSysMessage("  重建 |cffffff00%u|r 个（%u 个失败）",
                    rebuilt, missing - rebuilt);
            else
                handler->PSendSysMessage("  |cffffff00%u 个找不到|r —— "
                    "用 |cffffff00.scene rebuild %s|r 可重新召唤", missing, name.c_str());
        }
        return true;
    }

    // ------------------------------------------------------------------
    static bool DoList(ChatHandler* handler)
    {
        QueryResult r = WorldDatabase.Query(
            "SELECT `name`,`creator`,`map`,`npc_count`,`radius` "
            "FROM `custom_scene` ORDER BY `id` DESC LIMIT 30");

        if (!r)
        {
            handler->PSendSysMessage("|cffffff00还没有快照|r");
            handler->PSendSysMessage("用 |cffffff00.scene save <名字>|r 存一个");
            return true;
        }

        handler->PSendSysMessage("|cff00ff00===== 快照列表 =====|r");
        uint32 n = 0;
        do
        {
            Field* f = r->Fetch();
            handler->PSendSysMessage("|cffffff00%-16s|r %3u个  地图%u  %.0f码  |cff888888by %s|r",
                f[0].GetCString(), f[3].GetUInt32(), f[2].GetUInt32(),
                f[4].GetFloat(), f[1].GetCString());
            ++n;
        }
        while (r->NextRow());
        handler->PSendSysMessage("|cff888888共 %u 个（最多显示30）|r", n);
        return true;
    }

    // ------------------------------------------------------------------
    static bool DoInfo(ChatHandler* handler, std::string const& name)
    {
        uint32 sceneId = FindSceneId(name);
        if (!sceneId)
        {
            handler->PSendSysMessage("|cffff0000找不到快照|r：%s", name.c_str());
            return true;
        }

        QueryResult h = WorldDatabase.PQuery(
            "SELECT `creator`,`map`,`radius`,`npc_count`,`created` "
            "FROM `custom_scene` WHERE `id` = {}", sceneId);
        if (h)
        {
            Field* f = h->Fetch();
            handler->PSendSysMessage("|cff00ff00===== %s =====|r", name.c_str());
            handler->PSendSysMessage("创建者 |cffffffff%s|r  地图 |cffffffff%u|r  "
                "半径 |cffffffff%.0f|r  NPC |cffffffff%u|r 个",
                f[0].GetCString(), f[1].GetUInt32(), f[2].GetFloat(), f[3].GetUInt32());
        }

        QueryResult r = WorldDatabase.PQuery(
            "SELECT `entry`,`name_cache`,COUNT(*) as cnt FROM `custom_scene_npc` "
            "WHERE `scene_id` = {} GROUP BY `entry`,`name_cache` "
            "ORDER BY cnt DESC LIMIT 20", sceneId);
        if (r)
        {
            handler->PSendSysMessage("|cff00ccff----- 内容 -----|r");
            do
            {
                Field* f = r->Fetch();
                handler->PSendSysMessage("  %-20s |cff888888entry %u|r  x%u",
                    f[1].GetCString(), f[0].GetUInt32(), f[2].GetUInt32());
            }
            while (r->NextRow());
        }
        return true;
    }

    // ------------------------------------------------------------------
    static bool DoDel(ChatHandler* handler, std::string const& name)
    {
        uint32 sceneId = FindSceneId(name);
        if (!sceneId)
        {
            handler->PSendSysMessage("|cffff0000找不到快照|r：%s", name.c_str());
            return true;
        }

        WorldDatabase.DirectPExecute("DELETE FROM `custom_scene_npc` WHERE `scene_id` = {}", sceneId);
        WorldDatabase.DirectPExecute("DELETE FROM `custom_scene` WHERE `id` = {}", sceneId);

        handler->PSendSysMessage("|cff00ff00[已删除]|r 快照 %s", name.c_str());
        return true;
    }
};

void AddSC_scene_commandscript()
{
    new scene_commandscript();
}
