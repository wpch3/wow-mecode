/*
 * ============================================================================
 *  step40  .pbot —— 真实 Player 型 Bot（第1步：能站着、不崩服）
 * ============================================================================
 *
 *  目标：验证「TrinityCore 能否创建无网络连接的真实 Player 对象」
 *
 *  这套方案的核心是"无连接会话"，三道门全部实证过：
 *
 *    WorldSession.h:495   构造函数收 std::shared_ptr<WorldSocket>，可传 nullptr
 *    WorldSession.h:503   bool PlayerDisconnected() const { return !m_Socket; }
 *                         -> 核心【自己就有】"无socket会话"这个概念
 *    WorldSession.cpp:211 void WorldSession::SendPacket(...)
 *                         { if (!m_Socket) return; }
 *                         -> 发包被安全丢弃，不崩
 *
 *  这意味着：无连接会话 + 真实 Player 对象 = 有背包、有任务日志、有天赋的 bot。
 *  这正是 playerbot 的原理。
 *
 *  三道门与破法：
 *
 *    门1  LoginQueryHolder 定义在 CharacterHandler.cpp:58，不在任何头文件
 *         -> 外部无法构造，无法直接调 HandlePlayerLogin()
 *         -> 解法：伪造 CMSG_PLAYER_LOGIN 包走 QueuePacket()，用官方原路
 *
 *    门2  _legitCharacters 白名单（WorldSession.h:1285）
 *         只在客户端请求角色列表时填充，bot 没客户端 -> 集合为空 -> 登录被拒
 *         -> 解法：给 WorldSession 加一个 public 的 AddLegitCharacter()
 *
 *    门3  WorldSession.cpp:298  m_Socket->CloseSocket();   【没判空】
 *         IsConnectionIdle() 迟早为真 -> 空指针崩服
 *         -> 解法：会话用 SEC_ADMINISTRATOR，
 *            HasPermission(RBAC_PERM_IGNORE_IDLE_CONNECTION) 为真则条件短路
 *
 *  已知约束：
 *    - 不硬编码账号ID/角色GUID，全部运行时查库
 *    - SQL 占位符用 {} 不是 %u
 *    - 查 API 同时确认访问段
 *
 *  权限：rbac::RBAC_PERM_COMMAND_WORLDTOOLS（step21 自建 = 71012）
 * ============================================================================
 */

#include "pbot_autoaccept.h"    // step42: PBotEntry / PBotAutoFlags / g_pbots

#include "ScriptMgr.h"
#include "Chat.h"
#include "Common.h"             // AccountTypes / SEC_ADMINISTRATOR (common/Common.h:38,43)
#include "DatabaseEnv.h"
#include "Duration.h"           // Minutes (common/Utilities/Duration.h:30)
#include "Group.h"              // Group::IsMember / IsLeader / ChangeLeader / AddMember
#include "GroupMgr.h"           // sGroupMgr->AddGroup (invite 需要建队伍)
#include "Map.h"                // Map::IsDungeon / IsBattlegroundOrArena / GetInstanceId
#include "MotionMaster.h"       // MoveFollow / MoveIdle
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"            // CMSG_PLAYER_LOGIN (Opcodes.h:90)
#include "Player.h"
#include "RBAC.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <cstdio>
#include <string>
#include <vector>

// ============================================================================
//  头文件
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

// ============================================================================
//  bot 登记表
// ============================================================================
//  【step42 起】struct PBotEntry 移到 pbot_autoaccept.h，
//  因为自动接受模块也要读它。这里改成【定义】那个 extern 变量。
//  两边共用同一份数据 -> .pbot auto 改的就是自动层每帧读的值。
std::vector<PBotEntry> g_pbots;

// ============================================================================

//  A41 · spawn 核心（不依赖 ChatHandler，供自动上线复用）

// ============================================================================

//  原 SpawnBot 把"查找+校验+登录"和"给玩家发消息"混在一起，

//  自动上线时没有 ChatHandler，所以必须把核心逻辑抽出来。

//  这里是原 :252-300 那段的无 UI 版本。

// ============================================================================

bool PBotSpawnCore(uint32 accountId, ObjectGuid charGuid,

                std::string const& charName, uint32 autoFlags,

                std::string* reason)

{

// 已经在线

    if (ObjectAccessor::FindPlayer(charGuid))

    {

        if (reason) *reason = "已经在线";

        return false;

    }

    // 账号已有会话

    if (sWorld->FindSession(accountId))

    {

        if (reason) *reason = "账号已有活动会话";

        return false;

    }

    // 造无连接会话（同 SpawnBot :263）

    std::string sessName = charName;

    WorldSession* sess = new WorldSession(

        accountId,

        std::move(sessName),

        nullptr,                    // 无 socket

        SEC_ADMINISTRATOR,

        2,                          // WotLK

        0,

        Minutes(0),

        LOCALE_zhCN,

        0,

        false);

    sess->SetBotSession(true);

    sess->AddLegitCharacter(charGuid);

    sWorld->AddSession(sess);

    WorldPacket* pkt = new WorldPacket(CMSG_PLAYER_LOGIN, 8);

    *pkt << charGuid;

    sess->QueuePacket(pkt);

    // 登记进内存表（去重：同一个 guid 只留一份）

    for (PBotEntry const& e : g_pbots)

        if (e.CharGuid == charGuid)

            return true;

    g_pbots.push_back({ accountId, charGuid, charName, autoFlags });

    return true;

}

// ============================================================================

//  A41 · 名册读写

// ============================================================================

void PBotLoadRoster(ObjectGuid ownerGuid, std::vector<PBotEntry>& out)

