-- G17DragonBar v6 - 御龙术专用技能条（混合模式 + 诊断）
--
-- v6 解决"第 7 格不显示"。研究结论（详见 证据/g17dragonbar_v6_research_20260827.md）：
--   * 3.3.5 客户端 VehicleMenuBar 硬编码 6 按钮（VehicleMenuBar.lua:6）
--   * 载具法术写入 BonusActionBar 动作槽（12 格容量），客户端 UI 层只画 6 格；
--     是否填充第 7+ 槽属客户端 C++ 内部行为，Lua 无法保证 —— 本条用混合模式兜底
--   * 服务端 spell_g17_combat_skill 明确支持双施法者（载具条按钮=龙施法 / 玩家施法），
--     29 个载体全部带 CASTABLE_WHILE_MOUNTED —— 玩家按 ID 施法完全走得通
--   * SecureActionButton type="spell" spell=<ID> → CastSpellByID（客户端原生安全路径）
--
-- 混合模式（槽 7/8）：
--   * 若 Bonus 槽 7/8 有动作（客户端确实填充）→ 原生动作槽模式（type="action"）
--   * 否则 → 玩家施法兜底（type="spell" spell=<ID>），默认槽7=制动(990028)、槽8=切页(990025)
--     （可 /g17bar set 7 <法术ID> / set 8 <法术ID> 自定义，任意 990000-990028）
--
-- 诊断（回答一切疑问）：
--   /g17bar status   → 打印 GetBonusBarOffset、Bonus 槽 1-12 的动作内容、当前页、槽 7/8 模式
--   /g17bar testcast <法术ID> → 尝试玩家施法并回报客户端发送/失败事件
--
-- v5 功能保留：冷却圈（CLEU 施法成功跟踪 4/6/20/10/60s+切页1s，无幻影）、龙能读数、
--   按键 1-8/小键盘、可拖动缩放、只在御龙载具激活（能量类型3+上限100），任务坐骑零影响。

local TOCVERSION = "v6_hybrid_bar"
local NUM_NATIVE = 6     -- 原生 BonusActionButtonTemplate 槽
local NUM_EXTRA = 2      -- 混合槽 7/8
local NUM_BUTTONS = NUM_NATIVE + NUM_EXTRA
local BUTTON_SIZE = 44
local BUTTON_GAP = 6

local db
local native = {}      -- [1..6] BonusActionButtonTemplate 按钮
local extra = {}       -- [7..8] SecureActionButton 混合按钮
local onDragon = false
local updateTimer = 0
local bindFailedOnce = false
local lastCastReport = ""

-- ============ 冷却表（与服务端 cs_dragonriding.cpp COMBAT_CD_MS 一致） ============
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
    DEFAULT_CHAT_FRAME:AddMessage("|cffff8c00G17|r DragonBar |cff909090["..TOCVERSION.."]|r "..tostring(msg))
end

-- ============ 御龙载具检测（v4 已验证） ============
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

-- ============ Bonus 槽位工具 ============
-- 动作槽 ID = 槽号 + (NUM_ACTIONBAR_PAGES(6) + GetBonusBarOffset() - 1) * 12
-- （ActionButton.lua:140 ActionButton_CalculateAction 的同款公式）
local function BonusSlotAction(slot)
    local offset = GetBonusBarOffset()
    if not offset or offset <= 0 then offset = 1 end
    return slot + (6 + offset - 1) * 12
end

local function BonusSlotSpell(slot)
    local action = BonusSlotAction(slot)
    local ok, spellType, id, subType, spellId = pcall(GetActionInfo, action)
    if ok and spellType == "spell" then
        return spellId or id, action
    end
    return nil, action
end

-- 当前页：扫描 Bonus 槽 1-6 的法术（移动页含真实法术 55215/52197/52226）
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

