/*
 * ============================================================================
 *  剧情对白 —— cs_say.cpp   (step30)
 * ============================================================================
 *
 *   .say <文本>                   选中目标说话（白字）
 *   .say yell <文本>              喊话（红字，远距离）
 *   .say emote <文本>             第三人称描述（橙字）
 *   .say boss <文本>              全屏 BOSS 提示（需在 conf 开关打开）
 *   .say whisper <文本>           对自己密语
 *   .say r <半径> <文本>          周围所有 NPC 一起说
 *   .say entry <ID> <文本>        指定 entry 的 NPC 说
 *   .say me <文本>                自己说
 *   .say noemote <文本>           不自动配表情
 *   .say help                     帮助
 *
 *   修饰符可组合，顺序随意：
 *       .say yell r 40 大家快跑！
 *       .say noemote entry 1234 ...
 *
 * ----------------------------------------------------------------------------
 *  和 step29 .emote 的关系
 *
 *  .emote 让 NPC 有动作，.say 让 NPC 有台词。
 *  两者【自动联动】：说话时按末尾标点自动播对应的一次性表情，
 *  不用再单独敲一条 .emote。
 *
 *  这个思路来自官方 cs_npc.cpp:912-918：
 *      switch (text.back())
 *      {
 *          case '?':   HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);    break;
 *          case '!':   HandleEmoteCommand(EMOTE_ONESHOT_EXCLAMATION); break;
 *          default:    HandleEmoteCommand(EMOTE_ONESHOT_TALK);        break;
 *      }
 *
 *  但官方只判 ASCII。中文剧情用的是全角标点，是 UTF-8 多字节，
 *  用 text.back() 取【最后一个字节】会判错 —— 本实现按字节序列比对。
 *
 * ----------------------------------------------------------------------------
 *  已核实 API（全 public，Unit.h:811 起的 public 段，下一个 private 在 1669）
 *
 *   Unit.h:1823   virtual void Say(std::string_view, Language, WorldObject const* = nullptr)
 *   Unit.h:1824   virtual void Yell(std::string_view, Language, WorldObject const* = nullptr)
 *   Unit.h:1825   virtual void TextEmote(std::string_view, WorldObject const* = nullptr, bool isBossEmote = false)
 *   Unit.h:1826   virtual void Whisper(std::string_view, Language, Player*, bool isBossWhisper = false)
 *   Unit.h:1037   void HandleEmoteCommand(Emote emoteId)
 *   Chat.h:104    Creature* getSelectedCreature()
 *   Creature.h:394 bool IsNPCBotOrPet() const
 *   SharedDefines.h:829  LANG_UNIVERSAL = 0
 *
 *  实现差异（查了 Unit.cpp:14773 起，四个都是 Talk() 的薄封装）：
 *   Say        -> CHAT_MSG_MONSTER_SAY    + CONFIG_LISTEN_RANGE_SAY
 *   Yell       -> CHAT_MSG_MONSTER_YELL   + CONFIG_LISTEN_RANGE_YELL
 *   TextEmote  -> CHAT_MSG_MONSTER_EMOTE  + CONFIG_LISTEN_RANGE_TEXTEMOTE
 *   TextEmote(isBoss=true) -> CHAT_MSG_RAID_BOSS_EMOTE   <- 全屏，官方指令没暴露
 *
 * ----------------------------------------------------------------------------
 *  【设计决策】不做延时台词（用户已确认走 A 方案）
 *
 *  查证：GM 指令是一次性调用，执行完即返回，没有持续 Update 的宿主。
 *    TaskScheduler.h:225   存在，但需要宿主对象持续调 Update
 *    Creature.h            没有内置 TaskScheduler 成员
 *    events.ScheduleEvent  是 EventMap，只在 CreatureAI 内部可用
 *    World.h:819           m_timers 是固定用途 IntervalTimer，塞不了任务
 *
 *  唯一能做延时的办法是像 .dummy(step23) 那样 AIM_Initialize 注入 AI，
 *  但那会【顶掉 NPC 原有 AI】，副作用太大。
 *
 *  结论：单句即时说话。多句由 GM 手动导演，
 *  自动序列留给将来的 .timeline。
 *
 * ----------------------------------------------------------------------------
 *  注册语法：旧式 std::vector<ChatCommand>，与 .nst/.scene/.emote 一致
 *  权限：RBAC_PERM_COMMAND_WORLDTOOLS，不用动 RBAC 表
 *
 *  【重要】本仓库 DirectPExecute 走 fmt 库（DatabaseWorkerPool.h:99），
 *  占位符是 {} 不是 %u。本文件不写库，但留此提醒。
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "CreatureData.h"
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
#include <cstring>      // EndsWith 用 strlen
#include <string>
#include <vector>

// ============================================================================
//  开关：全屏 BOSS 提示
//
//  用户要求"这种需要做开关的就做开关"。
//  BOSS 提示是全屏红字，滥用会很吵，所以默认【关闭】。
//
//  打开方式二选一：
//    1. 改这里的 false -> true 重编译
//    2. 用 .say bosson / .say bossoff 运行时切换（本会话有效）
// ============================================================================
static bool g_allowBossEmote = false;

// ============================================================================
//  说话方式
// ============================================================================
enum SayMode
{
    SAY_NORMAL,     // 白字，近距离
    SAY_YELL,       // 红字，远距离
    SAY_EMOTE,      // 橙字，第三人称
    SAY_BOSS,       // 全屏 BOSS 提示
    SAY_WHISPER,    // 密语
};

// ============================================================================
//  小工具
// ============================================================================

// 按空白切分
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

static bool IsAllDigit(std::string const& s)
{
    if (s.empty())
        return false;
    for (char c : s)
        if (c < '0' || c > '9')
            return false;
    return true;
}

static std::string Lower(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}

// 从第 n 段开始，把【原文】拼回来（保留段间单空格）
//
//  台词含空格，不能切完就丢。
//  例：.say yell r 40 大家 快 跑！  ->  从第3段起 = "大家 快 跑！"
static std::string JoinFrom(std::vector<std::string> const& tok, size_t n)
{
    std::string out;
    for (size_t i = n; i < tok.size(); ++i)
    {
        if (!out.empty())
            out += ' ';
        out += tok[i];
    }
    return out;
}

// 字符串是否以某个字节序列结尾
static bool EndsWith(std::string const& s, char const* suffix)
{
    size_t n = strlen(suffix);
    if (s.size() < n)
        return false;
    return s.compare(s.size() - n, n, suffix) == 0;
}

// ----------------------------------------------------------------------------
//  按末尾标点挑表情
//
//  官方 cs_npc.cpp:913 用 text.back() 只能判 ASCII。
//  中文全角标点是 UTF-8 多字节（实测字节序列）：
//      ？ = EF BC 9F      ！ = EF BC 81
//      。 = E3 80 82      … = E2 80 A6
//  取最后一个【字节】会拿到 9F / 81 这种续字节，判断必然出错。
//  所以这里按【字节序列】比对。
// ----------------------------------------------------------------------------
static uint32 PickEmoteByPunct(std::string const& text)
{
    if (text.empty())
        return 0;

    // 疑问：? 或 ？
    if (EndsWith(text, "?") || EndsWith(text, "\xEF\xBC\x9F"))
        return 6;      // EMOTE_ONESHOT_QUESTION

    // 惊叹：! 或 ！
    if (EndsWith(text, "!") || EndsWith(text, "\xEF\xBC\x81"))
        return 5;      // EMOTE_ONESHOT_EXCLAMATION

    // 省略号：... 或 …… 或 … -> 沉思，不播动作
    if (EndsWith(text, "...") || EndsWith(text, "\xE2\x80\xA6"))
        return 0;

    // 其余一律普通说话动作
    return 1;          // EMOTE_ONESHOT_TALK
}

// 收集半径内的 Creature（沿用 .nst step26 / .emote step29 的写法）
static void CollectNear(Player* player, float radius, std::vector<Creature*>& out)
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
        if (c->IsPet() || c->IsTotem())
            continue;
        out.push_back(c);
    }
}

static void CollectByEntry(Player* player, uint32 entry, float radius,
                           std::vector<Creature*>& out)
{
    std::vector<Creature*> all;
    CollectNear(player, radius, all);
    for (Creature* c : all)
        if (c->GetEntry() == entry)
            out.push_back(c);
}

// ----------------------------------------------------------------------------
//  真正说话
// ----------------------------------------------------------------------------
static void DoTalk(Unit* u, SayMode mode, std::string const& text,
                   Player* whisperTo, bool withEmote)
{
    if (!u || text.empty())
        return;

    switch (mode)
    {
        case SAY_YELL:
            u->Yell(text, LANG_UNIVERSAL);                    // Unit.h:1824
            break;
        case SAY_EMOTE:
            u->TextEmote(text, nullptr, false);               // Unit.h:1825
            break;
        case SAY_BOSS:
            u->TextEmote(text, nullptr, true);                // isBossEmote -> 全屏
            break;
        case SAY_WHISPER:
            if (whisperTo)
                u->Whisper(text, LANG_UNIVERSAL, whisperTo, false);  // Unit.h:1826
            break;
        case SAY_NORMAL:
        default:
            u->Say(text, LANG_UNIVERSAL);                     // Unit.h:1823
            break;
    }

    // 自动配表情。emote/boss 是第三人称描述，本身不是"说话"，不配动作
    if (withEmote && mode != SAY_EMOTE && mode != SAY_BOSS)
    {
        uint32 e = PickEmoteByPunct(text);
        if (e)
            u->HandleEmoteCommand(Emote(e));                  // Unit.h:1037
    }
}

// ============================================================================
//  指令实现
// ============================================================================
class say_commandscript : public CommandScript
{
public:
    say_commandscript() : CommandScript("say_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "say", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleSay, "" },
        };
        return commandTable;
    }

    static void SendHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff00ff00[.say 剧情对白]|r");
        handler->SendSysMessage("  .say <文本>              选中目标说话（白字）");
        handler->SendSysMessage("  .say yell <文本>         喊话（红字，远距离）");
        handler->SendSysMessage("  .say emote <文本>        第三人称描述（橙字）");
        handler->SendSysMessage("  .say boss <文本>         全屏BOSS提示（需开开关）");
        handler->SendSysMessage("  .say whisper <文本>      对自己密语");
        handler->SendSysMessage("  .say r <半径> <文本>     周围所有NPC一起说");
        handler->SendSysMessage("  .say entry <ID> <文本>   指定entry的NPC说");
        handler->SendSysMessage("  .say me <文本>           自己说");
        handler->SendSysMessage("  .say noemote <文本>      不自动配表情");
        handler->SendSysMessage("  .say bosson / bossoff    开关全屏BOSS提示");
        handler->SendSysMessage("|cffffff00 修饰符可组合: .say yell r 40 大家快跑！|r");
        handler->SendSysMessage("|cffffff00 说话会按末尾标点自动配表情(?疑问 !惊叹 ...沉思)|r");
    }

    static bool HandleSay(ChatHandler* handler, char const* args)
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

        // ---------- help ----------
        if (s0 == "help" || s0 == "?" || tok[0] == "帮助")
        {
            SendHelp(handler);
            return true;
        }

        // ---------- BOSS 提示开关 ----------
        if (s0 == "bosson")
        {
            g_allowBossEmote = true;
            handler->SendSysMessage("|cff00ff00 全屏BOSS提示已开启|r");
            handler->SendSysMessage("|cffffff00 注意: 全屏红字对本地图所有人可见|r");
            return true;
        }
        if (s0 == "bossoff")
        {
            g_allowBossEmote = false;
            handler->SendSysMessage("|cff00ff00 全屏BOSS提示已关闭|r");
            return true;
        }

        // ---------- 解析修饰符（可任意顺序、可组合）----------
        SayMode mode      = SAY_NORMAL;
        bool    withEmote = true;
        bool    useRadius = false;
        bool    useEntry  = false;
        float   radius    = 0.0f;
        uint32  entry     = 0;
        bool    toSelf    = false;
        size_t  idx       = 0;

        while (idx < tok.size())
        {
            std::string w = Lower(tok[idx]);

            if (w == "yell" || tok[idx] == "喊话")
            {
                mode = SAY_YELL;   ++idx; continue;
            }
            if (w == "emote" || tok[idx] == "动作")
            {
                mode = SAY_EMOTE;  ++idx; continue;
            }
            if (w == "boss")
            {
                mode = SAY_BOSS;   ++idx; continue;
            }
            if (w == "whisper" || tok[idx] == "密语")
            {
                mode = SAY_WHISPER; ++idx; continue;
            }
            if (w == "noemote" || tok[idx] == "无表情")
            {
                withEmote = false; ++idx; continue;
            }
            if (w == "me" || tok[idx] == "自己")
            {
                toSelf = true;     ++idx; continue;
            }
            if (w == "r" || tok[idx] == "范围")
            {
                if (idx + 1 >= tok.size())
                {
                    handler->SendSysMessage("|cffff0000 用法: .say r <半径> <文本>|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                radius = float(atof(tok[idx + 1].c_str()));
                if (radius <= 0.0f || radius > 500.0f)
                {
                    handler->SendSysMessage("|cffff0000 半径需在 0-500 之间|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                useRadius = true;
                idx += 2;
                continue;
            }
            if (w == "entry" || tok[idx] == "编号")
            {
                if (idx + 1 >= tok.size() || !IsAllDigit(tok[idx + 1]))
                {
                    handler->SendSysMessage("|cffff0000 用法: .say entry <ID> <文本>|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                entry = uint32(atoi(tok[idx + 1].c_str()));
                useEntry = true;
                idx += 2;
                continue;
            }
            // 不是修饰符 -> 从这里开始是台词
            break;
        }

        // ---------- 取台词 ----------
        std::string text = JoinFrom(tok, idx);
        if (text.empty())
        {
            handler->SendSysMessage("|cffff0000 没有台词内容。.say help 看用法|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ---------- BOSS 开关拦截 ----------
        if (mode == SAY_BOSS && !g_allowBossEmote)
        {
            handler->SendSysMessage("|cffff0000 全屏BOSS提示当前【关闭】|r");
            handler->SendSysMessage("|cffffff00 用 .say bosson 开启（全屏红字，慎用）|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ---------- me：自己说 ----------
        if (toSelf)
        {
            DoTalk(player, mode, text,
                   mode == SAY_WHISPER ? player : nullptr, withEmote);
            snprintf(buf, sizeof(buf), "|cff00ff00 自己%s: %s|r",
                     ModeName(mode), text.c_str());
            handler->SendSysMessage(buf);
            return true;
        }

        // ---------- 收集目标 ----------
        std::vector<Creature*> targets;
        std::string scope = "选中";

        if (useRadius)
        {
            CollectNear(player, radius, targets);
            snprintf(buf, sizeof(buf), "半径%.0f", radius);
            scope = buf;
            // r 和 entry 同时给：先按半径收，再按 entry 过滤
            if (useEntry)
            {
                std::vector<Creature*> f;
                for (Creature* c : targets)
                    if (c->GetEntry() == entry)
                        f.push_back(c);
                targets.swap(f);
                snprintf(buf, sizeof(buf), "半径%.0f+entry%u", radius, entry);
                scope = buf;
            }
        }
        else if (useEntry)
        {
            CollectByEntry(player, entry, 200.0f, targets);
            snprintf(buf, sizeof(buf), "entry=%u", entry);
            scope = buf;
        }
        else
        {
            Creature* c = handler->getSelectedCreature();   // Chat.h:104
            if (!c)
            {
                handler->SendSysMessage("|cffff0000 没有选中 NPC。用 .say me <文本> 自己说，"
                                        "或 .say r <半径> <文本> 让周围NPC说|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            targets.push_back(c);
        }

        if (targets.empty())
        {
            handler->SendSysMessage("|cffff0000 范围内没有符合条件的 NPC|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ---------- 开说 ----------
        // 密语只能对 Player，NPC 对 NPC 密语没有意义 -> 统一密给发令的 GM
        Player* wt = (mode == SAY_WHISPER) ? player : nullptr;

        int n = 0;
        for (Creature* c : targets)
        {
            DoTalk(c, mode, text, wt, withEmote);
            ++n;
        }

        snprintf(buf, sizeof(buf), "|cff00ff00 %s: %d 个目标%s|r",
                 scope.c_str(), n, ModeName(mode));
        handler->SendSysMessage(buf);

        if (n > 10)
            handler->SendSysMessage("|cffffff00 提示: 目标较多，刷屏了就用更小的半径|r");

        return true;
    }

    static char const* ModeName(SayMode m)
    {
        switch (m)
        {
            case SAY_YELL:    return "[喊话]";
            case SAY_EMOTE:   return "[动作]";
            case SAY_BOSS:    return "[全屏BOSS]";
            case SAY_WHISPER: return "[密语]";
            default:          return "[说话]";
        }
    }
};

void AddSC_say_commandscript()
{
    new say_commandscript();
}
