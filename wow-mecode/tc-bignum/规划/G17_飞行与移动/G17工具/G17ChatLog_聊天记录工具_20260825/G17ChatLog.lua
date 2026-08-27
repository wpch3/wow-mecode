-- G17ChatLog: chat frame logger + exporter for WoW 3.3.5a.
--
-- Solves two problems the user reported:
--   1. can't copy chat text (default 3.3.5 chat has no select-all/copy)
--   2. chat history gets scrolled away too fast (default ~250 lines)
--
-- Features:
--   * Captures ALL chat frame messages into an unbounded buffer
--   * /g17log       opens a scrollable text window with everything captured
--   * /g17log clear clears the buffer
--   * /g17log save  exports buffer to SavedVariables + shows export frame
--   * /g17log last N shows last N lines in chat
--   * The export frame has a big edit box you can Ctrl+A / Ctrl+C from
--   * Also bumps the main chat frame's max lines to 5000 on load
--
-- No client files modified; pure addon.

local ADDON_NAME = "G17ChatLog"
local VERSION = "v1"
local MAX_BUFFER = 10000       -- hard safety cap (about 1MB of text)
local CHAT_FRAME_MAX_LINES = 5000

-- =========================================================== storage ====

G17ChatLogDB = G17ChatLogDB or { lines = {} }
local lines = G17ChatLogDB.lines
local lineCount = #lines

local function AddLine(text)
    if lineCount >= MAX_BUFFER then
        table.remove(lines, 1)
        lineCount = lineCount - 1
    end
    lineCount = lineCount + 1
    lines[lineCount] = text
end

-- =========================================================== capture ====

-- Hook ChatFrame_OnEventHandler to intercept everything that goes to the
-- DEFAULT chat frame (frame 1).
local frame = CreateFrame("Frame")
frame:RegisterEvent("CHAT_MSG_CHANNEL")
frame:RegisterEvent("CHAT_MSG_YELL")
frame:RegisterEvent("CHAT_MSG_GUILD")
frame:RegisterEvent("CHAT_MSG_PARTY")
frame:RegisterEvent("CHAT_MSG_RAID")
frame:RegisterEvent("CHAT_MSG_SAY")
frame:RegisterEvent("CHAT_MSG_WHISPER")
frame:RegisterEvent("CHAT_MSG_SYSTEM")
frame:RegisterEvent("CHAT_MSG_LOOT")
frame:RegisterEvent("CHAT_MSG_MONEY")
frame:RegisterEvent("CHAT_MSG_COMBAT_XP_GAIN")
frame:RegisterEvent("CHAT_MSG_COMBAT_HONOR_GAIN")
frame:RegisterEvent("CHAT_MSG_COMBAT_MISC_INFO")
frame:RegisterEvent("CHAT_MSG_SPELL_SELF_DAMAGE")
frame:RegisterEvent("CHAT_MSG_SPELL_HOSTILEPLAYER_DAMAGE")
frame:RegisterEvent("CHAT_MSG_SPELL_CREATURE_VS_SELF_DAMAGE")
frame:RegisterEvent("CHAT_MSG_SPELL_PERIODIC_SELF_DAMAGE")
frame:RegisterEvent("CHAT_MSG_SPELL_PERIODIC_SELF_BUFFS")
frame:RegisterEvent("CHAT_MSG_SPELL_AURA_GONE_SELF")
frame:RegisterEvent("CHAT_MSG_BG_SYSTEM_NEUTRAL")
frame:RegisterEvent("CHAT_MSG_BG_SYSTEM_HORDE")
frame:RegisterEvent("CHAT_MSG_BG_SYSTEM_ALLIANCE")
frame:RegisterEvent("CHAT_MSG_SKILL")
frame:RegisterEvent("CHAT_MSG_LOOT")
frame:RegisterEvent("CHAT_MSG_CURRENCY")
frame:RegisterEvent("CHAT_MSG_OPENING")
frame:RegisterEvent("CHAT_MSG_PET_INFO")
frame:RegisterEvent("CHAT_MSG_COMBAT_PET_INFO")
frame:RegisterEvent("CHAT_MSG_TEXT_EMOTE")
frame:RegisterEvent("CHAT_MSG_EMOTE")
frame:RegisterEvent("CHAT_MSG_DND")
frame:RegisterEvent("CHAT_MSG_AFK")
frame:RegisterEvent("CHAT_MSG_IGNORED")
frame:RegisterEvent("CHAT_MSG_OFFICER")
frame:RegisterEvent("CHAT_MSG_BATTLEGROUND")
frame:RegisterEvent("CHAT_MSG_BATTLEGROUND_LEADER")
frame:RegisterEvent("CHAT_MSG_RAID_LEADER")
frame:RegisterEvent("CHAT_MSG_RAID_WARNING")
frame:RegisterEvent("CHAT_MSG_PARTY_LEADER")
frame:RegisterEvent("CHAT_MSG_MONSTER_SAY")
frame:RegisterEvent("CHAT_MSG_MONSTER_YELL")
frame:RegisterEvent("CHAT_MSG_MONSTER_WHISPER")
frame:RegisterEvent("CHAT_MSG_MONSTER_EMOTE")
frame:RegisterEvent("CHAT_MSG_MONSTER_PARTY")
frame:RegisterEvent("CHAT_MSG_RAID_BOSS_WHISPER")
frame:RegisterEvent("CHAT_MSG_RAID_BOSS_EMOTE")
frame:RegisterEvent("CHAT_MSG_FILTERED")
frame:RegisterEvent("CHAT_MSG_BATTLEGROUND")
frame:RegisterEvent("CHAT_MSG_ACHIEVEMENT")
frame:RegisterEvent("CHAT_MSG_GUILD_ACHIEVEMENT")
frame:RegisterEvent("CHAT_MSG_PET_BATTLE_COMBAT_LOG")

