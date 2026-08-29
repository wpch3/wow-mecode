-- G17DragonRide v1 - 御龙术专属完整界面（全新自定义 UI，覆盖原版无效载具条）
--
-- 架构（三层保障，全部基于已验证机制）：
--   主技能行（6 原生槽）：BonusActionButtonTemplate —— 与原版载具条同机制，
--       100% 可用（原版条已证明）；显示服务端当前页（B3R8+: 5技能+切页@6）
--   扩展技能行（最多 6 格）：SecureActionButtonTemplate + type="spell" 按 ID
--       玩家施法（CastSpellByID 安全路径）。配 B3R9 服务端后全部技能可走此路，
--       彻底摆脱 6 格限制；默认 [制动990028]，可 /g17ride add 配置任意技能
--   原版无效 UI 覆盖：默认隐藏原版载具条 6 个动作按钮（保留俯仰/离开等有效控件），
--       本界面提供能量条+页面指示+冷却圈+按键
--
-- 施法路径遥测（自动判定"玩家施法"是否被客户端放行）：
--   点击扩展行按钮后 0.5 秒内没有 UNIT_SPELLCAST_SENT → 判定客户端拦截，
--   自动折叠扩展行并提示（/g17ride forcextra 可强制重新启用）
--
-- 冷却：CLEU SPELL_CAST_SUCCESS 跟踪（成功才显示，无幻影；4/6/20/10/60s+切页1s）
-- 激活条件：御龙载具（能量类型3+上限≈100）——任务坐骑零影响
-- 命令：/g17ride unlock|lock|scale N|hideblizz on|off|add <1-6> <法术ID>|
--       del <1-6>|forcextra|test|status|reset

local TOCVERSION = "v1_full_ui"
local NUM_NATIVE = 6
local NUM_EXTRA = 6
local BUTTON_SIZE = 44
local GAP = 6

local db
local native = {}
local extra = {}
local onDragon = false
local updateTimer = 0
local bindFailed = false
local pathBlocked = false
local pendingClicks = {}   -- [spellId] = expiry
local warnShown = false

local SLOT_CD = { 4, 6, 20, 10, 60 }
local PAGE_SWITCH_CD = 1

local function SpellCooldownSeconds(spellId)
    if spellId >= 990000 and spellId <= 990024 then
        return SLOT_CD[((spellId - 990000) % 5) + 1]
    elseif spellId == 990025 then
        return PAGE_SWITCH_CD
    end
    return nil
end

local function safecall(func, ...)
    if type(func) ~= "function" then return nil end
    local ok, a, b, c = pcall(func, ...)
    if ok then return a, b, c end
    return nil, nil, tostring(a)
end

