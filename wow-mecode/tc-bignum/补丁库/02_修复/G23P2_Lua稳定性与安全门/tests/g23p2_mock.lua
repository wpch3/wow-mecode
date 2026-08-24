local root = assert(arg[1], "payload root required")
local stateArg = assert(arg[2], "state map id required")
local stateMapId = tonumber(stateArg)

local playerEvents, serverEvents, gossipEvents = {}, {}, {}
local dbCalls, worldCalls, luaEvents = 0, 0, 0

function GetStateMapId() return stateMapId end
function GetStateInstanceId() return 0 end
function GetGameTime() return 100000 end
function IsCompatibilityMode() return false end
function GetLuaEngine() return "Eluna" end
function GetCoreName() return "TrinityCore" end
function RegisterPlayerEvent(id, fn)
    playerEvents[id] = playerEvents[id] or {}
    table.insert(playerEvents[id], fn)
end
function RegisterServerEvent(id, fn)
    serverEvents[id] = serverEvents[id] or {}
    table.insert(serverEvents[id], fn)
end
function RegisterPlayerGossipEvent(menu, event, fn)
    gossipEvents[menu .. ":" .. event] = fn
end
function CreateLuaEvent(fn, delay, repeats)
    luaEvents = luaEvents + 1
    return 1000 + luaEvents
end
function SendWorldMessage(msg) end
function CharDBQuery(sql) dbCalls = dbCalls + 1; return nil end
function WorldDBQuery(sql) worldCalls = worldCalls + 1; return nil end
function SendMail(...) return 123 end
function CreateUint64(value) return value end

Player, Creature, GameObject, Map = {}, {}, {}, {}

local files = {
    "/lua_scripts/extensions/G23Core.ext",
    "/lua_scripts/extensions/ObjectVariables.ext",
    "/lua_scripts/bignum_selftest.lua",
    "/lua_scripts/custom_announce.lua",
    "/lua_scripts/custom_daily_reward.lua",
    "/lua_scripts/custom_diag.lua",
    "/lua_scripts/custom_teleport.lua",
    "/lua_scripts/custom_welcome.lua",
}
for _, rel in ipairs(files) do
    assert(loadfile(root .. rel))()
end

assert(dbCalls == 0, "top-level character DB query detected")
assert(worldCalls == 0, "top-level world DB query detected")
assert(playerEvents[42] and #playerEvents[42] == 1, "shared command dispatcher must register exactly once")

if stateMapId == -1 then
    assert(serverEvents[33] and #serverEvents[33] == 1, "world state must register one state-open callback")
    serverEvents[33][1](33)
    serverEvents[33][1](33)
    assert(luaEvents == 1, "world state must create exactly one announcement timer")
else
    assert(not serverEvents[33], "map state must not register announcement state-open callback")
    assert(luaEvents == 0, "map state must not create announcement timer")
end

local messages = {}
local player = {
    rank = 1,
    combat = false,
    SendBroadcastMessage = function(self, msg) table.insert(messages, msg) end,
    GetGMRank = function(self) return self.rank end,
    GetGUIDLow = function(self) return 77 end,
    IsAlive = function(self) return true end,
    IsDead = function(self) return false end,
    IsInCombat = function(self) return self.combat end,
    IsPvPFlagged = function(self) return false end,
    InBattleground = function(self) return false end,
    InArena = function(self) return false end,
    InBattlegroundQueue = function(self) return false end,
    IsOnVehicle = function(self) return false end,
    GetVehicle = function(self) return nil end,
    IsMounted = function(self) return false end,
    IsFlying = function(self) return false end,
    GetMovementType = function(self) return 0 end,
    GetMap = function(self)
        return {
            IsBattleground=function() return false end,
            IsArena=function() return false end,
            IsDungeon=function() return false end,
            IsRaid=function() return false end,
        }
    end,
    GossipClearMenu = function() end,
    GossipMenuAddItem = function() end,
    GossipSendMenu = function() end,
    GossipComplete = function() end,
    GetName = function() return "Mock" end,
}

local dispatcher = playerEvents[42][1]
local beforeDb = dbCalls
local result = dispatcher(42, player, "luadiag")
assert(result == false, "unauthorized luadiag must be intercepted")
assert(dbCalls == beforeDb, "unauthorized luadiag must not query DB")

player.combat = true
local beforeWorld = worldCalls
dispatcher(42, player, "tp Stormwind")
assert(worldCalls == beforeWorld, "combat teleport must be denied before DB query")

player.combat = false
player.rank = 2
dispatcher(42, player, "luadiag")
assert(dbCalls > beforeDb, "authorized on-demand diag should query DB")

print("G23P2_LUA_MOCK=PASS state=" .. stateMapId)