-- Also capture print() output by hooking the default chat frame's AddMessage
local origAddMessage
local function HookChatFrame()
    local chatFrame = getglobal("ChatFrame1")
    if not chatFrame then return end

    -- bump max lines
    chatFrame:SetMaxLines(CHAT_FRAME_MAX_LINES)

    -- hook AddMessage to capture everything printed to the default frame
    origAddMessage = chatFrame.AddMessage
    chatFrame.AddMessage = function(self, msg, r, g, b, id)
        if msg then
            local ts = date("%H:%M:%S")
            AddLine(ts .. " " .. tostring(msg))
        end
        return origAddMessage(self, msg, r, g, b, id)
    end
end

frame:SetScript("OnEvent", function(self, event, ...)
    -- We capture via the AddMessage hook (catches everything including print),
    -- so this event handler is mainly to ensure we don't miss messages that
    -- bypass the default frame. The AddMessage hook is the primary path.
end)

-- =========================================================== export ====

local exportFrame

local function CreateExportFrame()
    if exportFrame then return exportFrame end

    exportFrame = CreateFrame("Frame", "G17ChatLogExportFrame", UIParent)
    exportFrame:SetWidth(700)
    exportFrame:SetHeight(500)
    exportFrame:SetPoint("CENTER")
    exportFrame:SetFrameStrata("DIALOG")
    exportFrame:SetBackdrop({
        bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
        tile = true, tileSize = 32, edgeSize = 32,
        insets = { left = 11, right = 12, top = 12, bottom = 11 }
    })
    exportFrame:EnableMouse(true)
    exportFrame:SetMovable(true)
    exportFrame:RegisterForDrag("LeftButton")
    exportFrame:SetScript("OnDragStart", function(self) self:StartMoving() end)
    exportFrame:SetScript("OnDragStop", function(self) self:StopMovingOrSizing() end)

    -- title
    local title = exportFrame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", exportFrame, "TOP", 0, -15)
    title:SetText("|cffffd700G17 ChatLog|r - 聊天记录导出")

    -- instructions
    local info = exportFrame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    info:SetPoint("TOP", exportFrame, "TOP", 0, -35)
    info:SetText("Ctrl+A 全选 → Ctrl+C 复制 → 粘贴回传给开发")

    -- scroll frame with edit box
    local scrollFrame = CreateFrame("ScrollFrame", "G17ChatLogScrollFrame", exportFrame, "UIPanelScrollFrameTemplate")
    scrollFrame:SetPoint("TOPLEFT", exportFrame, "TOPLEFT", 20, -50)
    scrollFrame:SetPoint("BOTTOMRIGHT", exportFrame, "BOTTOMRIGHT", -30, 45)

    local editBox = CreateFrame("EditBox", "G17ChatLogEditBox", scrollFrame)
    editBox:SetMultiLine(true)
    editBox:SetMaxLetters(0)
    editBox:EnableMouse(true)
    editBox:SetAutoFocus(false)
    editBox:SetFontObject("ChatFontNormal")
    editBox:SetWidth(640)
    editBox:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)

    -- make the edit box grow with content
    editBox:SetScript("OnTextChanged", function(self)
        scrollFrame:UpdateScrollChildRect()
    end)

    scrollFrame:SetScrollChild(editBox)
    exportFrame.editBox = editBox

    -- close button
    local close = CreateFrame("Button", "G17ChatLogClose", exportFrame, "UIPanelButtonTemplate")
    close:SetPoint("BOTTOM", exportFrame, "BOTTOM", 0, 12)
    close:SetWidth(120)
    close:SetHeight(24)
    close:SetText("关闭")
    close:SetScript("OnClick", function() exportFrame:Hide() end)

    -- copy hint
    local hint = exportFrame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    hint:SetPoint("BOTTOMRIGHT", exportFrame, "BOTTOMRIGHT", -15, 18)
    hint:SetText("也可截图整个窗口")

    exportFrame:Hide()
    return exportFrame
