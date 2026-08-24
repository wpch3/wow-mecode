local root = assert(arg[1])
local playerEvents, gossipEvents = {}, {}
local now, worldQueries, teleports = 100000, 0, 0
local rows = {}

function GetStateMapId() return 0 end
function GetStateInstanceId() return 0 end
function GetGameTime() return now end
function RegisterPlayerEvent(id, fn)
    playerEvents[id] = playerEvents[id] or {}
    table.insert(playerEvents[id], fn)
end
function RegisterPlayerGossipEvent(menu, event, fn) gossipEvents[menu .. ":" .. event] = fn end
function CharDBQuery() return nil end
function CreateLuaEvent() return 1 end
function SendWorldMessage() end
function SendMail() return 1 end

local function makeQuery(data)
    local index = 1
    local q = {}
    function q:GetUInt32(column) return tonumber(data[index][column + 1]) or 0 end
    function q:GetString(column) return tostring(data[index][column + 1] or "") end
    function q:GetFloat(column) return tonumber(data[index][column + 1]) or 0 end
    function q:NextRow()
        index = index + 1
        return data[index] ~= nil
    end
    return q
end
function WorldDBQuery(sql)
    worldQueries = worldQueries + 1
    if #rows == 0 then return nil end
    return makeQuery(rows)
end

Player, Creature, GameObject, Map = {}, {}, {}, {}
assert(loadfile(root .. "/lua_scripts/extensions/G23Core.ext"))()
assert(loadfile(root .. "/lua_scripts/custom_teleport.lua"))()
local dispatcher = assert(playerEvents[42] and playerEvents[42][1])
local onGossip = assert(gossipEvents["60500:2"])

local messages = {}
local sourceDungeon = false
local player = {
    rank=0, combat=false, pvp=false, mounted=false, flying=false, vehicle=false,
    SendBroadcastMessage=function(self, msg) table.insert(messages, msg) end,
    GetGMRank=function(self) return self.rank end,
    GetGUIDLow=function() return 77 end,
    IsAlive=function() return true end,
    IsDead=function() return false end,
    IsInCombat=function(self) return self.combat end,
    IsPvPFlagged=function(self) return self.pvp end,
    InBattleground=function() return false end,
    InArena=function() return false end,
    InBattlegroundQueue=function() return false end,
    IsOnVehicle=function(self) return self.vehicle end,
    GetVehicle=function(self) return self.vehicle and {} or nil end,
    IsMounted=function(self) return self.mounted end,
    IsFlying=function(self) return self.flying end,
    GetMovementType=function() return 0 end,
    GetMap=function()
        return {
            IsBattleground=function() return false end,
            IsArena=function() return false end,
            IsDungeon=function() return sourceDungeon end,
            IsRaid=function() return false end,
        }
    end,
    Teleport=function(self, map, x, y, z, o) teleports=teleports+1; return true end,
    GossipClearMenu=function() end,
    GossipMenuAddItem=function() end,
    GossipSendMenu=function() end,
    GossipComplete=function() end,
}

rows = { {1,"Stormwind",0,1,2,3,4,"暴风城",1} }
player.combat = true
local beforeQueries = worldQueries
dispatcher(42, player, "tp 暴风")
assert(worldQueries == beforeQueries and teleports == 0, "combat gate ran after query/teleport")

player.combat = false
dispatcher(42, player, "tp 暴风")
assert(teleports == 1, "safe open-world teleport did not execute")

rows = { {2,"Dungeon",33,1,2,3,4,"副本",1} }
dispatcher(42, player, "tp 副本")
assert(teleports == 1, "normal player bypassed destination permission gate")

player.rank = 2
dispatcher(42, player, "tp 副本")
assert(teleports == 2, "GM2 could not use special-map destination")

player.rank = 0
player.pvp = true
beforeQueries = worldQueries
dispatcher(42, player, "tp 暴风")
assert(worldQueries == beforeQueries and teleports == 2, "PVP gate ran after query/teleport")
player.pvp = false

sourceDungeon = true
beforeQueries = worldQueries
dispatcher(42, player, "tp 暴风")
assert(worldQueries == beforeQueries and teleports == 2, "source dungeon gate ran after query/teleport")
sourceDungeon = false

rows = {
    {3,"One",0,1,2,3,4,"一",1},
    {4,"Two",0,5,6,7,8,"二",2},
}
dispatcher(42, player, "tp test")
now = now + 301
onGossip(2, player, player, 9102, 1, "")
assert(teleports == 2, "expired gossip session was reused")

print("G23P2_TELEPORT_SAFETY_LUA_MOCK=PASS")
