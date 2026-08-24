#include <set>
#include "tcstub.h"
#include "tcstub2.h"

std::vector<std::string> g_log;
ObjectGuid const ObjectGuid::Empty;
GroupMgrStub _gmgr;
GuildMgrStub _gumgr;

// ---- 模拟 pbot_autoaccept.h 的内容 ----
enum PBotAutoFlags : uint32 {
    PBOT_AUTO_NONE=0x00, PBOT_AUTO_GROUP=0x01, PBOT_AUTO_GUILD=0x02,
    PBOT_AUTO_DUEL=0x04, PBOT_AUTO_RESURRECT=0x08, PBOT_AUTO_SUMMON=0x10,
    PBOT_AUTO_TRADE=0x20, PBOT_AUTO_RELEASE=0x40,
    PBOT_AUTO_DEFAULT = PBOT_AUTO_GROUP|PBOT_AUTO_GUILD|PBOT_AUTO_RESURRECT|
                        PBOT_AUTO_SUMMON|PBOT_AUTO_RELEASE,
    PBOT_AUTO_ALL = PBOT_AUTO_GROUP|PBOT_AUTO_GUILD|PBOT_AUTO_DUEL|PBOT_AUTO_RESURRECT|
                    PBOT_AUTO_SUMMON|PBOT_AUTO_TRADE|PBOT_AUTO_RELEASE
};
struct PBotEntry { uint32 AccountId; ObjectGuid CharGuid; std::string CharName;
                   uint32 AutoFlags = PBOT_AUTO_DEFAULT; };
std::vector<PBotEntry> g_pbots;
bool g_pbotAutoMasterSwitch = true;
static std::set<ObjectGuid> _pbotTradeOpened;

#define CMSG_DUEL_ACCEPTED 0x16C

// 供 ObjectAccessor 查找
static std::vector<Player*> g_world;
namespace ObjectAccessor {
    Player* FindPlayer(ObjectGuid g){ for(auto p:g_world) if(p->guid==g) return p; return nullptr; }
}

// ======== 被测代码：直接从 pbot_autoaccept.cpp 提取 ========
#include "extracted.inc"

// ======== 测试 ========
static int pass=0,fail=0;
#define CHECK(c,n) do{ if(c){++pass;} else {++fail;printf("  [FAIL] %s\n",n);} }while(0)
static bool has(char const* s){for(auto&l:g_log) if(l==s) return true; return false;}
static void reset(){ g_log.clear(); g_world.clear(); g_pbots.clear();
                     g_pbotAutoMasterSwitch=true; _gumgr.g=nullptr;
                     _pbotTradeOpened.clear(); }   // 桩bot共用GUID 0，必须清

