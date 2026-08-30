-- G17CombatBar v4: combat skill bar for WoW 3.3.5a (zhCN).
--
-- ARCHITECTURE CHANGE (v4, based on real client diagnostic data):
--   v1-v3 tried to detect G17 spells in the player's spellbook. Real diagnostic
--   proved this CANNOT work: LearnSpell() in 3.3.5 only registers server-side;
--   the client reports IsKnown=false for ALL 29 G17 spells and the spellbook
--   scan (210 entries) finds none of them.
--
--   The vehicle action bar (6 buttons) DOES work perfectly - the vehicle casts
--   its own m_spells[] which the server sets via WriteMovementPage().
--
--   v4 detects the G17 dragon by VEHICLE STATE (power type 3 = energy with
--   max 100) and provides 5 COMBAT SKILL buttons as a supplement.
--   Vehicle bar shows movement; this bar shows combat.
--   Together: 6 movement + 5 combat = 11 skills visible simultaneously.
--
-- Casting: buttons use CastSpellByName() which sends the cast to the server.
--   Our SpellScript's CheckCast() resolves the dragon from the player and
--   validates (the spell has CASTABLE_WHILE_MOUNTED = 0x100).

local COMBAT_BASE = 990000
local BUTTON_SIZE, PADDING = 36, 6
local MAX_BUTTONS = 5

local frame

local function safecall(fn, ...)
    local ok, a, b, c = pcall(fn, ...)
    if ok then return a, b, c end
    return nil, nil, tostring(a)
end

local function DBG(msg)
    print("|cffff8c00G17|r CombatBar |cff909090[v4]|r " .. tostring(msg))
end

-- Detect if the player is riding a G17 dragon by vehicle power state.
local function IsOnG17Dragon()
    if not safecall(UnitExists, "vehicle") then return false end
    if not safecall(UnitInVehicle, "player") then return false end
    local maxEnergy = safecall(UnitPowerMax, "vehicle", 3)
    if maxEnergy and maxEnergy >= 90 and maxEnergy <= 110 then
        return true
    end
    return false
end

-- Detect archetype from vehicle creature type + name heuristics.
local function DetectArchetype()
    local vtype = safecall(UnitCreatureType, "vehicle") or ""
    local vname = safecall(UnitName, "vehicle") or ""

    if vname:find("机械") or vname:find("摩托") or vname:find("直升机")
       or vname:find("火箭") or vname:find("飞行器") then
        return 3 -- mechanical
    elseif vname:find("飞毯") or vname:find("扫帚") or vname:find("凤凰")
       or vname:find("魔法") or vname:find("虚空") then
        return 2 -- magic
    elseif vname:find("兽") or vname:find("马") or vname:find("狼")
       or vname:find("虎") or vname:find("熊") or vname:find("豹")
       or vtype == "野兽" then
        return 1 -- beast
    elseif vname:find("龙") or vtype == "龙类" then
        return 0 -- dragon
    end
    -- Default: dragon (most common archetype)
    return 0
end

-- ============================================================ buttons ====

local buttons = {}

local function CreateButton(i)
    local name = "G17CombatBarButton" .. i
    local b = CreateFrame("Button", name, frame, "SecureActionButtonTemplate")
    b:SetWidth(BUTTON_SIZE)
    b:SetHeight(BUTTON_SIZE)
    b:RegisterForClicks("AnyUp")

    local bg = b:CreateTexture(name .. "Background", "BACKGROUND")
    bg:SetTexture(0, 0, 0, 0.9)
    bg:SetAllPoints(b)

    local icon = b:CreateTexture(name .. "Icon", "ARTWORK")
    icon:SetAllPoints(b)
    icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)

    local border = b:CreateTexture(name .. "Border", "OVERLAY")
    border:SetTexture("Interface\\Buttons\\UI-Quickslot2")
    border:SetAllPoints(b)
    border:SetTexCoord(0.2, 0.8, 0.2, 0.8)

    local cd = CreateFrame("Cooldown", name .. "Cooldown", b, "CooldownFrameTemplate")
    cd:SetAllPoints(b)

    b.iconTex = icon
    b.cooldown = cd
    b.spellId = nil
    b.spellName = nil

    b:SetScript("OnEnter", function(self)
        if self.spellName then
            safecall(function()
                GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
                GameTooltip:AddLine(self.spellName, 0.8, 0.9, 1.0)
                GameTooltip:AddLine("点击施放（由坐骑施法）", 0.5, 0.5, 0.5)
                GameTooltip:Show()
            end)
        end
    end)
    b:SetScript("OnLeave", function(self)
        safecall(function() GameTooltip:Hide() end)
    end)

    -- Click: CastSpellByName sends to the server; our SpellScript handles it
    b:SetScript("OnClick", function(self, mouseButton)
        if self.spellName then
            local ok = pcall(CastSpellByName, self.spellName)
            if not ok then
                -- try with explicit target
                pcall(function() CastSpellByName(self.spellName, "target") end)
            end
        end
    end)

    buttons[i] = b
    return b
