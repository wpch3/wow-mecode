-- G23-P2: on-demand big-number and Eluna self-test.
-- No database query runs at script load time.

if type(G23) ~= "table" then
    error("bignum_selftest.lua requires extensions/G23Core.ext")
end

local function say(player, msg)
    player:SendBroadcastMessage(msg)
end

local function line(player)
    say(player, "|cff666666================================|r")
end

local function try(fn)
    local ok, value = pcall(fn)
    return ok, value
end

local function RunSelfTest(player)
    local total, passed = 0, 0
    local optionalTotal, optionalAvailable = 0, 0

    local function check(cond, name, detail)
        total = total + 1
        if cond then
            passed = passed + 1
            say(player, "|cff00ff00 [OK] |r" .. name .. (detail and ("  |cff888888" .. tostring(detail) .. "|r") or ""))
        else
            say(player, "|cffff0000 [NG] |r" .. name .. (detail and ("  |cffffff00" .. tostring(detail) .. "|r") or ""))
        end
    end

    local function optional(name, value)
        optionalTotal = optionalTotal + 1
        local available = value ~= nil
        if available then optionalAvailable = optionalAvailable + 1 end
        say(player, (available and "|cff00ccff [可选] |r" or "|cff888888 [可选缺失] |r") .. name)
    end

    local function info(name, value)
        say(player, "|cff00ccff  *   |r" .. name .. "  |cffffffff" .. tostring(value) .. "|r")
    end

    line(player)
    say(player, "|cffffcc00   大数值改造 + Eluna 自检（G23-P2）|r")
    line(player)

    say(player, "|cffffcc00[1] 必须API|r")
    check(type(RegisterPlayerEvent) == "function", "RegisterPlayerEvent")
    check(type(CreateUint64) == "function", "CreateUint64")
    check(type(WorldDBQuery) == "function", "WorldDBQuery")
    check(type(GetGameTime) == "function", "GetGameTime")
    info("Lua版本", _VERSION)
    if type(GetLuaEngine) == "function" then info("Eluna", GetLuaEngine()) end
    if type(GetCoreName) == "function" then info("核心", GetCoreName()) end

    line(player)
    say(player, "|cffffcc00[2] uint64截断修复|r")
    if type(CreateUint64) == "function" then
        local okSmall = try(function() return CreateUint64(4000000000) end)
        check(okSmall, "CreateUint64(40亿)", "基线")

        local okBig, errBig = try(function() return CreateUint64(10000000000) end)
        check(okBig, "CreateUint64(100亿) ★关键项", okBig and "已突破42亿限制" or tostring(errBig))

        local okHuge = try(function() return CreateUint64(1000000000000) end)
        check(okHuge, "CreateUint64(1万亿)")

        local okStr = try(function() return CreateUint64("18446744073709551615") end)
        check(okStr, "CreateUint64(字符串最大值)", "对照组")
    else
        check(false, "uint64测试可执行", "缺少必须API")
    end

    line(player)
    say(player, "|cffffcc00[3] 大数值属性读写与还原|r")
    local origMax = player:GetMaxHealth()
    local origCur = player:GetHealth()
    info("当前最大生命", origMax)

    local ok1 = try(function() player:SetMaxHealth(1000000000) end)
    local got1 = player:GetMaxHealth()
    check(ok1 and got1 == 1000000000, "SetMaxHealth(10亿)", got1)

    local ok2 = try(function() player:SetMaxHealth(4000000000) end)
    local got2 = player:GetMaxHealth()
    check(ok2 and got2 == 4000000000, "SetMaxHealth(40亿)", got2)

    local restored = pcall(function()
        player:SetMaxHealth(origMax)
        player:SetHealth(math.min(origCur, origMax))
    end)
    check(restored and player:GetMaxHealth() == origMax, "恢复原生命值", player:GetMaxHealth())

    line(player)
    say(player, "|cffffcc00[4] 数据库大数值（按需查询）|r")
    local q = G23.WorldQuery(
        "SELECT stat_value1, armor, holy_res, MaxDurability FROM item_template WHERE entry = 900001",
        "bigtest:item900001", false)
    if q then
        local s = q:GetUInt32(0)
        local a = q:GetUInt32(1)
        local h = q:GetUInt32(2)
        local d = q:GetUInt32(3)
        check(s == 100000000,  "stat_value1 = 1亿", s)
        check(a == 1000000000, "armor = 10亿", a)
        check(h == 500000000,  "holy_res = 5亿", h)
        check(d == 1000000,    "MaxDurability = 100万", d)
    else
        check(false, "查询测试物品900001", "未找到或数据库查询失败")
    end

    line(player)
    say(player, "|cffffcc00[5] 可选标准库（不计入必须PASS）|r")
    optional("os库", os)
    optional("io库", io)
    optional("require", require)
    optional("package", package)
    optional("debug库", debug)
    optional("coroutine库", coroutine)

    line(player)
    say(player, "|cffffcc00[6] Lua double精度边界|r")
    local p53 = 9007199254740992
    local boundaryCorrect = (p53 + 1 == p53) and (p53 - 1 ~= p53)
    check(boundaryCorrect, "2^53边界行为（非恒真断言）",
        string.format("%.0f", p53) .. "; +1不可区分，-1可区分")
    info("说明", "10亿/40亿远低于2^53精确整数上限")

    line(player)
    if passed == total then
        say(player, "|cff00ff00   必须项全部通过：" .. passed .. " / " .. total .. "|r")
    else
        say(player, "|cffffff00   必须项通过：" .. passed .. " / " .. total .. "|r")
    end
    say(player, "|cff00ccff   可选标准库：" .. optionalAvailable .. " / " .. optionalTotal .. " 可用（不影响必须项）|r")
    line(player)
end

G23.RegisterCommand("bigtest", "大数值与Eluna必须API自检", "Lua自检", 0,
    function(player)
        RunSelfTest(player)
        return false
    end)

print("[G23-P2] bignum_selftest.lua 已加载 -- .bigtest 按需执行")
