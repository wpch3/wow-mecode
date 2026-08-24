/*
 * CustomAoELoot.cpp - F45 群体拾取可靠性修复
 */

#include "CustomAoELoot.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "Item.h"
#include "Log.h"
#include "Loot.h"
#include "LootItemStorage.h"
#include "LootMgr.h"
#include "Mail.h"
#include "Player.h"
#include "WorldSession.h"
#ifdef ELUNA
#include "LuaEngine.h"
#endif
#include <algorithm>
#include <limits>
#include <vector>

namespace
{
    struct AoEStats
    {
        uint32 discovered = 0;
        uint32 eligible = 0;
        uint32 corpsesChanged = 0;
        uint32 itemsStored = 0;
        uint32 itemsMailed = 0;
        uint32 noDynamicFlag = 0;
        uint32 permissionDenied = 0;
        uint32 skinning = 0;
        uint32 rollSlots = 0;
        uint32 inventoryErrors = 0;
        uint32 limitHit = 0;
    };

    struct MailEntry
    {
        Item* item;
        uint32 count;
        ObjectGuid sourceGuid;
        LootType lootType;
    };

    typedef std::vector<MailEntry> MailQueue;

    // NPCBot 使用独立的 GroupBotReference 链，不计入 GetMembersCount()。
    // 因此 GetMembersCount()>1 精确表示组内还有真人槽位（含暂时离线真人）。
    bool HasRealGroupMates(Player const* player)
    {
        Group const* group = player ? player->GetGroup() : nullptr;
        return group && group->GetMembersCount() > 1;
    }

    bool GroupAoELootEnabled(Player const* player)
    {
        return !HasRealGroupMates(player) || CustomAoELoot::EnabledInGroup();
    }

    // 与 Player::SendLoot 的尸体归属边界保持一致，但不套用“整具尸体是否有
    // 当前玩家可见普通格”的判断；金币会在同组原分金链中再次合法分配。
    bool HasCorpseRights(Player* player, Creature* creature)
    {
        if (!player || !creature || !creature->isDead())
            return false;

        if (!creature->IsDamageEnoughForLootingAndReward() || player->HasPendingBind())
            return false;

        if (Group* group = player->GetGroup())
            return creature->GetLootRecipientGroup() == group;

        return creature->GetLootRecipient() == player;
    }

    bool IsProtectedLootMethod(Group const* group)
    {
        if (!group)
            return false;

        switch (group->GetLootMethod())
        {
            case GROUP_LOOT:
            case NEED_BEFORE_GREED:
            case MASTER_LOOT:
                return true;
            default:
                return false;
        }
    }

    bool IsRollProtected(Player* player, LootItem const* item, NotNormalLootItem const* qitem)
    {
        if (!item)
            return true;

        // 已经存在的原版 roll/master 状态永远优先；赢家本人可在解锁后拾取。
        if (!item->rollWinnerGUID.IsEmpty())
            return item->rollWinnerGUID != player->GetGUID() || item->is_blocked;

        // 普通格的 blocked 就是进行中的 roll/master 保护。任务格的 blocked
        // 另有每玩家可见性用途，仅在 follow_loot_rules 时作为组队保护。
        if ((!qitem || item->follow_loot_rules) && item->is_blocked)
            return true;

        if (!CustomAoELoot::SkipRollItems() || !HasRealGroupMates(player))
            return false;

        Group const* group = player->GetGroup();
        if (!IsProtectedLootMethod(group) || item->freeforall)
            return false;

        if (qitem)
            return item->follow_loot_rules;

        // FillLoot 已按组阈值设置 is_underthreshold。只保留需要原版
        // roll/master 的格，低于阈值的普通格继续处理。
        return !item->is_underthreshold;
    }

