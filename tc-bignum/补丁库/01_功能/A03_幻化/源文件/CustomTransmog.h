/*
 * CustomTransmog.h - 幻化系统（Transmogrification）
 *
 * 本服自定义模块。
 *
 * 核心原理（已逐行核实）：
 *   全服只有 Player::SetVisibleItemSlot()（Player.cpp:12170）一处写外观字段：
 *       SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), pItem->GetEntry());
 *   调用点仅 5 处（12205 / 12287 / 12417 / 17279 + 定义本身）。
 *   在这一个函数里插「查幻化缓存」，所有场景自动生效：
 *       登录、换装、跨图、进副本、别人看你、镜像/幻象。
 *
 * 时序陷阱（必须用内存缓存的原因）：
 *   Player.cpp:17279 在 LoadFromDB 内会遍历 19 个槽位调 SetVisibleItemSlot(slot, nullptr)，
 *   随后 17764 行才 _LoadInventory() 填回装备。
 *   而 PlayerScript::OnLogin 钩子在整个 LoadFromDB 之后才触发 ——
 *   若靠 OnLogin 去查库，登录瞬间外观是空的，必须换装才显示。
 *   因此改用「服务器启动时全量载入内存」，SetVisibleItemSlot 直接查内存：
 *       零 DB 查询、零时序问题、跨图不失效。
 *
 * 设计决策（用户拍板）：
 *   - 完全自由：任何装备可幻化成任何外观，不限甲类/职业/等级
 *   - 双模式：默认填 ID 即可；Transmog.RequireItem=1 时要求背包内拥有该物品
 *   - 存储：新建表 characters.custom_transmog（不碰官方表）
 *   - 支持多套外观方案保存/切换（characters.custom_transmog_sets）
 */

#ifndef _CUSTOM_TRANSMOG_H
#define _CUSTOM_TRANSMOG_H

#include "Define.h"
#include "ObjectGuid.h"
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class Player;
class Item;

// 装备槽位数（Player.h:576 EQUIPMENT_SLOT_END = 19）
#define TRANSMOG_MAX_SLOT 19

class TC_GAME_API CustomTransmogMgr
{
    public:
        static CustomTransmogMgr* instance();

        // ---------- 生命周期 ----------

        // 服务器启动时全量载入（在 World.cpp 的加载序列里调用）
        void LoadFromDB();

        // 读取配置
        void LoadConfig();

        // ---------- 开关 ----------
        bool Enabled() const { return _enabled; }
        bool RequireItem() const { return _requireItem; }
        uint32 CostPerSlot() const { return _costPerSlot; }
        bool AllowWeaponCross() const { return _allowWeaponCross; }
        uint32 PreviewSeconds() const { return _previewSeconds; }

        // ---------- 核心查询（SetVisibleItemSlot 里调用，必须极快）----------

        /*
         * 取某玩家某槽位的幻化外观 entry。
         * @return 0 表示无幻化，调用方使用原装备 entry
         *
         * 这个函数在每次装备变化/登录时被调用 19 次，
         * 必须是纯内存查找，不能碰数据库。
         */
        uint32 GetFakeEntry(ObjectGuid::LowType guidLow, uint8 slot) const;

        // 该玩家是否有任何幻化（快速短路用）
        bool HasAny(ObjectGuid::LowType guidLow) const;

        // ---------- 修改 ----------

        // 设置单槽幻化。fakeEntry=0 等同于移除
        void SetFakeEntry(ObjectGuid::LowType guidLow, uint8 slot, uint32 fakeEntry);

        // 移除单槽
        void RemoveSlot(ObjectGuid::LowType guidLow, uint8 slot);

        // 清空该玩家全部幻化
        void ClearAll(ObjectGuid::LowType guidLow);

        // 删除角色时清理（PlayerScript::OnDelete 调用）
        void OnCharacterDeleted(ObjectGuid::LowType guidLow);

        // ---------- 外观方案（多套保存）----------

        // 保存当前幻化为命名方案
        bool SaveSet(ObjectGuid::LowType guidLow, std::string const& name);

        // 载入命名方案（只改缓存与库，调用方负责刷新外观）
        bool LoadSet(ObjectGuid::LowType guidLow, std::string const& name);

        // 删除命名方案
        bool DeleteSet(ObjectGuid::LowType guidLow, std::string const& name);

        // 列出该玩家所有方案名
        std::vector<std::string> ListSets(ObjectGuid::LowType guidLow) const;

        // ---------- 辅助 ----------

        // 校验 fakeEntry 是否可用作外观（存在 + 有模型）
        // @param err 失败原因（中文），可为 nullptr
        bool ValidateEntry(uint32 fakeEntry, std::string* err = nullptr) const;

        // 校验某槽位能否接受该外观（完全自由模式下只查槽位类型是否匹配）
        bool ValidateSlot(uint8 slot, uint32 fakeEntry, std::string* err = nullptr) const;

        // 把玩家身上 19 个槽位的外观全部重刷一遍（改完幻化后调用）
        static void RefreshPlayer(Player* player);

        // 槽位中文名（用于指令与菜单显示）
        static char const* SlotName(uint8 slot);

        // 中文槽位名 -> 槽位号，失败返回 TRANSMOG_MAX_SLOT
        static uint8 SlotFromName(std::string const& name);

    private:
        CustomTransmogMgr() { }
        ~CustomTransmogMgr() { }
        CustomTransmogMgr(CustomTransmogMgr const&) = delete;
        CustomTransmogMgr& operator=(CustomTransmogMgr const&) = delete;

        typedef std::array<uint32, TRANSMOG_MAX_SLOT> SlotArray;

        // guidLow -> 19 个槽位的 fakeEntry
        std::unordered_map<ObjectGuid::LowType, SlotArray> _data;

        // guidLow -> (方案名 -> 19 槽)
        std::unordered_map<ObjectGuid::LowType, std::unordered_map<std::string, SlotArray>> _sets;

        bool _enabled = true;
        bool _requireItem = false;
        uint32 _costPerSlot = 0;
        bool _allowWeaponCross = true;
        uint32 _maxSets = 10;
        uint32 _previewSeconds = 15;
};

#define sCustomTransmog CustomTransmogMgr::instance()

#endif // _CUSTOM_TRANSMOG_H
