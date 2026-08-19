/*
 * ============================================================================
 *  step42  PlayerBot 自动接受
 *          组队 / 公会 / 决斗 / 复活 / 召唤 / 交易 / 死后释放
 *
 *  【step56 修订记录 —— 重要】
 *    step53 我误判"交易不需要bot配合"，把 TryAcceptTrade 注释掉了。
 *    那是错的。TradeHandler.cpp:729 的 BEGIN_TRADE 收包方是 pOther(=bot)，
 *    不是发起交易的玩家。bot 收不到 -> 走不到第二步 -> 玩家窗口永远不开。
 *    step56 已恢复，并补上距离自愈。详见 TryAcceptTrade 里的大段注释。
 * ============================================================================
 *
 *  用户需求：「让playerbot不用指令就可以接受组队邀请还有接受工会邀请等
 *              快捷功能，但是也要有指令操作」
 *
 *  ==========================================================================
 *  【核心思路】轮询状态，而不是拦截网络包
 *  ==========================================================================
 *
 *  一开始我想的是"钩住邀请事件"，但实查发现有更干净的路：
 *  邀请到达时，服务端会在【被邀请者身上留下一个可读的状态】，
 *  我们只要每隔一段时间看一眼这个状态就行。
 *
 *    组队  Group.cpp:401  AddInvite() -> player->SetGroupInvite(this)
 *          -> Player.h:2153  GetGroupInvite()      非空 = 有邀请挂着
 *
 *    公会  Guild.cpp:1515  pInvitee->SetGuildIdInvited(m_id)
 *          -> Player.h:1604  GetGuildIdInvited()   非零 = 有邀请挂着
 *
 *    决斗  Player.h:1583   duel（unique_ptr<DuelInfo>）
 *          -> DuelHandler.cpp:28 的判断条件就是现成的模板
 *
 *    复活  Player.h:1546   IsResurrectRequested()
 *
 *    召唤  Player.h:923    SummonIfPossible(bool agree)
 *
 *  【这样做的三个好处】
 *    1. 完全不用改上游任何文件（不用改 GroupHandler/GuildHandler）
 *    2. 不管邀请是谁发的、走哪条路来的，都能接住
 *    3. 真人玩家完全不受影响（我们只遍历 g_pbots 里登记的bot）
 *
 *  ==========================================================================
 *  【为什么可以直接调 Handler 函数】
 *  ==========================================================================
 *
 *    GuildHandler.cpp:60
 *      void WorldSession::HandleGuildAcceptOpcode(AcceptGuildInvite& invite)
 *      官方把参数名【用注释包起来了】（写成 注释-invite-注释 的形式），
 *      这是 C++ 里表示"这个参数没用到"的惯例 -> 可以传任意值
 *
 *    GroupHandler.cpp:215
 *      void WorldSession::HandleGroupAcceptOpcode(WorldPacket& recvData)
 *      {
 *          recvData.read_skip<uint32>();     <- 只是跳过4字节，不读内容
 *
 *    -> 这两个都能安全地"空调用"。
 *
 *    但决斗【不行】：
 *    DuelHandler.cpp:31  recvPacket >> guid;  <- 真的要读 arbiter GUID
 *    -> 必须构造带内容的包。见下面 TryAcceptDuel()。
 *
 *  ==========================================================================
 *  【发包安全性已验证】
 *  ==========================================================================
 *    Player.cpp:6288  SendDirectMessage -> m_session->SendPacket(data)
 *    WorldSession.cpp:211  SendPacket { if (!m_Socket) return; }
 *    -> bot 无 socket，包被安全丢弃，不会崩。
 *
 *  【性能】ScriptMgr.h:256 注释明确写着 "don't execute too heavy code here"，
 *          所以这里做了节流：默认每 1 秒才扫一次，且只遍历 g_pbots（通常个位数）。
 * ============================================================================
 */

#include "pbot_autoaccept.h"

#include "ScriptMgr.h"
#include "Chat.h"
#include "Group.h"              // Group::IsFull/IsCreated/GetLeaderGUID (Group.h:222/227/228)
#include "GroupMgr.h"           // sGroupMgr (GroupMgr.h:58 #define sGroupMgr)
                                //【教训】GroupHandler.cpp 也是 Group.h + GroupMgr.h 一起 include
