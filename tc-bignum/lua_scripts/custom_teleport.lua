--[[
================================================================================
    传送系统  custom_teleport.lua
================================================================================

    用你数据库里现成的 game_tele 表（TrinityCore 自带 1000+ 个传送点）
    做一个带中文搜索和分类的传送系统。

    ── 用法 ──────────────────────────────────────────────────────────────
      .tp                  打开传送菜单（可点击，自动分页）
      .tp <关键词>         搜索传送点，例如 .tp 暴风
      .tp home             提示如何回炉石点
      .tp back             提示如何返回上一位置（用原版 .recall）

    ── 特点 ──────────────────────────────────────────────────────────────
      · 不用编译，改完 .reload eluna 立刻生效
      · 中文/英文都能搜
      · 按大陆/副本自动分类
      · Gossip 严守 32 条上限，超出自动分页

    ── 文件名规则（重要）─────────────────────────────────────────────────
      Eluna 会把文件名开头的数字当成 mapId（ElunaLoader.cpp:225）。
      所以文件名【不能以数字开头】，用 custom_ 前缀。

    放置：D:\TC-Build\bin\RelWithDebInfo\lua_scripts\custom_teleport.lua
================================================================================
]]

local PLAYER_EVENT_ON_COMMAND      = 42
local PLAYER_EVENT_ON_GOSSIP_SELECT = 61   -- 见文件末尾说明

-- Gossip 硬上限 32（GossipDef.cpp:42 有 ASSERT，超了崩服）
local PER_PAGE  = 28      -- 28 内容 + 最多 4 导航 = 32
local MENU_ID   = 60500

-- 【重要】套装系统(cs_gearset.cpp)的 OnGossipSelect 只按 sender 过滤，
-- 它占用 sender 1~11 且不检查 menuId。所以这里必须用高位 sender 避开，
-- 否则点传送菜单会跳到套装系统去。
local SENDER_CAT  = 9101
local SENDER_TELE = 9102
local SENDER_NAV  = 9109

local NAV_PREV, NAV_NEXT, NAV_BACK, NAV_CLOSE = 1, 2, 3, 4

-- ---------------------------------------------------------------
-- 分类定义：按 mapId 归类
-- ---------------------------------------------------------------
local CATEGORIES = {
    { name = "东部王国",   maps = { [0]=true } },
    { name = "卡利姆多",   maps = { [1]=true } },
    { name = "外域",       maps = { [530]=true } },
    { name = "诺森德",     maps = { [571]=true } },
    { name = "副本与团本", maps = nil },   -- nil = 其余全部归这
}

local function CatOf(mapId)
    for i, c in ipairs(CATEGORIES) do
        if c.maps and c.maps[mapId] then
            return i
        end
    end
    return #CATEGORIES   -- 最后一个 = 副本与团本
end

-- ---------------------------------------------------------------
-- 会话
-- ---------------------------------------------------------------
local sess = {}

local function S(player)
    local g = player:GetGUIDLow()
    if not sess[g] then
        sess[g] = { cat = 0, page = 0, list = {} }
    end
    return sess[g]
end

local function say(player, msg)
    player:SendBroadcastMessage(msg)
end

-- ---------------------------------------------------------------
-- 查询传送点
-- ---------------------------------------------------------------
-- 查询传送点
--   LEFT JOIN 中文名表：有中文就显示中文，没有就退回英文
--   排序：有中文的排前面（按 sort），没中文的按英文名排后面
local function QueryTele(where, limit)
    local sql =
        "SELECT t.id, t.name, t.map, t.position_x, t.position_y, t.position_z, " ..
        "t.orientation, IFNULL(c.name_cn,''), IFNULL(c.sort, 99999) " ..
        "FROM game_tele t " ..
        "LEFT JOIN custom_tele_cn c ON c.tele_id = t.id " ..
        (where or "") ..
        " ORDER BY IFNULL(c.sort,99999), t.name"
    if limit then
        sql = sql .. " LIMIT " .. limit
    end

    local q = WorldDBQuery(sql)
    local out = {}
    if not q then
        return out
    end
    repeat
        local cn = q:GetString(7)
        local en = q:GetString(1)
        out[#out + 1] = {
            id   = q:GetUInt32(0),
            en   = en,
            cn   = cn,
            -- 显示名：有中文用中文（后面小字标英文），没有就用英文
            name = (cn ~= "" and cn or en),
            map  = q:GetUInt32(2),
            x    = q:GetFloat(3),
            y    = q:GetFloat(4),
            z    = q:GetFloat(5),
            o    = q:GetFloat(6),
        }
    until not q:NextRow()
    return out
