--[[
================================================================================
    Eluna 环境诊断  custom_diag.lua
================================================================================

    如果 02/03 还是不加载，放这个文件进去，重启后看控制台输出。
    它会报告 Eluna 里到底哪些 API 可用、哪些不可用。

    确认没问题后可以删掉这个文件。
================================================================================
]]

local function chk(name, v)
    print("[诊断] " .. name .. " = " .. tostring(v ~= nil))
end

print("========== Eluna 环境诊断 开始 ==========")

-- 标准库
chk("os 库",        os)
if os then
    chk("  os.date",  os.date)
    chk("  os.time",  os.time)
end
chk("string 库",    string)
chk("math 库",      math)
chk("table 库",     table)

-- Eluna 全局函数
chk("RegisterPlayerEvent", RegisterPlayerEvent)
chk("CreateLuaEvent",      CreateLuaEvent)
chk("SendWorldMessage",    SendWorldMessage)
chk("CharDBQuery",         CharDBQuery)
chk("CharDBExecute",       CharDBExecute)
chk("WorldDBQuery",        WorldDBQuery)

-- 数据库连通性
local q = CharDBQuery("SELECT DATE_FORMAT(CURDATE(),'%Y%m%d')")
if q then
    print("[诊断] 数据库查询正常，今天是 " .. tostring(q:GetString(0)))
else
    print("[诊断] 数据库查询返回 nil")
end

-- 表是否存在
local t = CharDBQuery("SELECT COUNT(*) FROM custom_daily_reward")
if t then
    print("[诊断] custom_daily_reward 表存在，当前 " ..
          tostring(t:GetUInt32(0)) .. " 条记录")
else
    print("[诊断] custom_daily_reward 表【不存在或查询失败】")
end

print("========== Eluna 环境诊断 结束 ==========")
