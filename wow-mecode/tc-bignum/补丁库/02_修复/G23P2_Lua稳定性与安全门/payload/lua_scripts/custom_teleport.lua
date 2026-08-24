-- G23-P2 safe Chinese teleport menu.
-- All command, gossip and final Teleport paths re-check safety gates.

if type(G23) ~= "table" then
    error("custom_teleport.lua requires extensions/G23Core.ext")
end

local PER_PAGE = 28
local MENU_ID = 60500
local SENDER_CAT, SENDER_TELE, SENDER_NAV = 9101, 9102, 9109
local NAV_PREV, NAV_NEXT, NAV_BACK, NAV_CLOSE = 1, 2, 3, 4
local SESSION_TTL = 300
local INSTANCE_MIN_GM_RANK = 2
local SAFETY_BYPASS_GM_RANK = 3
local FLIGHT_MOTION_TYPE = 7

local OPEN_WORLD_MAPS = { [0]=true, [1]=true, [530]=true, [571]=true }
local CATEGORIES = {
    { name = "东部王国", maps = { [0]=true } },
    { name = "卡利姆多", maps = { [1]=true } },
    { name = "外域", maps = { [530]=true } },
    { name = "诺森德", maps = { [571]=true } },
    { name = "副本与团本（GM）", maps = nil },
}

local sessions = {}
local lastSweep = 0

local function say(player, msg)
    player:SendBroadcastMessage(msg)
end

local function rankOf(player)
    local ok, rank = pcall(player.GetGMRank, player)
    return ok and (tonumber(rank) or 0) or 0
end

local function sweepSessions(force)
    local stamp = G23.Now()
    if not force and stamp > 0 and stamp - lastSweep < 60 then return end
    lastSweep = stamp
    for guid, session in pairs(sessions) do
        if stamp > 0 and stamp - (session.touched or 0) > SESSION_TTL then
            sessions[guid] = nil
        end
    end
end

local function sessionOf(player, create)
    sweepSessions(false)
    local guid = player:GetGUIDLow()
    local stamp = G23.Now()
    local session = sessions[guid]
    if session and stamp > 0 and stamp - (session.touched or 0) > SESSION_TTL then
        sessions[guid] = nil
        session = nil
    end
    if not session and create ~= false then
        session = { cat=0, page=0, list={}, touched=stamp }
        sessions[guid] = session
    end
    if session then session.touched = stamp end
    return session
end

local function deny(reason)
    return false, "|cffff8800传送已阻止：" .. reason .. "|r"
end

local function checkSafety(player, destination)
    local rank = rankOf(player)
    local bypass = rank >= SAFETY_BYPASS_GM_RANK

    if not player:IsAlive() or player:IsDead() then return deny("死亡或灵魂状态不可传送") end
    if not bypass and player:IsInCombat() then return deny("战斗中不可传送") end
    if not bypass and player:IsPvPFlagged() then return deny("PVP标记期间不可传送") end
    if not bypass and (player:InBattleground() or player:InArena() or player:InBattlegroundQueue()) then
        return deny("战场、竞技场或排队状态不可传送")
    end
    if not bypass and (player:IsOnVehicle() or player:GetVehicle() ~= nil) then return deny("载具中不可传送") end
    if not bypass and player:IsMounted() then return deny("请先下坐骑再传送") end
    if not bypass and (player:IsFlying() or player:GetMovementType() == FLIGHT_MOTION_TYPE) then
        return deny("飞行或航线状态不可传送")
    end

    local map = player:GetMap()
    if not map then return deny("当前地图状态不可用") end
    if not bypass and (map:IsBattleground() or map:IsArena()) then return deny("PVP地图不可传送") end
    if not bypass and (map:IsDungeon() or map:IsRaid()) then return deny("副本或团本内不可使用便捷传送") end

    if destination then
        local mapId = tonumber(destination.map)
        if not mapId or mapId < 0 then return deny("目标地图无效") end
        if not OPEN_WORLD_MAPS[mapId] and rank < INSTANCE_MIN_GM_RANK then
            return deny("副本、团本及特殊地图目标需要GM等级 " .. INSTANCE_MIN_GM_RANK)
        end
        local coords = { destination.x, destination.y, destination.z, destination.o }
        for _, value in ipairs(coords) do
            value = tonumber(value)
            if not value or value ~= value or math.abs(value) > 1000000 then
                return deny("目标坐标无效")
            end
        end
    end
    return true
