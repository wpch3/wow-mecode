/*
 * ============================================================================
 *  step34  伙伴关怀系统 —— 数据层
 * ============================================================================
 *
 *  用户需求原话（2026-08-02）：
 *    「是给你分享自己的见闻趣事、所见所得，就像是真的在陪你冒险一样，
 *      会在你饿了给你面包，渴了给你水，穷了给你钱，以此类推，也不止于此。
 *      就像是一个伙伴，一个值得信赖的伙伴，
 *      是会自己拿出背包里的东西给予伙伴的一个战友！」
 *
 *  设计要点：
 *    - 台词、物品【全部来自数据库】，不写死在代码里（改一句不用重编译）
 *    - 物品要有【真实来源】，从 bot 自己的虚拟背包里扣，不凭空生成
 *    - 启动时校验 item_template，不存在的物品直接跳过（铁律：不硬编码内容ID）
 *
 *  依赖表（world 库）：
 *    npcbot_care_text     台词池
 *    npcbot_care_item     可给予物品池
 *  依赖表（characters 库）：
 *    npcbot_inventory     bot 虚拟背包
 * ============================================================================
 */

#ifndef BOT_COMPANION_H_
#define BOT_COMPANION_H_

#include "Define.h"
#include <string>
#include <unordered_map>
#include <vector>

// 关怀类型。数值和 npcbot_care_text.care_type 对应，改了要同步改 SQL。
enum CompanionCareType : uint8
{
    CARE_TYPE_NONE      = 0,
    CARE_TYPE_FOOD      = 1,   // 你饿了 -> 给面包
    CARE_TYPE_DRINK     = 2,   // 你渴了 -> 给水
    CARE_TYPE_MONEY     = 3,   // 你穷了 -> 给钱
    CARE_TYPE_CHAT      = 4,   // 没事 -> 聊两句见闻
    CARE_TYPE_LEVELUP   = 5,   // 你升级了 -> 祝贺
    CARE_TYPE_REVIVE    = 6,   // 你复活了 -> 关心

    CARE_TYPE_MAX       = 7
};

// 一条台词
struct CompanionText
{
    uint32      Id;
    uint8       CareType;
    uint8       BotClass;      // 0 = 通用
    std::string Text;          // 支持 {item} {gold} 占位符
    uint32      Emote;         // 0 = 不播表情
    uint8       Weight;        // 随机权重
};

// 一条可给予物品
struct CompanionItem
{
    uint32      Id;
    uint8       CareType;
    uint32      ItemId;
    uint8       MinLevel;
    uint8       MaxLevel;
    std::string SourceText;    // 来源描述，用于台词
};

// bot 背包里的一格
struct CompanionInvEntry
{
    uint32      ItemId;
    uint32      Count;
    std::string AcquiredFrom;
};

class TC_GAME_API BotCompanionMgr
{
public:
    static BotCompanionMgr* instance();

    // 启动时加载。会校验 item_template，无效物品跳过并打日志。
    void LoadCareTexts();
    void LoadCareItems();
    void LoadInventories();

    // 全部重载（给 .reload 指令用）
    void ReloadAll();

    // 按类型和职业随机取一条台词。取不到返回空串。
    std::string PickText(uint8 careType, uint8 botClass) const;

    // 按类型和等级挑一个合适的物品。挑不到返回 nullptr。
    CompanionItem const* PickItem(uint8 careType, uint8 level) const;

    // ---- 虚拟背包 ----
    void AddToInventory(uint32 botGuid, uint32 itemId, uint32 count, std::string const& from);
    bool TakeFromInventory(uint32 botGuid, uint32 itemId, uint32 count);
    bool HasInInventory(uint32 botGuid, uint32 itemId, uint32 count = 1) const;
    // 从背包里找一个符合 careType + 等级 的物品，返回 itemId，找不到返回 0
    uint32 FindInInventory(uint32 botGuid, uint8 careType, uint8 level, std::string& outFrom) const;
    std::vector<CompanionInvEntry> const* GetInventory(uint32 botGuid) const;

    // 给 bot 补充初始物资（招募时 / 定期）
    void RestockBot(uint32 botGuid, uint8 level);

private:
    BotCompanionMgr() { }
    ~BotCompanionMgr() { }
    BotCompanionMgr(BotCompanionMgr const&) = delete;
    BotCompanionMgr& operator=(BotCompanionMgr const&) = delete;

    // careType -> 台词列表
    std::unordered_map<uint8, std::vector<CompanionText>> _texts;
    // careType -> 物品列表
    std::unordered_map<uint8, std::vector<CompanionItem>> _items;
    // botGuid -> 背包
    std::unordered_map<uint32, std::vector<CompanionInvEntry>> _inventories;
};

#define sBotCompanionMgr BotCompanionMgr::instance()

#endif // BOT_COMPANION_H_
