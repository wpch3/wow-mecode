local script = assert(arg[1], "custom_teleport.lua path required")
local sourceFile = assert(io.open(script, "rb"))
local source = sourceFile:read("*a")
sourceFile:close()
assert(not source:find("local sessions", 1, true), "state-local sessions table still present")
assert(not source:find("SESSION_TTL", 1, true), "session timeout dependency still present")
assert(not source:find("传送会话已超时", 1, true), "old false-timeout message still present")

local commandHandler, gossipHandler
local worldQueries, teleports = 0, 0
local rows = {}
local lastSql = ""

G23 = {
    RegisterCommand = function(name, description, category, rank, handler)
        assert(name == "tp")
        commandHandler = handler
    end,
    WorldQuery = function(sql)
        worldQueries = worldQueries + 1
        lastSql = sql
        if #rows == 0 then return nil end
        local index = 1
        local q = {}
        function q:GetUInt32(column) return tonumber(rows[index][column + 1]) or 0 end
        function q:GetString(column) return tostring(rows[index][column + 1] or "") end
        function q:GetFloat(column) return tonumber(rows[index][column + 1]) or 0 end
        function q:NextRow()
            index = index + 1
            return rows[index] ~= nil
        end
        return q
    end,
    Log = function() end,
}
function RegisterPlayerGossipEvent(menu, event, fn)
    assert(menu == 60500 and event == 2)
    gossipHandler = fn
end

local messages, menuItems = {}, {}
local sourceDungeon = false
local player = {
    rank=0, combat=false, pvp=false, mounted=false, flying=false, vehicle=false,
    SendBroadcastMessage=function(self, msg) table.insert(messages, msg) end,
    GetGMRank=function(self) return self.rank end,
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
    GossipClearMenu=function() menuItems = {} end,
    GossipMenuAddItem=function(self, icon, label, sender, intid)
        table.insert(menuItems, {label=label, sender=sender, intid=intid})
    end,
    GossipSendMenu=function() end,
    GossipComplete=function() end,
}

assert(loadfile(script))()
assert(type(commandHandler) == "function" and type(gossipHandler) == "function")

local function hasMessage(text)
    for _, msg in ipairs(messages) do
        if msg:find(text, 1, true) then return true end
    end
    return false
end
local function hasSender(sender)
    for _, item in ipairs(menuItems) do
        if item.sender == sender then return true end
    end
    return false
end

-- Safety must run before any query.
rows = { {1,"Stormwind",0,1,2,3,4,"暴风城",1} }
player.combat = true
local before = worldQueries
commandHandler(player, "暴风城")
assert(worldQueries == before and teleports == 0, "combat gate ran after query")
player.combat = false

-- A category click is invoked directly, with no preceding command and therefore
-- models a gossip callback arriving in another Eluna state. It must rebuild
-- the page from sender/intid instead of looking for an in-memory session.
rows = {}
for i = 1, 29 do
    rows[i] = {100+i,"Place"..i,0,i,i+1,i+2,0,"地点"..i,i}
end
messages, menuItems = {}, {}
gossipHandler(2, player, player, 9101, 1, "")
assert(worldQueries > before, "stateless category click did not query")
assert(not hasMessage("传送会话已超时"), "category click still reports false timeout")
assert(hasSender(9102), "category page has no target entries")
assert(hasSender(9103), "29-row category has no next-page navigation")
assert(lastSql:find("t.map IN (0)", 1, true), "category map filter missing")

-- Navigation also works from encoded category/page alone, without a session.
messages, menuItems = {}, {}
gossipHandler(2, player, player, 9103, 100001, "")
assert(not hasMessage("传送会话已超时"), "stateless next page reports false timeout")
assert(lastSql:find("OFFSET 28", 1, true), "next page offset missing")

-- Target selection re-queries the immutable tele id; no previous menu memory is used.
rows = { {321,"Stormwind",0,1,2,3,4,"暴风城",1} }
messages = {}
gossipHandler(2, player, player, 9102, 321, "")
assert(lastSql:find("WHERE t.id=321", 1, true), "target was not re-queried by id")
assert(teleports == 1, "stateless target selection did not teleport")

-- Destination permission still applies after target re-query.
rows = { {777,"Dungeon",33,1,2,3,4,"副本",1} }
gossipHandler(2, player, player, 9102, 777, "")
assert(teleports == 1, "normal player bypassed special-map permission")
player.rank = 2
gossipHandler(2, player, player, 9102, 777, "")
assert(teleports == 2, "GM2 could not use special-map destination")

-- Direct one-result search remains intact.
player.rank = 0
rows = { {1,"Stormwind",0,1,2,3,4,"暴风城",1} }
commandHandler(player, "暴风城")
assert(teleports == 3, "direct one-result search regressed")

print("G23P2R1_TELEPORT_STATELESS_MOCK=PASS")