end

local function safeTeleport(player, target)
    local ok, reason = checkSafety(player, target)
    if not ok then say(player, reason); return false end
    local called, moved = pcall(player.Teleport, player, target.map, target.x, target.y, target.z, target.o)
    if not called or not moved then
        say(player, "|cffff0000传送失败，目标地图或坐标未被核心接受。|r")
        G23.Log("ERROR", "teleport:core:" .. tostring(target.id),
            "Teleport返回失败，tele_id=" .. tostring(target.id), 60)
        return false
    end
    say(player, "|cff00ff00已传送到|r " .. target.name)
    sessions[player:GetGUIDLow()] = nil
    return true
end

local function escLike(text)
    text = tostring(text or ""):sub(1, 48)
    text = text:gsub("[%z\1-\31\\]", "")
    return text:gsub("'", "''")
end

local function queryTele(where, limit)
    local sql =
        "SELECT t.id,t.name,t.map,t.position_x,t.position_y,t.position_z," ..
        "t.orientation,IFNULL(c.name_cn,''),IFNULL(c.sort,99999) " ..
        "FROM game_tele t LEFT JOIN custom_tele_cn c ON c.tele_id=t.id " ..
        (where or "") .. " ORDER BY IFNULL(c.sort,99999),t.name"
    if limit then sql = sql .. " LIMIT " .. tonumber(limit) end

    local q = G23.WorldQuery(sql, "teleport:list", false)
    local out = {}
    if not q then return out end
    repeat
        local cn, en = q:GetString(7), q:GetString(1)
        out[#out + 1] = {
            id=q:GetUInt32(0), en=en, cn=cn, name=(cn ~= "" and cn or en),
            map=q:GetUInt32(2), x=q:GetFloat(3), y=q:GetFloat(4),
            z=q:GetFloat(5), o=q:GetFloat(6),
        }
    until not q:NextRow()
    return out
end

local function showCategories(player)
    local ok, reason = checkSafety(player)
    if not ok then say(player, reason); return end
    local session = sessionOf(player, true)
    session.cat, session.page, session.list = 0, 0, {}
    player:GossipClearMenu()
    for index, category in ipairs(CATEGORIES) do
        player:GossipMenuAddItem(2, "【" .. category.name .. "】", SENDER_CAT, index)
    end
    player:GossipMenuAddItem(0, "|cff888888也可使用 .tp <关键词> 搜索|r", SENDER_NAV, NAV_CLOSE)
    player:GossipMenuAddItem(0, "|cff888888关闭|r", SENDER_NAV, NAV_CLOSE)
    player:GossipSendMenu(100, player, MENU_ID)
end

local function showList(player)
    local session = sessionOf(player, false)
    if not session then
        say(player, "|cffff8800传送会话已超时，请重新输入 .tp。|r")
        player:GossipComplete()
        return
    end
    local total = #session.list
    local maxPage = total > 0 and math.floor((total - 1) / PER_PAGE) or 0
    if session.page > maxPage then session.page = maxPage end
    local first = session.page * PER_PAGE + 1
    local last = math.min(first + PER_PAGE - 1, total)

    player:GossipClearMenu()
    for index = first, last do
        local target = session.list[index]
        local label = target.name
        if target.cn ~= "" and target.cn ~= target.en then
            label = target.cn .. "  |cff666666" .. target.en .. "|r"
        end
        player:GossipMenuAddItem(2, label, SENDER_TELE, index)
    end
    if session.page > 0 then
        player:GossipMenuAddItem(0, "|cff00ccff<< 上一页|r", SENDER_NAV, NAV_PREV)
    end
    if session.page < maxPage then
        player:GossipMenuAddItem(0, "|cff00ccff下一页 >> (" ..
            (session.page + 1) .. "/" .. (maxPage + 1) .. ")|r", SENDER_NAV, NAV_NEXT)
    end
    player:GossipMenuAddItem(0, "|cff888888返回分类|r", SENDER_NAV, NAV_BACK)
    player:GossipMenuAddItem(0, "|cff888888关闭|r", SENDER_NAV, NAV_CLOSE)
    player:GossipSendMenu(100, player, MENU_ID)