    bool HasRoundRobinSlotRight(Player* player, Loot* loot, LootItem const* item,
        NotNormalLootItem const* qitem, NotNormalLootItem const* ffaitem,
        NotNormalLootItem const* conditem)
    {
        if (!player || !loot || !item)
            return false;

        // 这些格已经由 LootItemInSlot 的玩家专属列表确定可见性。
        if (qitem || ffaitem || conditem || item->freeforall)
            return true;

        Group const* group = player->GetGroup();
        if (!group || !HasRealGroupMates(player))
            return true;

        bool roundRobinApplies = false;
        switch (group->GetLootMethod())
        {
            case ROUND_ROBIN:
                roundRobinApplies = true;
                break;
            case GROUP_LOOT:
            case NEED_BEFORE_GREED:
            case MASTER_LOOT:
                roundRobinApplies = item->is_underthreshold;
                break;
            default:
                break;
        }

        return !roundRobinApplies || loot->roundRobinPlayer.IsEmpty() ||
            loot->roundRobinPlayer == player->GetGUID();
    }

    void MarkLootItemRemoved(Player* player, Loot* loot, uint8 slot, LootItem* item,
        NotNormalLootItem* qitem, NotNormalLootItem* ffaitem, NotNormalLootItem* conditem)
    {
        if (qitem)
        {
            qitem->is_looted = true;
            // 邻近尸体不是当前客户端拾取窗，不能发送直接 slot 删除包。
            // 非 FFA 且多人各自持有任务列表时，仍通知其它真实查看者。
            if (!item->freeforall && loot->GetPlayerQuestItems().size() != 1)
                loot->NotifyQuestItemRemoved(qitem->index);
        }
        else if (ffaitem)
            ffaitem->is_looted = true;
        else
        {
            if (conditem)
                conditem->is_looted = true;
            loot->NotifyItemRemoved(slot);
        }

        if (!item->freeforall)
            item->is_looted = true;

        if (loot->unlootedCount > 0)
            --loot->unlootedCount;
        else
            TC_LOG_ERROR("loot", "F45 AoE Loot: unlootedCount already zero for item {}", item->itemid);

        if (loot->containerID > 0)
            sLootItemStorage->RemoveStoredLootItemForContainer(
                loot->containerID, item->itemid, item->count, item->itemIndex);
    }

    void CreditLootEvents(Player* player, Item* newItem, uint32 itemId, uint32 count,
        LootType lootType, ObjectGuid sourceGuid)
    {
        player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM, itemId, count);
        player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE, lootType, count);
        player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_EPIC_ITEM, itemId, count);

#ifdef ELUNA
        if (Eluna* e = player->GetEluna())
            e->OnLootItem(player, newItem, count, sourceGuid);
