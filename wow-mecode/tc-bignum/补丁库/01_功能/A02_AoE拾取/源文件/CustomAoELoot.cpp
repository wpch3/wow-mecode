/*
 * CustomAoELoot.cpp - 群体拾取（AoE Loot）实现
 * 详见 CustomAoELoot.h 头部说明
 */

#include "CustomAoELoot.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Loot.h"
#include "LootMgr.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <map>
#include <tuple>
#include <vector>

namespace
{
    // 背包放不下时暂存，稍后合并邮寄
    // key: itemid -> randomPropertyId -> 总数量
    typedef std::map<uint32, std::map<int32, uint32>> OverflowMap;

    /*
     * 判断组里有没有「真人队友」（不含自己，不含 NPCBot）。
     *
     * 为什么需要：NPCBot 会被加进真实 Group（botmgr.cpp:997-1006 里
     * `gr = new Group; gr->Create(_owner); gr->AddMember(bot)`），
     * 所以只要召一个 bot，GetGroup() 就非空。
     * 若直接用 GetGroup() 判断，带 bot 刷材料时群体拾取会永远不生效
     * —— 而那恰恰是最需要它的场景。
     */
    bool HasRealGroupMates(Player* player)
    {
        Group* group = player->GetGroup();
        if (!group)
            return false;

        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member)
                continue;
            if (member == player)
                continue;
            // 真人队友（不管在不在线、在不在附近，只要在队里就算）
            return true;
        }

        return false;
    }

    /*
     * 组队且开启「保护 roll」时，这具尸体是否该整具跳过。
     *
     * 超阈值物品（紫装等）在 GROUP_LOOT / NEED_BEFORE_GREED 下要走
     * roll 窗口，不能被一键捞走，否则队友看不到 roll。
     */
    bool ShouldSkipForRoll(Player* player, Loot* loot)
    {
        if (!CustomAoELoot::SkipRollItems())
            return false;

        Group* group = player->GetGroup();
        if (!group)
            return false;

        switch (group->GetLootMethod())
        {
            case GROUP_LOOT:
            case NEED_BEFORE_GREED:
            case MASTER_LOOT:
                return loot->hasOverThresholdItem();
            default:
                return false;
        }
    }

    /*
     * 把溢出物品合并成邮件发给玩家。
     * 每封邮件最多 MAX_MAIL_ITEMS(12) 件，超出自动分多封。
     * 超过最大堆叠数的自动拆堆。
     */
    void MailOverflow(Player* player, OverflowMap const& overflow)
    {
        if (overflow.empty())
            return;

        // 先展开成「一件一堆」的列表
        std::vector<std::tuple<uint32, uint32, int32>> stacks;
        for (auto const& entryPair : overflow)
        {
            uint32 entry = entryPair.first;
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
            if (!proto)
                continue;

            uint32 maxStack = proto->GetMaxStackSize();
            if (maxStack == 0)
                maxStack = 1;

            for (auto const& propPair : entryPair.second)
            {
                int32 randomPropertyId = propPair.first;
                uint32 count = propPair.second;

                while (count > maxStack)
                {
                    stacks.emplace_back(entry, maxStack, randomPropertyId);
                    count -= maxStack;
                }
                if (count > 0)
                    stacks.emplace_back(entry, count, randomPropertyId);
            }
        }

        if (stacks.empty())
            return;

        // 按每封 MAX_MAIL_ITEMS 件分批
        size_t idx = 0;
        while (idx < stacks.size())
        {
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

            MailSender sender(MAIL_CREATURE, 34337 /* The Postmaster */);
            MailDraft draft("Recovered Item",
                "We recovered a lost item in the twisting nether and noted that it was yours.$B$BPlease find said object enclosed.");

            uint32 added = 0;
            while (idx < stacks.size() && added < MAX_MAIL_ITEMS)
            {
                uint32 entry;
                uint32 count;
                int32 randomPropertyId;
                std::tie(entry, count, randomPropertyId) = stacks[idx];

                if (Item* item = Item::CreateItem(entry, count, player))
                {
                    if (randomPropertyId)
                        item->SetItemRandomProperties(randomPropertyId);
                    item->SaveToDB(trans);
                    draft.AddItem(item);
                    ++added;
                }
                ++idx;
            }

            if (added > 0)
                draft.SendMailTo(trans, MailReceiver(player, player->GetGUID().GetCounter()), sender);

            CharacterDatabase.CommitTransaction(trans);
        }
    }
}

