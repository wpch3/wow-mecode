/*
 * ============================================================================
 *  CombatSpecData.h —— 33 个专精的技能数据表
 * ============================================================================
 *
 *  数据来源（关键）：
 *    所有法术 ID 均取自【本仓库自带的 NPCBot 职业 AI】，路径：
 *      src/server/game/AI/NpcBots/bot_<class>_ai.cpp
 *    这些 ID 是 NPCBot 每天在跑的实战数据，不是网上抄的。
 *    共提取 536 个 rank-1 ID，10 个职业。
 *
 *  为什么用 rank-1 ID 而不是 80 级的高阶 ID：
 *    NPCBot 的做法是存 rank-1，运行时用 SpellMgr 的 rank 链找玩家会的最高阶。
 *    好处：低级号也能用（35 级战士配 35 级那阶的致死打击），
 *          不会出现"配了栏但技能是灰的"。
 *    见 SpellMgr.h:594 GetFirstSpellInChain / :596 GetNextSpellInChain（都是 public）。
 *
 *  33 个专精 = 10 职业 × 3 专精 + 德鲁伊第 4 专精（平衡/野性/恢复 + 熊坦）
 *    德鲁伊按用户要求做 4 个专精窗口（平衡、野性输出、熊坦、恢复）。
 *
 *  每个专精 4 组数据：
 *    rotation   主循环   —— .combo 按优先级往下找第一个能放的
 *    burst      爆发     —— 爆发期一次性开
 *    defensive  保命     —— 血少自动用（.combo 里做了血量触发）
 *    buffs      增益     —— 上场前一次性开
 *
 *  ============================================================================
 */

#ifndef _COMBAT_SPEC_DATA_H
#define _COMBAT_SPEC_DATA_H

#include "Define.h"
#include <vector>

namespace CombatSpec
{
    // ==================================================================
    //  队伍职责（v3 新增）
    // ==================================================================
    /*
     * 同一个专精，职责不同打法完全不同：
     *   防护战当坦克 -> 优先嘲讽、拉怪、开减伤
     *   防护战当输出 -> 优先伤害技，不浪费 GCD 在嘲讽上
     * 所以「专精 × 职责」才能确定一套循环。
     */
    enum TeamRole : uint8
    {
        ROLE_AUTO    = 0,   // 跟专精推荐
        ROLE_TANK    = 1,
        ROLE_DPS     = 2,
        ROLE_HEALER  = 3,
        ROLE_MAX     = 4
    };

    char const* RoleName(uint8 r);

    // ==================================================================
    //  战斗场景（v3 新增）
    // ==================================================================
    /*
     * 场景决定「打法激进程度」和「AOE 门槛」：
     *   日常刷怪 -> 怪少血薄，2 个怪就该 AOE，不用留保命
     *   聚怪刷материал -> 大量怪，AOE 门槛最低，优先群伤
     *   高级团本 -> 命最重要，保命阈值拉高，增益必须全，爆发要留
     */
    enum SceneMode : uint8
    {
        SCENE_AUTO      = 0,   // 自动识别
        SCENE_QUEST     = 1,   // 做任务 / 普通刷怪
        SCENE_FARM      = 2,   // 聚怪刷材料
        SCENE_DUNGEON   = 3,   // 5人副本
        SCENE_RAID      = 4,   // 团本
        SCENE_MYTHIC    = 5,   // 高级副本/团本（英雄团）
        SCENE_MAX       = 6
    };

    char const* SceneName(uint8 s);

    // 每个场景的行为参数
    struct SceneTuning
    {
        uint32 aoeThreshold;      // 周围几个怪才放 AOE
        uint32 defensiveHpPct;    // 血量低于百分之几开保命
        uint32 healSelfPct;       // 治疗给自己加血的血线
        uint32 healTargetPct;     // 治疗给队友加血的血线
        uint32 emergencyPct;      // 紧急救人血线（用大招）
        bool   keepBuffs;         // 是否严格维持增益
        bool   saveBurst;         // 是否省着用爆发（留给BOSS）
        char const* desc;
    };

    SceneTuning const& GetTuning(uint8 scene);

    // 一条技能项：法术 ID + 使用条件
    enum SkillFlag : uint32
    {
        SF_NONE          = 0x00,
        SF_MELEE         = 0x01,   // 需要贴身（近战距离内才放）
        SF_EXECUTE       = 0x02,   // 斩杀技，目标血量低于阈值才放
        SF_AOE           = 0x04,   // AOE，周围怪 >= 阈值 才放
        SF_NO_MOVE       = 0x08,   // 读条技，移动中跳过（下一 tick 再来）
        SF_SELF          = 0x10,   // 对自己放
        SF_DEBUFF_KEEP   = 0x20,   // DOT/debuff，目标身上没有才放
        SF_BUFF_KEEP     = 0x40,   // BUFF，自己身上没有才放
        SF_COMBO_FINISH  = 0x80,   // 终结技，需要连击点
        SF_COMBO_BUILD   = 0x100,  // 生成连击点

