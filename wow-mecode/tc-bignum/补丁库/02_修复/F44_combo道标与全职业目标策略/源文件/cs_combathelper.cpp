/*
 * ============================================================================
 *  战斗辅助 —— cs_combathelper.cpp  v2（专精菜单版）
 * ============================================================================
 *
 *  三个功能合一：
 *
 *   .set    属性精调    —— 百分比锁定暴击/命中/急速/精准等
 *   .bar    智能配栏    —— 弹菜单选专精，自动配满快捷栏
 *   .combo  自动连招    —— 弹菜单选专精，全循环自动打
 *
 *  ── v2 新增 ────────────────────────────────────────────────────────────
 *   1. .bar / .combo 都改成 Gossip 菜单，自动识别职业弹出对应专精窗口
 *      · 一般职业 4 个窗口：3 个单专精 + 1 个"全专精"（GM3 全天赋）
 *      · 德鲁伊 5 个窗口：平衡/野性猫/野性熊/恢复 + 全专精
 *   2. 移动感知：读条技（SF_NO_MOVE）在移动中自动跳过，
 *      站定后立刻补上，连招不断档，并给聊天框提示
 *   3. 智能条件：斩杀技看血线、AOE 看周围怪数、DOT 看目标身上有没有、
 *      BUFF 看自己身上有没有、终结技看连击点、近战技看距离
 *   4. 法术等级自动解析：数据表存 rank-1 ID，
 *      运行时用 rank 链找玩家会的最高阶（低级号也能用）
 *
 *  ── 指令 ──────────────────────────────────────────────────────────────
 *   .set crit 100 / hit / haste / expertise / dodge / parry / block / arp
 *   .set allres 5000   .set bosshp 50   .set show   .set clear
 *
 *   .bar              弹专精菜单
 *   .bar auto         按当前天赋自动判断专精直接配
 *   .bar clear        清空快捷栏
 *
 *   .combo            弹专精菜单（选完即开）
 *   .combo off        关闭
 *   .combo list       看当前连招序列
 *
 *  ── 已核实的 API（文件:行号）───────────────────────────────────────────
 *   Player.h:1649   ApplyRatingMod(CombatRating, int32, bool)         public
 *   Player.h:1663   GetRatingMultiplier(CombatRating)                 public
 *   Player.h:1443   HasSpell(uint32)                                  public
 *   Player.h:1556   addActionButton(uint8, uint32, uint8)             public
 *   Player.h:1557   removeActionButton(uint8)                         public
 *   Player.h:1560   SendActionButtons(uint32) const                   public
 *   Player.h:1477   GetActiveTalentGroup()                            public
 *   Player.h:1507   GetTalentMap(uint8)                               public
 *   Player.h:1401   GetSelectedUnit()                                 public
 *   Unit.h:1771     isMoving()                                        public
 *   Unit.h:1417     HasAura(uint32, ...)                              public
 *   Unit.h:1707     GetComboPoints(Unit const*)                       public
 *   Unit.h:860      GetVictim()                                       public
 *   Unit.h:872      SelectNearbyTarget(Unit*, float)                  public
 *   Unit.h:845      IsWithinMeleeRange(Unit const*)                   public
 *   Unit.h:1482     IsNonMeleeSpellCast(bool,...)                     public
 *   Unit.h:1496     GetSpellHistory()                                 public
 *   SpellHistory.h:79   IsReady(SpellInfo const*, uint32, bool)       public
 *   SpellHistory.h:132  HasGlobalCooldown(SpellInfo const*)           public
 *   SpellMgr.h:659  GetSpellInfo(uint32)                              public
 *   SpellMgr.h:596  GetNextSpellInChain(uint32)                       public
 *   SpellInfo.h:491 CalcCastTime(Spell*)                              public
 *   SpellInfo.h:341 CastTimeEntry                                     public
 *   SpellInfo.h:346 InterruptFlags                                    public
 *   SpellDefines.h:46  SPELL_INTERRUPT_FLAG_MOVEMENT = 0x01
 *   Object.h:532    CastSpell(CastSpellTargetArg, uint32, CastSpellExtraArgs)
 *   Object.h:620    EventProcessor m_Events                           public
 *   EventProcessor.h:109  AddEventAtOffset(lambda, Milliseconds)
 *   WorldObject.h:534  IsValidAttackTarget(WorldObject const*, ...)   public
 *   GossipDef.h:168 AddMenuItem(int32,icon,msg,sender,action,box,money,coded)
 *   GossipDef.h:30  GOSSIP_MAX_MENU_ITEMS = 32   ← 硬上限，超了 ASSERT 崩服
 *
 *  ── sender 段登记（全服唯一）──────────────────────────────────────────
 *   本文件占用 9401 - 9409。1~11 是套装、9101+ 传送、9201+ 幻化、9301+ gmhelp。
 *
 *  放置：D:\TrinityCore\src\server\scripts\Commands\cs_combathelper.cpp
 *  同目录还要放 CombatSpecData.h / CombatSpecData.cpp
 *  RBAC：71011      新增源文件必须重跑 CMake
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Cell.h"
#include "CellImpl.h"
#include "Chat.h"
#include "CombatSpecData.h"
#include "Creature.h"
#include "CustomStatPersist.h"      // step20：属性持久化
#include "DBCStores.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "RBAC.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellDefines.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace CombatHelper
{
    // ==================================================================
    //  sender 段（全服唯一，见文件头登记表）
    // ==================================================================
    static constexpr uint32 SENDER_BAR_SPEC   = 9401;  // .bar   选专精
    static constexpr uint32 SENDER_COMBO_SPEC = 9402;  // .combo 选专精
    static constexpr uint32 SENDER_BAR_ROLE   = 9403;  // .bar   选职责
    static constexpr uint32 SENDER_COMBO_ROLE = 9404;  // .combo 选职责
    static constexpr uint32 SENDER_SCENE      = 9405;  // 选场景
    static constexpr uint32 SENDER_BUFF       = 9406;  // .buff  菜单
    static constexpr uint32 SENDER_SETUP      = 9407;  // .setup menu 开关配置
    static constexpr uint32 SENDER_NAV        = 9409;  // 导航

    static constexpr uint32 ACTION_ALL_SPECS  = 200;   // "全专精"选项
    static constexpr uint32 NAV_CLOSE         = 1;
    static constexpr uint32 NAV_BACK          = 2;

    // ==================================================================
    //  属性名 -> CombatRating（Unit.h:322 起 25 项）
    // ==================================================================
    struct RatingAlias
    {
        char const*   name;
        CombatRating  cr;
        char const*   cn;
    };

    static RatingAlias const g_ratings[] =
    {
        { "crit",      CR_CRIT_MELEE,        "暴击"     },
        { "hit",       CR_HIT_MELEE,         "命中"     },
        { "haste",     CR_HASTE_MELEE,       "急速"     },
        { "expertise", CR_EXPERTISE,         "精准"     },
        { "dodge",     CR_DODGE,             "躲闪"     },
        { "parry",     CR_PARRY,             "招架"     },
        { "block",     CR_BLOCK,             "格挡"     },
        { "defense",   CR_DEFENSE_SKILL,     "防御"     },
        { "arp",       CR_ARMOR_PENETRATION, "护甲穿透" },
    };

    inline void GetLinkedRatings(CombatRating base, std::vector<CombatRating>& out)
    {
        out.clear();
        switch (base)
        {
            case CR_CRIT_MELEE:  out = { CR_CRIT_MELEE,  CR_CRIT_RANGED,  CR_CRIT_SPELL  }; break;
            case CR_HIT_MELEE:   out = { CR_HIT_MELEE,   CR_HIT_RANGED,   CR_HIT_SPELL   }; break;
            case CR_HASTE_MELEE: out = { CR_HASTE_MELEE, CR_HASTE_RANGED, CR_HASTE_SPELL }; break;
            default:             out = { base }; break;
        }
    }

    inline RatingAlias const* FindRating(std::string const& s)
    {
        std::string t = s;
        std::transform(t.begin(), t.end(), t.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // 中文别名单独判，避免 tolower 破坏多字节
        if (s == "暴击")     return &g_ratings[0];
        if (s == "命中")     return &g_ratings[1];
        if (s == "急速")     return &g_ratings[2];
        if (s == "精准")     return &g_ratings[3];
        if (s == "躲闪")     return &g_ratings[4];
        if (s == "招架")     return &g_ratings[5];
        if (s == "格挡")     return &g_ratings[6];
        if (s == "防御")     return &g_ratings[7];
        if (s == "护甲穿透") return &g_ratings[8];

        for (auto const& r : g_ratings)
            if (t == r.name)
                return &r;
        return nullptr;
    }

    // ==================================================================
    //  法术等级解析
    // ==================================================================
    /*
     * 数据表里存的是 rank-1 的 ID（取自 NPCBot AI）。
     * 这里沿着 rank 链往上爬，返回玩家【实际学会的最高阶】。
     *
     * SpellMgr.h:596 GetNextSpellInChain() 是 public（已核实）。
     * 找不到就返回 0，表示这个技能玩家还没学。
     */
    inline uint32 ResolveRank(Player* player, uint32 rank1)
    {
        uint32 best = 0;
        uint32 cur  = rank1;
        uint32 guard = 0;              // 防御性：链最多爬 20 层，避免脏数据死循环

        while (cur && guard++ < 20)
        {
            if (player->HasSpell(cur))
                best = cur;
            cur = sSpellMgr->GetNextSpellInChain(cur);
        }
        return best;
    }

    // ==================================================================
    //  按天赋点自动判断当前专精
    // ==================================================================
    /*
     * 思路：数每个天赋页（TalentTab）投了多少点，点最多的那页就是主专精。
     *
     * TalentEntry（DBCStructure.h:1665）：TabID / SpellRank[5]
     * GetTalentTabPages(cls)（DBCStores.h:67）返回该职业 3 个页的 ID，
     *   按 OrderIndex 排（DBCStores.cpp:552），正好对应我们的 specIdx 0/1/2。
     * GetTalentMap(group)（Player.h:1507，public）是玩家投点记录。
     *
     * 德鲁伊特判：野性页（OrderIndex=1）拆成"猫"和"熊"两个专精，
     *   用当前形态区分——熊形态(5487)算熊坦，否则算猫。
     */
    inline uint8 DetectSpec(Player* player)
    {
        uint8 cls = player->GetClass();
        uint32 const* tabs = GetTalentTabPages(cls);
        if (!tabs)
            return 0;

        uint32 points[3] = { 0, 0, 0 };

        PlayerTalentMap const* tmap = player->GetTalentMap(player->GetActiveTalentGroup());
        if (tmap)
        {
            for (auto const& kv : *tmap)
            {
                if (kv.second == PLAYERSPELL_REMOVED)
                    continue;

                TalentSpellPos const* pos = GetTalentSpellPos(kv.first);
                if (!pos)
                    continue;

                TalentEntry const* te = sTalentStore.LookupEntry(pos->talent_id);
                if (!te)
                    continue;

                for (uint8 i = 0; i < 3; ++i)
                    if (tabs[i] == te->TabID)
                        points[i] += (pos->rank + 1);
            }
        }

        uint8 best = 0;
        for (uint8 i = 1; i < 3; ++i)
            if (points[i] > points[best])
                best = i;

        // 德鲁伊：野性页(1) 要再分猫/熊
        if (cls == CLASS_DRUID)
        {
            if (best == 1)
            {
                // 熊形态 5487 / 巨熊形态 9634 都算熊坦
                if (player->HasAura(5487) || player->HasAura(9634))
                    return 2;          // 野性-熊
                return 1;              // 野性-猫
            }
            if (best == 2)
                return 3;              // 恢复 -> specIdx 3
            return 0;                  // 平衡
        }

        return best;
    }

    // ==================================================================
    //  快捷栏分区
    // ==================================================================
    /*
     * 客户端槽位映射（wowpedia Action slot，已核实）：
     *   0-11   主界面第1排         ← 玩家最常看的
     *   12-23  主界面第2页（Shift翻页）
     *   24-35  右侧竖排 1
     *   36-47  右侧竖排 2
     * 服务端 Player.h:241 MAX_ACTION_BUTTONS = 144。
     */
    /*
     * v3.5：主循环给【两排】。
     *
     * 实测 93 种组合里有 24 种主循环超过 12 个技能
     * （神圣骑治疗 17 个、鲜血DK 16 个），
     * 旧版一排 12 格会把后面的技能【直接截断配不上栏】——
     * 填充技尤其容易被砍掉，正好是用来填 GCD 空隙的那批。
     *
     * 现在：主循环 0-23（两排 24 格），够放最大的 17 个。
     */
    static constexpr uint8 BAR_ROTATION  = 0;    // 0-23   主循环（两排）
    static constexpr uint8 BAR_BURST     = 24;   // 24-35  爆发
    static constexpr uint8 BAR_DEFENSIVE = 36;   // 36-47  保命/紧急
    static constexpr uint8 BAR_BUFF      = 48;   // 48-59  增益/功能
    static constexpr uint8 BAR_SLOTS_PER = 12;
    static constexpr uint8 BAR_ROTATION_SLOTS = 24;   // 主循环专用容量
    static constexpr uint8 BAR_TOTAL_SLOTS    = 60;   // 需要清理的总格数

    // ==================================================================
    //  每个玩家的连招状态
    // ==================================================================
    struct ComboState
    {
        bool   on          = false;
        uint8  specIdx     = 0;
        bool   allSpecs    = false;   // 全专精模式：把该职业所有专精的技能混在一起
        uint32 tick        = 0;
        uint32 generation  = 0;       // F44：每次开/关递增，旧500ms事件自动失效
        bool   warnedMove  = false;   // 移动提示只发一次，不刷屏
        uint32 castCount   = 0;

        // ---- v3 ----
        uint8  role        = CombatSpec::ROLE_AUTO;   // 队伍职责（玩家自选，会记住）
        uint8  scene       = CombatSpec::SCENE_AUTO;  // 战斗场景
        bool   autoBuff    = true;                    // 自动补增益
        bool   autoDispel  = true;                    // 自动驱散/解控
        uint32 healCount   = 0;                       // 奶了多少次
        uint32 dispelCount = 0;                       // 驱散了多少次
        uint32 lastBuffTry = 0;                       // 上次补 buff 的时间戳(ms)，限流用
        uint32 buffQueueUntil = 0;                    // .buff 队列跑到什么时候(ms)，期间 combo 不碰 buff
        uint32 buffGeneration = 0;                   // F44：取消旧.buff队列事件

        // F44：rank-1技能 -> 稳定友方目标。最低血变化不会迁移。
        std::unordered_map<uint32, ObjectGuid> maintainTargets;

        // 菜单流程中的临时选择（选专精 -> 选职责 -> 应用）
        uint8  pendingSpec    = 0;
        bool   pendingAll     = false;
        bool   pendingFromBar = false;   // true=.bar 流程, false=.combo 流程

        // ---- v4：.setup 一键配置 ----
        /*
         * .setup 是【一键执行】，不弹菜单。
         * 这几个开关决定它执行哪几步 —— 用 .setup menu 单独配置，
         * 配完就记住，以后 .setup 直接按这套跑。
         */
        bool setupGear   = true;    // 发装备 + 自动穿上
        bool setupBar    = true;    // 配快捷栏
        bool setupBuff   = true;    // 补满增益
        bool setupCombo  = false;   // 开自动连招（默认关，让玩家自己决定何时开打）
        bool setupStat   = false;   // 属性精调（默认关，怕误改）
        uint32 setupIlvl = 0;       // 装等上限，0=不限
    };

    inline std::unordered_map<uint32, ComboState>& States()
    {
        static std::unordered_map<uint32, ComboState> m;
        return m;
    }

    // F44：从ComboTick局部静态移到可清理的命名缓存。
    struct PlanCache
    {
        CombatSpec::BuiltPlan plan;
        uint8 cls = 255, spec = 255, role = 255, scene = 255;
        bool all = false;
    };

    inline std::unordered_map<uint32, PlanCache>& PlanCaches()
    {
        static std::unordered_map<uint32, PlanCache> m;
        return m;
    }

    // ==================================================================
    //  v3.1：施法失败退避表
    // ==================================================================
    /*
     * 为什么需要这个：
     *   有些技能【永远放不出来】但服务端条件检查又拦不住，典型例子：
     *     · 圣骑士「强效力量祝福」需要消耗材料【王者之符】，没有就 SPELL_FAILED_REAGENTS
     *     · 萨满图腾在某些地形放不下
     *     · 需要特定姿态/形态但玩家不满足
     *   这些技能 HasAura 永远为假 -> 每 500ms 重试一次 -> 无限刷屏（用户实测到的 bug）
     *
     * 解决：施法真的失败了就记一笔，短时间内不再试。
     *   · 材料/学习类硬失败 -> 冷静 5 分钟（基本等于本次战斗不再试）
     *   · 距离/朝向/视线类软失败 -> 冷静 2 秒（走近了就能放）
     *   · 其余 -> 冷静 10 秒
     *
     * GameTime::GetGameTimeMS()（GameTime.h:35）已核实。
     */
    struct FailInfo
    {
        uint32 untilMs    = 0;
        uint32 count      = 0;
        uint32 lastFailMs = 0;   // v3.4：用来判断"隔了很久没失败"就清零计数
    };

    inline std::unordered_map<uint64, FailInfo>& Fails()
    {
        static std::unordered_map<uint64, FailInfo> m;
        return m;
    }

    inline uint64 FailKey(Player* p, uint32 spellId)
    {
        return (uint64(p->GetGUID().GetCounter()) << 32) | uint64(spellId);
    }

    /*
     * 沿 rank 链检查身上有没有【任意一阶】的这个 buff。
     *
     * 传入 rank-1 的 ID，从链头往上逐阶查。
     * SpellMgr.h:594 GetFirstSpellInChain / :596 GetNextSpellInChain（都 public，已核实）
     */
    inline bool HasAnyRankAura(Unit* unit, uint32 rank1,
                               ObjectGuid caster = ObjectGuid::Empty)
    {
        if (!unit)
            return false;

        uint32 cur = sSpellMgr->GetFirstSpellInChain(rank1);
        if (!cur)
            cur = rank1;

        uint32 guard = 0;
        while (cur && guard++ < 20)          // 防脏数据死循环
        {
            if (unit->HasAura(cur, caster))
                return true;
            cur = sSpellMgr->GetNextSpellInChain(cur);
        }
        return false;
    }

    /*
     * v3.3：buff 还剩多久过期（毫秒）。
     *   返回 -1 = 永久光环（圣印/光环/姿态这类）
     *   返回  0 = 身上没有
     *
     * 用来解决「脱战每 3 秒刷一次祝福」——
     * 光看"有没有"不够，还得看"是不是快到期了"。
     * 只有剩余 < 5 分钟才值得重上，否则纯属浪费。
     *
     * Unit.h:1407 GetAuraApplication / SpellAuras.h:75 GetBase()
     * SpellAuras.h:156 GetDuration() / :161 IsPermanent()   全部 public 已核实
     */
    inline int32 AuraRemainMs(Unit* unit, uint32 rank1,
                             ObjectGuid caster = ObjectGuid::Empty)
    {
        if (!unit)
            return 0;

        uint32 cur = sSpellMgr->GetFirstSpellInChain(rank1);
        if (!cur)
            cur = rank1;

        uint32 guard = 0;
        while (cur && guard++ < 20)
        {
            if (AuraApplication const* app = unit->GetAuraApplication(cur, caster))
            {
                if (Aura const* a = app->GetBase())
                {
                    if (a->IsPermanent())
                        return -1;
                    return a->GetDuration();
                }
            }
            cur = sSpellMgr->GetNextSpellInChain(cur);
        }
        return 0;
    }

    inline bool IsBackedOff(Player* p, uint32 spellId)
    {
        auto it = Fails().find(FailKey(p, spellId));
        if (it == Fails().end())
            return false;
        return GameTime::GetGameTimeMS() < it->second.untilMs;
    }

    inline void MarkFailed(Player* p, uint32 spellId, SpellCastResult res)
    {
        /*
         * v3.4 重做（用户实测：猎人只剩普通攻击、法师只剩火焰冲击）
         *
         * 老版的致命缺陷：
         *   1. count 只增不减 —— 技能因【临时原因】失败 3 次后被永久拉黑 60 秒，
         *      战斗越久被拉黑的越多，最后只剩少数几个技能能放。
         *   2. TOO_CLOSE / NO_AMMO 等常见码没处理，落到 default 10 秒。
         *      猎人怪一贴脸，所有射击技全部 TOO_CLOSE -> 10 秒起步 -> 3次后60秒
         *      -> 表现就是"只有普通攻击"。
         *   3. 移动中读条被判失败也会累积 -> 法师只剩瞬发技。
         *
         * 新版原则：
         *   · 位置/朝向/移动/资源类 = 瞬态，退避极短(0.5~2秒)，且【不累计】
         *   · 只有真正的硬失败（缺材料/缺物品）才长退避并累计
         */
        uint32 cd;
        bool   transient = false;      // 瞬态失败：不累计次数，不升级惩罚

        switch (res)
        {
            // ---- 硬失败：缺东西，这次战斗基本没戏 ----
            case SPELL_FAILED_REAGENTS:
            case SPELL_FAILED_ITEM_NOT_FOUND:
            case SPELL_FAILED_NEED_AMMO:
            case SPELL_FAILED_NEED_AMMO_POUCH:
            case SPELL_FAILED_NEED_EXOTIC_AMMO:
            case SPELL_FAILED_NO_AMMO:
            case SPELL_FAILED_EQUIPPED_ITEM:
            case SPELL_FAILED_EQUIPPED_ITEM_CLASS:
            case SPELL_FAILED_EQUIPPED_ITEM_CLASS_MAINHAND:
            case SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND:
                cd = 300000;    // 5 分钟
                break;

            // ---- 姿态/形态：换个姿态就能放，中等 ----
            case SPELL_FAILED_NOT_SHAPESHIFT:
            case SPELL_FAILED_ONLY_SHAPESHIFT:
            case SPELL_FAILED_NOT_STANDING:
                cd = 15000;
                break;

            /*
             * ---- 瞬态失败：位置/朝向/移动/资源 ----
             * 这些下一秒就可能变了，只压 0.5~2 秒，而且【绝不累计】。
             * 猎人的 TOO_CLOSE（怪贴脸）、法师的 MOVING 都走这条。
             */
            case SPELL_FAILED_TOO_CLOSE:
            case SPELL_FAILED_OUT_OF_RANGE:
            case SPELL_FAILED_LINE_OF_SIGHT:
            case SPELL_FAILED_UNIT_NOT_INFRONT:
            case SPELL_FAILED_NOT_BEHIND:
            case SPELL_FAILED_MOVING:
                cd = 800;
                transient = true;
                break;

            // 正在读条 / 还没好：极短
            case SPELL_FAILED_SPELL_IN_PROGRESS:
            case SPELL_FAILED_NOT_READY:
                cd = 500;
                transient = true;
                break;

            // 资源不够：等回蓝/回怒
            case SPELL_FAILED_NO_POWER:
                cd = 2000;
                transient = true;
                break;

            // 目标状态类（免疫/血线不对/光环状态）：短
            case SPELL_FAILED_BAD_TARGETS:
            case SPELL_FAILED_TARGET_AURASTATE:
            case SPELL_FAILED_CASTER_AURASTATE:
                cd = 3000;
                transient = true;
                break;

            default:
                cd = 5000;      // 未知原因：5 秒，不再是 10 秒
                break;
        }

        uint32 now = GameTime::GetGameTimeMS();
        FailInfo& fi = Fails()[FailKey(p, spellId)];

        /*
         * 关键修复：距上次失败超过 30 秒，说明中间它是能用的，
         * 把计数清零 —— 否则整场战斗累计下来必被拉黑。
         */
        if (fi.lastFailMs && now - fi.lastFailMs > 30000)
            fi.count = 0;

        fi.lastFailMs = now;
        fi.untilMs    = now + cd;

        if (!transient)
        {
            ++fi.count;
            // 只有非瞬态失败反复出现，才升级惩罚（且封顶 60 秒）
            if (fi.count >= 5 && cd < 60000)
                fi.untilMs = now + 60000;
        }
    }

    inline void ClearFailed(Player* p, uint32 spellId)
    {
        Fails().erase(FailKey(p, spellId));
    }

    // F44：所有自动路径共用同一施法事务；失败绝不记为成功。
    inline bool CastChecked(Player* player, Unit* target, uint32 spellId)
    {
        if (!player || !target || !spellId || IsBackedOff(player, spellId))
            return false;

        SpellCastResult result =
            player->CastSpell(target, spellId, CastSpellExtraArgs(false));
        if (result != SPELL_CAST_OK)
        {
            MarkFailed(player, spellId, result);
            return false;
        }

        ClearFailed(player, spellId);
        return true;
    }

    // 玩家下线/重开连招时清掉他的退避记录
    inline void ClearAllFails(Player* p)
    {
        uint32 low = p->GetGUID().GetCounter();
        for (auto it = Fails().begin(); it != Fails().end(); )
        {
            if (uint32(it->first >> 32) == low)
                it = Fails().erase(it);
            else
                ++it;
        }
    }

    inline ComboState& St(Player* p) { return States()[p->GetGUID().GetCounter()]; }

    inline void InvalidatePlayerRuntime(Player* player, bool eraseState)
    {
        if (!player)
            return;
        uint32 low = player->GetGUID().GetCounter();
        auto it = States().find(low);
        if (it != States().end())
        {
            it->second.on = false;
            ++it->second.generation;
            ++it->second.buffGeneration;
            it->second.maintainTargets.clear();
            it->second.buffQueueUntil = 0;
            if (eraseState)
                States().erase(it);
        }
        PlanCaches().erase(low);
        ClearAllFails(player);
    }

    // ==================================================================
    //  F44：玩家 + NPCBot统一友方成员与稳定维护目标
    // ==================================================================
    template <class Fn>
    inline void ForEachFriendlyGroupUnit(Player* player, Fn&& fn)
    {
        if (!player)
            return;

        Group* group = player->GetGroup();
        bool sawSelf = false;

        auto visit = [&](Unit* unit)
        {
            if (!unit || !player->IsFriendlyTo(unit))
                return;
            if (unit == player)
                sawSelf = true;

            bool mainTank = group &&
                (group->GetMemberFlags(unit->GetGUID()) & MEMBER_FLAG_MAINTANK) != 0;
            bool tankish = mainTank;
            uint8 cls = unit->GetClass();
            if (!tankish && (cls == CLASS_WARRIOR || cls == CLASS_PALADIN ||
                             cls == CLASS_DEATH_KNIGHT || cls == CLASS_DRUID))
                tankish = unit->GetMaxHealth() > player->GetMaxHealth() * 12 / 10;

            fn(unit, tankish, mainTank);
        };

        if (group)
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
                if (Player* member = itr->GetSource())
                    visit(member);

            // NPCBot是Creature，不会出现在GroupReference<Player>中。
            for (GroupBotReference* itr = group->GetFirstBotMember(); itr; itr = itr->next())
                if (Creature* bot = itr->GetSource())
                    visit(bot);
        }

        if (!sawSelf)
            visit(player);
    }

    inline bool IsLegalFriendlyGroupTarget(Player* player, Unit* target, float maxDist)
    {
        if (!player || !target || !target->IsAlive() || !target->IsInMap(player))
            return false;
        if (!player->IsFriendlyTo(target) || player->GetExactDist2d(target) > maxDist)
            return false;
        if (!player->IsWithinLOSInMap(target))
            return false;

        bool found = false;
        ForEachFriendlyGroupUnit(player, [&](Unit* unit, bool, bool)
        {
            if (unit == target)
                found = true;
        });
        return found;
    }

    inline Unit* FindMaintainedTarget(Player* player, uint32 rank1, float maxDist)
    {
        ComboState& state = St(player);
        auto saved = state.maintainTargets.find(rank1);
        if (saved != state.maintainTargets.end())
        {
            if (Unit* current = ObjectAccessor::GetUnit(*player, saved->second))
                if (IsLegalFriendlyGroupTarget(player, current, maxDist))
                    return current;
            state.maintainTargets.erase(saved);
        }

        // 重启.combo后先恢复真实已有Aura，避免把现有道标/盾迁走。
        Unit* auraTarget = nullptr;
        ForEachFriendlyGroupUnit(player, [&](Unit* unit, bool, bool)
        {
            if (!auraTarget && IsLegalFriendlyGroupTarget(player, unit, maxDist) &&
                HasAnyRankAura(unit, rank1, player->GetGUID()))
                auraTarget = unit;
        });
        if (auraTarget)
        {
            state.maintainTargets[rank1] = auraTarget->GetGUID();
            return auraTarget;
        }

        Unit* best = nullptr;
        uint8 bestTier = 0;
        uint32 bestHealth = 0;
        ForEachFriendlyGroupUnit(player, [&](Unit* unit, bool tankish, bool mainTank)
        {
            if (!IsLegalFriendlyGroupTarget(player, unit, maxDist))
                return;

            uint8 tier = mainTank ? 3 : (tankish ? 2 : (unit == player ? 0 : 1));
            if (!best || tier > bestTier ||
                (tier == bestTier && unit->GetMaxHealth() > bestHealth))
            {
                best = unit;
                bestTier = tier;
                bestHealth = unit->GetMaxHealth();
            }
        });
        return best ? best : player;
    }

    inline void CommitMaintainedTarget(Player* player, uint32 rank1, Unit* target)
    {
        if (player && target)
            St(player).maintainTargets[rank1] = target->GetGUID();
    }

    inline void CollectBuffTargets(Player* player, CombatSpec::Skill const& skill,
                                   float maxDist, std::vector<Unit*>& out)
    {
        out.clear();
        ObjectGuid caster = (skill.flags & CombatSpec::SF_MAINTAIN_FRIEND)
            ? player->GetGUID() : ObjectGuid::Empty;

        auto needsRefresh = [&](Unit* unit) -> bool
        {
            int32 remain = AuraRemainMs(unit, skill.spell, caster);
            int32 refreshWindow = (skill.flags & CombatSpec::SF_MAINTAIN_FRIEND)
                ? 10000 : 300000;
            return remain == 0 || (remain > 0 && remain <= refreshWindow);
        };

        if (skill.flags & CombatSpec::SF_MAINTAIN_FRIEND)
        {
            if (Unit* target = FindMaintainedTarget(player, skill.spell, maxDist))
                if (needsRefresh(target))
                    out.push_back(target);
            return;
        }

        if (skill.flags & CombatSpec::SF_RAID_BUFF)
        {
            ForEachFriendlyGroupUnit(player, [&](Unit* unit, bool, bool)
            {
                if (IsLegalFriendlyGroupTarget(player, unit, maxDist) && needsRefresh(unit))
                    out.push_back(unit);
            });
            return;
        }

        if (skill.flags & CombatSpec::SF_SELF)
            if (needsRefresh(player))
                out.push_back(player);
    }

    // ==================================================================
    //  v3：场景自动识别
    // ==================================================================
    /*
     * 已核实 API：
     *   WorldObject.h:467  GetMap()                 public
     *   Map.h:446 IsDungeon() / :448 IsRaid() / :450 IsHeroic()
     *   Map.h:451 Is25ManRaid() / :452 IsBattleground()   全部 public
     *   Player.h:2155 GetGroup()                    public
     *   Group.h:224 isRaidGroup() / :259 GetMembersCount()
     *
     * 判定顺序（从严到松）：
     *   英雄团本/25人团 -> 高级团本
     *   普通团本        -> 团本
     *   英雄5人本       -> 高级副本（也归 MYTHIC，打法一样保守）
     *   普通5人本       -> 副本
     *   野外 + 组队      -> 聚怪刷材料
     *   野外 + 单人      -> 做任务
     */
    inline uint8 DetectScene(Player* player)
    {
        Map* map = player->GetMap();
        if (!map)
            return CombatSpec::SCENE_QUEST;

        if (map->IsBattleground())
            return CombatSpec::SCENE_DUNGEON;   // 战场按副本强度打

        if (map->IsRaid())
        {
            if (map->IsHeroic() || map->Is25ManRaid())
                return CombatSpec::SCENE_MYTHIC;
            return CombatSpec::SCENE_RAID;
        }

        if (map->IsDungeon())
        {
            if (map->IsHeroic())
                return CombatSpec::SCENE_MYTHIC;
            return CombatSpec::SCENE_DUNGEON;
        }

        // 野外：组队了就当刷材料（会聚怪），单人当做任务
        if (Group* g = player->GetGroup())
            if (g->GetMembersCount() > 1)
                return CombatSpec::SCENE_FARM;

        return CombatSpec::SCENE_QUEST;
    }

    // 取实际生效的场景（AUTO 就现算）
    inline uint8 EffectiveScene(Player* player, uint8 stored)
    {
        return (stored == CombatSpec::SCENE_AUTO) ? DetectScene(player) : stored;
    }

    // ==================================================================
    //  v3：治疗目标选择
    // ==================================================================
    struct HealTarget
    {
        Unit*  unit    = nullptr;
        float  hpPct   = 100.0f;
        bool   isTank  = false;
        uint32 hurtCnt = 0;      // 团队里有几个人受伤（决定要不要群奶）
    };

    /*
     * 扫描队伍/团队，挑最该奶的人。
     *
     * 优先级：
     *   1. 血最少的
     *   2. 同血量时坦克优先（坦克死了团就灭）
     *   3. 没组队就看自己和当前选中的友方
     *
     * 已核实：Group.h:257 GetFirstMember() / GroupReference.h:36 next()
     *         Unit.h:922 GetHealthPct()  public
     *         WorldObject.h:413 IsInMap() public
     */
    inline void PickHealTarget(Player* player, float maxDist, HealTarget& out)
    {
        out.unit = nullptr; out.hpPct = 101.0f; out.isTank = false; out.hurtCnt = 0;

        ForEachFriendlyGroupUnit(player, [&](Unit* unit, bool tankish, bool)
        {
            if (!IsLegalFriendlyGroupTarget(player, unit, maxDist))
                return;

            float pct = unit->GetHealthPct();
            if (pct < 95.0f)
                ++out.hurtCnt;

            // 坦克在同血量下优先：给它5%的虚拟加权。
            float weighted = tankish ? pct - 5.0f : pct;
            float current = out.isTank ? out.hpPct - 5.0f : out.hpPct;
            if (!out.unit || weighted < current)
            {
                out.unit = unit;
                out.hpPct = pct;
                out.isTank = tankish;
            }
        });
    }

    // ==================================================================
    //  v3：找一个身上有可驱散负面的队友
    // ==================================================================
    /*
     * 已核实：Unit.h:1350 GetAppliedAuras()   public
     *         SpellAuras.h:75 GetBase() / :81 IsPositive()
     *         SpellInfo.h:316 Dispel  public
     *
     * 只驱散 4 种标准可驱散类型（DISPEL_ALL_MASK，SharedDefines.h:1481）：
     *   魔法 / 诅咒 / 疾病 / 中毒
     */
    inline Unit* FindDispelTarget(Player* player, uint32 dispelMask, float maxDist)
    {
        auto hasDispellable = [&](Unit* u) -> bool
        {
            if (!IsLegalFriendlyGroupTarget(player, u, maxDist))
                return false;

            for (auto const& kv : u->GetAppliedAuras())
            {
                AuraApplication const* app = kv.second;
                if (!app || app->IsPositive())
                    continue;

                Aura const* aura = app->GetBase();
                if (!aura)
                    continue;

                SpellInfo const* si = aura->GetSpellInfo();
                if (!si)
                    continue;

                if (si->Dispel && (dispelMask & (1 << si->Dispel)))
                    return true;
            }
            return false;
        };

        Unit* result = nullptr;
        ForEachFriendlyGroupUnit(player, [&](Unit* unit, bool, bool)
        {
            if (!result && hasDispellable(unit))
                result = unit;
        });
        return result;
    }

    // 该驱散法术能驱散哪些类型
    inline uint32 DispelMaskOfSpell(SpellInfo const* si)
    {
        if (!si)
            return 0;

        uint32 mask = 0;
        for (SpellEffectInfo const& eff : si->GetEffects())
        {
            if (eff.Effect == SPELL_EFFECT_DISPEL)
            {
                if (eff.MiscValue == DISPEL_ALL)
                    mask |= DISPEL_ALL_MASK;
                else if (eff.MiscValue > 0)
                    mask |= (1 << uint32(eff.MiscValue));
            }
        }
        return mask;
    }

    // ==================================================================
    //  v3.1：找最该打断的敌人（优先蓄力大招）
    // ==================================================================
    /*
     * 已核实 API：
     *   Unit.h:1488  GetCurrentSpell(CurrentSpellTypes)   public
     *   Spell.h:356  getState()                           public
     *   Spell.h:422  GetTimer()   剩余读条毫秒            public
     *   Spell.h:424  GetCastTime() 总读条毫秒             public
     *   Spell.h:456  GetSpellInfo()                       public
     *   Spell.h:132  SPELL_STATE_PREPARING = 1
     *   SpellInfo.h:420 IsTargetingArea()                 public
     *   SpellInfo.h:384 PreventionType                    public
     *   SharedDefines.h:1649 SPELL_PREVENTION_TYPE_SILENCE = 1
     */
    struct CastingEnemy
    {
        Unit*       unit      = nullptr;
        uint32      score     = 0;
        bool        isAoe     = false;
        char const* spellName = "?";
    };

    inline void FindBestInterruptTarget(Player* player, float radius, CastingEnemy& out)
    {
        out = CastingEnemy();

        std::list<Unit*> targets;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(player, player, radius);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck>
            searcher(player, targets, check);
        Cell::VisitAllObjects(player, searcher, radius);

        LocaleConstant loc = player->GetSession()
            ? player->GetSession()->GetSessionDbcLocale() : DEFAULT_LOCALE;

        for (Unit* u : targets)
        {
            if (!u || !u->IsAlive() || u->IsTotem() || u->IsCritter())
                continue;
            if (!player->IsValidAttackTarget(u))
                continue;

            // 只看"正在读条"的（引导类 CURRENT_CHANNELED_SPELL 打断意义不大，这里也一并看）
            Spell* sp = u->GetCurrentSpell(CURRENT_GENERIC_SPELL);
            if (!sp || sp->getState() != SPELL_STATE_PREPARING)
                continue;

            SpellInfo const* si = sp->GetSpellInfo();
            if (!si)
                continue;

            // 完全打不断的（PreventionType 不是沉默类）就别浪费打断技
            if (si->PreventionType != SPELL_PREVENTION_TYPE_SILENCE)
                continue;

            int32 remain = sp->GetTimer();          // 剩余毫秒
            int32 total  = sp->GetCastTime();       // 总读条毫秒

            // 快读完了（<0.3秒），打断多半来不及，跳过
            if (remain < 300)
                continue;

            /*
             * 打分：
             *   基础分 = 总读条时间（毫秒）—— 读条越长越是大招
             *   群体技 ×2      —— AOE 伤害更致命
             *   正在打我 +2000 —— 冲我来的优先
             *   剩余时间充裕 +1000 —— 打得中
             */
            uint32 score = uint32(total > 0 ? total : 1000);

            bool aoe = si->IsTargetingArea();
            if (aoe)
                score *= 2;

            if (u->GetVictim() == player)
                score += 2000;

            if (remain > 1000)
                score += 1000;

            if (score > out.score)
            {
                out.unit      = u;
                out.score     = score;
                out.isAoe     = aoe;
                out.spellName = (si->SpellName[loc] && *si->SpellName[loc])
                                ? si->SpellName[loc] : "未知法术";
            }
        }
    }

    // 汇总某职业所有专精的技能（全专精模式用）
    inline void CollectAllSpecSkills(uint8 cls,
                                     std::vector<CombatSpec::Skill>& rotation,
                                     std::vector<CombatSpec::Skill>& burst,
                                     std::vector<CombatSpec::Skill>& defensive,
                                     std::vector<CombatSpec::Skill>& buffs)
    {
        rotation.clear(); burst.clear(); defensive.clear(); buffs.clear();

        std::vector<CombatSpec::SpecInfo const*> specs;
        CombatSpec::GetSpecsOfClass(cls, specs);

        auto append = [](std::vector<CombatSpec::Skill>& dst,
                         std::vector<CombatSpec::Skill> const& src)
        {
            for (CombatSpec::Skill const& s : src)
            {
                bool dup = false;
                for (CombatSpec::Skill const& d : dst)
                    if (d.spell == s.spell && d.flags == s.flags)
                    {
                        dup = true;
                        break;
                    }
                if (!dup)
                    dst.push_back(s);
            }
        };

        for (CombatSpec::SpecInfo const* sp : specs)
        {
            append(rotation,  sp->rotation);
            append(burst,     sp->burst);
            append(defensive, sp->defensive);
            append(buffs,     sp->buffs);
        }
    }
}

