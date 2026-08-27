-- G17DragonBar: DF-style dragonriding skill panel for WoW 3.3.5a (zhCN).
--
-- While the rider is mounted on a G17 dragon the server temporarily teaches
-- the dragonriding skills to the PLAYER (cs_dragonriding.cpp GrantSkillPanel,
-- B3-R3).  This addon renders those known spells as a standalone 11-button
-- secure bar: 6 movement skills + the 5 combat skills of the current mount
-- archetype (auto-detected by scanning which of 990000-990024 are known).
--
-- The server explicitly allows rider casts while seated (carriers carry
-- Attributes 0x100 / SPELL_ATTR0_CASTABLE_WHILE_MOUNTED; see
-- SpellInfo::CheckVehicle in the fork).  The vehicle action bar keeps
-- working as before - this bar is additive, no default UI is modified.
--
-- No keybindings in v1; drag with the left mouse (out of combat).

local MOVEMENT = { 990026, 990027, 55215, 52197, 990028, 52226 } -- 拉升/俯冲/推进/冲刺/制动/着陆
local COMBAT_BASE, COMBAT_COUNT = 990000, 25
local BUTTON_SIZE, PADDING = 34, 5
local MAX_BUTTONS = 12

local apidebugf = nil

local function sanitizePos()
    if type(G17DragonBarDB) ~= "table" then G17DragonBarDB = {} end
    if not G17DragonBarDB.point then
        G17DragonBarDB = { point = "BOTTOM", x = 0, y = 150 }
    end
end

local frame = CreateFrame("Frame", "G17DragonBarFrame", UIParent)
frame:Hide()
frame:SetFrameStrata("MEDIUM")
frame:SetClampedToScreen(true)

local buttons = {}
local powerText

local function IsKnown(id)
    if IsSpellKnown then return IsSpellKnown(id) end
    return false
end

local function SetButtonSpell(button, spellId)
    local name, _, icon = GetSpellInfo(spellId)
    if not name then
        button:Hide()
        button.spellId = nil
        return
    end
    button.spellId = spellId
    button.spellName = name
    button:SetAttribute("type", "spell")
    button:SetAttribute("spell", name)
    local iconTex = getglobal(button:GetName() .. "Icon")
    if iconTex then
        iconTex:SetTexture(icon)
        iconTex:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    end
    button:SetChecked(false)
    button:Show()
end

local function UpdateCooldowns()
    for _, b in ipairs(buttons) do
        if b:IsShown() and b.spellId then
            local start, duration, enable = GetSpellCooldown(b.spellId)
            local cd = getglobal(b:GetName() .. "Cooldown")
            if cd then
                if start and start > 0 and duration and duration > 0 then
                    CooldownFrame_SetTimer(cd, start, duration, enable)
                else
                    CooldownFrame_SetTimer(cd, 0, 0, 0)
                end
            end
        end
    end
end

local function LayoutButtons(count)
    local width = count * BUTTON_SIZE + (count + 1) * PADDING
    frame:SetWidth(width)
    frame:SetHeight(BUTTON_SIZE + 2 * PADDING + 14)
    for i = 1, MAX_BUTTONS do
        local b = buttons[i]
        if i <= count then
            b:ClearAllPoints()
            b:SetPoint("LEFT", frame, "LEFT", PADDING + (i - 1) * (BUTTON_SIZE + PADDING), 7)
            b:Show()
        else
            b:Hide()
        end
    end
end