end

local function SetButtonSpell(button, spellId)
    local name, _, icon = GetSpellInfo(spellId)
    if not name then
        button.spellId = nil
        button.spellName = nil
        button:Hide()
        return
    end
    button.spellId = spellId
    button.spellName = name
    if icon and button.iconTex then
        button.iconTex:SetTexture(icon)
    end
    button:Show()
end

local function UpdateCooldowns()
    for _, b in ipairs(buttons) do
        if b:IsShown() and b.spellId and b.cooldown then
            local start, duration, enable = GetSpellCooldown(b.spellId)
            if start and duration then
                if start > 0 and duration > 0 then
                    safecall(CooldownFrame_SetTimer, b.cooldown, start, duration, enable)
                else
                    safecall(CooldownFrame_SetTimer, b.cooldown, 0, 0, 0)
                end
            end
        end
    end
end

-- ============================================================ layout ====

local function LayoutButtons(count)
    local width = count * BUTTON_SIZE + (count + 1) * PADDING
    frame:SetWidth(width)
    frame:SetHeight(BUTTON_SIZE + 2 * PADDING + 22)
    for i = 1, MAX_BUTTONS do
        local b = buttons[i]
        if i <= count then
            b:ClearAllPoints()
            b:SetPoint("LEFT", frame, "LEFT", PADDING + (i - 1) * (BUTTON_SIZE + PADDING), 12)
            b:Show()
        else
            b:Hide()
        end
    end
end

-- ============================================================ refresh ====

local function Refresh()
    if InCombatLockdown() then return end

    if not IsOnG17Dragon() then
        frame:Hide()
        return
    end

    local arch = DetectArchetype()
    local base = COMBAT_BASE + arch * 5

    for i = 1, MAX_BUTTONS do
        SetButtonSpell(buttons[i], base + i - 1)
    end

    LayoutButtons(MAX_BUTTONS)
    frame:Show()
    UpdateCooldowns()
end

-- ============================================================ frame ======

frame = CreateFrame("Frame", "G17CombatBarFrame", UIParent)
frame:Hide()
frame:SetFrameStrata("MEDIUM")
frame:SetClampedToScreen(true)

for i = 1, MAX_BUTTONS do CreateButton(i) end

local title = frame:CreateFontString("G17CombatBarTitle", "OVERLAY", "GameFontNormalSmall")
title:SetPoint("TOP", frame, "TOP", 0, 0)
title:SetTextColor(1.0, 0.7, 0.3)
title:SetText("战斗技能")

local powerText = frame:CreateFontString("G17CombatBarPowerText", "OVERLAY", "GameFontNormalSmall")
powerText:SetPoint("BOTTOM", frame, "BOTTOM", 0, 0)
powerText:SetTextColor(0.6, 0.95, 1.0)

local bgTex = frame:CreateTexture("G17CombatBarBG", "BACKGROUND")
bgTex:SetTexture(0, 0, 0, 0.35)
bgTex:SetAllPoints(frame)

local function UpdatePower()
    if not frame:IsShown() then return end
    local value, max = 0, 0
    safecall(function()
        if UnitExists("vehicle") then
            value = UnitPower("vehicle", 3) or 0
            max = UnitPowerMax("vehicle", 3) or 0
        end
    end)
    powerText:SetText(string.format("龙能量 %d/%d", value, max))
