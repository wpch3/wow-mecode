-- 模拟 Eluna 环境，验证 bignum_selftest.lua 逻辑
local Player = {}
Player.__index = Player
function Player:SendBroadcastMessage(m)
    m = m:gsub("|c%x%x%x%x%x%x%x%x", ""):gsub("|r", "")
    print(m)
end
local _hp, _maxhp = 18000, 20000
function Player:GetMaxHealth() return _maxhp end
function Player:GetHealth() return _hp end
function Player:SetMaxHealth(v) _maxhp = v end
function Player:SetHealth(v) _hp = v end
function Player:GetCoinage() return 12345 end
function Player:GetGUID() return "guid-obj" end

_G.GetLuaEngine = function() return "Eluna 4.x" end
_G.GetCoreName  = function() return "TrinityCore" end

-- 模拟【已打补丁】的 CreateUint64
_G.CreateUint64 = function(n)
    if type(n)=="number" and n > 4294967295 then error("bad argument #1 (value must be less than or equal to 4294967295)",0) end
    if type(n) == "string" then return n end
    return n
end

local Q = {}
Q.__index = Q
local vals = {100000000, 1000000000, 500000000, 1000000}
function Q:GetUInt32(i) return vals[i + 1] end
_G.WorldDBQuery = function(sql) return setmetatable({}, Q) end

local handler
_G.RegisterPlayerEvent = function(id, fn) handler = fn; print("[注册] 事件 " .. id) end

dofile("/home/user/tc-bignum/lua_scripts/bignum_selftest.lua")
print("")
print("########## 模拟执行 .bigtest ##########")
print("")
handler(42, setmetatable({}, Player), "bigtest")