#include "Guild.h"              // Guild::HandleAcceptMember (Guild.h:661 public 627段)
#include "GuildMgr.h"           // sGuildMgr->GetGuildById (GuildMgr.h:41)
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Opcodes.h"            // CMSG_DUEL_ACCEPTED = 0x16C (Opcodes.h:393)
#include "Player.h"             // 已含 Object.h -> UpdateFields.h (PLAYER_DUEL_ARBITER)
#include "ObjectDefines.h"      // step56: TRADE_DISTANCE = 11.11f (ObjectDefines.h:27)
#include "TradeData.h"          // TradeData::GetTrader (TradeData.h:41 public 36段)
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <set>

// ============================================================================
//  全局状态（在这里定义，cs_playerbot.cpp 用 extern 引用）
// ============================================================================
bool g_pbotAutoMasterSwitch = true;

// 记录"已经给谁开过交易窗口"，避免每秒重复发包刷屏。
// 交易一结束（GetTradeData() 变空）就把记录清掉，见 OnUpdate 里的清理。
static std::set<ObjectGuid> _pbotTradeOpened;

// ============================================================================
//  开关位 -> 中文（供 .pbot auto 显示）
// ============================================================================
std::string PBotFlagsToText(uint32 flags)
{
    if (flags == PBOT_AUTO_NONE)
        return "全部关闭";

    std::string s;
    if (flags & PBOT_AUTO_GROUP)     s += "组队 ";
    if (flags & PBOT_AUTO_GUILD)     s += "公会 ";
    if (flags & PBOT_AUTO_DUEL)      s += "决斗 ";
    if (flags & PBOT_AUTO_RESURRECT) s += "复活 ";
    if (flags & PBOT_AUTO_SUMMON)    s += "召唤 ";
    if (flags & PBOT_AUTO_TRADE)     s += "交易 ";
    if (flags & PBOT_AUTO_RELEASE)   s += "释放 ";

    if (!s.empty() && s.back() == ' ')
        s.pop_back();
    return s;
}

// ============================================================================
//  五个接受动作 —— 写成【文件级函数】而不是类成员
// ============================================================================
//  这样 .pbot accept（指令层，强制立即接受）和
//  WorldScript::OnUpdate（自动层，按开关轮询）能【共用同一份实现】，
//  不会出现"自动的和手动的行为不一致"。
// ============================================================================

// 前置声明：TryAcceptResurrect / TryAcceptSummon 都要用它，
// 但它定义在后面。C++ 要求先声明后使用。
static void FinishPendingTeleport(Player* bot);

// ------------------------------------------------------------------
//  组队邀请
// ------------------------------------------------------------------
//  照抄 GroupHandler.cpp:215 HandleGroupAcceptOpcode 的完整逻辑，
//  但【不走包】—— 直接调用，省掉伪造包的开销。
//
//  为什么不直接调 handler：
//    handler 里有 recvData.read_skip<uint32>()，要构造一个至少4字节的包。
//    与其造包，不如照抄逻辑（就十几行），还能少一次内存分配。
//    逻辑我是【逐行对照】抄的，见下面注释里的行号。
static void TryAcceptGroup(Player* bot)
{
    Group* group = bot->GetGroupInvite();       // Player.h:2153 public(904段)
    if (!group)
        return;

    // ---- 以下逐行对应 GroupHandler.cpp:219-268 ----

    group->RemoveInvite(bot);                   // Group.h:206 public(185段)

    // GroupHandler.cpp:228  不能接受自己发起的队伍
    if (group->GetLeaderGUID() == bot->GetGUID())   // Group.h:228 public
        return;

    // GroupHandler.cpp:235  队伍满了
    if (group->IsFull())                        // Group.h:222 public
        return;

    Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());

    // GroupHandler.cpp:247  队伍还没真正建立（第一个人加入时才建）
    if (!group->IsCreated())                    // Group.h:227 public
    {
        if (!leader)
        {
            group->RemoveAllInvites();
            return;
        }
        group->RemoveInvite(leader);
        group->Create(leader);                  // Group.h:192 public
        sGroupMgr->AddGroup(group);
    }

    // GroupHandler.cpp:265
    if (!group->AddMember(bot))                 // Group.h:209 public
        return;

    group->BroadcastGroupUpdate();              // Group.h:350 public

    // 给邀请人一个反馈，让他知道bot接了
    if (leader && leader->GetSession())
        ChatHandler(leader->GetSession()).PSendSysMessage(
            "|cff00ff00[%s]|r 接受了你的组队邀请。", bot->GetName().c_str());
}

