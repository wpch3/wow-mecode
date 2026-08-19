/*
 * ============================================================================
 *  GM 指令助手 —— cs_gmhelper.cpp
 * ============================================================================
 *
 *  解决的问题：
 *    想改个体型，得先去网上查是哪个指令，查到 .modify scale，再手输。
 *    做成纯 Gossip 菜单又受 32 条上限，层级深了更烦。
 *
 *  方案：搜索 + 菜单 + 可点击链接，三层并用。
 *
 *  ── 指令 ──────────────────────────────────────────────────────────────
 *   .gm find <关键词>    中文/英文/别名搜索指令   ← 核心功能
 *   .gm menu             分类菜单（可点击，自动分页）
 *   .gm list [分类]      列出某分类的全部指令
 *   .gm cat              显示所有分类
 *
 *  ── 设计要点 ──────────────────────────────────────────────────────────
 *   1. 指令库是静态表，加指令只需在 BuildLibrary() 里加一行
 *   2. 搜索匹配：指令名 / 中文名 / 别名 / 说明，任一命中即可
 *   3. Gossip 严守 32 条硬上限（GossipDef.cpp:42 有 ASSERT），
 *      用 29 内容 + 3 导航分页
 *   4. 可点击链接只用客户端原生支持的类型，不需要插件
 *
 *  放置：D:\TrinityCore\src\server\scripts\Commands\cs_gmhelper.cpp
 *  需要：注册 cs_script_loader.cpp + RBAC.h 加权限 71008
 *  新增源文件必须重跑 CMake
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "Player.h"
#include "RBAC.h"
#include "GossipDef.h"
#include "WorldSession.h"
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

namespace GmHelper
{
    // Gossip 硬上限：GossipDef.cpp:42 有 ASSERT(_menuItems.size() <= 32)
    static constexpr uint32 GOSSIP_HARD_LIMIT = 32;
    static constexpr uint32 NAV_SLOTS         = 3;
    static constexpr uint32 PER_PAGE          = GOSSIP_HARD_LIMIT - NAV_SLOTS;  // 29
    static constexpr uint32 MENU_ID           = 60300;   // 避开 gearset 的 60100

    /*
     * sender 段必须避开 cs_gearset.cpp:1943 ——
     * 套装系统的 OnGossipSelect 只按 sender 过滤（1~11）且【不看 menuId】，
     * 用 1/2/3/9 会被它先截走，点分类会跳到套装菜单。
     * 已占用：套装 1-11、传送 9101/9102/9109、幻化预留 9201+
     */
    static constexpr uint32 SENDER_CAT   = 9301;  // 选分类
    static constexpr uint32 SENDER_CMD   = 9302;  // 选具体指令
    static constexpr uint32 SENDER_QUICK = 9303;  // 快捷值
    static constexpr uint32 SENDER_NAV   = 9309;

    static constexpr uint32 NAV_PREV   = 1;
    static constexpr uint32 NAV_NEXT   = 2;
    static constexpr uint32 NAV_BACK   = 3;
    static constexpr uint32 NAV_CLOSE  = 4;

    // ------------------------------------------------------------------
    //  指令条目
    // ------------------------------------------------------------------
    struct CmdEntry
    {
        char const* cmd;        // 实际指令
        char const* cn;         // 中文名
        char const* alias;      // 别名（空格分隔，用于搜索）
        char const* usage;      // 用法示例
        char const* note;       // 说明
        uint8       cat;        // 分类
    };

    enum Category : uint8
    {
        CAT_CHAR    = 0,   // 角色调整
        CAT_TELE    = 1,   // 传送移动
        CAT_ITEM    = 2,   // 物品装备
        CAT_NPC     = 3,   // NPC与生物
        CAT_CHEAT   = 4,   // 作弊开关
        CAT_INST    = 5,   // 副本相关
        CAT_CUSTOM  = 6,   // 本服自定义
        CAT_MISC    = 7,   // 其他
        CAT_MAX     = 8
    };

    inline char const* CatName(uint8 c)
    {
        switch (c)
        {
            case CAT_CHAR:   return "角色调整";
            case CAT_TELE:   return "传送移动";
            case CAT_ITEM:   return "物品装备";
            case CAT_NPC:    return "NPC与生物";
            case CAT_CHEAT:  return "作弊开关";
            case CAT_INST:   return "副本相关";
            case CAT_CUSTOM: return "本服自定义";
            default:         return "其他";
        }
    }

    // ------------------------------------------------------------------
    //  指令库
    //  加新指令只需在这里加一行
    // ------------------------------------------------------------------
    inline std::vector<CmdEntry> const& Library()
    {
        static std::vector<CmdEntry> lib =
        {
        // ---------------- 角色调整 ----------------
        { ".modify scale",     "体型大小", "体型 大小 变大 变小 缩放 scale size",
          ".modify scale 2",   "1=正常 0.5=一半 2=两倍 10=巨大", CAT_CHAR },
        { ".modify speed",     "移动速度", "速度 跑速 移动 快 speed run",
          ".modify speed 5",   "1=正常，最大约50", CAT_CHAR },
        { ".modify swim",      "游泳速度", "游泳 水下 swim",
          ".modify swim 5",    "", CAT_CHAR },
        { ".modify fly",       "飞行速度", "飞行 飞 fly",
          ".modify fly 5",     "", CAT_CHAR },
        { ".modify hp",        "生命值",   "血量 生命 血 hp health",
          ".modify hp 100000", "", CAT_CHAR },
        { ".modify mana",      "法力值",   "蓝量 法力 mana mp",
          ".modify mana 50000","", CAT_CHAR },
        { ".modify money",     "金钱",     "金币 钱 金 money gold",
          ".modify money 10000000", "单位是铜，10000=1金", CAT_CHAR },
        { ".character level",  "设置等级", "等级 级别 升级 level lv",
          ".character level 80", "不写名字就是自己", CAT_CHAR },
        { ".modify gender",    "改变性别", "性别 男 女 gender",
          ".modify gender male", "male / female", CAT_CHAR },
        { ".morph",            "变形",     "变形 模型 外观 morph model",
          ".morph 448",        "填 DisplayID", CAT_CHAR },
        { ".demorph",          "解除变形", "解除变形 还原 demorph",
          ".demorph",          "", CAT_CHAR },
        { ".modify talentpoints", "天赋点", "天赋点 天赋 talent",
          ".modify talentpoints 71", "", CAT_CHAR },
        { ".modify standstate","站立姿势", "姿势 动作 坐 躺 standstate",
          ".modify standstate 1", "0站 1坐 3睡", CAT_CHAR },
        { ".modify phase",     "相位",     "相位 phase 分身",
          ".modify phase 1",   "", CAT_CHAR },
        { ".modify drunk",     "醉酒度",   "喝醉 醉 drunk",
          ".modify drunk 100", "", CAT_CHAR },

        // ---------------- 本服自定义（放前面，你最常用）----------------
        { ".modify allstats",  "五维全改", "五维 全属性 属性 allstats",
          ".modify allstats 5000000", "本服自定义。力敏耐智精一起改", CAT_CUSTOM },
        { ".modify stat",      "单项属性", "力量 敏捷 耐力 智力 精神 stat",
          ".modify stat sta 400000000", "str/agi/sta/int/spi。耐力上限4.2亿", CAT_CUSTOM },
        { ".gearset",          "套装系统", "套装 装备 配装 gearset gear",
          ".gearset 战士 264", "本服自定义。打开菜单直接 .gearset", CAT_CUSTOM },
        { ".gearset tier",     "职业套装", "T套 职业套装 tier",
          ".gearset tier on",  "默认关闭，刷本解锁", CAT_CUSTOM },
        { ".gearset progress", "刷本进度", "进度 刷本 解锁 progress",
          ".gearset progress", "看还差几次解锁套装", CAT_CUSTOM },
        { ".add",              "智能添加", "添加 加物品 搜物品 add",
          ".add 十亿之刃",     "本服自定义。支持中文模糊搜索", CAT_CUSTOM },
        { ".spawn",            "智能生成", "生成 刷怪 召唤NPC spawn",
          ".spawn 训练假人",   "本服自定义。支持中文", CAT_CUSTOM },
        { ".clean",            "清理生成物", "清理 清除 clean",
          ".clean",            "清掉自己刷的NPC", CAT_CUSTOM },
        { ".speed",            "战斗节奏", "GCD 施法间隔 读条 冷却 speed",
          ".speed",            "本服自定义。查看/调整GCD读条CD", CAT_CUSTOM },
        { ".spell clean",      "技能书清理", "技能书 清理技能 低级技能 spell clean",
          ".spell clean",      "本服自定义。清掉被顶替的低阶技能", CAT_CUSTOM },
        { ".reloaditem",       "物品热重载", "重载 刷新物品 reloaditem",
          ".reloaditem 900001","本服自定义。改数值不用重启", CAT_CUSTOM },
        { ".bigtest",          "大数值自检", "自检 测试 bigtest",
          ".bigtest",          "本服自定义。17项检查", CAT_CUSTOM },
        { ".help2",            "指令导览", "帮助 导览 help2",
          ".help2",            "本服自定义。列出全部自定义指令", CAT_CUSTOM },
        { ".transmog copy",    "幻化外观", "幻化 外观 换皮 时装 transmog",
          ".transmog copy 12640", "本服自定义。自动识别部位，属性不变", CAT_CUSTOM },
        { ".transmog find",    "搜外观", "搜外观 找外观 幻化搜索",
          ".transmog find 头盔 紫", "按品质搜，结果可点击看模型", CAT_CUSTOM },
        { ".transmog preview", "试穿外观", "试穿 预览外观 preview",
          ".transmog preview 12640", "临时穿上看效果，15秒自动恢复", CAT_CUSTOM },
        { ".transmog save",    "外观方案", "外观方案 存外观 套装外观",
          ".transmog save 战斗套", "存多套外观，load 一键切换", CAT_CUSTOM },
        { ".item clone",       "克隆装备", "造装备 克隆装备 魔改装备 复制装备",
          ".item clone 49623 装等300 -y", "本服自定义。抄现成装备改数值", CAT_CUSTOM },
        { ".item list",        "自造装备表", "自造装备 我造的装备",
          ".item list",        "列出所有自造装备(800000+)", CAT_CUSTOM },

        // ---------------- 传送移动 ----------------
        { ".tp",               "传送菜单", "传送 tp 去 到 地点 tele",
          ".tp",               "本服传送系统，中文搜索+分类+分页", CAT_TELE },
        { ".tp <关键词>",      "搜传送点", "搜传送 查地点 找地图 tp",
          ".tp 暴风",          "中英文都能搜，结果可点击直接传", CAT_TELE },
        { ".tele",             "原版传送", "原版传送 tele",
          ".tele 暴风城",      "只认英文名，建议改用 .tp", CAT_TELE },
        { ".appear",           "去某人身边", "去找 到某人 appear 传送到玩家",
          ".appear 玩家名",    "", CAT_TELE },
        { ".summon",           "把某人拉来", "拉人 召唤玩家 summon",
          ".summon 玩家名",    "", CAT_TELE },
        { ".go xyz",           "按坐标传送", "坐标 xyz go",
          ".go xyz -8913 554 93", "x y z [mapId]", CAT_TELE },
        { ".go creature",      "去某NPC处", "去NPC go creature",
          ".go creature 12345", "填 GUID 或 entry", CAT_TELE },
        { ".recall",           "回上个位置", "回去 撤销传送 recall",
          ".recall",           "传错了用这个", CAT_TELE },
        { ".unstuck",          "卡住自救",  "卡住 卡死 自救 unstuck",
          ".unstuck",          "", CAT_TELE },
        { ".cheat taxi",       "开全飞行点", "飞行点 开飞行 taxi",
          ".cheat taxi on",    "不污染阵营数据", CAT_TELE },

        // ---------------- 物品装备 ----------------
        { ".additem",          "添加物品", "加物品 给物品 additem item",
          ".additem 49623",    "支持 .additem [物品链接]", CAT_ITEM },
        { ".additem set",      "添加整套", "整套 套装ID additem set",
          ".additem set 881",  "按 ItemSet.dbc 的ID发整套", CAT_ITEM },
        { ".lookup item",      "查物品",   "查物品 找装备 搜物品 lookup item",
          ".lookup item 十亿", "支持中文", CAT_ITEM },
        { ".repairitems",      "修理装备", "修理 修装备 耐久 repair",
          ".repairitems",      "全身+背包都修", CAT_ITEM },
        { ".maxskill",         "技能拉满", "技能满 武器技能 maxskill",
          ".maxskill",         "", CAT_ITEM },
        { ".learn all recipes","学全配方", "配方 学配方 recipes",
          ".learn all recipes","", CAT_ITEM },
        { ".bank",             "随地开银行", "银行 bank",
          ".bank",             "", CAT_ITEM },
        { ".mailbox",          "随地开邮箱", "邮箱 邮件 mailbox mail",
          ".mailbox",          "", CAT_ITEM },

        // ---------------- NPC与生物 ----------------
        { ".npc add",          "生成NPC",  "刷NPC 生成怪 npc add",
          ".npc add 12345",    "填 creature entry", CAT_NPC },
        { ".npc delete",       "删除NPC",  "删NPC 删怪 npc delete",
          ".npc delete",       "先选中目标", CAT_NPC },
        { ".npc info",         "NPC信息",  "NPC信息 查看怪 npc info",
          ".npc info",         "先选中目标", CAT_NPC },
        { ".lookup creature",  "查生物",   "查怪 找NPC lookup creature",
          ".lookup creature 假人", "支持中文", CAT_NPC },
        { ".die",              "杀死目标", "杀死 秒杀 die kill",
          ".die",              "先选中目标", CAT_NPC },
        { ".revive",           "复活",     "复活 revive",
          ".revive",           "", CAT_NPC },
        { ".npcbot add",       "召唤机器人", "机器人 队友 bot npcbot",
          ".npcbot add",       "先选中 bot NPC", CAT_NPC },

        // ---------------- 作弊开关 ----------------
        { ".cheat god",        "无敌",     "无敌 不死 god 免伤",
          ".cheat god on",     "", CAT_CHEAT },
        { ".cheat cooldown",   "无冷却",   "无CD 冷却 cooldown",
          ".cheat cooldown on","注意：会同时干掉GCD", CAT_CHEAT },
        { ".cheat casttime",   "瞬发",     "瞬发 无读条 casttime",
          ".cheat casttime on","", CAT_CHEAT },
        { ".cheat power",      "无限能量", "无限蓝 能量 power",
          ".cheat power on",   "", CAT_CHEAT },
        { ".cheat explore",    "开全地图", "地图 迷雾 探索 explore",
          ".cheat explore on", "", CAT_CHEAT },
        { ".gm on",            "GM模式",   "GM模式 gm on",
          ".gm on",            "怪物不主动攻击", CAT_CHEAT },
        { ".gm fly",           "GM飞行",   "飞 飞行 gm fly",
          ".gm fly on",        "", CAT_CHEAT },
        { ".gm visible",       "隐身",     "隐身 消失 visible",
          ".gm visible off",   "", CAT_CHEAT },
        { ".combatstop",       "强制脱战", "脱战 战斗 combatstop",
          ".combatstop",       "", CAT_CHEAT },

        // ---------------- 副本相关 ----------------
        { ".instance unbind",  "解除副本CD", "副本CD 解除绑定 unbind",
          ".instance unbind all", "", CAT_INST },
        { ".instance stats",   "副本统计", "副本统计 instance stats",
          ".instance stats",   "", CAT_INST },
        { ".reset all",        "重置天赋", "洗点 重置天赋 reset",
          ".reset talents",    "", CAT_INST },
        { ".group leader",     "设队长",   "队长 group leader",
          ".group leader 玩家名", "", CAT_INST },

        // ---------------- 其他 ----------------
        { ".announce",         "全服公告", "公告 广播 announce",
          ".announce 内容",    "", CAT_MISC },
        { ".server info",      "服务器信息", "服务器 在线 info",
          ".server info",      "", CAT_MISC },
        { ".account set gmlevel", "设GM等级", "GM等级 权限 gmlevel",
          ".account set gmlevel 账号 3 -1", "", CAT_MISC },
        { ".reload config",    "重载配置", "重载配置 刷新conf reload",
          ".reload config",    "改完 conf 用这个", CAT_MISC },
        };
        return lib;
    }

    // ------------------------------------------------------------------
    //  搜索：指令名 / 中文名 / 别名 / 说明，任一命中
    // ------------------------------------------------------------------
    inline std::vector<CmdEntry const*> Search(std::string const& kw)
    {
        std::vector<CmdEntry const*> out;
        if (kw.empty())
            return out;

        for (CmdEntry const& e : Library())
        {
            bool hit =
                   (std::string(e.cmd).find(kw)   != std::string::npos)
                || (std::string(e.cn).find(kw)    != std::string::npos)
                || (std::string(e.alias).find(kw) != std::string::npos)
                || (std::string(e.note).find(kw)  != std::string::npos);

            if (hit)
                out.push_back(&e);
        }
        return out;
    }

    inline std::vector<CmdEntry const*> ByCategory(uint8 cat)
    {
        std::vector<CmdEntry const*> out;
        for (CmdEntry const& e : Library())
            if (e.cat == cat)
                out.push_back(&e);
        return out;
    }

    // ------------------------------------------------------------------
    //  会话（菜单分页用）
    // ------------------------------------------------------------------
    struct Session
    {
        uint8  cat  = CAT_MAX;   // CAT_MAX = 在分类列表页
        uint32 page = 0;
        void Clear() { cat = CAT_MAX; page = 0; }
    };

    static std::unordered_map<uint32, Session> s_sess;

    inline Session& Sess(Player* p) { return s_sess[p->GetGUID().GetCounter()]; }
}

