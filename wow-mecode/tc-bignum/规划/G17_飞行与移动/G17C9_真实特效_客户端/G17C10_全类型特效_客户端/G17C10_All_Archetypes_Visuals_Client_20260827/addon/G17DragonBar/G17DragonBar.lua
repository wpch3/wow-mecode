-- G17DragonBar v5 - 御龙术专用技能条
--
-- 设计依据（全部来自提取的客户端 FrameXML 源码 + fork 服务端源码，见交接文件 §3.3/§3.8）：
--   * 载具法术由 SMSG_PET_SPELLS 写入 BONUS ACTION BAR 动作槽（VehicleSpellInitialize
--     发送全部 m_spells[0..7]，MAX_SPELL_CONTROL_BAR=10）
--   * 默认 VehicleMenuBar 只渲染 6 格（VEHICLE_MAX_ACTIONBUTTONS=6，VehicleMenuBar.lua:6）
--   * BonusActionButtonTemplate 按钮继承全部原生机制：安全点击施法、图标、
--     GetActionCooldown 冷却、UPDATE_BONUS_ACTIONBAR 自动刷新（ActionButton.lua:369）
--   * 动作槽 ID = GetID() + (NUM_ACTIONBAR_PAGES + GetBonusBarOffset() - 1) * 12
--     （ActionButton_CalculateAction，ActionButton.lua:140）
--   因此本条直接继承 BonusActionButtonTemplate 即可显示并施放第 7/8 格载具法术。
--
-- 冷却显示修复（服务端 DBC RecoveryTime=0 后客户端无本地预测）：
--   * COMBAT_LOG_EVENT_UNFILTERED 里 SPELL_CAST_SUCCESS（施法者=玩家或龙）→
--     按服务端冷却表（COMBAT_CD_MS={4,6,20,10,60}s，切页 1s）对相应按钮
--     CooldownFrame_SetTimer；同时覆盖默认载具条 6 个按钮。
--   * 原生 ACTIONBAR_UPDATE_COOLDOWN 路径继续保留（真实法术的 DBC 冷却照常显示）。
--
-- 御龙/任务坐骑分离：只在"能量类型3且上限≈100"的载具（G17 御空龙）上显示；
-- 任务坐骑（无此能量结构）不受影响，保持原生 6 格条。可选按名称严格匹配。

local TOCVERSION = "v5_dedicated_bar"
local NUM_BUTTONS = 8
local BUTTON_SIZE = 44
local BUTTON_GAP = 6

local db
local buttons = {}
local onDragon = false
local updateTimer = 0
local bindFailedOnce = false

-- ============ 工具 ============
local function safecall(func, ...)
    if type(func) ~= "function" then return nil end
    local ok, a, b, c = pcall(func, ...)
    if ok then return a, b, c end
    return nil, nil, tostring(a)
end

local function DBG(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cffff8c00G17|r DragonBar |cff909090["..TOCVERSION.."]|r "..tostring(msg))
end

-- ============ 御龙载具检测（v4 已验证逻辑） ============
local function IsOnG17Dragon()
    if not safecall(UnitExists, "vehicle") then return false end
    if not safecall(UnitInVehicle, "player") then return false end
    local maxEnergy = safecall(UnitPowerMax, "vehicle", 3)
    if not maxEnergy or maxEnergy < 90 or maxEnergy > 110 then return false end
    if db and db.strictName then
        local vname = safecall(UnitName, "vehicle") or ""
        if not (string.find(vname, "御空龙") or string.find(vname, "御龙")) then
            return false
        end
    end
    return true
end

-- ============ 冷却表（与服务端 cs_dragonriding.cpp COMBAT_CD_MS 一致） ============
local SLOT_CD = { 4, 6, 20, 10, 60 }  -- slot0..slot4（生成器/普攻/防御/机动/终结）
local PAGE_SWITCH_CD = 1               -- 990025 切页

local function SpellCooldownSeconds(spellId)
    if spellId >= 990000 and spellId <= 990024 then
        return SLOT_CD[((spellId - 990000) % 5) + 1]
    elseif spellId == 990025 then
        return PAGE_SWITCH_CD
    end
    return nil
end

-- ============ 按钮 ============
local function CreateButtons(frame)
    for i = 1, NUM_BUTTONS do
        local btn = CreateFrame("CheckButton", "G17DragonBarButton"..i, frame, "BonusActionButtonTemplate")
        btn:SetID(i)
        btn:SetWidth(BUTTON_SIZE)
        btn:SetHeight(BUTTON_SIZE)
        if i == 1 then
            btn:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 14, 16)
        else
            btn:SetPoint("LEFT", buttons[i-1], "RIGHT", BUTTON_GAP, 0)
        end
        -- 模板 OnLoad 时 GetID()=0 导致动作槽算错，创建后立即重算
        safecall(ActionButton_UpdateAction, btn)
        safecall(ActionButton_Update, btn)
        buttons[i] = btn
    end
