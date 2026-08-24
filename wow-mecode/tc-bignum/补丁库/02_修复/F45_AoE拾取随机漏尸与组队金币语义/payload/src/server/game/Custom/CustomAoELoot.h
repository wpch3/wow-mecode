/*
 * CustomAoELoot.h - F45 群体拾取可靠性修复
 *
 * F45 关键约束：
 *   1. 主入口由“合法打开生物尸体拾取窗”触发，不依赖成功存入某个物品槽。
 *   2. 物品按格处理；真人组队的 roll/master 格保留，不能整具尸体跳过。
 *   3. 纯 NPCBot 队不视作存在真人 roll 竞争者。
 *   4. 周围金币只聚合到圆心 Loot，随后仍由 LootHandler 原有单人/真人/NPCBot
 *      分金链结算一次；本模块不直接 ModifyMoney。
 *   5. 邮件兜底只接受明确的 EQUIP_ERR_INV_FULL；唯一数量等其它错误原样保留。
 */

#ifndef _CUSTOM_AOE_LOOT_H
#define _CUSTOM_AOE_LOOT_H

#include "Define.h"

class Player;
class Creature;

namespace CustomAoELoot
{
    bool Enabled();
    bool EnabledInGroup();
    float Range();
    uint32 MaxCorpses();
    bool SkipRollItems();
    bool MailEnabled();
    bool Announce();

    /*
     * 在合法 SendLoot 成功后处理圆心以外的物品格。
     * gatheredGold 仅用于同一次触发的诊断播报；金币已在 SendLoot 前聚合到圆心。
     * 返回实际发生物品移除的额外尸体数。
     */
    uint32 LootAllAround(Player* player, Creature* origin,
        uint32 gatheredGold = 0, uint32 gatheredGoldCorpses = 0);

    /*
     * 在圆心 SendLoot 前，将周围合法尸体的金币搬到圆心 Loot。
     * 这里只移动 Loot::gold，不给玩家加钱、不做成就、不触发 OnLootMoney；
     * LootHandler 随后的原有分金链会对聚合总额统一结算一次。
     */
    uint32 GatherMoneyAround(Player* player, Creature* origin,
        uint32* gatheredCorpses = nullptr);
}

#endif // _CUSTOM_AOE_LOOT_H
