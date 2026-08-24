/*
 * ============================================================================
 *  step36  .bd —— bot 对话拦截诊断
 * ============================================================================
 *
 *  为什么做这个：
 *    游荡bot无法对话的问题，我已经猜错三次（BC5那次的教训又犯了）：
 *      1. 猜 conf 默认值是0        -> 用户改成1，无效
 *      2. 猜传送状态未清除          -> 等10秒，无效
 *      3. 猜 conf.d 没被加载        -> 写进主conf，无效
 *
 *    【铁律】第一次归因失败后不要在同一假设上继续改。
 *    所以停止猜测 —— 让服务端【逐条打印】拦截条件的实际值。
 *
 *  bot_ai.cpp:7703-7707 的拦截条件一共 8 个，任何一个为真都会关闭对话框：
 *      1. !IsNpcBotModEnabled()
 *      2. !(IsWanderer() ? IsWanderingClassEnabled(class) : IsClassEnabled(class))
 *      3. IsTempBot()
 *      4. me->IsInCombat()
 *      5. CCed(me)
 *      6. IsCasting()
 *      7. IsDuringTeleport()          <- 高度可疑，见下
 *      8. HasBotCommandState(BOT_COMMAND_ISSUED_ORDER | BOT_COMMAND_NOGOSSIP)
 *      9. me->GetVehicle() && vehicle->IsInCombat()   (实现改用 Unit::GetVehicleBase())
 *     10. !IsGameMaster() && (IsWanderer() || IsSummon())    <- step33 改的这条
 *
 *  【实查发现的死锁】（这可能才是真凶）
 *
 *    bot_ai.cpp:17717   if (IsDuringTeleport()) return false;      <- GlobalUpdate 提前退出
 *    bot_ai.cpp:18257   恢复 UNIT_NPC_FLAG_GOSSIP                  <- 在 17717 【之后】
 *
 *    如果 _duringTeleport 卡住不清，GlobalUpdate 每次都在 17717 提前 return，
 *    18257 永远执行不到 -> bot 【永远拿不回 GOSSIP 标记】
 *    -> Player::GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_GOSSIP) 返回 nullptr
 *    -> 【服务端根本不会调用 OnGossipHello】，右键完全没反应
 *
 *    这个症状和"开关没生效"一模一样，但病根完全不同。
 *
 *  所以本指令同时报告 NPC_FLAG_GOSSIP 的实际状态，一眼分辨两种病。
 *
 *  权限：rbac::RBAC_PERM_COMMAND_WORLDTOOLS（step21 自建）
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "Spell.h"          // Spell::GetSpellInfo (Spells/Spell.h:456)  完整定义，不能只前向声明
#include "SpellInfo.h"      // SpellInfo::Id / SpellName (Spells/SpellInfo.h:314/377)
#include "UnitDefines.h"    // UNIT_STAND_STATE_STAND (Entities/Unit/UnitDefines.h:34)
#include "World.h"
#include "WorldSession.h"

#include "bot_ai.h"
#include "botdatamgr.h"
#include "botmgr.h"
#include "botconfig.h"
#include "botcommon.h"

#include <cstdio>
#include <string>
#include <vector>

class botdiag_commandscript : public CommandScript
{
public:
    botdiag_commandscript() : CommandScript("botdiag_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "bd",      rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleBotDiag, "" },
            { "botdiag", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleBotDiag, "" },
        };
        return commandTable;
    }

    // 打印一行 "条件名: 值 [结论]"
    static void Line(ChatHandler* handler, char const* name, bool blocking, char const* extra = nullptr)
    {
        char buf[512];
        if (blocking)
            snprintf(buf, sizeof(buf), "  |cffff0000[拦截]|r %-28s = true %s",
                     name, extra ? extra : "");
        else
            snprintf(buf, sizeof(buf), "  |cff00ff00[通过]|r %-28s = false %s",
                     name, extra ? extra : "");
        handler->SendSysMessage(buf);
    }

    // ------------------------------------------------------------------
    //  .bd fix —— 强制打破传送死锁
    //
    //  做三件事：
    //    1. AbortTeleport()          取消挂起的传送事件
    //    2. SetIsDuringTeleport(false)  清掉卡住的标记
    //    3. 直接补上 GOSSIP 标记     不等 GlobalUpdate
    //
    //  为什么要第3步：即使清了标记，GlobalUpdate 也要等下一个 tick 才恢复
    //  GOSSIP，而且如果还有别的条件挡着 18257，照样恢复不了。直接补最保险。
    // ------------------------------------------------------------------
    static bool DoFix(ChatHandler* handler, Creature* c, bot_ai* ai)
    {
        char buf[512];
        bool didSomething = false;

        if (ai->IsDuringTeleport())
        {
            ai->AbortTeleport();              // bot_ai.h:234  public
            ai->SetIsDuringTeleport(false);   // bot_ai.h:235  public
            handler->SendSysMessage("  |cff00ff00已清除卡住的传送状态|r");
            didSomething = true;
        }

        if (!c->HasNpcFlag(UNIT_NPC_FLAG_GOSSIP))
        {
            c->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);   // Unit.h:1098
            handler->SendSysMessage("  |cff00ff00已补上 GOSSIP 标记|r");
            didSomething = true;
        }

        // 【实测确认的主因】bot 在吃东西/喝水时 IsCasting()==true，
        // 会被 bot_ai.cpp:7704 拦截导致对话框直接关闭。
        // 吃喝是 channel 类法术（bot_ai.cpp:6053/6060 CastSpell），
        // 持续 20-30 秒，等不起。直接打断。
        // Unit.h:1486  void InterruptNonMeleeSpells(bool withDelayed, ...)  public
        bool wasCasting = c->HasUnitState(UNIT_STATE_CASTING)
                       || c->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr
                       || c->IsNonMeleeSpellCast(false, false, true, false, false);
        if (wasCasting)
        {
            // 先打断真实法术
            c->InterruptNonMeleeSpells(true);

            // 【重要】如果四个槽位本来就是空的，说明 UNIT_STATE_CASTING
            // 是【卡住的残留状态位】—— InterruptNonMeleeSpells 对它无效
            // （那函数只处理真实存在的 Spell 对象）。必须直接清状态位。
            // 这解释了"bd fix 说打断了，但 IsCasting 还是 true"。
            bool stillCasting = c->HasUnitState(UNIT_STATE_CASTING);
            if (stillCasting)
            {
                c->ClearUnitState(UNIT_STATE_CASTING);      // Unit.h public
                handler->SendSysMessage("  |cff00ff00已强制清除卡住的 UNIT_STATE_CASTING|r");
            }
            else
            {
                handler->SendSysMessage("  |cff00ff00已打断施法|r");
            }
            didSomething = true;
        }

        // 顺便让它站起来，坐着吃饭的姿势会继续触发进食判定
        if (c->GetStandState() != UNIT_STAND_STATE_STAND)
        {
            c->SetStandState(UNIT_STAND_STATE_STAND);
            handler->SendSysMessage("  |cff00ff00已让它站起来|r");
            didSomething = true;
        }

        if (!didSomething)
        {
            handler->SendSysMessage("  |cffffff00没有需要修复的（状态本来就正常）|r");
            return true;
        }

        snprintf(buf, sizeof(buf), "|cff00ff00 %s 已修复，现在试试右键|r", c->GetName().c_str());
        handler->SendSysMessage(buf);
        return true;
    }

    static bool HandleBotDiag(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        Creature* c = handler->getSelectedCreature();
        if (!c)
        {
            handler->SendSysMessage("|cffff0000 先选中一个 bot（.bf come 叫过来再选）|r");
            handler->SendSysMessage("|cffffff00 .bd      诊断  |  .bd fix  强制修复|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        char buf[512];

        // ---------- .bd fix ----------
        bool wantFix = false;
        if (args && *args)
        {
            std::string a(args);
            // 去掉首尾空格再比对
            size_t b = a.find_first_not_of(" \t");
            size_t e = a.find_last_not_of(" \t");
            if (b != std::string::npos)
            {
                a = a.substr(b, e - b + 1);
                for (char& ch : a)
                    if (ch >= 'A' && ch <= 'Z')
                        ch = char(ch - 'A' + 'a');
                if (a == "fix" || a == "修复")
                    wantFix = true;
            }
        }

        if (wantFix)
        {
            if (!c->IsNPCBot())
            {
                handler->SendSysMessage("|cffff0000 这不是 NPCBot|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            bot_ai* fixAi = c->GetBotAI();
            if (!fixAi)
            {
                handler->SendSysMessage("|cffff0000 这个bot没有 bot_ai|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            snprintf(buf, sizeof(buf), "|cff00ff00=== 修复 %s ===|r", c->GetName().c_str());
            handler->SendSysMessage(buf);
            return DoFix(handler, c, fixAi);
        }

        // ---------- 基本信息 ----------
        snprintf(buf, sizeof(buf), "|cff00ff00=== %s (entry %u) ===|r",
                 c->GetName().c_str(), c->GetEntry());
        handler->SendSysMessage(buf);

        if (!c->IsNPCBot())
        {
            handler->SendSysMessage("|cffff0000 这不是 NPCBot，是普通生物|r");
            handler->SendSysMessage("|cffffff00 用 .bf come 召一个真的游荡bot过来|r");
            return true;
        }

        bot_ai* ai = c->GetBotAI();
        if (!ai)
        {
            handler->SendSysMessage("|cffff0000 这个bot没有 bot_ai（异常状态）|r");
            return true;
        }

        // ---------- 【第一关】GOSSIP 标记 ----------
        // 这一关在 OnGossipHello 【之前】，Player::GetNPCIfCanInteractWith 检查
        // 没有这个标记 -> 服务端根本不调 OnGossipHello -> 右键完全没反应
        handler->SendSysMessage("|cffffff00--- 第一关：NPC_FLAG_GOSSIP ---|r");
        bool hasGossipFlag = c->HasNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        if (hasGossipFlag)
        {
            handler->SendSysMessage("  |cff00ff00[通过]|r 有 GOSSIP 标记");
        }
        else
        {
            handler->SendSysMessage("  |cffff0000[拦截]|r |cffff0000没有 GOSSIP 标记！|r");
            handler->SendSysMessage("  |cffff0000 服务端【根本不会调用 OnGossipHello】|r");
            handler->SendSysMessage("  |cffff0000 所以 step33 改的开关再对也没用|r");
            handler->SendSysMessage("  |cffffff00 根因见下面的 IsDuringTeleport|r");
        }

        // ---------- 【第二关】OnGossipHello 内部 10 个条件 ----------
        handler->SendSysMessage("|cffffff00--- 第二关：OnGossipHello 内部条件 ---|r");

        bool isWanderer = ai->IsWanderer();
        uint8 botClass  = ai->GetBotClass();

        Line(handler, "!IsNpcBotModEnabled()", !BotCfg::IsNpcBotModEnabled());

        bool classDisabled = !(isWanderer ? BotCfg::IsWanderingClassEnabled(botClass)
                                          : BotCfg::IsClassEnabled(botClass));
        snprintf(buf, sizeof(buf), "(class=%u wanderer=%s)", uint32(botClass), isWanderer ? "yes" : "no");
        Line(handler, "class disabled", classDisabled, buf);

        Line(handler, "IsTempBot()",        ai->IsTempBot());
        Line(handler, "me->IsInCombat()",   c->IsInCombat());
        Line(handler, "CCed(me)",           bot_ai::CCed(c));

        // bot_ai::IsCasting 是 protected（bot_ai.h:450，377行起的protected段），
        // scripts 里访问不到。照它的实现（bot_ai.cpp:20864）用 Unit 的 public 方法等价复现：
        //   return u->HasUnitState(UNIT_STATE_CASTING) || IsChanneling(u)
        //          || u->IsNonMeleeSpellCast(false, false, true, false, false);
        // IsChanneling 同样不可见，但它查的是 CURRENT_CHANNELED_SPELL，
        // 用 GetCurrentSpell(CURRENT_CHANNELED_SPELL) 直接判等价。
        bool casting = c->HasUnitState(UNIT_STATE_CASTING)
                    || c->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr
                    || c->IsNonMeleeSpellCast(false, false, true, false, false);
        Line(handler, "IsCasting()",        casting);

        // 【关键】IsCasting 为真时，把三个来源【分别】拆开报告，
        // 并打印具体是哪个法术。不然只知道"在施法"却不知道施什么，
        // 又要开始猜 —— 前面已经因为猜错浪费三轮了。
        if (casting)
        {
            handler->SendSysMessage("  |cffffff00  施法来源拆解：|r");

            bool s1 = c->HasUnitState(UNIT_STATE_CASTING);
            bool s2 = c->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr;
            bool s3 = c->IsNonMeleeSpellCast(false, false, true, false, false);

            snprintf(buf, sizeof(buf), "    UNIT_STATE_CASTING        = %s", s1 ? "|cffff0000true|r" : "false");
            handler->SendSysMessage(buf);
            snprintf(buf, sizeof(buf), "    CHANNELED_SPELL 非空      = %s", s2 ? "|cffff0000true|r" : "false");
            handler->SendSysMessage(buf);
            snprintf(buf, sizeof(buf), "    IsNonMeleeSpellCast       = %s", s3 ? "|cffff0000true|r" : "false");
            handler->SendSysMessage(buf);

            // 四个槽位逐个打印正在施放的法术
            // Unit.h:1488 GetCurrentSpell / Spell.h:456 GetSpellInfo / SpellInfo.h:314 Id
            static char const* slotName[4] = { "MELEE", "GENERIC", "CHANNELED", "AUTOREPEAT" };
            bool anySpell = false;
            for (uint8 i = 0; i < 4; ++i)
            {
                Spell* sp = c->GetCurrentSpell(CurrentSpellTypes(i));
                if (!sp)
                    continue;
                anySpell = true;
                SpellInfo const* si = sp->GetSpellInfo();
                uint32 sid = si ? si->Id : 0;
                char const* sname = (si && si->SpellName[0]) ? si->SpellName[0] : "?";
                snprintf(buf, sizeof(buf), "    [%s] |cff00ff00%u|r  %s", slotName[i], sid, sname);
                handler->SendSysMessage(buf);
            }
            if (!anySpell)
            {
                handler->SendSysMessage("    |cffff0000四个槽位都没有法术，但状态位是true|r");
                handler->SendSysMessage("    |cffff0000-> 是【卡住的残留状态】，不是真在施法|r");
                handler->SendSysMessage("    |cffffff00-> .bd fix 会强制清除|r");
            }
        }

        // 【重点】这条如果 true，同时又没有 GOSSIP 标记，就是那个死锁
        bool duringTele = ai->IsDuringTeleport();
        Line(handler, "IsDuringTeleport()", duringTele,
             duringTele ? "|cffff0000<<< 很可能是这个|r" : nullptr);

        bool cmdBlock = ai->HasBotCommandState(BOT_COMMAND_ISSUED_ORDER | BOT_COMMAND_NOGOSSIP);
        Line(handler, "ISSUED_ORDER|NOGOSSIP", cmdBlock);

        // 原文是 me->GetVehicle() && me->GetVehicle()->GetBase()->IsInCombat()
        // 但 GetBase() 需要 Vehicle 的完整定义（要 include Vehicle.h）。
        // Unit.h:1752 有现成的 GetVehicleBase()，直接返回 Unit*，不需要完整类型。
        Unit* vehBase = c->GetVehicleBase();
        bool vehCombat = vehBase && vehBase->IsInCombat();
        Line(handler, "vehicle in combat", vehCombat);

        // ---------- step33 改的那条 ----------
        handler->SendSysMessage("|cffffff00--- step33 改的条件 ---|r");

        bool allowHire = BotCfg::IsWanderingBotHireEnabled();
        snprintf(buf, sizeof(buf),
                 "  AllowHire 配置读到的值 = |cff00ff00%s|r", allowHire ? "true(1)" : "false(0)");
        handler->SendSysMessage(buf);

        if (!allowHire)
        {
            handler->SendSysMessage("  |cffff0000 配置是0！检查主conf里的|r");
            handler->SendSysMessage("  |cffff0000 NpcBot.WanderingBot.AllowHire = 1|r");
        }

        bool isSummon = c->IsSummon();
        bool isGM     = player->IsGameMaster();

        // 改后的表达式
        bool step33Block = (!isGM) && ((isWanderer && !allowHire) || isSummon);
        snprintf(buf, sizeof(buf), "(GM=%s wanderer=%s summon=%s)",
                 isGM ? "yes" : "no", isWanderer ? "yes" : "no", isSummon ? "yes" : "no");
        Line(handler, "step33 条件", step33Block, buf);

        // ---------- 结论 ----------
        handler->SendSysMessage("|cffffff00--- 结论 ---|r");

        if (!hasGossipFlag)
        {
            handler->SendSysMessage("|cffff0000 病因：缺 GOSSIP 标记，卡在第一关|r");
            if (duringTele)
            {
                handler->SendSysMessage("|cffff0000 根因：IsDuringTeleport 卡住了|r");
                handler->SendSysMessage("|cffffff00 死锁：bot_ai.cpp:17717 提前return|r");
                handler->SendSysMessage("|cffffff00       导致 18257 恢复标记的代码永远跑不到|r");
                handler->SendSysMessage("|cff00ff00 解法：用 .bd fix 强制修复|r");
            }
            else
            {
                handler->SendSysMessage("|cffffff00 但传送状态是正常的，需要进一步查|r");
                handler->SendSysMessage("|cff00ff00 试试 .bd fix|r");
            }
            return true;
        }

        bool anyBlock = !BotCfg::IsNpcBotModEnabled() || classDisabled || ai->IsTempBot() ||
                        c->IsInCombat() || bot_ai::CCed(c) || casting || duringTele ||
                        cmdBlock || vehCombat || step33Block;

        if (!anyBlock)
        {
            handler->SendSysMessage("|cff00ff00 所有条件都通过，对话【应该】能开|r");
            handler->SendSysMessage("|cffffff00 如果实际打不开，问题在客户端或网络层|r");
        }
        else
        {
            handler->SendSysMessage("|cffff0000 上面标红的就是原因|r");
        }

        return true;
    }
};

void AddSC_botdiag_commandscript()
{
    new botdiag_commandscript();
}

/* ============================================================================
 *  API 核实记录
 * ============================================================================
 *  bot_ai.h:134   uint8 GetBotClass() const { return _botclass; }        public
 *  bot_ai.h:151   bool HasBotCommandState(uint32 st) const               public
 *  bot_ai.h:192   bool IAmFree() const;                                  public
 *  bot_ai.h:195   bool IsWanderer() const { return _wanderer; }          public
 *  bot_ai.h:226   static bool CCed(Unit const*, bool root = false);      public static
 *  bot_ai.h:232   bool IsDuringTeleport() const                          public
 *  bot_ai.h:235   void SetIsDuringTeleport(bool value)                   public  <- fix 用
 *  bot_ai.h:240   bool IsTempBot() const;                                public
 *  bot_ai.h:450   bool IsCasting(...) const;   【protected!】不可用，已用 Unit 公有方法等价替代
 *  Creature.h:392 bool IsNPCBot() const override;
 *
 *  拦截条件原文 bot_ai.cpp:7703-7707
 *  GOSSIP标记恢复 bot_ai.cpp:18257（在 GlobalUpdate 内）
 *  提前return     bot_ai.cpp:17717  if (IsDuringTeleport()) return false;
 *  标记设置       botmgr.cpp:555    botai->SetIsDuringTeleport(true);
 *  标记清除       botmgr.cpp:634    botai->SetIsDuringTeleport(false);
 *                 bot_ai.cpp:19031  SetIsDuringTeleport(false);
 *  第一关检查     Player.cpp:2178   if (npcFlags && !creature->HasNpcFlag(npcFlags)) return nullptr;
 * ============================================================================
 */
