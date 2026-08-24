-- G23-P2: atomic daily-reward claim gate with failure compensation.
-- Requires sql/G23P2_daily_reward_atomic.sql.

if type(G23) ~= "table" then
    error("custom_daily_reward.lua requires extensions/G23Core.ext")
end

local DAILY_GOLD = 50000
local STREAK_BONUS_GOLD = 10000
local MAX_STREAK = 30
local WEEKLY_ITEM = 0
local WEEKLY_ITEM_COUNT = 1
local MAIL_STATIONERY_DEFAULT = 41

local function say(player, msg)
    player:SendBroadcastMessage(msg)
end

local function line(player)
    say(player, "|cff00ccff================================================|r")
end

local function validToken(token)
    return type(token) == "string" and #token == 32 and token:match("^[0-9a-f]+$") ~= nil
end

local function releaseClaim(guid, token)
    if not validToken(token) then return false end
    G23.CharExecVerified(
        "DELETE FROM characters.custom_daily_reward_claim " ..
        "WHERE guid=" .. guid .. " AND claim_date=CURDATE() " ..
        "AND token='" .. token .. "' AND status='pending'",
        "daily:release")
    local q = G23.CharQuery(
        "SELECT 1 FROM characters.custom_daily_reward_claim " ..
        "WHERE guid=" .. guid .. " AND claim_date=CURDATE() AND token='" .. token .. "'",
        "daily:release_verify", false)
    return q == nil
end

local function deliverWeeklyItem(player, guid, streak)
    if WEEKLY_ITEM <= 0 or (streak % 7) ~= 0 then
        return true, false
    end

    local okAdd, item = pcall(player.AddItem, player, WEEKLY_ITEM, WEEKLY_ITEM_COUNT)
    if okAdd and item then
        return true, true
    end

    if type(SendMail) ~= "function" then
        G23.Log("ERROR", "daily:mail_api", "背包已满且缺少SendMail，周奖励无法发放", 60)
        return false, false
    end

    local okMail, itemGuid = pcall(SendMail,
        "每日登录奖励", "背包空间不足，连续登录奖励已通过邮件补发。",
        guid, 0, MAIL_STATIONERY_DEFAULT, 0, 0, 0,
        WEEKLY_ITEM, WEEKLY_ITEM_COUNT)
    if okMail and itemGuid then
        return true, true
    end

    G23.Log("ERROR", "daily:mail_failed", "周奖励物品入包与邮件均失败，guid=" .. guid, 60)
    return false, false
end

local function finalizeClaim(guid, token, streak, totalDays)
    -- First persist summary while the unique daily claim remains pending.
    G23.CharExecVerified(
        "INSERT INTO characters.custom_daily_reward (guid,last_date,streak,total_days) " ..
        "SELECT guid,DATE_FORMAT(claim_date,'%Y%m%d'),streak,total_days " ..
        "FROM characters.custom_daily_reward_claim " ..
        "WHERE guid=" .. guid .. " AND token='" .. token .. "' AND status='pending' " ..
        "ON DUPLICATE KEY UPDATE last_date=VALUES(last_date)," ..
        "streak=VALUES(streak),total_days=VALUES(total_days)",
        "daily:summary")

    local summary = G23.CharQuery(
        "SELECT streak,total_days FROM characters.custom_daily_reward " ..
        "WHERE guid=" .. guid .. " AND last_date=DATE_FORMAT(CURDATE(),'%Y%m%d')",
        "daily:summary_verify", true)
    if not summary or summary:GetUInt32(0) ~= streak or summary:GetUInt32(1) ~= totalDays then
        G23.Log("ERROR", "daily:summary_mismatch:" .. guid,
            "奖励已发放但汇总写入未确认；claim保持pending防止重复，guid=" .. guid, 60)
        return false
    end

    G23.CharExecVerified(
        "UPDATE characters.custom_daily_reward_claim SET status='granted',granted_at=NOW() " ..
        "WHERE guid=" .. guid .. " AND claim_date=CURDATE() " ..
        "AND token='" .. token .. "' AND status='pending'",
        "daily:grant")
    local q = G23.CharQuery(
        "SELECT status FROM characters.custom_daily_reward_claim " ..
        "WHERE guid=" .. guid .. " AND claim_date=CURDATE() AND token='" .. token .. "'",
        "daily:grant_verify", true)
    return q ~= nil and q:GetString(0) == "granted"
