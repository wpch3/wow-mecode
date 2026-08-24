local root = assert(arg[1])
local playerEvents = {}
local uuidCounter = 0
local claim = nil
local summary = nil

function GetStateMapId() return 0 end
function GetStateInstanceId() return 0 end
function GetGameTime() return 100000 end
function RegisterPlayerEvent(id, fn)
    playerEvents[id] = playerEvents[id] or {}
    table.insert(playerEvents[id], fn)
end

local function query(row)
    return {
        GetString=function(self, index) return tostring(row[index + 1]) end,
        GetUInt32=function(self, index) return tonumber(row[index + 1]) or 0 end,
    }
end

function CharDBQuery(sql)
    if sql:find("SELECT LOWER(REPLACE(UUID()", 1, true) then
        uuidCounter = uuidCounter + 1
        return query({ string.format("%032x", uuidCounter) })
    end
    if sql:find("INSERT IGNORE INTO characters.custom_daily_reward_claim", 1, true) then
        local token = assert(sql:match("CURDATE%(%)%s*,%s*'([0-9a-f]+)'"))
        if not claim then
            claim = { token=token, status="pending", streak=1, total=1 }
        end
        return nil
    end
    if sql:find("SELECT token,status,streak,total_days", 1, true) then
        if not claim then return nil end
        return query({ claim.token, claim.status, claim.streak, claim.total })
    end
    if sql:find("INSERT INTO characters.custom_daily_reward ", 1, true) then
        if claim and claim.status == "pending" then
            summary = { streak=claim.streak, total=claim.total }
        end
        return nil
    end
    if sql:find("SELECT streak,total_days FROM characters.custom_daily_reward", 1, true) then
        if not summary then return nil end
        return query({ summary.streak, summary.total })
    end
    if sql:find("UPDATE characters.custom_daily_reward_claim SET status='granted'", 1, true) then
        local token = sql:match("token='([0-9a-f]+)'")
        if claim and claim.token == token and claim.status == "pending" then claim.status = "granted" end
        return nil
    end
    if sql:find("SELECT status FROM characters.custom_daily_reward_claim", 1, true) then
        if not claim then return nil end
        return query({ claim.status })
    end
    if sql:find("DELETE FROM characters.custom_daily_reward_claim", 1, true) then
        local token = sql:match("token='([0-9a-f]+)'")
        if claim and claim.token == token and claim.status == "pending" then claim = nil end
        return nil
    end
    if sql:find("SELECT 1 FROM characters.custom_daily_reward_claim", 1, true) then
        return claim and query({1}) or nil
    end
    error("unhandled SQL: " .. sql)
end

function WorldDBQuery(sql) return nil end
function SendMail(...) return 1 end
function CreateLuaEvent(...) return 1 end
function SendWorldMessage(...) end

Player, Creature, GameObject, Map = {}, {}, {}, {}
assert(loadfile(root .. "/lua_scripts/extensions/G23Core.ext"))()
assert(loadfile(root .. "/lua_scripts/custom_daily_reward.lua"))()
assert(playerEvents[3] and #playerEvents[3] == 1)

local money = 0
local messages = {}
local player = {
    GetGUIDLow=function() return 77 end,
    ModifyMoney=function(self, amount) money = money + amount end,
    AddItem=function() return {} end,
    SendBroadcastMessage=function(self, msg) table.insert(messages, msg) end,
}

playerEvents[3][1](3, player)
assert(money == 50000, "first winner did not receive exactly one reward")
assert(claim and claim.status == "granted", "claim not finalized")
assert(summary and summary.streak == 1 and summary.total == 1, "summary not finalized")

playerEvents[3][1](3, player)
assert(money == 50000, "second state/login duplicated reward")
assert(uuidCounter == 2, "second call did not use an independent DB UUID")

-- A pre-award money failure must release the pending row so a later login can retry.
claim, summary, money = nil, nil, 0
player.ModifyMoney = function() error("mock money failure") end
playerEvents[3][1](3, player)
assert(claim == nil, "failed award did not release pending claim")
assert(money == 0, "failed award changed money")

player.ModifyMoney = function(self, amount) money = money + amount end
playerEvents[3][1](3, player)
assert(money == 50000 and claim and claim.status == "granted",
    "safe retry after compensation did not grant exactly once")

print("G23P2_DAILY_ATOMIC_LUA_MOCK=PASS")