class combathelper_commandscript : public CommandScript
{
public:
    combathelper_commandscript() : CommandScript("combathelper_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "set",   rbac::RBAC_PERM_COMMAND_COMBATHELPER, false, &HandleSetCommand,   "" },
            { "bar",   rbac::RBAC_PERM_COMMAND_COMBATHELPER, false, &HandleBarCommand,   "" },
            { "combo", rbac::RBAC_PERM_COMMAND_COMBATHELPER, false, &HandleComboCommand, "" },
            { "buff",  rbac::RBAC_PERM_COMMAND_COMBATHELPER, false, &HandleBuffCommand,  "" },
            { "setup", rbac::RBAC_PERM_COMMAND_COMBATHELPER, false, &HandleSetupCommand, "" },
        };
        return commandTable;
    }

    // ==================================================================
    //  .set —— 属性精调（v1 已实测通过，逻辑未动）
    // ==================================================================
    static bool HandleSetCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = Tokenize(args);
        if (tok.empty())
        {
            ShowSetHelp(handler);
            return true;
        }

        std::string const& sub = tok[0];
        if (sub == "show"   || sub == "查看")   return SetShow(handler, player);
        if (sub == "clear"  || sub == "清除")   return SetClear(handler, player);
        if (sub == "bosshp")                    return SetBossHp(handler, player, tok);
        if (sub == "allres" || sub == "全抗性") return SetAllRes(handler, player, tok);
        return SetRating(handler, player, tok);
    }

    static void ShowSetHelp(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ff00===== 属性精调 =====|r");
        handler->PSendSysMessage("|cffffff00.set crit 100|r      暴击锁到 100%%");
        handler->PSendSysMessage("|cffffff00.set hit 100|r       命中");
        handler->PSendSysMessage("|cffffff00.set haste 100|r     急速");
        handler->PSendSysMessage("|cffffff00.set expertise 50|r  精准");
        handler->PSendSysMessage("|cffffff00.set dodge 50|r      躲闪（招架/格挡/防御同理）");
        handler->PSendSysMessage("|cffffff00.set arp 100|r       护甲穿透");
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cffffff00.set allres 5000|r   全抗性");
        handler->PSendSysMessage("|cffffff00.set bosshp 50|r     选中目标血量降到 50%%");
        handler->PSendSysMessage("|cffffff00.set show|r          查看当前锁定");
        handler->PSendSysMessage("|cffffff00.set clear|r         清除所有锁定");
    }

    static std::unordered_map<uint32, std::unordered_map<int32, int32>>& Store()
    {
        static std::unordered_map<uint32, std::unordered_map<int32, int32>> s;
        return s;
    }

    static bool SetRating(ChatHandler* handler, Player* player,
                          std::vector<std::string> const& tok)
    {
        CombatHelper::RatingAlias const* ra = CombatHelper::FindRating(tok[0]);
        if (!ra)
        {
            handler->PSendSysMessage("|cffff0000不认识的属性「%s」。|r", tok[0].c_str());
            ShowSetHelp(handler);
            return true;
        }

        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.set %s <百分比>|r", tok[0].c_str());
            return true;
        }

        double pct = atof(tok[1].c_str());
        pct = std::clamp(pct, 0.0, 1000.0);

        std::vector<CombatRating> crs;
        CombatHelper::GetLinkedRatings(ra->cr, crs);

        std::ostringstream detail;
        for (CombatRating cr : crs)
        {
            float mult = player->GetRatingMultiplier(cr);
            if (mult <= 0.0f)
                continue;

            int32 want = int32(pct / double(mult));

            auto& mine = Store()[player->GetGUID().GetCounter()];
            auto it = mine.find(int32(cr));
            if (it != mine.end() && it->second != 0)
                player->ApplyRatingMod(cr, -it->second, true);

            player->ApplyRatingMod(cr, want, true);
            mine[int32(cr)] = want;

            /*
             * step20：持久化，重登后自动恢复。
             *
             * 放在内存修改【之后】—— 确保 ApplyRatingMod 真的生效了才写库，
             * 避免出现"库里有记录但当场没效果"的不一致。
             *
             * CustomStatPersist.h:86  Record(guidLow, type, index, amount)
             * CustomStatPersist.h:71  TYPE_RATING = 1
             */
            if (sCustomStatPersist->Enabled())
                sCustomStatPersist->Record(player->GetGUID().GetCounter(),
                    CustomStatPersistMgr::TYPE_RATING, uint8(cr), float(want));

            if (detail.tellp() > 0)
                detail << " / ";
            detail << want;
        }

        handler->PSendSysMessage("|cff00ff00[已锁定]|r %s = |cffffff00%.1f%%|r  |cff888888(rating %s)|r",
            ra->cn, pct, detail.str().c_str());

        if (crs.size() > 1)
            handler->PSendSysMessage("|cff888888近战/远程/法术已同步设置|r");
        return true;
    }

    static bool SetAllRes(ChatHandler* handler, Player* player,
                          std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.set allres <数值>|r");
            return true;
        }

        int64 v = atoll(tok[1].c_str());
        v = std::clamp<int64>(v, 0, 2100000000LL);

        for (int32 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
            player->SetResistance(SpellSchools(i), int32(v));

        handler->PSendSysMessage("|cff00ff00[已设置]|r 全抗性 = |cffffff00%lld|r", (long long)v);
        return true;
    }

    static bool SetBossHp(ChatHandler* handler, Player* player,
                          std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.set bosshp <百分比>|r");
            return true;
        }

        Unit* target = player->GetSelectedUnit();
        if (!target)
        {
            handler->PSendSysMessage("|cffff0000请先选中一个目标。|r");
            return true;
        }

        double pct = std::clamp(atof(tok[1].c_str()), 1.0, 100.0);
        uint32 maxHp = target->GetMaxHealth();
        uint32 newHp = std::max<uint32>(1, uint32(double(maxHp) * pct / 100.0));
        target->SetHealth(newHp);

        handler->PSendSysMessage("|cff00ff00[已设置]|r %s 血量 -> |cffffff00%.0f%%|r (%u / %u)",
            target->GetName().c_str(), pct, newHp, maxHp);
        return true;
    }

    static bool SetShow(ChatHandler* handler, Player* player)
    {
        auto& mine = Store()[player->GetGUID().GetCounter()];
        handler->PSendSysMessage("|cff00ff00===== 当前锁定 =====|r");

        if (mine.empty())
        {
            handler->PSendSysMessage("|cffff8000没有任何锁定。|r");
            return true;
        }

        for (auto const& kv : mine)
        {
            if (!kv.second)
                continue;
            CombatRating cr = CombatRating(kv.first);
            float mult = player->GetRatingMultiplier(cr);
            handler->PSendSysMessage("  CR_%d  rating |cffffff00%d|r  = |cff00ff00%.1f%%|r",
                kv.first, kv.second, double(mult) * double(kv.second));
        }
        return true;
    }

    static bool SetClear(ChatHandler* handler, Player* player)
    {
        auto& mine = Store()[player->GetGUID().GetCounter()];
        uint32 n = 0;
        for (auto& kv : mine)
        {
            if (!kv.second)
                continue;
            player->ApplyRatingMod(CombatRating(kv.first), -kv.second, true);

            /*
             * step20 关键：必须同步清库！
             *
             * 否则 .set clear 只清了内存，库里记录还在 ——
             * 当场看着是清了，【重登又全冒出来】。
             * Record(..., 0) 表示删除该项（CustomStatPersist.h:86 注释）。
             */
            if (sCustomStatPersist->Enabled())
                sCustomStatPersist->Record(player->GetGUID().GetCounter(),
                    CustomStatPersistMgr::TYPE_RATING, uint8(kv.first), 0.0f);

            kv.second = 0;
            ++n;
        }
        mine.clear();
        handler->PSendSysMessage("|cff00ff00[已清除]|r %u 项锁定。", n);
        return true;
    }

    // ==================================================================
    //  .bar —— 智能配栏
    // ==================================================================
    static bool HandleBarCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = Tokenize(args);

        if (!tok.empty())
        {
            if (tok[0] == "clear" || tok[0] == "清空")
                return BarClear(handler, player);

            if (tok[0] == "auto" || tok[0] == "自动")
            {
                uint8 spec = CombatHelper::DetectSpec(player);
                return BarApply(handler, player, spec, false, CombatHelper::St(player).role);
            }

            if (tok[0] == "all" || tok[0] == "全部")
                return BarApply(handler, player, 0, true, CombatHelper::St(player).role);
        }

        // 无参数 -> 弹专精菜单
        ShowSpecMenu(player, CombatHelper::SENDER_BAR_SPEC, "智能配栏");
        return true;
    }

    // ------------------------------------------------------------------
    //  专精选择菜单（.bar 和 .combo 共用）
    // ------------------------------------------------------------------
    /*
     * 菜单项数 = 专精数(3或4) + 全专精(1) + 关闭(1) = 最多 6 项，
     * 远低于 GossipDef.h:30 的 GOSSIP_MAX_MENU_ITEMS=32，不会触发 ASSERT。
     */
    static void ShowSpecMenu(Player* player, uint32 sender, char const* title)
    {
        std::vector<CombatSpec::SpecInfo const*> specs;
        CombatSpec::GetSpecsOfClass(player->GetClass(), specs);

        if (specs.empty())
        {
            ChatHandler(player->GetSession())
                .PSendSysMessage("|cffff0000你的职业暂无方案。|r");
            return;
        }

        uint8 detected = CombatHelper::DetectSpec(player);

        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        for (CombatSpec::SpecInfo const* sp : specs)
        {
            std::ostringstream label;
            label << "【" << sp->name << "】" << sp->role;
            if (sp->specIdx == detected)
                label << "  |cff00ff00<当前天赋>|r";

            menu.AddMenuItem(-1, GOSSIP_ICON_BATTLE, label.str(),
                             sender, sp->specIdx, "", 0, false);
        }

        menu.AddMenuItem(-1, GOSSIP_ICON_TRAINER,
                         "【全专精】GM3 全天赋 - 所有技能混合",
                         sender, CombatHelper::ACTION_ALL_SPECS, "", 0, false);

        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888关闭|r",
                         CombatHelper::SENDER_NAV, CombatHelper::NAV_CLOSE, "", 0, false);

        // MiscHandler.cpp:150 有 guid.IsPlayer() 分支，用自己的 GUID 合法
        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());

        ChatHandler(player->GetSession())
            .PSendSysMessage("|cff00ff00[%s]|r 第 1 步：选专精。", title);
    }

    // ------------------------------------------------------------------
    //  第 2 步：选队伍职责
    // ------------------------------------------------------------------
    /*
     * 用户明确提过「系统有时候会选错」，所以这里：
     *   · 三个职责【全部列出来】，随时可改
     *   · 系统推荐的那个标 <推荐>
     *   · 上次选过的标 <上次>
     *   · 选完记住（存在 ComboState.role）
     */
    static void ShowRoleMenu(Player* player, uint32 sender, uint8 specIdx, bool allSpecs)
    {
        CombatHelper::ComboState& st = CombatHelper::St(player);
        st.pendingSpec = specIdx;
        st.pendingAll  = allSpecs;

        uint8 suggested = allSpecs
            ? CombatSpec::SuggestRole(player->GetClass(), CombatHelper::DetectSpec(player))
            : CombatSpec::SuggestRole(player->GetClass(), specIdx);

        char const* specName = "全专精";
        if (!allSpecs)
            if (CombatSpec::SpecInfo const* sp = CombatSpec::GetSpec(player->GetClass(), specIdx))
                specName = sp->name;

        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        struct RoleOpt { uint8 role; char const* label; char const* desc; };
        static RoleOpt const opts[] =
        {
            { CombatSpec::ROLE_TANK,   "坦克", "嘲讽拉怪 + 减伤自保优先" },
            { CombatSpec::ROLE_DPS,    "输出", "纯伤害循环 + 爆发最大化" },
            { CombatSpec::ROLE_HEALER, "治疗", "奶队友 + 保自己 + 群疗" },
        };

        for (RoleOpt const& o : opts)
        {
            std::ostringstream label;
            label << "【" << o.label << "】" << o.desc;
            if (o.role == suggested)
                label << "  |cff00ff00<推荐>|r";
            if (o.role == st.role && st.role != CombatSpec::ROLE_AUTO)
                label << "  |cffffff00<上次>|r";

            menu.AddMenuItem(-1, GOSSIP_ICON_BATTLE, label.str(),
                             sender, o.role, "", 0, false);
        }

        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888返回上一步|r",
                         CombatHelper::SENDER_NAV, CombatHelper::NAV_BACK, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888关闭|r",
                         CombatHelper::SENDER_NAV, CombatHelper::NAV_CLOSE, "", 0, false);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());

        ChatHandler(player->GetSession())
            .PSendSysMessage("|cff00ff00[%s]|r 第 2 步：选你在队伍里的职责。", specName);
    }

    // ------------------------------------------------------------------
    //  场景菜单
    // ------------------------------------------------------------------
    static void ShowSceneMenu(Player* player)
    {
        CombatHelper::ComboState& st = CombatHelper::St(player);
        uint8 detected = CombatHelper::DetectScene(player);

        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        // 自动识别选项
        {
            std::ostringstream label;
            label << "【自动识别】当前判定为：" << CombatSpec::SceneName(detected);
            if (st.scene == CombatSpec::SCENE_AUTO)
                label << "  |cff00ff00<使用中>|r";
            menu.AddMenuItem(-1, GOSSIP_ICON_TRAINER, label.str(),
                             CombatHelper::SENDER_SCENE, CombatSpec::SCENE_AUTO, "", 0, false);
        }

        for (uint8 i = CombatSpec::SCENE_QUEST; i < CombatSpec::SCENE_MAX; ++i)
        {
            CombatSpec::SceneTuning const& t = CombatSpec::GetTuning(i);
            std::ostringstream label;
            label << "【" << CombatSpec::SceneName(i) << "】";
            if (st.scene == i)
                label << "  |cff00ff00<使用中>|r";

            menu.AddMenuItem(-1, GOSSIP_ICON_BATTLE, label.str(),
                             CombatHelper::SENDER_SCENE, i, t.desc, 0, false);
        }

        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888关闭|r",
                         CombatHelper::SENDER_NAV, CombatHelper::NAV_CLOSE, "", 0, false);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }

    // ------------------------------------------------------------------
    //  真正配栏
    // ------------------------------------------------------------------
    static bool BarApply(ChatHandler* handler, Player* player, uint8 specIdx,
                         bool allSpecs, uint8 role)
    {
        if (role == CombatSpec::ROLE_AUTO)
            role = CombatSpec::SuggestRole(player->GetClass(), specIdx);

        uint8 scene = CombatHelper::EffectiveScene(player, CombatHelper::St(player).scene);

        CombatSpec::BuiltPlan plan;
        CombatSpec::BuildPlan(player->GetClass(), specIdx, role, scene, allSpecs, plan);

        if (plan.core.empty() && plan.opener.empty())
        {
            handler->PSendSysMessage("|cffff0000找不到该专精方案。|r");
            return true;
        }

        /*
         * 配栏分区（按职责调整含义）：
         *   第1区 主循环   —— 坦克是"拉怪+输出"，治疗是"治疗循环"，输出是"伤害循环"
         *   第2区 爆发
         *   第3区 保命/紧急
         *   第4区 增益 + 功能技（驱散/打断/回蓝）
         */
        std::vector<CombatSpec::Skill> zone4 = plan.opener;
        for (CombatSpec::Skill const& s : plan.utility)
        {
            bool dup = false;
            for (CombatSpec::Skill const& d : zone4)
                if (d.spell == s.spell) { dup = true; break; }
            if (!dup)
                zone4.push_back(s);
        }

        struct Zone
        {
            uint8 start;
            uint8 cap;                  // v3.5：每区容量可以不一样
            std::vector<CombatSpec::Skill> const* list;
            char const* name;
        };

        Zone zones[] =
        {
            { CombatHelper::BAR_ROTATION,  CombatHelper::BAR_ROTATION_SLOTS,
              &plan.core,      "主循环"   },
            { CombatHelper::BAR_BURST,     CombatHelper::BAR_SLOTS_PER,
              &plan.burst,     "爆发"     },
            { CombatHelper::BAR_DEFENSIVE, CombatHelper::BAR_SLOTS_PER,
              &plan.emergency, "保命/紧急" },
            { CombatHelper::BAR_BUFF,      CombatHelper::BAR_SLOTS_PER,
              &zone4,          "增益/功能" },
        };

        // 先清掉所有用到的格子，避免旧配置残留
        for (uint8 i = 0; i < CombatHelper::BAR_TOTAL_SLOTS; ++i)
            player->removeActionButton(i);

        uint32 placed = 0, notLearned = 0, truncated = 0;

        /*
         * v3.5：同一个技能不重复占格。
         * core 里可能同时含 rotation 和 filler 的相同技能，
         * 之前会在不同区各占一格，白白浪费。
         */
        std::vector<uint32> usedSpells;

        for (Zone const& z : zones)
        {
            uint8 slot = z.start;
            uint32 zoneCount = 0;

            for (CombatSpec::Skill const& sk : *z.list)
            {
                if (slot >= z.start + z.cap)
                {
                    ++truncated;        // 记下放不下的，最后提示玩家
                    continue;
                }

                // 解析成玩家实际会的那一阶
                uint32 real = CombatHelper::ResolveRank(player, sk.spell);
                if (!real)
                {
                    ++notLearned;
                    continue;
                }

                // 已经在别的格子放过了就跳过
                bool dup = false;
                for (uint32 u : usedSpells)
                    if (u == real) { dup = true; break; }
                if (dup)
                    continue;

                player->addActionButton(slot, real, ACTION_BUTTON_SPELL);
                usedSpells.push_back(real);
                ++slot; ++placed; ++zoneCount;
            }

            if (zoneCount)
                handler->PSendSysMessage("|cff00ccff[%s]|r 槽位 %u-%u 放了 %u 个",
                    z.name, uint32(z.start) + 1, uint32(z.start + zoneCount), zoneCount);
        }

        /*
         * 关键：addActionButton()（Player.cpp:6190）只改服务端 m_actionButtons，
         * 【从不发包】。必须调 SendActionButtons(1)（Player.cpp:6123）推给客户端，
         * 否则就是"提示成功但界面没变"。
         */
        player->SendActionButtons(1);

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00[配栏完成]|r %s · %s · %s，共 |cffffff00%u|r 个技能",
            plan.specName, CombatSpec::RoleName(role), CombatSpec::SceneName(scene), placed);

        if (notLearned)
            handler->PSendSysMessage("|cff888888跳过 %u 个（还没学会 / 该天赋没点）|r", notLearned);

        if (truncated)
            handler->PSendSysMessage("|cffff8000有 %u 个技能放不下（该区已满）|r", truncated);

        /*
         * 槽位对应的客户端界面（wowpedia Action slot，已核实）：
         *   0-11  主界面第1排
         *   12-23 主界面第2页（Shift+翻页 或 设置里开"额外动作条"）
         *   24-35 右侧竖排1
         *   36-47 右侧竖排2
         *   48-59 底部右侧条
         */
        handler->PSendSysMessage("|cffff8000第1排+第2页=主循环  右侧栏1=爆发  右侧栏2=保命|r");
        handler->PSendSysMessage("|cffff8000底部右侧条=增益/功能（驱散·打断·回蓝）|r");
        handler->PSendSysMessage("|cff888888界面-操作 里打开全部动作条才看得到后面几排|r");
        return true;
    }

    static bool BarClear(ChatHandler* handler, Player* player)
    {
        /*
         * 必须用 removeActionButton。
         * addActionButton(i, 0, ...) 会被 IsActionButtonDataValid()（Player.cpp:6136）
         * 拒绝（法术 0 不存在），结果什么都没清掉。
         */
        for (uint8 i = 0; i < CombatHelper::BAR_TOTAL_SLOTS; ++i)
            player->removeActionButton(i);

        player->SendActionButtons(1);
        handler->PSendSysMessage("|cff00ff00[已清空]|r 前 %u 个槽位。",
            uint32(CombatHelper::BAR_TOTAL_SLOTS));
        return true;
    }

    // ==================================================================
    //  .combo —— 自动连招
    // ==================================================================
    static bool HandleComboCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = Tokenize(args);
        CombatHelper::ComboState& st = CombatHelper::St(player);

        if (!tok.empty())
        {
            if (tok[0] == "off" || tok[0] == "关" || tok[0] == "stop")
            {
                uint32 casts = st.castCount;
                CombatHelper::InvalidatePlayerRuntime(player, false);
                handler->PSendSysMessage("|cffff8000[连招]|r 已关闭。共施放 %u 次。", casts);
                return true;
            }

            if (tok[0] == "list" || tok[0] == "列表")
                return ComboList(handler, player);
        }

        // 已开着 -> 关掉（开关切换）
        if (st.on)
        {
            uint32 casts = st.castCount;
            CombatHelper::InvalidatePlayerRuntime(player, false);
            handler->PSendSysMessage("|cffff8000[连招]|r 已关闭。共施放 %u 次。", casts);
            return true;
        }

        // 没开 -> 弹菜单选专精
        ShowSpecMenu(player, CombatHelper::SENDER_COMBO_SPEC, "自动连招");
        return true;
    }

    static bool ComboList(ChatHandler* handler, Player* player)
    {
        /*
         * v3.4：改成真正的【诊断工具】。
         *
         * 用户报「猎人只有普通攻击、法师只有火焰冲击」时，
         * 光看列表看不出为什么，得逐条说明"这个技能现在为什么放不出来"。
         * 现在会打印每个技能的实时状态：未学会 / CD / 太远 / 太近 /
         * 被退避 / 需站定 / 缺资源 / 就绪。
         */
        CombatHelper::ComboState& st = CombatHelper::St(player);

        uint8 role = st.role;
        if (role == CombatSpec::ROLE_AUTO)
            role = CombatSpec::SuggestRole(player->GetClass(), st.specIdx);

        uint8 scene = CombatHelper::EffectiveScene(player, st.scene);
        CombatSpec::SceneTuning const& tune = CombatSpec::GetTuning(scene);

        CombatSpec::BuiltPlan plan;
        CombatSpec::BuildPlan(player->GetClass(), st.specIdx, role, scene,
                              st.allSpecs, plan);

        if (plan.core.empty())
        {
            handler->PSendSysMessage("|cffff0000还没选专精，先输入 |r|cffffff00.combo|r");
            return true;
        }

        Unit*  target = player->GetSelectedUnit();
        uint32 nearby = CountNearbyEnemies(player, 10.0f);

        handler->PSendSysMessage("|cff00ff00===== %s · %s · %s =====|r",
            plan.specName, CombatSpec::RoleName(role), CombatSpec::SceneName(scene));
        handler->PSendSysMessage("|cff888888周围敌人 %u 个 / AOE门槛 %u / 目标：%s|r",
            nearby, tune.aoeThreshold,
            target ? target->GetName().c_str() : "无");
        handler->PSendSysMessage(" ");

        uint32 idx = 0, readyCnt = 0;
        for (CombatSpec::Skill const& sk : plan.core)
        {
            if (++idx > 20)      // 最多列 20 条，避免刷屏
                break;

            uint32 real = CombatHelper::ResolveRank(player, sk.spell);
            if (!real)
            {
                handler->PSendSysMessage("  %2u. %-12s |cff888888未学会|r", idx, sk.cn);
                continue;
            }

            SpellInfo const* si = sSpellMgr->GetSpellInfo(real);
            if (!si)
            {
                handler->PSendSysMessage("  %2u. %-12s |cffff0000法术不存在|r", idx, sk.cn);
                continue;
            }

            // 逐条给出"为什么放不出来"
            char const* why = nullptr;

            if (CombatHelper::IsBackedOff(player, real))
                why = "|cffff8000暂时跳过(刚失败过)|r";
            else if (!player->GetSpellHistory()->IsReady(si))
                why = "|cffff8000冷却中|r";
            else if (player->GetSpellHistory()->HasGlobalCooldown(si))
                why = "|cff888888公共CD|r";
            else if ((sk.flags & CombatSpec::SF_NO_MOVE) && player->isMoving())
                why = "|cff8888ff需站定|r";
            else if ((sk.flags & CombatSpec::SF_AOE) && nearby < tune.aoeThreshold)
                why = "|cff888888怪不够(AOE)|r";
            else if (target && !(sk.flags & CombatSpec::SF_SELF))
            {
                float dist   = player->GetExactDist2d(target);
                float maxRng = si->GetMaxRange(false);
                float minRng = si->GetMinRange(false);

                if ((sk.flags & CombatSpec::SF_MELEE) && !player->IsWithinMeleeRange(target))
                    why = "|cffff8000太远(近战)|r";
                else if (maxRng > 0.0f && dist > maxRng)
                    why = "|cffff8000超出射程|r";
                else if (minRng > 0.0f && dist < minRng)
                    why = "|cffff0000太近(死亡区)|r";
            }

            if (!why)
            {
                why = "|cff00ff00就绪|r";
                ++readyCnt;
            }

            // 标记技能类型
            std::string tag;
            if (sk.flags & CombatSpec::SF_HEAL || sk.flags & CombatSpec::SF_HEAL_AOE ||
                sk.flags & CombatSpec::SF_HOT  || sk.flags & CombatSpec::SF_HEAL_EMERG)
                tag = " |cff00ccff[治疗]|r";
            else if (sk.flags & (CombatSpec::SF_TAUNT | CombatSpec::SF_TAUNT_AOE))
                tag = " |cffffcc00[拉怪]|r";
            else if (sk.flags & CombatSpec::SF_AOE)
                tag = " |cffff8800[AOE]|r";
            else if (sk.flags & CombatSpec::SF_EXECUTE)
                tag = " |cffff0000[斩杀]|r";
            else if (sk.flags & CombatSpec::SF_NO_MOVE)
                tag = " |cff8888ff[读条]|r";

            handler->PSendSysMessage("  %2u. %-12s %s%s", idx, sk.cn, why, tag.c_str());
        }

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00当前可放 %u 个|r  |cff888888(主循环共 %u 个技能)|r",
            readyCnt, uint32(plan.core.size()));

        if (!readyCnt)
            handler->PSendSysMessage("|cffff0000一个都放不出来？检查：有没有选中目标、距离、武器/弹药|r");

        return true;
    }

    // ------------------------------------------------------------------
    //  开启连招
    // ------------------------------------------------------------------
    static void ComboStart(Player* player, uint8 specIdx, bool allSpecs, uint8 role)
    {
        // 先使旧事件链/缓存/稳定目标全部失效，再建立新generation。
        CombatHelper::InvalidatePlayerRuntime(player, false);
        CombatHelper::ComboState& st = CombatHelper::St(player);
        ++st.generation;
        st.on         = true;
        st.specIdx    = specIdx;
        st.allSpecs   = allSpecs;
        st.role       = role;
        st.warnedMove = false;
        st.castCount  = 0;
        st.healCount  = 0;
        st.dispelCount= 0;

        // 重开连招时清掉旧的失败记录，给之前放不出的技能一次新机会
        CombatHelper::ClearAllFails(player);

        ChatHandler handler(player->GetSession());

        if (role == CombatSpec::ROLE_AUTO)
            role = CombatSpec::SuggestRole(player->GetClass(), specIdx);

        uint8 scene = CombatHelper::EffectiveScene(player, st.scene);
        CombatSpec::SceneTuning const& t = CombatSpec::GetTuning(scene);

        char const* nm = "全专精";
        if (!allSpecs)
            if (CombatSpec::SpecInfo const* sp = CombatSpec::GetSpec(player->GetClass(), specIdx))
                nm = sp->name;

        handler.PSendSysMessage("|cff00ff00[连招已开启]|r %s · |cffffff00%s|r · %s",
            nm, CombatSpec::RoleName(role), CombatSpec::SceneName(scene));

        // 按职责给出不同说明，让玩家知道现在是什么打法
        switch (role)
        {
            case CombatSpec::ROLE_TANK:
                handler.PSendSysMessage("|cff888888坦克模式：优先嘲讽拉怪、打断、开减伤，血少自动自保|r");
                break;
            case CombatSpec::ROLE_HEALER:
                handler.PSendSysMessage("|cff888888治疗模式：自动扫全团，谁血少奶谁，坦克优先|r");
                handler.PSendSysMessage("|cff888888多人受伤自动切群疗，血线极低用救命大招|r");
                break;
            default:
                handler.PSendSysMessage("|cff888888输出模式：纯伤害循环，爆发自动开|r");
                break;
        }

        handler.PSendSysMessage("|cff888888AOE门槛 %u 个怪 / 保命线 %u%%%% / 救人线 %u%%%%|r",
            t.aoeThreshold, t.defensiveHpPct, t.emergencyPct);
        handler.PSendSysMessage("|cff8888ff读条技会等站定再放，移动中自动跳过不断连|r");
        handler.PSendSysMessage("|cffffff00再输入一次 .combo 关闭|r");

        ScheduleTick(player, st.generation);
        ComboTick(player);          // 立刻来一发，不用等
    }

    // ==================================================================
    //  .buff —— 自动补增益（独立指令，也可单独用）
    // ==================================================================
    static bool HandleBuffCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = Tokenize(args);
        CombatHelper::ComboState& st = CombatHelper::St(player);

        if (!tok.empty())
        {
            if (tok[0] == "now" || tok[0] == "立刻")
                return BuffNow(handler, player);

            if (tok[0] == "on"  || tok[0] == "开")
            {
                st.autoBuff = true;
                handler->PSendSysMessage("|cff00ff00[自动增益]|r 已开启（连招运行时自动补）");
                return true;
            }
            if (tok[0] == "off" || tok[0] == "关")
            {
                st.autoBuff = false;
                handler->PSendSysMessage("|cffff8000[自动增益]|r 已关闭");
                return true;
            }
            if (tok[0] == "dispel")
            {
                st.autoDispel = !st.autoDispel;
                handler->PSendSysMessage("|cff00ff00[自动驱散]|r %s",
                    st.autoDispel ? "已开启" : "已关闭");
                return true;
            }
            if (tok[0] == "scene" || tok[0] == "场景")
            {
                ShowSceneMenu(player);
                return true;
            }
        }

        ShowBuffMenu(player);
        return true;
    }

    static void ShowBuffMenu(Player* player)
    {
        CombatHelper::ComboState& st = CombatHelper::St(player);
        uint8 scene = CombatHelper::EffectiveScene(player, st.scene);

        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        menu.AddMenuItem(-1, GOSSIP_ICON_TRAINER, "【立刻补满增益】把该上的 buff 全开一遍",
                         CombatHelper::SENDER_BUFF, 1, "", 0, false);

        {
            std::ostringstream l;
            l << "【自动补增益】" << (st.autoBuff ? "|cff00ff00[开]|r" : "|cffff0000[关]|r")
              << " 连招时自动维持";
            menu.AddMenuItem(-1, GOSSIP_ICON_BATTLE, l.str(),
                             CombatHelper::SENDER_BUFF, 2, "", 0, false);
        }
        {
            std::ostringstream l;
            l << "【自动驱散解控】" << (st.autoDispel ? "|cff00ff00[开]|r" : "|cffff0000[关]|r")
              << " 解毒/疫病/魔法/诅咒";
            menu.AddMenuItem(-1, GOSSIP_ICON_BATTLE, l.str(),
                             CombatHelper::SENDER_BUFF, 3, "", 0, false);
        }
        {
            std::ostringstream l;
            l << "【战斗场景】当前：" << CombatSpec::SceneName(scene);
            menu.AddMenuItem(-1, GOSSIP_ICON_TRAINER, l.str(),
                             CombatHelper::SENDER_BUFF, 4, "", 0, false);
        }

        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888关闭|r",
                         CombatHelper::SENDER_NAV, CombatHelper::NAV_CLOSE, "", 0, false);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }

    static void BuffMenuAction(ChatHandler* handler, Player* player, uint32 action)
    {
        CombatHelper::ComboState& st = CombatHelper::St(player);
        switch (action)
        {
            case 1: BuffNow(handler, player); break;
            case 2:
                st.autoBuff = !st.autoBuff;
                handler->PSendSysMessage("|cff00ff00[自动增益]|r %s",
                    st.autoBuff ? "已开启" : "已关闭");
                break;
            case 3:
                st.autoDispel = !st.autoDispel;
                handler->PSendSysMessage("|cff00ff00[自动驱散]|r %s",
                    st.autoDispel ? "已开启" : "已关闭");
                break;
            case 4: ShowSceneMenu(player); break;
            default: break;
        }
    }

    // ==================================================================
    //  .setup —— 一键开荒（v4）
    // ==================================================================
    /*
     * 设计原则（用户明确要求）：
     *   「.setup 一定是一键使用的，不然还是菜单就背道而驰了」
     *
     * 所以：
     *   .setup          -> 【立刻执行】，不弹任何窗口
     *   .setup menu     -> 才弹开关配置窗口（配完就记住）
     *
     * 执行顺序有讲究：
     *   1. 发装备并穿上   —— 必须最先，属性/技能都依赖装备
     *   2. 配快捷栏       —— 装备穿好后再配，图标才不会是灰的
     *   3. 补增益         —— 排队 1.6 秒一个，走 GCD
     *   4. 开连招（可选） —— 最后开，前面都就绪了
     *
     * 天赋不碰（用户决定）：自动点天赋会覆盖现有加点，
     * 而且 3.3.5 洗点要花钱，风险大于收益。
     */
    static bool HandleSetupCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = Tokenize(args);
        CombatHelper::ComboState& st = CombatHelper::St(player);

        // ---- 只有 menu 才弹窗口 ----
        if (!tok.empty() && (tok[0] == "menu" || tok[0] == "菜单" || tok[0] == "设置"))
        {
            ShowSetupMenu(player);
            return true;
        }

        if (!tok.empty() && (tok[0] == "help" || tok[0] == "?"))
        {
            ShowSetupHelp(handler);
            return true;
        }

        // ---- 其余一律【立刻执行】 ----
        // 可选参数：装等   例：.setup 200
        for (std::string const& t : tok)
            if (!t.empty() && isdigit((unsigned char)t[0]))
                st.setupIlvl = uint32(atoi(t.c_str()));

        return SetupRun(handler, player);
    }

    static void ShowSetupHelp(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ff00===== 一键开荒 =====|r");
        handler->PSendSysMessage("|cffffff00.setup|r          一键执行（发装备+配栏+上buff）");
        handler->PSendSysMessage("|cffffff00.setup 200|r      同上，装等上限 200");
        handler->PSendSysMessage("|cffffff00.setup menu|r     配置执行哪几步");
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff888888天赋需要自己点，系统不会碰|r");
    }

    // ------------------------------------------------------------------
    //  真正执行
    // ------------------------------------------------------------------
    static bool SetupRun(ChatHandler* handler, Player* player)
    {
        CombatHelper::ComboState& st = CombatHelper::St(player);

        // 职责：优先用记住的，没有就按天赋推荐
        uint8 specIdx = CombatHelper::DetectSpec(player);
        uint8 role    = st.role;
        if (role == CombatSpec::ROLE_AUTO)
            role = CombatSpec::SuggestRole(player->GetClass(), specIdx);

        uint8 scene = CombatHelper::EffectiveScene(player, st.scene);

        CombatSpec::SpecInfo const* sp = CombatSpec::GetSpec(player->GetClass(), specIdx);
        char const* specName = sp ? sp->name : "未知";

        handler->PSendSysMessage("|cff00ff00===== 一键开荒 =====|r");
        handler->PSendSysMessage("|cffffcc00%s · %s · %s|r",
            specName, CombatSpec::RoleName(role), CombatSpec::SceneName(scene));
        handler->PSendSysMessage(" ");

        uint32 step = 0;

        // ---------- 第 1 步：装备 ----------
        if (st.setupGear)
        {
            ++step;
            handler->PSendSysMessage("|cff00ccff[%u] 发装备|r", step);

            /*
             * 直接复用 .gearset —— 它已经处理好了护甲精通、
             * 需求等级、职责评分这些细节，没必要在这里重写一遍。
             *
             * ChatHandler::ParseCommands（Chat.h:87，public，已核实）
             * 会走完整的指令派发流程，和玩家自己敲一样。
             */
            std::ostringstream cmd;
            cmd << ".gearset " << GearSetClassName(player->GetClass());

            switch (role)
            {
                case CombatSpec::ROLE_TANK:   cmd << " 坦克"; break;
                case CombatSpec::ROLE_HEALER: cmd << " 治疗"; break;
                default:                      cmd << " 输出"; break;
            }

            if (st.setupIlvl)
                cmd << " " << st.setupIlvl;

            cmd << " equip";        // 直接穿上

            handler->ParseCommands(cmd.str());
        }

        // ---------- 第 2 步：配栏 ----------
        if (st.setupBar)
        {
            ++step;
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff00ccff[%u] 配快捷栏|r", step);
            BarApply(handler, player, specIdx, false, role);
        }

        // ---------- 第 3 步：增益 ----------
        if (st.setupBuff)
        {
            ++step;
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff00ccff[%u] 补增益|r", step);
            BuffNow(handler, player);
        }

        // ---------- 第 4 步：属性精调（默认关）----------
        if (st.setupStat)
        {
            ++step;
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff00ccff[%u] 属性精调|r", step);

            // 命中/精准拉满，避免打本 miss —— 这两项不影响平衡，纯粹去掉恶心
            std::vector<std::string> hitCmd  = { "hit",       "100" };
            std::vector<std::string> expCmd  = { "expertise", "100" };
            SetRating(handler, player, hitCmd);
            SetRating(handler, player, expCmd);
        }

        // ---------- 第 5 步：连招（默认关）----------
        if (st.setupCombo)
        {
            ++step;
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff00ccff[%u] 开自动连招|r", step);
            ComboStart(player, specIdx, false, role);
        }

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00===== 完成，共 %u 步 =====|r", step);

        if (!st.setupCombo)
            handler->PSendSysMessage("|cff888888想自动打，输入 |r|cffffff00.combo|r");

        handler->PSendSysMessage("|cffff8000天赋记得自己点|r");
        handler->PSendSysMessage("|cff888888改执行步骤：|r|cffffff00.setup menu|r");
        return true;
    }

    // .gearset 认的职业名
    static char const* GearSetClassName(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:      return "战士";
            case CLASS_PALADIN:      return "圣骑士";
            case CLASS_HUNTER:       return "猎人";
            case CLASS_ROGUE:        return "盗贼";
            case CLASS_PRIEST:       return "牧师";
            case CLASS_DEATH_KNIGHT: return "死亡骑士";
            case CLASS_SHAMAN:       return "萨满";
            case CLASS_MAGE:         return "法师";
            case CLASS_WARLOCK:      return "术士";
            case CLASS_DRUID:        return "德鲁伊";
            default:                 return "战士";
        }
    }

    // ------------------------------------------------------------------
    //  .setup menu —— 只配开关，不执行
    // ------------------------------------------------------------------
    static void ShowSetupMenu(Player* player)
    {
        CombatHelper::ComboState& st = CombatHelper::St(player);

        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        auto sw = [](bool on) { return on ? "|cff00ff00[开]|r" : "|cffff0000[关]|r"; };

        {
            std::ostringstream l;
            l << "【发装备】" << sw(st.setupGear) << " 按职责挑装备并穿上";
            menu.AddMenuItem(-1, GOSSIP_ICON_BATTLE, l.str(),
                             CombatHelper::SENDER_SETUP, 1, "", 0, false);
        }
        {
            std::ostringstream l;
            l << "【配快捷栏】" << sw(st.setupBar) << " 主循环/爆发/保命/增益";
            menu.AddMenuItem(-1, GOSSIP_ICON_BATTLE, l.str(),
                             CombatHelper::SENDER_SETUP, 2, "", 0, false);
        }
        {
            std::ostringstream l;
            l << "【补增益】" << sw(st.setupBuff) << " 上满 buff";
            menu.AddMenuItem(-1, GOSSIP_ICON_BATTLE, l.str(),
                             CombatHelper::SENDER_SETUP, 3, "", 0, false);
        }
        {
            std::ostringstream l;
            l << "【属性精调】" << sw(st.setupStat) << " 命中/精准拉满";
            menu.AddMenuItem(-1, GOSSIP_ICON_BATTLE, l.str(),
                             CombatHelper::SENDER_SETUP, 4, "", 0, false);
        }
        {
            std::ostringstream l;
            l << "【开自动连招】" << sw(st.setupCombo) << " 配完直接开打";
            menu.AddMenuItem(-1, GOSSIP_ICON_BATTLE, l.str(),
                             CombatHelper::SENDER_SETUP, 5, "", 0, false);
        }

        {
            std::ostringstream l;
            l << "|cff888888装等上限：" << (st.setupIlvl ? std::to_string(st.setupIlvl) : "不限")
              << "（用 .setup 200 改）|r";
            menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, l.str(),
                             CombatHelper::SENDER_SETUP, 6, "", 0, false);
        }

        menu.AddMenuItem(-1, GOSSIP_ICON_TRAINER, "|cff00ff00>> 保存并立刻执行 <<|r",
                         CombatHelper::SENDER_SETUP, 100, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888只保存，不执行|r",
                         CombatHelper::SENDER_NAV, CombatHelper::NAV_CLOSE, "", 0, false);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());

        ChatHandler(player->GetSession())
            .PSendSysMessage("|cff888888配好之后，以后直接 |r|cffffff00.setup|r|cff888888 一键执行|r");
    }

    static void SetupMenuAction(ChatHandler* handler, Player* player, uint32 action)
    {
        CombatHelper::ComboState& st = CombatHelper::St(player);

        switch (action)
        {
            case 1: st.setupGear  = !st.setupGear;  break;
            case 2: st.setupBar   = !st.setupBar;   break;
            case 3: st.setupBuff  = !st.setupBuff;  break;
            case 4: st.setupStat  = !st.setupStat;  break;
            case 5: st.setupCombo = !st.setupCombo; break;
            case 6: st.setupIlvl  = 0;
                    handler->PSendSysMessage("|cff00ff00[装等上限]|r 已改为不限");
                    break;

            case 100:
                SetupRun(handler, player);
                return;

            default:
                return;
        }

        // 改完再弹一次，方便连续调
        ShowSetupMenu(player);
    }

    // ------------------------------------------------------------------
    //  立刻补满增益
    // ------------------------------------------------------------------
    /*
     * 一次性把该职业该上的 buff 全开。
     * 用 EventProcessor 按 1.6 秒间隔排队 —— 因为 buff 都吃 GCD，
     * 一次性全 CastSpell 只有第一个能成功，后面全被 GCD 挡掉。
     */
    static bool BuffNow(ChatHandler* handler, Player* player)
    {
        CombatHelper::ComboState& st = CombatHelper::St(player);

        uint8 role = st.role;
        if (role == CombatSpec::ROLE_AUTO)
            role = CombatSpec::SuggestRole(player->GetClass(), CombatHelper::DetectSpec(player));

        uint8 specIdx = st.specIdx;
        if (!st.on && !st.allSpecs)
            specIdx = CombatHelper::DetectSpec(player);

        CombatSpec::BuiltPlan plan;
        CombatSpec::BuildPlan(player->GetClass(), specIdx, role,
                              CombatHelper::EffectiveScene(player, st.scene),
                              st.allSpecs, plan);

        struct BuffCast
        {
            uint32 spellId;
            uint32 rank1;
            uint32 flags;
            ObjectGuid target;
        };
        std::vector<BuffCast> queue;
        std::vector<Unit*> targets;
        for (CombatSpec::Skill const& sk : plan.opener)
        {
            uint32 real = CombatHelper::ResolveRank(player, sk.spell);
            if (!real || CombatHelper::IsBackedOff(player, real))
                continue;

            CombatHelper::CollectBuffTargets(player, sk, 40.0f, targets);
            for (Unit* target : targets)
                queue.push_back({ real, sk.spell, sk.flags, target->GetGUID() });
        }

        if (queue.empty())
        {
            handler->PSendSysMessage("|cff00ff00[增益]|r 该上的都在合法目标身上了，不用补。");
            return true;
        }

        ObjectGuid guid = player->GetGUID();
        uint32 buffGeneration = ++st.buffGeneration;
        uint32 delay = 0;
        for (BuffCast cast : queue)
        {
            player->m_Events.AddEventAtOffset([guid, cast, buffGeneration]()
            {
                Player* p = ObjectAccessor::FindPlayer(guid);
                if (!p || !p->IsInWorld() || !p->IsAlive())
                    return;

                CombatHelper::ComboState& state = CombatHelper::St(p);
                if (state.buffGeneration != buffGeneration)
                    return;

                Unit* target = ObjectAccessor::GetUnit(*p, cast.target);
                if (!CombatHelper::IsLegalFriendlyGroupTarget(p, target, 40.0f))
                    return;

                ObjectGuid auraCaster = (cast.flags & CombatSpec::SF_MAINTAIN_FRIEND)
                    ? p->GetGUID() : ObjectGuid::Empty;
                int32 remain = CombatHelper::AuraRemainMs(target, cast.rank1, auraCaster);
                int32 refreshWindow = (cast.flags & CombatSpec::SF_MAINTAIN_FRIEND)
                    ? 10000 : 300000;
                if (remain < 0 || remain > refreshWindow)
                    return;

                if (CombatHelper::CastChecked(p, target, cast.spellId) &&
                    (cast.flags & CombatSpec::SF_MAINTAIN_FRIEND))
                    CombatHelper::CommitMaintainedTarget(p, cast.rank1, target);
            }, Milliseconds(delay));
            delay += 1600;             // 略大于 GCD，保证每个都放得出去
        }

        /*
         * v3.2 修复（用户实测的核心冲突）：
         * 告诉 ComboTick「这段时间我在补 buff，你别抢 GCD」。
         *
         * 之前两套系统各放各的：.buff 队列每 1.6 秒放一个，
         * .combo 每 0.5 秒也在放 —— 互相打断，谁都放不干净，
         * 表现就是祝福反复重放。关掉 .buff 反而正常，正是这个原因。
         */
        st.buffQueueUntil = GameTime::GetGameTimeMS() + delay + 500;

        handler->PSendSysMessage("|cff00ff00[增益]|r 正在补 |cffffff00%u|r 个增益，约 %.1f 秒完成。",
            uint32(queue.size()), double(delay) / 1000.0);
        handler->PSendSysMessage("|cff888888补增益期间连招会让出施法，补完自动继续。|r");
        return true;
    }

    // ------------------------------------------------------------------
    //  选目标
    // ------------------------------------------------------------------
    static Unit* PickTarget(Player* player)
    {
        if (Unit* t = player->GetSelectedUnit())
            if (t->IsAlive() && player->IsValidAttackTarget(t))
                return t;

        if (Unit* t = player->GetVictim())
            if (t->IsAlive() && player->IsValidAttackTarget(t))
                return t;

        if (Unit* t = player->SelectNearbyTarget(nullptr, 30.0f))
            if (t->IsAlive() && player->IsValidAttackTarget(t))
            {
                // 顺手把客户端选中框也同步过去，玩家看得见在打谁
                player->SetSelection(t->GetGUID());
                return t;
            }

        return nullptr;
    }

    // 数周围有几个能打的（AOE 判定用）
    /*
     * 用官方的格子搜索，和 Unit::SelectNearbyTarget（Unit.cpp:11155-11160）
     * 完全一样的写法：
     *     AnyUnfriendlyUnitInObjectRangeCheck  (GridNotifiers.h:959)
     *     UnitListSearcher                     (GridNotifiers.h:435)
     *     Cell::VisitAllObjects                (CellImpl.h)
     *
     * 之前的版本只数 getAttackers()（正在打我的），会漏掉一大堆：
     * 站桩 AOE 时怪还没摸到我，攻击者列表是空的 —— AOE 永远放不出来。
     * 打本正是这个场景，所以必须真扫格子。
     */
    static uint32 CountNearbyEnemies(Player* player, float radius)
    {
        std::list<Unit*> targets;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(player, player, radius);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck>
            searcher(player, targets, check);
        Cell::VisitAllObjects(player, searcher, radius);

        uint32 n = 0;
        for (Unit* u : targets)
        {
            if (!u || !u->IsAlive())
                continue;
            if (u->IsTotem() || u->IsCritter())      // 图腾/小动物不算
                continue;
            if (!player->IsValidAttackTarget(u))
                continue;
            ++n;
        }
        return n;
    }

    static bool IsTauntCandidate(Player* player, Unit* enemy)
    {
        if (!enemy || !enemy->IsAlive() || !player->IsValidAttackTarget(enemy))
            return false;
        if (enemy->IsTotem() || enemy->IsCritter())
            return false;

        Unit* victim = enemy->GetVictim();
        if (!victim || victim == player || !player->IsFriendlyTo(victim))
            return false;
        return CombatHelper::IsLegalFriendlyGroupTarget(player, victim, 100.0f);
    }

    static uint32 CountNearbyTauntCandidates(Player* player, float radius)
    {
        std::list<Unit*> targets;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(player, player, radius);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck>
            searcher(player, targets, check);
        Cell::VisitAllObjects(player, searcher, radius);

        uint32 n = 0;
        for (Unit* enemy : targets)
            if (IsTauntCandidate(player, enemy))
                ++n;
        return n;
    }

    static Unit* FindTauntCandidate(Player* player, float radius)
    {
        std::list<Unit*> targets;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(player, player, radius);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck>
            searcher(player, targets, check);
        Cell::VisitAllObjects(player, searcher, radius);

        for (Unit* enemy : targets)
            if (IsTauntCandidate(player, enemy))
                return enemy;
        return nullptr;
    }

    static Unit* FindTauntVictim(Player* player, float radius)
    {
        if (Unit* enemy = FindTauntCandidate(player, radius))
            return enemy->GetVictim();
        return nullptr;
    }

    static bool HasTauntCandidateForVictim(Player* player, Unit* victim)
    {
        if (!CombatHelper::IsLegalFriendlyGroupTarget(player, victim, 40.0f))
            return false;
        for (Unit* enemy : victim->getAttackers())
            if (IsTauntCandidate(player, enemy))
                return true;
        return false;
    }

    // ------------------------------------------------------------------
    //  判断一个技能现在能不能放
    // ------------------------------------------------------------------
    /*
     * 返回值：
     *   0 = 可以放
     *   1 = 因为在移动而跳过（读条技）→ 要给玩家提示
     *   2 = 其他原因跳过（CD、距离、血线、已有buff…）
     */
    static uint8 CheckSkill(Player* player, Unit* target,
                            CombatSpec::Skill const& sk, uint32 realSpell,
                            uint32 nearbyCount, uint32 tauntCandidateCount,
                            CombatSpec::SceneTuning const& tune)
    {
        SpellInfo const* si = sSpellMgr->GetSpellInfo(realSpell);
        if (!si)
            return 2;

        // --- CD ---
        if (!player->GetSpellHistory()->IsReady(si))
            return 2;

        // --- 全局CD（SpellHistory.h:132）---
        if (player->GetSpellHistory()->HasGlobalCooldown(si))
            return 2;

        // --- 移动 + 读条 ---
        /*
         * Spell.cpp:6654 的判定：
         *   if (m_casttime > 0 && (InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT))
         *       return SPELL_FAILED_MOVING;
         * 我们提前用同样的条件判，避免白白吃一个失败提示。
         * SpellInfo.h:491 CalcCastTime() / :346 InterruptFlags 都是 public。
         */
        if (player->isMoving())
        {
            bool hasCastTime = si->CalcCastTime() > 0;
            bool breakOnMove = (si->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT) != 0;

            if ((sk.flags & CombatSpec::SF_NO_MOVE) || (hasCastTime && breakOnMove))
                return 1;
        }

        // --- 正在读条时不打断自己 ---
        // Unit.h:1482 IsNonMeleeSpellCast(withDelayed=false)
        if (player->IsNonMeleeSpellCast(false))
            return 2;

        // --- 近战距离 ---
        if ((sk.flags & CombatSpec::SF_MELEE) && target)
            if (!player->IsWithinMeleeRange(target))
                return 2;

        /*
         * --- v3.4：射程预检（含猎人最小射程"死亡区"）---
         *
         * 用户实测「猎人只有普通攻击，奥术射击都没有」，根因之一就是：
         * 怪贴脸时所有射击技都在最小射程内，服务端返回 SPELL_FAILED_TOO_CLOSE。
         * 之前不预检 -> 硬放 -> 失败 -> 进退避表 -> 累计后被拉黑。
         *
         * 现在放之前先量距离，够不着/太近直接跳过，连失败都不产生。
         * SpellInfo.h:483 GetMinRange / :484 GetMaxRange（已核实 public）
         */
        if (target && target != player && !(sk.flags & CombatSpec::SF_SELF))
        {
            float dist   = player->GetExactDist2d(target);
            float maxRng = si->GetMaxRange(false);
            float minRng = si->GetMinRange(false);

            // maxRange 为 0 的多是自身/无限距离法术，不做上限判断
            if (maxRng > 0.0f && dist > maxRng)
                return 2;

            // 猎人射击类的死亡区：太近放不出来
            if (minRng > 0.0f && dist < minRng)
                return 2;
        }

        // --- 目标类型：动态友方绝不能退化成敌方或自己兜底 ---
        if ((sk.flags & CombatSpec::SF_FRIEND) &&
            !CombatHelper::IsLegalFriendlyGroupTarget(player, target, 40.0f))
            return 2;

        // --- 斩杀线（默认 20%）---
        if ((sk.flags & CombatSpec::SF_EXECUTE) && target)
        {
            uint32 mx = target->GetMaxHealth();
            if (!mx || (uint64(target->GetHealth()) * 100 / mx) > 20)
                return 2;
        }

        // --- AOE：普通伤害看敌人数；AOE嘲讽只看真正需要接怪的数量 ---
        if ((sk.flags & CombatSpec::SF_AOE) && !(sk.flags & CombatSpec::SF_TAUNT_AOE) &&
            nearbyCount < tune.aoeThreshold)
            return 2;
        if (sk.flags & CombatSpec::SF_TAUNT_AOE)
        {
            bool eligible = (target == player)
                ? (tauntCandidateCount >= 1)
                : HasTauntCandidateForVictim(player, target);
            if (!eligible)
                return 2;
        }
        if ((sk.flags & CombatSpec::SF_TAUNT) && !(sk.flags & CombatSpec::SF_TAUNT_AOE) &&
            !IsTauntCandidate(player, target))
            return 2;

        // --- DOT/debuff：按rank链和自己的caster检查实际目标 ---
        if ((sk.flags & CombatSpec::SF_DEBUFF_KEEP) && target)
            if (CombatHelper::HasAnyRankAura(target, sk.spell, player->GetGUID()))
                return 2;

        // --- BUFF：自己身上已经有了就不重复开 ---
        /*
         * v3.2：必须按【整条 rank 链】判断，不能只看 realSpell。
         *
         * Unit::HasAura()（Unit.cpp:4793）只认精确 spellId。
         * 而祝福/光环这类 buff 常见的情况是：
         *   身上挂着别人给的、或低一阶的版本 -> HasAura(最高阶) 返回 false
         *   -> 判定"没有" -> 重复放 -> 又被服务端以各种理由拒
         * 这是圣骑士祝福反复重放的另一个诱因。
         */
        if (sk.flags & CombatSpec::SF_BUFF_KEEP)
        {
            /*
             * v3.3：不能只看"有没有"，还要看"还剩多久"。
             *
             * 用户实测：脱战时每 3 秒补一次祝福，白白消耗。
             * 原因是某些 buff（尤其是被别的效果顶掉又自动恢复的）
             * 在 HasAura 上会出现瞬时空窗，于是反复重上。
             *
             *   -1  = 永久光环（圣印/光环/姿态）-> 有就绝不重上
             *   >5分钟 = 还很久 -> 不重上
             *   0    = 真没有 -> 才去上
             */
            if (!target)
                return 2;
            if ((sk.flags & CombatSpec::SF_SELF) && target != player)
                return 2;
            if ((sk.flags & (CombatSpec::SF_RAID_BUFF | CombatSpec::SF_MAINTAIN_FRIEND)) &&
                !CombatHelper::IsLegalFriendlyGroupTarget(player, target, 40.0f))
                return 2;

            ObjectGuid auraCaster = (sk.flags & CombatSpec::SF_MAINTAIN_FRIEND)
                ? player->GetGUID() : ObjectGuid::Empty;
            int32 remain = CombatHelper::AuraRemainMs(target, sk.spell, auraCaster);
            if (remain != 0)
            {
                if (remain < 0)               // 永久光环
                    return 2;
                int32 refreshWindow = (sk.flags & CombatSpec::SF_MAINTAIN_FRIEND)
                    ? 10000 : 300000;
                if (remain > refreshWindow)
                    return 2;
            }
        }

        // --- 终结技：要有连击点（Unit.h:1707）---
        if ((sk.flags & CombatSpec::SF_COMBO_FINISH) && target)
            if (player->GetComboPoints(target) < 1)
                return 2;

        // ================= v3 新增判定 =================

        // --- 自我解控：只在真的被控住时才用，否则浪费 CD ---
        if (sk.flags & CombatSpec::SF_FREE_SELF)
        {
            uint32 const ccMask = (1u << MECHANIC_FEAR)  | (1u << MECHANIC_STUN)  |
                                  (1u << MECHANIC_ROOT)  | (1u << MECHANIC_SNARE) |
                                  (1u << MECHANIC_SLEEP) | (1u << MECHANIC_CHARM) |
                                  (1u << MECHANIC_DISORIENTED) | (1u << MECHANIC_FREEZE);
            if (!player->HasAuraWithMechanic(ccMask))
                return 2;
        }

        /*
         * --- 打断 ---
         * 目标是否值得打断，已经由 FindBestInterruptTarget 选好了
         * （它会挑读条最长/群体技/冲我来的那个）。
         * 这里只做兜底：万一从别的路径进来，目标没在读条就跳过。
         */
        if (sk.flags & CombatSpec::SF_INTERRUPT)
        {
            if (!target)
                return 2;

            Spell* cur = target->GetCurrentSpell(CURRENT_GENERIC_SPELL);
            if (!cur || cur->getState() != SPELL_STATE_PREPARING)
                return 2;
        }

        // --- 回蓝技：蓝多的时候不用 ---
        if (sk.flags & CombatSpec::SF_MANA_LOW)
        {
            if (player->GetPowerType() != POWER_MANA)
                return 2;
            uint32 mx = player->GetMaxPower(POWER_MANA);
            if (!mx || (uint64(player->GetPower(POWER_MANA)) * 100 / mx) > 40)
                return 2;
        }

        return 0;
    }

    // ------------------------------------------------------------------
    //  v3：治疗类技能单独判定（目标是队友，逻辑和打人不一样）
    // ------------------------------------------------------------------
    /*
     * 返回同 CheckSkill：0 可放 / 1 被移动挡住 / 2 其他跳过
     */
    static uint8 CheckHealSkill(Player* player, CombatHelper::HealTarget const& ht,
                                CombatSpec::Skill const& sk, uint32 realSpell,
                                CombatSpec::SceneTuning const& tune)
    {
        if (!ht.unit)
            return 2;

        SpellInfo const* si = sSpellMgr->GetSpellInfo(realSpell);
        if (!si)
            return 2;

        float maxRange = si->GetMaxRange(true, player, nullptr);
        if (!CombatHelper::IsLegalFriendlyGroupTarget(player, ht.unit,
                maxRange > 0.0f ? maxRange : 40.0f))
            return 2;

        if (!player->GetSpellHistory()->IsReady(si))
            return 2;
        if (player->GetSpellHistory()->HasGlobalCooldown(si))
            return 2;

        // 移动 + 读条：和打人技一样的判定
        if (player->isMoving())
        {
            bool hasCastTime = si->CalcCastTime() > 0;
            bool breakOnMove = (si->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT) != 0;
            if ((sk.flags & CombatSpec::SF_NO_MOVE) || (hasCastTime && breakOnMove))
                return 1;
        }

        if (player->IsNonMeleeSpellCast(false))
            return 2;

        // 紧急救命大招：血线极低才用，平时留着
        if (sk.flags & CombatSpec::SF_HEAL_EMERG)
            return (ht.hpPct <= float(tune.emergencyPct)) ? 0 : 2;

        // 群疗：至少 3 个人受伤才值得
        if (sk.flags & CombatSpec::SF_HEAL_AOE)
            return (ht.hurtCnt >= 3) ? 0 : 2;

        // HOT：目标身上已经有了就不重复上
        if (sk.flags & CombatSpec::SF_HOT)
        {
            if (CombatHelper::HasAnyRankAura(ht.unit, sk.spell, player->GetGUID()))
                return 2;
            // HOT 可以提前上，血线放宽 10%
            return (ht.hpPct <= float(tune.healTargetPct) + 10.0f) ? 0 : 2;
        }

        // 普通单奶：看血线
        bool isSelf = (ht.unit == player);
        uint32 line = isSelf ? tune.healSelfPct : tune.healTargetPct;
        return (ht.hpPct <= float(line)) ? 0 : 2;
    }

    // ------------------------------------------------------------------
    //  一次 tick：挑一个技能放出去
    // ------------------------------------------------------------------
    static void ComboTick(Player* player)
    {
        // 先把需要的字段取成值快照，避免后面误用悬空引用
        uint8 specIdx, role, sceneStored;
        bool allSpecs, autoBuff, autoDispel;
        {
            CombatHelper::ComboState& s = CombatHelper::St(player);
            if (!s.on)
                return;
            specIdx     = s.specIdx;
            allSpecs    = s.allSpecs;
            role        = s.role;
            sceneStored = s.scene;
            autoBuff    = s.autoBuff;
            autoDispel  = s.autoDispel;
        }

        ChatHandler handler(player->GetSession());

        uint8 scene = CombatHelper::EffectiveScene(player, sceneStored);
        CombatSpec::SceneTuning const& tune = CombatSpec::GetTuning(scene);

        if (role == CombatSpec::ROLE_AUTO)
            role = CombatSpec::SuggestRole(player->GetClass(), specIdx);

        /*
         * 用 BuildPlan 按「专精 + 职责 + 场景」组装序列。
         * 结果缓存在 thread_local，只有三要素之一变了才重算，
         * 避免每 500ms 重新组装（那会很费）。
         */
        /*
         * v3.3 修复：缓存必须【按玩家】分开存。
         *
         * 之前用 static thread_local 存单份 plan —— 所有玩家共用一份！
         * A 是治疗、B 是输出，两人都开连招时，谁后进来谁覆盖缓存，
         * 另一个人就拿到了错的方案（治疗拿到输出循环 -> 治疗技根本不出现）。
         * 这就是"奶的治疗技能我没有看到使用"的根因之一。
         */
        CombatHelper::PlanCache& pc =
            CombatHelper::PlanCaches()[player->GetGUID().GetCounter()];
        if (pc.cls != player->GetClass() || pc.spec != specIdx ||
            pc.role != role || pc.all != allSpecs || pc.scene != scene)
        {
            CombatSpec::BuildPlan(player->GetClass(), specIdx, role, scene, allSpecs, pc.plan);
            pc.cls = player->GetClass(); pc.spec = specIdx;
            pc.role = role; pc.all = allSpecs; pc.scene = scene;
        }
        CombatSpec::BuiltPlan const& plan = pc.plan;

        Unit*  target = PickTarget(player);
        uint32 nearby = CountNearbyEnemies(player, 10.0f);
        uint32 tauntNearby = CountNearbyTauntCandidates(player, 10.0f);

        float myHpPct = player->GetHealthPct();
        bool  lowHp   = myHpPct < float(tune.defensiveHpPct);

        bool blockedByMove = false;

        // 通用施法：放出去就记账并返回 true
        auto tryCast = [&](CombatSpec::Skill const& sk, Unit* castTarget) -> bool
        {
            if (sk.flags & CombatSpec::SF_SELF)
                castTarget = player;

            uint32 real = CombatHelper::ResolveRank(player, sk.spell);
            if (!real)
                return false;

            uint8 r = CheckSkill(player, castTarget, sk, real, nearby, tauntNearby, tune);
            if (r == 1) { blockedByMove = true; return false; }
            if (r != 0) return false;

            if (!CombatHelper::CastChecked(player, castTarget, real))
                return false;

            if (sk.flags & CombatSpec::SF_MAINTAIN_FRIEND)
                CombatHelper::CommitMaintainedTarget(player, sk.spell, castTarget);

            CombatHelper::ComboState& st = CombatHelper::St(player);
            ++st.castCount;
            st.warnedMove = false;
            return true;
        };

        // ==============================================================
        //  优先级 1：紧急 —— 自我解控 / 保命
        // ==============================================================
        for (CombatSpec::Skill const& sk : plan.emergency)
        {
            bool needSelf = (sk.flags & (CombatSpec::SF_SELF | CombatSpec::SF_FREE_SELF)) != 0;
            bool needFriend = (sk.flags & CombatSpec::SF_FRIEND) != 0;
            Unit* ct = needSelf ? player : (target ? target : player);

            if (needFriend)
            {
                CombatHelper::HealTarget friendTarget;
                CombatHelper::PickHealTarget(player, 40.0f, friendTarget);
                if (!friendTarget.unit || friendTarget.hpPct > float(tune.emergencyPct))
                    continue;
                ct = friendTarget.unit;
            }
            else if (!(sk.flags & CombatSpec::SF_FREE_SELF) &&
                     !(sk.flags & CombatSpec::SF_HEAL_EMERG) && !lowHp)
                continue;

            if (sk.flags & CombatSpec::SF_HEAL_EMERG)
                continue;                       // 救命奶在下面治疗段处理

            if (tryCast(sk, ct))
                return;
        }

        // ==============================================================
        //  优先级 2：治疗（治疗职责，或任何职责血少自救）
        // ==============================================================
        /*
         * v3.3 修复：治疗触发条件写错了。
         *
         * 旧条件是 (role==HEALER || 自己血少)，看似没问题，
         * 但下面又套了一层 allow = (role==HEALER || ht.unit==player)，
         * 而 PickHealTarget 挑出来的往往是【队友】——
         * 于是非治疗职责时 allow=false 直接跳过，
         * 治疗职责时如果 PickHealTarget 因为距离/组队判断没拿到人，也跳过。
         *
         * 现在拆成两条清晰的路径：
         *   治疗职责  -> 全团扫描，谁血少奶谁（这才是"奶"）
         *   其他职责  -> 只在自己血少时自奶
         */
        {
            bool isHealer = (role == CombatSpec::ROLE_HEALER);

            CombatHelper::HealTarget ht;
            if (isHealer)
            {
                // 治疗：扫全团（40 码内）
                CombatHelper::PickHealTarget(player, 40.0f, ht);
            }
            else if (myHpPct < float(tune.healSelfPct))
            {
                // 非治疗：只管自己
                ht.unit  = player;
                ht.hpPct = myHpPct;
                ht.isTank = false;
                ht.hurtCnt = 1;
            }

            if (ht.unit)
            {
                for (CombatSpec::Skill const& sk : plan.core)
                {
                    if (!(sk.flags & (CombatSpec::SF_HEAL | CombatSpec::SF_HEAL_AOE |
                                      CombatSpec::SF_HOT  | CombatSpec::SF_HEAL_EMERG)))
                        continue;

                    uint32 real = CombatHelper::ResolveRank(player, sk.spell);
                    if (!real)
                        continue;

                    CombatHelper::HealTarget castHt = ht;
                    if (sk.flags & CombatSpec::SF_SELF)
                    {
                        castHt.unit = player;
                        castHt.hpPct = myHpPct;
                        castHt.isTank = false;
                    }

                    uint8 r = CheckHealSkill(player, castHt, sk, real, tune);
                    if (r == 1) { blockedByMove = true; continue; }
                    if (r != 0) continue;

                    if (!CombatHelper::CastChecked(player, castHt.unit, real))
                        continue;

                    CombatHelper::ComboState& st = CombatHelper::St(player);
                    ++st.castCount; ++st.healCount;
                    st.warnedMove = false;
                    return;
                }
            }
        }

        // ==============================================================
        //  优先级 3：驱散队友负面
        // ==============================================================
        if (autoDispel)
        {
            for (CombatSpec::Skill const& sk : plan.utility)
            {
                if (!(sk.flags & CombatSpec::SF_DISPEL_FRIEND))
                    continue;

                uint32 real = CombatHelper::ResolveRank(player, sk.spell);
                if (!real)
                    continue;

                SpellInfo const* si = sSpellMgr->GetSpellInfo(real);
                if (!si || !player->GetSpellHistory()->IsReady(si))
                    continue;
                if (player->GetSpellHistory()->HasGlobalCooldown(si))
                    continue;

                uint32 mask = CombatHelper::DispelMaskOfSpell(si);
                if (!mask)
                    continue;

                Unit* dt = CombatHelper::FindDispelTarget(player, mask, 40.0f);
                if (!dt)
                    continue;

                if (player->isMoving() && si->CalcCastTime() > 0 &&
                    (si->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT))
                {
                    blockedByMove = true;
                    continue;
                }

                if (!CombatHelper::CastChecked(player, dt, real))
                    continue;

                CombatHelper::ComboState& st = CombatHelper::St(player);
                ++st.castCount; ++st.dispelCount;
                return;
            }
        }

        // ==============================================================
        //  优先级 4：打断（v3.1 强化：优先打断蓄力大招）
        // ==============================================================
        /*
         * 不是见到读条就打断，而是【挑最危险的那个】。
         *
         * 做法：先扫周围所有正在读条的敌人，用 ThreatScore 打分，
         *       分最高的那个才打。分数看：
         *         · 读条时间越长 = 蓄力大招，分越高
         *         · 群体技（IsTargetingArea）加权
         *         · 正在打我的加权
         *         · 剩余读条时间太短（<0.3秒）就别浪费打断了
         *
         * 这样「敌人放蓄力的强大技能优先打断」就成立了。
         */
        {
            CombatHelper::CastingEnemy best;
            CombatHelper::FindBestInterruptTarget(player, 30.0f, best);

            if (best.unit)
            {
                for (CombatSpec::Skill const& sk : plan.utility)
                {
                    if (!(sk.flags & CombatSpec::SF_INTERRUPT))
                        continue;

                    // 打断技也有距离要求，先确认够得着
                    uint32 real = CombatHelper::ResolveRank(player, sk.spell);
                    if (!real)
                        continue;

                    SpellInfo const* si = sSpellMgr->GetSpellInfo(real);
                    if (si && player->GetExactDist2d(best.unit) > si->GetMaxRange(false))
                        continue;

                    if (tryCast(sk, best.unit))
                    {
                        handler.PSendSysMessage("|cffff4400[打断]|r %s 的 |cffffff00%s|r%s",
                            best.unit->GetName().c_str(), best.spellName,
                            best.isAoe ? "  |cffff0000(群体技)|r" : "");
                        return;
                    }
                }
            }
        }

        // ==============================================================
        //  优先级 5：增益（自动补 buff）
        // ==============================================================
        /*
         * v3.2：补 buff 加了两道闸，解决和 .buff 指令抢 GCD 的冲突。
         *
         *  闸 1  .buff 队列正在跑 -> 完全让路，别去抢施法
         *  闸 2  限流 3 秒一次    -> buff 是长时间增益，没必要每 0.5 秒试
         *                           （lastBuffTry 字段之前声明了却没用上）
         *
         * 战斗中默认也不补主动增益：打着架去换圣印/光环会打断输出节奏。
         * 真缺了可以手动 .buff now。
         */
        if (autoBuff && tune.keepBuffs && !player->IsInCombat())
        {
            uint32 nowMs = GameTime::GetGameTimeMS();
            CombatHelper::ComboState& bs = CombatHelper::St(player);

            bool queueRunning = (nowMs < bs.buffQueueUntil);

            /*
             * v3.3：限流从 3 秒放宽到 15 秒。
             *
             * 用户实测「脱战时大概 3 秒补充一次 buff，还是会多余消耗」。
             * buff 是分钟级的增益，3 秒一查太频繁了；
             * 配合上面 AuraRemainMs 的"剩余 <5 分钟才补"，
             * 正常情况下整场战斗只会在真的掉了时补一次。
             */
            bool throttled = (bs.lastBuffTry != 0 && nowMs - bs.lastBuffTry < 15000);

            if (!queueRunning && !throttled)
            {
                bs.lastBuffTry = nowMs;

                std::vector<Unit*> buffTargets;
                for (CombatSpec::Skill const& sk : plan.opener)
                {
                    CombatHelper::CollectBuffTargets(player, sk, 40.0f, buffTargets);
                    for (Unit* buffTarget : buffTargets)
                        if (tryCast(sk, buffTarget))
                            return;
                }
            }
        }

        // ==============================================================
        //  优先级 6：回蓝
        // ==============================================================
        for (CombatSpec::Skill const& sk : plan.utility)
        {
            if (!(sk.flags & CombatSpec::SF_MANA_LOW))
                continue;
            if (tryCast(sk, player))
                return;
        }

        // ==============================================================
        //  优先级 7：爆发（高难度场景省着用，只在 BOSS 血多时开）
        // ==============================================================
        if (target)
        {
            bool burstOk = true;
            if (tune.saveBurst)
            {
                // 留给 BOSS：血上限明显高于自己才算 BOSS
                burstOk = target->GetMaxHealth() > player->GetMaxHealth() * 3;
            }

            if (burstOk)
                for (CombatSpec::Skill const& sk : plan.burst)
                {
                    Unit* ct = (sk.flags & CombatSpec::SF_SELF) ? player : target;
                    if (tryCast(sk, ct))
                        return;
                }
        }

        // ==============================================================
        //  优先级 8：主循环
        // ==============================================================
        for (CombatSpec::Skill const& sk : plan.core)
        {
            // 治疗/增益已在专用路径处理，绝不拿敌方目标兜底。
            if (sk.flags & (CombatSpec::SF_HEAL | CombatSpec::SF_HEAL_AOE |
                            CombatSpec::SF_HOT  | CombatSpec::SF_HEAL_EMERG |
                            CombatSpec::SF_BUFF_KEEP | CombatSpec::SF_RAID_BUFF |
                            CombatSpec::SF_MAINTAIN_FRIEND))
                continue;

            bool selfCast = (sk.flags & CombatSpec::SF_SELF) != 0;
            Unit* ct = selfCast ? player : target;
            if ((sk.flags & CombatSpec::SF_TAUNT) &&
                !(sk.flags & CombatSpec::SF_TAUNT_AOE))
                ct = FindTauntCandidate(player, 30.0f);
            else if ((sk.flags & CombatSpec::SF_TAUNT_AOE) && !selfCast)
                ct = FindTauntVictim(player, 30.0f);
            if (!ct)
                continue;
            if (tryCast(sk, ct))
                return;
        }

        // 一个都没放出来，且是因为在移动 -> 提示一次
        CombatHelper::ComboState& s = CombatHelper::St(player);
        if (blockedByMove && !s.warnedMove)
        {
            s.warnedMove = true;
            handler.PSendSysMessage("|cff8888ff[连招]|r 读条技需要站定，停下就会自动接上。");
        }
    }

    // ------------------------------------------------------------------
    //  自循环
    // ------------------------------------------------------------------
    /*
     * Object.h:620 EventProcessor m_Events 是 public，
     * EventProcessor.h:109 AddEventAtOffset(lambda, Milliseconds) 支持 lambda。
     *
     * 500ms 一跳：比 GCD(1.5s) 密，站定后能立刻补上被移动挡掉的技能，
     * 又不会太频繁拖累性能。
     */
    static void ScheduleTick(Player* player, uint32 generation)
    {
        ObjectGuid guid = player->GetGUID();

        player->m_Events.AddEventAtOffset([guid, generation]()
        {
            Player* p = ObjectAccessor::FindPlayer(guid);
            if (!p || !p->IsInWorld())
                return;

            /*
             * 注意：这里【不能】把 States() 的迭代器/引用跨 ComboTick 使用。
             * ComboTick 内部会调 St(player)，那是 operator[]，
             * 若该 key 不存在会插入新元素 —— unordered_map 插入可能 rehash，
             * 之前拿到的迭代器和引用就全失效了（未定义行为）。
             * 所以每次都重新查，用完即弃。
             */
            {
                auto it = CombatHelper::States().find(guid.GetCounter());
                if (it == CombatHelper::States().end() || !it->second.on ||
                    it->second.generation != generation)
                    return;
            }

            if (!p->IsAlive())
            {
                CombatHelper::InvalidatePlayerRuntime(p, false);

                ChatHandler(p->GetSession())
                    .PSendSysMessage("|cffff8000[连招]|r 你已阵亡，自动停止。");
                return;
            }

            ComboTick(p);

            // ComboTick 里可能已经关掉了（比如玩家手动 off），要重新确认
            auto it2 = CombatHelper::States().find(guid.GetCounter());
            if (it2 == CombatHelper::States().end() || !it2->second.on ||
                it2->second.generation != generation)
                return;

            ScheduleTick(p, generation);
        }, Milliseconds(500));
    }