end

local function OnLogin(event, player)
    G23.Safe("daily:on_login", function()
        local guid = player:GetGUIDLow()

        -- Database-generated UUID prevents equal tokens across independent Lua states.
        local uq = G23.CharQuery("SELECT LOWER(REPLACE(UUID(),'-',''))", "daily:uuid", true)
        if not uq then return end
        local token = uq:GetString(0)
        if not validToken(token) then
            G23.Log("ERROR", "daily:bad_uuid", "数据库UUID格式异常，停止发奖", 60)
            return
        end

        -- PRIMARY KEY(guid, claim_date) is the atomic gate. Only the INSERT winner
        -- owns token; every other state sees a different token and cannot award.
        local insertSql =
            "INSERT IGNORE INTO characters.custom_daily_reward_claim " ..
            "(guid,claim_date,token,status,streak,total_days) VALUES (" ..
            guid .. ",CURDATE(),'" .. token .. "','pending'," ..
            "COALESCE((SELECT CASE WHEN last_date=DATE_FORMAT(DATE_SUB(CURDATE(),INTERVAL 1 DAY),'%Y%m%d') " ..
            "THEN LEAST(streak+1," .. MAX_STREAK .. ") ELSE 1 END " ..
            "FROM characters.custom_daily_reward WHERE guid=" .. guid .. "),1)," ..
            "COALESCE((SELECT total_days+1 FROM characters.custom_daily_reward WHERE guid=" .. guid .. "),1))"
        G23.CharExecVerified(insertSql, "daily:reserve")

        local claim = G23.CharQuery(
            "SELECT token,status,streak,total_days FROM characters.custom_daily_reward_claim " ..
            "WHERE guid=" .. guid .. " AND claim_date=CURDATE()",
            "daily:reserve_verify", true)
        if not claim then return end

        local ownerToken = claim:GetString(0)
        local status = claim:GetString(1)
        local streak = claim:GetUInt32(2)
        local totalDays = claim:GetUInt32(3)

        if ownerToken ~= token then
            if status == "pending" then
                G23.Log("WARN", "daily:pending:" .. guid,
                    "检测到未完成的每日奖励claim；保持锁定以防重复，guid=" .. guid, 300)
            end
            return
        end
        if status ~= "pending" then return end

        local gold = DAILY_GOLD + (streak - 1) * STREAK_BONUS_GOLD
        local moneyOk = pcall(player.ModifyMoney, player, gold)
        if not moneyOk then
            releaseClaim(guid, token)
            G23.Log("ERROR", "daily:money:" .. guid, "金币发放失败，claim已尝试释放，guid=" .. guid, 60)
            return
        end

        local itemOk, gotItem = deliverWeeklyItem(player, guid, streak)
        if not itemOk then
            local rollbackOk = pcall(player.ModifyMoney, player, -gold)
            if rollbackOk then
                releaseClaim(guid, token)
            else
                G23.Log("ERROR", "daily:compensation:" .. guid,
                    "物品发放失败且金币补偿回滚失败；claim保持pending防重复，guid=" .. guid, 60)
            end
            return
        end

        local finalized = finalizeClaim(guid, token, streak, totalDays)
        if not finalized then
            say(player, "|cffffff00每日奖励已发放，但数据库确认处于保护状态；不会重复发放，请管理员查看.luadiag。|r")
            return
        end

        line(player)
        say(player, "|cffffcc00           每日登录奖励|r")
        line(player)
        say(player, "  连续登录：|cff00ff00" .. streak .. "|r 天")
        say(player, "  累计领取：|cff00ff00" .. totalDays .. "|r 天")
        say(player, "  获得金币：|cffffff00" .. math.floor(gold / 10000) .. "|r 金")
        if gotItem then
            say(player, "  |cffa335ee连续 " .. streak .. " 天额外奖励已发放！|r")
        end
        if streak < 7 then
            say(player, "  |cff888888再连续登录 " .. (7 - streak) .. " 天到达7日节点|r")
        end
        line(player)
    end)
end

RegisterPlayerEvent(3, OnLogin)
print("[G23-P2] custom_daily_reward.lua 已加载 -- 数据库原子领取闸")