// ============================================================================
//  Gossip 回调
// ============================================================================
class gmhelper_playerscript : public PlayerScript
{
public:
    gmhelper_playerscript() : PlayerScript("gmhelper_playerscript") { }

    void OnLogout(Player* player) override
    {
        GmHelper::s_sess.erase(player->GetGUID().GetCounter());
    }

    void OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 sender, uint32 action) override;
};

// ============================================================================
//  指令
// ============================================================================
class gmhelper_commandscript : public CommandScript
{
public:
    gmhelper_commandscript() : CommandScript("gmhelper_commandscript") { }

    // 本仓库 cs_modify.cpp 用旧版框架，保持一致
    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "gmhelp", rbac::RBAC_PERM_COMMAND_GMHELPER, false, &HandleGmHelpCommand, "" },
        };
        return commandTable;
    }

    static bool HandleGmHelpCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::string a = args ? args : "";

        // 拆第一个词
        std::string sub, rest;
        {
            size_t sp = a.find(' ');
            if (sp == std::string::npos) { sub = a; }
            else { sub = a.substr(0, sp); rest = a.substr(sp + 1); }
        }

        if (sub.empty() || sub == "menu")
        {
            ShowCategoryMenu(handler, player);
            return true;
        }
        if (sub == "find" || sub == "search")
            return DoFind(handler, rest);
        if (sub == "cat")
            return ShowCategories(handler);
        if (sub == "list")
            return DoList(handler, rest);

        // 没写子命令，直接当关键词搜
        return DoFind(handler, a);
    }

    // ==================================================================
    //  搜索  —— 核心功能
    // ==================================================================
    static bool DoFind(ChatHandler* handler, std::string const& kw)
    {
        if (kw.empty())
        {
            handler->PSendSysMessage("用法：|cffffff00.gmhelp find <关键词>|r");
            handler->PSendSysMessage("例如：|cffffff00.gmhelp find 体型|r");
            handler->PSendSysMessage("      |cffffff00.gmhelp find 传送|r");
            handler->PSendSysMessage("      |cffffff00.gmhelp find 无敌|r");
            return true;
        }

        std::vector<GmHelper::CmdEntry const*> hits = GmHelper::Search(kw);

        if (hits.empty())
        {
            handler->PSendSysMessage("|cffff8800没有找到「%s」相关的指令|r", kw.c_str());
            handler->PSendSysMessage("试试：|cffffff00.gmhelp menu|r 浏览分类，"
                                     "或换个词再搜");
            return true;
        }

        handler->PSendSysMessage("|cff00ccff===== 搜索「%s」找到 %u 条 =====|r",
                                 kw.c_str(), uint32(hits.size()));

        uint32 shown = 0;
        for (GmHelper::CmdEntry const* e : hits)
        {
            if (shown++ >= 15)
            {
                handler->PSendSysMessage("|cff888888... 还有 %u 条，请用更精确的关键词|r",
                                         uint32(hits.size()) - 15);
                break;
            }

            handler->PSendSysMessage("|cff00ff00%s|r  |cffffff00%s|r",
                                     e->cn, e->cmd);
            handler->PSendSysMessage("   用法：|cffaaaaaa%s|r", e->usage);
            if (e->note && *e->note)
                handler->PSendSysMessage("   说明：%s", e->note);
        }
        handler->PSendSysMessage("|cff00ccff================================|r");
        return true;
    }

    // ==================================================================
    //  分类列表（文字版）
    // ==================================================================
    static bool ShowCategories(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ccff========== 指令分类 ==========|r");
        for (uint8 c = 0; c < GmHelper::CAT_MAX; ++c)
        {
            uint32 n = uint32(GmHelper::ByCategory(c).size());
            if (!n) continue;
            handler->PSendSysMessage("  |cff00ff00%s|r  (%u 条)   "
                                     "|cffaaaaaa.gmhelp list %s|r",
                                     GmHelper::CatName(c), n, GmHelper::CatName(c));
        }
        handler->PSendSysMessage("|cff00ccff==============================|r");
        handler->PSendSysMessage("或用 |cffffff00.gmhelp menu|r 打开可点击菜单");
        return true;
    }

    // ==================================================================
    //  列出某分类
    // ==================================================================
    static bool DoList(ChatHandler* handler, std::string const& catName)
    {
        if (catName.empty())
            return ShowCategories(handler);

        uint8 found = GmHelper::CAT_MAX;
        for (uint8 c = 0; c < GmHelper::CAT_MAX; ++c)
        {
            if (catName == GmHelper::CatName(c))
            {
                found = c;
                break;
            }
        }

        if (found == GmHelper::CAT_MAX)
        {
            handler->PSendSysMessage("|cffff0000未知分类：%s|r", catName.c_str());
            return ShowCategories(handler);
        }

        std::vector<GmHelper::CmdEntry const*> list = GmHelper::ByCategory(found);
        handler->PSendSysMessage("|cff00ccff===== %s (%u 条) =====|r",
                                 GmHelper::CatName(found), uint32(list.size()));
        for (GmHelper::CmdEntry const* e : list)
        {
            handler->PSendSysMessage("|cff00ff00%s|r  |cffffff00%s|r", e->cn, e->cmd);
            handler->PSendSysMessage("   |cffaaaaaa%s|r", e->usage);
        }
        return true;
    }

    // ==================================================================
    //  分类菜单（Gossip）
    // ==================================================================
    static void ShowCategoryMenu(ChatHandler* /*handler*/, Player* player)
    {
        GmHelper::Sess(player).Clear();

        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        for (uint8 c = 0; c < GmHelper::CAT_MAX; ++c)
        {
            uint32 n = uint32(GmHelper::ByCategory(c).size());
            if (!n)
                continue;
            std::string label = std::string("【") + GmHelper::CatName(c) + "】  "
                              + std::to_string(n) + " 条";
            menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, label,
                             GmHelper::SENDER_CAT, c, "", 0, false);
        }

        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT,
                         "|cff888888提示：也可以用 .gmhelp find <关键词> 直接搜|r",
                         GmHelper::SENDER_NAV, GmHelper::NAV_CLOSE, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888关闭|r",
                         GmHelper::SENDER_NAV, GmHelper::NAV_CLOSE, "", 0, false);

        // 用玩家自己 GUID 合法（MiscHandler.cpp:150 有 guid.IsPlayer() 分支）
        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }

    // ==================================================================
    //  指令列表菜单（带分页，严守 32 条上限）
    // ==================================================================
    static void ShowCmdMenu(Player* player)
    {
        GmHelper::Session& ss = GmHelper::Sess(player);
        std::vector<GmHelper::CmdEntry const*> list = GmHelper::ByCategory(ss.cat);

        uint32 total = uint32(list.size());
        uint32 maxPg = total ? ((total - 1) / GmHelper::PER_PAGE) : 0;
        if (ss.page > maxPg)
            ss.page = maxPg;

        uint32 begin = ss.page * GmHelper::PER_PAGE;
        uint32 end   = std::min(begin + GmHelper::PER_PAGE, total);

        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        for (uint32 i = begin; i < end; ++i)
        {
            std::string label = std::string("|cff00ff00") + list[i]->cn + "|r  "
                              + list[i]->cmd;
            menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, label,
                             GmHelper::SENDER_CMD, i, "", 0, false);
        }

        // 导航最多 3 条，29 + 3 = 32 正好卡住上限
        if (ss.page > 0)
            menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff00ccff<< 上一页|r",
                             GmHelper::SENDER_NAV, GmHelper::NAV_PREV, "", 0, false);
        if (ss.page < maxPg)
            menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff00ccff下一页 >>|r",
                             GmHelper::SENDER_NAV, GmHelper::NAV_NEXT, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888返回分类|r",
                         GmHelper::SENDER_NAV, GmHelper::NAV_BACK, "", 0, false);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }

    // ==================================================================
    //  显示单条指令详情
    // ==================================================================
    static void ShowCmdDetail(ChatHandler* handler, GmHelper::CmdEntry const* e)
    {
        if (!e)
            return;

        handler->PSendSysMessage("|cff00ccff========== %s ==========|r", e->cn);
        handler->PSendSysMessage("  指令：|cffffff00%s|r", e->cmd);
        handler->PSendSysMessage("  用法：|cff00ff00%s|r", e->usage);
        if (e->note && *e->note)
            handler->PSendSysMessage("  说明：%s", e->note);
        handler->PSendSysMessage("|cff888888复制上面的用法直接输入即可|r");
    }
};

