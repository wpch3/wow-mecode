local payload = assert(arg[1], "payload root required")
local playerEvents, gossipEvents = {}, {}
local charCalls, worldCalls = 0, 0

function GetStateMapId() return -1 end
function GetStateInstanceId() return 0 end
function GetGameTime() return 100000 end
function IsCompatibilityMode() return false end
function RegisterPlayerEvent(id, fn)
    playerEvents[id] = playerEvents[id] or {}
    table.insert(playerEvents[id], fn)
end
function RegisterPlayerGossipEvent(menu, event, fn) gossipEvents[menu .. ":" .. event] = fn end
function CreateLuaEvent() return 1 end
function SendMail() return 1 end

local function query(data)
    local index, q = 1, {}
    function q:GetUInt32(column) return tonumber(data[index][column+1]) or 0 end
    function q:GetString(column) return tostring(data[index][column+1] or "") end
    function q:NextRow() index=index+1; return data[index] ~= nil end
    return q
end
function CharDBQuery(sql)
    charCalls = charCalls + 1
    if sql:find("information_schema",1,true) then return query({{2}}) end
    if sql:find("status='pending'",1,true) then return query({{0}}) end
    return query({{"granted","2026-08-22",3,10}})
end
function WorldDBQuery(sql)
    worldCalls = worldCalls + 1
    if sql:find("COUNT(*) FROM command",1,true) then return query({{500}}) end
    return query({{"server info","Syntax: .server info"},{"tele","Syntax: .tele $location"}})
end

Player, Creature, GameObject, Map = {}, {}, {}, {}
assert(loadfile(payload .. "/lua_scripts/extensions/G23Core.ext"))()
assert(loadfile(payload .. "/lua_scripts/custom_gmhelp.lua"))()
assert(loadfile(payload .. "/lua_scripts/custom_server_assistant.lua"))()
assert(loadfile(payload .. "/lua_scripts/custom_welcome.lua"))()
assert(charCalls == 0 and worldCalls == 0, "top-level DB query detected")
assert(playerEvents[42] and #playerEvents[42] == 1, "shared dispatcher count changed")
assert(gossipEvents["60510:2"], "gmhelp gossip not registered")
assert(G23.runtime.gmhelpEntryCount >= 140, "gmhelp project catalog is still incomplete")
assert(G23.runtime.gmhelpCategoryCount == 11, "gmhelp category count mismatch")

local messages, menuItems = {}, {}
local player = {
    rank=3,
    GetGMRank=function(self) return self.rank end,
    GetGUIDLow=function() return 77 end,
    GetName=function() return "Mock" end,
    SendBroadcastMessage=function(self,msg) messages[#messages+1]=msg end,
    GossipClearMenu=function() menuItems={} end,
    GossipMenuAddItem=function(self,icon,label,sender,intid)
        menuItems[#menuItems+1]={label=label,sender=sender,intid=intid}
    end,
    GossipSendMenu=function() end,
    GossipComplete=function() end,
}
local dispatch = playerEvents[42][1]

-- Bare/custom .server is consumed, but every native/unknown subtree passes to core.
assert(dispatch(42,player,"server") == false, "bare .server was not handled")
local beforeChar, beforeWorld = charCalls, worldCalls
assert(dispatch(42,player,"server status") == false, "server status was not handled")
assert(charCalls==beforeChar and worldCalls==beforeWorld, "server status queried DB")
assert(dispatch(42,player,"server info") == nil, "native .server info was shadowed")
assert(dispatch(42,player,"server restart 10") == nil, "native .server restart was shadowed")
assert(dispatch(42,player,"server shutdown 10") == nil, "native .server shutdown was shadowed")
assert(dispatch(42,player,"server future-native") == nil, "future native server command was shadowed")
assert(dispatch(42,nil,"server shutdown 1") == nil, "console command was intercepted")

assert(dispatch(42,player,"server daily") == false and charCalls>beforeChar,
       "on-demand daily status did not query")
beforeWorld=worldCalls
assert(dispatch(42,player,"gmhelp find 传送") == false and worldCalls>beforeWorld,
       "gmhelp did not combine world.command search")

-- GM permission is checked before handler/DB work.
player.rank=0; beforeWorld=worldCalls
assert(dispatch(42,player,"gmhelp find server") == false, "unauthorized gmhelp not intercepted")
assert(worldCalls==beforeWorld, "unauthorized gmhelp reached DB")

-- Direct category click models another map state: static sender/intid is enough.
player.rank=3; messages,menuItems={},{}
local gossip=gossipEvents["60510:2"]
gossip(2,player,player,9311,3,"",60510)
assert(#menuItems>0, "stateless gmhelp category click produced no entries")
for _,msg in ipairs(messages) do assert(not msg:find("会话",1,true), "gmhelp reintroduced session dependency") end

-- Rank-aware shared help hides GM-only entries from players.
local normal=G23.GetHelpEntries(0)
for _,entry in ipairs(normal) do assert(entry.name~="gmhelp", "rank filter leaked gmhelp") end
local gm=G23.GetHelpEntries(1)
local found=false
for _,entry in ipairs(gm) do if entry.name=="gmhelp" then found=true end end
assert(found, "rank filter hid gmhelp from GM1")

print("G23P3A_COMMAND_PASS_THROUGH_MOCK=PASS")
print("G23P3A_GMHELP_DYNAMIC_AND_STATELESS_MOCK=PASS")
print("G23P3A_NO_TOP_LEVEL_DB=PASS")
