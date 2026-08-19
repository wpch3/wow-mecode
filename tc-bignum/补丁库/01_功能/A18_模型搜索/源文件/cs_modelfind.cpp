/*
 * ============================================================================
 *  step32  .findmodel —— 游戏内模型搜索
 * ============================================================================
 *
 *  【为什么要有这个指令】
 *
 *  用户问：".model 非要这么麻烦吗？没有任何分享模型 id 的网站吗？"
 *
 *  答案分两半：
 *    1. 有网站（wowhead / dMorph 列表），但它们给的是【暴雪原版 displayid】。
 *       你的客户端打了 HD 补丁，模型文件被换过，网站的号对不上你的客户端。
 *       这就是 step31 v1 硬编码 displayid 全错的根本原因。
 *    2. 真正的权威表就在你自己硬盘里：
 *          D:\TC-Build\bin\RelWithDebInfo\dbc\CreatureDisplayInfo.dbc
 *          D:\TC-Build\bin\RelWithDebInfo\dbc\CreatureModelData.dbc
 *       服务端启动时已经把它们全读进内存了，只是没有指令能查。
 *       本指令就是把这两张表暴露成一个可搜索的界面。
 *
 *  【原理链条】（全部实查源码，见文末"API 核实记录"）
 *
 *      displayid  --CreatureDisplayInfo.dbc-->  ModelID
 *      ModelID    --CreatureModelData.dbc  -->  ModelName（模型文件路径字符串）
 *
 *      例：displayid 26232 -> ModelID 2775 -> "Creature\Nerubian\Nerubian.mdx"
 *
 *      ModelName 是【人能读懂的路径】，所以可以按关键字搜。
 *      搜 "wolf" 就能列出所有狼模型，搜 "Nerubian" 列出所有蜘蛛人。
 *
 *      这条链是服务端 DBC 的真实内容，不是网上抄来的表，
 *      所以【你的客户端换了什么模型，这里搜出来的就是什么】。
 *
 *  【铁律遵守】
 *    - 不硬编码任何 displayid：全部实时遍历 DBC
 *    - SQL 占位符用 {} 不用 %u（本仓库 DirectPExecute 走 fmt）—— 本文件不写库
 *    - 权限沿用 RBAC_PERM_COMMAND_WORLDTOOLS（step21 自建 = 71012）
 *    - 文档/代码 GBK 兼容：不使用特殊符号（勾叉箭头等）
 *
 *  权限：rbac::RBAC_PERM_COMMAND_WORLDTOOLS
 *  依赖：DBCStores.h（sCreatureDisplayInfoStore / sCreatureModelDataStore）
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
//  小工具（与 step29/30/31 保持一致）
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

static std::string Lower(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
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

// 大小写无关的子串查找。
// 不用 strcasestr —— 那是 POSIX 扩展，MSVC 没有。
static bool ContainsNoCase(char const* hay, std::string const& needleLower)
{
    if (!hay || !*hay)
        return false;
    if (needleLower.empty())
        return true;

    std::string h = Lower(std::string(hay));
    return h.find(needleLower) != std::string::npos;
}

// v2 修复：把字符串里所有【非字母数字】的字符去掉，全部转小写。
//
// 起因（用户实测）：`.fm The Lich King` 搜不到。
// 病因：旧代码把多个词拼成 "the lich king"（带空格），
//       但 DBC 里的路径是 Creature\LichKing\LichKing.mdx —— 【没有空格】，
//       所以 find("the lich king") 必然失败。
//
// 同一个模型在不同地方的写法千奇百怪：
//       LichKing / Lich_King / lich-king / The Lich King
// 全部规约成 lichking 之后就能互相匹配。
static std::string Squash(std::string const& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        if (c >= 'A' && c <= 'Z')
            out += char(c - 'A' + 'a');
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            out += c;
        // 其余（空格 下划线 减号 反斜杠 点 中文字节…）一律丢弃
    }
    return out;
}

// 取某个 displayid 对应的模型文件名。查不到返回 nullptr。
//   DBCStructure.h:442 CreatureDisplayInfoEntry { ID, ModelID, ExtendedDisplayInfoID, CreatureModelScale }
//   DBCStructure.h:496 CreatureModelDataEntry   { ID, Flags, ModelName, ModelScale, ... }
static char const* GetModelNameByDisplayId(uint32 displayId)
{
    CreatureDisplayInfoEntry const* disp = sCreatureDisplayInfoStore.LookupEntry(displayId);
    if (!disp)
        return nullptr;

    CreatureModelDataEntry const* md = sCreatureModelDataStore.LookupEntry(disp->ModelID);
    if (!md)
        return nullptr;

    return md->ModelName;
}

// 把 "Creature\Nerubian\Nerubian.mdx" 削成 "Nerubian"，方便肉眼扫。
// 只做显示用，不参与匹配（匹配用全路径，能搜目录名）。
static std::string ShortName(char const* full)
{
    if (!full || !*full)
        return std::string();

    std::string s(full);

    // 去目录：反斜杠和正斜杠都要处理（DBC 里是反斜杠，但保险起见）
    size_t p = s.find_last_of("\\/");
    if (p != std::string::npos)
        s = s.substr(p + 1);

    // 去扩展名（.mdx / .m2）
    p = s.find_last_of('.');
    if (p != std::string::npos)
        s = s.substr(0, p);

    return s;
}

// ============================================================================
//  搜索结果
// ============================================================================

struct ModelHit
{
    uint32      displayId;
    uint32      modelId;
    std::string shortName;
    std::string fullName;
};

// 一次最多回显多少条。
// 聊天框刷屏会把有用信息顶掉，而且 SendSysMessage 是逐条发包。
static uint32 const MAX_SHOW = 30;

// ============================================================================
//  .findmodel 实现
// ============================================================================
class modelfind_commandscript : public CommandScript
{
public:
    modelfind_commandscript() : CommandScript("modelfind_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "findmodel", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleFindModel, "" },
            { "fm",        rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleFindModel, "" },
        };
        return commandTable;
    }

    static void SendHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff00ff00[.findmodel 模型搜索]|r  简写 .fm");
        handler->SendSysMessage("  .fm <关键字>        按模型文件名搜（英文）");
        handler->SendSysMessage("  .fm id <displayid>  反查这个ID是什么模型");
        handler->SendSysMessage("  .fm npc <entry>     看这个生物用的模型叫什么");
        handler->SendSysMessage("  .fm sel             看当前选中目标的模型");
        handler->SendSysMessage("  .fm me              看自己现在的模型");
        handler->SendSysMessage("|cffffff00--- 搜出来直接能用 ---|r");
        handler->SendSysMessage("  .fm wolf     ->  列出所有狼模型和它们的 displayid");
        handler->SendSysMessage("  .model id <搜到的displayid>   ->  变过去");
        handler->SendSysMessage("|cffffff00--- 多个词=同时满足，空格/大小写随便 ---|r");
        handler->SendSysMessage("  .fm the lich king   =  .fm lichking  =  .fm king lich");
        handler->SendSysMessage("  .fm night elf female   两个词都要命中才列出");
        handler->SendSysMessage("|cffffff00--- 常用关键字 ---|r");
        handler->SendSysMessage("  wolf bear cat spider dragon murloc gnoll");
        handler->SendSysMessage("  human orc troll scourge felguard infernal");
        handler->SendSysMessage("|cff00ff00 搜出来的是你自己客户端的模型表，网站的号不一定对得上|r");
    }

    // ------------------------------------------------------------------
    //  把一条命中打印出来
    // ------------------------------------------------------------------
    static void PrintHit(ChatHandler* handler, ModelHit const& hit)
    {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "|cff00ff00%u|r  %s  |cff888888(model %u)|r",
                 hit.displayId, hit.shortName.c_str(), hit.modelId);
        handler->SendSysMessage(buf);
    }

    // ------------------------------------------------------------------
    //  详细报告单个 displayid
    // ------------------------------------------------------------------
    static void ReportDisplayId(ChatHandler* handler, uint32 displayId, char const* prefix)
    {
        char buf[512];

        CreatureDisplayInfoEntry const* disp = sCreatureDisplayInfoStore.LookupEntry(displayId);
        if (!disp)
        {
            snprintf(buf, sizeof(buf),
                     "|cffff0000 displayid %u 在 CreatureDisplayInfo.dbc 里不存在|r", displayId);
            handler->SendSysMessage(buf);
            handler->SendSysMessage("|cffffff00 用这个ID换模型会变成隐形或崩客户端|r");
            return;
        }

        CreatureModelDataEntry const* md = sCreatureModelDataStore.LookupEntry(disp->ModelID);

        if (prefix && *prefix)
            handler->SendSysMessage(prefix);

        snprintf(buf, sizeof(buf), "  displayid : |cff00ff00%u|r", displayId);
        handler->SendSysMessage(buf);

        snprintf(buf, sizeof(buf), "  modelid   : %u", disp->ModelID);
        handler->SendSysMessage(buf);

        if (md && md->ModelName && *md->ModelName)
        {
            snprintf(buf, sizeof(buf), "  模型文件  : |cffffff00%s|r", md->ModelName);
            handler->SendSysMessage(buf);
        }
        else
        {
            handler->SendSysMessage("  模型文件  : |cffff0000查不到（CreatureModelData 缺行）|r");
        }

        snprintf(buf, sizeof(buf), "  模型缩放  : %.2f", disp->CreatureModelScale);
        handler->SendSysMessage(buf);

        // ExtendedDisplayInfoID 非 0 = 这是个"玩家型"模型（要靠 Extra 表穿装备）
        if (disp->ExtendedDisplayInfoID)
        {
            snprintf(buf, sizeof(buf),
                     "  |cff00ccff玩家型模型|r (Extra %u) —— 外观由装备决定",
                     disp->ExtendedDisplayInfoID);
            handler->SendSysMessage(buf);
        }

        snprintf(buf, sizeof(buf), "|cffffff00 用法: .model id %u|r", displayId);
        handler->SendSysMessage(buf);
    }

    // ------------------------------------------------------------------
    //  主入口
    // ------------------------------------------------------------------
    static bool HandleFindModel(ChatHandler* handler, char const* args)
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

        // ---------- help ----------
        if (s0 == "help" || tok[0] == "帮助")
        {
            SendHelp(handler);
            return true;
        }

        // ---------- .fm me ----------
        if (s0 == "me" || tok[0] == "自己")
        {
            ReportDisplayId(handler, player->GetDisplayId(), "|cff00ff00[你当前的模型]|r");

            uint32 native = player->GetNativeDisplayId();
            if (native != player->GetDisplayId())
            {
                snprintf(buf, sizeof(buf),
                         "|cff888888 原生模型: %u（.model reset 可复位）|r", native);
                handler->SendSysMessage(buf);
            }
            return true;
        }

        // ---------- .fm sel ----------
        if (s0 == "sel" || s0 == "target" || tok[0] == "选中")
        {
            Unit* target = handler->getSelectedUnit();      // Chat.h:105
            if (!target)
            {
                handler->SendSysMessage("|cffff0000 没有选中目标|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            snprintf(buf, sizeof(buf), "|cff00ff00[%s 的模型]|r", target->GetName().c_str());
            ReportDisplayId(handler, target->GetDisplayId(), buf);

            if (Creature* c = target->ToCreature())
            {
                snprintf(buf, sizeof(buf), "|cff888888 entry: %u|r", c->GetEntry());
                handler->SendSysMessage(buf);
            }
            return true;
        }

        // ---------- .fm id <displayid> ----------
        if (s0 == "id")
        {
            if (tok.size() < 2 || !IsAllDigit(tok[1]))
            {
                handler->SendSysMessage("|cffff0000 用法: .fm id <displayid>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            ReportDisplayId(handler, uint32(atoi(tok[1].c_str())), "|cff00ff00[反查 displayid]|r");
            return true;
        }

        // ---------- .fm npc <entry> ----------
        if (s0 == "npc")
        {
            if (tok.size() < 2 || !IsAllDigit(tok[1]))
            {
                handler->SendSysMessage("|cffff0000 用法: .fm npc <entry>|r");
                handler->SendSysMessage("|cffffff00 entry 用 .lookup creature <名字> 查|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            uint32 entry = uint32(atoi(tok[1].c_str()));
            CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(entry);   // ObjectMgr.h:982
            if (!ct)
            {
                snprintf(buf, sizeof(buf), "|cffff0000 creature_template 里没有 entry %u|r", entry);
                handler->SendSysMessage(buf);
                handler->SetSentErrorMessage(true);
                return false;
            }

            snprintf(buf, sizeof(buf), "|cff00ff00[%s]|r  entry %u", ct->Name.c_str(), entry);
            handler->SendSysMessage(buf);

            // 一个 creature_template 最多挂 4 个模型，随机取一个用。
            // CreatureData.h:358 / Creature.cpp:133 GetFirstValidModelId()
            uint32 const mids[4] = { ct->Modelid1, ct->Modelid2, ct->Modelid3, ct->Modelid4 };
            uint32 shown = 0;
            for (uint32 i = 0; i < 4; ++i)
            {
                if (!mids[i])
                    continue;

                ++shown;
                char const* mn = GetModelNameByDisplayId(mids[i]);
                snprintf(buf, sizeof(buf),
                         "  modelid%u: |cff00ff00%u|r  %s",
                         i + 1, mids[i],
                         mn ? ShortName(mn).c_str() : "|cffff0000(DBC查不到)|r");
                handler->SendSysMessage(buf);
            }

            if (!shown)
            {
                handler->SendSysMessage("|cffff0000 这个生物一个模型都没配（4个槽全是0）|r");
                return true;
            }

            snprintf(buf, sizeof(buf), "|cffffff00 用法: .model npc %u|r", entry);
            handler->SendSysMessage(buf);
            return true;
        }

        // ---------- 关键字搜索 ----------
        // v2 修复：用户实测 `.fm The Lich King` 搜不到。
        //
        // 旧写法把词拼成 "the lich king"（带空格）去匹配，
        // 但路径是 Creature\LichKing\LichKing.mdx，没有空格，必然失败。
        //
        // 新写法：每个词【各自】Squash 后独立匹配，全部命中才算数（AND）。
        // 这样以下写法【全都能搜到】同一个模型：
        //     .fm lichking      .fm The Lich King
        //     .fm lich king     .fm king lich      （顺序无关）
        //     .fm lich_king     .fm LICH-KING
        std::vector<std::string> needles;
        for (std::string const& t : tok)
        {
            std::string sq = Squash(t);
            if (!sq.empty())
                needles.push_back(sq);
        }

        // 整体长度校验：单词太短会刷屏（搜 "a" 能命中几千条）
        size_t totalLen = 0;
        for (std::string const& n : needles)
            totalLen += n.size();

        if (needles.empty() || totalLen < 2)
        {
            handler->SendSysMessage("|cffff0000 关键字至少 2 个字符（英文/数字）|r");
            handler->SendSysMessage("|cffffff00 模型文件名是英文的，中文搜不到|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 拼一份用于回显的原文
        std::string shown;
        for (size_t i = 0; i < tok.size(); ++i)
        {
            if (i)
                shown += " ";
            shown += tok[i];
        }

        // 遍历整张 CreatureDisplayInfo。
        // GetNumRows() 是索引表大小，中间有空洞，LookupEntry 会返回 nullptr。
        // 这是官方 cs_lookup.cpp:109 的标准写法。
        //
        // v2 算法：【最佳匹配】而不是严格 AND。
        //
        // 为什么不用严格 AND —— 单测里实际撞到的坑：
        //   `.fm The Lich King` 里的 "the" 在模型路径里【根本不存在】
        //   （路径是 Creature\LichKing\LichKing.mdx）。
        //   严格 AND 会因为 "the" 一个词没命中就返回 0 条 —— 等于没修。
        //
        // 所以改成两遍扫描：
        //   第一遍：算出"最多能同时命中几个词"(best)，并记录哪些词全表零命中
        //   第二遍：只收命中数 == best 的行
        //
        // 效果：多余的词（the / of / 拼错的词）被自动忽略并如实告知，
        //       有效的词照常起 AND 作用（越具体结果越少）。
        std::vector<ModelHit> hits;
        uint32 total = 0;

        std::vector<bool> everHit(needles.size(), false);
        uint32 best = 0;

        for (uint32 id = 0; id < sCreatureDisplayInfoStore.GetNumRows(); ++id)
        {
            CreatureDisplayInfoEntry const* disp = sCreatureDisplayInfoStore.LookupEntry(id);
            if (!disp)
                continue;

            CreatureModelDataEntry const* md = sCreatureModelDataStore.LookupEntry(disp->ModelID);
            if (!md || !md->ModelName)
                continue;

            std::string hay = Squash(md->ModelName);
            uint32 n = 0;
            for (size_t i = 0; i < needles.size(); ++i)
            {
                if (hay.find(needles[i]) != std::string::npos)
                {
                    ++n;
                    everHit[i] = true;
                }
            }
            if (n > best)
                best = n;
        }

        // 收集全表零命中的词，待会儿告诉用户"这几个词我没理"
        std::vector<std::string> ignored;
        for (size_t i = 0; i < needles.size(); ++i)
            if (!everHit[i])
                ignored.push_back(needles[i]);

        if (best > 0)
        {
            for (uint32 id = 0; id < sCreatureDisplayInfoStore.GetNumRows(); ++id)
            {
                CreatureDisplayInfoEntry const* disp = sCreatureDisplayInfoStore.LookupEntry(id);
                if (!disp)
                    continue;

                CreatureModelDataEntry const* md = sCreatureModelDataStore.LookupEntry(disp->ModelID);
                if (!md || !md->ModelName)
                    continue;

                std::string hay = Squash(md->ModelName);
                uint32 n = 0;
                for (std::string const& nd : needles)
                    if (hay.find(nd) != std::string::npos)
                        ++n;

                if (n != best)
                    continue;

                ++total;
                if (hits.size() < MAX_SHOW)
                {
                    ModelHit h;
                    h.displayId = disp->ID;
                    h.modelId   = disp->ModelID;
                    h.fullName  = md->ModelName;
                    h.shortName = ShortName(md->ModelName);
                    hits.push_back(h);
                }
            }
        }

        if (hits.empty())
        {
            snprintf(buf, sizeof(buf), "|cffff0000 没搜到含 \"%s\" 的模型|r", shown.c_str());
            handler->SendSysMessage(buf);
            handler->SendSysMessage("|cffffff00 关键字要用英文（模型文件名是英文的）|r");
            handler->SendSysMessage("|cffffff00 试试: wolf / bear / spider / dragon / human|r");
            return true;
        }

        snprintf(buf, sizeof(buf),
                 "|cff00ff00[搜到 %u 个含 \"%s\" 的模型]|r", total, shown.c_str());
        handler->SendSysMessage(buf);

        // 如实告知哪些词被忽略了，避免用户以为搜索是按全部词生效的
        if (!ignored.empty())
        {
            std::string ig;
            for (size_t i = 0; i < ignored.size(); ++i)
            {
                if (i)
                    ig += " ";
                ig += ignored[i];
            }
            snprintf(buf, sizeof(buf),
                     "|cff888888 (这些词模型名里没有，已忽略: %s)|r", ig.c_str());
            handler->SendSysMessage(buf);
        }

        for (ModelHit const& h : hits)
            PrintHit(handler, h);

        if (total > MAX_SHOW)
        {
            snprintf(buf, sizeof(buf),
                     "|cffffff00 ... 还有 %u 个没显示，关键字再具体点|r", total - MAX_SHOW);
            handler->SendSysMessage(buf);
        }

        handler->SendSysMessage("|cffffff00 选中一个ID -> .model id <该ID>|r");
        return true;
    }
};

void AddSC_modelfind_commandscript()
{
    new modelfind_commandscript();
}

/* ============================================================================
 *  API 核实记录（全部 grep 自 328950225/TrinityCore-NPCBOT-Eluna-zhCN
 *                分支 NPCBOT-Eluna-zhCN-2026）
 * ============================================================================
 *
 *  src/server/shared/DataStores/DBCfmt.h:42
 *      char constexpr CreatureDisplayInfofmt[] = "nixifxxxxxxxxxxx";
 *  src/server/shared/DataStores/DBCfmt.h:45
 *      char constexpr CreatureModelDatafmt[]   = "nisxfxxxxxxxxxxffxxxxxxxxxxx";
 *          -> 'i' 表示该列被载入，'x' 表示跳过。
 *             CreatureModelData 第3列(s=字符串)就是 ModelName，确实载入了内存。
 *
 *  src/server/shared/DataStores/DBCStructure.h:442-456
 *      struct CreatureDisplayInfoEntry
 *      {
 *          uint32 ID;                       // 0
 *          uint32 ModelID;                  // 1
 *          uint32 ExtendedDisplayInfoID;    // 3
 *          float  CreatureModelScale;       // 4
 *      };
 *          -> struct，默认 public，四个字段可直接读。
 *
 *  src/server/shared/DataStores/DBCStructure.h:496-516
 *      struct CreatureModelDataEntry
 *      {
 *          uint32      ID;              // 0
 *          uint32      Flags;           // 1
 *          char const* ModelName;       // 2   <- 关键：模型文件路径
 *          float       ModelScale;      // 4
 *          float       CollisionHeight; // 15
 *          float       MountHeight;     // 16
 *      };
 *
 *  src/server/game/DataStores/DBCStores.h:110
 *      TC_GAME_API extern DBCStorage<CreatureDisplayInfoEntry> sCreatureDisplayInfoStore;
 *  src/server/game/DataStores/DBCStores.h:113
 *      TC_GAME_API extern DBCStorage<CreatureModelDataEntry>   sCreatureModelDataStore;
 *          -> TC_GAME_API 导出，scripts 模块可以直接用。
 *
 *  src/server/game/DataStores/DBCStores.cpp:298
 *      LOAD_DBC(sCreatureDisplayInfoStore, "CreatureDisplayInfo.dbc");
 *  src/server/game/DataStores/DBCStores.cpp:301
 *      LOAD_DBC(sCreatureModelDataStore,   "CreatureModelData.dbc");
 *  src/server/game/DataStores/DBCStores.cpp:273
 *      std::string dbcPath = dataPath + "dbc/";
 *          -> 服务端启动就从 <DataDir>/dbc/ 全量载入，运行时零 IO。
 *
 *  src/server/shared/DataStores/DBCStore.h:67
 *      T const* LookupEntry(uint32 id) const
 *          { return (id >= _indexTableSize) ? nullptr : _indexTable.AsT[id]; }
 *  src/server/shared/DataStores/DBCStore.h:89
 *      uint32 GetNumRows() const { return _indexTableSize; }
 *          -> public，越界和空洞都返回 nullptr，遍历安全。
 *
 *  官方同款遍历写法（照抄）：
 *  src/server/scripts/Commands/cs_lookup.cpp:109
 *      for (uint32 i = 0; i < sAreaTableStore.GetNumRows(); ++i)
 *
 *  官方同款 ModelName 用法（证明这字段确实是可读字符串）：
 *  src/server/game/Globals/ObjectMgr.cpp:1777
 *      modelInfo.is_trigger = strstr(modelData->ModelName, "InvisibleStalker") != nullptr;
 *          -> 官方自己就在对 ModelName 做子串匹配，本指令是同一套路。
 *
 *  src/server/game/Globals/ObjectMgr.h:982      GetCreatureTemplate(entry)
 *  src/server/game/Entities/Creature/CreatureData.h:296   struct CreatureTemplate（public）
 *  src/server/game/Chat/Chat.h:104/105          getSelectedCreature() / getSelectedUnit()
 *  src/server/game/Accounts/RBAC.h              WORLDTOOLS=71012 为 step21 自建，非官方
 *
 *  性别副作用（调 .model 时要知道，本指令只读不写所以无影响）：
 *  src/server/game/Entities/Unit/Unit.cpp:10938
 *      void Unit::SetDisplayId(uint32 modelId)
 *      {
 *          SetUInt32Value(UNIT_FIELD_DISPLAYID, modelId);
 *          if (CreatureModelInfo const* minfo = sObjectMgr->GetCreatureModelInfo(modelId))
 *              SetGender(Gender(minfo->gender));     // <- 会连带改性别
 *      }
 * ============================================================================
 */