{

    out.clear();

    QueryResult res = CharacterDatabase.PQuery(

        "SELECT bot_account, bot_guid, bot_name, auto_flags "

        "FROM playerbot_roster WHERE owner_guid = {} AND enabled = 1",

        ownerGuid.GetCounter());

    if (!res)

        return;

    do

    {

        Field* f = res->Fetch();

        PBotEntry e;

        e.AccountId = f[0].GetUInt32();

        e.CharGuid  = ObjectGuid::Create<HighGuid::Player>(f[1].GetUInt32());

        e.CharName  = f[2].GetString();

        e.AutoFlags = f[3].GetUInt32();

        out.push_back(e);

    } while (res->NextRow());

}

void PBotRosterAdd(ObjectGuid ownerGuid, PBotEntry const& e)

{

    CharacterDatabase.PExecute(

        "REPLACE INTO playerbot_roster "

        "(owner_guid, bot_account, bot_guid, bot_name, auto_flags, enabled) "

        "VALUES ({}, {}, {}, '{}', {}, 1)",

        ownerGuid.GetCounter(), e.AccountId, e.CharGuid.GetCounter(),

        e.CharName, e.AutoFlags);

}

void PBotRosterRemove(ObjectGuid ownerGuid, ObjectGuid botGuid)

{

    CharacterDatabase.PExecute(

        "DELETE FROM playerbot_roster WHERE owner_guid = {} AND bot_guid = {}",

        ownerGuid.GetCounter(), botGuid.GetCounter());

}

// ============================================================================
//  遍历所有bot，找名字匹配的那个
//    太小 = 和你重叠，看不见也点不到
//    太大 = 不像"在身边"
//    2.0 码 ≈ 一个身位，正好面对面
// ============================================================================
static constexpr float PBOT_COME_DIST = 2.0f;