private:
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

    friend class combathelper_playerscript;
};

// ============================================================================
//  PlayerScript：接 Gossip 回调
// ============================================================================
class combathelper_playerscript : public PlayerScript
{
public:
    combathelper_playerscript() : PlayerScript("combathelper_playerscript") { }

    /*
     * ScriptMgr.h:719 OnGossipSelect(Player*, uint32 menu_id, uint32 sender, uint32 action)
     *
     * 注意：Eluna 那边 menu_id 永远是 nil，C++ 这边虽然有值但套装脚本
     * （cs_gearset.cpp:1943）只按 sender 过滤。所以我们也只认自己的 sender 段，
     * 其余原样放过，不干扰别的脚本。
     */
    void OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 sender, uint32 action) override
    {
        if (!player)
            return;

        // 只认自己的 sender 段（9401-9409），其余原样放过不干扰别的脚本
        if (sender < CombatHelper::SENDER_BAR_SPEC || sender > CombatHelper::SENDER_NAV)
            return;

        ChatHandler handler(player->GetSession());
        CombatHelper::ComboState& st = CombatHelper::St(player);

        switch (sender)
        {
            // ---- 第 1 步：选完专精 -> 弹职责菜单 ----
            case CombatHelper::SENDER_BAR_SPEC:
            case CombatHelper::SENDER_COMBO_SPEC:
            {
                bool allSpecs = (action == CombatHelper::ACTION_ALL_SPECS);
                uint8 specIdx = allSpecs ? 0 : uint8(action);

                st.pendingFromBar = (sender == CombatHelper::SENDER_BAR_SPEC);

                player->PlayerTalkClass->SendCloseGossip();
                combathelper_commandscript::ShowRoleMenu(player,
                    st.pendingFromBar ? CombatHelper::SENDER_BAR_ROLE
                                      : CombatHelper::SENDER_COMBO_ROLE,
                    specIdx, allSpecs);
                return;
            }

            // ---- 第 2 步：选完职责 -> 真正应用 ----
            case CombatHelper::SENDER_BAR_ROLE:
            case CombatHelper::SENDER_COMBO_ROLE:
            {
                uint8 role = uint8(action);
                if (role >= CombatSpec::ROLE_MAX)
                    role = CombatSpec::ROLE_DPS;

                st.role = role;                 // 记住选择
                uint8 specIdx = st.pendingSpec;
                bool  allSpecs = st.pendingAll;

                player->PlayerTalkClass->SendCloseGossip();

                if (sender == CombatHelper::SENDER_BAR_ROLE)
                    combathelper_commandscript::BarApply(&handler, player, specIdx, allSpecs, role);
                else
                    combathelper_commandscript::ComboStart(player, specIdx, allSpecs, role);
                return;
            }

            // ---- 场景选择 ----
            case CombatHelper::SENDER_SCENE:
            {
                uint8 sc = uint8(action);
                if (sc >= CombatSpec::SCENE_MAX)
                    sc = CombatSpec::SCENE_AUTO;
                st.scene = sc;

                player->PlayerTalkClass->SendCloseGossip();

                uint8 eff = CombatHelper::EffectiveScene(player, sc);
                handler.PSendSysMessage("|cff00ff00[场景]|r 已设为 |cffffff00%s|r",
                    CombatSpec::SceneName(eff));

                CombatSpec::SceneTuning const& t = CombatSpec::GetTuning(eff);
                handler.PSendSysMessage("|cff888888AOE门槛 %u 个怪 / 保命线 %u%% / 救人线 %u%%|r",
                    t.aoeThreshold, t.defensiveHpPct, t.emergencyPct);
                return;
            }

            // ---- .buff 开关菜单 ----
            case CombatHelper::SENDER_BUFF:
            {
                player->PlayerTalkClass->SendCloseGossip();
                combathelper_commandscript::BuffMenuAction(&handler, player, action);
                return;
            }

            // ---- .setup menu 开关配置 ----
            case CombatHelper::SENDER_SETUP:
            {
                player->PlayerTalkClass->SendCloseGossip();
                combathelper_commandscript::SetupMenuAction(&handler, player, action);
                return;
            }

            // ---- 导航 ----
            case CombatHelper::SENDER_NAV:
            {
                if (action == CombatHelper::NAV_BACK)
                {
                    player->PlayerTalkClass->SendCloseGossip();
                    combathelper_commandscript::ShowSpecMenu(player,
                        st.pendingFromBar ? CombatHelper::SENDER_BAR_SPEC
                                          : CombatHelper::SENDER_COMBO_SPEC,
                        st.pendingFromBar ? "智能配栏" : "自动连招");
                    return;
                }
                player->PlayerTalkClass->SendCloseGossip();
                return;
            }

            default:
                return;
        }
    }

    // 下线时关掉连招，避免事件残留
    void OnLogout(Player* player) override
    {
        if (!player)
            return;
        CombatHelper::InvalidatePlayerRuntime(player, true);
    }
};

void AddSC_combathelper_commandscript()
{
    new combathelper_commandscript();
    new combathelper_playerscript();
}
