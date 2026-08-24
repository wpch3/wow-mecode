-- G23-P2: exactly one world-state announcement scheduler.
-- Map/instance Lua states do not register a timer.

if type(G23) ~= "table" then
    error("custom_announce.lua requires extensions/G23Core.ext")
end

local INTERVAL = 600000
local PREFIX = "|cff00ccff[提示]|r "
local SERVER_EVENT_ON_LUA_STATE_OPEN = 33

local MESSAGES = {
    "输入 |cffffff00.help2|r 查看本服当前自定义指令",
    "输入 |cffffff00.gearset|r 打开套装系统，可按职业和装等配装",
    "刷副本可以解锁职业套装，输入 |cffffff00.gearset progress|r 查看进度",
    "本服物品属性上限已提升至 |cffffff0021 亿|r",
    "战斗节奏已优化；PVP与副本安全规则仍然生效",
    "输入 |cffffff00.speed|r 查看当前战斗节奏设置",
    "输入 |cffffff00.tp|r 使用带安全门的中文传送菜单",
    "遇到问题可让管理员使用 |cffffff00.luadiag|r 做按需诊断",
}

local index = 0
local timerId = nil

local function DoAnnounce()
    G23.Safe("announce:send", function()
        if #MESSAGES == 0 then return end
        index = index + 1
        if index > #MESSAGES then index = 1 end
        SendWorldMessage(PREFIX .. MESSAGES[index])
    end)
end

local function StartWorldTimer()
    if GetStateMapId() ~= -1 or timerId ~= nil then
        return
    end
    local ok, value = pcall(CreateLuaEvent, DoAnnounce, INTERVAL, 0)
    if not ok or not value then
        G23.Log("ERROR", "announce:timer", "全服公告定时器创建失败: " .. tostring(value), 60)
        return
    end
    timerId = value
    G23.runtime.announceTimerId = timerId
    G23.runtime.announceInterval = INTERVAL
    print("[G23-P2] custom_announce.lua 世界state单例定时器已启动，eventId=" .. tostring(timerId))
end

-- GetStateMapId() == -1 is the unique world state in this multistate Eluna.
if type(GetStateMapId) == "function" and GetStateMapId() == -1 then
    RegisterServerEvent(SERVER_EVENT_ON_LUA_STATE_OPEN, StartWorldTimer)
    print("[G23-P2] custom_announce.lua 已加载 -- 仅世界state注册调度器")
end
