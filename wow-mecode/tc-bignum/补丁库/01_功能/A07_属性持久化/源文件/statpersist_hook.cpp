/*
 * statpersist_hook.cpp - GM属性持久化的钩子
 *
 * 单独一个文件，因为 PlayerScript 需要独立注册。
 * 放置：D:\TrinityCore\src\server\scripts\Custom\statpersist_hook.cpp
 *       （如果没有 Custom 目录，放 src\server\scripts\World\ 也行）
 */

#include "ScriptMgr.h"
#include "CustomStatPersist.h"
#include "Player.h"

class statpersist_playerscript : public PlayerScript
{
public:
    statpersist_playerscript() : PlayerScript("statpersist_playerscript") { }

    /*
     * 登录时重新应用 GM 加的属性。
     *
     * 为什么放 OnLogin 而不是像幻化那样插进底层函数：
     * 属性是在登录流程【之后】才最终结算的，
     * 这里 apply 不会被后续的 UpdateAllStats 冲掉。
     * （幻化必须插底层是因为 SetVisibleItemSlot 在 OnLogin 之前就跑完了）
     */
    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        sCustomStatPersist->OnPlayerLogin(player);
    }

    // 删角色时清数据，防止 GUID 被复用后新角色继承旧属性
    void OnDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        sCustomStatPersist->OnCharacterDeleted(guid.GetCounter());
    }
};

void AddSC_statpersist_hook()
{
    new statpersist_playerscript();
}
