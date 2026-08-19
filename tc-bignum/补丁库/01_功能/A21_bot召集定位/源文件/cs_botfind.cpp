/*
 * ============================================================================
 *  step35  .bf —— 游荡 bot 定位与召集
 * ============================================================================
 *
 *  用户需求：「我无法定位bot，有没有什么办法可以召集游荡npc到我身边，
 *              我好测试」
 *
 *  【为什么官方指令不够用】（实查结论）
 *
 *    .npcbot recall            botcommands.cpp:2937
 *        只对【自己雇佣的】bot 有效：owner->GetBotMgr()->GetBot(guid)
 *        游荡 bot 没有 owner，用不了。
 *
 *    .npcbot list spawned free botcommands.cpp:3959
 *        能列出无主 bot，但只给 entry + 名字 + 等级 + 区域名，
 *        【没有坐标】，你还是找不到它在哪。
 *
 *  所以这个指令补三件事：
 *    1. 列出附近/全服游荡 bot，带【距离和坐标】
 *    2. 把游荡 bot【传送到你身边】（测试 step33/34 必需）
 *    3. 把你【传送到 bot 那里】（想看它原本在干嘛）
 *
 *  权限：rbac::RBAC_PERM_COMMAND_WORLDTOOLS（step21 自建 = 71012）
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "DBCStores.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "World.h"
#include "WorldSession.h"

#include "botdatamgr.h"
#include "botmgr.h"
#include "bot_ai.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

// ============================================================================
//  小工具（沿用 step29-32 的写法）
// ============================================================================

static std::vector<std::string> Tok(char const* args)
{
    std::vector<std::string> out;
    if (!args)
        return out;

    std::string s(args);
    size_t i = 0;
    while (i < s.size())
    {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
            ++i;
        if (i >= s.size())
            break;
        size_t j = i;
        while (j < s.size() && s[j] != ' ' && s[j] != '\t')
            ++j;
        out.push_back(s.substr(i, j - i));
        i = j;
    }
    return out;
}

static std::string Lower(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}

static bool IsAllDigit(std::string const& s)
{
    if (s.empty())
        return false;
    for (char c : s)
        if (c < '0' || c > '9')
            return false;
    return true;
}

// 一次最多显示多少条，防刷屏
static uint32 const BF_MAX_SHOW = 20;

// 判断一个 bot 是不是"无主的游荡 bot"
//   botdatamgr.h:216  SelectNpcBotData(entry)->owner
static bool IsFreeWanderer(Creature const* bot)
{
    if (!bot || !bot->IsInWorld() || !bot->IsAlive())
        return false;
    if (!bot->IsNPCBot())
        return false;

    NpcBotData const* data = BotDataMgr::SelectNpcBotData(bot->GetEntry());
    if (!data || data->owner != 0)
        return false;                 // 有主人的不算

    return true;
}

// ============================================================================
//  .bf 实现
// ============================================================================
class botfind_commandscript : public CommandScript
{
public:
    botfind_commandscript() : CommandScript("botfind_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "bf",     rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleBotFind, "" },
            { "botfind",rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleBotFind, "" },
            // step37: 独立的"传送自己过去"指令（用户要求单独做一个）
            { "tome",   rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleToMe,    "" },
        };
        return commandTable;
    }

    static void SendHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff00ff00[.bf 游荡bot定位与召集]|r");
        handler->SendSysMessage("  .bf                 列出【本地图】的游荡bot，带距离");
        handler->SendSysMessage("  .bf all             列出【全服】游荡bot");
        handler->SendSysMessage("  .bf near <码>       只列出这个距离内的");
        handler->SendSysMessage("|cffffff00--- 召集（测试用）---|r");
        handler->SendSysMessage("  .bf come            把【最近的一个】叫过来");
        handler->SendSysMessage("  .bf come <数量>     把最近的N个叫过来");
        handler->SendSysMessage("  .bf come entry <ID> 把指定entry的叫过来");
        handler->SendSysMessage("|cffffff00--- 过去找它 ---|r");
        handler->SendSysMessage("  .bf goto            传送到最近的游荡bot");
        handler->SendSysMessage("  .bf goto entry <ID> 传送到指定bot");
        handler->SendSysMessage("|cffffff00--- 传送自己（最安全，不动bot状态）---|r");
        handler->SendSysMessage("  .tome               传送到最近的游荡bot（.bf goto 的简写）");
        handler->SendSysMessage("  .tome <entry>       传送到指定entry的bot");
        handler->SendSysMessage("|cff888888 官方 .npcbot recall 只能召自己雇的bot|r");
        handler->SendSysMessage("|cff888888 官方 .npcbot list spawned free 不给坐标|r");
    }

    // ------------------------------------------------------------------
    //  收集符合条件的游荡 bot
    //    sameMapOnly  只要本地图的
    //    maxDist      距离上限，0 = 不限
    // ------------------------------------------------------------------
    static void Collect(Player* player, bool sameMapOnly, float maxDist,
                        std::vector<Creature*>& out)
    {
        // botdatamgr.h:234  static NpcBotRegistry const& GetExistingNPCBots();
        NpcBotRegistry const& all = BotDataMgr::GetExistingNPCBots();

        for (Creature const* cbot : all)
        {
            if (!IsFreeWanderer(cbot))
                continue;

            // registry 里存的是 const，这里要操作它得去 const
            Creature* bot = const_cast<Creature*>(cbot);

            if (sameMapOnly && bot->GetMapId() != player->GetMapId())
                continue;

            if (maxDist > 0.0f)
            {
                if (bot->GetMapId() != player->GetMapId())
                    continue;
                if (player->GetDistance(bot) > maxDist)
                    continue;
            }

            out.push_back(bot);
        }

        // 同图的按距离排序，跨图的排后面
        uint32 myMap = player->GetMapId();
        std::sort(out.begin(), out.end(), [player, myMap](Creature* a, Creature* b)
        {
            bool aSame = a->GetMapId() == myMap;
            bool bSame = b->GetMapId() == myMap;
            if (aSame != bSame)
                return aSame;
            if (!aSame)
                return a->GetEntry() < b->GetEntry();
            return player->GetDistance(a) < player->GetDistance(b);
        });
    }

    // ------------------------------------------------------------------
    //  打印一条
    // ------------------------------------------------------------------
    static void PrintBot(ChatHandler* handler, Player* player, Creature* bot, uint32 idx)
    {
        char buf[512];

        std::string zoneName = "未知";
        if (AreaTableEntry const* zone = sAreaTableStore.LookupEntry(bot->GetZoneId()))
        {
            uint8 loc = handler->GetSession() ? uint8(handler->GetSessionDbLocaleIndex()) : uint8(0);
            if (zone->AreaName[loc] && *zone->AreaName[loc])
                zoneName = zone->AreaName[loc];
        }

        if (bot->GetMapId() == player->GetMapId())
        {
            snprintf(buf, sizeof(buf),
                     "%u) |cff00ff00%s|r  entry %u  %u级  |cffffff00%.0f码|r  %s",
                     idx, bot->GetName().c_str(), bot->GetEntry(),
                     uint32(bot->GetLevel()), player->GetDistance(bot), zoneName.c_str());
        }
        else
        {
            snprintf(buf, sizeof(buf),
                     "%u) |cff00ff00%s|r  entry %u  %u级  |cff888888[地图%u]|r  %s",
                     idx, bot->GetName().c_str(), bot->GetEntry(),
                     uint32(bot->GetLevel()), bot->GetMapId(), zoneName.c_str());
        }
        handler->SendSysMessage(buf);
    }

    // ------------------------------------------------------------------
    //  主入口
    // ------------------------------------------------------------------
    static bool HandleBotFind(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = Tok(args);
        char buf[512];

        std::string s0 = tok.empty() ? "" : Lower(tok[0]);

        if (s0 == "help" || (!tok.empty() && tok[0] == "帮助"))
        {
            SendHelp(handler);
            return true;
        }

        // ================= .bf come =================
        if (s0 == "come" || (!tok.empty() && tok[0] == "召集"))
        {
            // .bf come entry <id>
            if (tok.size() >= 3 && Lower(tok[1]) == "entry" && IsAllDigit(tok[2]))
            {
                uint32 entry = uint32(atoi(tok[2].c_str()));

                std::vector<Creature*> found;
                Collect(player, false, 0.0f, found);

                for (Creature* bot : found)
                {
                    if (bot->GetEntry() != entry)
                        continue;

                    // botmgr.h:256
                    // static void TeleportBot(Creature*, Map*, Position const*, bool quick, ...)
                    Position pos = player->GetPosition();
                    BotMgr::TeleportBot(bot, player->GetMap(), &pos, true);

                    // step37: 必须重设"家"，否则它发现离原路点太远会念炉石传回去
                    // （bot_ai.cpp:18632 -> WANDERER_HEARTHSTONE，10秒读条期间无法对话）
                    if (bot_ai* bai = bot->GetBotAI())
                        bai->ResetWanderHomeToCurrent();

                    snprintf(buf, sizeof(buf), "|cff00ff00 已召来: %s (entry %u)|r",
                             bot->GetName().c_str(), entry);
                    handler->SendSysMessage(buf);
                    return true;
                }

                snprintf(buf, sizeof(buf), "|cffff0000 没找到 entry %u 的游荡bot|r", entry);
                handler->SendSysMessage(buf);
                handler->SetSentErrorMessage(true);
                return false;
            }

            // .bf come [数量]
            uint32 want = 1;
            if (tok.size() >= 2 && IsAllDigit(tok[1]))
                want = std::max<uint32>(1, std::min<uint32>(10, uint32(atoi(tok[1].c_str()))));

            std::vector<Creature*> found;
            Collect(player, true, 0.0f, found);      // 先只找本图的

            if (found.empty())
            {
                handler->SendSysMessage("|cffff0000 本地图没有游荡bot|r");
                handler->SendSysMessage("|cffffff00 用 .bf all 看全服，再 .bf come entry <ID>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            uint32 done = 0;
            for (Creature* bot : found)
            {
                if (done >= want)
                    break;

                Position pos = player->GetPosition();
                BotMgr::TeleportBot(bot, player->GetMap(), &pos, true);

                // step37: 同上，重设"家"防止它念炉石跑回去
                if (bot_ai* bai = bot->GetBotAI())
                    bai->ResetWanderHomeToCurrent();

                ++done;

                snprintf(buf, sizeof(buf), "  |cff00ff00%s|r (entry %u) 已到",
                         bot->GetName().c_str(), bot->GetEntry());
                handler->SendSysMessage(buf);
            }

            snprintf(buf, sizeof(buf), "|cff00ff00 共召来 %u 个游荡bot|r", done);
            handler->SendSysMessage(buf);
            return true;
        }

        // ================= .bf goto =================
        if (s0 == "goto" || (!tok.empty() && tok[0] == "过去"))
        {
            Creature* target = nullptr;

            if (tok.size() >= 3 && Lower(tok[1]) == "entry" && IsAllDigit(tok[2]))
            {
                uint32 entry = uint32(atoi(tok[2].c_str()));
                std::vector<Creature*> found;
                Collect(player, false, 0.0f, found);
                for (Creature* bot : found)
                    if (bot->GetEntry() == entry)
                    {
                        target = bot;
                        break;
                    }

                if (!target)
                {
                    snprintf(buf, sizeof(buf), "|cffff0000 没找到 entry %u 的游荡bot|r", entry);
                    handler->SendSysMessage(buf);
                    handler->SetSentErrorMessage(true);
                    return false;
                }
            }
            else
            {
                std::vector<Creature*> found;
                Collect(player, true, 0.0f, found);
                if (found.empty())
                {
                    handler->SendSysMessage("|cffff0000 本地图没有游荡bot，试试 .bf all|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                target = found[0];
            }

            if (player->IsInCombat())
            {
                handler->SendSysMessage("|cffff0000 战斗中不能传送|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            player->TeleportTo(target->GetMapId(),
                               target->GetPositionX(), target->GetPositionY(),
                               target->GetPositionZ(), target->GetOrientation());

            snprintf(buf, sizeof(buf), "|cff00ff00 已传送到 %s 身边|r", target->GetName().c_str());
            handler->SendSysMessage(buf);
            return true;
        }

        // ================= .bf near <码> =================
        float maxDist = 0.0f;
        bool sameMapOnly = true;

        if (s0 == "near" || (!tok.empty() && tok[0] == "附近"))
        {
            if (tok.size() >= 2 && IsAllDigit(tok[1]))
                maxDist = float(atoi(tok[1].c_str()));
            else
                maxDist = 200.0f;
        }
        else if (s0 == "all" || (!tok.empty() && tok[0] == "全部"))
        {
            sameMapOnly = false;
        }
        else if (!tok.empty())
        {
            // 无法识别的参数，给帮助
            SendHelp(handler);
            return true;
        }

        // ================= 列表 =================
        std::vector<Creature*> found;
        Collect(player, sameMapOnly, maxDist, found);

        if (found.empty())
        {
            if (sameMapOnly)
            {
                handler->SendSysMessage("|cffff0000 本地图没有无主的游荡bot|r");
                handler->SendSysMessage("|cffffff00 试试 .bf all 看全服|r");
            }
            else
            {
                handler->SendSysMessage("|cffff0000 全服都没有无主的游荡bot|r");
                handler->SendSysMessage("|cffffff00 检查 conf: NpcBot.WanderingBots.Continents.Enable|r");
            }
            return true;
        }

        snprintf(buf, sizeof(buf), "|cff00ff00[找到 %u 个游荡bot]|r", uint32(found.size()));
        handler->SendSysMessage(buf);

        uint32 shown = 0;
        for (Creature* bot : found)
        {
            if (shown >= BF_MAX_SHOW)
                break;
            ++shown;
            PrintBot(handler, player, bot, shown);
        }

        if (found.size() > BF_MAX_SHOW)
        {
            snprintf(buf, sizeof(buf), "|cffffff00 ... 还有 %u 个没显示|r",
                     uint32(found.size()) - BF_MAX_SHOW);
            handler->SendSysMessage(buf);
        }

        handler->SendSysMessage("|cffffff00 .bf come 召来最近的 | .bf goto 过去找它|r");
        return true;
    }

    // ------------------------------------------------------------------
    //  .tome —— 传送【自己】到 bot 身边
    //
    //  为什么单独做一个：
    //    .bf come 是把 bot 拽过来，会改变它的位置状态；
    //    .tome 只移动你自己，【完全不碰 bot】，最安全。
    //
    //  用户原话：「多做一个传送自己的指令」
    // ------------------------------------------------------------------
    static bool HandleToMe(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        char buf[512];

        if (player->IsInCombat())
        {
            handler->SendSysMessage("|cffff0000 战斗中不能传送|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::vector<std::string> tok = Tok(args);
        Creature* target = nullptr;

        std::vector<Creature*> found;

        if (!tok.empty() && IsAllDigit(tok[0]))
        {
            // .tome <entry>  全服范围找
            uint32 entry = uint32(atoi(tok[0].c_str()));
            Collect(player, false, 0.0f, found);
            for (Creature* bot : found)
                if (bot->GetEntry() == entry)
                {
                    target = bot;
                    break;
                }

            if (!target)
            {
                snprintf(buf, sizeof(buf), "|cffff0000 没找到 entry %u 的游荡bot|r", entry);
                handler->SendSysMessage(buf);
                handler->SendSysMessage("|cffffff00 用 .bf all 看全服列表|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
        }
        else
        {
            // .tome  先找本图最近的，本图没有就找全服
            Collect(player, true, 0.0f, found);
            if (found.empty())
                Collect(player, false, 0.0f, found);

            if (found.empty())
            {
                handler->SendSysMessage("|cffff0000 全服都没有无主的游荡bot|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            target = found[0];
        }

        player->TeleportTo(target->GetMapId(),
                           target->GetPositionX(), target->GetPositionY(),
                           target->GetPositionZ(), target->GetOrientation());

        snprintf(buf, sizeof(buf),
                 "|cff00ff00 已传送到 %s (entry %u) 身边|r",
                 target->GetName().c_str(), target->GetEntry());
        handler->SendSysMessage(buf);
        handler->SendSysMessage("|cffffff00 它在自己路点附近，不会念炉石，可以直接右键|r");
        return true;
    }
};

void AddSC_botfind_commandscript()
{
    new botfind_commandscript();
}

/* ============================================================================
 *  API 核实记录（全部 grep 自本仓库）
 * ============================================================================
 *
 *  botdatamgr.h:187   using NpcBotRegistry = std::set<Creature const*>;
 *  botdatamgr.h:234   static NpcBotRegistry const& GetExistingNPCBots();
 *                     public 静态，可直接遍历全服所有 bot
 *  botdatamgr.h:216   static NpcBotData const* SelectNpcBotData(uint32 entry);
 *                     用 ->owner == 0 判断无主
 *  botmgr.h:256       static void TeleportBot(Creature* bot, Map* newMap,
 *                                             Position const* pos, bool quick = false,
 *                                             bool reset = false, bot_ai* detached_ai = nullptr);
 *                     public 静态。bot_ai.cpp:7400 官方自己就是这么传送游荡bot的：
 *                       BotMgr::TeleportBot(me, targetMap, _travel_node_cur, true);
 *
 *  为什么不用官方指令：
 *  botcommands.cpp:2937  HandleNpcBotRecallCommand
 *      owner->GetBotMgr()->GetBot(guid)  -> 只能召【自己雇佣的】
 *  botcommands.cpp:3959  HandleNpcBotSpawnedFreeCommand
 *      输出只有 entry/名字/等级/区域名，【没有坐标和距离】
 *
 *  Chat.h:104/105     getSelectedCreature() / getSelectedUnit()
 *  AreaTableEntry     sAreaTableStore（DBCStores.h）用于区域名本地化
 * ============================================================================
 */