// ------------------------------------------------------------------
//  公会邀请
// ------------------------------------------------------------------
//  这个最干净：GuildHandler.cpp:60 的参数被注释掉了，
//  函数体就三行，直接照抄。
static void TryAcceptGuild(Player* bot)
{
    // GuildHandler.cpp:63  已经有公会就不处理
    if (bot->GetGuildId())                      // Player.h:1601 public
        return;

    ObjectGuid::LowType invitedTo = bot->GetGuildIdInvited();   // Player.h:1604 public
    if (!invitedTo)
        return;

    Guild* guild = sGuildMgr->GetGuildById(invitedTo);          // GuildMgr.h:41
    if (!guild)
    {
        // 公会没了（比如解散了），把邀请状态清掉，免得一直卡着
        bot->SetGuildIdInvited(0);              // Player.h:1600 public
        return;
    }

    // GuildHandler.cpp:65
    guild->HandleAcceptMember(bot->GetSession());   // Guild.h:661 public(627段)
}

// ------------------------------------------------------------------
//  决斗请求
// ------------------------------------------------------------------
//  【和上面两个不一样】DuelHandler.cpp:31 会真的读包内容：
//      recvPacket >> guid;
//      if (target->GetGuidValue(PLAYER_DUEL_ARBITER) != guid) return;
//  所以必须构造一个【带 arbiter GUID】的包，然后走 handler。
//
//  这里选择走 handler 而不是照抄逻辑，因为决斗涉及倒计时状态机
//  （DUEL_STATE_CHALLENGED -> COUNTDOWN），照抄容易漏状态。
static void TryAcceptDuel(Player* bot)
{
    // 逐条对应 DuelHandler.cpp:28 的判断
    if (!bot->duel)                                     // Player.h:1583 public
        return;
    if (bot == bot->duel->Initiator)                    // 自己发起的不能自己接
        return;
    if (bot->duel->State != DUEL_STATE_CHALLENGED)      // Player.h:258
        return;

    Player* opponent = bot->duel->Opponent;
    if (!opponent)
        return;

    // DuelHandler.cpp:35  handler 会拿这个值做校验，我们必须给对
    ObjectGuid arbiter = opponent->GetGuidValue(PLAYER_DUEL_ARBITER);
    //                             Object.h:120 public(79段) / UpdateFields.h:177
    if (arbiter.IsEmpty())
        return;

    WorldPacket data(CMSG_DUEL_ACCEPTED, 8);            // Opcodes.h:393
    data << arbiter;
    bot->GetSession()->HandleDuelAcceptedOpcode(data);  // WorldSession.h:920 public(706段)
}

// ------------------------------------------------------------------
//  复活请求（有人给你放复活术）
// ------------------------------------------------------------------
static void TryAcceptResurrect(Player* bot)
{
    if (!bot->IsResurrectRequested())           // Player.h:1546 public
        return;

    // 活着的时候不该有复活请求，保险起见判一下
    if (bot->IsAlive())
        return;

    bot->ResurrectUsingRequestData();           // Player.h:1547 public

    // ============================================================
    //  【又一个 ACK 坑】复活【也会】传送，而且更隐蔽
    // ============================================================
    //  Player.cpp:24016  ResurrectUsingRequestData:
    //      TeleportTo(_resurrectionData->Location);   // 先传到尸体那儿
    //      if (IsBeingTeleported())
    //      {
    //          ScheduleDelayedOperation(DELAYED_RESURRECT_PLAYER);
    //          return;                 <- 【真正的复活被推迟了】
    //      }
    //      ResurrectUsingRequestDataImpl();
    //
    //  被推迟的动作要靠 ProcessDelayedOperations()（Player.cpp:1815）执行，
    //  而它只在【客户端回ACK】的路径上被调用（MovementHandler.cpp:397）。
    //
    //  -> bot 不补 ACK 的话：人没传过去，而且【永远复活不了】，
    //     变成一具卡在原地的鬼魂。
    //
    //  所以这里必须补完传送，补完之后 ProcessDelayedOperations 会把
    //  DELAYED_RESURRECT_PLAYER 消化掉，复活才真正生效。
    FinishPendingTeleport(bot);
}