namespace CustomAoELoot
{
    bool Enabled()
    {
        return sConfigMgr->GetBoolDefault("AoELoot.Enable", true);
    }

    bool EnabledInGroup()
    {
        return sConfigMgr->GetBoolDefault("AoELoot.InGroup", true);
    }

    float Range()
    {
        float range = sConfigMgr->GetFloatDefault("AoELoot.Range", 60.0f);
        // 上限保护：搜索半径过大时网格遍历开销陡增
        return std::clamp(range, 5.0f, 200.0f);
    }

    uint32 MaxCorpses()
    {
        int32 v = sConfigMgr->GetIntDefault("AoELoot.MaxCorpses", 50);
        return uint32(std::clamp(v, 1, 500));
    }

    bool SkipRollItems()
    {
        return sConfigMgr->GetBoolDefault("AoELoot.SkipRollItems", true);
    }

    bool MailEnabled()
    {
        return sConfigMgr->GetBoolDefault("AoELoot.MailExcess", true);
    }

    bool Announce()
    {
        return sConfigMgr->GetBoolDefault("AoELoot.Announce", true);
    }

    uint32 LootAllAround(Player* player, Creature* origin)
    {
        if (!player || !origin)
            return 0;

        if (!Enabled())
            return 0;

        // 组队策略：按「真人队友」判断，纯 bot 队视同单人
        if (HasRealGroupMates(player) && !EnabledInGroup())
            return 0;

        // 只找死亡的生物。
        // CreatureWithOptionsInObjectRangeCheck（GridNotifiers.h:1404）已内建：
        //   - getDeathState() == DEAD 的（已消失的尸体）直接排除
        //   - IsAlive.has_value() 时按 IsAlive 过滤
        // 可拾取的尸体处于 CORPSE 状态，IsAlive()==false，正好命中。
        FindCreatureOptions options;
        options.IsAlive = false;

        std::vector<Creature*> corpses;
        player->GetCreatureListWithOptionsInGrid(corpses, Range(), options);

        if (corpses.empty())
            return 0;

        uint32 maxCorpses = MaxCorpses();
        uint32 processed = 0;
        uint32 itemsLooted = 0;
        OverflowMap overflow;

        for (Creature* c : corpses)
        {
            if (processed >= maxCorpses)
                break;

            if (!c)
                continue;

            // 圆心那具由原版流程处理，避免与客户端 lootSlot 索引冲突
            if (c == origin)
                continue;

            // NPCBot 及其宠物的尸体不参与
            if (c->IsNPCBotOrPet())
                continue;

            // 没有可拾取标记的跳过（没打过、已捡完、不该他捡）
            if (!c->HasDynamicFlag(UNIT_DYNFLAG_LOOTABLE))
                continue;

            /*
             * 越权校验 —— 这是与上游实现最关键的差异。
             * Player::isAllowedToLoot()（Player.cpp:17886）内部已处理：
             *   FREE_FOR_ALL      -> true（先到先得，暴雪原意）
             *   ROUND_ROBIN       -> 仅轮到自己 / 有专属物品
             *   MASTER_LOOT       -> true（下面还有 permission 二次卡）
             *   GROUP/NEED_GREED  -> 轮抽逻辑 + 超阈值判断
             *   LOOT_SKINNING     -> 仅 lootRecipient 本人
             * 上游版本完全没做这个校验，能捞走别人打的怪。
             */
            if (!player->isAllowedToLoot(c))
                continue;

            Loot* loot = &c->loot;

            // 剥皮战利品不参与群体拾取（需要玩家主动剥）
            if (loot->loot_type == LOOT_SKINNING)
                continue;

            // 组队时保护要 roll 的物品，留给正常 roll 窗口
            if (ShouldSkipForRoll(player, loot))
                continue;

            ++processed;

            /*
             * 逐格拾取。
             * 全程调用原版 Player::StoreLootItem()，由它负责：
             *   - is_looted / unlootedCount 的维护（上游漏了 -> 可无限刷装备）
             *   - qitem / ffaitem / conditem 的分支处理
             *   - 成就统计 UpdateAchievementCriteria
             *   - Eluna 的 OnLootItem 钩子
             *   - LootItemStorage 容器持久化
             * 背包满时 StoreLootItem 会发 EQUIP_ERR_INVENTORY_FULL 并保留物品，
             * 我们据此把剩余物品收进 overflow 走邮件。
             *
             * 倒序遍历：StoreLootItem 成功后不会移动 items 下标
             * （只置 is_looted 标记），但倒序更稳妥。
             */
            uint32 maxSlot = loot->GetMaxSlotInLootFor(player);
            for (uint32 slot = 0; slot < maxSlot; ++slot)
            {
                LootItem* item = loot->LootItemInSlot(slot, player, nullptr, nullptr, nullptr);
                if (!item || item->is_looted)
                    continue;

                if (!item->AllowedForPlayer(player))
                    continue;

                // 被 roll 锁定 / 别人赢走的不碰
                if (item->is_blocked)
                    continue;
                if (!item->rollWinnerGUID.IsEmpty() && item->rollWinnerGUID != player->GetGUID())
                    continue;

                // 先试能不能进背包
                ItemPosCountVec dest;
                InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item->itemid, item->count);
                if (msg == EQUIP_ERR_OK)
                {
                    player->StoreLootItem(uint8(slot), loot);
                    ++itemsLooted;
                }
                else if (MailEnabled())
                {
                    // 背包满 -> 记下来走邮件，并手工结算这一格
                    overflow[item->itemid][item->randomPropertyId] += item->count;

                    item->is_looted = true;
                    loot->NotifyItemRemoved(uint8(slot));
                    if (loot->unlootedCount > 0)
                        --loot->unlootedCount;

                    ++itemsLooted;
                }
                // 背包满且没开邮寄 -> 原样留在尸体上，玩家自己处理
            }

            // 金币一并收走
            if (loot->gold)
            {
                player->ModifyMoney(loot->gold);
                player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, loot->gold);
                loot->gold = 0;
                loot->NotifyMoneyRemoved();
            }

