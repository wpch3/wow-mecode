--[[
================================================================================
    自动公告轮播  custom_announce.lua   (v2 修复版)
================================================================================

    v1 的问题：CreateLuaEvent 在脚本【顶层】直接调用。
    Eluna 是多状态的（每张地图一个独立 Lua state），脚本加载时
    某些 state 还没准备好定时器，会执行失败被静默跳过。

    v2 改法：改成在 ON_LOGIN 事件里首次注册，并用标志位保证只注册一次。
    这样定时器一定在 state 完全就绪之后才创建。

    放置：D:\TC-Build\bin\RelWithDebInfo\lua_scripts\custom_announce.lua
    生效：.reload eluna

    【重要 · 文件名规则】
    Eluna 会把文件名开头的数字当成 mapId（ElunaLoader.cpp:225 std::from_chars）。
    比如 01_xxx.lua 只在 map 1 加载，02_xxx.lua 因为 map 2 不存在而永不加载。
    所以文件名【不能以数字开头】。

================================================================================
]]

local INTERVAL = 600000        -- 10 分钟
local PREFIX   = "|cff00ccff[提示]|r "

local PLAYER_EVENT_ON_LOGIN = 3

-- ---------------------------------------------------------------
-- 公告内容（改这里即可）
-- ---------------------------------------------------------------
local MESSAGES = {
    "输入 |cffffff00.help2|r 查看本服全部自定义指令",
    "输入 |cffffff00.gearset|r 打开套装系统，可按职业+装等一键配装",
    "刷副本可以解锁整套职业套装，输入 |cffffff00.gearset progress|r 查看进度",
    "本服物品属性上限已提升至 |cffffff0021 亿|r，不再受 32767 限制",
    "战斗节奏已优化：GCD、读条、技能冷却均已加速，PVP 中保持原版",
    "输入 |cffffff00.speed|r 查看当前的战斗节奏设置",
    "可以雇佣 NPCBot 作为队友，支持五人本与团本",
    "遇到问题或有建议，欢迎随时反馈",
}

-- ---------------------------------------------------------------
local index   = 0
local started = false

local function DoAnnounce()
    if #MESSAGES == 0 then
        return
    end
    index = index + 1
    if index > #MESSAGES then
        index = 1
    end
    SendWorldMessage(PREFIX .. MESSAGES[index])
end

-- 首个玩家登录时才启动定时器，保证 Lua state 已完全就绪
local function OnLogin(event, player)
    if started then
        return
    end
    started = true
    CreateLuaEvent(DoAnnounce, INTERVAL, 0)   -- 0 = 无限重复
end

RegisterPlayerEvent(PLAYER_EVENT_ON_LOGIN, OnLogin)

print("[Eluna] custom_announce.lua 已加载 -- 共 " .. #MESSAGES ..
      " 条公告，首位玩家登录后开始轮播")