-- ============ 冷却显示（v5 逻辑保留） ============
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
    for idx = 1, NUM_EXTRA do
        local btn = extra[idx]
        if btn and btn.g17spell == spellId then targets[#targets+1] = btn end
    end
    for _, btn in ipairs(targets) do
        local actionOk, spellType, id, subType, spellId = pcall(GetActionInfo, btn.action or 0)
        local match
        if actionOk and spellType == "spell" then
            match = (spellId == nil and id == spellId) or (spellId == spellId)
        end
        if btn.g17spell then match = (btn.g17spell == spellId) end
        if match then
            local cd = _G[btn:GetName().."Cooldown"]
            if cd then safecall(CooldownFrame_SetTimer, cd, start, duration, 1) end
        end
    end
end

-- ============ 混合槽 7/8 ============
local function ConfigureExtraButton(idx)
    -- idx: 1 或 2（对应条上第 7/8 格）
    local btn = extra[idx]
    if not btn or InCombatLockdown() then return false end
    local slot = NUM_NATIVE + idx
    local nativeSpell, action = BonusSlotSpell(slot)
    if nativeSpell then
        -- 原生模式：type="action" 直接引用 Bonus 动作槽
        btn.g17mode = "native"
        btn.g17spell = nativeSpell
        btn.action = action
        btn:SetAttribute("type", "action")
        btn:SetAttribute("action", action)
        local tex = safecall(GetActionTexture, action)
        if tex then _G[btn:GetName().."Icon"]:SetTexture(tex) end
    else
        -- 兜底模式：type="spell" 玩家按 ID 施法（服务端双施法路径）
        local spellId = db and db["extra"..idx] or ((idx == 1) and 990028 or 990025)
        btn.g17mode = "spell"
        btn.g17spell = spellId
        btn.action = nil
        btn:SetAttribute("type", "spell")
        btn:SetAttribute("spell", spellId)
        local tex = safecall(GetSpellTexture, spellId)
        if tex and tex ~= "" then
            _G[btn:GetName().."Icon"]:SetTexture(tex)
        else
            _G[btn:GetName().."Icon"]:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")
        end
    end
    btn:Show()
    return true
end

local function ConfigureAllExtra()
    for idx = 1, NUM_EXTRA do
        ConfigureExtraButton(idx)
    end
end

-- ============ 按键绑定 ============
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
    for i = 1, NUM_NATIVE do
        SetOverrideBindingClick(frame, true, tostring(i), "G17DragonBarButton"..i, "LeftButton")
    end
    for idx = 1, NUM_EXTRA do
        local key = tostring(NUM_NATIVE + idx)
        SetOverrideBindingClick(frame, true, key, "G17DragonBarButton"..(NUM_NATIVE+idx), "LeftButton")
    end
    for i = 1, NUM_BUTTONS do
        SetOverrideBindingClick(frame, true, "NUMPAD"..i, "G17DragonBarButton"..i, "LeftButton")
    end
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
        for i = 1, NUM_NATIVE do
            safecall(ActionButton_UpdateAction, native[i])
            safecall(ActionButton_Update, native[i])
        end
        ConfigureAllExtra()
        if db and db.hideBlizz then SetBlizzVehicleButtonsHidden(true) end
        ApplyKeyBindings()
        local p7 = BonusSlotSpell(7)
        DBG(string.format("已激活。当前=%s；Bonus槽7=%s（%s模式）；/g17bar status 看全部诊断。",
            DetectPage(), tostring(p7), extra[1] and extra[1].g17mode or "?"))
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
        if onDragon then DBG("御龙术条已激活（6原生＋2混合格＋冷却显示）。") end
        UpdateBarState()
    elseif onDragon then
        -- 页面切换后刷新混合槽（原生模式可能变化）
        ConfigureAllExtra()
    end
end

-- ============ 能量读数 ============
local function UpdatePowerText()
    local frame = G17DragonBarFrame
    if not frame or not onDragon then return end
    local power = safecall(UnitPower, "vehicle", 3) or 0
    local max = safecall(UnitPowerMax, "vehicle", 3) or 0
    local t = _G[frame:GetName().."Power"]
    if t then t:SetText(string.format("龙能 %d / %d   %s", power, max, DetectPage())) end
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
            ConfigureAllExtra()
        end
    elseif event == "PLAYER_REGEN_ENABLED" then
        if onDragon then
            ApplyKeyBindings()
            ConfigureAllExtra()
        end
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
    elseif event == "UNIT_SPELLCAST_SENT" then
        local unit, spell, rank = ...
        if unit == "player" then
            lastCastReport = "SENT:"..tostring(spell)
        end
    elseif event == "UNIT_SPELLCAST_FAILED" or event == "UNIT_SPELLCAST_INTERRUPTED" then
        local unit, spell = ...
        if unit == "player" then
            DBG("施法失败/打断："..tostring(spell).."（上一状态 "..lastCastReport.."）")
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

-- ============ 位置 ============
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
        extra1 = nil, extra2 = nil,
    }
    db = G17DragonBarDB

    -- 原生槽 1-6：继承 Blizzard BonusActionButtonTemplate（安全动作槽全套机制）
    for i = 1, NUM_NATIVE do
        local btn = CreateFrame("CheckButton", "G17DragonBarButton"..i, self, "BonusActionButtonTemplate")
        btn:SetID(i)
        btn:SetWidth(BUTTON_SIZE)
        btn:SetHeight(BUTTON_SIZE)
        if i == 1 then
            btn:SetPoint("BOTTOMLEFT", self, "BOTTOMLEFT", 14, 16)
        else
            btn:SetPoint("LEFT", native[i-1], "RIGHT", BUTTON_GAP, 0)
        end
        safecall(ActionButton_UpdateAction, btn)
        safecall(ActionButton_Update, btn)
        native[i] = btn
    end

    -- 混合槽 7-8：SecureActionButtonTemplate（原生/玩家施法双模式）
    for idx = 1, NUM_EXTRA do
        local i = NUM_NATIVE + idx
        local btn = CreateFrame("CheckButton", "G17DragonBarButton"..i, self, "SecureActionButtonTemplate, ActionButtonTemplate")
        btn:SetID(i)
        btn:SetWidth(BUTTON_SIZE)
        btn:SetHeight(BUTTON_SIZE)
        btn:SetPoint("LEFT", native[NUM_NATIVE], "RIGHT", BUTTON_GAP + (idx-1)*(BUTTON_SIZE+BUTTON_GAP), 0)
        btn:RegisterForClicks("AnyUp")
        btn:SetAttribute("useparent-unit", true)
        btn:SetAttribute("*unit1", "target")
        local cd = _G[btn:GetName().."Cooldown"]
        if cd then
            cd:SetAllPoints(btn)
            cd:Show()
        end
        btn:SetScript("OnEnter", function(b)
            GameTooltip:SetOwner(b, "ANCHOR_TOP")
            local ok = pcall(GameTooltip.SetHyperlink, GameTooltip, "spell:"..tostring(b.g17spell or 0))
            if not ok then GameTooltip:Hide() end
        end)
        btn:SetScript("OnLeave", function() GameTooltip:Hide() end)
        extra[idx] = btn
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
    self:RegisterEvent("UNIT_SPELLCAST_FAILED")
    self:RegisterEvent("UNIT_SPELLCAST_INTERRUPTED")
    self:SetMovable(true)
    self:RegisterForDrag("LeftButton")
    DBG("已加载。/g17bar 查看命令；上龙自动显示 8 格混合条。")
