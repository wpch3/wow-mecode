/*
 * CustomStatPersist.h - GM 属性修改持久化
 *
 * ============================================================================
 *  解决什么
 * ============================================================================
 *
 *  `.modify stat` / `.set crit` 改的属性【重登就没了】。
 *
 *  根因：
 *    · .modify 用 SetStatFlatModifier()（Unit.h:1521）—— 只改内存里的 UnitMods 数组
 *    · .set    用 ApplyRatingMod()（Player.h:1649）    —— 只改内存里的 rating
 *    · characters 表里没有存这些修正值的字段
 *    · 所以 SaveToDB 不会保存，重登内存重建 = 归零
 *
 *  装备属性为什么不会丢？因为它是每次登录从 character_inventory 重新算出来的。
 *  而 GM 手动加的属性是凭空来的，无处可存。
 *
 * ============================================================================
 *  方案：复用幻化模块的成熟架构
 * ============================================================================
 *
 *    存储      characters.custom_playerstat（新表，不碰官方表）
 *    缓存      服务器启动时全量载入内存
 *    应用      PlayerScript::OnLogin（ScriptMgr.h:692）重新 apply 一遍
 *    保存      改的时候直接写库
 *
 *  和幻化的差异：
 *    幻化必须在 SetVisibleItemSlot 里查缓存（有时序陷阱），
 *    属性只需要 OnLogin 时重新 apply —— 登录后才算属性，不存在抢跑。
 *
 * ============================================================================
 *  两类属性
 * ============================================================================
 *
 *    TYPE_UNITMOD  —— .modify stat 那类（力量/敏捷/耐力/智力/精神/攻强/法伤...）
 *                     对应 UnitMods 枚举（Unit.h:157-186，共 26 项）
 *                     用 SetStatFlatModifier(unitMod, TOTAL_VALUE, val)
 *
 *    TYPE_RATING   —— .set 那类（暴击/命中/急速/精准/躲闪...）
 *                     对应 CombatRating 枚举（Unit.h:322-349，共 25 项）
 *                     用 ApplyRatingMod(cr, val, true)
 *
 * ============================================================================
 *  和 Solocraft 的关系
 * ============================================================================
 *
 *  Solocraft 用 SetStatPctModifier（百分比槽 PCT），
 *  本模块用 SetStatFlatModifier（固定值槽 TOTAL_VALUE），
 *  两者是【不同的槽位】，互不干扰，不会互相覆盖。
 */

#ifndef _CUSTOM_STAT_PERSIST_H
#define _CUSTOM_STAT_PERSIST_H

#include "Define.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <vector>

class Player;

class TC_GAME_API CustomStatPersistMgr
{
    public:
        static CustomStatPersistMgr* instance();

        enum StatType : uint8
        {
            TYPE_UNITMOD = 0,   // .modify stat 类
            TYPE_RATING  = 1,   // .set 类
        };

        // ---------- 生命周期 ----------
        void LoadFromDB();      // 服务器启动时全量载入
        void LoadConfig();
        bool Enabled() const { return _enabled; }

        // ---------- 玩家事件 ----------
        void OnPlayerLogin(Player* player);       // 重新 apply
        void OnCharacterDeleted(ObjectGuid::LowType guidLow);

        // ---------- 记录/查询 ----------

        // 记一条修改（会写库）。amount = 0 表示删除该项
        void Record(ObjectGuid::LowType guidLow, StatType type, uint8 index, float amount);

        // 取某项当前记录值（没有则 0）
        float Get(ObjectGuid::LowType guidLow, StatType type, uint8 index) const;

        // 清空某玩家全部记录（会写库 + 立即从玩家身上撤销）
        void ClearAll(Player* player);

        // 该玩家有没有任何记录
        bool HasAny(ObjectGuid::LowType guidLow) const;

        // 列出某玩家的全部记录（给 .stat show 用）
        struct Entry
        {
            StatType type;
            uint8    index;
            float    amount;
        };
        std::vector<Entry> List(ObjectGuid::LowType guidLow) const;

        // ---------- 名字辅助 ----------
        static char const* UnitModName(uint8 idx);
        static char const* RatingName(uint8 idx);

    private:
        CustomStatPersistMgr() { }
        ~CustomStatPersistMgr() { }
        CustomStatPersistMgr(CustomStatPersistMgr const&) = delete;
        CustomStatPersistMgr& operator=(CustomStatPersistMgr const&) = delete;

        // key = (type << 8) | index
        typedef std::unordered_map<uint16, float> StatMap;
        std::unordered_map<ObjectGuid::LowType, StatMap> _data;

        static uint16 MakeKey(StatType t, uint8 idx) { return uint16((uint16(t) << 8) | idx); }

        bool _enabled = true;
};

#define sCustomStatPersist CustomStatPersistMgr::instance()

#endif // _CUSTOM_STAT_PERSIST_H