#endif
    }

    bool StoreAoEItem(Player* player, Creature* source, Loot* loot, uint8 slot,
        LootItem* item, NotNormalLootItem* qitem, NotNormalLootItem* ffaitem,
        NotNormalLootItem* conditem, ItemPosCountVec const& dest)
    {
        Item* newItem = player->StoreNewItem(dest, item->itemid, true,
            item->randomPropertyId, item->GetAllowedLooters());
        if (!newItem)
            return false;

        uint32 itemId = item->itemid;
        uint32 count = item->count;
        MarkLootItemRemoved(player, loot, slot, item, qitem, ffaitem, conditem);

        player->SendNewItem(newItem, count, false, false, true);
        CreditLootEvents(player, newItem, itemId, count, LOOT_CORPSE, source->GetGUID());
        return true;
    }

    bool QueueMailedItem(Player* player, Creature* source, Loot* loot, uint8 slot,
        LootItem* item, NotNormalLootItem* qitem, NotNormalLootItem* ffaitem,
        NotNormalLootItem* conditem, MailQueue& queue)
    {
        Item* mailItem = Item::CreateItem(item->itemid, item->count, player);
        if (!mailItem)
            return false;

        if (item->randomPropertyId)
            mailItem->SetItemRandomProperties(item->randomPropertyId);

        MailEntry entry = { mailItem, item->count, source->GetGUID(), LOOT_CORPSE };
        queue.push_back(entry);
        MarkLootItemRemoved(player, loot, slot, item, qitem, ffaitem, conditem);
        return true;
    }

    uint32 SendOverflowMail(Player* player, MailQueue& queue)
    {
        if (!player || queue.empty())
            return 0;

        uint32 sent = 0;
        size_t index = 0;
        while (index < queue.size())
        {
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            MailSender sender(MAIL_CREATURE, 34337 /* The Postmaster */);
            MailDraft draft("Recovered Item",
                "We recovered a lost item in the twisting nether and noted that it was yours.$B$BPlease find said object enclosed.");

            uint32 attached = 0;
            std::vector<MailEntry*> credited;
            while (index < queue.size() && attached < MAX_MAIL_ITEMS)
            {
                MailEntry& entry = queue[index++];
                entry.item->SaveToDB(trans);
                draft.AddItem(entry.item);
                credited.push_back(&entry);
                ++attached;
            }

            if (attached > 0)
            {
                draft.SendMailTo(trans,
                    MailReceiver(player, player->GetGUID().GetCounter()), sender);
                CharacterDatabase.CommitTransaction(trans);

                for (MailEntry* entry : credited)
                {
                    CreditLootEvents(player, entry->item, entry->item->GetEntry(),
                        entry->count, entry->lootType, entry->sourceGuid);
                    ++sent;
                }
            }
        }

        return sent;
    }

    void FinishCorpse(Creature* creature, Loot* loot)
    {
        if (loot->isLooted())
        {
            creature->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
            if (!creature->IsAlive())
                creature->AllLootRemovedFromCorpse();
            loot->clear();
        }
        else
            creature->ForceValuesUpdateAtIndex(UNIT_DYNAMIC_FLAGS);
    }

    void SortNearestFirst(Player* player, std::vector<Creature*>& corpses)
    {
        std::sort(corpses.begin(), corpses.end(), [player](Creature const* left, Creature const* right)
        {
            if (!left)
                return false;
            if (!right)
                return true;
            return player->GetDistance(left) < player->GetDistance(right);
        });
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
        return std::clamp(sConfigMgr->GetFloatDefault("AoELoot.Range", 60.0f), 5.0f, 200.0f);
    }

    uint32 MaxCorpses()
    {
        int32 value = sConfigMgr->GetIntDefault("AoELoot.MaxCorpses", 50);
        return uint32(std::clamp(value, 1, 500));
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

    uint32 LootAllAround(Player* player, Creature* origin,
        uint32 gatheredGold, uint32 gatheredGoldCorpses)
    {
        if (!player || !origin || !Enabled() || !GroupAoELootEnabled(player))
            return 0;

        // 此入口位于 SendLoot 成功之后；再次锁住尸体归属可防未来误调用。
        if (!origin->HasDynamicFlag(UNIT_DYNFLAG_LOOTABLE) ||
            !HasCorpseRights(player, origin) || origin->loot.loot_type == LOOT_SKINNING)
            return 0;

        FindCreatureOptions options;
        options.IsAlive = false;

        std::vector<Creature*> corpses;
        player->GetCreatureListWithOptionsInGrid(corpses, Range(), options);
        SortNearestFirst(player, corpses);

        AoEStats stats;
        MailQueue mailQueue;
        uint32 processed = 0;
        uint32 maxCorpses = MaxCorpses();

        for (Creature* creature : corpses)
        {
            if (!creature || creature == origin || creature->IsNPCBotOrPet())
                continue;

            ++stats.discovered;

            if (!creature->HasDynamicFlag(UNIT_DYNFLAG_LOOTABLE))
            {
                ++stats.noDynamicFlag;
                continue;
            }

            if (!HasCorpseRights(player, creature) || !player->isAllowedToLoot(creature))
            {
                ++stats.permissionDenied;
                continue;
            }

            Loot* loot = &creature->loot;
            if (loot->loot_type == LOOT_SKINNING)
            {
                ++stats.skinning;
                continue;
            }

            if (processed >= maxCorpses)
            {
                stats.limitHit = 1;
                break;
            }

            ++processed;
            ++stats.eligible;
            bool corpseChanged = false;

            uint32 maxSlot = loot->GetMaxSlotInLootFor(player);
            for (uint32 slot = 0; slot < maxSlot; ++slot)
            {
                NotNormalLootItem* qitem = nullptr;
                NotNormalLootItem* ffaitem = nullptr;
                NotNormalLootItem* conditem = nullptr;
                LootItem* item = loot->LootItemInSlot(
                    slot, player, &qitem, &ffaitem, &conditem);

                if (!item || item->is_looted || !item->AllowedForPlayer(player))
                    continue;

                if (IsRollProtected(player, item, qitem))
                {
                    ++stats.rollSlots;
                    continue;
                }

                if (!HasRoundRobinSlotRight(
                    player, loot, item, qitem, ffaitem, conditem))
                {
                    ++stats.permissionDenied;
                    continue;
                }

                ItemPosCountVec dest;
                InventoryResult result = player->CanStoreNewItem(
                    NULL_BAG, NULL_SLOT, dest, item->itemid, item->count);

                if (result == EQUIP_ERR_OK)
                {
                    if (StoreAoEItem(player, creature, loot, uint8(slot), item,
                        qitem, ffaitem, conditem, dest))
                    {
                        ++stats.itemsStored;
                        corpseChanged = true;
                    }
                    else
                        ++stats.inventoryErrors;
                }
                else if (result == EQUIP_ERR_INV_FULL && MailEnabled())
                {
                    // 仅明确“总库存空间不足”允许邮件兜底。唯一数量、携带上限、
                    // 物品条件等其它结果全部保留在尸体，不得借邮件绕过。
                    if (QueueMailedItem(player, creature, loot, uint8(slot), item,
                        qitem, ffaitem, conditem, mailQueue))
                    {
                        ++stats.itemsMailed;
                        corpseChanged = true;
                    }
                    else
                        ++stats.inventoryErrors;
                }
                else
                    ++stats.inventoryErrors;
            }

            if (corpseChanged)
            {
                ++stats.corpsesChanged;
                FinishCorpse(creature, loot);
            }
        }

        uint32 mailed = SendOverflowMail(player, mailQueue);
        if (mailed > 0 && player->GetSession())
            player->GetSession()->SendAreaTriggerMessage(
                "背包空间不足，符合容量兜底条件的物品已寄到邮箱。");

        if (Announce() && player->GetSession() &&
            (stats.discovered > 0 || gatheredGold > 0))
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "群体拾取F45：发现%u 合法%u 完成尸体%u，物品%u 邮寄%u，金币%u具/%u铜；保留[标志%u 权限%u 剥皮%u roll格%u 背包错误%u 上限%u]。",
                stats.discovered, stats.eligible, stats.corpsesChanged,
                stats.itemsStored, mailed, gatheredGoldCorpses, gatheredGold,
                stats.noDynamicFlag, stats.permissionDenied, stats.skinning,
                stats.rollSlots, stats.inventoryErrors, stats.limitHit);
        }

        return stats.corpsesChanged;
    }

    uint32 GatherMoneyAround(Player* player, Creature* origin, uint32* gatheredCorpses)
    {
        if (gatheredCorpses)
            *gatheredCorpses = 0;

        if (!player || !origin || !Enabled() || !GroupAoELootEnabled(player))
            return 0;

        if (!origin->HasDynamicFlag(UNIT_DYNFLAG_LOOTABLE) ||
            !HasCorpseRights(player, origin) || origin->loot.loot_type == LOOT_SKINNING)
            return 0;

        FindCreatureOptions options;
        options.IsAlive = false;

        std::vector<Creature*> corpses;
        player->GetCreatureListWithOptionsInGrid(corpses, Range(), options);
        SortNearestFirst(player, corpses);

        uint32 maxCorpses = MaxCorpses();
        uint32 processed = 0;
        uint32 extraGold = 0;
        uint32 room = std::numeric_limits<uint32>::max() - origin->loot.gold;

        for (Creature* creature : corpses)
        {
            if (!creature || creature == origin || creature->IsNPCBotOrPet())
                continue;

            if (!creature->HasDynamicFlag(UNIT_DYNFLAG_LOOTABLE) ||
                !HasCorpseRights(player, creature))
                continue;

            Loot* loot = &creature->loot;
            if (!loot->gold || loot->loot_type == LOOT_SKINNING)
                continue;

            if (processed >= maxCorpses)
                break;

            // 不允许 uint32 聚合溢出；放不下的金币保持原尸体不动。
            if (loot->gold > room - extraGold)
                continue;

            extraGold += loot->gold;
            loot->gold = 0;
            loot->NotifyMoneyRemoved();
            ++processed;

            if (loot->isLooted())
                FinishCorpse(creature, loot);
        }

        if (gatheredCorpses)
            *gatheredCorpses = processed;
        return extraGold;
    }
}