end

function G17DragonBar_OnEvent(self, event, ...)
    OnEvent(self, event, ...)
end

function G17DragonBar_OnUpdate(self, elapsed)
    OnUpdate(self, elapsed)
end

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
        DBG("已解锁，可拖动；/g17bar lock 锁定。")
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
    elseif cmd == "set" then
        local slotStr, spellStr = string.match(arg or "", "^(%d+)%s+(%d+)$")
        local slot, spellId = tonumber(slotStr), tonumber(spellStr)
        if slot and spellId and (slot == 7 or slot == 8) and spellId >= 990000 and spellId <= 990028 then
            db["extra"..(slot-6)] = spellId
            ConfigureExtraButton(slot - 6)
            DBG(string.format("槽 %d 已设为法术 %d（玩家施法兜底模式）。", slot, spellId))
        else
            DBG("用法: /g17bar set 7|8 <990000-990028>")
        end
    elseif cmd == "testcast" then
        local spellId = tonumber(arg)
        if spellId then
            lastCastReport = "TEST:"..spellId
            DBG("尝试玩家施法 "..spellId.."（若服务端放行会出现施法事件/特效；若客户端拦截则无任何反应）")
            if not InCombatLockdown() then
                local btn = extra[1]
                if btn then
                    btn:SetAttribute("type", "spell")
                    btn:SetAttribute("spell", spellId)
                    -- 模拟点击不可行（安全限制），提示用按钮测试
                    DBG("已把槽 7 临时指向该法术，请点击条上第 7 格按钮测试。")
                end
            else
                DBG("战斗中不能改安全按钮属性，脱战后再试。")
            end
        else
            DBG("用法: /g17bar testcast <法术ID>")
        end
    elseif cmd == "test" then
        for i = 1, NUM_NATIVE do
            local ok, spellType, _, _, sid = pcall(GetActionInfo, native[i].action or 0)
            if ok and spellType == "spell" then ShowCooldownForSpell(sid, 3) end
        end
        for idx = 1, NUM_EXTRA do
            if extra[idx] and extra[idx].g17spell then ShowCooldownForSpell(extra[idx].g17spell, 3) end
        end
        DBG("测试：为当前栏上的法术显示 3 秒冷却圈。")
    elseif cmd == "status" then
        DBG("== 诊断 ==")
        DBG("BonusBarOffset="..tostring(safecall(GetBonusBarOffset))
            .."  当前页="..DetectPage()
            .."  onDragon="..tostring(onDragon)
            .."  战斗中="..tostring(InCombatLockdown()))
        for s = 1, 12 do
            local action = BonusSlotAction(s)
            local ok, atype, id, sub, sid = pcall(GetActionInfo, action)
            local has = "无"
            if ok and atype then has = atype..":"..tostring(sid or id) end
            DBG(string.format("  Bonus槽%-2d action=%-4d %s", s, action, has))
        end
        for idx = 1, NUM_EXTRA do
            local b = extra[idx]
            if b then
                DBG(string.format("  条上第%d格: 模式=%s 法术=%s", NUM_NATIVE+idx, tostring(b.g17mode), tostring(b.g17spell)))
            end
        end
    elseif cmd == "reset" then
        db.point, db.relPoint, db.x, db.y = nil, nil, nil, nil
        db.scale = 1.0
        db.extra1, db.extra2 = nil, nil
        frame:ClearAllPoints()
        frame:SetPoint("BOTTOM", UIParent, "BOTTOM", 0, 150)
        frame:SetScale(1.0)
        DBG("位置/缩放/扩展槽已重置。")
    else
        DBG("命令: unlock|lock|scale N|hideblizz on|off|strict on|off|set 7|8 <法术ID>|testcast <ID>|test|status|reset")
    end
end
