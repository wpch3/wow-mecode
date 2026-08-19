/*
 * CustomSpellTweak.h - 老职业技能强化（A 级 · 零 DBC 成本）
 *
 * ============================================================================
 *  核心原理（已逐行核实）
 * ============================================================================
 *
 *  SpellMgr.h:671   SpellInfo* _GetSpellInfo(uint32 spellId)   <- 非 const！
 *  SpellInfo.h:387  std::array<SpellEffectInfo, 3> _effects    <- public 区
 *  SpellInfo.h:342  uint32 RecoveryTime / CategoryRecoveryTime / ...
 *  SpellInfo.h:226  uint32 ChainTargets
 *  SpellInfo.h:215  int32  BasePoints
 *
 *  意味着：服务器启动时可以【运行时改写任何现有法术】的数值、冷却、
 *  目标数、跳跃数 —— 而客户端只负责画图标和播动画，实际结算全在服务端。
 *
 *  => 改数值、改冷却、改射程、改目标数，全部【零 DBC 成本】，
 *     玩家不用下任何补丁。
 *
 * ============================================================================
 *  设计原则（见《老职业魔改-十职业扩充方案.txt》第一部）
 * ============================================================================
 *
 *  1. 不做数值膨胀 —— 补功能位，不是单纯加伤害
 *  2. 每个职业补它缺的，不是它已经强的
 *  3. 和新职业错位，不抢饭碗
 *
 *  你的环境有 Solocraft(5-40倍) + speed.conf(CD压缩)，
 *  所以这里一律用【温和倍率】，避免叠加后指数膨胀。
 *
 * ============================================================================
 *  安全性
 * ============================================================================
 *
 *  · 每条改动独立 conf 开关，不好玩单独关掉
 *  · 总开关 CustomSpell.Enable = 0 一键全关
 *  · 改动在服务器启动时一次性应用，不影响运行时性能
 *  · 所有改动都是【放大现有效果】，不新增效果，不会崩服
 *  · 改完 conf 需重启（法术数据在启动时加载）
 */

#ifndef _CUSTOM_SPELL_TWEAK_H
#define _CUSTOM_SPELL_TWEAK_H

#include "Define.h"
#include <string>
#include <vector>

class ChatHandler;

class TC_GAME_API CustomSpellTweakMgr
{
    public:
        static CustomSpellTweakMgr* instance();

        // 服务器启动时调用（World.cpp 里，必须在 LoadSpellInfoStore 之后）
        void ApplyAll();

        // 读配置
        void LoadConfig();

        bool Enabled() const { return _enabled; }

        // 已应用的改动数量
        uint32 AppliedCount() const { return _applied; }

        // 给 .spell tweak 指令用：列出所有改动
        struct TweakLog
        {
            uint32      spellId;
            std::string spellName;
            std::string what;       // 改了什么
            std::string cls;        // 职业
            bool        applied;    // 是否成功
            std::string reason;     // 失败原因
        };
        std::vector<TweakLog> const& GetLog() const { return _log; }

    private:
        CustomSpellTweakMgr() { }
        ~CustomSpellTweakMgr() { }
        CustomSpellTweakMgr(CustomSpellTweakMgr const&) = delete;
        CustomSpellTweakMgr& operator=(CustomSpellTweakMgr const&) = delete;

        // ---------- 底层操作 ----------

        // 放大法术效果数值（BasePoints）
        bool ScaleEffect(uint32 spellId, float mult, char const* cls, char const* desc);

        // 改冷却（毫秒）。mult < 1 = 变短
        bool ScaleCooldown(uint32 spellId, float mult, char const* cls, char const* desc);

        // 直接设定冷却（毫秒）
        bool SetCooldown(uint32 spellId, uint32 ms, char const* cls, char const* desc);

        // 改链式跳跃次数（治疗链、闪电链）
        bool AddChainTargets(uint32 spellId, int32 add, char const* cls, char const* desc);

        // 改最大影响目标数
        bool SetMaxTargets(uint32 spellId, uint32 n, char const* cls, char const* desc);

        // 改召唤数量（亡者大军等，改 BasePoints）
        bool SetSummonCount(uint32 spellId, int32 n, char const* cls, char const* desc);

        // 改持续时间倍率（改 DurationEntry 做不到，改 Amplitude/StackAmount 代替）
        bool ScaleStack(uint32 spellId, float mult, char const* cls, char const* desc);

        // 记录日志
        void Log(uint32 spellId, char const* cls, std::string const& what,
                 bool ok, char const* reason = "");

        // ---------- 各职业改动 ----------
        void ApplyWarrior();
        void ApplyPaladin();
        void ApplyHunter();
        void ApplyRogue();
        void ApplyPriest();
        void ApplyDeathKnight();
        void ApplyShaman();
        void ApplyMage();
        void ApplyWarlock();
        void ApplyDruid();

        bool _enabled = true;
        uint32 _applied = 0;
        std::vector<TweakLog> _log;

        // 各职业开关
        bool _warrior = true, _paladin = true, _hunter = true, _rogue = true;
        bool _priest  = true, _dk = true, _shaman = true, _mage = true;
        bool _warlock = true, _druid = true;

        // 全局倍率（conf 可调，方便整体收放）
        float _globalScale = 1.0f;
};

#define sCustomSpellTweak CustomSpellTweakMgr::instance()

#endif // _CUSTOM_SPELL_TWEAK_H
