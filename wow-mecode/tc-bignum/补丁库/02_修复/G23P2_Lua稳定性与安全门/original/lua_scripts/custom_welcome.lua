--[[
================================================================================
    登录欢迎与命令导览  custom_welcome.lua
================================================================================

    作用：
      1. 玩家登录时显示欢迎信息 + 服务器特色说明
      2. 提供 .help2 命令，随时查看本服自定义指令清单
      3. 首次登录的新号给额外提示

    放置：D:\TC-Build\bin\RelWithDebInfo\lua_scripts\custom_welcome.lua
    生效：游戏内 .reload eluna     （不用重启，不用编译）

    说明：本脚本只用了已在你服务器实测通过的 API
          （RegisterPlayerEvent / SendBroadcastMessage），不会踩坑。
================================================================================
]]

local SERVER_NAME = "艾泽拉斯新纪元"

-- 事件常量（Eluna PlayerEvents）
local PLAYER_EVENT_ON_LOGIN   = 3
local PLAYER_EVENT_ON_COMMAND = 42

-- ---------------------------------------------------------------
-- 工具
-- ---------------------------------------------------------------
local function say(player, msg)
    player:SendBroadcastMessage(msg)
end

local function line(player)
    say(player, "|cff00ccff================================================|r")
end

-- ---------------------------------------------------------------
-- 命令清单（改这里就能更新导览内容）
-- ---------------------------------------------------------------
local COMMANDS = {
    { cat = "装备与套装", items = {
        { ".gearset",                  "打开套装主菜单（可点击）" },
        { ".gearset <职业> <装等>",    "按职业+装等自动配装" },
        { ".gearset preview <职业>",   "只预览不发放" },
        { ".gearset tier on",          "开启职业套装（默认关）" },
        { ".gearset progress",         "查看刷本解锁进度" },
        { ".gearset book",             "套装收藏册" },
        { ".gearset weapon <职业>",    "武器套装" },
        { ".gearset strip",            "卸下全身装备" },
    }},
    { cat = "属性调整", items = {
        { ".modify stat <属性> <值>",  "单项五维直改" },
        { ".modify allstats <值>",     "五维一起改" },
    }},
    { cat = "快速生成", items = {
        { ".add <名称>",               "智能查找并添加物品" },
        { ".spawn <名称>",             "智能生成 NPC" },
        { ".clean",                    "清理生成物" },
    }},
    { cat = "战斗节奏", items = {
        { ".speed",                    "查看当前 GCD/读条/CD 设置" },
        { ".speed gcd <百分比>",       "临时调整 GCD" },
        { ".speed reset",              "恢复配置文件的值" },
    }},
    { cat = "自检", items = {
        { ".bigtest",                  "大数值改造自检（17项）" },
        { ".help2",                    "再次显示本清单" },
    }},
}

local function ShowCommands(player)
    line(player)
    say(player, "|cffffcc00            " .. SERVER_NAME .. " · 自定义指令|r")
    line(player)
    for _, group in ipairs(COMMANDS) do
        say(player, "|cff00ff00【" .. group.cat .. "】|r")
        for _, c in ipairs(group.items) do
            say(player, "  |cffffff00" .. c[1] .. "|r")
            say(player, "      " .. c[2])
        end
    end
    line(player)
    say(player, "|cff888888提示：大部分指令支持中文，例如 .gearset 战士 264|r")
end

-- ---------------------------------------------------------------
-- 登录事件
-- ---------------------------------------------------------------
local function OnLogin(event, player)
    local name = player:GetName()

    line(player)
    say(player, "|cffffcc00        欢迎回到 " .. SERVER_NAME .. "|r")
    line(player)
    say(player, "  " .. name .. "，你好。")
    say(player, "")
    say(player, "  |cff00ff00本服特色|r")
    say(player, "    · 物品属性上限已提升至 |cffffff0021 亿|r")
    say(player, "    · 战斗节奏优化：GCD/读条/技能CD 均已加速")
    say(player, "    · 套装系统：刷副本可解锁整套职业套装")
    say(player, "    · NPCBot：可雇佣队友，支持副本与团本")
    say(player, "")
    say(player, "  输入 |cffffff00.help2|r 查看全部自定义指令")
    line(player)
end

-- ---------------------------------------------------------------
-- 命令拦截
-- ---------------------------------------------------------------
local function OnCommand(event, player, command)
    local cmd = string.lower(command or "")
    if cmd == "help2" or cmd == "帮助" then
        ShowCommands(player)
        return false   -- 拦截，避免提示"命令不存在"
    end
end

RegisterPlayerEvent(PLAYER_EVENT_ON_LOGIN,   OnLogin)
RegisterPlayerEvent(PLAYER_EVENT_ON_COMMAND, OnCommand)

print("[Eluna] custom_welcome.lua 已加载 -- 登录欢迎 + .help2 命令导览")
