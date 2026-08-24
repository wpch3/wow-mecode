/*
 * CustomAoELoot.h - 群体拾取（AoE Loot）
 *
 * 本服自定义模块，不是上游 TrinityCoreCustomChanges 的代码。
 * 上游 3.3.5-aoe-loot-mail-excess 分支的实现有 4 个已知缺陷，
 * 且要改 8 个文件（含 NPCBot 重灾区 Player.cpp / Group.cpp），
 * 因此重写为「只改 LootHandler.cpp 一处」的版本。
 *
 * 设计要点：
 *   1. 复用源码已有的 GetCreatureListWithOptionsInGrid + FindCreatureOptions
 *      （WorldObject.h:549 / :315），不新增网格搜索器
 *   2. 全程走 Player::StoreLootItem()，拾取状态/成就/Eluna钩子由原版维护，
 *      不手工改 is_looted / unlootedCount，杜绝刷装备
 *   3. 用 Player::isAllowedToLoot()（Player.cpp:17886）做越权校验，
 *      五种分配方式全部兼容，组队时不会抢队友的东西
 *   4. NPCBot 感知：bot 会被加进真实 Group（botmgr.cpp:997-1006），
 *      因此「是否组队」按组内真人数判断，纯 bot 队视同单人
 */

#ifndef _CUSTOM_AOE_LOOT_H
#define _CUSTOM_AOE_LOOT_H

#include "Define.h"

class Player;
class Creature;

namespace CustomAoELoot
{
    // 总开关
    bool Enabled();

    // 组队时是否启用
    bool EnabledInGroup();

    // 拾取半径（码）
    float Range();

    // 单次最多处理多少具尸体（防卡）
    uint32 MaxCorpses();

    // 组队时是否跳过需要 roll 的超阈值物品
    bool SkipRollItems();

    // 是否把背包装不下的物品邮寄给自己
    bool MailEnabled();

    // 是否播报本次拾取汇总
    bool Announce();

    /*
     * 主入口：拾取 origin 周围所有合法尸体。
     *
     * @param player  拾取者
     * @param origin  本次交互的尸体（玩家点开的那具），用作搜索圆心
     * @return        额外处理的尸体数量（不含 origin 本身）
     *
     * 注意：origin 自身不在此处理，由原版 StoreLootItem 流程处理，
     *       避免与客户端的 lootSlot 索引冲突。
     */
    uint32 LootAllAround(Player* player, Creature* origin);

    // 金币专用：汇总周围尸体的金币到 origin 的 loot 里
    // @return 汇总到的额外金币（不含 origin 自身的）
    uint32 GatherMoneyAround(Player* player, Creature* origin);
}

#endif // _CUSTOM_AOE_LOOT_H
