-- G23-P2 login welcome and shared command registry view.

if type(G23) ~= "table" then
    error("custom_welcome.lua requires extensions/G23Core.ext")
end

local SERVER_NAME = "艾泽拉斯新纪元"

local function say(player, msg)
    player:SendBroadcastMessage(msg)
end

local function line(player)
    say(player, "|cff00ccff================================================|r")
end

-- C++ commands cannot be enumerated through this Eluna API, so register only
-- commands backed by current project sources. Lua commands register themselves.
local CORE_HELP = {
    { "gearset", "套装系统主菜单与配装", "装备与套装" },
    { "gearset progress", "查看副本套装解锁进度", "装备与套装" },
    { "gearset preview <职业>", "只预览职业套装", "装备与套装" },
    { "modify stat <属性> <值>", "调整单项属性", "属性与工具" },
    { "modify allstats <值>", "一起调整五维", "属性与工具" },
    { "add <名称>", "查找并添加物品", "属性与工具" },
    { "spawn <名称>", "查找并生成NPC", "属性与工具" },
    { "clean", "清理生成物", "属性与工具" },
    { "speed", "查看战斗节奏设置", "战斗与Bot" },
    { "combo", "NPCBot战斗辅助、增益与治疗策略", "战斗与Bot" },
}
for _, entry in ipairs(CORE_HELP) do
    G23.AddHelp(entry[1], entry[2], entry[3])
end

local function showCommands(player)
    line(player)
    say(player, "|cffffcc00            " .. SERVER_NAME .. " · 当前指令|r")
    line(player)
    local lastCategory = nil
    for _, entry in ipairs(G23.GetHelpEntries()) do
        if entry.category ~= lastCategory then
            lastCategory = entry.category
            say(player, "|cff00ff00【" .. lastCategory .. "】|r")
        end
        say(player, "  |cffffff00." .. entry.name .. "|r  " .. entry.description)
    end
    line(player)
    say(player, "|cff888888每日奖励自动触发；管理员可用 .luadiag 按需诊断。|r")
end

local function onLogin(event, player)
    line(player)
    say(player, "|cffffcc00欢迎回到 " .. SERVER_NAME .. "，" .. player:GetName() .. "。|r")
    say(player, "  · F45群体拾取已启用并显示单次统计")
    say(player, "  · 每日奖励使用数据库原子领取闸")
    say(player, "  · .tp 已启用战斗/PVP/飞行/载具/地图安全门")
    say(player, "  输入 |cffffff00.help2|r 查看当前指令")
    line(player)
end

G23.RegisterCommand("help2", "显示当前自定义指令清单", "帮助", 0,
    function(player)
        showCommands(player)
        return false
    end)
RegisterPlayerEvent(3, onLogin)

print("[G23-P2] custom_welcome.lua 已加载 -- 共享命令表驱动.help2")
