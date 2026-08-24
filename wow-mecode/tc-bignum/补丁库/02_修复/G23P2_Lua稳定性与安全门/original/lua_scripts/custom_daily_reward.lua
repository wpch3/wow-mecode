--[[
================================================================================
    每日登录奖励  custom_daily_reward.lua   (v2 修复版)
================================================================================

    v1 的问题：用了 os.date() / os.time()。
    Eluna 的沙箱在某些配置下会限制 os 库，导致脚本执行失败被静默跳过。

    v2 改法：
      1. 日期改用 MySQL 的 CURDATE() 取，不依赖 Lua 的 os 库
      2. 连续判定改用 SQL 的 DATEDIFF()，逻辑更可靠
      3. 整条 SQL 一次搞定，减少往返

    需要建表：sql/13_daily_reward.sql

    放置：D:\TC-Build\bin\RelWithDebInfo\lua_scripts\custom_daily_reward.lua
    生效：.reload eluna

    【重要 · 文件名规则】
    Eluna 会把文件名开头的数字当成 mapId（ElunaLoader.cpp:225 std::from_chars）。
    比如 01_xxx.lua 只在 map 1 加载，02_xxx.lua 因为 map 2 不存在而永不加载。
    所以文件名【不能以数字开头】。

================================================================================
]]

local PLAYER_EVENT_ON_LOGIN = 3

-- ---------------------------------------------------------------
-- 奖励配置
-- ---------------------------------------------------------------
local DAILY_GOLD        = 50000    -- 每日基础 5 金（单位铜）
local STREAK_BONUS_GOLD = 10000    -- 每多连续一天 +1 金
local MAX_STREAK        = 30       -- 连续天数上限（防止无限叠加）
local WEEKLY_ITEM       = 0        -- 连续7天额外物品ID，0=不发
local WEEKLY_ITEM_COUNT = 1

-- ---------------------------------------------------------------
local function say(player, msg)
    player:SendBroadcastMessage(msg)
end

local function line(player)
    say(player, "|cff00ccff================================================|r")
end

-- ---------------------------------------------------------------
local function OnLogin(event, player)
    local guid = player:GetGUIDLow()

    -- 用 SQL 取日期和天数差，不依赖 Lua 的 os 库
    --   d0 : 今天是不是已经领过了 (1=领过)
    --   d1 : 距上次领取过了几天 (NULL=首次)
    --   st : 上次的连续天数
    local q = CharDBQuery(
        "SELECT " ..
        "  IFNULL(last_date = DATE_FORMAT(CURDATE(),'%Y%m%d'), 0), " ..
        "  IFNULL(DATEDIFF(CURDATE(), STR_TO_DATE(last_date,'%Y%m%d')), 999), " ..
        "  IFNULL(streak, 0), " ..
        "  DATE_FORMAT(CURDATE(),'%Y%m%d') " ..
        "FROM custom_daily_reward WHERE guid = " .. guid)

    local today   = nil
    local already = false
    local daydiff = 999
    local streak  = 0

    if q then
        already = (q:GetUInt32(0) == 1)
        daydiff = q:GetUInt32(1)
        streak  = q:GetUInt32(2)
        today   = q:GetString(3)
    else
        -- 没有记录 = 首次登录，单独取一次日期
        local q2 = CharDBQuery("SELECT DATE_FORMAT(CURDATE(),'%Y%m%d')")
        if q2 then
            today = q2:GetString(0)
        end
    end

    if not today then
        return   -- 取日期失败，静默跳过，不影响登录
    end

    if already then
        return   -- 今天已领
    end

    -- 连续判定：昨天领过就 +1，否则重置
    if daydiff == 1 then
        streak = streak + 1
        if streak > MAX_STREAK then
            streak = MAX_STREAK
        end
    else
        streak = 1
    end

    -- 发放
    local gold = DAILY_GOLD + (streak - 1) * STREAK_BONUS_GOLD
    player:ModifyMoney(gold)

    local gotItem = false
    if WEEKLY_ITEM > 0 and (streak % 7) == 0 then
        player:AddItem(WEEKLY_ITEM, WEEKLY_ITEM_COUNT)
        gotItem = true
    end

    -- 写回记录
    CharDBExecute(
        "INSERT INTO custom_daily_reward (guid, last_date, streak, total_days) " ..
        "VALUES (" .. guid .. ", '" .. today .. "', " .. streak .. ", 1) " ..
        "ON DUPLICATE KEY UPDATE " ..
        "  last_date = '" .. today .. "', " ..
        "  streak = " .. streak .. ", " ..
        "  total_days = total_days + 1")

    -- 提示
    line(player)
    say(player, "|cffffcc00           每日登录奖励|r")
    line(player)
    say(player, "  连续登录：|cff00ff00" .. streak .. "|r 天")
    say(player, "  获得金币：|cffffff00" .. math.floor(gold / 10000) .. "|r 金")
    if gotItem then
        say(player, "  |cffa335ee连续 " .. streak .. " 天额外奖励已发放！|r")
    end
    if streak < 7 then
        say(player, "  |cff888888再连续登录 " .. (7 - streak) .. " 天有额外奖励|r")
    end
    line(player)
end

RegisterPlayerEvent(PLAYER_EVENT_ON_LOGIN, OnLogin)

print("[Eluna] custom_daily_reward.lua 已加载 -- 每日登录奖励")