// ============================================================================
//  Gossip 回调实现（放在 commandscript 之后，要调它的 static 函数）
// ============================================================================
void gmhelper_playerscript::OnGossipSelect(Player* player, uint32 /*menuId*/,
                                           uint32 sender, uint32 action)
{
    if (!player)
        return;

    // 只处理本系统的 sender
    if (sender != GmHelper::SENDER_CAT
     && sender != GmHelper::SENDER_CMD
     && sender != GmHelper::SENDER_QUICK
     && sender != GmHelper::SENDER_NAV)
        return;

    ChatHandler handler(player->GetSession());
    GmHelper::Session& ss = GmHelper::Sess(player);

    switch (sender)
    {
        case GmHelper::SENDER_CAT:
            ss.cat  = uint8(action);
            ss.page = 0;
            player->PlayerTalkClass->SendCloseGossip();
            gmhelper_commandscript::ShowCmdMenu(player);
            return;

        case GmHelper::SENDER_CMD:
        {
            player->PlayerTalkClass->SendCloseGossip();
            std::vector<GmHelper::CmdEntry const*> list = GmHelper::ByCategory(ss.cat);
            if (action < list.size())
                gmhelper_commandscript::ShowCmdDetail(&handler, list[action]);
            return;
        }

        case GmHelper::SENDER_NAV:
            switch (action)
            {
                case GmHelper::NAV_PREV:
                    if (ss.page > 0) --ss.page;
                    gmhelper_commandscript::ShowCmdMenu(player);
                    return;
                case GmHelper::NAV_NEXT:
                    ++ss.page;
                    gmhelper_commandscript::ShowCmdMenu(player);
                    return;
                case GmHelper::NAV_BACK:
                    // 不能先 SendCloseGossip()：客户端收到关闭包后会把
                    // 紧随其后的菜单包丢掉，表现为「点返回菜单就没了」。
                    // ShowCategoryMenu 内部已有 ClearMenus()，直接调即可。
                    gmhelper_commandscript::ShowCategoryMenu(&handler, player);
                    return;
                case GmHelper::NAV_CLOSE:
                default:
                    player->PlayerTalkClass->SendCloseGossip();
                    ss.Clear();
                    return;
            }

        default:
            return;
    }
}

void AddSC_gmhelper_commandscript()
{
    new gmhelper_commandscript();
    new gmhelper_playerscript();
}