            /*
             * 收尾：清干净的尸体摘掉可拾取标记。
             * 顺序很重要 —— 必须先判断 isLooted() 再 clear()。
             * 上游版本先 clear() 再判断，clear() 会把 unlootedCount 归零，
             * 导致 isLooted() 恒为真，逻辑等于写死。
             */
            if (loot->isLooted())
            {
                c->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
                // 用当前尸体 c 判断，不是外层的 origin
                // （上游这里误用了外层变量，导致剥皮可无限刷）
                if (!c->IsAlive())
                    c->AllLootRemovedFromCorpse();
                loot->clear();
            }
            else
            {
                // 还有剩余（比如保护下来的 roll 物品），刷新一下拾取者显示
                c->ForceValuesUpdateAtIndex(UNIT_DYNAMIC_FLAGS);
            }
        }

        // 邮寄溢出物品
        if (!overflow.empty())
        {
            MailOverflow(player, overflow);
            if (player->GetSession())
                player->GetSession()->SendAreaTriggerMessage("背包已满，多余物品已寄到你的邮箱。");
        }

        if (Announce() && itemsLooted > 0 && processed > 0)
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "群体拾取：额外收取 %u 具尸体，共 %u 件物品。", processed, itemsLooted);
        }

        return processed;
    }

    uint32 GatherMoneyAround(Player* player, Creature* origin)
    {
        if (!player || !origin)
            return 0;

        if (!Enabled())
            return 0;

        if (HasRealGroupMates(player) && !EnabledInGroup())
            return 0;

        FindCreatureOptions options;
        options.IsAlive = false;

        std::vector<Creature*> corpses;
        player->GetCreatureListWithOptionsInGrid(corpses, Range(), options);

        if (corpses.empty())
            return 0;

        uint32 maxCorpses = MaxCorpses();
        uint32 processed = 0;
        uint32 extraGold = 0;

        for (Creature* c : corpses)
        {
            if (processed >= maxCorpses)
                break;

            if (!c || c == origin)
                continue;

            if (c->IsNPCBotOrPet())
                continue;

            if (!c->HasDynamicFlag(UNIT_DYNFLAG_LOOTABLE))
                continue;

            if (!player->isAllowedToLoot(c))
                continue;

            Loot* loot = &c->loot;
            if (!loot->gold)
                continue;

            if (loot->loot_type == LOOT_SKINNING)
                continue;

            extraGold += loot->gold;
            loot->gold = 0;
            loot->NotifyMoneyRemoved();
            ++processed;

            // 金币收完后如果整具尸体也空了，摘标记
            if (loot->isLooted())
            {
                c->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
                if (!c->IsAlive())
                    c->AllLootRemovedFromCorpse();
                loot->clear();
            }
        }

        return extraGold;
    }
}
