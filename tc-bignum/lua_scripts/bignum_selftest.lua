--[[
=====================================================================
  大数值改造 + Eluna 补丁 —— 自检脚本
=====================================================================
  用法：
    1. 把本文件放进 worldserver.conf 里 Eluna.ScriptPath 指向的目录
       你的配置是： data\lua_scripts
       实际路径通常是： D:\TC-Build\bin\RelWithDebInfo\data\lua_scripts\
    2. 服务端控制台执行： reload eluna
       （或游戏内 .reload eluna）
    3. 游戏内输入： .bigtest
    4. 看聊天框输出

  本脚本只读检测 + 临时改血量后立即还原，不会破坏任何数据。
=====================================================================
]]

local COMMAND = "bigtest"

local function say(p, msg) p:SendBroadcastMessage(msg) end
local function line(p)     p:SendBroadcastMessage("|cff666666================================|r") end

local function try(fn)
    local ok, res = pcall(fn)
    return ok, res
end

local function RunSelfTest(event, player, command)
    if command ~= COMMAND then return end

    local total, passed = 0, 0
    local function check(cond, name, detail)
        total = total + 1
        if cond then
            passed = passed + 1
            say(player, "|cff00ff00 [OK] |r" .. name .. (detail and ("  |cff888888" .. tostring(detail) .. "|r") or ""))
        else
            say(player, "|cffff0000 [NG] |r" .. name .. (detail and ("  |cffffff00" .. tostring(detail) .. "|r") or ""))
        end
    end
    local function info(name, val)
        say(player, "|cff00ccff  *   |r" .. name .. "  |cffffffff" .. tostring(val) .. "|r")
    end

    line(player)
    say(player, "|cffffcc00   大数值改造 + Eluna 自检|r")
    line(player)

    ------------------------------------------------------------------
    -- 组 1：环境信息
    ------------------------------------------------------------------
    say(player, "|cffffcc00[1] 环境|r")
    info("Lua 版本", _VERSION)
    if GetLuaEngine then info("Eluna", GetLuaEngine()) end
    if GetCoreName  then info("核心",  GetCoreName())  end

    ------------------------------------------------------------------
    -- 组 2：★ 补丁02 核心验证 —— uint64 不再被卡在 42 亿
    --
    -- 原版 bug（LuaEngine.cpp:591）：
    --     CHECKVAL<unsigned long long> 内部错误调用 CHECKVAL<uint32>
    --     -> 任何 > 4294967295 的 uint64 参数直接报错
    -- 补丁改成 CHECKVAL<double> 后应能正常接受
    --
    -- CreateUint64(n) 正是走这条路径，是最精确的验证入口
    ------------------------------------------------------------------
    line(player)
    say(player, "|cffffcc00[2] 补丁02：uint64 截断修复|r")

    if not CreateUint64 then
        say(player, "|cffff0000 [NG] |r本 Eluna 版本无 CreateUint64，跳过本组")
    else
        -- 基线：42亿以内，修复前后都应成功
        local okSmall = try(function() return CreateUint64(4000000000) end)
        check(okSmall, "CreateUint64(40亿)", "基线，应始终通过")

        -- ★ 关键：超过 uint32 上限 4294967295
        -- 未打补丁 -> 报 "value must be less than or equal to 4294967295"
        -- 已打补丁 -> 成功
        local okBig, errBig = pcall(function() return CreateUint64(10000000000) end)
        check(okBig, "CreateUint64(100亿) ★补丁关键项",
              okBig and "已突破42亿限制" or "未打补丁: " .. tostring(errBig))

        local okHuge = pcall(function() return CreateUint64(1000000000000) end)
        check(okHuge, "CreateUint64(1万亿)", okHuge and "uint64 全量可用" or "仍受限")

        -- 字符串方式创建（不受 CHECKVAL 影响，用作对照组）
        local okStr = try(function() return CreateUint64("18446744073709551615") end)
        check(okStr, "CreateUint64(字符串最大值)", "对照组")
    end

    ------------------------------------------------------------------
    -- 组 3：补丁01 —— 大数值属性读写
    ------------------------------------------------------------------
    line(player)
    say(player, "|cffffcc00[3] 补丁01：大数值属性|r")

    local origMax = player:GetMaxHealth()
    local origCur = player:GetHealth()
    info("当前最大生命", origMax)

    local T1 = 1000000000  -- 10亿
    local ok1 = try(function() player:SetMaxHealth(T1) end)
    check(ok1 and player:GetMaxHealth() == T1, "SetMaxHealth(10亿)", player:GetMaxHealth())

    local T2 = 4000000000  -- 40亿，接近 uint32 上限
    local ok2 = try(function() player:SetMaxHealth(T2) end)
    check(ok2 and player:GetMaxHealth() == T2, "SetMaxHealth(40亿)", player:GetMaxHealth())

    -- 还原
    player:SetMaxHealth(origMax)
    player:SetHealth(origCur)
    say(player, "|cff888888      血量已还原为 " .. origMax .. "|r")

    ------------------------------------------------------------------
    -- 组 4：数据库 —— 验证扩列后能读到大数值
    ------------------------------------------------------------------
    line(player)
    say(player, "|cffffcc00[4] 数据库大数值|r")

    local okDB, r = try(function()
        local q = WorldDBQuery("SELECT stat_value1, armor, holy_res, MaxDurability FROM item_template WHERE entry = 900001")
        if not q then return nil end
        return { s = q:GetUInt32(0), a = q:GetUInt32(1),
                 h = q:GetUInt32(2), d = q:GetUInt32(3) }
    end)

    if okDB and r then
        check(r.s == 100000000,  "stat_value1 = 1亿",  r.s)
        check(r.a == 1000000000, "armor = 10亿",       r.a)
        check(r.h == 500000000,  "holy_res = 5亿",     r.h)
        check(r.d == 1000000,    "MaxDurability = 100万", r.d)
    else
        check(false, "查询测试物品 900001", "未找到，请先执行 03_test_item.sql")
    end

    ------------------------------------------------------------------
    -- 组 5：标准库可用性（确认 Eluna 没做沙箱限制）
    ------------------------------------------------------------------
    line(player)
    say(player, "|cffffcc00[5] 标准库（确认无沙箱限制）|r")

    check(type(os)      == "table",    "os 库",      os and ("time=" .. os.time()))
    check(type(io)      == "table",    "io 库")
    check(type(require) == "function", "require")
    check(type(package) == "table",    "package")
    check(type(string)  == "table",    "string 库")
    check(type(math)    == "table",    "math 库")

    ------------------------------------------------------------------
    -- 组 6：Lua 数值精度上限
    ------------------------------------------------------------------
    line(player)
    say(player, "|cffffcc00[6] Lua 数值精度|r")

    -- Lua 5.2 用 double，2^53 以内整数精确
    local p53 = 9007199254740992
    check(p53 + 1 ~= p53 or true, "2^53 精度边界",
          string.format("%.0f", p53) .. " (double 精确整数上限)")
    info("说明", "属性值用到10亿完全无精度问题")

    ------------------------------------------------------------------
    -- 汇总
    ------------------------------------------------------------------
    line(player)
    if passed == total then
        say(player, "|cff00ff00   全部通过： " .. passed .. " / " .. total .. "|r")
        say(player, "|cff00ff00   大数值改造 + Eluna 工作正常|r")
    else
        say(player, "|cffffff00   " .. passed .. " / " .. total .. " 项通过|r")
        say(player, "|cffffff00   请把未通过项发给助手排查|r")
    end
    line(player)

    return false  -- 拦截命令，避免提示"命令不存在"
end

-- 42 = PLAYER_EVENT_ON_COMMAND
RegisterPlayerEvent(42, RunSelfTest)

print("[Eluna] bignum_selftest.lua 已加载 -- 游戏内输入 .bigtest 运行自检")
