local oldScript = assert(arg[1], "old script required")
local newScript = assert(arg[2], "new script required")

local function run(script)
    local messages, queryCount, gossipHandler = {}, 0, nil
    local env = {}
    setmetatable(env, {__index=_G})
    env.G23 = {
        Now=function() return 100000 end,
        RegisterCommand=function() end,
        WorldQuery=function()
            queryCount = queryCount + 1
            local data = { {1,"Stormwind",0,1,2,3,4,"暴风城",1} }
            local index, q = 1, {}
            function q:GetUInt32(column) return tonumber(data[index][column+1]) or 0 end
            function q:GetString(column) return tostring(data[index][column+1] or "") end
            function q:GetFloat(column) return tonumber(data[index][column+1]) or 0 end
            function q:NextRow() index=index+1; return data[index] ~= nil end
            return q
        end,
        Log=function() end,
    }
    env.RegisterPlayerEvent=function() end
    env.RegisterPlayerGossipEvent=function(menu, event, fn) gossipHandler=fn end
    local player = {
        SendBroadcastMessage=function(self,msg) messages[#messages+1]=msg end,
        GetGMRank=function() return 0 end,
        GetGUIDLow=function() return 77 end,
        IsAlive=function() return true end, IsDead=function() return false end,
        IsInCombat=function() return false end, IsPvPFlagged=function() return false end,
        InBattleground=function() return false end, InArena=function() return false end,
        InBattlegroundQueue=function() return false end, IsOnVehicle=function() return false end,
        GetVehicle=function() return nil end, IsMounted=function() return false end,
        IsFlying=function() return false end, GetMovementType=function() return 0 end,
        GetMap=function() return {
            IsBattleground=function() return false end, IsArena=function() return false end,
            IsDungeon=function() return false end, IsRaid=function() return false end,
        } end,
        GossipComplete=function() end, GossipClearMenu=function() end,
        GossipMenuAddItem=function() end, GossipSendMenu=function() end,
        Teleport=function() return true end,
    }
    assert(loadfile(script, "t", env))()
    assert(gossipHandler, "gossip handler not registered")
    -- Deliberately click a category without creating a session in this state.
    gossipHandler(2, player, player, 9101, 1, "")
    local timeout = false
    for _, msg in ipairs(messages) do
        if msg:find("传送会话已超时", 1, true) then timeout=true end
    end
    return timeout, queryCount
end

local oldTimeout, oldQueries = run(oldScript)
local newTimeout, newQueries = run(newScript)
assert(oldTimeout and oldQueries == 0, "old script did not reproduce cross-state false timeout")
assert(not newTimeout and newQueries == 1, "new script did not recover statelessly")
print("G23P2R1_CROSS_STATE_REPRO_OLD=PASS")
print("G23P2R1_CROSS_STATE_FIXED_NEW=PASS")