end

-- SQL 转义（防止名字里有单引号出问题）
local function esc(s)
    return (tostring(s):gsub("'", "''"))
end

-- ---------------------------------------------------------------
-- 分类菜单
-- ---------------------------------------------------------------
local function ShowCategories(player)
    local s = S(player)
    s.cat  = 0
    s.page = 0

    player:GossipClearMenu()

    for i, c in ipairs(CATEGORIES) do
        player:GossipMenuAddItem(2, "【" .. c.name .. "】", SENDER_CAT, i)
    end

    player:GossipMenuAddItem(0,
        "|cff888888提示：也可以用 .tp <关键词> 直接搜索|r",
        SENDER_NAV, NAV_CLOSE)
    player:GossipMenuAddItem(0, "|cff888888关闭|r", SENDER_NAV, NAV_CLOSE)

    player:GossipSendMenu(100, player, MENU_ID)
end

-- ---------------------------------------------------------------
-- 传送点列表（带分页）
-- ---------------------------------------------------------------
local function ShowList(player)
    local s = S(player)
    local total = #s.list
    local maxPg = 0
    if total > 0 then
        maxPg = math.floor((total - 1) / PER_PAGE)
    end
    if s.page > maxPg then
        s.page = maxPg
    end

    local first = s.page * PER_PAGE + 1
    local last  = math.min(first + PER_PAGE - 1, total)

    player:GossipClearMenu()

    for i = first, last do
        local t = s.list[i]
        local label = t.name
        -- 有中文名时，把英文名用灰色小字附在后面，方便对照
        if t.cn ~= "" and t.cn ~= t.en then
            label = t.cn .. "  |cff666666" .. t.en .. "|r"
        end
        player:GossipMenuAddItem(2, label, SENDER_TELE, i)
    end

    -- 导航最多 4 条，28 + 4 = 32 正好卡住上限
    if s.page > 0 then
        player:GossipMenuAddItem(0, "|cff00ccff<< 上一页|r", SENDER_NAV, NAV_PREV)
    end
    if s.page < maxPg then
        player:GossipMenuAddItem(0, "|cff00ccff下一页 >> (" ..
            (s.page + 1) .. "/" .. (maxPg + 1) .. ")|r", SENDER_NAV, NAV_NEXT)
    end
    player:GossipMenuAddItem(0, "|cff888888返回分类|r", SENDER_NAV, NAV_BACK)
    player:GossipMenuAddItem(0, "|cff888888关闭|r", SENDER_NAV, NAV_CLOSE)

    player:GossipSendMenu(100, player, MENU_ID)
end

