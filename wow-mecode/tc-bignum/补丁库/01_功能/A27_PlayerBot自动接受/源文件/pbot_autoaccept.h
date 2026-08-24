/*
 * ============================================================================
 *  step42  PlayerBot 自动接受 —— 共享状态头文件
 * ============================================================================
 *
 *  给 cs_playerbot.cpp（指令层）和 pbot_autoaccept.cpp（自动层）共用。
 *
 *  【设计原则】
 *    指令层和自动层【共用同一份开关】，所以 .pbot auto 改的就是
 *    自动层每帧读的那个值，不会出现"指令说开了但实际没开"。
 * ============================================================================
 */

#ifndef _PBOT_AUTOACCEPT_H
#define _PBOT_AUTOACCEPT_H

#include "Define.h"
#include "ObjectGuid.h"

#include <string>
#include <vector>

// ============================================================================
//  自动接受的开关位（可按位组合）
// ============================================================================
enum PBotAutoFlags : uint32
{
    PBOT_AUTO_NONE      = 0x00,
    PBOT_AUTO_GROUP     = 0x01,     // 组队邀请
    PBOT_AUTO_GUILD     = 0x02,     // 公会邀请
    PBOT_AUTO_DUEL      = 0x04,     // 决斗请求
    PBOT_AUTO_RESURRECT = 0x08,     // 复活请求
    PBOT_AUTO_SUMMON    = 0x10,     // 召唤确认（术士仪式/传送门）
    PBOT_AUTO_TRADE     = 0x20,     // 交易请求（只开窗口，【不】自动点确认）
    PBOT_AUTO_RELEASE   = 0x40,     // 死后自动释放尸体变鬼魂

    // 默认：组队+公会+复活+召唤+释放。
    // 【决斗默认关】—— 不然你想切磋一下它就自动应战，反而没了仪式感；
    // 而且 step43 对手系统要用决斗做"棋逢对手"的戏，自动接受会抢戏。
    // 【交易默认关】—— 见 pbot_autoaccept.cpp 里 TryAcceptTrade 的大段说明，
    // 这是【唯一一个能让你丢东西】的功能，必须由你显式打开。
    PBOT_AUTO_DEFAULT   = PBOT_AUTO_GROUP | PBOT_AUTO_GUILD |
                          PBOT_AUTO_RESURRECT | PBOT_AUTO_SUMMON |
                          PBOT_AUTO_RELEASE,

    PBOT_AUTO_ALL       = PBOT_AUTO_GROUP | PBOT_AUTO_GUILD | PBOT_AUTO_DUEL |
                          PBOT_AUTO_RESURRECT | PBOT_AUTO_SUMMON |
                          PBOT_AUTO_TRADE | PBOT_AUTO_RELEASE
};

// ============================================================================
//  一个 PlayerBot 的登记项
// ============================================================================
struct PBotEntry
{
    uint32      AccountId;
    ObjectGuid  CharGuid;
    std::string CharName;
    uint32      AutoFlags = PBOT_AUTO_DEFAULT;   // 每个bot独立开关
};

// ============================================================================
//  全局bot表（定义在 cs_playerbot.cpp）
// ============================================================================
extern std::vector<PBotEntry> g_pbots;

// 全局总开关：关掉后所有bot都不自动接受（应急用）
extern bool g_pbotAutoMasterSwitch;

// 工具：把开关位翻译成中文，供 .pbot auto 显示
std::string PBotFlagsToText(uint32 flags);

// 立刻处理一个bot身上所有挂起的邀请（无视开关）。
// 供 .pbot accept 调用 —— 不想等下一次轮询时用。
class Player;
void PBotForceAcceptAll(Player* bot);

#endif // _PBOT_AUTOACCEPT_H
