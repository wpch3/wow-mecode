/*
 * ============================================================================
 *  剧情表情 —— cs_emote.cpp   (step29)
 * ============================================================================
 *
 *   .emote <表情>                   对选中目标做表情
 *   .emote r <半径> <表情>          对周围所有 NPC
 *   .emote entry <ID> <表情>        对指定 entry 的所有 NPC
 *   .emote once <表情> [目标语法]   强制一次性播放
 *   .emote state <表情> [目标语法]  强制持续状态
 *   .emote clear [目标语法]         清除状态表情
 *   .emote save                     把当前状态写库（持久化）
 *   .emote list [关键词]            查表情表（174 个记不住，用这个）
 *   .emote me <表情>                自己做表情
 *
 *   表情参数三种写法都行：
 *       .emote 跳舞        中文别名
 *       .emote dance       英文别名
 *       .emote 10          原始数字
 *
 * ----------------------------------------------------------------------------
 *  为什么要做这个
 *
 *  .scene（step28）能存位置和状态，但场景是"静止"的。
 *  加上表情，NPC 才会跪、会哭、会跳舞、会干活 —— 场景才活起来。
 *
 *  官方已有 .npc playemote，但有三个硬伤：
 *    1. 只能 SetEmoteState，做不了一次性动作（挥手、鞠躬）
 *    2. 只能对【选中的单个】目标，做群像场景要点一百次
 *    3. 必须填数字，174 个表情谁记得住
 *
 *  这三条正是本指令要解决的。
 *
 * ----------------------------------------------------------------------------
 *  【核心】两种表情的本质区别（已查实现，不是猜的）
 *
 *  Unit.cpp:1712  HandleEmoteCommand —— 只发一个网络包，不改任何字段
 *      void Unit::HandleEmoteCommand(Emote emoteId)
 *      {
 *          WorldPackets::Chat::Emote packet;
 *          packet.Guid = GetGUID();
 *          packet.EmoteID = emoteId;
 *          SendMessageToSet(packet.Write(), true);
 *      }
 *    -> 播完就没，重进视野看不到。适合 EMOTE_ONESHOT_*
 *
 *  Unit.h:968     SetEmoteState —— 写 UNIT_NPC_EMOTESTATE 字段
 *      void SetEmoteState(Emote emote) { SetUInt32Value(UNIT_NPC_EMOTESTATE, emote); }
 *    -> 一直保持，新玩家进视野也看得到。适合 EMOTE_STATE_*
 *    -> .scene(step28) 存的就是这个字段
 *
 *  所以本指令【两种都支持】，并根据表情类型自动选。
 *
 * ----------------------------------------------------------------------------
 *  已核实 API（全 public，逐个查过访问权限）
 *
 *   Unit.h:1037          void HandleEmoteCommand(Emote emoteId)   [public，811行起public段]
 *   Unit.h:967           Emote GetEmoteState() const
 *   Unit.h:968           void SetEmoteState(Emote emote)
 *   Chat.h:104           Creature* getSelectedCreature()
 *   Creature.h:394       bool IsNPCBotOrPet() const
 *   Creature.h:98        ObjectGuid::LowType GetSpawnId() const
 *   SharedDefines.h:1998 enum Emote : uint32          (到 2175 行，共 174 个)
 *   UpdateFields.h:140   UNIT_NPC_EMOTESTATE
 *   GridNotifiers.h:1088 AnyUnitInObjectRangeCheck
 *   GridNotifiers.h:475  CreatureListSearcher
 *
 *  官方参考实现：
 *   cs_npc.cpp:660       HandleNpcPlayEmoteCommand（只有 SetEmoteState）
 *   cs_npc.cpp:915-917   .npc say 按标点自动配表情（? ! 分别对应）
 *   cs_modify.cpp:830    玩家自己 SetEmoteState
 *
 * ----------------------------------------------------------------------------
 *  注册语法：用【旧式】std::vector<ChatCommand>
 *
 *  仓库里两种语法并存，旧式虽标 deprecated 但官方 cs_wp.cpp / cs_ticket.cpp
 *  等多个文件仍在用。选旧式的三个理由：
 *    1. 和 .nst / .scene / .dummy 保持一致
 *    2. 旧式接 char const* args，方便自己解析 "r 30 跳舞 save" 这种混合参数
 *       （新式是强类型自动解析，做不了）
 *    3. 不引入无谓的编译风险
 *
 *  权限沿用 RBAC_PERM_COMMAND_WORLDTOOLS，不用动 RBAC 表。
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DatabaseEnv.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "SharedDefines.h"
#include "Unit.h"
#include "UnitDefines.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

// ============================================================================
//  表情表
//
//  只收录剧情真正用得上的。174 个全塞进来反而找不到。
//  isState = true  -> 用 SetEmoteState（持续）
//  isState = false -> 用 HandleEmoteCommand（一次性）
//
//  数值全部来自 SharedDefines.h:1998-2175，逐个核对过。
// ============================================================================
// ----------------------------------------------------------------------------
//  【v2 修正 2026-08-01】坐/睡/跪/假死 不是表情，是「站姿」
//
//  用户实测：除了跳舞，大部分表情用了就是站着不动。
//
//  查源码找到根因 —— 这两个是【完全不同的字段】：
//
//    表情  SetEmoteState   -> UNIT_NPC_EMOTESTATE       174个值
//    站姿  SetStandState   -> UNIT_FIELD_BYTES_1 的一个字节  只有0-9共10个值
//                             (Unit.cpp:10894)
//
//  UnitStandStateType 全部取值（UnitDefines.h:32-46）：
//    0 站 / 1 坐 / 2 坐椅 / 3 睡 / 4 矮椅 / 5 中椅 / 6 高椅 / 7 死 / 8 跪 / 9 潜
//
//  官方脚本【全部】用 SetStandState 做坐跪睡，没有一处用 EMOTE_STATE_SIT：
//    zone_terokkar_forest.cpp:84      me->SetStandState(UNIT_STAND_STATE_SIT)
//    zone_borean_tundra.cpp:492       talbot->SetStandState(UNIT_STAND_STATE_KNEEL)
//    boss_algalon_the_observer.cpp:669  me->SetStandState(UNIT_STAND_STATE_KNEEL)
//    boss_nefarian.cpp:196            me->SetStandState(UNIT_STAND_STATE_SIT_HIGH_CHAIR)
//
//  所以 EMOTE_STATE_SIT(13) / SLEEP(12) / KNEEL(68) 写进 emote 字段
//  对客户端【无效】—— 这就是"站着不动"的原因。
//
//  跳舞能用，是因为 EMOTE_STATE_DANCE(10) 确实是纯表情，没有对应站姿。
//
//  修法：加 stand 字段。stand >= 0 的走 SetStandState，-1 的走表情。
// ----------------------------------------------------------------------------
struct EmoteDef
{
    uint32      id;      // 表情ID（stand>=0 时此值仅作参考/兼容）
    bool        isState; // true=持续 false=一次性
    int         stand;   // >=0 -> 用 SetStandState(stand)；-1 -> 用表情字段
    char const* cn;      // 中文别名
    char const* en;      // 英文别名
    char const* desc;    // 说明
};

static EmoteDef const g_emotes[] =
{
    // ---------- 【站姿】走 SetStandState，这些才是真正能"坐下跪下"的 ----------
    //     id   持续  stand  中文        英文          说明
    {   0, true,   0,  "站立",     "stand",     "恢复站立（清除坐跪睡）" },
    {   0, true,   1,  "坐下",     "sit",       "坐在地上" },
    {   0, true,   2,  "坐椅",     "sitchair",  "坐椅子（普通）" },
    {   0, true,   3,  "睡觉",     "sleep",     "躺地上睡" },
    {   0, true,   4,  "坐矮椅",   "sitlow",    "坐矮椅" },
    {   0, true,   5,  "坐中椅",   "sitmid",    "坐中等椅" },
    {   0, true,   6,  "坐高椅",   "sithigh",   "坐高椅（王座）" },
    {   0, true,   7,  "假死",     "dead",      "装死（不是真死）" },
    {   0, true,   8,  "跪下",     "kneel",     "跪着" },
    {   0, true,   9,  "潜水",     "submerged", "潜入地下/水下" },

    // ---------- 【持续表情】走 SetEmoteState，没有对应站姿 ----------
    {  10, true,  -1,  "跳舞",     "dance",     "持续跳舞" },
    {  30, true,  -1,  "无",       "none",      "清除表情（保留站姿）" },
    {  64, true,  -1,  "昏迷",     "stun",      "昏迷" },
    {  69, true,  -1,  "使用",     "use",       "站立使用物品" },
    { 173, true,  -1,  "干活",     "work",      "通用劳作" },
    { 233, true,  -1,  "挖矿",     "mining",    "挖矿动作" },
    { 234, true,  -1,  "砍柴",     "chopwood",  "砍柴动作" },
    { 253, true,  -1,  "鼓掌持续", "applaudst", "持续鼓掌" },
    { 313, true,  -1,  "稍息",     "atease",    "放松站姿" },
    { 333, true,  -1,  "持单手",   "ready1h",   "单手武器戒备" },
    { 353, true,  -1,  "施法跪",   "kneelcast", "跪姿施法（表情版）" },
    { 375, true,  -1,  "持双手",   "ready2h",   "双手武器戒备" },
    { 376, true,  -1,  "持弓",     "readybow",  "弓箭戒备" },
    { 378, true,  -1,  "说话持续", "talkst",    "持续说话动作" },
    { 379, true,  -1,  "钓鱼",     "fishing",   "钓鱼" },
    { 382, true,  -1,  "旋风斩",   "whirlwind", "旋风斩姿态" },
    { 383, true,  -1,  "溺水",     "drowned",   "溺水状态" },
    {  27, true,  -1,  "空手戒备", "readyun",   "空手戒备" },
    {  28, true,  -1,  "收剑干活", "worksh",    "收起武器干活" },
    {  29, true,  -1,  "指向持续", "pointst",   "持续指向" },
    {  93, true,  -1,  "昏迷不收", "stunns",    "昏迷（不收武器）" },
    { 193, true,  -1,  "预施法",   "precast",   "施法前摇" },
    { 214, true,  -1,  "持枪",     "readyrifle","枪械戒备" },

    // ---------- 【一次性动作】走 HandleEmoteCommand ----------
    {   1, false, -1,  "说话",     "talk",      "说话一次" },
    {   2, false, -1,  "鞠躬",     "bow",       "鞠躬" },
    {   3, false, -1,  "挥手",     "wave",      "挥手" },
    {   4, false, -1,  "欢呼",     "cheer",     "欢呼" },
    {   5, false, -1,  "惊叹",     "exclaim",   "感叹号动作" },
    {   6, false, -1,  "疑问",     "question",  "问号动作" },
    {   7, false, -1,  "吃",       "eat",       "吃东西" },
    {  11, false, -1,  "大笑",     "laugh",     "大笑" },
    {  14, false, -1,  "无礼",     "rude",      "鄙视手势" },
    {  15, false, -1,  "咆哮",     "roar",      "咆哮" },
    {  16, false, -1,  "下跪一次", "kneelonce", "跪一下就起（动作）" },
    {  17, false, -1,  "亲吻",     "kiss",      "飞吻" },
    {  18, false, -1,  "哭泣",     "cry",       "哭" },
    {  19, false, -1,  "学鸡叫",   "chicken",   "学鸡叫" },
    {  20, false, -1,  "乞求",     "beg",       "乞讨" },
    {  21, false, -1,  "鼓掌",     "applaud",   "鼓掌一次" },
    {  22, false, -1,  "呼喊",     "shout",     "大喊" },
    {  23, false, -1,  "秀肌肉",   "flex",      "展示肌肉" },
    {  24, false, -1,  "害羞",     "shy",       "害羞" },
    {  25, false, -1,  "指向",     "point",     "指一下" },
    {  33, false, -1,  "受伤",     "wound",     "受伤动作" },
    {  34, false, -1,  "重伤",     "woundcrit", "重伤动作" },
};

static size_t const g_emoteCount = sizeof(g_emotes) / sizeof(g_emotes[0]);

// ============================================================================
//  小工具
// ============================================================================

// 按空白切分
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

static bool IsAllDigit(std::string const& s)
{
    if (s.empty())
        return false;
    for (char c : s)
        if (c < '0' || c > '9')
            return false;
    return true;
}

// 转小写（只处理 ASCII，中文不受影响）
static std::string Lower(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}

// 查表：返回下标，找不到返回 -1
static int FindEmote(std::string const& key)
{
    std::string k = Lower(key);

    // 先按别名精确匹配
    for (size_t i = 0; i < g_emoteCount; ++i)
    {
        if (k == Lower(g_emotes[i].cn) || k == Lower(g_emotes[i].en))
            return int(i);
    }

    // 再按数字匹配表内条目
    // 注意：站姿条目 id 全是 0，不能参与数字匹配，否则输 "0" 会命中第一条站姿
    if (IsAllDigit(key))
    {
        uint32 v = uint32(atoi(key.c_str()));
        for (size_t i = 0; i < g_emoteCount; ++i)
            if (g_emotes[i].stand < 0 && g_emotes[i].id == v)
                return int(i);
    }
    return -1;
}

// 判断一个裸数字是不是 STATE 类
static bool GuessIsState(uint32 id)
{
    for (size_t i = 0; i < g_emoteCount; ++i)
        if (g_emotes[i].stand < 0 && g_emotes[i].id == id)
            return g_emotes[i].isState;

    // 表外的数字：保守起见当一次性处理，
    // 用户要持续效果可以用 .emote state <数字> 强制
    return false;
}

// 判断一个 emote 值能否写进 UNIT_NPC_EMOTESTATE 当持续状态
//
//  【v3 新增】3.3.5 客户端读 UNIT_NPC_EMOTESTATE 时只认 STATE 类表情。
//  把 EMOTE_ONESHOT_* 写进去客户端【直接忽略】，表现为"设了没反应"。
//  （用户实测 .emote state 挥手 无动作即此原因）
//
//  表内条目直接查 isState；表外裸数字用 SharedDefines.h:1998-2175
//  实际枚举里 STATE 类的取值分布来判断。
static bool IsStateEmote(uint32 id)
{
    for (size_t i = 0; i < g_emoteCount; ++i)
        if (g_emotes[i].stand < 0 && g_emotes[i].id == id)
            return g_emotes[i].isState;

    // 表外数字：这些是 SharedDefines.h 里 EMOTE_STATE_* 的实际取值
    static uint32 const stateIds[] = {
        10, 12, 13, 26, 27, 28, 29, 30, 64, 65, 68, 69, 93, 133, 173, 193,
        214, 233, 234, 253, 273, 293, 313, 333, 353, 354, 373, 374, 375, 376,
        377, 378, 379, 380, 381, 382, 383, 384, 385, 386, 387, 388, 389, 390,
        391, 392, 393, 394, 395, 396, 397, 398, 399, 400, 401, 402, 403, 404,
        405, 406, 407, 408, 409, 410, 411, 412, 415, 416, 417, 418, 419, 420,
        421, 422, 423, 424, 425, 426, 427, 428
    };
    for (uint32 v : stateIds)
        if (v == id)
            return true;
    return false;
}

// 收集半径内的 Creature（沿用 .nst step26 的写法）
static void CollectNear(Player* player, float radius, std::vector<Creature*>& out)
{
    std::list<Creature*> found;
    Trinity::AnyUnitInObjectRangeCheck check(player, radius);
    Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck>
        searcher(player, found, check);
    Cell::VisitAllObjects(player, searcher, radius);

    for (Creature* c : found)
    {
        if (!c || !c->IsInWorld())
            continue;
        // NPCBot 绝不误伤（Creature.h:394）
        if (c->IsNPCBotOrPet())
            continue;
        if (c->IsPet() || c->IsTotem())
            continue;
        out.push_back(c);
    }
}

// 按 entry 收集（半径给大一点，覆盖当前地图可见范围）
static void CollectByEntry(Player* player, uint32 entry, float radius,
                           std::vector<Creature*>& out)
{
    std::vector<Creature*> all;
    CollectNear(player, radius, all);
    for (Creature* c : all)
        if (c->GetEntry() == entry)
            out.push_back(c);
}

// 真正施放
//   stand >= 0 -> SetStandState（坐/跪/睡/假死，Unit.cpp:10894）
//   stand <  0 -> 走表情字段
static void ApplyEmote(Unit* u, uint32 emoteId, bool asState, int stand)
{
    if (!u)
        return;

    if (stand >= 0)
    {
        // 站姿。设站姿前先清掉持续表情，否则两者会打架
        // （比如先跳舞再坐下，客户端可能仍在跳）
        u->SetEmoteState(EMOTE_ONESHOT_NONE);                       // 0
        u->SetStandState(UnitStandStateType(stand));                // Unit.cpp:10894
        return;
    }

    if (asState)
        u->SetEmoteState(Emote(emoteId));       // Unit.h:968
    else
        u->HandleEmoteCommand(Emote(emoteId));  // Unit.h:1037
}

// 写库持久化
//   creature_addon 有【两列】要写（ObjectMgr.cpp:1291 的 SELECT 可见）：
//     StandState  <- 坐/跪/睡/假死
//     emote       <- 持续表情
//   一次性表情不写（本来就不持久）
//
// ---------------------------------------------------------------------------
//  【v3 关键修正】只写数据库【不够】，必须同步内存缓存
//
//  用户实测：save 之后重启/重生，NPC 又变回原样。
//
//  查源码找到根因（三级链路）：
//    Creature.cpp:2705  LoadCreaturesAddon()
//        -> Creature.cpp:2692  GetCreatureAddon()
//        -> ObjectMgr.cpp:1477 sObjectMgr->GetCreatureAddon(m_spawnId)
//        -> ObjectMgr.cpp:1479 查的是【内存容器】_creatureAddonStore
//
//  也就是说 NPC 重生时读的是【内存】，不是数据库。
//  只写库 -> 内存还是旧值 -> 重生即回滚。
//
//  _creatureAddonStore 是 private（ObjectMgr.h:1692），没有公开写入口，
//  但 ObjectMgr.h:1170 的 LoadCreatureAddons() 是 public（947行起public段），
//  它会整表重读刷新内存。所以：写库后调它一次即可，
//  且【不用改任何核心头文件】。
// ---------------------------------------------------------------------------
static void SaveEmoteToDB(Creature* c, uint32 emoteId, int stand)
{
    if (!c)
        return;

    ObjectGuid::LowType spawnId = c->GetSpawnId();   // Creature.h:98
    if (!spawnId)
        return;   // 临时召唤物没有 spawnId，存不了

    // 两列的当前值：没被本次修改的那一列保持现状，避免互相覆盖
    uint32 curStand = uint32(c->GetStandState());    // Unit.h:1002
    uint32 curEmote = uint32(c->GetEmoteState());    // Unit.h:967

    if (stand >= 0)
        curStand = uint32(stand);
    else
        curEmote = emoteId;

    // creature_addon 可能还没有这一行，用 INSERT ... ON DUPLICATE
    //
    // ---------------------------------------------------------------------
    //  【v5 崩溃真因】占位符必须是 {} 不是 %u
    //
    //  用户实测：改了 v4 之后 .emote 跪下 save 【仍然崩】。
    //  说明 v4 归因（addon 缓存 rehash）不是真因。
    //
    //  查签名找到真相：
    //    DatabaseWorkerPool.h:99
    //      void DirectPExecute(Trinity::FormatString<Args...> sql, Args&&... args)
    //          -> Trinity::StringFormat(sql, args...)
    //
    //  本仓库的 DirectPExecute 走的是 【fmt 库】，占位符是 {}，不是 printf 的 %u。
    //  写 %u 时 fmt 一个参数都消费不掉，多余实参直接抛异常 -> 服务端崩溃。
    //
    //  这条早已记录在案（DatabaseWorkerPool.h:99/122 用 {} 不是 %u），
    //  是我写的时候没查。凡本仓库 DirectPExecute/PQuery 一律用 {}。
    // ---------------------------------------------------------------------
    WorldDatabase.DirectPExecute(
        "INSERT INTO `creature_addon` (`guid`, `StandState`, `emote`) "
        "VALUES ({}, {}, {}) "
        "ON DUPLICATE KEY UPDATE `StandState` = {}, `emote` = {}",
        spawnId, curStand, curEmote, curStand, curEmote);
}

// ---------------------------------------------------------------------------
//  【v4 崩溃修复】撤销 v3 的 RefreshAddonCache()
//
//  用户实测：.emote 跪下 save -> 服务端闪退。
//
//  根因（源码实证）：
//    ObjectMgr.cpp:1313   CreatureAddon& creatureAddon = _creatureAddonStore[guid];
//                         ^^ 拿的是 unordered_map 元素的【引用】
//    ObjectMgr.cpp:1477   GetCreatureAddon() 返回 &(itr->second)
//                         ^^ 活着的 NPC 持有的是【指向容器元素的指针】
//
//  LoadCreatureAddons() 在运行时被调用时，会往 map 里插入元素触发 rehash，
//  【所有已存在的引用/指针全部失效】-> NPC 持有的 CreatureAddon* 变悬空指针
//  -> 下次访问即崩溃。
//
//  这个函数设计上【只在启动时调用一次】，那时没有任何 NPC 持有指针，所以安全。
//  运行时调它 = 在拆正在走的桥。
//
//  结论：不刷缓存。写库即可，用 .respawn / 重启生效。
//  代价是不立即可见，但这和官方 .npc set model 等指令行为一致，可接受。
//  ——【绝对不要】在运行时调 sObjectMgr->LoadCreatureAddons()
// ---------------------------------------------------------------------------


// ============================================================================
//  指令实现
// ============================================================================
class emote_commandscript : public CommandScript
{
public:
    emote_commandscript() : CommandScript("emote_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "emote", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleEmote, "" },
        };
        return commandTable;
    }

    static void SendHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff00ff00[.emote 剧情表情]|r");
        handler->SendSysMessage("  .emote <表情>              对选中目标");
        handler->SendSysMessage("  .emote r <半径> <表情>     对周围所有NPC");
        handler->SendSysMessage("  .emote entry <ID> <表情>   对指定entry的NPC");
        handler->SendSysMessage("  .emote once <表情> [...]   强制一次性播放");
        handler->SendSysMessage("  .emote state <表情> [...]  强制持续状态");
        handler->SendSysMessage("  .emote clear [...]         复位（站立+清表情）");
        handler->SendSysMessage("  .emote save                当前状态写库");
        handler->SendSysMessage("  .emote list [关键词]       查表情表");
        handler->SendSysMessage("  .emote me <表情>           自己做表情");
        handler->SendSysMessage("|cffffff00 表情可填: 中文名 / 英文名 / 数字|r");
        handler->SendSysMessage("|cffffff00 坐/跪/睡/假死 属【站姿】，只能用别名不能用数字|r");
        handler->SendSysMessage("|cffffff00 例: .emote 跳舞  |  .emote r 30 跪下|r");
    }

    // ------------------------------------------------------------------
    //  .emote list [关键词]
    // ------------------------------------------------------------------
    static bool DoList(ChatHandler* handler, std::string const& filter)
    {
        char buf[512];
        int shown = 0;

        // 第一类：站姿。这些才是真能"坐下跪下"的，排最前
        handler->SendSysMessage("|cff00ff00=== 站姿（坐/跪/睡，最常用）===|r");
        for (size_t i = 0; i < g_emoteCount; ++i)
        {
            if (g_emotes[i].stand < 0)
                continue;
            if (!filter.empty() &&
                std::string(g_emotes[i].cn).find(filter) == std::string::npos &&
                Lower(g_emotes[i].en).find(Lower(filter)) == std::string::npos)
                continue;

            snprintf(buf, sizeof(buf), "  %-10s %-11s  站姿%d  %s",
                     g_emotes[i].cn, g_emotes[i].en, g_emotes[i].stand, g_emotes[i].desc);
            handler->SendSysMessage(buf);
            ++shown;
        }

        handler->SendSysMessage("|cff00ff00=== 持续表情（会一直保持）===|r");
        for (size_t i = 0; i < g_emoteCount; ++i)
        {
            if (g_emotes[i].stand >= 0 || !g_emotes[i].isState)
                continue;
            if (!filter.empty() &&
                std::string(g_emotes[i].cn).find(filter) == std::string::npos &&
                Lower(g_emotes[i].en).find(Lower(filter)) == std::string::npos)
                continue;

            snprintf(buf, sizeof(buf), "  %-10s %-11s %4u  %s",
                     g_emotes[i].cn, g_emotes[i].en, g_emotes[i].id, g_emotes[i].desc);
            handler->SendSysMessage(buf);
            ++shown;
        }

        handler->SendSysMessage("|cff00ff00=== 一次性动作（播完就没）===|r");
        for (size_t i = 0; i < g_emoteCount; ++i)
        {
            if (g_emotes[i].stand >= 0 || g_emotes[i].isState)
                continue;
            if (!filter.empty() &&
                std::string(g_emotes[i].cn).find(filter) == std::string::npos &&
                Lower(g_emotes[i].en).find(Lower(filter)) == std::string::npos)
                continue;

            snprintf(buf, sizeof(buf), "  %-10s %-11s %4u  %s",
                     g_emotes[i].cn, g_emotes[i].en, g_emotes[i].id, g_emotes[i].desc);
            handler->SendSysMessage(buf);
            ++shown;
        }

        if (!shown)
            handler->SendSysMessage("|cffff0000 没找到匹配的表情|r");
        else
        {
            snprintf(buf, sizeof(buf), "|cffffff00 共 %d 条。表里没有的数字也能直接用。|r", shown);
            handler->SendSysMessage(buf);
        }
        return true;
    }

    // ------------------------------------------------------------------
    //  主入口
    // ------------------------------------------------------------------
    static bool HandleEmote(ChatHandler* handler, char const* args)
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

        std::string sub = Lower(tok[0]);
        char buf[512];

        // ---------- list ----------
        if (sub == "list" || sub == "?" || tok[0] == "列表")
        {
            std::string filter = tok.size() >= 2 ? tok[1] : "";
            return DoList(handler, filter);
        }

        // ---------- save ----------
        if (sub == "save" || tok[0] == "保存")
        {
            Creature* c = handler->getSelectedCreature();   // Chat.h:104
            if (!c)
            {
                handler->SendSysMessage("|cffff0000 先选中一个 NPC|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            // 裸 save：把当前【表情 + 站姿】原样存下来
            // 传 stand=-1 让 SaveEmoteToDB 内部各取当前值，两列都不丢
            uint32 curE = uint32(c->GetEmoteState());       // Unit.h:967
            uint32 curS = uint32(c->GetStandState());       // Unit.h:1002
            if (!c->GetSpawnId())
            {
                handler->SendSysMessage(
                    "|cffff0000 这是临时召唤物（无 spawnId），无法持久化|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            SaveEmoteToDB(c, curE, -1);
            // v4: 【不要】在这里刷 addon 缓存，会让活着的 NPC 持有悬空指针而崩溃
            snprintf(buf, sizeof(buf),
                     "|cff00ff00 已写库: guid=%u emote=%u 站姿=%u|r",
                     uint32(c->GetSpawnId()), curE, curS);
            handler->SendSysMessage(buf);
            handler->SendSysMessage(
                "|cffffff00 用 .respawn 或重启服务端后生效|r");
            return true;
        }

        // ---------- 解析：强制模式 ----------
        bool forceState = false;
        bool forceOnce  = false;
        size_t idx = 0;

        if (sub == "once" || tok[0] == "一次")
        {
            forceOnce = true;
            idx = 1;
        }
        else if (sub == "state" || tok[0] == "持续")
        {
            forceState = true;
            idx = 1;
        }

        if (idx >= tok.size() && !forceOnce && !forceState)
        {
            SendHelp(handler);
            return true;
        }

        // ---------- clear（只识别【紧跟在指令后】的写法：.emote clear）----------
        //
        //  【v3 修正】原来在这里就把 clear 吃掉，导致 ".emote r 30 clear" 里的
        //  clear 排在 "r 30" 后面，走到目标选择之后才轮到它，
        //  却被当成【表情名】去查表 -> 报"认不出表情"。
        //  现在这里只处理前置写法，后置写法在【取表情】那一步统一识别。
        //
        bool isClear = false;
        if (idx < tok.size())
        {
            std::string c0 = Lower(tok[idx]);
            if (c0 == "clear" || tok[idx] == "清除")
            {
                isClear = true;
                ++idx;
            }
        }

        // ---------- me：自己做表情 ----------
        if (idx < tok.size() && (Lower(tok[idx]) == "me" || tok[idx] == "自己"))
        {
            ++idx;
            if (idx >= tok.size())
            {
                handler->SendSysMessage("|cffff0000 用法: .emote me <表情>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            uint32 eid;
            bool   st;
            int    sd;
            if (!ResolveEmote(handler, tok[idx], eid, st, sd))
                return false;
            // 强制模式只对表情有效，站姿没有"一次性"概念
            if (sd < 0)
            {
                if (forceOnce)  st = false;
                if (forceState) st = true;
            }

            ApplyEmote(player, eid, st, sd);
            if (sd >= 0)
                snprintf(buf, sizeof(buf), "|cff00ff00 自己: %s [站姿%d]|r",
                         tok[idx].c_str(), sd);
            else
                snprintf(buf, sizeof(buf), "|cff00ff00 自己: %s (%u) %s|r",
                         tok[idx].c_str(), eid, st ? "[持续]" : "[一次性]");
            handler->SendSysMessage(buf);
            return true;
        }

        // ---------- 目标选择：r <半径> / entry <ID> / 选中 ----------
        std::vector<Creature*> targets;
        std::string scope = "选中";

        if (idx < tok.size() && (Lower(tok[idx]) == "r" || tok[idx] == "范围"))
        {
            if (idx + 1 >= tok.size())
            {
                handler->SendSysMessage("|cffff0000 用法: .emote r <半径> <表情>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            float radius = float(atof(tok[idx + 1].c_str()));
            if (radius <= 0.0f || radius > 500.0f)
            {
                handler->SendSysMessage("|cffff0000 半径需在 0-500 之间|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            CollectNear(player, radius, targets);
            snprintf(buf, sizeof(buf), "半径%.0f", radius);
            scope = buf;
            idx += 2;
        }
        else if (idx < tok.size() && (Lower(tok[idx]) == "entry" || tok[idx] == "编号"))
        {
            if (idx + 1 >= tok.size() || !IsAllDigit(tok[idx + 1]))
            {
                handler->SendSysMessage("|cffff0000 用法: .emote entry <ID> <表情>|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            uint32 entry = uint32(atoi(tok[idx + 1].c_str()));
            CollectByEntry(player, entry, 200.0f, targets);
            snprintf(buf, sizeof(buf), "entry=%u", entry);
            scope = buf;
            idx += 2;
        }
        else
        {
            Creature* c = handler->getSelectedCreature();
            if (!c)
            {
                handler->SendSysMessage("|cffff0000 没有选中 NPC。用 .emote me <表情> 对自己，"
                                        "或 .emote r <半径> <表情> 对周围|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            targets.push_back(c);
        }

        if (targets.empty())
        {
            handler->SendSysMessage("|cffff0000 范围内没有符合条件的 NPC|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ---------- 取表情 ----------
        uint32 emoteId = 0;
        bool   asState = false;
        int    standSt = -1;

        if (isClear)
        {
            // clear 要把【两个字段】都复位：表情清空 + 站姿回站立
            emoteId = 0;           // EMOTE_ONESHOT_NONE
            asState = true;
            standSt = 0;           // UNIT_STAND_STATE_STAND
        }
        else
        {
            if (idx >= tok.size())
            {
                handler->SendSysMessage("|cffff0000 缺少表情参数。.emote list 查看可用表情|r");
                handler->SetSentErrorMessage(true);
                return false;
            }

            // 【v3 修正】后置 clear：.emote r 30 clear / .emote entry 123 clear
            // 走到这里 clear 位于表情参数位，必须在查表【之前】拦下，
            // 否则会被当成表情名 -> "认不出表情"（用户实测到的 bug 1）
            std::string cw = Lower(tok[idx]);
            if (cw == "clear" || tok[idx] == "清除")
            {
                isClear = true;
                emoteId = 0;       // EMOTE_ONESHOT_NONE
                asState = true;
                standSt = 0;       // UNIT_STAND_STATE_STAND
                ++idx;
            }
            else
            {
                if (!ResolveEmote(handler, tok[idx], emoteId, asState, standSt))
                    return false;
                ++idx;
            }
        }

        // 强制模式只对表情有效，站姿没有"一次性"概念
        if (standSt < 0)
        {
            if (forceOnce)  asState = false;

            if (forceState)
            {
                // 【v3 修正】ONESHOT 类【不能】塞进 UNIT_NPC_EMOTESTATE。
                //
                //  用户实测：.emote state 挥手 -> 完全没动作。
                //  原因：3.3.5 客户端读 UNIT_NPC_EMOTESTATE 时只认 STATE 类表情，
                //  写入 EMOTE_ONESHOT_WAVE(3) 这类值客户端直接忽略，
                //  表现为"设了但没反应"。这是引擎限制，不是权限或数值问题。
                //
                //  与其静默失败，不如明确拦下并给出可行替代。
                if (!IsStateEmote(emoteId))
                {
                    snprintf(buf, sizeof(buf),
                             "|cffff0000 \"%s\"(%u) 是一次性动作，无法设为持续状态。|r",
                             tok[idx > 0 ? idx - 1 : 0].c_str(), emoteId);
                    handler->SendSysMessage(buf);
                    handler->SendSysMessage(
                        "|cffffff00 客户端只认 STATE 类表情做持续状态。|r");
                    handler->SendSysMessage(
                        "|cffffff00 想要持续效果请用 .emote list 里【持续表情】"
                        "或【站姿】那两段的条目。|r");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                asState = true;
            }
        }

        // ---------- 末尾可跟 save ----------
        bool doSave = false;
        if (idx < tok.size() && (Lower(tok[idx]) == "save" || tok[idx] == "保存"))
            doSave = true;

        // 站姿和持续表情都能持久化；只有一次性动作不行
        bool canSave = (standSt >= 0) || asState;

        // ---------- 施放 ----------
        int okCount = 0;
        int savedCount = 0;
        for (Creature* c : targets)
        {
            ApplyEmote(c, emoteId, asState, standSt);
            ++okCount;

            if (doSave && canSave)
            {
                SaveEmoteToDB(c, emoteId, standSt);
                ++savedCount;
            }
        }

        // v4: 【不要】刷 addon 缓存（见 SaveEmoteToDB 上方的崩溃说明）

        if (standSt >= 0)
            snprintf(buf, sizeof(buf), "|cff00ff00 %s: %d 个目标 -> 站姿%d|r",
                     scope.c_str(), okCount, standSt);
        else
            snprintf(buf, sizeof(buf), "|cff00ff00 %s: %d 个目标 -> %u %s|r",
                     scope.c_str(), okCount, emoteId,
                     asState ? "[持续]" : "[一次性]");
        handler->SendSysMessage(buf);

        if (doSave)
        {
            if (!canSave)
                handler->SendSysMessage("|cffffff00 提示: 一次性动作无法持久化，save 已忽略|r");
            else
            {
                snprintf(buf, sizeof(buf), "|cff00ff00 已写库 %d 条|r", savedCount);
                handler->SendSysMessage(buf);
                handler->SendSysMessage(
                    "|cffffff00 用 .respawn 或重启服务端后生效|r");
            }
        }
        return true;
    }

    // 解析表情参数：中文名 / 英文名 / 数字
    static bool ResolveEmote(ChatHandler* handler, std::string const& key,
                             uint32& outId, bool& outIsState, int& outStand)
    {
        int i = FindEmote(key);
        if (i >= 0)
        {
            outId      = g_emotes[i].id;
            outIsState = g_emotes[i].isState;
            outStand   = g_emotes[i].stand;
            return true;
        }

        // 表里没有，但如果是纯数字就放行（174 个不可能全收录）
        if (IsAllDigit(key))
        {
            uint32 v = uint32(atoi(key.c_str()));
            if (v > 500)
            {
                handler->SendSysMessage("|cffff0000 表情ID超出范围(0-500)|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
            outId      = v;
            outIsState = GuessIsState(v);
            outStand   = -1;   // 裸数字一律当表情，站姿只能用别名
            return true;
        }

        char buf[256];
        snprintf(buf, sizeof(buf),
                 "|cffff0000 认不出表情 \"%s\"。用 .emote list 查看|r", key.c_str());
        handler->SendSysMessage(buf);
        handler->SetSentErrorMessage(true);
        return false;
    }
};

void AddSC_emote_commandscript()
{
    new emote_commandscript();
}
