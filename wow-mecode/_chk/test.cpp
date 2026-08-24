#include "stub.h"
DBCStorage<ItemSetEntry> sItemSetStore;
DBCStorage<DungeonEncounterEntry> sDungeonEncounterStore;
DBCStorage<FactionEntry> sFactionStore;
DB CharacterDatabase, WorldDatabase;
ObjectMgr* sObjectMgr = nullptr;

#include "gearset_ns.inc"

int main() {
    // 触发模板实例化，验证逻辑可编译
    (void)GearSet::GetClassName(1);
    (void)GearSet::ParseClassName("战士");
    (void)GearSet::GetRoleName(GearSet::ROLE_TANK);
    (void)GearSet::ParseRole("tank");
    (void)GearSet::GetArmorSubClassForClass(CLASS_MAGE);
    (void)GearSet::ScoreForRole(nullptr, GearSet::ROLE_DPS);
    (void)GearSet::GetSlotNeeds();
    (void)GearSet::GetWeaponNeeds(CLASS_ROGUE, GearSet::ROLE_DPS);
    (void)GearSet::PickGemForSocket(SOCKET_COLOR_RED, GearSet::ROLE_HEAL);
    (void)GearSet::DungeonKey(631, 3);
    std::unordered_set<uint32> ex;
    (void)GearSet::PickBestItem(1, GearSet::ROLE_TANK, 264, INVTYPE_HEAD, 4, ex);
    (void)GearSet::PickBestWeapon(4, GearSet::ROLE_DPS, 264, INVTYPE_WEAPONMAINHAND, 15, ex);
    (void)GearSet::FindTierSets(1, 200, 300, 0);
    (void)GearSet::GetKillCount(1, 631, 3);
    (void)GearSet::IsSetUnlocked(1, 900);
    (void)GearSet::IsFinalBoss(631, 36597);
    printf("GearSet 命名空间编译并链接成功\n");
    return 0;
}