local function Refresh()
    if InCombatLockdown() then return end
    if not IsKnown(990026) then
        frame:Hide()
        return
    end

    -- movement skills first
    local list = {}
    for _, id in ipairs(MOVEMENT) do list[#list + 1] = id end
    -- then whichever combat skills are currently known (the archetype set)
    for i = 0, COMBAT_COUNT - 1 do
        local id = COMBAT_BASE + i
        if IsKnown(id) then list[#list + 1] = id end
        if #list >= MAX_BUTTONS then break end
    end

    for i = 1, MAX_BUTTONS do
        local b = buttons[i]
        if list[i] then
            SetButtonSpell(b, list[i])
        else
            b.spellId = nil
            b:Hide()
        end
    end
    LayoutButtons(#list)
    frame:Show()
    UpdateCooldowns()
end

local function UpdatePower()
    if not frame:IsShown() then return end
    if not powerText then return end
    local value, max = 0, 0
    -- POWER TYPE 3 = ENERGY (WotLK enum); the dragon is the player's vehicle
    pcall(function()
        if UnitExists("vehicle") then
            value = UnitPower("vehicle", 3) or 0
            max = UnitPowerMax("vehicle", 3) or 0
        end
    end)
    powerText:SetText(string.format("龙能量 %d/%d", value, max))
end

-- construction
for i = 1, MAX_BUTTONS do
    local name = "G17DragonBarButton" .. i
    local b = CreateFrame("CheckButton", name, frame, "ActionButtonTemplate, SecureActionButtonTemplate")
    b:SetWidth(BUTTON_SIZE)
    b:SetHeight(BUTTON_SIZE)
    b:RegisterForClicks("AnyUp")
    buttons[i] = b
end

powerText = frame:CreateFontString("G17DragonBarPowerText", "OVERLAY", "GameFontNormalSmall")
powerText:SetPoint("TOP", frame, "TOP", 0, 0)
powerText:SetTextColor(0.6, 0.95, 1.0)

local bg = frame:CreateTexture("G17DragonBarBG", "BACKGROUND")
bg:SetTexture(0, 0, 0, 0.35)
bg:SetAllPoints(frame)

-- dragging (out of combat only; secure frame rules)
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", function(self)
    if not InCombatLockdown() then self:StartMoving() end
end)
frame:SetScript("OnDragStop", function(self)
    self:StopMovingOrSizing()
    local point, _, _, x, y = self:GetPoint(1)
    if point then
        G17DragonBarDB.point = point
        G17DragonBarDB.x = x
        G17DragonBarDB.y = y
    end
end)

-- events
frame:RegisterEvent("PLAYER_ENTERING_WORLD")
frame:RegisterEvent("UNIT_ENTERED_VEHICLE")
frame:RegisterEvent("UNIT_EXITED_VEHICLE")
frame:RegisterEvent("VEHICLE_UPDATE")
frame:RegisterEvent("SPELL_UPDATE_COOLDOWN")
frame:RegisterEvent("ACTIONBAR_UPDATE_COOLDOWN")
frame:RegisterEvent("UNIT_POWER")
frame:SetScript("OnEvent", function(self, event, unit)
    if event == "UNIT_POWER" then
        if unit == "vehicle" then UpdatePower() end
        return
    end
    if event == "SPELL_UPDATE_COOLDOWN" or event == "ACTIONBAR_UPDATE_COOLDOWN" then
        UpdateCooldowns()
        return
    end
    Refresh()
    UpdatePower()
end)

-- throttled power refresh (UNIT_POWER arg coverage varies)
local elapsed = 0
frame:SetScript("OnUpdate", function(self, diff)
    elapsed = elapsed + diff
    if elapsed >= 0.25 then
        elapsed = 0
        UpdatePower()
    end
end)

-- restore position
sanitizePos()
frame:ClearAllPoints()
pcall(function()
    frame:SetPoint(G17DragonBarDB.point, UIParent, G17DragonBarDB.point, G17DragonBarDB.x or 0, G17DragonBarDB.y or 150)
end)

SLASH_G17DRAGONBAR1 = "/g17bar"
SlashCmdList["G17DRAGONBAR"] = function(msg)
    msg = (msg or ""):lower()
    if msg == "reset" then
        G17DragonBarDB = { point = "BOTTOM", x = 0, y = 150 }
        frame:ClearAllPoints()
        frame:SetPoint("BOTTOM", UIParent, "BOTTOM", 0, 150)
        Refresh()
        print("|cff80dfffG17 DragonBar|r 位置已重置。")
    elseif msg == "refresh" then
        Refresh()
        print("|cff80dfffG17 DragonBar|r 已刷新。")
    else
        print("|cff80dfffG17 DragonBar|r 用法: /g17bar reset | refresh")
    end
end

Refresh()