// ------------------------------------------------------------------
//  把"挂起的传送"补完 —— bot 专用
// ------------------------------------------------------------------
//  抽出来复用：召唤和复活都会触发传送，两处逻辑完全一样。
//  写法照抄官方 WorldSession.cpp:524/589 与 MovementHandler.cpp:356。
static void FinishPendingTeleport(Player* bot)
{
    if (bot->IsBeingTeleportedFar())            // Player.h:1804 public
    {
        uint8 guard = 0;
        while (bot->IsBeingTeleportedFar() && guard++ < 5)
            bot->GetSession()->HandleMoveWorldportAck();   // WorldSession.h:829 public
    }
    else if (bot->IsBeingTeleportedNear())      // Player.h:1803 public
    {
        bot->SetSemaphoreTeleportNear(false);   // Player.h:1805 public
        bot->UpdatePosition(bot->GetTeleportDest(), true);  // Unit.h:1205 public
        bot->SetFallInformation(0, bot->GetPositionZ());    // Player.h:2045 public

        uint32 nz, na;
        bot->GetZoneAndAreaId(nz, na);          // Object.h:381 public
        bot->UpdateZone(nz, na);                // Player.h:1568 public

        // 【关键】这一步把 DELAYED_RESURRECT_PLAYER 等挂起动作消化掉
        bot->ProcessDelayedOperations();        // Player.h:1807 public
    }
}

// ------------------------------------------------------------------
//  召唤确认（术士仪式 / 会面石）
// ------------------------------------------------------------------
static void TryAcceptSummon(Player* bot)
{
    // Player.h:921  bool HasSummonPending() const   public(904段)
    //   { return m_summon_expire >= GameTime::GetGameTime(); }
    // 先查再调，而不是无脑空调用 —— 省掉每秒一次的无谓函数调用，
    // 也让下面的"传送落地"补救只在真正召唤时才跑。
    if (!bot->HasSummonPending())
        return;

    bot->SummonIfPossible(true);        // Player.h:923 public(904段)

    // ============================================================
    //  【接 step40 的坑】召唤内部会 TeleportTo，而 bot 没客户端
    // ============================================================
    //  Player.cpp:23650  SummonIfPossible 结尾是 TeleportTo(m_summon_location)
    //  但传送要等客户端回 ACK 才落地（Player.cpp:1660/1793），
    //  bot 永远不回 ACK -> 传送卡住 -> 人没过去。
    //
    //  这正是上一轮 .pbot come 踩的同一个坑。
    FinishPendingTeleport(bot);
}