        // ---- v3 新增 ----
        SF_HEAL          = 0x200,  // 治疗技，对队友放
        SF_HEAL_AOE      = 0x400,  // 群体治疗，多人受伤才放
        SF_HEAL_EMERG    = 0x800,  // 紧急救命（大招，血线极低才用）
        SF_HOT           = 0x1000, // 持续治疗，目标没有才上
        SF_TAUNT         = 0x2000, // 嘲讽，目标不在打我才用
        SF_TAUNT_AOE     = 0x4000, // 群嘲
        SF_DISPEL_FRIEND = 0x8000, // 驱散队友身上的负面
        SF_FREE_SELF     = 0x10000,// 自我解控（中控时才用）
        SF_RAID_BUFF     = 0x20000,// 团队增益（给全团上）
        SF_MANA_LOW      = 0x40000,// 蓝少时才用（回蓝技）
        SF_INTERRUPT     = 0x80000,// 打断，目标在读条才用
    };

    struct Skill
    {
        uint32      spell;      // rank-1 法术 ID
        uint32      flags;      // SkillFlag 组合
        char const* cn;         // 中文名（显示用，不依赖 DBC 语言包）
    };

    struct SpecInfo
    {
        uint8       cls;        // 职业 ID
        uint8       specIdx;    // 0/1/2/3 专精序号
        char const* name;       // 专精中文名
        char const* role;       // 定位：坦克/治疗/近战DPS/远程DPS
        uint8       defaultRole;// 推荐职责（TeamRole）
        std::vector<Skill> rotation;
        std::vector<Skill> burst;
        std::vector<Skill> defensive;
        std::vector<Skill> buffs;

        // ---- v3 新增：职责专属技能组 ----
        std::vector<Skill> tankKit;   // 当坦克时额外用（嘲讽/减伤/拉怪）
        std::vector<Skill> healKit;   // 当治疗时的治疗循环
        std::vector<Skill> utility;   // 驱散/解控/打断/回蓝
        std::vector<Skill> filler;    // v3.3 填充技：前面全CD时用，不让连招有空隙
    };


    // ------------------------------------------------------------------
    //  返回全部专精表（静态构造一次）
    // ------------------------------------------------------------------
    std::vector<SpecInfo> const& GetAllSpecs();

    // 取某职业的全部专精
    void GetSpecsOfClass(uint8 cls, std::vector<SpecInfo const*>& out);

    // 取指定专精
    SpecInfo const* GetSpec(uint8 cls, uint8 specIdx);

    // 该职业有几个专精窗口
    uint8 GetSpecCount(uint8 cls);

    // ------------------------------------------------------------------
    //  v3：按「专精 + 职责 + 场景」组装最终技能序列
    // ------------------------------------------------------------------
    /*
     * 这是 v3 的核心。同一个专精，选不同职责出来的循环完全不同：
     *
     *   防护战 + 坦克  -> tankKit(嘲讽/减伤) 优先，然后 rotation
     *   防护战 + 输出  -> 直接 rotation，不浪费 GCD
     *   神圣骑 + 治疗  -> healKit 优先，血线安全时才用 rotation 打伤害
     *   神圣骑 + 输出  -> rotation 打伤害，healKit 只在自己血少时用
     *
     * 输出顺序就是优先级顺序，.combo 从头往下找第一个能放的。
     */
    struct BuiltPlan
    {
        std::vector<Skill> opener;     // 开场/维持：增益、光环
        std::vector<Skill> emergency;  // 最高优先：解控、紧急救命、保命
        std::vector<Skill> core;       // 主循环（含职责专属）
        std::vector<Skill> burst;      // 爆发
        std::vector<Skill> utility;    // 驱散/打断/回蓝
        char const* specName;
        char const* roleName;
        char const* sceneName;
    };

    // allSpecs = true 时合并该职业所有专精，按「最强顺序」排
    void BuildPlan(uint8 cls, uint8 specIdx, uint8 role, uint8 scene,
                   bool allSpecs, BuiltPlan& out);

    // 根据专精推荐职责
    uint8 SuggestRole(uint8 cls, uint8 specIdx);
}

#endif
