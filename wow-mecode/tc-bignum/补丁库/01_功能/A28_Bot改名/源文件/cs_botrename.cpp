/*
 * ============================================================================
 *  step43  .botname —— 给 NPCBot 和 PlayerBot 改名（支持中英文）
 * ============================================================================
 *
 *  用户需求：「可以用指令或者sql更改npcbot和playerbot的角色名，包括中英文」
 *
 *  ==========================================================================
 *  【先说清楚：两种bot是完全不同的东西，改名机制也完全不同】
 *  ==========================================================================
 *
 *    NPCBot    = Creature      名字在 world.creature_template.Name
 *                              -> 【同 entry 的所有bot共享一个名字】
 *                              -> 客户端走 CMSG_CREATURE_QUERY 拿名字并【本地缓存】
 *
 *    PlayerBot = 真实 Player   名字在 characters.characters.name
 *                              -> 每个角色独立
 *                              -> 走官方 .character rename 那套
 *
 *  ==========================================================================
 *  【中文名到底能不能用】—— 实查结论：能，而且是官方原生支持
 *  ==========================================================================
 *
 *    ObjectMgr.cpp:8669  CheckPlayerName()
 *      -> isValidString(wname, strictMask, false, create)   (ObjectMgr.cpp:8633)
 *         strictMask == 0 时：
 *             if (isEastAsianString(wstr, numericOrSpace)) return true;
 *                ^^^^^^^^^^^^^^^^^^ 【中日韩】
 *
 *    Util.h:155  isEastAsianCharacter()
 *      if (wchar >= 0x4E00 && wchar <= 0x9FC3)   // Unified CJK Ideographs
 *          return true;                          <- 汉字在这个区间
 *
 *    worldserver.conf.dist:607  StrictPlayerNames = 0   <- 默认值
 *
 *    => 默认配置下【中文名直接可用】，不用改任何配置。
 *       如果你把 StrictPlayerNames 改成 1（只允许基础拉丁字母），
 *       中文就会被拒 —— 那是配置问题，不是代码问题。
 *
 *  ==========================================================================
 *  【NPCBot 中英文双名的做法】
 *  ==========================================================================
 *
 *    Creature.cpp:3093  GetNameForLocaleIdx(loc_idx)
 *    {
 *        if (loc_idx != DEFAULT_LOCALE)
 *            if (CreatureLocale const* cl = GetCreatureLocale(GetEntry()))
 *                if (cl->Name.size() > uloc_idx && !cl->Name[uloc_idx].empty())
 *                    return cl->Name[uloc_idx];      <- 【优先用本地化名】
 *        return GetName();                           <- 没有才用默认名
 *    }
 *
 *    所以：
 *      creature_template.Name         放【英文名】（默认，其它语言客户端看到）
 *      creature_template_locale.Name  放【中文名】（zhCN 客户端看到）
 *    两者【可以共存】，这就是 .botname 的 -en / -cn 两个开关的由来。
 *
 *  ==========================================================================
 *  【一个必须知道的坑：客户端会缓存名字】
 *  ==========================================================================
 *
 *    QueryHandler.cpp:119  HandleCreatureQueryOpcode
 *      客户端第一次见到某个 entry 时发 CMSG_CREATURE_QUERY 问名字，
 *      拿到后【存在本地 WDB 缓存文件里】，之后不再问。
 *
 *    => 改完 NPCBot 名字后，你自己的客户端可能还显示旧名。
 *       这【不是没改成功】。解决办法（指令里会提示）：
 *         1. 删掉 客户端目录\Cache\WDB\zhCN\creaturecache.wdb
 *         2. 重登客户端
 *       别的玩家（没缓存过这个entry的）会直接看到新名字。
 *
 *  权限：rbac::RBAC_PERM_COMMAND_WORLDTOOLS（step21 自建 = 71012）
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "CharacterCache.h"     // sCharacterCache->UpdateCharacterData (CharacterCache.h:50)
#include "Creature.h"
#include "DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"          // CheckPlayerName(1470) / IsReservedName(1467) / normalizePlayerName(890)
#include "Player.h"
#include "RBAC.h"
#include "World.h"
#include "WorldSession.h"

#include <cstdio>
#include <string>
#include <vector>

// ============================================================================
//  小工具：切词（和 cs_playerbot.cpp 同款，保持一致）
// ============================================================================
static std::vector<std::string> BnTok(char const* args)
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
        size_t start = i;
        while (i < s.size() && s[i] != ' ' && s[i] != '\t')
            ++i;
        out.push_back(s.substr(start, i - start));
    }
    return out;
}

static std::string BnLower(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}

// ============================================================================
//  把 tok[from] 之后的所有词拼回一个字符串
// ============================================================================
//  名字可能带空格（比如 "Arthas Menethil"），切词后要拼回去。
static std::string BnJoin(std::vector<std::string> const& tok, size_t from)
{
    std::string s;
    for (size_t i = from; i < tok.size(); ++i)
    {
        if (!s.empty())
            s += ' ';
        s += tok[i];
    }
    return s;
}

// ============================================================================
//  名字合法性检查（两种bot共用）
// ============================================================================
//  照抄官方 cs_character.cpp:302 起的检查顺序。
//  返回 true = 合法。
static bool BnValidateName(ChatHandler* handler, std::string const& name, bool isPlayerBot)
{
    char buf[512];

    if (name.empty())
    {
        handler->SendSysMessage("|cffff0000 名字不能为空|r");
        return false;
    }

    // ---- Creature 的名字限制比 Player 宽松得多 ----
    // Player 走官方全套校验；Creature 只要不太离谱就行。
    if (!isPlayerBot)
    {
        // creature_template.Name 是 varchar(100)
        if (name.size() > 100)
        {
            handler->SendSysMessage("|cffff0000 名字太长了（最多100字节）|r");
            return false;
        }
        return true;
    }

    // ================= 以下是 PlayerBot（真实角色）的校验 =================
    // 逐条对应 cs_character.cpp:302-330

    std::string check = name;

    // cs_character.cpp:303  normalizePlayerName
    // 【注意】这个函数会把首字母大写、其余小写。
    //  对中文名没有影响（中文没有大小写），对英文名会规范化。
    if (!normalizePlayerName(check))            // ObjectMgr.h:890
    {
        handler->SendSysMessage("|cffff0000 名字格式非法|r");
        return false;
    }

    // cs_character.cpp:309  CheckPlayerName
    //   内部：长度 / 语言(含中文) / 三连字符 / 保留词
    LocaleConstant loc = handler->GetSession()
        ? handler->GetSession()->GetSessionDbcLocale()
        : sWorld->GetDefaultDbcLocale();

    ResponseCodes res = ObjectMgr::CheckPlayerName(check, loc, true);   // ObjectMgr.h:1470 public(947段)
    if (res != CHAR_NAME_SUCCESS)
    {
        // 把官方的错误码翻译成人话 —— 不然你看到一串数字不知道哪儿错了
        char const* why = "名字不合法";
        switch (res)
        {
            case CHAR_NAME_TOO_LONG:            why = "名字太长"; break;
            case CHAR_NAME_TOO_SHORT:           why = "名字太短"; break;
            case CHAR_NAME_INVALID_CHARACTER:   why = "含有非法字符"; break;
            case CHAR_NAME_MIXED_LANGUAGES:     why = "混用了多种语言（中文和英文不能混）"; break;
            case CHAR_NAME_THREE_CONSECUTIVE:   why = "有三个连续相同的字符"; break;
            case CHAR_NAME_RESERVED:            why = "是保留名"; break;
            case CHAR_NAME_PROFANE:             why = "被判定为不雅词汇"; break;
            default: break;
        }
        snprintf(buf, sizeof(buf), "|cffff0000 %s|r", why);
        handler->SendSysMessage(buf);

        if (res == CHAR_NAME_MIXED_LANGUAGES)
        {
            handler->SendSysMessage("|cffffff00 提示: 纯中文可以，纯英文可以，但【不能混着写】|r");
            handler->SendSysMessage("|cffffff00 这是暴雪原版规则(ObjectMgr.cpp:8633 isValidString)|r");
        }
        return false;
    }

    // cs_character.cpp:317  保留名检查
    if (WorldSession* session = handler->GetSession())
    {
        if (!session->HasPermission(rbac::RBAC_PERM_SKIP_CHECK_CHARACTER_CREATION_RESERVEDNAME) &&
            sObjectMgr->IsReservedName(check))                          // ObjectMgr.h:1467 public
        {
            handler->SendSysMessage("|cffff0000 这是保留名，不能用|r");
            return false;
        }
    }

    return true;
}

// ============================================================================
//  .botname 实现
// ============================================================================
class botrename_commandscript : public CommandScript
{
public:
    botrename_commandscript() : CommandScript("botrename_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "botname", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleBotName, "" },
        };
        return commandTable;
    }

    static void SendHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff00ff00[.botname Bot改名]|r");
        handler->SendSysMessage("|cffffff00--- NPCBot（选中它，或给entry）---|r");
        handler->SendSysMessage("  .botname cn <中文名>        改中文名（zhCN客户端可见）");
        handler->SendSysMessage("  .botname en <英文名>        改英文名（默认名）");
        handler->SendSysMessage("  .botname entry <ID> cn <名> 按entry改，不用选中");
        handler->SendSysMessage("  .botname show               看当前中英文名");
        handler->SendSysMessage("|cffffff00--- PlayerBot（真实角色）---|r");
        handler->SendSysMessage("  .botname player <旧名> <新名>");
        handler->SendSysMessage("|cffffff00--- 中文别名 ---|r");
        handler->SendSysMessage("  .botname 中文 / 英文 / 编号 / 查看 / 玩家");
        handler->SendSysMessage("|cffff0000 注意: NPCBot改名影响【同entry的所有bot】|r");
    }

    // ------------------------------------------------------------------
    //  显示某个 entry 当前的中英文名
    // ------------------------------------------------------------------
    static void ShowNames(ChatHandler* handler, uint32 entry)
    {
        char buf[512];

        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(entry);
        if (!ct)
        {
            snprintf(buf, sizeof(buf), "|cffff0000 找不到 entry %u|r", entry);
            handler->SendSysMessage(buf);
            return;
        }

        snprintf(buf, sizeof(buf), "|cff00ff00[entry %u]|r", entry);
        handler->SendSysMessage(buf);

        snprintf(buf, sizeof(buf), "  英文名(默认): |cffffffff%s|r", ct->Name.c_str());
        handler->SendSysMessage(buf);

        // 本地化名从 world.creature_template_locale 读
        // ObjectMgr.cpp:266  SELECT entry, locale, Name, Title FROM creature_template_locale
        QueryResult res = WorldDatabase.PQuery(
            "SELECT `Name` FROM `creature_template_locale` WHERE `entry` = {} AND `locale` = 'zhCN'",
            entry);

        if (res)
        {
            snprintf(buf, sizeof(buf), "  中文名(zhCN): |cff00ff00%s|r",
                     (*res)[0].GetCString());
            handler->SendSysMessage(buf);
        }
        else
        {
            handler->SendSysMessage("  中文名(zhCN): |cff888888(未设置，会显示英文名)|r");
        }
    }

    // ------------------------------------------------------------------
    //  改 NPCBot 的名字
    // ------------------------------------------------------------------
    static bool RenameNpcBot(ChatHandler* handler, uint32 entry, std::string const& newName, bool chinese)
    {
        char buf[512];

        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(entry);
        if (!ct)
        {
            snprintf(buf, sizeof(buf), "|cffff0000 找不到 entry %u|r", entry);
            handler->SendSysMessage(buf);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!BnValidateName(handler, newName, false))
        {
            handler->SetSentErrorMessage(true);
            return false;
        }

        // SQL 转义 —— 名字里可能有单引号
        std::string safe = newName;
        WorldDatabase.EscapeString(safe);

        if (chinese)
        {
            // ---- 中文名写进 creature_template_locale ----
            // 表结构 ObjectMgr.cpp:266  (entry, locale, Name, Title)
            // 用 REPLACE 而不是 INSERT，已存在就覆盖。
            //
            // 铁律5：本仓库 PExecute 走 fmt 库，占位符是 {} 不是 %u
            WorldDatabase.PExecute(
                "REPLACE INTO `creature_template_locale` (`entry`, `locale`, `Name`) "
                "VALUES ({}, 'zhCN', '{}')", entry, safe);

            // 热加载，不用重启
            sObjectMgr->LoadCreatureLocales();      // ObjectMgr.h:1156
        }
        else
        {
            // ---- 英文名（默认名）写进 creature_template ----
            WorldDatabase.PExecute(
                "UPDATE `creature_template` SET `name` = '{}' WHERE `entry` = {}", safe, entry);

            // creature_template 没有单表热载函数，
            // 但官方 .reload creature_template <entry> 走的是
            // sObjectMgr->LoadCreatureTemplate(fields)（cs_reload.cpp:293）。
            // 我们这里提示用户去执行，比在这儿复刻一遍安全。
        }

        snprintf(buf, sizeof(buf), "|cff00ff00 entry %u 的%s名已改为: %s|r",
                 entry, chinese ? "中文" : "英文", newName.c_str());
        handler->SendSysMessage(buf);

        // ---- 已在世界里的同 entry 生物，内存里的名字也要跟着改 ----
        //  不改的话，服务端内部（比如聊天前缀、日志）还是旧名。
        //
        //  【为什么用 GetMap()->GetCreatureBySpawnId 这类办法不行】
        //  同 entry 可能有很多只、分布在不同地图，没有"按entry取全部"的现成接口。
        //  这里退而求其次：只改【你视野附近】的，其余靠 reload + 重进视野刷新。
        //  这是有意的取舍 —— 全服扫所有地图的所有生物代价太大，
        //  而且 Creature 的名字客户端本来就走 entry 查询，改内存意义有限。
        //
        //  Object.h:399  SetName(std::string)  public(356段)
        if (Unit* sel = handler->getSelectedUnit())
        {
            if (sel->GetTypeId() == TYPEID_UNIT && sel->GetEntry() == entry)
            {
                // 只有改默认名时才动内存；中文名是 locale 层，
                // 走 GetNameForLocaleIdx 自动生效，不该覆盖 m_name。
                if (!chinese)
                    sel->SetName(newName);
            }
        }

        if (!chinese)
        {
            snprintf(buf, sizeof(buf),
                     "|cffffff00 请执行: .reload creature_template %u|r", entry);
            handler->SendSysMessage(buf);
        }

        // ---- 客户端缓存提醒（这是最容易被误判成"没生效"的地方）----
        handler->SendSysMessage("|cffffff00 如果你的客户端还显示旧名字:|r");
        handler->SendSysMessage("|cffffff00   删掉 Cache\\WDB\\zhCN\\creaturecache.wdb 后重登|r");
        handler->SendSysMessage("|cffffff00   （客户端会缓存NPC名字，这不是没改成功）|r");

        return true;
    }

    // ------------------------------------------------------------------
    //  改 PlayerBot 的名字
    // ------------------------------------------------------------------
    //  照抄官方 cs_character.cpp:286 HandleCharacterRenameCommand，
    //  但【去掉 KickPlayer】—— 官方改在线角色时会踢下线让客户端刷新，
    //  对 PlayerBot 来说踢下线 = bot 没了，这不是我们要的。
    //  bot 没有客户端，不需要靠重连来刷新界面。
    static bool RenamePlayerBot(ChatHandler* handler, std::string const& oldName, std::string const& newName)
    {
        char buf[512];

        if (!BnValidateName(handler, newName, true))
        {
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 规范化（官方会做这一步，我们要和它一致）
        std::string finalName = newName;
        normalizePlayerName(finalName);

        // ---- 查旧角色 ----
        std::string safeOld = oldName;
        CharacterDatabase.EscapeString(safeOld);

        QueryResult res = CharacterDatabase.PQuery(
            "SELECT guid FROM characters WHERE name = '{}'", safeOld);
        if (!res)
        {
            snprintf(buf, sizeof(buf), "|cffff0000 找不到角色 %s|r", oldName.c_str());
            handler->SendSysMessage(buf);
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 low = (*res)[0].GetUInt32();
        // ObjectGuid 没有 (HighGuid, low) 构造函数，用官方工厂（ObjectGuid.h:224）
        ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(low);

        // ---- 新名字不能和别人重复 ----
        std::string safeNew = finalName;
        CharacterDatabase.EscapeString(safeNew);

        QueryResult dup = CharacterDatabase.PQuery(
            "SELECT guid FROM characters WHERE name = '{}'", safeNew);
        if (dup)
        {
            uint32 dupLow = (*dup)[0].GetUInt32();
            if (dupLow != low)      // 改成自己现在的名字不算重复
            {
                snprintf(buf, sizeof(buf), "|cffff0000 名字 %s 已经被占用了|r", finalName.c_str());
                handler->SendSysMessage(buf);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        // ---- 清掉变格名（俄语客户端用的，不清会对不上）----
        // cs_character.cpp:337  CHAR_DEL_CHAR_DECLINED_NAME
        CharacterDatabase.PExecute("DELETE FROM character_declinedname WHERE guid = {}", low);

        // ---- 在线的直接改内存，离线的改库 ----
        if (Player* target = ObjectAccessor::FindPlayer(guid))
        {
            target->SetName(finalName);         // Object.h:399 public(356段)

            //【和官方的区别】这里【不】调 KickPlayer。
            //  官方 cs_character.cpp:346 踢人是为了让【真人客户端】重新加载角色名，
            //  bot 没有客户端，踢了反而把 bot 弄没了。
            //
            //  但内存里改了还要落库，否则重启就回去了：
            CharacterDatabase.PExecute(
                "UPDATE characters SET name = '{}' WHERE guid = {}", safeNew, low);

            handler->SendSysMessage("|cffffff00 该bot在线，已同时改内存和数据库|r");
        }
        else
        {
            CharacterDatabase.PExecute(
                "UPDATE characters SET name = '{}' WHERE guid = {}", safeNew, low);
        }

        // ---- 更新角色名缓存（不更新的话 /who、好友列表还是旧名）----
        // CharacterCache.h:50
        sCharacterCache->UpdateCharacterData(guid, finalName);

        snprintf(buf, sizeof(buf), "|cff00ff00 %s 已改名为 %s|r",
                 oldName.c_str(), finalName.c_str());
        handler->SendSysMessage(buf);

        // g_pbots 里存的名字也要同步，否则 .pbot come <名> 会找不到
        handler->SendSysMessage("|cffffff00 提示: 如果它是 .pbot 登录的，|r");
        handler->SendSysMessage("|cffffff00 请 .pbot despawn 旧名 后用新名重新 spawn|r");

        return true;
    }

    // ------------------------------------------------------------------
    static bool HandleBotName(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = BnTok(args);
        if (tok.empty())
        {
            SendHelp(handler);
            return true;
        }

        char buf[512];
        std::string s0 = BnLower(tok[0]);

        // ---------- player：改 PlayerBot ----------
        if (s0 == "player" || tok[0] == "玩家" || tok[0] == "角色")
        {
            if (tok.size() < 3)
            {
                handler->SendSysMessage("|cffff0000 用法: .botname player <旧名> <新名>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            // 新名可能带空格，把第3个词之后全拼起来
            return RenamePlayerBot(handler, tok[1], BnJoin(tok, 2));
        }

        // ---------- entry：按 entry 改，不用选中 ----------
        if (s0 == "entry" || tok[0] == "编号")
        {
            if (tok.size() < 4)
            {
                handler->SendSysMessage("|cffff0000 用法: .botname entry <ID> <cn|en> <名字>|r");
                handler->SendSysMessage("|cffffff00 例: .botname entry 70001 cn 阿尔萨斯|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            uint32 entry = uint32(atoi(tok[1].c_str()));
            if (!entry)
            {
                handler->SendSysMessage("|cffff0000 entry 必须是数字|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            std::string mode = BnLower(tok[2]);
            bool chinese = (mode == "cn" || mode == "zh" || tok[2] == "中文");
            bool english = (mode == "en" || tok[2] == "英文");

            if (!chinese && !english)
            {
                handler->SendSysMessage("|cffff0000 第三个参数只能是 cn 或 en|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            return RenameNpcBot(handler, entry, BnJoin(tok, 3), chinese);
        }

        // ---------- show：看当前名字 ----------
        if (s0 == "show" || tok[0] == "查看")
        {
            uint32 entry = 0;

            if (tok.size() >= 2)
                entry = uint32(atoi(tok[1].c_str()));
            else
            {
                // 没给entry就用选中的目标
                Unit* sel = handler->getSelectedUnit();
                if (!sel || sel->GetTypeId() != TYPEID_UNIT)
                {
                    handler->SendSysMessage("|cffff0000 先选中一个NPC，或 .botname show <entry>|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                entry = sel->GetEntry();
            }

            ShowNames(handler, entry);
            return true;
        }

        // ---------- cn / en：改选中目标 ----------
        if (s0 == "cn" || s0 == "zh" || tok[0] == "中文" ||
            s0 == "en" || tok[0] == "英文")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("|cffff0000 用法: .botname cn <新名字>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            Unit* sel = handler->getSelectedUnit();
            if (!sel || sel->GetTypeId() != TYPEID_UNIT)
            {
                handler->SendSysMessage("|cffff0000 请先选中一个NPC/Bot|r");
                handler->SendSysMessage("|cffffff00 或用: .botname entry <ID> cn <名字>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            bool chinese = (s0 == "cn" || s0 == "zh" || tok[0] == "中文");

            snprintf(buf, sizeof(buf), "|cffffff00 目标 entry = %u|r", sel->GetEntry());
            handler->SendSysMessage(buf);

            return RenameNpcBot(handler, sel->GetEntry(), BnJoin(tok, 1), chinese);
        }

        SendHelp(handler);
        return true;
    }
};

void AddSC_botrename_commandscript()
{
    new botrename_commandscript();
}

/* ============================================================================
 *  API 核实记录（全部含访问段，逐条 grep 实查）
 * ============================================================================
 *  【名字校验】
 *  ObjectMgr.h:890    normalizePlayerName(std::string&)        全局函数
 *  ObjectMgr.h:1467   IsReservedName()                         public(947段)
 *  ObjectMgr.h:1470   CheckPlayerName() static                 public(947段)
 *  ObjectMgr.cpp:8669 CheckPlayerName 实现
 *  ObjectMgr.cpp:8633 isValidString -> isEastAsianString       <- 中文放行处
 *  Util.h:155         isEastAsianCharacter
 *                     0x4E00-0x9FC3 = Unified CJK Ideographs   <- 汉字
 *  worldserver.conf.dist:607  StrictPlayerNames = 0（默认，中文可用）
 *
 *  【NPCBot / Creature】
 *  Creature.cpp:3093  GetNameForLocaleIdx  优先 locale 名，回退默认名
 *  CreatureData.h:305 CreatureTemplate::Name
 *  CreatureData.h:451 CreatureLocale::Name（vector，按locale索引）
 *  ObjectMgr.cpp:266  SELECT entry, locale, Name, Title FROM creature_template_locale
 *  ObjectMgr.h:1156   LoadCreatureLocales()  <- 热加载中文名
 *  Object.h:399       SetName(std::string)                     public(356段)
 *  QueryHandler.cpp:119  HandleCreatureQueryOpcode <- 客户端拿名字并缓存
 *  cs_reload.cpp:98   .reload creature_template <entry>
 *  cs_reload.cpp:116  .reload creature_template_locale
 *
 *  【PlayerBot / Player】
 *  cs_character.cpp:286  HandleCharacterRenameCommand  <- 逻辑照抄自这里
 *  cs_character.cpp:346  KickPlayer  <-【我们故意不做这步】
 *  CharacterCache.h:50   UpdateCharacterData(guid, name, ...)
 *  CharacterDatabase.cpp:163  CHAR_UPD_NAME_BY_GUID
 *                             "UPDATE characters SET name = ? WHERE guid = ?"
 *  ObjectGuid.h:224      ObjectGuid::Create<HighGuid::Player>(low)
 *
 *  【铁律遵守】
 *  - PExecute/PQuery 用 {} 不是 %u（DatabaseWorkerPool.h:99/133）
 *  - 所有 API 都确认了访问段
 *  - 不硬编码 entry / guid，全部运行时查
 * ============================================================================
 */