// ------------------------------------------------------------------
//  交易请求
// ------------------------------------------------------------------
//  【这一条我做了保守设计，先说清楚为什么】
//
//  交易是【唯一一个能让你真的丢东西】的功能。
//  如果做成"全自动点确认"，会出现这种情况：
//      你往交易框里放了东西 -> bot 秒点确认 -> 东西没了
//  想撤都来不及。这不是"方便"，这是事故。
//
//  所以这里【只做一半】：
//      自动【打开】交易窗口（相当于bot点了"是"）
//      【不】自动点最终的"确认交易"
//
//  这样你能正常给它东西、从它那儿拿东西，
//  但最后那一下【永远由你决定】。
//
//  === 状态从哪来 ===
//  TradeHandler.cpp:118  发起交易时【双方的 m_trade 都被创建】：
//      _player->m_trade = new TradeData(_player, pOther);
//      pOther->m_trade  = new TradeData(pOther, _player);
//  -> bot 侧 GetTradeData()(Player.h:1190 public) 非空 = 有人在跟它交易
//
//  === 为什么能空调用 handler ===
//  TradeHandler.cpp  HandleBeginTradeOpcode(WorldPacket& recvPacket)
//  官方把参数名用注释包起来了 = 没用到。
//  函数体只是给双方发 TRADE_STATUS_OPEN_WINDOW。
static void TryAcceptTrade(Player* bot)
{
    TradeData* trade = bot->GetTradeData();     // Player.h:1190 public(904段)

    // 【状态清理放在函数最前面，而不是调用方】
    // 一开始我把清理写在 OnUpdate 里，但那样有两个漏洞：
    //   1. .pbot accept 走 PBotForceAcceptAll，不经过 OnUpdate -> 不清理
    //   2. bot despawn 后不在 g_pbots 里了 -> 记录永远残留
    // 放在这里，两个入口都能清到，逻辑只有一份。
    if (!trade)
    {
        _pbotTradeOpened.erase(bot->GetGUID());
        return;
    }

    Player* trader = trade->GetTrader();        // TradeData.h:41 public(36段)
    if (!trader)
        return;

    // ================================================================
    //  step56 新增A：距离自愈
    // ================================================================
    //  你走远了但交易还挂着 -> 你的 m_trade 不释放 -> 再点交易被
    //  TradeHandler.cpp:611 的 if (GetPlayer()->m_trade) return; 拦下
    //  -> 表现为"点交易没反应/说已在交易中"。
    //
    //  官方发起交易时会查距离(TradeHandler.cpp:704)，但【建立之后不再查】，
    //  靠的是客户端主动发 CMSG_CANCEL_TRADE。bot 没客户端，不会发。
    //  所以这里替它查。
    //
    //  ObjectDefines.h:27  #define TRADE_DISTANCE 11.11f
    //  Object.h:420        IsWithinDistInMap(...)  public(356段)
    //  Player.h:1191       TradeCancel(bool sendback, TradeStatus)  public(904段)
    if (!bot->IsWithinDistInMap(trader, TRADE_DISTANCE, false))
    {
        bot->TradeCancel(true);                 // 双方都会收到取消包，m_trade 被清
        _pbotTradeOpened.erase(bot->GetGUID());
        return;
    }

    // 已经开过窗口就别重复开 —— 否则每秒发一次包，刷屏且浪费
    if (_pbotTradeOpened.count(bot->GetGUID()))
        return;

    // 只跟真人开窗口。bot 之间自动交易没有意义，还可能互相刷包。
    if (trader->GetSession() && trader->GetSession()->PlayerDisconnected())
        return;

    _pbotTradeOpened.insert(bot->GetGUID());

    // ================================================================
    //  这一步就是"替 bot 点【接受交易】按钮"
    // ================================================================
    //  【step56 更正 step53 的错误判断】
    //
    //  真人之间的交易是两段式的：
    //    1) A 发起 -> 核心给【B】发 BEGIN_TRADE (TradeHandler.cpp:727-729)
    //                 注意收包的是 pOther，也就是 B
    //       -> B 的客户端弹出"A 想和你交易"
    //    2) B 点【接受】-> 客户端发 CMSG_BEGIN_TRADE
    //       -> HandleBeginTradeOpcode (TradeHandler.cpp:577)
    //       -> 给【双方】各发一次 OPEN_WINDOW (:584-586)
    //       -> 两边的交易窗口这才真正打开
    //
    //  bot 没有客户端，第 1 步的包被 WorldSession.cpp:211 丢弃，
    //  它永远走不到第 2 步 -> 【你的窗口也永远打不开】。
    //
    //  所以这里直接调 HandleBeginTradeOpcode，等价于 bot 点了"接受"。
    //  参数在官方实现里被注释掉了(WorldPacket& /*recvPacket*/)，可空调用。
    //
    //  WorldSession.h:924  HandleBeginTradeOpcode  public(706段)
    WorldPacket dummy(CMSG_BEGIN_TRADE, 0);
    bot->GetSession()->HandleBeginTradeOpcode(dummy);

    // 告诉发起人：窗口开了，但最后一下要他自己点
    if (trader->GetSession())
        ChatHandler(trader->GetSession()).PSendSysMessage(
            "|cff00ff00[%s]|r 接受了交易。|cffffff00（放好东西后需要你自己点确认）|r",
            bot->GetName().c_str());
}