end

local function doSearch(player, keyword)
    local ok, reason = checkSafety(player)
    if not ok then say(player, reason); return end
    keyword = tostring(keyword or ""):gsub("^%s+", ""):gsub("%s+$", "")
    if keyword == "" then showCategories(player); return end
    if keyword == "home" then
        say(player, "回炉石点请使用炉石或原版 .tele home。")
        return
    end
    if keyword == "back" then
        say(player, "返回上一位置请使用受原版权限控制的 .recall。")
        return
    end

    local escaped = escLike(keyword)
    if escaped == "" then say(player, "|cffff8800搜索词无效。|r"); return end
    local list = queryTele(
        "WHERE c.name_cn LIKE '%" .. escaped .. "%' OR t.name LIKE '%" .. escaped .. "%'", 200)
    if #list == 0 then
        say(player, "|cffff8800没有找到「" .. keyword .. "」相关的传送点|r")
        return
    end
    if #list == 1 then safeTeleport(player, list[1]); return end

    local session = sessionOf(player, true)
    session.list, session.page, session.cat = list, 0, -1
    say(player, "|cff00ccff找到 " .. #list .. " 个匹配地点|r")
    showList(player)
end

local function onGossip(event, player, object, sender, intid, code)
    if sender ~= SENDER_CAT and sender ~= SENDER_TELE and sender ~= SENDER_NAV then return end
    local session = sessionOf(player, false)
    if not session then
        player:GossipComplete()
        say(player, "|cffff8800传送会话已超时，请重新输入 .tp。|r")
        return
    end

    local ok, reason = checkSafety(player)
    if not ok then player:GossipComplete(); say(player, reason); return end

    if sender == SENDER_CAT then
        local category = CATEGORIES[intid]
        if not category then player:GossipComplete(); return end
        if not category.maps and rankOf(player) < INSTANCE_MIN_GM_RANK then
            player:GossipComplete()
            say(player, "|cffff8800副本与特殊地图目标需要GM等级 " .. INSTANCE_MIN_GM_RANK .. "。|r")
            return
        end
        local where
        if category.maps then
            local ids = {}
            for mapId in pairs(category.maps) do ids[#ids + 1] = tostring(mapId) end
            where = "WHERE t.map IN (" .. table.concat(ids, ",") .. ")"
        else
            local excluded = {}
            for index = 1, #CATEGORIES - 1 do
                for mapId in pairs(CATEGORIES[index].maps) do excluded[#excluded + 1] = tostring(mapId) end
            end
            where = "WHERE t.map NOT IN (" .. table.concat(excluded, ",") .. ")"
        end
        session.cat, session.page, session.list = intid, 0, queryTele(where, 500)
        player:GossipComplete()
        showList(player)
        return
    end

    if sender == SENDER_TELE then
        local target = session.list[intid]
        player:GossipComplete()
        if target then safeTeleport(player, target) end
        return
    end

    if intid == NAV_PREV then
        if session.page > 0 then session.page = session.page - 1 end
        player:GossipComplete(); showList(player)
    elseif intid == NAV_NEXT then
        session.page = session.page + 1
        player:GossipComplete(); showList(player)
    elseif intid == NAV_BACK then
        player:GossipComplete(); showCategories(player)
    else
        player:GossipComplete(); sessions[player:GetGUIDLow()] = nil
    end
end

local function onLogout(event, player)
    sessions[player:GetGUIDLow()] = nil
    sweepSessions(true)
end

G23.RegisterCommand("tp", "安全中文传送：.tp 或 .tp <关键词>", "Lua便捷功能", 0,
    function(player, args)
        doSearch(player, args)
        return false
    end)
RegisterPlayerEvent(4, onLogout)
RegisterPlayerGossipEvent(MENU_ID, 2, onGossip)

print("[G23-P2] custom_teleport.lua 已加载 -- 战斗/PVP/飞行/载具/死亡/地图/权限安全门")