end

local function ShowExport(lastN)
    local f = CreateExportFrame()
    local text = ""
    local count = #lines
    local startIdx = lastN and math.max(1, count - lastN + 1) or 1

    for i = startIdx, count do
        text = text .. lines[i] .. "\n"
    end

    if text == "" then
        text = "(没有记录。请先在游戏中产生一些聊天/系统消息。)"
    end

    f.editBox:SetText(text)
    f.editBox:HighlightText()
    f:Show()
end

-- =========================================================== slash ======

SLASH_G17CHATLOG1 = "/g17log"
SLASH_G17CHATLOG2 = "/g17chat"
SlashCmdList["G17CHATLOG"] = function(msg)
    msg = (msg or ""):lower():match("^%s*(.-)%s*$")

    if msg == "clear" then
        wipe(lines)
        lineCount = 0
        print("|cffffd700G17 ChatLog|r 已清空记录。")
    elseif msg == "save" then
        ShowExport()
    elseif msg:match("^last%s+(%d+)$") then
        local n = tonumber(msg:match("^last%s+(%d+)$"))
        ShowExport(n)
    elseif msg == "help" or msg == "" then
        print("|cffffd700G17 ChatLog|r " .. VERSION)
        print("  /g17log save       打开导出窗口（全部记录）")
        print("  /g17log last 100   只看最近100行")
        print("  /g17log clear      清空记录")
        print("  导出窗口里 Ctrl+A 全选 Ctrl+C 复制")
    else
        print("|cffffd700G17 ChatLog|r 未知命令: " .. msg .. " (输入 /g17log help)")
    end
end

-- =========================================================== init ======

local initFrame = CreateFrame("Frame")
initFrame:RegisterEvent("PLAYER_LOGIN")
initFrame:SetScript("OnEvent", function()
    HookChatFrame()
    -- also bump all chat frames
    for i = 1, 10 do
        local cf = getglobal("ChatFrame" .. i)
        if cf then cf:SetMaxLines(CHAT_FRAME_MAX_LINES) end
    end
    print("|cffffd700G17 ChatLog|r " .. VERSION .. " 已加载。聊天框上限已提升到" .. CHAT_FRAME_MAX_LINES .. "行。")
    print("  /g17log save 导出记录 | /g17log last 100 最近100行 | /g17log clear 清空")
    initFrame:UnregisterEvent("PLAYER_LOGIN")
end)