// ------------------------------------------------------------------
//  死后自动释放尸体
// ------------------------------------------------------------------
//  不做这个的话，bot 死了就是一具躺在地上的尸体，永远不变鬼魂，
//  也就永远走不到墓地、等不到你去复活它。
//
//  Unit.h:1236  isDead() { return m_deathState == DEAD || m_deathState == CORPSE; }
//  Player.h:1743 BuildPlayerRepop()   public(904段)  变成鬼魂
//  Player.h:1744 RepopAtGraveyard()   public(904段)  送到最近墓地
static void TryAutoRelease(Player* bot)
{
    // JUST_DIED = 刚死还没处理；这时候释放最自然
    // CORPSE    = 已经是尸体状态但还没释放（比如上面那步没赶上）
    if (bot->getDeathState() != JUST_DIED &&    // Unit.h:1238 public(811段)
        bot->getDeathState() != CORPSE)
        return;

    // 已经是鬼魂了就别重复处理
    if (bot->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        return;

    // 【重要】有人正在给它放复活术时【不能】释放，
    // 否则会把别人的复活打断，还白白跑一趟墓地。
    if (bot->IsResurrectRequested())            // Player.h:1546 public
        return;

    bot->BuildPlayerRepop();                    // Player.h:1743 public
    bot->RepopAtGraveyard();                    // Player.h:1744 public

    // 送到墓地也是传送 —— 同样要补 ACK，否则卡在半路
    FinishPendingTeleport(bot);
}

// ============================================================================
//  强制接受（供 .pbot accept 调用）
// ============================================================================
//  和自动层【共用上面那五个函数】，所以行为完全一致，
//  区别只是：这里无视开关，全部试一遍。
void PBotForceAcceptAll(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !bot->GetSession())
        return;

    TryAcceptGroup(bot);
    TryAcceptGuild(bot);
    TryAcceptDuel(bot);
    TryAcceptResurrect(bot);
    TryAcceptSummon(bot);
    TryAcceptTrade(bot);        // step56: 恢复，理由同 OnUpdate 里那处
    TryAutoRelease(bot);
}

// ============================================================================
//  世界每帧回调 —— 自动层
// ============================================================================
class pbot_autoaccept_worldscript : public WorldScript
{
public:
    pbot_autoaccept_worldscript() : WorldScript("pbot_autoaccept_worldscript") { }

    void OnUpdate(uint32 diff) override
    {
        // ---- 节流：ScriptMgr.h:256 "don't execute too heavy code here" ----
        _timer += diff;
        if (_timer < CHECK_INTERVAL)
            return;
        _timer = 0;

        if (!g_pbotAutoMasterSwitch)
            return;
        if (g_pbots.empty())
            return;

        for (PBotEntry const& e : g_pbots)
        {
            if (e.AutoFlags == PBOT_AUTO_NONE)
                continue;

            Player* bot = ObjectAccessor::FindPlayer(e.CharGuid);
            if (!bot)
                continue;                   // 还没进世界 / 已下线

            if (!bot->IsInWorld())
                continue;

            // 会话没了就跳过（despawn 竞态保护）
            if (!bot->GetSession())
                continue;

            // 交易记录的清理已经放进 TryAcceptTrade 内部（函数最前面），
            // 那里两个入口都能覆盖到。这里不再重复。
            //
            // 但【交易开关被关掉】的情况要单独处理：
            // 关了开关就不会再进 TryAcceptTrade，残留记录得在这清。
            if (!(e.AutoFlags & PBOT_AUTO_TRADE))
                _pbotTradeOpened.erase(bot->GetGUID());

            if (e.AutoFlags & PBOT_AUTO_GROUP)      TryAcceptGroup(bot);
            if (e.AutoFlags & PBOT_AUTO_GUILD)      TryAcceptGuild(bot);
            if (e.AutoFlags & PBOT_AUTO_DUEL)       TryAcceptDuel(bot);
            if (e.AutoFlags & PBOT_AUTO_RESURRECT)  TryAcceptResurrect(bot);
            if (e.AutoFlags & PBOT_AUTO_SUMMON)     TryAcceptSummon(bot);
            // step56: 【恢复】step53 我判断反了，把这行注释掉是错的。
            //   TradeHandler.cpp:729  BEGIN_TRADE 是发给【pOther】(=bot) 的，
            //   不是发给玩家的。bot 没客户端 -> 包被丢 -> 玩家永远没窗口。
            //   真正打开双方窗口的是 HandleBeginTradeOpcode 发的 OPEN_WINDOW
            //   (TradeHandler.cpp:584-586，给 trader 和自己【各发一次】)。
            //   所以必须替 bot 点这一下"接受"。
            if (e.AutoFlags & PBOT_AUTO_TRADE)      TryAcceptTrade(bot);
            if (e.AutoFlags & PBOT_AUTO_RELEASE)    TryAutoRelease(bot);
        }
    }

private:
    // 1秒扫一次。邀请是低频事件，1秒的延迟感知不到，
    // 但比每帧扫（约每50ms一次）省 20 倍开销。
    static constexpr uint32 CHECK_INTERVAL = 1000;
    uint32 _timer = 0;
};

