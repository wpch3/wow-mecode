-- G23-P3A unified server assistant.
-- All database reads are on-demand; native TrinityCore `.server *` commands
-- not owned by this assistant explicitly pass through to the core.

if type(G23) ~= "table" then
    error("custom_server_assistant.lua requires extensions/G23Core.ext")
end

local PAGE_SIZE = 12
local CUSTOM = {
    [""] = true, help = true, menu = true, status = true, lua = true,
    commands = true, daily = true, health = true, gmhelp = true, tp = true,
}

local function say(player, text)
    player:SendBroadcastMessage(text)
end

local function line(player)
    say(player, "|cff00ccff================================================|r")
end

local function words(text)
    local first, rest = tostring(text or ""):match("^(%S*)%s*(.-)%s*$")
    return (first or ""):lower(), rest or ""
end

local function showMain(player)
    line(player)
    say(player, "|cffffcc00        艾泽拉斯新纪元 · 服务器助手|r")
    line(player)
    say(player, "  |cffffff00.server status|r    Lua版本、state与已注册命令")
    say(player, "  |cffffff00.server commands|r  当前可用自定义指令分页")
    say(player, "  |cffffff00.server daily|r     今日奖励领取状态")
    say(player, "  |cffffff00.server tp|r        安全传送说明")
    say(player, "  |cffffff00.server gmhelp|r    GM完整指令助手说明")
    if G23.GetRank(player) >= 2 then
        say(player, "  |cffffff00.server health|r    GM只读Lua/数据库健康页")
    end
    line(player)
    say(player, "|cff888888原生.server info/restart/shutdown等命令保持核心处理。|r")
end

local function showStatus(player)
    local commands = G23.GetHelpEntries(G23.GetRank(player))
    line(player)
    say(player, "|cffffcc00服务器功能状态|r")
    say(player, "G23版本：|cff00ff00" .. tostring(G23.VERSION) .. "|r")
    say(player, "Lua state：map=" .. tostring(GetStateMapId()) ..
        " instance=" .. tostring(GetStateInstanceId()))
    say(player, "Eluna模式：" .. (IsCompatibilityMode() and "compatibility" or "multistate"))
    say(player, "当前权限可见自定义指令：" .. #commands)
    say(player, "传送菜单：P2R1无状态分页；每日奖励：数据库原子领取闸")
    line(player)
end

local function showCommands(player, pageText)
    local list = G23.GetHelpEntries(G23.GetRank(player))
    local maxPage = math.max(1, math.ceil(#list / PAGE_SIZE))
    local page = math.floor(tonumber(pageText) or 1)
    if page < 1 then page = 1 end
    if page > maxPage then page = maxPage end
    local first = (page - 1) * PAGE_SIZE + 1
    local last = math.min(first + PAGE_SIZE - 1, #list)

    line(player)
    say(player, "|cffffcc00自定义指令 " .. page .. "/" .. maxPage .. "|r")
    for index = first, last do
        local entry = list[index]
        say(player, "|cffffff00." .. entry.name .. "|r  " .. entry.description)
    end
    if page < maxPage then
        say(player, "下一页：|cffffff00.server commands " .. (page + 1) .. "|r")
    end
    line(player)
end

local function showDaily(player)
    local guid = tonumber(player:GetGUIDLow()) or 0
    if guid <= 0 then
        say(player, "|cffff0000无法读取角色GUID。|r")
        return
    end
    local q = G23.CharQuery(
        "SELECT status,DATE_FORMAT(claim_date,'%Y-%m-%d'),streak,total_days " ..
        "FROM characters.custom_daily_reward_claim WHERE guid=" .. math.floor(guid) ..
        " AND claim_date=CURDATE() LIMIT 1",
        "server:daily", false)
    if not q then
        say(player, "|cffffff00今天没有claim记录，或P2每日奖励表尚不可用。|r")
        return
    end
    local status = q:GetString(0)
    local color = status == "granted" and "|cff00ff00" or "|cffffff00"
    say(player, "今日奖励：" .. color .. status .. "|r  日期=" .. q:GetString(1) ..
        " 连续=" .. q:GetUInt32(2) .. " 累计=" .. q:GetUInt32(3))
    if status == "pending" then
        say(player, "|cffff8800pending用于防止重复发奖；不要手工删除，请用.luadiag诊断。|r")
    end
end

local function available(value)
    return value and "|cff00ff00PASS|r" or "|cffff0000MISSING|r"
end

local function showHealth(player)
    if G23.GetRank(player) < 2 then
        say(player, "|cffff0000权限不足：.server health需要GM等级2。|r")
        return
    end
    line(player)
    say(player, "|cffffcc00G23-P3A只读健康页|r")
    for _, entry in ipairs({
        {"RegisterPlayerEvent", RegisterPlayerEvent},
        {"RegisterPlayerGossipEvent", RegisterPlayerGossipEvent},
        {"CharDBQuery", CharDBQuery}, {"WorldDBQuery", WorldDBQuery},
        {"SendMail", SendMail}, {"CreateLuaEvent", CreateLuaEvent},
    }) do
        say(player, "API " .. entry[1] .. "：" .. available(type(entry[2]) == "function"))
    end
    local daily = G23.CharQuery(
        "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='characters' " ..
        "AND table_name IN ('custom_daily_reward','custom_daily_reward_claim')",
        "server:health:daily", true)
    if daily then say(player, "每日奖励表：" .. daily:GetUInt32(0) .. "/2") end
    local pending = G23.CharQuery(
        "SELECT COUNT(*) FROM characters.custom_daily_reward_claim WHERE status='pending'",
        "server:health:pending", false)
    if pending then say(player, "pending claim：" .. pending:GetUInt32(0)) end
    local core = G23.WorldQuery("SELECT COUNT(*) FROM command", "server:health:command", false)
    if core then say(player, "world.command核心帮助条目：" .. core:GetUInt32(0)) end
    say(player, "共享注册指令：" .. #G23.GetHelpEntries())
    say(player, "更完整诊断：|cffffff00.luadiag|r")
    line(player)
end

local function handle(player, args)
    local sub, rest = words(args)
    if not CUSTOM[sub] then
        -- Preserve every native or future TrinityCore `.server` subcommand.
        return G23.PASS_THROUGH
    end
    if sub == "" or sub == "help" or sub == "menu" then showMain(player)
    elseif sub == "status" or sub == "lua" then showStatus(player)
    elseif sub == "commands" then showCommands(player, rest)
    elseif sub == "daily" then showDaily(player)
    elseif sub == "health" then showHealth(player)
    elseif sub == "gmhelp" then
        say(player, "GM指令：.gmhelp打开无状态分类菜单；.gmhelp find <关键词>同时搜索自定义目录与world.command。")
    elseif sub == "tp" then
        say(player, "安全传送：.tp打开分类；.tp <关键词>直接搜索。战斗/PVP/飞行/载具/副本安全门保持启用。")
    end
    return false
end

G23.RegisterCommand("server", "统一服务器助手；原生.server子命令保持透传", "服务器助手", 0, handle)

print("[G23-P3A] custom_server_assistant.lua 已加载 -- .server助手与原生子命令安全透传")