end

-- dragging
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", function(self)
    safecall(function()
        if not InCombatLockdown() then self:StartMoving() end
    end)
end)
frame:SetScript("OnDragStop", function(self)
    safecall(function()
        self:StopMovingOrSizing()
        local point, _, _, x, y = self:GetPoint(1)
        if point then
            G17CombatBarDB = G17CombatBarDB or {}
            G17CombatBarDB.point = point
            G17CombatBarDB.x = x
            G17CombatBarDB.y = y
        end
    end)
end)

-- events
frame:RegisterEvent("PLAYER_ENTERING_WORLD")
frame:RegisterEvent("UNIT_ENTERED_VEHICLE")
frame:RegisterEvent("UNIT_EXITED_VEHICLE")
frame:RegisterEvent("VEHICLE_UPDATE")
frame:RegisterEvent("SPELL_UPDATE_COOLDOWN")
frame:RegisterEvent("UNIT_POWER")
frame:SetScript("OnEvent", function(_, event, unit)
    if event == "SPELL_UPDATE_COOLDOWN" then
        UpdateCooldowns()
        return
    end
    if event == "UNIT_POWER" then
        if unit == "vehicle" then UpdatePower() end
        return
    end
    safecall(Refresh)
    UpdatePower()
end)

-- throttled power refresh
do
    local elapsed = 0
    frame:SetScript("OnUpdate", function(_, diff)
        elapsed = elapsed + diff
        if elapsed >= 0.25 then
            elapsed = 0
            safecall(UpdatePower)
        end
    end)
end

-- position (default: above the vehicle bar)
G17CombatBarDB = G17CombatBarDB or { point = "BOTTOM", x = 0, y = 100 }
safecall(function()
    frame:ClearAllPoints()
    frame:SetPoint(G17CombatBarDB.point or "BOTTOM", UIParent,
        G17CombatBarDB.point or "BOTTOM", G17CombatBarDB.x or 0, G17CombatBarDB.y or 100)
end)

-- ============================================================ slash ======

SLASH_G17COMBATBAR1 = "/g17combat"
SLASH_G17COMBATBAR2 = "/g17c"
SlashCmdList["G17COMBATBAR"] = function(msg)
    msg = (msg or ""):lower():match("^%s*(.-)%s*$")
    if msg == "reset" then
        G17CombatBarDB = { point = "BOTTOM", x = 0, y = 100 }
        safecall(function()
            frame:ClearAllPoints()
            frame:SetPoint("BOTTOM", UIParent, "BOTTOM", 0, 100)
        end)
        safecall(Refresh)
        print("|cffff8c00G17|r CombatBar 位置已重置。")
    elseif msg == "debug" then
        DBG("=== v4 诊断 ===")
        DBG("在载具: " .. tostring(safecall(UnitInVehicle, "player")))
        DBG("载具存在: " .. tostring(safecall(UnitExists, "vehicle")))
        DBG("载具名称: " .. tostring(safecall(UnitName, "vehicle")))
        DBG("载具类型: " .. tostring(safecall(UnitCreatureType, "vehicle")))
        DBG("能量max(type3): " .. tostring(safecall(UnitPowerMax, "vehicle", 3)))
        DBG("IsOnG17Dragon: " .. tostring(IsOnG17Dragon()))
        DBG("DetectArchetype: " .. DetectArchetype() .. " (0=龙 1=兽 2=魔法 3=机械)")
        DBG("条可见: " .. tostring(frame:IsShown()))
        for i = 1, MAX_BUTTONS do
            local b = buttons[i]
            DBG(string.format("  按钮%d: spell=%s name=%s visible=%s",
                i, tostring(b.spellId), tostring(b.spellName), tostring(b:IsShown())))
        end
    elseif msg == "arch" then
        DBG("当前检测到类型: " .. DetectArchetype())
    else
        print("|cffff8c00G17|r CombatBar v4 用法: /g17c reset | debug | arch")
    end
end

-- load
print("|cffff8c00G17|r CombatBar v4 已加载。骑上御龙后自动显示战斗技能条。/g17c debug 诊断。")
safecall(Refresh)
