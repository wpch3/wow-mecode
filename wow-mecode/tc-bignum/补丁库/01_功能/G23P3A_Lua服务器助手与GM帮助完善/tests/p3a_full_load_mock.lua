local root=assert(arg[1]); local stateMapId=tonumber(assert(arg[2]))
local playerEvents,serverEvents,gossipEvents={},{},{}
local dbCalls,worldCalls,luaEvents=0,0,0
function GetStateMapId() return stateMapId end
function GetStateInstanceId() return 0 end
function GetGameTime() return 100000 end
function IsCompatibilityMode() return false end
function GetLuaEngine() return "Eluna" end
function GetCoreName() return "TrinityCore" end
function RegisterPlayerEvent(id,fn) playerEvents[id]=playerEvents[id] or {}; table.insert(playerEvents[id],fn) end
function RegisterServerEvent(id,fn) serverEvents[id]=serverEvents[id] or {}; table.insert(serverEvents[id],fn) end
function RegisterPlayerGossipEvent(menu,event,fn) gossipEvents[menu..":"..event]=fn end
function CreateLuaEvent() luaEvents=luaEvents+1; return 1000+luaEvents end
function SendWorldMessage() end
function CharDBQuery() dbCalls=dbCalls+1; return nil end
function WorldDBQuery() worldCalls=worldCalls+1; return nil end
function SendMail() return 1 end
function CreateUint64(v) return v end
Player,Creature,GameObject,Map={},{},{},{}
for _,rel in ipairs({
 "/lua_scripts/extensions/G23Core.ext","/lua_scripts/extensions/ObjectVariables.ext",
 "/lua_scripts/bignum_selftest.lua","/lua_scripts/custom_announce.lua",
 "/lua_scripts/custom_daily_reward.lua","/lua_scripts/custom_diag.lua",
 "/lua_scripts/custom_gmhelp.lua","/lua_scripts/custom_server_assistant.lua",
 "/lua_scripts/custom_teleport.lua","/lua_scripts/custom_welcome.lua",
}) do assert(loadfile(root..rel))() end
assert(dbCalls==0 and worldCalls==0,"full load made top-level DB query")
assert(playerEvents[42] and #playerEvents[42]==1,"dispatcher is not unique")
for _,name in ipairs({"bigtest","luadiag","gmhelp","server","tp","help2"}) do
 assert(G23.commands[name],"missing command: "..name)
end
assert(gossipEvents["60500:2"] and gossipEvents["60510:2"],"teleport/gmhelp gossip collision")
if stateMapId==-1 then
 assert(serverEvents[33] and #serverEvents[33]==1,"world announcement callback missing")
 serverEvents[33][1](33); serverEvents[33][1](33); assert(luaEvents==1,"world timer not singleton")
else
 assert(not serverEvents[33] and luaEvents==0,"map state started world timer")
end
local messages={}
local p={rank=3,GetGMRank=function(s)return s.rank end,GetGUIDLow=function()return 77 end,
 GetName=function()return "Mock" end,SendBroadcastMessage=function(s,m)messages[#messages+1]=m end,
 GossipClearMenu=function()end,GossipMenuAddItem=function()end,GossipSendMenu=function()end,GossipComplete=function()end,
 IsAlive=function()return true end,IsDead=function()return false end,IsInCombat=function()return false end,
 IsPvPFlagged=function()return false end,InBattleground=function()return false end,InArena=function()return false end,
 InBattlegroundQueue=function()return false end,IsOnVehicle=function()return false end,GetVehicle=function()return nil end,
 IsMounted=function()return false end,IsFlying=function()return false end,GetMovementType=function()return 0 end,
 GetMap=function()return {IsBattleground=function()return false end,IsArena=function()return false end,IsDungeon=function()return false end,IsRaid=function()return false end}end,
}
local dispatch=playerEvents[42][1]
assert(dispatch(42,p,"server info")==nil,"full load shadowed native server info")
assert(dispatch(42,p,"server")==false,"full load did not handle server assistant")
print("G23P3A_FULL_LOAD_MOCK=PASS state="..stateMapId)
