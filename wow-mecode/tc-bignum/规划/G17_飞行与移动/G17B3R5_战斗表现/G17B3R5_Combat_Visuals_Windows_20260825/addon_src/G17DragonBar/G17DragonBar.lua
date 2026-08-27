-- G17DragonBar v3: DF-style dragonriding skill panel for WoW 3.3.5a (zhCN).
--
-- While the rider is mounted on a G17 dragon the server temporarily teaches
-- the dragonriding skills to the PLAYER (cs_dragonriding.cpp GrantSkillPanel).
-- This addon renders those known spells as a standalone 11-button secure bar:
-- 6 movement skills + the 5 combat skills of the current mount archetype.
--
-- v3 (investigation build - the v2 bar still never showed on the user's
-- client; this version TELLS us why):
--   * a load banner prints in chat on PLAYER_LOGIN - if the banner is absent
--     the addon is not loading at all (check the character screen addon list);
--   * detection: spellbook-name scan (GetSpellBookItemName/GetSpellName) with
--     an IsUsableSpell-by-name fallback - any one signal counts;
--   * if the player is riding (vehicle present) but detection fails, a
--     one-line diagnostic prints automatically per ride;
--   * /g17bar debug prints the entire detection chain;
--   * buttons are plain SecureActionButtonTemplate frames with hand-built
--     children, everything pcall-guarded.

local MOVEMENT = { 990026, 990027, 55215, 52197, 990028, 52226 } -- 拉升/俯冲/推进/冲刺/制动/着陆
local COMBAT_BASE, COMBAT_COUNT = 990000, 25
local MARKER_ID = 990026 -- the ascend skill doubles as the "riding a G17 dragon" marker
local BUTTON_SIZE, PADDING = 34, 5
local MAX_BUTTONS = 12

-- ---------------------------------------------------------------- helpers --

local function safecall(fn, ...)
    local ok, a, b, c = pcall(fn, ...)
    if ok then return a, b, c end
end

local function DBG(msg)
    print("|cff80dfffG17DragonBar|r|cff909090 [diag]|r " .. tostring(msg))
end

local spellNames = {}
local function SpellNameOf(id)
    if spellNames[id] ~= nil then return spellNames[id] end
    local name = safecall(GetSpellInfo, id)
    spellNames[id] = name or false
    return spellNames[id]
end

-- WotLK-safe "does the player know this spell": scan the spellbook by name,
-- with IsUsableSpell-by-name as an extra signal.
local bookCache, bookSize = nil, 0
local function RefreshBookCache()
    bookCache = {}
    bookSize = 0
    local i = 1
    while true do
        local name
        if GetSpellBookItemName then
            name = safecall(GetSpellBookItemName, i, BOOKTYPE_SPELL or "spell")
        end
        if not name and GetSpellName then
            name = safecall(GetSpellName, i, BOOKTYPE_SPELL or "spell")
        end
        if not name then return end
        bookCache[name] = true
        bookSize = bookSize + 1
        i = i + 1
        if i > 4000 then return end
    end
end

local function IsKnown(id)
    local name = SpellNameOf(id)
    if not name then return false end
    if IsSpellKnown then
        local ok, v = pcall(IsSpellKnown, id)
        if ok and v == true then return true end
    end
    if bookCache and bookCache[name] then return true end
    local usable = safecall(IsUsableSpell, name)
    if usable == true then return true end
    return false
end

-- ------------------------------------------------------------------ frame --

local frame = CreateFrame("Frame", "G17DragonBarFrame", UIParent)
frame:Hide()
frame:SetFrameStrata("MEDIUM")
frame:SetClampedToScreen(true)

local buttons = {}
local powerText
local rideDiagShown = false

local function BuildButton(i)
    local name = "G17DragonBarButton" .. i
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

    b:SetScript("OnEnter", function(self)
        if self.spellId then
            safecall(function()
                GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
                GameTooltip:SetSpell(self.spellId, BOOKTYPE_SPELL or "spell")
                GameTooltip:Show()
            end)
        end
    end)
    b:SetScript("OnLeave", function(self)
        safecall(function() GameTooltip:Hide() end)
    end)

    buttons[i] = b
    return b
end

local function SetButtonSpell(button, spellId)
    local name = SpellNameOf(spellId)
    if not name then
        button.spellId = nil
        button:Hide()
        return
    end
    button.spellId = spellId
    safecall(button.SetAttribute, button, "type", "spell")
    safecall(button.SetAttribute, button, "spell", name)
    local _, _, icon = GetSpellInfo(spellId)
    if icon and button.iconTex then button.iconTex:SetTexture(icon) end
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

local function LayoutButtons(count)
    local width = count * BUTTON_SIZE + (count + 1) * PADDING
    frame:SetWidth(width)
    frame:SetHeight(BUTTON_SIZE + 2 * PADDING + 16)
    for i = 1, MAX_BUTTONS do
        local b = buttons[i]
        if i <= count then
            b:ClearAllPoints()
            b:SetPoint("LEFT", frame, "LEFT", PADDING + (i - 1) * (BUTTON_SIZE + PADDING), 9)
            b:Show()
        else
            b:Hide()
        end
    end
end

