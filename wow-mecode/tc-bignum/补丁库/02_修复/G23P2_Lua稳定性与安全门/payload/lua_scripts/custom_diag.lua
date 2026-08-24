-- G23-P2 administrator-only, on-demand Lua diagnostics.
-- Deliberately performs no database query at script load time.

if type(G23) ~= "table" then
    error("custom_diag.lua requires extensions/G23Core.ext")
end

local function say(player, text)
    player:SendBroadcastMessage(text)
end

local function boolText(value)
    return value and "|cff00ff00可用|r" or "|cffff0000缺失|r"
end

local function runDiag(player)
    say(player, "|cff00ccff========== G23-P2 Lua按需诊断 ==========|r")
    say(player, "版本：" .. tostring(G23.VERSION))
    say(player, "当前Lua state：map=" .. tostring(GetStateMapId()) ..
        " instance=" .. tostring(GetStateInstanceId()))
    say(player, "模式：" .. (IsCompatibilityMode() and "compatibility" or "multistate"))

    local required = {
        { "RegisterPlayerEvent", RegisterPlayerEvent },
        { "RegisterPlayerGossipEvent", RegisterPlayerGossipEvent },
        { "CreateLuaEvent", CreateLuaEvent },
        { "GetStateMapId", GetStateMapId },
        { "CharDBQuery", CharDBQuery },
        { "WorldDBQuery", WorldDBQuery },
        { "SendMail", SendMail },
    }
    for _, entry in ipairs(required) do
        say(player, "必须API " .. entry[1] .. "：" .. boolText(type(entry[2]) == "function"))
    end

    local optional = {
        { "os", os }, { "io", io }, { "debug", debug },
        { "package", package }, { "coroutine", coroutine },
    }
    for _, entry in ipairs(optional) do
        say(player, "可选库 " .. entry[1] .. "：" .. boolText(entry[2] ~= nil))
    end

    -- Database checks start only here, after an authorized GM runs .luadiag.
    local tables = G23.CharQuery(
        "SELECT COUNT(*) FROM information_schema.tables " ..
        "WHERE table_schema='characters' AND table_name IN " ..
        "('custom_daily_reward','custom_daily_reward_claim')",
        "diag:daily_tables", true)
    if tables then
        local count = tables:GetUInt32(0)
        say(player, "每日奖励数据表：" .. (count == 2 and "|cff00ff00PASS 2/2|r" or "|cffff0000FAIL " .. count .. "/2|r"))
    end

    local pending = G23.CharQuery(
        "SELECT COUNT(*) FROM characters.custom_daily_reward_claim WHERE status='pending'",
        "diag:pending_claims", false)
    if pending then
        local count = pending:GetUInt32(0)
        say(player, "待确认奖励claim：" .. (count == 0 and "|cff00ff000|r" or "|cffffff00" .. count .. "|r"))
    end

    local tele = G23.WorldQuery(
        "SELECT (SELECT COUNT(*) FROM game_tele)," ..
        "(SELECT COUNT(*) FROM custom_tele_cn)",
        "diag:tele_tables", true)
    if tele then
        say(player, "传送点：game_tele=" .. tele:GetUInt32(0) ..
            "，中文名=" .. tele:GetUInt32(1))
    end

    local getDataPresent = type(Creature) == "table" and type(Creature.GetData) == "function"
    say(player, "ObjectVariables旧方法：" ..
        (getDataPresent and "|cffffff00仍存在（检查部署状态）|r" or "|cff00ff00已隔离|r"))
    say(player, "公告调度：仅GetStateMapId()=-1的世界state建立一个定时器")
    say(player, "配置要求：Eluna.ReloadSecurityLevel=3（由zz_g23_p2_eluna_security.conf覆盖）")
    say(player, "|cff00ccff=========================================|r")
end

G23.RegisterCommand("luadiag", "管理员按需检查Lua API、DB表及pending claim", "Lua自检", 2,
    function(player)
        runDiag(player)
        return false
    end)

print("[G23-P2] custom_diag.lua 已加载 -- .luadiag按需执行，不做顶层查库")