end

-- 找到持有某法术的按钮并显示冷却（覆盖本条 + 默认载具条）
local function ShowCooldownForSpell(spellId, duration)
    if not duration or duration <= 0 then return end
    local start = GetTime()
    local targets = {}
    for i = 1, NUM_BUTTONS do
        if buttons[i] then targets[#targets+1] = buttons[i] end
    end
    for i = 1, 6 do
        local b = _G["VehicleMenuBarActionButton"..i]
        if b then targets[#targets+1] = b end
    end
    for _, btn in ipairs(targets) do
        local ok, spellType, _, _, spellId = pcall(GetActionInfo, btn.action or 0)
        if ok and spellType == "spell" and spellId == spellId then
            local cd = _G[btn:GetName().."Cooldown"]
            if cd then
                safecall(CooldownFrame_SetTimer, cd, start, duration, 1)
            end
        end
    end
end

-- ============ 显隐/绑定 ============
local function ApplyKeyBindings()
    if InCombatLockdown() then
        if not bindFailedOnce then
            DBG("战斗中无法绑定快捷键，脱战后重新上龙即可。")
            bindFailedOnce = true
        end
        return
    end
    local frame = G17DragonBarFrame
    if not frame then return end
    for i = 1, NUM_BUTTONS do
        SetOverrideBindingClick(frame, true, tostring(i), "G17DragonBarButton"..i, "LeftButton")
    end
    SetOverrideBindingClick(frame, true, "NUMPAD1", "G17DragonBarButton1", "LeftButton")
    SetOverrideBindingClick(frame, true, "NUMPAD2", "G17DragonBarButton2", "LeftButton")
    SetOverrideBindingClick(frame, true, "NUMPAD3", "G17DragonBarButton3", "LeftButton")
    SetOverrideBindingClick(frame, true, "NUMPAD4", "G17DragonBarButton4", "LeftButton")
    SetOverrideBindingClick(frame, true, "NUMPAD5", "G17DragonBarButton5", "LeftButton")
    SetOverrideBindingClick(frame, true, "NUMPAD6", "G17DragonBarButton6", "LeftButton")
    SetOverrideBindingClick(frame, true, "NUMPAD7", "G17DragonBarButton7", "LeftButton")
    SetOverrideBindingClick(frame, true, "NUMPAD8", "G17DragonBarButton8", "LeftButton")
end

local function ClearKeyBindings()
    if InCombatLockdown() then return end
    local frame = G17DragonBarFrame
    if frame then ClearOverrideBindings(frame) end
end

local function SetBlizzVehicleButtonsHidden(hidden)
    for i = 1, 6 do
        local b = _G["VehicleMenuBarActionButton"..i]
        if b then
            if hidden then b:Hide() else safecall(ActionButton_Update, b) end
        end
    end
end

local function UpdateBarState()
    local frame = G17DragonBarFrame
    if not frame then return end
    if onDragon then
        frame:Show()
        for i = 1, NUM_BUTTONS do
            safecall(ActionButton_UpdateAction, buttons[i])
            safecall(ActionButton_Update, buttons[i])
        end
        if db and db.hideBlizz then SetBlizzVehicleButtonsHidden(true) end
        ApplyKeyBindings()
    else
        ClearKeyBindings()
        if db and db.hideBlizz then SetBlizzVehicleButtonsHidden(false) end
        frame:Hide()
    end
end

local function CheckVehicleState()
    local now = IsOnG17Dragon()
    if now ~= onDragon then
        onDragon = now
        if onDragon then DBG("御龙术条已激活（8格＋冷却显示）。") end
        UpdateBarState()
    end
end

-- ============ 能量读数 ============
local function UpdatePowerText()
    local frame = G17DragonBarFrame
    if not frame or not onDragon then return end
    local power = safecall(UnitPower, "vehicle", 3) or 0
    local max = safecall(UnitPowerMax, "vehicle", 3) or 0
    local t = _G[frame:GetName().."Power"]
    if t then t:SetText(string.format("龙能 %d / %d", power, max)) end
end

-- ============ 事件 ============
local function OnEvent(self, event, ...)
    if event == "PLAYER_ENTERING_WORLD" or event == "UNIT_ENTERED_VEHICLE"
       or event == "UNIT_EXITED_VEHICLE" or event == "UNIT_DISPLAYPOWER"
       or event == "UPDATE_BONUS_ACTIONBAR" then
        CheckVehicleState()
        if event == "UPDATE_BONUS_ACTIONBAR" and onDragon then
            for i = 1, NUM_BUTTONS do
                safecall(ActionButton_UpdateAction, buttons[i])
                safecall(ActionButton_Update, buttons[i])
            end
        end
    elseif event == "PLAYER_REGEN_ENABLED" then
        if onDragon then ApplyKeyBindings() end
        bindFailedOnce = false
    elseif event == "COMBAT_LOG_EVENT_UNFILTERED" then
        local timestamp, combatEvent, sourceGUID = select(1, ...)
        if combatEvent == "SPELL_CAST_SUCCESS" and sourceGUID then
            local playerGUID = UnitGUID("player")
            local vehicleGUID = safecall(UnitGUID, "vehicle")
            if sourceGUID == playerGUID or (vehicleGUID and sourceGUID == vehicleGUID) then
                local spellId = select(9, ...)
                if spellId then
                    local duration = SpellCooldownSeconds(spellId)
                    if duration then
                        ShowCooldownForSpell(spellId, duration)
                    end
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
        UpdatePowerText()
    end
end

-- ============ 位置/设置 ============
local function SavePosition()
    local frame = G17DragonBarFrame
    if not frame or not db then return end
    local point, _, relPoint, x, y = frame:GetPoint(1)
    db.point = point; db.relPoint = relPoint; db.x = x; db.y = y
end

local function RestorePosition()
    local frame = G17DragonBarFrame
    if not frame or not db then return end
    if db.point then
        frame:ClearAllPoints()
        frame:SetPoint(db.point, UIParent, db.relPoint or "BOTTOM", db.x or 0, db.y or 150)
    end
    if db.scale then frame:SetScale(db.scale) end
end

-- ============ 初始化 ============
function G17DragonBar_OnLoad(self)
    G17DragonBarDB = G17DragonBarDB or {
        point = nil, relPoint = nil, x = nil, y = nil,
        scale = 1.0, hideBlizz = false, strictName = false, locked = true,
    }
    db = G17DragonBarDB
    CreateButtons(self)
    RestorePosition()
    self:RegisterEvent("PLAYER_ENTERING_WORLD")
    self:RegisterEvent("UNIT_ENTERED_VEHICLE")
    self:RegisterEvent("UNIT_EXITED_VEHICLE")
    self:RegisterEvent("UNIT_DISPLAYPOWER")
    self:RegisterEvent("UPDATE_BONUS_ACTIONBAR")
    self:RegisterEvent("PLAYER_REGEN_ENABLED")
    self:RegisterEvent("COMBAT_LOG_EVENT_UNFILTERED")
    self:SetMovable(true)
    self:RegisterForDrag("LeftButton")
    DBG("已加载。/g17bar 查看命令；上龙后自动显示 8 格专用条。")
end

function G17DragonBar_OnEvent(self, event, ...)
    OnEvent(self, event, ...)
end

function G17DragonBar_OnUpdate(self, elapsed)
    OnUpdate(self, elapsed)
end

-- 拖动（仅解锁时）
G17DragonBarFrame:SetScript("OnDragStart", function(self)
    if db and not db.locked and not InCombatLockdown() then
        self:StartMoving()
    end
end)
G17DragonBarFrame:SetScript("OnDragStop", function(self)
    self:StopMovingOrSizing()
    SavePosition()
end)

-- ============ 命令 ============
SLASH_G17DRAGONBAR1 = "/g17bar"
SLASH_G17DRAGONBAR2 = "/g17dragonbar"
SlashCmdList["G17DRAGONBAR"] = function(msg)
    local cmd, arg = string.match((msg or ""):gsub("%s+", " "), "^(%S*)%s*(.-)$")
    cmd = string.lower(cmd or "")
    local frame = G17DragonBarFrame
    if cmd == "unlock" then
        db.locked = false
        DBG("已解锁，可拖动标题区域移动位置；/g17bar lock 锁定。")
    elseif cmd == "lock" then
        db.locked = true
        SavePosition()
        DBG("已锁定并保存位置。")
    elseif cmd == "scale" then
        local s = tonumber(arg)
        if s and s >= 0.5 and s <= 2.5 then
            db.scale = s
            frame:SetScale(s)
            DBG("缩放已设为 "..s)
        else
            DBG("用法: /g17bar scale 0.5~2.5")
        end
    elseif cmd == "hideblizz" then
        db.hideBlizz = (arg ~= "off")
        if onDragon then SetBlizzVehicleButtonsHidden(db.hideBlizz) end
        DBG("隐藏默认载具条按钮: "..tostring(db.hideBlizz))
    elseif cmd == "strict" then
        db.strictName = (arg ~= "off")
        DBG("严格名称匹配(只认 御空龙): "..tostring(db.strictName))
    elseif cmd == "test" then
        -- 本地自检：对全部按钮走一次冷却显示
        for i = 1, NUM_BUTTONS do
            local ok, spellType, _, _, spellId = pcall(GetActionInfo, buttons[i].action or 0)
            if ok and spellType == "spell" then
                ShowCooldownForSpell(spellId, 3)
            end
        end
        DBG("测试：为当前栏上的法术显示 3 秒冷却圈。")
    elseif cmd == "reset" then
        db.point, db.relPoint, db.x, db.y = nil, nil, nil, nil
        db.scale = 1.0
        frame:ClearAllPoints()
        frame:SetPoint("BOTTOM", UIParent, "BOTTOM", 0, 150)
        frame:SetScale(1.0)
        DBG("位置与缩放已重置。")
    elseif cmd == "status" then
        DBG("状态: onDragon="..tostring(onDragon)
            .." scale="..tostring(db.scale)
            .." locked="..tostring(db.locked)
            .." hideBlizz="..tostring(db.hideBlizz)
            .." strictName="..tostring(db.strictName))
        for i = 1, NUM_BUTTONS do
            local ok, st, _, _, sid = pcall(GetActionInfo, buttons[i].action or 0)
            DBG(string.format("  槽%d: action=%s %s", i, tostring(buttons[i].action), ok and (st.." "..tostring(sid)) or "无"))
        end
    else
        DBG("命令: unlock|lock|scale N|hideblizz on|off|strict on|off|test|reset|status")
    end
end
