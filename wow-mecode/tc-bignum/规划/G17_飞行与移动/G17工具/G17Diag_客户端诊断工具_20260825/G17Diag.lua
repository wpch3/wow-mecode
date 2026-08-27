-- G17Diag: client-side diagnostic tool for the G17 dragonriding system.
-- Usage: /g17diag          (full report)
--        /g17diag vehicle  (vehicle only)
--        /g17diag spells   (G17 spells only)
--        /g17diag book     (spellbook dump)
--
-- Copy the chat output (or screenshot) and send it back.

local MOVEMENT = { 990026, 990027, 55215, 52197, 990028, 52226 }
local COMBAT = {}
for i = 0, 24 do COMBAT[#COMBAT + 1] = 990000 + i end
local ALL = {}
for _, id in ipairs(MOVEMENT) do ALL[#ALL + 1] = id end
for _, id in ipairs(COMBAT) do ALL[#ALL + 1] = id end

local function P(msg)
    print("|cffffd700[G17Diag]|r " .. tostring(msg))
end

local function P2(msg)
    print("  " .. tostring(msg))
end

local function sep()
    print("  " .. string.rep("-", 60))
end

local function safe(fn, ...)
    local ok, a, b, c = pcall(fn, ...)
    if ok then return a, b, c end
    return nil, nil, tostring(a)
end

-- ============================================================ report ====

local function ReportClient()
    P("== 客户端信息 ==")
    sep()
    P2("版本: " .. (GetBuildInfo and select(1, GetBuildInfo()) or "?"))
    P2("Build: " .. (GetBuildInfo and select(3, GetBuildInfo()) or "?"))
    P2("区域: " .. (GetLocale and GetLocale() or "?"))
    local f = GetCVar("locale")
    if f then P2("CVar locale: " .. f) end
    P2("分辨率: " .. (GetScreenWidth and math.floor(GetScreenWidth() * 100) .. "x" .. math.floor(GetScreenHeight() * 100) or "?"))
end

local function ReportVehicle()
    P("== 载具状态 ==")
    sep()
    local inVehicle = safe(UnitInVehicle, "player")
    local hasVehicleUI = safe(UnitHasVehicleUI, "player")
    P2("UnitInVehicle: " .. tostring(inVehicle))
    P2("UnitHasVehicleUI: " .. tostring(hasVehicleUI))

    if not safe(UnitExists, "vehicle") then
        P2("vehicle 单位不存在（未骑乘或非控制位）")
        return
    end

    P2("vehicle 名称: " .. tostring(safe(UnitName, "vehicle")))
    P2("vehicle GUID: " .. tostring(safe(UnitGUID, "vehicle")))
    P2("vehicle 生命: " .. tostring(safe(UnitHealth, "vehicle")) .. "/" .. tostring(safe(UnitHealthMax, "vehicle")))
    P2("vehicle 等级: " .. tostring(safe(UnitLevel, "vehicle")))
    P2("vehicle 类型: " .. tostring(safe(UnitCreatureType, "vehicle")))
    P2("vehicle 家族: " .. tostring(safe(UnitCreatureFamily, "vehicle")))

    -- power types (0=mana, 1=rage, 2=focus, 3=energy, 4=happiness)
    for pt = 0, 4 do
        local pow, max = safe(UnitPower, "vehicle", pt), safe(UnitPowerMax, "vehicle", pt)
        if max and max > 0 then
            P2("vehicle 能量类型" .. pt .. ": " .. tostring(pow) .. "/" .. tostring(max))
        end
    end

    -- buffs on the player
    P("== 玩家增益 ==")
    sep()
    for i = 1, 40 do
        local name, rank, icon, count, debuffType, duration, expirationTime, unitCaster,
              isStealable, shouldConsolidate, spellId = safe(UnitBuff, "player", i)
        if not name then break end
        P2(string.format("[%d] %s (ID:%d) 剩余%.0f秒", i, name, spellId or 0, (expirationTime or 0) - GetTime()))
    end

    -- vehicle buffs
    P("== 载具增益 ==")
    sep()
    for i = 1, 40 do
        local name, rank, icon, count, debuffType, duration, expirationTime, unitCaster,
              isStealable, shouldConsolidate, spellId = safe(UnitBuff, "vehicle", i)
        if not name then break end
        P2(string.format("[%d] %s (ID:%d) 剩余%.0f秒", i, name, spellId or 0, (expirationTime or 0) - GetTime()))
    end
end

local function ReportSpells()
    P("== G17 法术检查（全部29个）==")
    sep()
    P2("API 存在性:")
    P2("  GetSpellInfo: " .. tostring(GetSpellInfo ~= nil))
    P2("  GetSpellBookItemName: " .. tostring(GetSpellBookItemName ~= nil))
    P2("  GetSpellName: " .. tostring(GetSpellName ~= nil))
    P2("  IsSpellKnown: " .. tostring(IsSpellKnown ~= nil))
    P2("  IsUsableSpell: " .. tostring(IsUsableSpell ~= nil))
    P2("  GetSpellCooldown: " .. tostring(GetSpellCooldown ~= nil))
    P2("  GetSpellTabInfo: " .. tostring(GetSpellTabInfo ~= nil))
    sep()

    for _, id in ipairs(ALL) do
        local name, rank, icon = safe(GetSpellInfo, id)
        if name then
            local isk = "nil"
            if IsSpellKnown then
                local ok, v = pcall(IsSpellKnown, id)
                isk = tostring(v)
            end
            local usable, noMana = safe(IsUsableSpell, id)
            local start, duration, enable = safe(GetSpellCooldown, id)
            local cd = (start and start > 0 and duration and duration > 0)
                and string.format("%.0f秒", duration) or "无"
            P2(string.format("%d %s |图标=%s |IsKnown=%s |Usable=%s |CD=%s",
                id, name, icon and "有" or "无", isk, tostring(usable), cd))
        else
            P2(string.format("%d ??? |DBC无此法术!", id))
        end
    end
end

local function ReportBook()
    P("== 技能书扫描 ==")
    sep()

    -- tab info
    for tab = 1, 10 do
        local name, texture, offset, numSlots = safe(GetSpellTabInfo, tab)
        if not name then break end
        P2(string.format("标签%d: %s (offset=%d, slots=%d)", tab, name, offset or 0, numSlots or 0))
    end
    sep()

    -- scan all entries
    local count = 0
    local g17Found = {}
    for i = 1, 500 do
        local name, rank
        if GetSpellBookItemName then
            name, rank = safe(GetSpellBookItemName, i, BOOKTYPE_SPELL or "spell")
        end
        if not name and GetSpellName then
            name, rank = safe(GetSpellName, i, BOOKTYPE_SPELL or "spell")
        end
        if not name then break end
        count = count + 1
        -- check if it's one of our spells
        for _, id in ipairs(ALL) do
            local ourName = safe(GetSpellInfo, id)
            if ourName and name == ourName then
                g17Found[#g17Found + 1] = string.format("%d=%s(slot %d)", id, name, i)
            end
        end
    end
    P2("总共扫描到 " .. count .. " 条法术")
    if #g17Found > 0 then
        P2("找到 G17 法术:")
        for _, f in ipairs(g17Found) do P2("  " .. f) end
    else
        P2("未在法术书中找到任何 G17 法术")
        P2("(注意：'临时学习'的法术可能不出现在法术书标签中)")
    end
end

local function ReportActionBar()
    P("== 动作条状态 ==")
    sep()
    -- check if VehicleMenuBar exists and is visible
    if VehicleMenuBar then
        P2("VehicleMenuBar 存在")
        P2("VehicleMenuBar 可见: " .. tostring(VehicleMenuBar:IsVisible()))
        P2("VehicleMenuBar 显示: " .. tostring(VehicleMenuBar:IsShown()))
        if VehicleMenuBar:IsShown() then
            for i = 1, 8 do
                local btn = getglobal("VehicleMenuBarActionButton" .. i)
                if btn then
                    local icon = getglobal("VehicleMenuBarActionButton" .. i .. "Icon")
                    local texture = icon and icon:GetTexture() or "nil"
                    P2("  按钮" .. i .. ": icon=" .. tostring(texture))
                end
            end
        end
    else
        P2("VehicleMenuBar 不存在")
    end

    -- check player action bar visibility
    if MainMenuBar then
        P2("MainMenuBar 存在")
        P2("MainMenuBar 显示: " .. tostring(MainMenuBar:IsShown()))
    end

    -- current action bar page
    P2("当前动作条页: " .. tostring(GetCurrentActionBarPage and GetCurrentActionBarPage() or "?"))
end

local function ReportFull()
    P("====== G17 完整诊断报告 ======")
    P("时间: " .. date("%Y-%m-%d %H:%M:%S"))
    P("")
    ReportClient()
    P("")
    ReportVehicle()
    P("")
    ReportSpells()
    P("")
    ReportActionBar()
    P("")
    ReportBook()
    P("")
    P("====== 诊断完成，请截图或复制以上内容回传 ======")
end

-- ============================================================ slash ======

SLASH_G17DIAG1 = "/g17diag"
SLASH_G17DIAG2 = "/g17d"
SlashCmdList["G17DIAG"] = function(msg)
    msg = (msg or ""):lower():trim()
    if msg == "vehicle" or msg == "v" then
        ReportVehicle()
    elseif msg == "spells" or msg == "s" then
        ReportSpells()
    elseif msg == "book" or msg == "b" then
        ReportBook()
    elseif msg == "bar" then
        ReportActionBar()
    else
        ReportFull()
    end
end

-- load banner
print("|cffffd700G17Diag|r 已加载。输入 /g17diag 输出完整诊断，/g17d vehicle 查看载具状态。")