local function DBG(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cffff8c00G17|r DragonRide |cff909090["..TOCVERSION.."]|r "..tostring(msg))
end

-- ============ 御龙检测（v4 已验证） ============
local function IsOnG17Dragon()
    if not safecall(UnitExists, "vehicle") then return false end
    if not safecall(UnitInVehicle, "player") then return false end
    local maxEnergy = safecall(UnitPowerMax, "vehicle", 3)
    if not maxEnergy or maxEnergy < 90 or maxEnergy > 110 then return false end
    if db and db.strictName then
        local vname = safecall(UnitName, "vehicle") or ""
        if not (string.find(vname, "御空龙") or string.find(vname, "御龙")) then return false end
    end
    return true
end

-- ============ Bonus 槽工具（ActionButton.lua:140 同款公式） ============
local function BonusSlotAction(slot)
    local offset = GetBonusBarOffset()
    if not offset or offset <= 0 then offset = 1 end
    return slot + (6 + offset - 1) * 12
end

local function BonusSlotSpell(slot)
    local ok, spellType, id, subType, spellId = pcall(GetActionInfo, BonusSlotAction(slot))
    if ok and spellType == "spell" then return spellId or id end
    return nil
end

local function DetectPage()
    for i = 1, NUM_NATIVE do
        local sid = BonusSlotSpell(i)
        if sid then
            if sid == 55215 or sid == 52197 or sid == 52226 or sid == 990026 or sid == 990027 then
                return "移动页"
            elseif sid >= 990000 and sid <= 990024 then
                return "战斗页"
            end
        end
    end
    return "未知"
end

-- ============ 冷却显示（v5 已验证） ============
local function ShowCooldownForSpell(spellId, duration)
    if not duration or duration <= 0 then return end
    local start = GetTime()
    local targets = {}
    for i = 1, NUM_NATIVE do
        if native[i] then targets[#targets+1] = native[i] end
    end
    for i = 1, 6 do
        local b = _G["VehicleMenuBarActionButton"..i]
        if b then targets[#targets+1] = b end
    end
    for i = 1, NUM_EXTRA do
        if extra[i] and extra[i].g17spell == spellId then
            local cd = extra[i].cd
            if cd then safecall(CooldownFrame_SetTimer, cd, start, duration, 1) end
        end
    end
    for _, btn in ipairs(targets) do
        local ok, spellType, id, subType, actionSpellId = pcall(GetActionInfo, btn.action or 0)
        if ok and spellType == "spell" and actionSpellId == spellId then
            local cd = _G[btn:GetName().."Cooldown"]
            if cd then safecall(CooldownFrame_SetTimer, cd, start, duration, 1) end
        end
    end
end

-- ============ 扩展行（玩家施法按钮） ============
local function SpellName(spellId)
    local n = safecall(GetSpellInfo, spellId)
    return n or ("法术"..tostring(spellId))
end

local function UpdateExtraButton(i)
    local btn = extra[i]
    if not btn then return end
    local spellId = db.extras and db.extras[i]
    btn.g17spell = spellId
    if pathBlocked or not spellId then
        btn:Hide()
        return
    end
    btn:Show()
    if InCombatLockdown() then return end
    btn:SetAttribute("type", "spell")
    btn:SetAttribute("spell", spellId)
    local tex = safecall(GetSpellTexture, spellId)
    if tex and tex ~= "" then
        btn.icon:SetTexture(tex)
    else
        btn.icon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")
    end
end

local function UpdateExtraRow()
    for i = 1, NUM_EXTRA do UpdateExtraButton(i) end
end

-- ============ 原版 UI 覆盖 ============
local function SetBlizzVehicleButtonsHidden(hidden)
    for i = 1, 6 do
        local b = _G["VehicleMenuBarActionButton"..i]
        if b then
            if hidden then b:Hide() else safecall(ActionButton_Update, b) end
        end
    end
end

-- ============ 按键 ============
local EXTRA_KEYS = { "7", "8", "9", "0", "-", "=" }
local function ApplyKeyBindings()
    if InCombatLockdown() then
        if not bindFailed then
            DBG("战斗中无法绑定快捷键，脱战后重新上龙即可。")
            bindFailed = true
        end
        return
    end
    local frame = G17DragonRideFrame
    if not frame then return end
    for i = 1, NUM_NATIVE do
        SetOverrideBindingClick(frame, true, tostring(i), "G17RideNative"..i, "LeftButton")
    end
    for i = 1, NUM_EXTRA do
        if extra[i] and extra[i]:IsShown() then
            SetOverrideBindingClick(frame, true, EXTRA_KEYS[i], "G17RideExtra"..i, "LeftButton")
        end
    end
end

local function ClearKeyBindings()
    if InCombatLockdown() then return end
    local frame = G17DragonRideFrame
    if frame then ClearOverrideBindings(frame) end
end

-- ============ 主状态机 ============
local function UpdateBarState()
    local frame = G17DragonRideFrame
    if not frame then return end
    if onDragon then
        frame:Show()
        for i = 1, NUM_NATIVE do
            safecall(ActionButton_UpdateAction, native[i])
            safecall(ActionButton_Update, native[i])
        end
        UpdateExtraRow()
        if db.hideBlizz then SetBlizzVehicleButtonsHidden(true) end
        ApplyKeyBindings()
        local t = _G[frame:GetName().."Warn"]
        if t then
            t:SetText(pathBlocked and "玩家施法被客户端拦截：扩展行已折叠（/g17ride status 诊断）" or "")
        end
    else
        ClearKeyBindings()
        if db.hideBlizz then SetBlizzVehicleButtonsHidden(false) end
        frame:Hide()
    end
end

local function CheckVehicleState()
    local now = IsOnG17Dragon()
    if now ~= onDragon then
        onDragon = now
        if onDragon then
            DBG("御龙界面已激活：主行6格(原生) + 扩展行(玩家施法) + 能量条。")
        end
        UpdateBarState()
    end
end

-- ============ 能量/页面 ============
local function UpdateEnergy()
    local frame = G17DragonRideFrame
    if not frame or not onDragon then return end
    local power = safecall(UnitPower, "vehicle", 3) or 0
    local max = safecall(UnitPowerMax, "vehicle", 3) or 1
    local bar = _G[frame:GetName().."EnergyBar"]
    if bar then
        safecall(bar.SetMinMaxValues, bar, 0, max)
        safecall(bar.SetValue, bar, power)
    end
    local pt = _G[frame:GetName().."Page"]
    if pt then pt:SetText(string.format("%s  龙能 %d/%d", DetectPage(), power, max)) end
end

-- ============ 施法遥测 ============
local function CheckPendingClicks()
    local now = GetTime()
    for spellId, expiry in pairs(pendingClicks) do
        if now > expiry then
            pendingClicks[spellId] = nil
            if not pathBlocked then
                pathBlocked = true
                DBG("|cffff4040检测到玩家施法未被客户端发送（法术 "..tostring(spellId)
                    .."）。扩展行已折叠；主行6格不受影响。|r /g17ride status 查看诊断，/g17ride forcextra 强制重试。")
                UpdateExtraRow()
                UpdateBarState()
            end
        end
    end
end

-- ============ 事件 ============
local function OnEvent(self, event, ...)
    if event == "PLAYER_ENTERING_WORLD" or event == "UNIT_ENTERED_VEHICLE"
       or event == "UNIT_EXITED_VEHICLE" or event == "UNIT_DISPLAYPOWER"
       or event == "UPDATE_BONUS_ACTIONBAR" then
        CheckVehicleState()
        if event == "UPDATE_BONUS_ACTIONBAR" and onDragon then
            for i = 1, NUM_NATIVE do
                safecall(ActionButton_UpdateAction, native[i])
                safecall(ActionButton_Update, native[i])
            end
        end
    elseif event == "PLAYER_REGEN_ENABLED" then
        bindFailed = false
        if onDragon then
            ApplyKeyBindings()
            UpdateExtraRow()
        end
    elseif event == "UNIT_SPELLCAST_SENT" then
        local unit, spell, rank = ...
        if unit == "player" then
            for spellId in pairs(pendingClicks) do
                local n = safecall(GetSpellInfo, spellId)
                if n and spell == n then
                    pendingClicks[spellId] = nil
                    if pathBlocked then
                        pathBlocked = false
                        DBG("玩家施法路径恢复可用，扩展行已展开。")
                        UpdateExtraRow()
                    end
                end
            end
        end
    elseif event == "COMBAT_LOG_EVENT_UNFILTERED" then
        local timestamp, combatEvent, sourceGUID = select(1, ...)
        if combatEvent == "SPELL_CAST_SUCCESS" and sourceGUID then
            local playerGUID = UnitGUID("player")
            local vehicleGUID = safecall(UnitGUID, "vehicle")
            if sourceGUID == playerGUID or (vehicleGUID and sourceGUID == vehicleGUID) then
                local spellId = select(9, ...)
                if spellId then
                    local duration = SpellCooldownSeconds(spellId)
                    if duration then ShowCooldownForSpell(spellId, duration) end
                end
            end
        end
    end
end

local function OnUpdate(self, elapsed)
    updateTimer = updateTimer + elapsed
    if updateTimer < 0.2 then return end
    updateTimer = 0
    if onDragon then
        CheckVehicleState()
        UpdateEnergy()
        CheckPendingClicks()
    end
end

-- ============ 位置 ============
local function SavePosition()
    local frame = G17DragonRideFrame
    if not frame or not db then return end
    local point, _, relPoint, x, y = frame:GetPoint(1)
    db.point = point; db.relPoint = relPoint; db.x = x; db.y = y
end

local function RestorePosition()
    local frame = G17DragonRideFrame
    if not frame or not db then return end
    if db.point then
        frame:ClearAllPoints()
        frame:SetPoint(db.point, UIParent, db.relPoint or "BOTTOM", db.x or 0, db.y or 132)
    end
    if db.scale then frame:SetScale(db.scale) end
end

-- ============ 初始化 ============
function G17DragonRide_OnLoad(self)
    G17DragonRideDB = G17DragonRideDB or {
        point = nil, relPoint = nil, x = nil, y = nil,
        scale = 1.0, locked = true, hideBlizz = true, strictName = false,
        extras = { [1] = 990028 },  -- 默认扩展行第1格 = 制动
    }
    db = G17DragonRideDB

    -- 主技能行：原生 BonusActionButtonTemplate（100% 可用）
    for i = 1, NUM_NATIVE do
        local btn = CreateFrame("CheckButton", "G17RideNative"..i, self, "BonusActionButtonTemplate")
        btn:SetID(i)
        btn:SetWidth(BUTTON_SIZE)
        btn:SetHeight(BUTTON_SIZE)
        if i == 1 then
            btn:SetPoint("TOPLEFT", self, "TOPLEFT", 22, -46)
        else
            btn:SetPoint("LEFT", native[i-1], "RIGHT", GAP, 0)
        end
        safecall(ActionButton_UpdateAction, btn)
        safecall(ActionButton_Update, btn)
        native[i] = btn
    end

    -- 扩展技能行：SecureActionButtonTemplate 玩家施法按钮（自绘图标/冷却）
    for i = 1, NUM_EXTRA do
        local btn = CreateFrame("Button", "G17RideExtra"..i, self, "SecureActionButtonTemplate")
        btn:SetWidth(BUTTON_SIZE)
        btn:SetHeight(BUTTON_SIZE)
        if i == 1 then
            btn:SetPoint("TOPLEFT", self, "TOPLEFT", 22, -100)
        else
            btn:SetPoint("LEFT", extra[i-1], "RIGHT", GAP, 0)
        end
        btn:RegisterForClicks("AnyUp")
        local icon = btn:CreateTexture(nil, "BACKGROUND")
        icon:SetAllPoints(btn)
        icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
        btn.icon = icon
        local cd = CreateFrame("Cooldown", "G17RideExtra"..i.."Cooldown", btn, "CooldownFrameTemplate")
        cd:SetAllPoints(btn)
        btn.cd = cd
        local border = btn:CreateTexture(nil, "OVERLAY")
        border:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
        border:SetBlendMode("ADD")
        border:SetAllPoints(btn)
        local hotkey = btn:CreateFontString(nil, "OVERLAY", "NumberFontNormalSmallGray")
        hotkey:SetPoint("TOPLEFT", btn, "TOPLEFT", -2, -2)
        hotkey:SetText(EXTRA_KEYS[i])
        btn:SetScript("OnEnter", function(b)
            if not b.g17spell then return end
            GameTooltip:SetOwner(b, "ANCHOR_TOP")
            local ok = pcall(GameTooltip.SetHyperlink, GameTooltip, "spell:"..tostring(b.g17spell))
            if not ok then GameTooltip:Hide() end
        end)
        btn:SetScript("OnLeave", function() GameTooltip:Hide() end)
        btn:SetScript("PostClick", function(b)
            if b.g17spell then
                pendingClicks[b.g17spell] = GetTime() + 0.5
            end
        end)
        btn:Hide()
        extra[i] = btn
    end

    RestorePosition()
    self:RegisterEvent("PLAYER_ENTERING_WORLD")
    self:RegisterEvent("UNIT_ENTERED_VEHICLE")
    self:RegisterEvent("UNIT_EXITED_VEHICLE")
    self:RegisterEvent("UNIT_DISPLAYPOWER")
    self:RegisterEvent("UPDATE_BONUS_ACTIONBAR")
    self:RegisterEvent("PLAYER_REGEN_ENABLED")
    self:RegisterEvent("COMBAT_LOG_EVENT_UNFILTERED")
    self:RegisterEvent("UNIT_SPELLCAST_SENT")
    self:SetMovable(true)
    self:RegisterForDrag("LeftButton")
    DBG("已加载。上龙自动激活；/g17ride 查看命令。")
end

function G17DragonRide_OnEvent(self, event, ...)
    OnEvent(self, event, ...)
end

function G17DragonRide_OnUpdate(self, elapsed)
    OnUpdate(self, elapsed)
end

G17DragonRideFrame:SetScript("OnDragStart", function(self)
    if db and not db.locked and not InCombatLockdown() then
        self:StartMoving()
    end
end)
G17DragonRideFrame:SetScript("OnDragStop", function(self)
    self:StopMovingOrSizing()
    SavePosition()
end)

-- ============ 命令 ============
SLASH_G17DRAGONRIDE1 = "/g17ride"
SLASH_G17DRAGONRIDE2 = "/g17dragonride"
SlashCmdList["G17DRAGONRIDE"] = function(msg)
    local cmd, arg = string.match((msg or ""):gsub("%s+", " "), "^(%S*)%s*(.-)$")
    cmd = string.lower(cmd or "")
    local frame = G17DragonRideFrame
    if cmd == "unlock" then
        db.locked = false
        DBG("已解锁，可拖动；/g17ride lock 锁定。")
    elseif cmd == "lock" then
        db.locked = true
        SavePosition()
        DBG("已锁定并保存。")
    elseif cmd == "scale" then
        local s = tonumber(arg)
        if s and s >= 0.5 and s <= 2.5 then
            db.scale = s
            frame:SetScale(s)
            DBG("缩放 "..s)
        else
            DBG("用法: /g17ride scale 0.5~2.5")
        end
    elseif cmd == "hideblizz" then
        db.hideBlizz = (arg ~= "off")
        if onDragon then SetBlizzVehicleButtonsHidden(db.hideBlizz) end
        DBG("覆盖原版载具条按钮: "..tostring(db.hideBlizz))
    elseif cmd == "strict" then
        db.strictName = (arg ~= "off")
        DBG("严格名称匹配: "..tostring(db.strictName))
    elseif cmd == "add" then
        local posStr, spellStr = string.match(arg or "", "^(%d+)%s+(%d+)$")
        local pos, spellId = tonumber(posStr), tonumber(spellStr)
        if pos and spellId and pos >= 1 and pos <= NUM_EXTRA and spellId >= 990000 and spellId <= 990028 then
            db.extras = db.extras or {}
            db.extras[pos] = spellId
            pathBlocked = false
            UpdateExtraRow()
            DBG(string.format("扩展行第 %d 格 = %s(%d)。", pos, SpellName(spellId), spellId))
        else
            DBG("用法: /g17ride add <1-6> <990000-990028>")
        end
    elseif cmd == "del" then
        local pos = tonumber(arg)
        if pos and pos >= 1 and pos <= NUM_EXTRA and db.extras then
            db.extras[pos] = nil
            UpdateExtraRow()
            DBG("扩展行第 "..pos.." 格已清空。")
        else
            DBG("用法: /g17ride del <1-6>")
        end
    elseif cmd == "forcextra" then
        pathBlocked = false
        UpdateExtraRow()
        UpdateBarState()
        DBG("已强制重新展开扩展行（点击后自动检测是否被拦截）。")
    elseif cmd == "test" then
        for i = 1, NUM_NATIVE do
            local ok, spellType, _, _, sid = pcall(GetActionInfo, native[i].action or 0)
            if ok and spellType == "spell" then ShowCooldownForSpell(sid, 3) end
        end
        for i = 1, NUM_EXTRA do
            if extra[i] and extra[i].g17spell then ShowCooldownForSpell(extra[i].g17spell, 3) end
        end
        DBG("测试：当前全部按钮显示 3 秒冷却圈。")
    elseif cmd == "status" then
        DBG("== 诊断 ==")
        DBG("onDragon="..tostring(onDragon)
            .."  页="..DetectPage()
            .."  BonusBarOffset="..tostring(safecall(GetBonusBarOffset))
            .."  玩家施法路径="..(pathBlocked and "被拦截" or "可用/未判定")
            .."  战斗中="..tostring(InCombatLockdown()))
        for s = 1, 12 do
            local action = BonusSlotAction(s)
            local ok, atype, id, sub, sid = pcall(GetActionInfo, action)
            local has = "无"
            if ok and atype then has = atype..":"..tostring(sid or id) end
            DBG(string.format("  Bonus槽%-2d action=%-4d %s", s, action, has))
        end
        for i = 1, NUM_EXTRA do
            local sid = db.extras and db.extras[i]
            DBG(string.format("  扩展%d: %s", i, sid and (SpellName(sid).."("..sid..")") or "空"))
        end
    elseif cmd == "reset" then
        db.point, db.relPoint, db.x, db.y = nil, nil, nil, nil
        db.scale = 1.0
        db.extras = { [1] = 990028 }
        pathBlocked = false
        frame:ClearAllPoints()
        frame:SetPoint("BOTTOM", UIParent, "BOTTOM", 0, 132)
        frame:SetScale(1.0)
        UpdateExtraRow()
        DBG("已重置。")
    else
        DBG("命令: unlock|lock|scale N|hideblizz on|off|add <1-6> <法术ID>|del <1-6>|forcextra|test|status|reset")
    end
end