-- ---------------------------------------------------------------
-- 搜索
-- ---------------------------------------------------------------
local function DoSearch(player, kw)
    -- 中英双向搜索：中文名 或 英文名 命中都算
    local k = esc(kw)
    local list = QueryTele(
        "WHERE c.name_cn LIKE '%" .. k .. "%' OR t.name LIKE '%" .. k .. "%'", 200)

    if #list == 0 then
        say(player, "|cffff8800没有找到「" .. kw .. "」相关的传送点|r")
        say(player, "试试 |cffffff00.tp|r 浏览分类菜单")
        return
    end

    -- 只有一个结果就直接传，不用再点
    if #list == 1 then
        local t = list[1]
        say(player, "|cff00ff00传送到|r " .. t.name)
        player:Teleport(t.map, t.x, t.y, t.z, t.o)
        return
    end

    local s = S(player)
    s.list = list
    s.page = 0
    s.cat  = -1
    say(player, "|cff00ccff找到 " .. #list .. " 个匹配「" .. kw .. "」的地点|r")
    ShowList(player)
end

-- ---------------------------------------------------------------
-- 命令入口
-- ---------------------------------------------------------------
local function OnCommand(event, player, command)
    local cmd = tostring(command or "")

    -- 只处理 tp 开头的
    local head = cmd:match("^(%S+)")
    if head ~= "tp" then
        return
    end

    local arg = cmd:match("^%S+%s+(.+)$")

    if not arg or arg == "" then
        ShowCategories(player)
        return false
    end

    arg = arg:gsub("^%s+", ""):gsub("%s+$", "")

    -- 注意：这版 Eluna 没有读取炉石点的 API（只有 SetBindPoint 能写不能读），
    -- 所以 home / back 直接引导用原版命令，不自己实现
    if arg == "home" then
        say(player, "回炉石点请用原版命令：|cffffff00.tele home|r 或直接使用炉石")
        return false
    end

    if arg == "back" then
        say(player, "返回上一个位置请用原版命令：|cffffff00.recall|r")
        return false
    end

    DoSearch(player, arg)
    return false   -- 拦截命令，避免提示"命令不存在"
end

-- ---------------------------------------------------------------
-- Gossip 回调
-- ---------------------------------------------------------------
-- 【真实签名】对照 Eluna 源码 hooks/GossipHooks.cpp:81-88 的实际推参：
--     HookPush(pPlayer);   -- 参数2 = player
--     HookPush(pPlayer);   -- 参数3 = 又是 player（源码注释：just not to mess up the amount of args）
--     HookPush(sender);    -- 参数4 = sender
--     HookPush(action);    -- 参数5 = intid
--     HookPush(code);      -- 参数6 = code
--
-- 注意：文档里写的第7个 menu_id 【实际没有推送】，永远是 nil。
--       所以不能用 menu_id 做过滤，改用 sender 区间判断。
--       （这也是为什么 sender 必须选一个不会撞车的高位区间）
local function OnGossip(event, player, object, sender, intid, code)
    -- 用 sender 区间过滤本系统的菜单（menu_id 拿不到）
    if sender ~= SENDER_CAT and sender ~= SENDER_TELE and sender ~= SENDER_NAV then
        return
    end

    local s = S(player)

    if sender == SENDER_CAT then
        local c = CATEGORIES[intid]
        if not c then
            player:GossipComplete()
            return
        end
        s.cat  = intid
        s.page = 0

        -- 组装 WHERE 条件
        local where
        if c.maps then
            local ids = {}
            for m, _ in pairs(c.maps) do
                ids[#ids + 1] = tostring(m)
            end
            where = "WHERE t.map IN (" .. table.concat(ids, ",") .. ")"
        else
            -- 副本与团本 = 排除前面几个大陆
            local ex = {}
            for i = 1, #CATEGORIES - 1 do
                if CATEGORIES[i].maps then
                    for m, _ in pairs(CATEGORIES[i].maps) do
                        ex[#ex + 1] = tostring(m)
                    end
                end
            end
            where = "WHERE t.map NOT IN (" .. table.concat(ex, ",") .. ")"
        end

        s.list = QueryTele(where, 500)
        player:GossipComplete()
        ShowList(player)
        return
    end

    if sender == SENDER_TELE then
        local t = s.list[intid]
        player:GossipComplete()
        if t then
            say(player, "|cff00ff00传送到|r " .. t.name)
            player:Teleport(t.map, t.x, t.y, t.z, t.o)
        end
        return
    end

    if sender == SENDER_NAV then
        if intid == NAV_PREV then
            if s.page > 0 then s.page = s.page - 1 end
            player:GossipComplete()
            ShowList(player)
        elseif intid == NAV_NEXT then
            s.page = s.page + 1
            player:GossipComplete()
            ShowList(player)
        elseif intid == NAV_BACK then
            player:GossipComplete()
            ShowCategories(player)
        else
            player:GossipComplete()
        end
        return
    end
end

-- 登出清会话
local function OnLogout(event, player)
    sess[player:GetGUIDLow()] = nil
end

RegisterPlayerEvent(PLAYER_EVENT_ON_COMMAND, OnCommand)
RegisterPlayerEvent(4, OnLogout)   -- 4 = ON_LOGOUT
RegisterPlayerGossipEvent(MENU_ID, 2, OnGossip)   -- 2 = GOSSIP_EVENT_ON_SELECT

print("[Eluna] custom_teleport.lua 已加载 -- 输入 .tp 打开传送菜单")
