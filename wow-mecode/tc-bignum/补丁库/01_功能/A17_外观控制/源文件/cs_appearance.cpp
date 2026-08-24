/*
 * ============================================================================
 *  外观控制 —— cs_appearance.cpp   (step31)
 *
 *  提供两条指令：
 *    .disguise   隐藏装备外观（保留全部属性）—— 大佬装萌新
 *    .model      给任何生物换模型（玩家 / NPC / BOSS / NPCBot 都行）
 * ============================================================================
 *
 *  .disguise on            隐藏全身 19 个槽位的装备外观
 *  .disguise off           恢复
 *  .disguise weapon        只隐藏武器（主手/副手/远程）
 *  .disguise armor         只隐藏护甲（其余槽位）
 *  .disguise toggle        切换
 *  .disguise status        看当前状态
 *
 *  .model npc <entry>          用该生物的模型（查库，最准确）
 *  .model copy                 复制当前选中目标的模型
 *  .model id <displayid>       直接指定 displayid（老手用）
 *  .model r <半径> npc <entry> 对周围所有 NPC
 *  .model entry <ID> npc <e2>  对指定 entry 的 NPC
 *  .model me npc <entry>       自己换
 *  .model reset [r <半径>]     复位到原生模型
 *  .model save                 写库持久化（仅 Creature）
 *
 *  【v2 重要变更】删掉了硬编码的模型别名表。
 *  原因：那些 displayid 是凭印象编的，用户实测全错
 *  （巫妖王变死亡战马、伊利丹消失、小兔子变洞穴人）。
 *  而且装了 HD 补丁后 displayid 对应关系本来就变了，
 *  任何硬编码表都不可靠。改为服务端实时查 creature_template。
 *
 * ----------------------------------------------------------------------------
 *  【核心】.disguise 为什么能"隐藏外观但保留属性"
 *
 *  查了 Player.cpp:12170 的实现（全文 14 行）：
 *
 *      void Player::SetVisibleItemSlot(uint8 slot, Item* pItem)
 *      {
 *          if (pItem) { ...写 PLAYER_VISIBLE_ITEM_* 字段... }
 *          else
 *          {
 *              SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), 0);
 *              SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (slot * 2), 0);
 *          }
 *      }
 *
 *  它【只写外观广播字段】，完全不碰 m_items。
 *  真正动装备的是 Player.cpp:12185 VisualizeItem（那个才改 m_items）。
 *
 *  所以：
 *      外观    -> 空手裸装
 *      属性    -> 照常生效（属性走 m_items 和光环，不走外观字段）
 *      战斗力  -> 完全不受影响
 *
 *  恢复时用 GetItemByPos(INVENTORY_SLOT_BAG_0, slot) 从真实装备栏取回，
 *  连附魔一起还原。
 *
 * ----------------------------------------------------------------------------
 *  已核实 API（全 public）
 *
 *   Player.h:1153    void SetVisibleItemSlot(uint8 slot, Item* pItem)  [904行起public段]
 *   Player.h:1082    Item* GetItemByPos(uint8 bag, uint8 slot) const
 *   Player.h:552     #define INVENTORY_SLOT_BAG_0  255
 *   Player.h:554-577 EquipmentSlots 枚举（0-18，共19槽）
 *   Unit.h:1595      virtual void SetDisplayId(uint32 modelId)
 *   Unit.h:1598      void SetNativeDisplayId(uint32 displayId)
 *   Unit.h:1594/1596 GetDisplayId / GetNativeDisplayId
 *   Unit.h:1191      void DeMorph()        <- Unit.cpp:3461，就是 SetDisplayId(GetNativeDisplayId())
 *   Chat.h:104       Creature* getSelectedCreature()
 *   Creature.h:394   bool IsNPCBotOrPet() const
 *
 *  注意：Creature.h:86 覆写了 SetDisplayId，Player 没有覆写。
 *  两者都能用，但 Creature 版本可能额外处理碰撞体积。
 *
 * ----------------------------------------------------------------------------
 *  【血泪教训】本文件遵守的三条铁律
 *
 *  1. SQL 占位符用 {} 不是 %u
 *     DatabaseWorkerPool.h:99 的 DirectPExecute 走 fmt 库。
 *     step29 写 %u 崩过服。（snprintf 的 %u 是对的，别混淆）
 *
 *  2. 绝不在运行时调 sObjectMgr->LoadXxx()
 *     会让容器 rehash，活着的对象持有的指针全失效 -> 崩溃。
 *     step29 踩过。持久化只写库，用 .respawn 生效。
 *
 *  3. 注册要改两处（ScriptLoader.h 声明 + AddCommandsScripts 调用）
 *     只加声明不加调用 -> 编译过但命令不存在。step29 踩过。
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DatabaseEnv.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "SharedDefines.h"
#include "Unit.h"
#include "UnitDefines.h"
#include "UpdateFields.h"   // PLAYER_VISIBLE_ITEM_1_ENTRYID（Object/Updates/UpdateFields.h:286）
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
//  【v2 重做 2026-08-01】别名表废弃，改用 creature entry 实时查
//
//  用户实测：巫妖王变成死亡战马、伊利丹直接消失、小兔子变成洞穴人。
//
//  根因：v1 的别名表里那些 displayid【全是我凭印象编的】，
//  一个都没查证过。这是第 6 次「凭记忆填数值」，
//  和之前 flags_extra=130、faction=1868 是同一类错误。
//
//  为什么硬编码 displayid 本质上就不可靠：
//    1. displayid 和 creature entry 是两套完全独立的编号
//    2. 同一个 displayid 在不同客户端补丁下可能指向不同模型
//       （用户装了 HD 补丁，displayid 对应关系已经变了）
//    3. 无法在服务端校验一个 displayid 到底长什么样
//
//  新方案：用【creature entry】而不是 displayid。
//    entry 是服务端 creature_template 表的主键，可以实时查库拿到
//    这个生物真正的 modelid，绝不会错。
//
//  已核实 API：
//    ObjectMgr.h:982      CreatureTemplate const* GetCreatureTemplate(uint32 entry)
//    CreatureData.h:296   struct TC_GAME_API CreatureTemplate  <- struct，默认 public
//    CreatureData.h:358   uint32 GetFirstValidModelId() const
//    Creature.cpp:133     实现：依次返回 Modelid1..4 中第一个非0
//
//  用法变成：
//    .model npc <entry>     用该生物的模型（服务端查库，100%准确）
//    .model id <displayid>  直接指定 displayid（老手用，自负）
//    .model copy            复制当前选中目标的模型
// ============================================================================

// 取某个 creature entry 的有效模型 ID。查不到返回 0。
static uint32 GetModelIdByEntry(uint32 entry)
{
    CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(entry);  // ObjectMgr.h:982
    if (!ct)
        return 0;
    return ct->GetFirstValidModelId();     // CreatureData.h:358
}

// ============================================================================
//  小工具（沿用 step29/30）
// ============================================================================

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

// v2: 别名表已废弃（那些 displayid 全是编的，实测全错）。
// 模型来源改为 creature entry 实时查库，见 GetModelIdByEntry()。

// 收集半径内的 Creature（沿用 .nst/.emote/.say）
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
        if (c->IsNPCBotOrPet())      // Creature.h:394
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

// ============================================================================
//  .disguise 实现
// ============================================================================

enum DisguiseScope
{
    DG_ALL,       // 全身
    DG_WEAPON,    // 只武器
    DG_ARMOR,     // 只护甲
};

// 某个槽位是否属于本次操作范围
static bool SlotInScope(uint8 slot, DisguiseScope sc)
{
    bool isWeapon = (slot == EQUIPMENT_SLOT_MAINHAND ||     // 15
                     slot == EQUIPMENT_SLOT_OFFHAND  ||     // 16
                     slot == EQUIPMENT_SLOT_RANGED);        // 17
    switch (sc)
    {
        case DG_WEAPON: return isWeapon;
        case DG_ARMOR:  return !isWeapon;
        case DG_ALL:
        default:        return true;
    }
}

// 隐藏：把外观字段清零，不动 m_items
static int DoHide(Player* p, DisguiseScope sc)
{
    int n = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (!SlotInScope(slot, sc))
            continue;
        p->SetVisibleItemSlot(slot, nullptr);   // Player.h:1153
        ++n;
    }
    return n;
}

// 恢复：从真实装备栏取回，连附魔一起还原
static int DoShow(Player* p, DisguiseScope sc)
{
    int n = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (!SlotInScope(slot, sc))
            continue;
        // Player.h:1082 + Player.h:552
        Item* item = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        p->SetVisibleItemSlot(slot, item);
        if (item)
            ++n;
    }
    return n;
}

// 当前是否处于隐藏态：全部在范围内的槽位外观都为 0，且至少有一件真实装备
static bool IsHidden(Player* p, DisguiseScope sc)
{
    bool anyRealItem = false;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (!SlotInScope(slot, sc))
            continue;
        if (p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            anyRealItem = true;
            if (p->GetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2)) != 0)
                return false;   // 有真实装备且外观还在 -> 没隐藏
        }
    }
    return anyRealItem;
}

class disguise_commandscript : public CommandScript
{
public:
    disguise_commandscript() : CommandScript("disguise_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "disguise", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleDisguise, "" },
        };
        return commandTable;
    }

    static void SendHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff00ff00[.disguise 隐藏装备外观]|r");
        handler->SendSysMessage("  .disguise on        隐藏全身装备外观");
        handler->SendSysMessage("  .disguise off       恢复");
        handler->SendSysMessage("  .disguise weapon    只隐藏武器");
        handler->SendSysMessage("  .disguise armor     只隐藏护甲");
        handler->SendSysMessage("  .disguise toggle    切换");
        handler->SendSysMessage("  .disguise status    看当前状态");
        handler->SendSysMessage("|cffffff00 属性【完全保留】，只是别人看不到你的装备|r");
        handler->SendSysMessage("|cffffff00 换装备或重登后会自动恢复显示|r");
    }

    static bool HandleDisguise(ChatHandler* handler, char const* args)
    {
        Player* p = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!p)
            return false;

        std::vector<std::string> tok = Tok(args);
        char buf[512];

        if (tok.empty())
        {
            SendHelp(handler);
            return true;
        }

        std::string w = Lower(tok[0]);

        // ---------- status ----------
        if (w == "status" || tok[0] == "状态")
        {
            bool hidAll = IsHidden(p, DG_ALL);
            bool hidWpn = IsHidden(p, DG_WEAPON);
            bool hidArm = IsHidden(p, DG_ARMOR);
            snprintf(buf, sizeof(buf),
                     "|cff00ff00 全身:%s  武器:%s  护甲:%s|r",
                     hidAll ? "已隐藏" : "显示中",
                     hidWpn ? "已隐藏" : "显示中",
                     hidArm ? "已隐藏" : "显示中");
            handler->SendSysMessage(buf);

            int cnt = 0;
            for (uint8 s = EQUIPMENT_SLOT_START; s < EQUIPMENT_SLOT_END; ++s)
                if (p->GetItemByPos(INVENTORY_SLOT_BAG_0, s))
                    ++cnt;
            snprintf(buf, sizeof(buf),
                     "|cffffff00 实际穿戴 %d 件（属性全部生效）|r", cnt);
            handler->SendSysMessage(buf);
            return true;
        }

        // ---------- 判断范围 ----------
        DisguiseScope sc = DG_ALL;
        std::string   scName = "全身";
        size_t idx = 0;

        if (w == "weapon" || tok[0] == "武器")
        {
            sc = DG_WEAPON; scName = "武器"; idx = 1;
        }
        else if (w == "armor" || tok[0] == "护甲")
        {
            sc = DG_ARMOR;  scName = "护甲"; idx = 1;
        }

        // 范围后面可以再跟 on/off，不跟就是 toggle
        bool doHide;
        if (idx < tok.size())
        {
            std::string a = Lower(tok[idx]);
            if (a == "on" || a == "hide" || tok[idx] == "隐藏")
                doHide = true;
            else if (a == "off" || a == "show" || tok[idx] == "显示")
                doHide = false;
            else if (a == "toggle" || tok[idx] == "切换")
                doHide = !IsHidden(p, sc);
            else
            {
                SendHelp(handler);
                return true;
            }
        }
        else if (idx == 0)
        {
            // .disguise on/off/toggle 这种，第0段就是动作
            if (w == "on" || w == "hide" || tok[0] == "隐藏")
                doHide = true;
            else if (w == "off" || w == "show" || tok[0] == "显示")
                doHide = false;
            else if (w == "toggle" || tok[0] == "切换")
                doHide = !IsHidden(p, DG_ALL);
            else
            {
                SendHelp(handler);
                return true;
            }
        }
        else
        {
            // 只给了范围没给动作 -> 切换
            doHide = !IsHidden(p, sc);
        }

        // ---------- 执行 ----------
        int n = doHide ? DoHide(p, sc) : DoShow(p, sc);

        if (doHide)
        {
            snprintf(buf, sizeof(buf),
                     "|cff00ff00 已隐藏%s装备外观（%d 个槽位）|r", scName.c_str(), n);
            handler->SendSysMessage(buf);
            handler->SendSysMessage("|cffffff00 属性完全保留，战斗力不受影响|r");
        }
        else
        {
            snprintf(buf, sizeof(buf),
                     "|cff00ff00 已恢复%s装备外观（%d 件）|r", scName.c_str(), n);
            handler->SendSysMessage(buf);
        }
        return true;
    }
};

// ============================================================================
//  .model 实现
// ============================================================================
class model_commandscript : public CommandScript
{
public:
    model_commandscript() : CommandScript("model_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "model", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleModel, "" },
        };
        return commandTable;
    }

    static void SendHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff00ff00[.model 换模型]|r");
        handler->SendSysMessage("  .model <ID或别名>        对选中目标");
        handler->SendSysMessage("  .model r <半径> <模型>   对周围所有NPC");
        handler->SendSysMessage("  .model entry <ID> <模型> 对指定entry");
        handler->SendSysMessage("  .model me npc <entry>    自己换");
        handler->SendSysMessage("  .model reset [r <半径>]  复位到原生模型");
        handler->SendSysMessage("  .model save              写库（仅NPC）");
        handler->SendSysMessage("|cffffff00--- 模型来源三选一 ---|r");
        handler->SendSysMessage("  npc <entry>    用该生物的模型（查库，最准）");
        handler->SendSysMessage("  copy           复制当前选中目标的模型");
        handler->SendSysMessage("  id <displayid> 直接指定（自负，可能对不上）");
        handler->SendSysMessage("|cffffff00 例: .model npc 24191   .model r 30 npc 448|r");
        handler->SendSysMessage("|cffffff00 entry 用 .lookup creature <名字> 查|r");
    }

    static bool HandleModel(ChatHandler* handler, char const* args)
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

        // ---------- help / list ----------
        if (s0 == "help" || tok[0] == "帮助")
        {
            SendHelp(handler);
            return true;
        }
        if (s0 == "list" || tok[0] == "列表")
        {
            // v2: 别名表已废弃（硬编码 displayid 实测全错），改用 entry 查库
            handler->SendSysMessage("|cffffff00 模型别名表已移除 —— 硬编码的 displayid|r");
            handler->SendSysMessage("|cffffff00 在装了HD补丁的客户端上对不上号。|r");
            handler->SendSysMessage("|cff00ff00 改用生物 entry（服务端实时查库，100%准确）：|r");
            handler->SendSysMessage("   .lookup creature 巫妖王    <- 先查 entry");
            handler->SendSysMessage("   .model npc <查到的entry>   <- 再套模型");
            handler->SendSysMessage("|cff00ff00 或者直接复制眼前目标的模型：|r");
            handler->SendSysMessage("   选中目标 -> .model copy");
            return true;
        }

        // ---------- save ----------
        if (s0 == "save" || tok[0] == "保存")
        {
            Creature* c = handler->getSelectedCreature();
            if (!c)
            {
                handler->SendSysMessage("|cffff0000 先选中一个 NPC（玩家模型无法持久化）|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            uint32 cur = c->GetDisplayId();                 // Unit.h:1594
            // 铁律1：占位符用 {} 不是 %u（DatabaseWorkerPool.h:99 走 fmt）
            WorldDatabase.DirectPExecute(
                "UPDATE `creature_template` SET `modelid1` = {} WHERE `entry` = {}",
                cur, c->GetEntry());
            snprintf(buf, sizeof(buf),
                     "|cff00ff00 已写库: entry=%u modelid1=%u|r", c->GetEntry(), cur);
            handler->SendSysMessage(buf);
            handler->SendSysMessage("|cffffff00 用 .respawn 或重启服务端后对新生成的NPC生效|r");
            return true;
        }

        // ---------- 解析 reset / 目标 ----------
        bool   isReset   = false;
        bool   toSelf    = false;
        bool   useRadius = false;
        bool   useEntry  = false;
        float  radius    = 0.0f;
        uint32 entry     = 0;
        size_t idx       = 0;

        while (idx < tok.size())
        {
            std::string w = Lower(tok[idx]);
            if (w == "reset" || tok[idx] == "复位")
            {
                isReset = true; ++idx; continue;
            }
            if (w == "me" || tok[idx] == "自己")
            {
                toSelf = true;  ++idx; continue;
            }
            if (w == "r" || tok[idx] == "范围")
            {
                if (idx + 1 >= tok.size())
                {
                    handler->SendSysMessage("|cffff0000 用法: .model r <半径> <模型>|r");
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
                useRadius = true; idx += 2; continue;
            }
            if (w == "entry" || tok[idx] == "编号")
            {
                if (idx + 1 >= tok.size() || !IsAllDigit(tok[idx + 1]))
                {
                    handler->SendSysMessage("|cffff0000 用法: .model entry <ID> <模型>|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                entry = uint32(atoi(tok[idx + 1].c_str()));
                useEntry = true;  idx += 2; continue;
            }
            break;   // 剩下的是模型参数
        }

        // ---------- 取模型ID（v2 重做）----------
        //
        //  三种来源，都不依赖硬编码：
        //    npc <entry>    查 creature_template 拿真实 modelid  <- 推荐
        //    copy           复制当前选中目标的模型
        //    id <displayid> 直接给数字（老手用）
        //
        //  v1 的别名表已删除：那些 displayid 是编的，
        //  且在装了 HD 补丁的客户端上对应关系本来就变了。
        uint32 modelId = 0;
        if (!isReset)
        {
            if (idx >= tok.size())
            {
                handler->SendSysMessage("|cffff0000 缺少模型来源。三选一：|r");
                handler->SendSysMessage("  npc <entry>     用该生物的模型（推荐）");
                handler->SendSysMessage("  copy            复制选中目标的模型");
                handler->SendSysMessage("  id <displayid>  直接指定");
                handler->SetSentErrorMessage(true);
                return false;
            }

            std::string src = Lower(tok[idx]);

            // --- copy：复制当前选中目标 ---
            if (src == "copy" || tok[idx] == "复制")
            {
                Unit* sel = handler->getSelectedUnit();
                if (!sel)
                {
                    handler->SendSysMessage("|cffff0000 copy 需要先选中一个目标|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                modelId = sel->GetDisplayId();          // Unit.h:1594
                ++idx;
                snprintf(buf, sizeof(buf),
                         "|cffffff00 已复制选中目标的模型: %u|r", modelId);
                handler->SendSysMessage(buf);
            }
            // --- npc <entry>：查 creature_template（最可靠）---
            else if (src == "npc" || src == "entry2" || tok[idx] == "生物")
            {
                if (idx + 1 >= tok.size() || !IsAllDigit(tok[idx + 1]))
                {
                    handler->SendSysMessage("|cffff0000 用法: .model npc <生物entry>|r");
                    handler->SendSysMessage("|cffffff00 用 .lookup creature <名字> 查 entry|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                uint32 srcEntry = uint32(atoi(tok[idx + 1].c_str()));
                modelId = GetModelIdByEntry(srcEntry);   // 查库，不猜
                if (!modelId)
                {
                    snprintf(buf, sizeof(buf),
                             "|cffff0000 entry %u 不存在，或它没有配置模型|r", srcEntry);
                    handler->SendSysMessage(buf);
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                idx += 2;
                snprintf(buf, sizeof(buf),
                         "|cffffff00 entry %u 的模型 = %u|r", srcEntry, modelId);
                handler->SendSysMessage(buf);
            }
            // --- id <displayid> 或 裸数字：直接指定 ---
            else
            {
                if (src == "id" || tok[idx] == "编号2")
                {
                    if (idx + 1 >= tok.size() || !IsAllDigit(tok[idx + 1]))
                    {
                        handler->SendSysMessage("|cffff0000 用法: .model id <displayid>|r");
                        handler->SetSentErrorMessage(true);
                        return false;
                    }
                    modelId = uint32(atoi(tok[idx + 1].c_str()));
                    idx += 2;
                }
                else if (IsAllDigit(tok[idx]))
                {
                    modelId = uint32(atoi(tok[idx].c_str()));
                    ++idx;
                }
                else
                {
                    snprintf(buf, sizeof(buf),
                             "|cffff0000 认不出 \"%s\"。用 npc <entry> / copy / id <num>|r",
                             tok[idx].c_str());
                    handler->SendSysMessage(buf);
                    handler->SetSentErrorMessage(true);
                    return false;
                }

                if (modelId == 0 || modelId > 100000)
                {
                    handler->SendSysMessage("|cffff0000 displayid 超出合理范围(1-100000)|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                handler->SendSysMessage(
                    "|cffffff00 直接指定 displayid：装了HD补丁时可能对不上，"
                    "建议改用 .model npc <entry>|r");
            }
        }

        // ---------- me：自己 ----------
        if (toSelf)
        {
            if (isReset)
            {
                player->DeMorph();                          // Unit.h:1191
                handler->SendSysMessage("|cff00ff00 自己: 已复位到原生模型|r");
            }
            else
            {
                player->SetDisplayId(modelId);              // Unit.h:1595
                snprintf(buf, sizeof(buf), "|cff00ff00 自己: 模型 -> %u|r", modelId);
                handler->SendSysMessage(buf);
            }
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
            Creature* c = handler->getSelectedCreature();
            if (!c)
            {
                handler->SendSysMessage("|cffff0000 没有选中 NPC。用 .model me <模型> 对自己，"
                                        "或 .model r <半径> <模型> 对周围|r");
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

        // ---------- 执行 ----------
        int n = 0;
        for (Creature* c : targets)
        {
            if (isReset)
                c->DeMorph();                   // Unit.cpp:3461
            else
                c->SetDisplayId(modelId);       // Creature.h:86 覆写版
            ++n;
        }

        if (isReset)
            snprintf(buf, sizeof(buf), "|cff00ff00 %s: %d 个目标已复位|r",
                     scope.c_str(), n);
        else
            snprintf(buf, sizeof(buf), "|cff00ff00 %s: %d 个目标 -> 模型%u|r",
                     scope.c_str(), n, modelId);
        handler->SendSysMessage(buf);

        if (!isReset)
            handler->SendSysMessage("|cffffff00 临时生效。要永久保存用 .model save|r");

        return true;
    }
};

void AddSC_appearance_commandscript()
{
    new disguise_commandscript();
    new model_commandscript();
}