// ============================================================================
//  .pbot 实现
// ============================================================================
class playerbot_commandscript : public CommandScript
{
public:
    playerbot_commandscript() : CommandScript("playerbot_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "pbot", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandlePBot, "" },
        };
        return commandTable;
    }

    static void SendHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff00ff00[.pbot 真实Player型Bot]|r");
        handler->SendSysMessage("  .pbot spawn <账号名> <角色名>   让这个角色以bot身份登录");
        handler->SendSysMessage("  .pbot list                      列出已登录的bot");
        handler->SendSysMessage("  .pbot despawn <角色名>          让它下线");
        handler->SendSysMessage("  .pbot come <角色名>             把它传送到你身前");
        handler->SendSysMessage("  .pbot goto <角色名>             你传送到它那里");
        handler->SendSysMessage("|cff00ff00--- 自动接受（step42）---|r");
        handler->SendSysMessage("  .pbot auto                      看所有bot的开关");
        handler->SendSysMessage("  .pbot auto <角色名|all> <项目> <on|off>");
        handler->SendSysMessage("      项目: group/guild/duel/res/summon/trade/release/all");
        handler->SendSysMessage("  .pbot auto master <on|off>      总开关（应急全停）");
        handler->SendSysMessage("  .pbot accept <角色名|all>       立刻处理挂起的邀请");
        handler->SendSysMessage("|cff00ff00--- 队伍与移动（A39）---|r");
        handler->SendSysMessage("  .pbot invite <角色名|all>       把它拉进你的队伍");
        handler->SendSysMessage("  .pbot kick [角色名]             把它移出队伍（不填=全部）");
        handler->SendSysMessage("  .pbot follow <角色名|all>       让它跟着你走");
        handler->SendSysMessage("  .pbot stay <角色名|all>         让它原地待命");
        handler->SendSysMessage("  .pbot give <角色名> <物品ID> [数量]  让它把东西给你");
        handler->SendSysMessage("|cffffff00 默认开: 组队/公会/复活/召唤/释放|r");
        handler->SendSysMessage("|cffffff00 默认关: 决斗、交易（交易只开窗口不点确认）|r");
        handler->SendSysMessage("|cffffff00--- 中文别名（都能用）---|r");
        handler->SendSysMessage("  .pbot 上线 <账号> <角色>  |  .pbot 列表");
        handler->SendSysMessage("  .pbot 过来 <角色名>       |  .pbot 下线 <角色名>");
        handler->SendSysMessage("  .pbot 去找 <角色名>       |  .pbot 自动 / .pbot 接受");
        handler->SendSysMessage("|cffff0000 中文别名和英文指令等价，随便用哪个|r");
    }

    // ------------------------------------------------------------------
    //  账号ID和角色GUID都要校验，防止张冠李戴
    // ------------------------------------------------------------------
    static uint32 LookupAccountId(std::string const& accName)
    {
        std::string safe = accName;
        LoginDatabase.EscapeString(safe);

        // 铁律5：本仓库 DirectPExecute/PQuery 走 fmt，占位符是 {} 不是 %u
        QueryResult res = LoginDatabase.PQuery(
            "SELECT id FROM account WHERE username = '{}'", safe);
        if (!res)
            return 0;
        return (*res)[0].GetUInt32();
    }

    // ------------------------------------------------------------------
    //  查角色（返回 guid，并回填真实名字与所属账号）
    // ------------------------------------------------------------------
    static ObjectGuid LookupChar(std::string const& charName, uint32& outAccount, std::string& outName)
    {
        std::string safe = charName;
        CharacterDatabase.EscapeString(safe);

        QueryResult res = CharacterDatabase.PQuery(
            "SELECT guid, account, name FROM characters WHERE name = '{}'", safe);
        if (!res)
            return ObjectGuid::Empty;

        Field* f = res->Fetch();
        uint32 low  = f[0].GetUInt32();
        outAccount  = f[1].GetUInt32();
        outName     = f[2].GetString();

        // ObjectGuid 没有 (HighGuid, low) 这种构造函数，
        // 唯一的 explicit 构造是 ObjectGuid(uint64)（ObjectGuid.h:254）。
        // 官方统一用模板工厂（ObjectGuid.h:224）：
        //   TicketMgr.cpp:71  ObjectGuid::Create<HighGuid::Player>(fields[..].GetUInt32())
        return ObjectGuid::Create<HighGuid::Player>(low);
    }

    // ------------------------------------------------------------------
    //  下面是各子指令的分发，顺序不影响功能
    // ------------------------------------------------------------------
    static bool SpawnBot(ChatHandler* handler, std::string const& accName, std::string const& charName)
    {
        char buf[512];

        // ---- 1. 查账号 ----
        uint32 accountId = LookupAccountId(accName);
        if (!accountId)
        {
            snprintf(buf, sizeof(buf), "|cffff0000 找不到账号 %s|r", accName.c_str());
            handler->SendSysMessage(buf);
            handler->SendSysMessage("|cffffff00 先建：.account create <名> <密码>|r");
            return false;
        }

        // ---- 2. 查角色 ----
        uint32 charAccount = 0;
        std::string realName;
        ObjectGuid charGuid = LookupChar(charName, charAccount, realName);
        if (!charGuid)
        {
            snprintf(buf, sizeof(buf), "|cffff0000 找不到角色 %s|r", charName.c_str());
            handler->SendSysMessage(buf);
            return false;
        }

        // ---- 3. 角色必须属于该账号 ----
        if (charAccount != accountId)
        {
            snprintf(buf, sizeof(buf),
                     "|cffff0000 角色 %s 不属于账号 %s（它属于账号ID %u）|r",
                     realName.c_str(), accName.c_str(), charAccount);
            handler->SendSysMessage(buf);
            return false;
        }

        // ---- 4. 该角色不能已经在线 ----
        if (ObjectAccessor::FindPlayer(charGuid))
        {
            snprintf(buf, sizeof(buf), "|cffff0000 %s 已经在线了|r", realName.c_str());
            handler->SendSysMessage(buf);
            return false;
        }

        // ---- 5. 该账号不能已有会话 ----
        if (sWorld->FindSession(accountId))
        {
            snprintf(buf, sizeof(buf),
                     "|cffff0000 账号 %s 已有活动会话，先 .pbot despawn|r", accName.c_str());
            handler->SendSysMessage(buf);
            return false;
        }

        // ---- 6. 造无连接会话 ----
        // WorldSession.h:495
        //   WorldSession(uint32 id, std::string&& name, std::shared_ptr<WorldSocket> sock,
        //                AccountTypes sec, uint8 expansion, time_t mute_time,
        //                Minutes timezoneOffset, LocaleConstant locale,
        //                uint32 recruiter, bool isARecruiter);
        //
        // sock 传 nullptr —— 这是整个方案的基础。
        // sec 用 SEC_ADMINISTRATOR：让 HasPermission(RBAC_PERM_IGNORE_IDLE_CONNECTION)
        // 为真，从而绕开 WorldSession.cpp:298 那个没判空的 m_Socket->CloseSocket()。
        std::string sessName = accName;
        WorldSession* sess = new WorldSession(
            accountId,
            std::move(sessName),
            nullptr,                    // <- 无 socket
            SEC_ADMINISTRATOR,          // <- 门3 解法
            2,                          // expansion: 2 = WotLK
            0,                          // mute_time
            Minutes(0),                 // timezoneOffset
            LOCALE_zhCN,
            0,                          // recruiter
            false);                     // isARecruiter

        // ---- 6.5 标记为 bot 会话 ----
        // 【关键】WorldSession::Update 的取包循环是 while (m_Socket && ...)（cpp:312），
        // 无 socket 会导致投递的登录包【永远不被处理】。
        // 且 cpp:512 的 if (!m_Socket) return false; 会把会话踢出世界。
        // 这两处都要靠这个标记放行，见"修复-登录不进世界.md"。
        sess->SetBotSession(true);

        // ---- 7. 门2：把角色加进白名单 ----
        // 需要先给 WorldSession 加 public 方法 AddLegitCharacter()
        // 【注意】必须加在 public 段（577行 QueuePacket 附近），
        // 不能加在 IsLegitCharacterForAccount(1285) 旁边 —— 那里是 private 段
        sess->AddLegitCharacter(charGuid);

        // ---- 8. 注册进世界 ----
        sWorld->AddSession(sess);

        // ---- 9. 门1：伪造 CMSG_PLAYER_LOGIN，走官方登录流程 ----
        // CharacterHandler.cpp:698  recvData >> playerGuid;
        // ObjectGuid.cpp:136        operator<< 写的就是 uint64 原始值，格式对称
        WorldPacket* pkt = new WorldPacket(CMSG_PLAYER_LOGIN, 8);
        *pkt << charGuid;
        sess->QueuePacket(pkt);         // WorldSession.h:577  public

        // step42: 第4个字段是自动接受开关。
        // 写全而不是靠默认值 —— 让"新bot默认开哪些"这件事在代码里看得见。
        g_pbots.push_back({ accountId, charGuid, realName, PBOT_AUTO_DEFAULT });

        // A41: 登记名册，下次主人上线自动拉起
        if (Player* owner = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr)
        {
            PBotEntry ent{ accountId, charGuid, realName, PBOT_AUTO_DEFAULT };
            PBotRosterAdd(owner->GetGUID(), ent);
            handler->SendSysMessage("|cff00ff00 已加入自动上线名册|r");
        }

        snprintf(buf, sizeof(buf), "|cff00ff00 正在登录 %s ...|r", realName.c_str());
        handler->SendSysMessage(buf);
        handler->SendSysMessage("|cffffff00 登录是异步的，等1-3秒。用 .pbot list 看状态|r");
        return true;
    }

    // ------------------------------------------------------------------
    static bool HandlePBot(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = Tok(args);
        if (tok.empty())
        {
            SendHelp(handler);
            return true;
        }

        char buf[512];
        std::string s0 = Lower(tok[0]);

        // ---------- spawn ----------
        if (s0 == "spawn" || tok[0] == "上线" || tok[0] == "召出")
        {
            if (tok.size() < 3)
            {
                handler->SendSysMessage("|cffff0000 用法: .pbot spawn <账号名> <角色名>|r");
                handler->SendSysMessage("|cffffff00 例: .pbot spawn botcc1 Evirstara|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            return SpawnBot(handler, tok[1], tok[2]);
        }

        // ---------- list ----------
        if (s0 == "list" || tok[0] == "列表")
        {
            if (g_pbots.empty())
            {
                handler->SendSysMessage("|cffffff00 当前没有 PlayerBot|r");
                return true;
            }

            snprintf(buf, sizeof(buf), "|cff00ff00[PlayerBot 共 %u 个]|r", uint32(g_pbots.size()));
            handler->SendSysMessage(buf);

            for (PBotEntry const& e : g_pbots)
            {
                Player* p = ObjectAccessor::FindPlayer(e.CharGuid);
                if (p)
                {
                    // step42: 顺带显示自动接受开关，省得再敲一次 .pbot auto
                    snprintf(buf, sizeof(buf),
                             "  |cff00ff00%s|r  %u级  地图%u  |cff888888在线|r  自动:%s",
                             e.CharName.c_str(), uint32(p->GetLevel()), p->GetMapId(),
                             PBotFlagsToText(e.AutoFlags).c_str());
                }
                else
                {
                    snprintf(buf, sizeof(buf),
                             "  |cffff0000%s|r  |cffff0000未进入世界（还在加载或失败）|r",
                             e.CharName.c_str());
                }
                handler->SendSysMessage(buf);
            }
            return true;
        }

        // ---------- despawn ----------
        if (s0 == "despawn" || tok[0] == "下线")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("|cffff0000 用法: .pbot despawn <角色名>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            for (auto it = g_pbots.begin(); it != g_pbots.end(); ++it)
            {
                if (Lower(it->CharName) != Lower(tok[1]))
                    continue;

                if (WorldSession* s = sWorld->FindSession(it->AccountId))
                {
                    // 找到目标并执行传送
                    s->LogoutPlayer(true);
                    s->KickPlayer("pbot despawn");
                }

                snprintf(buf, sizeof(buf), "|cff00ff00 %s 已下线|r", it->CharName.c_str());
                handler->SendSysMessage(buf);
                // A41: 同步从名册移除
                if (Player* owner = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr)
                    PBotRosterRemove(owner->GetGUID(), it->CharGuid);
                g_pbots.erase(it);
                return true;
            }

            handler->SendSysMessage("|cffff0000 找不到这个bot|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ---------- come ----------
        if (s0 == "come" || tok[0] == "过来" || tok[0] == "召唤")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("|cffff0000 用法: .pbot come <角色名>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            for (PBotEntry const& e : g_pbots)
            {
                if (Lower(e.CharName) != Lower(tok[1]))
                    continue;

                Player* p = ObjectAccessor::FindPlayer(e.CharGuid);
                if (!p)
                {
                    handler->SendSysMessage("|cffff0000 它还没进世界|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }

                // ---- 照抄官方 .summon 的安全检查（cs_misc.cpp:517起）----
                // 原来只有一句 TeleportTo，跨地图/副本/战场时会出问题。

                if (p->IsBeingTeleported())      // Player.h:1802 public
                {
                    handler->SendSysMessage("|cffff0000 它正在传送中，稍等|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }

                Map* map = player->GetMap();

                if (map->IsBattlegroundOrArena() && !player->IsGameMaster())
                {
                    handler->SendSysMessage("|cffff0000 战场/竞技场里只有GM能召唤|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                else if (map->IsDungeon())
                {
                    // 你在副本里，而 bot 不在同一个副本实例
                    // 这种情况不能传送，否则会卡住
                    Map* targetMap = p->GetMap();
                    if (targetMap->GetId() != map->GetId() ||
                        targetMap->GetInstanceId() != map->GetInstanceId())
                    {
                        handler->SendSysMessage("|cffff0000 你在副本里，bot 不在同一个|r");
                        handler->SendSysMessage("|cffffff00 先让它跟你一起进副本|r");
                        handler->SetSentErrorMessage(true);
                        return false;
                    }
                }

                // 记住原始坐标，传送失败时好回滚
                if (player->IsInFlight())        // Unit.h:1120 public
                    player->FinishTaxiFlight();  // Player.h:964  public

                // ============================================================
                // 计算落点：玩家身前 PBOT_COME_DIST 码
                // ============================================================
                //  官方 .summon 用 GetClosePoint(x,y,z, reach)，
                //  relAngle 默认 0 -> 落在【玩家面朝方向】的正前方，
                //  距离 = distance2d(0) + size(reach) —— 贴得非常近，几乎重叠。
                //
                //  下面是各子指令的分发，顺序不影响功能
                //    distance2d = PBOT_COME_DIST(2码)，站前面一点，看得见
                //    GetFirstCollisionPosition 会做 raycast，
                //    保证不会卡进墙里/地板下（Object.cpp:3574 MovePositionToFirstCollision）
                bool sameMap = (p->GetMapId() == player->GetMapId());

                Position dest = player->GetFirstCollisionPosition(PBOT_COME_DIST, 0.0f);
                //                                                ^^^^^^^^^^^^^^  ^^^^
                //                        Object.h:370 public     距离           relAngle=0=正前方

                float x = dest.GetPositionX();
                float y = dest.GetPositionY();
                float z = dest.GetPositionZ();

                // 让 bot 面朝玩家，看起来像真的走过来打招呼
                // Position.h:128 GetAbsoluteAngle —— struct 默认 public
                float o = dest.GetAbsoluteAngle(player->GetPositionX(), player->GetPositionY());

                if (!p->TeleportTo(player->GetMapId(), x, y, z, o))
                {
                    handler->SendSysMessage("|cffff0000 传送失败，可能坐标不合法|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }

                // ============================================================
                //  【核心修复】把传送"落地"—— 这是 bot 和真人最大的区别
                // ============================================================
                //  真人玩家：TeleportTo 只是【发包】，客户端收到后回 ACK，
                //            服务端在 ACK 里才真正改坐标。
                //
                //  bot 没有客户端 -> 永远不会回 ACK -> 传送【永远停在半路】。
                //  这里用"先转队长再踢"的顺序，绕开队长不能被踢的限制
                //  bot 的坐标压根没被更新过，它还留在原地/上一次落点。
                //
                //  两条分支要分别收尾（Player.cpp:1557 TeleportTo 的 if/else）：
                //
                //  1) 同地图（近距离传送）Player.cpp:1660
                //       SetSemaphoreTeleportNear(IsMovedByClient());
                //       -> bot 会话在 WorldSession.cpp:157 也 new 了 GameClient，
                //          登录时 Player.cpp:22776 SetMovedUnit(this,true) 挂上，
                //          所以 IsMovedByClient() == true -> 信号量被置上 -> 卡住。
                //       正常路径是客户端回 MSG_MOVE_TELEPORT_ACK
                //          -> MovementHandler.cpp:356 HandleMoveTeleportAck
                //          -> UpdatePosition(dest, true)
                //       传送是异步的，不能立刻判断成功
                //
                //  2) 跨地图（远距离传送）Player.cpp:1793
                //       SetSemaphoreTeleportFar(true);
                //       正常路径是客户端回 MSG_MOVE_WORLDPORT_ACK
                //          -> HandleMoveWorldportAck()
                //       所以这里只发起，结果由客户端确认
                //          WorldSession.h:829  "// for server-side calls"
                //          WorldSession.cpp:524 / :589
                //             while (_player->IsBeingTeleportedFar())
                //                 HandleMoveWorldportAck();
                //       失败的话下一帧会自动重试

                if (p->IsBeingTeleportedFar())
                {
                    // 跨地图：官方同款收尾（WorldSession.cpp:589）
                    // 用 while 是因为 HandleMoveWorldportAck 内部失败时
                    // 会再次 TeleportTo 到 homebind，需要继续消化
                    uint8 guard = 0;
                    while (p->IsBeingTeleportedFar() && guard++ < 5)
                        p->GetSession()->HandleMoveWorldportAck();   // WorldSession.h:829 public(706段)
                }
                else if (p->IsBeingTeleportedNear())
                {
                    // 同地图：照抄 MovementHandler.cpp:356 HandleMoveTeleportAck 的收尾
                    p->SetSemaphoreTeleportNear(false);              // Player.h:1805 public(904段)

                    uint32 oldZone = p->GetZoneId();

                    WorldLocation const& tdest = p->GetTeleportDest();   // Player.h:1800 public
                    p->UpdatePosition(tdest, true);                      // Unit.h:1205 public(811段)
                    p->SetFallInformation(0, p->GetPositionZ());         // Player.h:2045 public

                    uint32 newZone, newArea;
                    p->GetZoneAndAreaId(newZone, newArea);               // Object.h:381 public(356段)
                    p->UpdateZone(newZone, newArea);                     // Player.h:1568 public

                    if (oldZone != newZone)
                    {
                        // 官方在这里处理 honorless target / PvP 状态
                        if (p->pvpInfo.IsHostile)                        // Player.h:1563 public
                            p->CastSpell(p, 2479, true);
                        else if (p->IsPvP() && !p->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP))
                            p->UpdatePvP(false, false);                  // Player.h:1567 public
                    }

                    p->ResummonPetTemporaryUnSummonedIfAny();            // Player.h:2111 public
                    p->ProcessDelayedOperations();                       // Player.h:1807 public
                }

                // 目标必须在世界里且不在传送中
                // Object.h:385 InSamePhase 是【位与】不是相等
                p->SetPhaseMask(player->GetPhaseMask(), true);         // Object.h:383 public

                // 取玩家当前位置作为传送目标
                p->StopMoving();                                       // Unit.h:1684 public(811段)

                snprintf(buf, sizeof(buf), "|cff00ff00 %s 已站到你面前|r（%s）",
                         e.CharName.c_str(), sameMap ? "同地图" : "跨地图");
                handler->SendSysMessage(buf);
                return true;
            }

            handler->SendSysMessage("|cffff0000 找不到这个bot|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ---------- goto ----------
        //  和 come 相反：【你】过去找它。
        //  这条安全得多 —— 移动的是【真人玩家】，有真实客户端会回 ACK，
        //  走的是完全原生的传送路径，不需要任何补 ACK 的特殊处理。
        //
        //  【教训来源】step35 的坑：`.bf come` 移动被观察者，
        //  制造了原本不存在的 bug，花了4轮排查自己造的问题。
        //  移动观察者（goto）比移动被观察者（come）安全。
        if (s0 == "goto" || tok[0] == "去找" || tok[0] == "前往")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("|cffff0000 用法: .pbot goto <角色名>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            for (PBotEntry const& e : g_pbots)
            {
                if (Lower(e.CharName) != Lower(tok[1]))
                    continue;

                Player* p = ObjectAccessor::FindPlayer(e.CharGuid);
                if (!p)
                {
                    handler->SendSysMessage("|cffff0000 它还没进世界|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }

                if (p->IsBeingTeleported())
                {
                    handler->SendSysMessage("|cffff0000 它正在传送中，稍等|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }

                // 找到目标并执行传送
                if (player->IsInFlight())        // Unit.h:1120 public
                    player->FinishTaxiFlight();  // Player.h:964  public
                else
                    player->SaveRecallPosition(); // 存个回程点，跟官方 .appear 一样

                // 记录日志
                Position dest = p->GetFirstCollisionPosition(PBOT_COME_DIST, 0.0f);
                float o = dest.GetAbsoluteAngle(p->GetPositionX(), p->GetPositionY());

                player->TeleportTo(p->GetMapId(),
                                   dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ(), o);

                // 已经在线
                player->SetPhaseMask(p->GetPhaseMask(), true);

                snprintf(buf, sizeof(buf), "|cff00ff00 正在前往 %s|r", e.CharName.c_str());
                handler->SendSysMessage(buf);
                return true;
            }

            handler->SendSysMessage("|cffff0000 找不到这个bot|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ---------- auto ----------
        //  .pbot auto                          看所有bot的开关状态
        //  .pbot auto <角色名>                 看某个bot的开关
        //  .pbot auto <角色名> <项目> <on/off> 单独开关某一项
        //  .pbot auto all <项目> <on/off>      一次改所有bot
        //
        //  项目：group / guild / duel / res / summon / all
        if (s0 == "auto" || tok[0] == "自动")
        {
            // ---- 只有 .pbot auto：列出所有 ----
            if (tok.size() < 2)
            {
                if (g_pbots.empty())
                {
                    handler->SendSysMessage("|cffffff00 还没有 PlayerBot|r");
                    return true;
                }

                handler->SendSysMessage("|cff00ff00[自动接受 开关状态]|r");
                snprintf(buf, sizeof(buf), "  总开关: %s",
                         g_pbotAutoMasterSwitch ? "|cff00ff00开|r" : "|cffff0000关|r");
                handler->SendSysMessage(buf);

                for (PBotEntry const& e : g_pbots)
                {
                    snprintf(buf, sizeof(buf), "  |cff00ff00%s|r : %s",
                             e.CharName.c_str(), PBotFlagsToText(e.AutoFlags).c_str());
                    handler->SendSysMessage(buf);
                }
                handler->SendSysMessage("|cffffff00 用法: .pbot auto <角色名|all> <项目> <on|off>|r");
                handler->SendSysMessage("|cffffff00 项目: group/guild/duel/res/summon/trade/release/all|r");
                return true;
            }

            // ---- .pbot auto master on/off：总开关 ----
            if (Lower(tok[1]) == "master" || tok[1] == "总开关")
            {
                if (tok.size() < 3)
                {
                    snprintf(buf, sizeof(buf), " 总开关当前: %s",
                         g_pbotAutoMasterSwitch ? "|cff00ff00开|r" : "|cffff0000关|r");
                    handler->SendSysMessage(buf);
                    return true;
                }
                std::string v = Lower(tok[2]);
                g_pbotAutoMasterSwitch = (v == "on" || v == "1" || tok[2] == "开");
                snprintf(buf, sizeof(buf), "|cff00ff00 总开关已%s|r",
                         g_pbotAutoMasterSwitch ? "打开" : "关闭");
                handler->SendSysMessage(buf);
                return true;
            }

            // ---- 查单个bot的开关状态 ----
            if (tok.size() < 3)
            {
                for (PBotEntry const& e : g_pbots)
                {
                    if (Lower(e.CharName) != Lower(tok[1]))
                        continue;
                    snprintf(buf, sizeof(buf), "|cff00ff00%s|r 的自动接受: %s",
                             e.CharName.c_str(), PBotFlagsToText(e.AutoFlags).c_str());
                    handler->SendSysMessage(buf);
                    return true;
                }
                    handler->SendSysMessage("|cffff0000 找不到这个bot|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            // ---- 改开关 ----
            std::string what = Lower(tok[2]);
            uint32 mask = 0;
            if (what == "group"  || tok[2] == "组队")  mask = PBOT_AUTO_GROUP;
            else if (what == "guild"  || tok[2] == "公会")  mask = PBOT_AUTO_GUILD;
            else if (what == "duel"   || tok[2] == "决斗")  mask = PBOT_AUTO_DUEL;
            else if (what == "res"    || what == "resurrect" || tok[2] == "复活")
                                                            mask = PBOT_AUTO_RESURRECT;
            else if (what == "summon" || tok[2] == "召唤")  mask = PBOT_AUTO_SUMMON;
            else if (what == "trade"  || tok[2] == "交易")  mask = PBOT_AUTO_TRADE;
            else if (what == "release"|| tok[2] == "释放")  mask = PBOT_AUTO_RELEASE;
            else if (what == "all"    || tok[2] == "全部")  mask = PBOT_AUTO_ALL;
            else
            {
                handler->SendSysMessage(
                    "|cffff0000 项目只能是: group/guild/duel/res/summon/trade/release/all|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            // 支持 on/off 两种写法，大小写不敏感
            if ((mask & PBOT_AUTO_TRADE) && tok.size() >= 4 &&
                Lower(tok[3]) != "off" && tok[3] != "关" && tok[3] != "0")
            {
                handler->SendSysMessage(
                    "|cffffff00 提示: 交易只会自动【开窗口】，最终确认仍需你自己点。|r");
            }

            // ---- 解析 on/off（不给就当 on）----
            bool turnOn = true;
            if (tok.size() >= 4)
            {
                std::string v = Lower(tok[3]);
                turnOn = !(v == "off" || v == "0" || tok[3] == "关");
            }

            // ---- all：改所有bot ----
            bool applyAll = (Lower(tok[1]) == "all" || tok[1] == "全部");
            uint32 changed = 0;

            for (PBotEntry& e : g_pbots)
            {
                if (!applyAll && Lower(e.CharName) != Lower(tok[1]))
                    continue;

                if (turnOn)
                    e.AutoFlags |= mask;
                else
                    e.AutoFlags &= ~mask;

                ++changed;

                if (!applyAll)
                {
                snprintf(buf, sizeof(buf), "|cff00ff00%s|r 已设: %s",
                             e.CharName.c_str(), PBotFlagsToText(e.AutoFlags).c_str());
                    handler->SendSysMessage(buf);
                    return true;
                }
            }

            if (!changed)
            {
                handler->SendSysMessage("|cffff0000 找不到这个bot|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            snprintf(buf, sizeof(buf), "|cff00ff00 已%s %u 个bot的[%s]|r",
                     turnOn ? "开启" : "关闭", changed, tok[2].c_str());
            handler->SendSysMessage(buf);
            return true;
        }

        // ---------- accept ----------
        //  找不到bot时给出明确提示，不要静默失败
        //  下面是队伍与移动相关的子指令
        if (s0 == "accept" || tok[0] == "接受")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("|cffff0000 用法: .pbot accept <角色名|all>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            bool applyAll = (Lower(tok[1]) == "all" || tok[1] == "全部");
            uint32 done = 0;

            for (PBotEntry const& e : g_pbots)
            {
                if (!applyAll && Lower(e.CharName) != Lower(tok[1]))
                    continue;

                Player* p = ObjectAccessor::FindPlayer(e.CharGuid);
                if (!p || !p->IsInWorld() || !p->GetSession())
                    continue;

                // 逐个应用，统计成功数
                PBotForceAcceptAll(p);
                ++done;

                if (!applyAll)
                {
                    snprintf(buf, sizeof(buf), "|cff00ff00 %s 已处理挂起的邀请|r",
                             e.CharName.c_str());
                    handler->SendSysMessage(buf);
                    return true;
                }
            }

            if (!done)
            {
                handler->SendSysMessage("|cffff0000 找不到这个bot|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            snprintf(buf, sizeof(buf), "|cff00ff00 已处理 %u 个bot的挂起邀请|r", done);
            handler->SendSysMessage(buf);
            return true;
        }

                // ---------- kick 移出队伍 ----------
        if (s0 == "kick" || tok[0] == "踢出" || tok[0] == "移出")
        {
            Group* grp = player->GetGroup();
            if (!grp)
            {
                handler->SendSysMessage("|cffff0000 你不在队伍里|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            std::vector<ObjectGuid> toKick;
            for (PBotEntry const& e : g_pbots)
            {
                if (!grp->IsMember(e.CharGuid))
                    continue;
                if (tok.size() >= 2 && Lower(e.CharName) != Lower(tok[1]))
                    continue;
                toKick.push_back(e.CharGuid);
            }

            if (toKick.empty())
            {
                handler->SendSysMessage("|cffff0000 队伍里没有你的 pbot|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            uint32 kicked = 0;
            for (ObjectGuid kguid : toKick)
            {
                if (grp->IsLeader(kguid))
                {
                    grp->ChangeLeader(player->GetGUID());
                    handler->SendSysMessage("|cffffff00 bot 是队长，已把队长转给你|r");
                }
                Player::RemoveFromGroup(grp, kguid, GROUP_REMOVEMETHOD_KICK, player->GetGUID(), "pbot kick");
                ++kicked;
            }

            snprintf(buf, sizeof(buf), "|cff00ff00 已把 %u 个 pbot 移出队伍|r", kicked);
            handler->SendSysMessage(buf);
            return true;
        }

        // ---------- invite 拉进队伍 ----------
        if (s0 == "invite" || tok[0] == "邀请" || tok[0] == "拉人")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("|cffff0000 用法: .pbot invite <角色名|all>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            bool wantAll = (Lower(tok[1]) == "all" || tok[1] == "全部");
            uint32 added = 0;
            uint32 failed = 0;

            for (PBotEntry const& e : g_pbots)
            {
                if (!wantAll && Lower(e.CharName) != Lower(tok[1]))
                    continue;

                Player* p = ObjectAccessor::FindPlayer(e.CharGuid);
                if (!p || !p->IsInWorld())
                {
                    ++failed;
                    continue;
                }
                if (p->GetGroup())
                {
                    ++failed;
                    continue;
                }

                Group* grp = player->GetGroup();
                if (!grp)
                {
                    grp = new Group();
                    if (!grp->Create(player))
                    {
                        delete grp;
                handler->SendSysMessage("|cffff0000 你不在队伍里|r");
                        handler->SetSentErrorMessage(true);
                        return false;
                    }
                    sGroupMgr->AddGroup(grp);
                }

                if (grp->IsFull())
                {
                handler->SendSysMessage("|cffff0000 建队失败|r");
                    break;
                }

                if (grp->AddMember(p))
                    ++added;
                else
                    ++failed;
            }

            snprintf(buf, sizeof(buf), "|cff00ff00 已拉进 %u 个|r|cffff0000  失败 %u 个|r", added, failed);
            handler->SendSysMessage(buf);
            return true;
        }

        // ---------- follow 跟随 ----------
        if (s0 == "follow" || tok[0] == "跟随" || tok[0] == "跟着")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("|cffff0000 用法: .pbot follow <角色名|all>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            bool wantAll = (Lower(tok[1]) == "all" || tok[1] == "全部");
            uint32 cnt = 0;

            for (PBotEntry const& e : g_pbots)
            {
                if (!wantAll && Lower(e.CharName) != Lower(tok[1]))
                    continue;

                Player* p = ObjectAccessor::FindPlayer(e.CharGuid);
                if (!p || !p->IsInWorld())
                    continue;
                if (p->GetMapId() != player->GetMapId())
                    continue;

                p->GetMotionMaster()->Clear();
                p->GetMotionMaster()->MoveFollow(player, 2.0f, float(M_PI));
                ++cnt;
            }

            snprintf(buf, sizeof(buf), "|cff00ff00 %u 个 pbot 开始跟随|r", cnt);
            handler->SendSysMessage(buf);
            return true;
        }

        // ---------- stay 原地待命 ----------
        if (s0 == "stay" || tok[0] == "待命" || tok[0] == "原地")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("|cffff0000 用法: .pbot stay <角色名|all>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            bool wantAll = (Lower(tok[1]) == "all" || tok[1] == "全部");
            uint32 cnt = 0;

            for (PBotEntry const& e : g_pbots)
            {
                if (!wantAll && Lower(e.CharName) != Lower(tok[1]))
                    continue;

                Player* p = ObjectAccessor::FindPlayer(e.CharGuid);
                if (!p || !p->IsInWorld())
                    continue;

                p->GetMotionMaster()->Clear();
                p->GetMotionMaster()->MoveIdle();
                ++cnt;
            }

            snprintf(buf, sizeof(buf), "|cff00ff00 %u 个 pbot 原地待命|r", cnt);
            handler->SendSysMessage(buf);
            return true;
        }

        // ---------- give 让bot给你东西 ----------
        if (s0 == "give" || tok[0] == "给我" || tok[0] == "索取")
        {
            if (tok.size() < 3)
            {
                handler->SendSysMessage("|cffff0000 用法: .pbot give <角色名> <物品ID> [数量]|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            uint32 itemId = uint32(atoi(tok[2].c_str()));
            uint32 count = (tok.size() >= 4) ? uint32(atoi(tok[3].c_str())) : 1;
            if (!itemId || !count)
            {
                handler->SendSysMessage("|cffff0000 物品ID或数量不对|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            for (PBotEntry const& e : g_pbots)
            {
                if (Lower(e.CharName) != Lower(tok[1]))
                    continue;

                Player* p = ObjectAccessor::FindPlayer(e.CharGuid);
                if (!p || !p->IsInWorld())
                {
                    handler->SendSysMessage("|cffff0000 它还没进世界|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }

                if (!p->HasItemCount(itemId, count, true))
                {
                    snprintf(buf, sizeof(buf), "|cffff0000 %s 身上没有足够的这个物品|r", e.CharName.c_str());
                    handler->SendSysMessage(buf);
                    handler->SetSentErrorMessage(true);
                    return false;
                }

                ItemPosCountVec dest;
                InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count);
                if (msg != EQUIP_ERR_OK)
                {
                    handler->SendSysMessage("|cffff0000 你的背包放不下|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }

                p->DestroyItemCount(itemId, count, true);
                Item* newItem = player->StoreNewItem(dest, itemId, true);
                if (newItem)
                    player->SendNewItem(newItem, count, true, false);

                snprintf(buf, sizeof(buf), "|cff00ff00 %s 把物品给了你|r", e.CharName.c_str());
                handler->SendSysMessage(buf);
                return true;
            }

            handler->SendSysMessage("|cffff0000 找不到这个 pbot|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        SendHelp(handler);
        return true;
    }
};

void AddSC_playerbot_commandscript()
{
    new playerbot_commandscript();
}

/* ============================================================================
 *  API 访问段与行号见各补丁文档
 * ============================================================================
 *  WorldSession.h:495   构造函数（public 494段）
 *  WorldSession.h:503   PlayerDisconnected()             public
 *  WorldSession.h:577   void QueuePacket(WorldPacket*)   public(494段)
 *  WorldSession.h:1285  IsLegitCharacterForAccount()  【private!】(1274段)
 *                       -> AddLegitCharacter() 必须加在 public 段（577行附近），
 *                          加在1285旁边会 C2248。见安装说明
 *  WorldSession.cpp:211 SendPacket: if (!m_Socket) return;   <- 关键
 *  WorldSession.cpp:298 m_Socket->CloseSocket();             <- 门3崩溃点
 *  WorldSession.cpp:764 IsConnectionIdle()
 *  World.h:582          void AddSession(WorldSession* s)
 *  Opcodes.h:90         CMSG_PLAYER_LOGIN = 0x03D
 *  ObjectGuid.cpp:136   operator<<(ByteBuffer&, ObjectGuid const&) 写 uint64
 *  CharacterHandler.cpp:698  recvData >> playerGuid;   （与上面对称）
 *  CharacterHandler.cpp:58   class LoginQueryHolder    <- 只在cpp里，门1
 *  Common.h:38/43       enum AccountTypes / SEC_ADMINISTRATOR = 3
 *  Duration.h:30        typedef std::chrono::minutes Minutes;
 * ============================================================================
 */