void AddSC_pbot_autoaccept()
{
    new pbot_autoaccept_worldscript();
}

/* ============================================================================
 *  API 核实记录（全部含访问段，逐条 grep 实查）
 * ============================================================================
 *  【组队】
 *  Player.h:2153   GetGroupInvite()              public(904段)
 *  Player.h:2154   SetGroupInvite()              public(904段)
 *  Group.h:192     Create(Player*)               public(185段)
 *  Group.h:206     RemoveInvite(Player*)         public(185段)
 *  Group.h:209     AddMember(Player*)            public(185段)
 *  Group.h:222     IsFull()                      public(185段)
 *  Group.h:227     IsCreated()                   public(185段)
 *  Group.h:228     GetLeaderGUID()               public(185段)
 *  Group.h:350     BroadcastGroupUpdate()        public(185段)
 *  Group.cpp:401   AddInvite -> SetGroupInvite   <- 状态从这来
 *  GroupHandler.cpp:215  HandleGroupAcceptOpcode <- 逻辑照抄自这里
 *
 *  【公会】
 *  Player.h:1600   SetGuildIdInvited()           public(904段)
 *  Player.h:1601   GetGuildId()                  public(904段)
 *  Player.h:1604   GetGuildIdInvited()           public(904段)
 *  Guild.h:661     HandleAcceptMember(WorldSession*)  public(627段)
 *  GuildMgr.h:41   GetGuildById()
 *  Guild.cpp:1515  SetGuildIdInvited(m_id)       <- 状态从这来
 *  GuildHandler.cpp:60   参数名被注释包起来 = 没用到
 *
 *  【决斗】
 *  Player.h:256    enum DuelState
 *  Player.h:258    DUEL_STATE_CHALLENGED
 *  Player.h:263    struct DuelInfo { Opponent / Initiator / State }
 *  Player.h:1583   duel（unique_ptr）            public(904段)
 *  Object.h:120    GetGuidValue()                public(79段)
 *  UpdateFields.h:177  PLAYER_DUEL_ARBITER
 *  Opcodes.h:393   CMSG_DUEL_ACCEPTED = 0x16C
 *  WorldSession.h:920  HandleDuelAcceptedOpcode  public(706段)
 *  DuelHandler.cpp:31  recvPacket >> guid  <- 【会读包】所以必须构造内容
 *
 *  【复活 / 召唤】
 *  Player.h:1546   IsResurrectRequested()        public(904段)
 *  Player.h:1547   ResurrectUsingRequestData()   public(904段)
 *  Player.h:923    SummonIfPossible(bool)        public(904段)
 *
 *  【挂点 / 安全】
 *  ScriptMgr.h:232 class WorldScript
 *  ScriptMgr.h:256 OnUpdate(uint32) "don't execute too heavy code here"
 *  World.cpp:2655  sScriptMgr->OnWorldUpdate(diff)   <- 每个世界tick
 *  Player.cpp:6288 SendDirectMessage -> m_session->SendPacket
 *  WorldSession.cpp:211  SendPacket { if (!m_Socket) return; }  <- 发包安全
 * ============================================================================
 */
