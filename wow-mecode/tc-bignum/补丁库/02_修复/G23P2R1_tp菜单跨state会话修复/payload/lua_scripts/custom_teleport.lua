-- G23-P2R1 safe Chinese teleport menu.
-- Gossip navigation is deliberately stateless so Eluna multistate dispatch
-- cannot lose an in-memory session between the chat command and menu click.

if type(G23) ~= "table" then
    error("custom_teleport.lua requires extensions/G23Core.ext")
end

local PER_PAGE = 28
local MENU_ID = 60500
local SENDER_CAT = 9101
local SENDER_TELE = 9102
local SENDER_PAGE = 9103
local SENDER_BACK = 9104
local SENDER_CLOSE = 9105
local PAGE_FACTOR = 100000
local MAX_CATEGORY_PAGE = 1000
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

local function say(player, msg)
    player:SendBroadcastMessage(msg)
end

local function rankOf(player)
    local ok, rank = pcall(player.GetGMRank, player)
    return ok and (tonumber(rank) or 0) or 0
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
    return true
end

local function escLike(text)
    text = tostring(text or ""):sub(1, 48)
    text = text:gsub("[%z\1-\31\\]", "")
    return text:gsub("'", "''")
end

local function queryTele(where, limit, offset, label)
    local sql =
        "SELECT t.id,t.name,t.map,t.position_x,t.position_y,t.position_z," ..
        "t.orientation,IFNULL(c.name_cn,''),IFNULL(c.sort,99999) " ..
        "FROM game_tele t LEFT JOIN custom_tele_cn c ON c.tele_id=t.id " ..
        (where or "") .. " ORDER BY IFNULL(c.sort,99999),t.name"
    if limit then
        limit = math.max(1, math.floor(tonumber(limit) or 1))
        offset = math.max(0, math.floor(tonumber(offset) or 0))
        sql = sql .. " LIMIT " .. limit
        if offset > 0 then sql = sql .. " OFFSET " .. offset end
    end

    local q = G23.WorldQuery(sql, label or "teleport:list", false)
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

local function validUInt(value)
    value = tonumber(value)
    if not value or value ~= value then return nil end
    value = math.floor(value)
    if value < 0 or value > 4294967295 then return nil end
    return value
end

local function queryTarget(teleId)
    teleId = validUInt(teleId)
    if not teleId then return nil end
    local list = queryTele("WHERE t.id=" .. teleId, 1, 0, "teleport:target")
    return list[1]
end