int main(){
    printf("=== step42 自动接受 测试 ===\n\n");

    // T1 组队：正常接受
    { reset(); Player leader,bot; leader.guid=ObjectGuid(1); bot.guid=ObjectGuid(2);
      Group g; g.leader=leader.guid; g.created=true;
      bot.SetGroupInvite(&g); g.invitees.push_back(&bot);
      g_world={&leader,&bot};
      TryAcceptGroup(&bot);
      CHECK(has("AddMember"),"T1 加入队伍");
      CHECK(has("BroadcastGroupUpdate"),"T1 广播更新");
      CHECK(has("RemoveInvite"),"T1 移除邀请"); }

    // T2 组队：没有邀请时什么都不做
    { reset(); Player bot; TryAcceptGroup(&bot);
      CHECK(!has("AddMember"),"T2 无邀请不加入"); }

    // T3 组队：不能接受自己发起的
    { reset(); Player bot; bot.guid=ObjectGuid(5);
      Group g; g.leader=bot.guid; g.created=true; bot.SetGroupInvite(&g);
      g_world={&bot}; TryAcceptGroup(&bot);
      CHECK(!has("AddMember"),"T3 拒绝自己的队伍"); }

    // T4 组队：满员
    { reset(); Player l,bot; l.guid=ObjectGuid(1); bot.guid=ObjectGuid(2);
      Group g; g.leader=l.guid; g.created=true; g.full=true; bot.SetGroupInvite(&g);
      g_world={&l,&bot}; TryAcceptGroup(&bot);
      CHECK(!has("AddMember"),"T4 满员不加入"); }

    // T5 组队：新队伍要先Create
    { reset(); Player l,bot; l.guid=ObjectGuid(1); bot.guid=ObjectGuid(2);
      Group g; g.leader=l.guid; g.created=false; bot.SetGroupInvite(&g);
      g_world={&l,&bot}; TryAcceptGroup(&bot);
      CHECK(has("Create"),"T5 新队伍先Create");
      CHECK(has("AddGroup"),"T5 注册到GroupMgr");
      CHECK(has("AddMember"),"T5 然后加入"); }

    // T6 组队：队长离线且队伍未建立 -> 清空邀请
    { reset(); Player bot; bot.guid=ObjectGuid(2);
      Group g; g.leader=ObjectGuid(99); g.created=false; bot.SetGroupInvite(&g);
      g_world={&bot}; TryAcceptGroup(&bot);
      CHECK(has("RemoveAllInvites"),"T6 队长离线清邀请");
      CHECK(!has("AddMember"),"T6 不加入"); }

    // T7 公会：正常接受
    { reset(); Player bot; Guild gu; gu.id=7; _gumgr.g=&gu;
      bot.guildIdInvited=7; TryAcceptGuild(&bot);
      CHECK(gu.acceptCalled,"T7 接受公会邀请"); }

    // T8 公会：已有公会不处理
    { reset(); Player bot; Guild gu; gu.id=7; _gumgr.g=&gu;
      bot.guildId=3; bot.guildIdInvited=7; TryAcceptGuild(&bot);
      CHECK(!gu.acceptCalled,"T8 已有公会不再加入"); }

    // T9 公会：公会已解散 -> 清理状态
    { reset(); Player bot; bot.guildIdInvited=99; _gumgr.g=nullptr;
      TryAcceptGuild(&bot);
      CHECK(bot.guildIdInvited==0,"T9 公会没了清空邀请状态"); }

    // T10 公会：无邀请
    { reset(); Player bot; Guild gu; gu.id=7; _gumgr.g=&gu;
      TryAcceptGuild(&bot); CHECK(!gu.acceptCalled,"T10 无邀请不动作"); }

    // T11 决斗：正常接受
    { reset(); Player bot,opp; bot.guid=ObjectGuid(1); opp.guid=ObjectGuid(2);
      opp.duelArbiter=ObjectGuid(555);
      bot.duel.reset(new DuelInfo(&opp,&opp,false));   // 对手发起
      opp.duel.reset(new DuelInfo(&bot,&opp,false));
      TryAcceptDuel(&bot);
      CHECK(has("DuelAccepted"),"T11 接受决斗");
      CHECK(bot.duel->State==DUEL_STATE_COUNTDOWN,"T11 进入倒计时"); }

    // T12 决斗：自己发起的不接
    { reset(); Player bot,opp; opp.duelArbiter=ObjectGuid(555);
      bot.duel.reset(new DuelInfo(&opp,&bot,false));   // 自己是发起人
      TryAcceptDuel(&bot);
      CHECK(!has("DuelAccepted"),"T12 不接受自己发起的决斗"); }

    // T13 决斗：arbiter为空要跳过（防止发错包）
    { reset(); Player bot,opp; opp.duelArbiter=ObjectGuid(0);
      bot.duel.reset(new DuelInfo(&opp,&opp,false));
      TryAcceptDuel(&bot);
      CHECK(!has("DuelAccepted"),"T13 arbiter为空时跳过"); }

    // T14 决斗：状态不对不接
    { reset(); Player bot,opp; opp.duelArbiter=ObjectGuid(555);
      bot.duel.reset(new DuelInfo(&opp,&opp,false));
      bot.duel->State=DUEL_STATE_IN_PROGRESS;
      TryAcceptDuel(&bot);
      CHECK(!has("DuelAccepted"),"T14 非CHALLENGED状态不接"); }

    // T15 【关键】复活：必须真的复活，不能卡成鬼魂
    { reset(); Player bot; bot.resReq=true; bot.alive=false;
      TryAcceptResurrect(&bot);
      CHECK(has("ResurrectDeferred"),"T15 复活先被推迟(还原真实行为)");
      CHECK(has("DelayedResurrectDone"),"T15 补ACK后延迟动作被执行");
      CHECK(bot.resurrected,"T15 【最关键】最终真的复活了");
      CHECK(!bot.IsBeingTeleportedNear(),"T15 传送信号量已清"); }

    // T16 【回归】不补ACK会怎样：证明这个坑真实存在
    { reset(); Player bot; bot.resReq=true; bot.alive=false;
      bot.ResurrectUsingRequestData();     // 只调官方函数，不补ACK
      CHECK(!bot.resurrected,"T16 不补ACK则【永远复活不了】= 坑复现");
      CHECK(bot.IsBeingTeleportedNear(),"T16 卡在传送中"); }

    // T17 复活：活着的时候不处理
    { reset(); Player bot; bot.resReq=true; bot.alive=true;
      TryAcceptResurrect(&bot);
      CHECK(!has("ResurrectDeferred"),"T17 活着时不复活"); }

    // T18 召唤：接受并落地
    { reset(); Player bot; bot.summonPending=true;
      TryAcceptSummon(&bot);
      CHECK(bot.summoned,"T18 接受召唤");
      CHECK(has("WorldportAck"),"T18 跨地图传送已落地");
      CHECK(!bot.IsBeingTeleportedFar(),"T18 传送信号量已清"); }

    // T19 召唤：没有待处理召唤时不动作
    { reset(); Player bot; bot.summonPending=false;
      TryAcceptSummon(&bot);
      CHECK(!bot.summoned,"T19 无召唤不动作");
      CHECK(!has("WorldportAck"),"T19 不做多余的传送补救"); }

    // T20 死循环保护
    { reset(); Player bot; bot.farSem=true;
      uint8 guard=0; int loops=0;
      while(bot.IsBeingTeleportedFar()&&guard++<5){++loops;bot.farSem=true;}
      CHECK(loops==5,"T20 guard限制5次不死循环"); }

    // T21 PBotForceAcceptAll：一次处理全部
    { reset(); Player l,bot; l.guid=ObjectGuid(1); bot.guid=ObjectGuid(2);
      Group g; g.leader=l.guid; g.created=true; bot.SetGroupInvite(&g);
      Guild gu; gu.id=7; _gumgr.g=&gu; bot.guildIdInvited=7;
      g_world={&l,&bot};
      PBotForceAcceptAll(&bot);
      CHECK(has("AddMember"),"T21 强制接受-组队");
      CHECK(gu.acceptCalled,"T21 强制接受-公会"); }

    // T22 PBotForceAcceptAll：不在世界时安全返回
    { reset(); Player bot; bot.inWorld=false;
      PBotForceAcceptAll(&bot);
      CHECK(g_log.empty(),"T22 不在世界时不动作"); }

    // T23 开关位文本
    { CHECK(PBotFlagsToText(PBOT_AUTO_NONE)=="全部关闭","T23 空开关文本");
      CHECK(PBotFlagsToText(PBOT_AUTO_GROUP)=="组队","T23 单项文本");
      std::string all=PBotFlagsToText(PBOT_AUTO_ALL);
      CHECK(all.find("组队")!=std::string::npos&&all.find("召唤")!=std::string::npos,
            "T23 全开文本");
      CHECK(all.back()!=' ',"T23 末尾无多余空格"); }

    // T24 默认开关：决斗默认关闭
    { PBotEntry e;
      CHECK((e.AutoFlags&PBOT_AUTO_GROUP)!=0,"T24 默认开组队");
      CHECK((e.AutoFlags&PBOT_AUTO_GUILD)!=0,"T24 默认开公会");
      CHECK((e.AutoFlags&PBOT_AUTO_DUEL)==0,"T24 决斗默认【关】"); }

    // T25 开关位运算
    { uint32 f=PBOT_AUTO_DEFAULT; f|=PBOT_AUTO_DUEL;
      CHECK((f&PBOT_AUTO_DUEL)!=0,"T25 开启决斗");
      f&=~PBOT_AUTO_DUEL;
      CHECK((f&PBOT_AUTO_DUEL)==0,"T25 关闭决斗");
      CHECK((f&PBOT_AUTO_GROUP)!=0,"T25 不影响其他项"); }


    // ===== step42b 新增：交易 =====

    // T26 交易：自动开窗口
    { reset(); Player bot,trader; trader.sess.hasSocket=true; trader.name="RealPlayer";
      TradeData td(&bot,&trader); bot.tradeData=&td;
      TryAcceptTrade(&bot);
      CHECK(has("BeginTrade"),"T26 自动打开交易窗口");
      CHECK(has("Notify"),"T26 通知了发起人"); }

    // T27 交易：【关键】不会自动点最终确认
    { reset(); Player bot,trader; trader.sess.hasSocket=true;
      TradeData td(&bot,&trader); bot.tradeData=&td;
      TryAcceptTrade(&bot);
      CHECK(!td.IsAccepted(),"T27 【安全】不自动点确认，你的东西丢不了"); }

    // T28 交易：不重复开窗口（防每秒刷包）
    { reset(); Player bot,trader; trader.sess.hasSocket=true;
      TradeData td(&bot,&trader); bot.tradeData=&td;
      TryAcceptTrade(&bot); TryAcceptTrade(&bot); TryAcceptTrade(&bot);
      int n=0; for(auto&l:g_log) if(l=="BeginTrade") ++n;
      CHECK(n==1,"T28 只开一次窗口，不刷包"); }

    // T29 交易：没有交易时不动作
    { reset(); Player bot; TryAcceptTrade(&bot);
      CHECK(!has("BeginTrade"),"T29 无交易不动作"); }

    // T30 交易：对方是bot（无socket）时跳过
    { reset(); Player bot,other; other.sess.hasSocket=false;
      TradeData td(&bot,&other); bot.tradeData=&td;
      TryAcceptTrade(&bot);
      CHECK(!has("BeginTrade"),"T30 bot之间不自动交易"); }

    // ===== step42b 新增：自动释放尸体 =====

    // T31 释放：刚死时自动变鬼魂并去墓地
    { reset(); Player bot; bot.deathState=JUST_DIED; bot.alive=false;
      TryAutoRelease(&bot);
      CHECK(bot.repopBuilt,"T31 变成鬼魂");
      CHECK(bot.repopGraveyard,"T31 送到墓地");
      CHECK(has("WorldportAck"),"T31 墓地传送已落地");
      CHECK(!bot.IsBeingTeleportedFar(),"T31 没卡在传送中"); }

    // T32 释放：活着时不动作
    { reset(); Player bot; bot.deathState=ALIVE; bot.alive=true;
      TryAutoRelease(&bot);
      CHECK(!bot.repopBuilt,"T32 活着时不释放"); }

    // T33 释放：已是鬼魂不重复
    { reset(); Player bot; bot.deathState=CORPSE; bot.alive=false;
      bot.pflags|=PLAYER_FLAGS_GHOST;
      TryAutoRelease(&bot);
      CHECK(!bot.repopBuilt,"T33 已是鬼魂不重复处理"); }

    // T34 【重要】有人在复活它时不能释放
    { reset(); Player bot; bot.deathState=JUST_DIED; bot.alive=false;
      bot.resReq=true;
      TryAutoRelease(&bot);
      CHECK(!bot.repopBuilt,"T34 有复活请求时【不】释放，不打断别人"); }

    // T35 CORPSE 状态也能释放
    { reset(); Player bot; bot.deathState=CORPSE; bot.alive=false;
      TryAutoRelease(&bot);
      CHECK(bot.repopBuilt,"T35 CORPSE状态也能释放"); }

    // T36 新开关的文本
    { std::string t=PBotFlagsToText(PBOT_AUTO_TRADE|PBOT_AUTO_RELEASE);
      CHECK(t.find("交易")!=std::string::npos,"T36 交易文本");
      CHECK(t.find("释放")!=std::string::npos,"T36 释放文本"); }

    // T37 默认开关：交易关、释放开
    { PBotEntry e;
      CHECK((e.AutoFlags&PBOT_AUTO_TRADE)==0,"T37 交易默认【关】(安全)");
      CHECK((e.AutoFlags&PBOT_AUTO_RELEASE)!=0,"T37 释放默认开");
      CHECK((e.AutoFlags&PBOT_AUTO_DUEL)==0,"T37 决斗仍默认关"); }

    // T38 ForceAcceptAll 包含新功能
    { reset(); Player bot,trader; trader.sess.hasSocket=true;
      TradeData td(&bot,&trader); bot.tradeData=&td;
      bot.deathState=JUST_DIED; bot.alive=false;
      PBotForceAcceptAll(&bot);
      CHECK(has("BeginTrade"),"T38 强制接受含交易");
      CHECK(bot.repopBuilt,"T38 强制接受含释放"); }


    // T39 【回归】交易结束后记录要被清掉，否则第二次交易开不了窗口
    //  这条是真发现的bug：清理原本只写在OnUpdate里，
    //  .pbot accept 走 PBotForceAcceptAll 不经过那儿 -> 记录残留
    { reset(); Player bot,trader; trader.sess.hasSocket=true;
      TradeData td(&bot,&trader);
      bot.tradeData=&td;
      PBotForceAcceptAll(&bot);                 // 第一次交易（走强制入口）
      int n1=0; for(auto&l:g_log) if(l=="BeginTrade") ++n1;
      CHECK(n1==1,"T39 第一次开窗口");

      bot.tradeData=nullptr;                    // 交易结束
      PBotForceAcceptAll(&bot);                 // 触发清理（不经过OnUpdate）

      g_log.clear();
      bot.tradeData=&td;                        // 第二次交易
      PBotForceAcceptAll(&bot);
      int n2=0; for(auto&l:g_log) if(l=="BeginTrade") ++n2;
      CHECK(n2==1,"T39 【回归】第二次仍能开窗口(记录已清)"); }

    printf("\n=== 结果: %d/%d 通过 ===\n",pass,pass+fail);
    return fail?1:0;
}