local function UpdatePower()
    if not frame:IsShown() or not powerText then return end
    local value, max = 0, 0
    safecall(function()
        if UnitExists("vehicle") then
            value = UnitPower("vehicle", 3) or 0   -- 3 = ENERGY (WotLK enum)
            max = UnitPowerMax("vehicle", 3) or 0
        end
    end)
    powerText:SetText(string.format("龙能量 %d/%d", value, max))
end

local function Diagnose(auto)
    local markerName = SpellNameOf(MARKER_ID)
    RefreshBookCache()
    local bookHit = markerName and bookCache[markerName]
    local usable = markerName and safecall(IsUsableSpell, markerName)
    local inVehicle = safecall(UnitInVehicle, "player")
    local hasVehicleUI = safecall(UnitHasVehicleUI, "player")
    DBG(string.format("%s 标记法术%d=%s | 技能书扫描%d条%s | IsUsableSpell=%s | 在载具=%s 载具UI=%s | API: GSBIN=%s GSN=%s ISK=%s",
        auto and "自动诊断" or "手动诊断",
        MARKER_ID, tostring(markerName),
        bookSize, bookHit and "(命中)" or "(未命中)",
        tostring(usable),
        tostring(inVehicle), tostring(hasVehicleUI),
        tostring(GetSpellBookItemName ~= nil), tostring(GetSpellName ~= nil),
        tostring(IsSpellKnown ~= nil)))
end

local function Refresh()
    if InCombatLockdown() then return end
    RefreshBookCache()
    local riding = false
    safecall(function() riding = UnitInVehicle("player") or UnitHasVehicleUI("player") end)
    if not IsKnown(MARKER_ID) then
        frame:Hide()
        if riding and not rideDiagShown then
            rideDiagShown = true
            Diagnose(true)
            DBG("检测未通过：技能条已隐藏。请把上面这行发回给开发。")
        end
        return
    end
    rideDiagShown = false

    local list = {}
    for _, id in ipairs(MOVEMENT) do list[#list + 1] = id end
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
    UpdatePower()
end

-- -------------------------------------------------------------- construct --

for i = 1, MAX_BUTTONS do BuildButton(i) end

powerText = frame:CreateFontString("G17DragonBarPowerText", "OVERLAY", "GameFontNormalSmall")
powerText:SetPoint("TOP", frame, "TOP", 0, 0)
powerText:SetTextColor(0.6, 0.95, 1.0)

local title = frame:CreateFontString("G17DragonBarTitle", "OVERLAY", "GameFontNormalSmall")
title:SetPoint("BOTTOM", frame, "BOTTOM", 0, 0)
title:SetTextColor(0.8, 0.9, 1.0)
title:SetText("G17 御龙技能")

local bgTex = frame:CreateTexture("G17DragonBarBG", "BACKGROUND")
bgTex:SetTexture(0, 0, 0, 0.35)
bgTex:SetAllPoints(frame)

-- dragging (out of combat only; secure frame rules)
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
            G17DragonBarDB.point = point
            G17DragonBarDB.x = x
            G17DragonBarDB.y = y
        end
    end)
end)

-- events
frame:RegisterEvent("PLAYER_ENTERING_WORLD")
frame:RegisterEvent("PLAYER_LOGIN")
frame:RegisterEvent("UNIT_ENTERED_VEHICLE")
frame:RegisterEvent("UNIT_EXITED_VEHICLE")
frame:RegisterEvent("VEHICLE_UPDATE")
frame:RegisterEvent("SPELL_UPDATE_COOLDOWN")
frame:RegisterEvent("ACTIONBAR_UPDATE_USABLE")
frame:RegisterEvent("LEARNED_SPELL_IN_TAB")
frame:SetScript("OnEvent", function(_, event)
    if event == "PLAYER_LOGIN" then
        print("|cff80dfffG17|r DragonBar v3 已加载（若骑龙时技能条未出现，输入 /g17bar debug 并回传输出）")
        return
    end
    if event == "SPELL_UPDATE_COOLDOWN" or event == "ACTIONBAR_UPDATE_USABLE" then
        UpdateCooldowns()
        return
    end
    safecall(Refresh)
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

-- saved position
G17DragonBarDB = G17DragonBarDB or { point = "BOTTOM", x = 0, y = 150 }
safecall(function()
    frame:ClearAllPoints()
    frame:SetPoint(G17DragonBarDB.point or "BOTTOM", UIParent,
        G17DragonBarDB.point or "BOTTOM", G17DragonBarDB.x or 0, G17DragonBarDB.y or 150)
end)

SLASH_G17DRAGONBAR1 = "/g17bar"
SlashCmdList["G17DRAGONBAR"] = function(msg)
    msg = (msg or ""):lower()
    if msg == "reset" then
        G17DragonBarDB = { point = "BOTTOM", x = 0, y = 150 }
        safecall(function()
            frame:ClearAllPoints()
            frame:SetPoint("BOTTOM", UIParent, "BOTTOM", 0, 150)
        end)
        safecall(Refresh)
        print("|cff80dfffG17 DragonBar|r 位置已重置。")
    elseif msg == "refresh" then
        safecall(Refresh)
        print("|cff80dfffG17 DragonBar|r 已刷新。")
    elseif msg == "debug" then
        Diagnose(false)
        safecall(Refresh)
    else
        print("|cff80dfffG17 DragonBar|r 用法: /g17bar reset | refresh | debug")
    end
end

safecall(Refresh)