local function categoryWhere(index)
    local category = CATEGORIES[index]
    if not category then return nil end
    if category.maps then
        local ids = {}
        for mapId in pairs(category.maps) do ids[#ids + 1] = tostring(mapId) end
        table.sort(ids)
        return "WHERE t.map IN (" .. table.concat(ids, ",") .. ")"
    end

    local excluded = {}
    for categoryIndex = 1, #CATEGORIES - 1 do
        for mapId in pairs(CATEGORIES[categoryIndex].maps) do
            excluded[#excluded + 1] = tostring(mapId)
        end
    end
    table.sort(excluded)
    return "WHERE t.map NOT IN (" .. table.concat(excluded, ",") .. ")"
end

local function addTarget(player, target)
    local label = target.name
    if target.cn ~= "" and target.cn ~= target.en then
        label = target.cn .. "  |cff666666" .. target.en .. "|r"
    end
    player:GossipMenuAddItem(2, label, SENDER_TELE, target.id)
end

local function encodePage(categoryIndex, page)
    return categoryIndex * PAGE_FACTOR + page
end

local function decodePage(value)
    value = validUInt(value)
    if not value then return nil, nil end
    local categoryIndex = math.floor(value / PAGE_FACTOR)
    local page = value % PAGE_FACTOR
    if not CATEGORIES[categoryIndex] or page > MAX_CATEGORY_PAGE then return nil, nil end
    return categoryIndex, page
end

local function showCategories(player)
    local ok, reason = checkSafety(player)
    if not ok then say(player, reason); return end
    player:GossipClearMenu()
    for index, category in ipairs(CATEGORIES) do
        player:GossipMenuAddItem(2, "【" .. category.name .. "】", SENDER_CAT, index)
    end
    player:GossipMenuAddItem(0, "|cff888888也可使用 .tp <关键词> 搜索|r", SENDER_CLOSE, 0)
    player:GossipMenuAddItem(0, "|cff888888关闭|r", SENDER_CLOSE, 0)
    player:GossipSendMenu(100, player, MENU_ID)
end

local function showCategoryPage(player, categoryIndex, page)
    local ok, reason = checkSafety(player)
    if not ok then say(player, reason); return end
    categoryIndex = validUInt(categoryIndex)
    page = validUInt(page)
    local category = categoryIndex and CATEGORIES[categoryIndex] or nil
    if not category or not page or page > MAX_CATEGORY_PAGE then
        say(player, "|cffff8800传送菜单参数无效，请重新输入 .tp。|r")
        return
    end
    if not category.maps and rankOf(player) < INSTANCE_MIN_GM_RANK then
        say(player, "|cffff8800副本与特殊地图目标需要GM等级 " .. INSTANCE_MIN_GM_RANK .. "。|r")
        return
    end

    local list = queryTele(categoryWhere(categoryIndex), PER_PAGE + 1,
        page * PER_PAGE, "teleport:category")
    local hasNext = #list > PER_PAGE
    if hasNext then list[#list] = nil end

    player:GossipClearMenu()
    if #list == 0 then
        player:GossipMenuAddItem(0, "|cffff8800此页没有传送点|r", SENDER_BACK, 0)
    else
        for _, target in ipairs(list) do addTarget(player, target) end
    end
    if page > 0 then
        player:GossipMenuAddItem(0, "|cff00ccff<< 上一页|r",
            SENDER_PAGE, encodePage(categoryIndex, page - 1))
    end
    if hasNext then
        player:GossipMenuAddItem(0, "|cff00ccff下一页 >>|r",
            SENDER_PAGE, encodePage(categoryIndex, page + 1))
    end
    player:GossipMenuAddItem(0, "|cff888888返回分类|r", SENDER_BACK, 0)
    player:GossipMenuAddItem(0, "|cff888888关闭|r", SENDER_CLOSE, 0)
    player:GossipSendMenu(100, player, MENU_ID)
end

local function showSearchResults(player, keyword, list)
    local truncated = #list > PER_PAGE
    if truncated then list[#list] = nil end
    player:GossipClearMenu()
    for _, target in ipairs(list) do addTarget(player, target) end
    if truncated then
        player:GossipMenuAddItem(0,
            "|cffff8800结果较多，请用更具体的关键词继续搜索|r", SENDER_CLOSE, 0)
    end
    player:GossipMenuAddItem(0, "|cff888888返回分类|r", SENDER_BACK, 0)
    player:GossipMenuAddItem(0, "|cff888888关闭|r", SENDER_CLOSE, 0)
    say(player, "|cff00ccff找到多个「" .. keyword .. "」匹配地点，请选择。|r")
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
        "WHERE c.name_cn LIKE '%" .. escaped .. "%' OR t.name LIKE '%" .. escaped .. "%'",
        PER_PAGE + 1, 0, "teleport:search")
    if #list == 0 then
        say(player, "|cffff8800没有找到「" .. keyword .. "」相关的传送点|r")
        return
    end
    if #list == 1 then safeTeleport(player, list[1]); return end
    showSearchResults(player, keyword, list)
end

local function onGossip(event, player, object, sender, intid, code)
    if sender ~= SENDER_CAT and sender ~= SENDER_TELE and sender ~= SENDER_PAGE and
        sender ~= SENDER_BACK and sender ~= SENDER_CLOSE then return end

    player:GossipComplete()
    local ok, reason = checkSafety(player)
    if not ok then say(player, reason); return end

    if sender == SENDER_CAT then
        showCategoryPage(player, intid, 0)
        return
    end
    if sender == SENDER_TELE then
        local target = queryTarget(intid)
        if not target then
            say(player, "|cffff8800传送点不存在或数据库查询失败，请重新输入 .tp。|r")
            return
        end
        safeTeleport(player, target)
        return
    end
    if sender == SENDER_PAGE then
        local categoryIndex, page = decodePage(intid)
        if not categoryIndex then
            say(player, "|cffff8800传送菜单参数无效，请重新输入 .tp。|r")
            return
        end
        showCategoryPage(player, categoryIndex, page)
        return
    end
    if sender == SENDER_BACK then
        showCategories(player)
        return
    end
end

G23.RegisterCommand("tp", "安全中文传送：.tp 或 .tp <关键词>", "Lua便捷功能", 0,
    function(player, args)
        doSearch(player, args)
        return false
    end)
RegisterPlayerGossipEvent(MENU_ID, 2, onGossip)

print("[G23-P2R1] custom_teleport.lua 已加载 -- 无跨state会话依赖，保留全部传送安全门")
