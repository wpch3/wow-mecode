-- G23-P3A complete GM help overlay.
-- Replaces the old fixed 69-entry C++ view at command-dispatch time without
-- modifying C++. The curated project catalog is combined with on-demand reads
-- from world.command, so native core help is no longer limited to a stale list.

if type(G23) ~= "table" then
    error("custom_gmhelp.lua requires extensions/G23Core.ext")
end

local MENU_ID = 60510
local SENDER_CAT, SENDER_ENTRY, SENDER_PAGE = 9311, 9312, 9313
local SENDER_BACK, SENDER_CLOSE = 9314, 9315
local PAGE_FACTOR = 100000
local MENU_PAGE_SIZE = 27
local TEXT_PAGE_SIZE = 14

local CATEGORIES = {
    [1] = "服务器与Lua", [2] = "战斗与增益", [3] = "Bot管理",
    [4] = "物品与装备", [5] = "外观与模型", [6] = "世界与NPC",
    [7] = "剧情与演出", [8] = "角色与技能", [9] = "传送与定位",
    [10] = "核心管理", [11] = "高危管理",
}

local entries = {}
local function E(cat, cmd, cn, alias, usage, note)
    entries[#entries + 1] = {
        cat=cat, cmd=cmd, cn=cn, alias=alias or "", usage=usage or cmd, note=note or "",
    }
end

-- Current project commands: sourced from the project command registry and final
-- F44R1/P2R1 sources, not from the old gmhelper memory list.
E(1,".server","统一服务器助手","服务器 状态 功能 health daily",".server","P3A；原生.server info/restart/shutdown仍由核心处理")
E(1,".server status","Lua与功能状态","版本 state 命令数量",".server status")
E(1,".server commands","可用自定义指令","指令列表 分页",".server commands 2")
E(1,".server daily","今日奖励状态","每日奖励 claim 连续",".server daily")
E(1,".server health","只读健康页","健康 API DB pending",".server health","GM2")
E(1,".help2","自定义指令导览","帮助 help",".help2")
E(1,".gmhelp","完整GM帮助","GM助手 指令搜索 菜单",".gmhelp","P3A无状态菜单")
E(1,".gmhelp find","中英文搜索全部帮助","查命令 搜索 core",".gmhelp find 传送","同时搜索项目目录与world.command")
E(1,".gmhelp core","搜索核心命令表","官方 原生 world command",".gmhelp core reload")
E(1,".gmhelp verify","帮助目录自检","数量 验证 完整",".gmhelp verify")
E(1,".luadiag","Lua按需诊断","Lua DB pending API",".luadiag","GM2；不在加载时查库")
E(1,".bigtest","大数值与Eluna自检","测试 精度 API DB",".bigtest")
E(1,".tp","安全中文传送","菜单 分类 传送",".tp","P2R1无状态分页")
E(1,".tp <关键词>","按中英文搜索传送","地点 搜索",".tp 暴风城")

E(2,".set","战斗辅助设置","属性 暴击 命中 急速 精准",".set","当前F44R1战斗辅助入口")
E(2,".bar","战斗辅助条","状态条 UI",".bar")
E(2,".combo","NPCBot自动战斗组合","治疗 输出 道标 职业",".combo")
E(2,".buff","增益与治疗场景","buff scene 血线 副本 团本",".buff scene","所有buff禁止提前循环补施")
E(2,".setup","战斗辅助快速设置","初始化 预设",".setup")
E(2,".combo on","启用组合战斗","自动战斗 开启",".combo on")
E(2,".combo off","停用组合战斗","自动战斗 关闭",".combo off")
E(2,".buff scene","选择治疗场景","任务 材料 5人 团本 高级团本",".buff scene")
E(2,".combatstop","强制脱战","脱战 停止战斗",".combatstop")
E(2,".cheat god","无敌开关","免伤 不死",".cheat god on")
E(2,".cheat cooldown","技能无冷却","无CD",".cheat cooldown on","会影响GCD，谨慎")
E(2,".cheat casttime","技能瞬发","无读条",".cheat casttime on")
E(2,".cheat power","无限资源","无限蓝 能量 怒气",".cheat power on")

E(3,".npcbot","NPCBot官方命令树","bot 机器人 队友",".npcbot")
E(3,".npcbot add","招募选中NPCBot","添加 队友",".npcbot add")
E(3,".npcbot remove","移除NPCBot","删除 队友",".npcbot remove")
E(3,".npcbot command","NPCBot命令","跟随 停留 攻击",".npcbot command")
E(3,".pbot","PlayerBot管理","玩家bot 上线 下线",".pbot list","当前只有基础能力，不冒充完整AI")
E(3,".pbot spawn","PlayerBot上线","召出 上线",".pbot spawn <账号> <角色>")
E(3,".pbot despawn","PlayerBot下线","收回 下线",".pbot despawn <角色>")
E(3,".pbot come","拉PlayerBot到身边","过来 召回",".pbot come <角色>")
E(3,".pbot goto","去PlayerBot身边","去找 定位",".pbot goto <角色>")
E(3,".pbot auto","自动接受设置","组队 公会 交易 召唤",".pbot auto <角色> all on")
E(3,".pbot accept","处理挂起邀请","接受 邀请",".pbot accept all")
E(3,".bf","召集/定位Bot","botfind 召集",".bf")
E(3,".botfind","召集/定位Bot完整名","查找 bot",".botfind")
E(3,".tome","Bot到我身边","召集 过来",".tome")
E(3,".bd","Bot诊断","botdiag 状态",".bd")
E(3,".botdiag","Bot诊断完整名","状态 诊断",".botdiag")
E(3,".botname","NPCBot/PlayerBot改名","中文 英文 名字",".botname show")
E(3,".botname cn","设置Bot中文名","改名 中文",".botname cn <名字>")
E(3,".botname player","PlayerBot改名","玩家bot 名字",".botname player <旧名> <新名>")
E(3,".pin status","游荡Bot只读状态","永久化 诊断",".pin status","其它.pin操作已停用，禁止恢复")

E(4,".gearset","装备套装系统","套装 配装 菜单",".gearset")
E(4,".gearset progress","套装解锁进度","刷本 进度",".gearset progress")
E(4,".gearset preview","职业套装预览","预览 职业",".gearset preview 战士")
E(4,".add","智能中文添加物品","物品 搜索 添加",".add <物品名>")
E(4,".add!","强制智能添加","物品 强制",".add! <物品名>")
E(4,".item","装备魔改入口","克隆 装等 属性",".item")
E(4,".item clone","克隆并魔改装备","造装备 复制",".item clone 49623 装等300 -y")
E(4,".item list","列出自造装备","装备列表",".item list")
E(4,".transmog","幻化系统","外观 时装",".transmog")
E(4,".transmog copy","复制物品外观","幻化 复制",".transmog copy 12640")
E(4,".transmog find","搜索外观","找模型 品质",".transmog find 头盔 紫")
E(4,".transmog preview","临时试穿","预览 试穿",".transmog preview 12640")
E(4,".transmog save","保存外观方案","存方案",".transmog save 战斗套")
E(4,".spell clean","清理低阶技能","技能书 清理",".spell clean")
E(4,".reloaditem","热重载物品模板","刷新 物品",".reloaditem 900001")
E(4,".gear","世界工具装备入口","装备 工具",".gear")
E(4,".additem","核心按ID添加物品","物品ID",".additem 49623")
E(4,".additem set","核心添加整套物品","套装ID",".additem set 881")
E(4,".repairitems","修理全部装备","耐久 修理",".repairitems")
E(4,".bank","随地打开银行","银行",".bank")
E(4,".mailbox","随地打开邮箱","邮箱",".mailbox")

E(5,".model","角色/NPC模型控制","DisplayID 模型",".model")
E(5,".disguise","伪装变身","外观 变身",".disguise")
E(5,".morph","按DisplayID变形","模型 变形",".morph 448")
E(5,".demorph","解除变形","还原 模型",".demorph")
E(5,".findmodel","搜索Creature模型","模型 搜索 display",".findmodel <关键词>")
E(5,".fm","模型搜索短命令","findmodel",".fm <关键词>")
E(5,".modify scale","修改体型","大小 缩放",".modify scale 2")
E(5,".modify gender","改变性别","男 女",".modify gender male")
E(5,".modify phase","修改相位","phase",".modify phase 1")

E(6,".spawn","智能中文生成NPC","刷怪 召唤NPC",".spawn <NPC名>")
E(6,".spawn!","强制生成NPC","刷怪 强制",".spawn! <NPC名>")
E(6,".clean","清理智能生成物","清除 NPC",".clean")
E(6,".protect","世界保护工具","保护 区域",".protect")
E(6,".killr","范围击杀工具","范围 杀怪",".killr")
E(6,".npcclean","范围NPC清理","清怪 NPC",".npcclean")
E(6,".inst","副本世界工具","副本 实例",".inst")
E(6,".raidbuff","团队增益工具","团本 buff",".raidbuff")
E(6,".service","随身服务工具","商人 修理 服务",".service")
E(6,".dummy","训练木桩管理","木桩 DPS 测试",".dummy")
E(6,".nst","NPC状态调度","NPC状态 动作",".nst")
E(6,".npc add","核心生成NPC","生物 entry",".npc add 12345")
E(6,".npc delete","核心删除NPC","删怪",".npc delete")
E(6,".npc info","查看NPC详情","GUID entry 模型",".npc info")
E(6,".npc move","移动NPC出生点","位置",".npc move")
E(6,".lookup creature","搜索生物模板","查怪 NPC",".lookup creature 假人")
E(6,".gobject add","生成游戏物体","GO 物体",".gobject add <entry>")
E(6,".gobject delete","删除游戏物体","GO 删除",".gobject delete <guid>")
E(6,".gobject info","游戏物体信息","GO 详情",".gobject info")
E(6,".die","杀死选中目标","秒杀",".die")
E(6,".revive","复活角色","复活",".revive")
E(6,".respawn","刷新选中目标","重生 刷新",".respawn")

E(7,".scene","场景快照与恢复","场景 演出 快照",".scene")
E(7,".emote","剧情表情动作","动作 演出",".emote")
E(7,".say","让NPC对白","剧情 台词 说话",".say <内容>")
E(7,".announce","全服公告","广播 公告",".announce <内容>")
E(7,".notify","屏幕通知","通知 广播",".notify <内容>")

E(8,".modify allstats","一起修改五维","力量 敏捷 耐力 智力 精神",".modify allstats 5000000")
E(8,".modify stat","修改单项属性","str agi sta int spi",".modify stat sta 400000000")
E(8,".modify hp","修改生命值","血量",".modify hp 100000")
E(8,".modify mana","修改法力值","蓝量",".modify mana 50000")
E(8,".modify money","修改金钱","金币 铜币",".modify money 10000000")
E(8,".modify speed","移动速度命令树","跑速 飞行 游泳",".modify speed all 5")
E(8,".modify talentpoints","修改天赋点","天赋",".modify talentpoints 71")
E(8,".modify reputation","修改声望","阵营 声望",".modify reputation <faction> <value>")
E(8,".character level","设置角色等级","等级",".character level 80")
E(8,".character rename","强制角色改名","改名",".character rename <玩家>")
E(8,".levelup","提升等级","升级",".levelup 1")
E(8,".reset talents","重置天赋","洗点",".reset talents")
E(8,".reset spells","重置技能","技能书",".reset spells")
E(8,".learn","学习法术","法术ID",".learn <spellId>")
E(8,".unlearn","遗忘法术","法术ID",".unlearn <spellId>")
E(8,".learn all recipes","学习职业全部配方","专业 配方",".learn all recipes <profession>")
E(8,".maxskill","技能熟练度拉满","武器 专业",".maxskill")

E(9,".tele","核心命名传送","英文地点",".tele Stormwind")
E(9,".appear","传送到玩家身边","去找 玩家",".appear <玩家>")
E(9,".summon","召唤玩家到身边","拉人 玩家",".summon <玩家>")
E(9,".recall","返回传送前位置","回去",".recall")
E(9,".go xyz","按坐标传送","坐标 x y z map",".go xyz -8913 554 93 0")
E(9,".go creature","传送到生物","NPC GUID entry",".go creature <guid>")
E(9,".go object","传送到游戏物体","GO guid",".go object <guid>")
E(9,".gps","显示当前位置","坐标 地图 区域",".gps")
E(9,".unstuck","卡住自救","脱困",".unstuck")
E(9,".cheat taxi","解锁飞行点","航点 taxi",".cheat taxi on")

E(10,".lookup item","搜索物品模板","查物品",".lookup item <关键词>")
E(10,".lookup spell","搜索法术","查技能",".lookup spell <关键词>")
E(10,".lookup quest","搜索任务","查任务",".lookup quest <关键词>")
E(10,".lookup tele","搜索核心传送名","查地点 英文",".lookup tele <关键词>")
E(10,".list item","列出目标物品","背包 装备",".list item <itemId>")
E(10,".list creature","列出生物生成点","生物 entry",".list creature <entry>")
E(10,".list object","列出物体生成点","GO entry",".list object <entry>")
E(10,".server info","核心服务器信息","在线人数 版本 uptime",".server info","由核心处理，不被P3A拦截")
E(10,".server motd","显示服务器公告","motd",".server motd","由核心处理")
E(10,".saveall","保存全部角色","存档",".saveall")
E(10,".kick","踢玩家下线","踢人",".kick <玩家> <原因>")
E(10,".mute","禁言账号","禁言",".mute <玩家> <分钟> <原因>")
E(10,".unmute","解除禁言","解禁",".unmute <玩家>")
E(10,".ban account","封禁账号","封号",".ban account <账号> <时长> <原因>")
E(10,".unban account","解除账号封禁","解封",".unban account <账号>")
E(10,".account set gmlevel","设置账号GM等级","权限 RBAC",".account set gmlevel <账号> <等级> -1")
E(10,".reload config","重载核心配置","conf 配置",".reload config","不会重载Lua；P3A禁止.reload eluna")

E(11,".server restart","计划重启服务器","高危 重启",".server restart <秒>","核心命令；使用前确认无人和保存状态")
E(11,".server shutdown","计划关闭服务器","高危 关服",".server shutdown <秒>","核心命令；不可误点")
E(11,".server restart cancel","取消计划重启","取消 重启",".server restart cancel")
E(11,".server shutdown cancel","取消计划关服","取消 关服",".server shutdown cancel")
E(11,".reload all","重载大量世界表","高危 重载 全部",".reload all","可能造成卡顿；不含Lua")

local function say(player, text)
    player:SendBroadcastMessage(text)
end

local function trim(text)
    return tostring(text or ""):gsub("^%s+", ""):gsub("%s+$", "")
end

local function escSql(text)
    text = trim(text):sub(1, 48):gsub("[%z\1-\31\\]", "")
    return text:gsub("'", "''")
end

local function categoryEntries(cat)
    local out = {}
    for index, entry in ipairs(entries) do
        if entry.cat == cat then out[#out + 1] = {index=index, entry=entry} end
    end
    return out
end

local function findCategory(text)
    text = trim(text)
    local number = tonumber(text)
    if number and CATEGORIES[math.floor(number)] then return math.floor(number) end
    for id, name in pairs(CATEGORIES) do
        if text == name then return id end
    end
    return nil
end

local function staticSearch(keyword)
    local out = {}
    local needle = trim(keyword):lower()
    if needle == "" then return out end
    for index, entry in ipairs(entries) do
        local hay = table.concat({entry.cmd, entry.cn, entry.alias, entry.usage, entry.note}, " "):lower()
        if hay:find(needle, 1, true) then out[#out + 1] = {index=index, entry=entry} end
    end
    return out
end

local function queryCore(keyword, limit)
    local escaped = escSql(keyword)
    if escaped == "" then return {}, false end
    limit = math.min(50, math.max(1, tonumber(limit) or 30))
    local q = G23.WorldQuery(
        "SELECT name,IFNULL(help,'') FROM command WHERE name LIKE '%" .. escaped ..
        "%' OR help LIKE '%" .. escaped .. "%' ORDER BY name LIMIT " .. math.floor(limit),
        "gmhelp:core", false)
    local out = {}
    if not q then return out, false end
    repeat
        out[#out + 1] = {name=q:GetString(0), help=q:GetString(1)}
    until not q:NextRow()
    return out, true
end

local function shortHelp(text)
    text = tostring(text or ""):gsub("[\r\n]+", " "):gsub("%s+", " ")
    if #text > 150 then text = text:sub(1, 147) .. "..." end
    return text
end

local function detail(player, entry)
    say(player, "|cff00ccff========== " .. entry.cn .. " ==========|r")
    say(player, "指令：|cffffff00" .. entry.cmd .. "|r")
    say(player, "用法：|cff00ff00" .. entry.usage .. "|r")
    if entry.note ~= "" then say(player, "说明：" .. entry.note) end
end

local function showTextList(player, list, page)
    local maxPage = math.max(1, math.ceil(#list / TEXT_PAGE_SIZE))
    page = math.floor(tonumber(page) or 1)
    if page < 1 then page = 1 end
    if page > maxPage then page = maxPage end
    local first = (page - 1) * TEXT_PAGE_SIZE + 1
    local last = math.min(first + TEXT_PAGE_SIZE - 1, #list)
    say(player, "|cff00ccff===== 项目指令 " .. page .. "/" .. maxPage .. "（共" .. #list .. "条）=====|r")
    for i = first, last do
        local e = list[i].entry
        say(player, "|cffffff00" .. e.cmd .. "|r  " .. e.cn)
    end
end

local function showCategories(player)
    player:GossipClearMenu()
    for id = 1, #CATEGORIES do
        player:GossipMenuAddItem(0, "【" .. CATEGORIES[id] .. "】 " .. #categoryEntries(id) .. "条", SENDER_CAT, id)
    end
    player:GossipMenuAddItem(0, "|cff888888搜索：.gmhelp find <关键词>|r", SENDER_CLOSE, 0)
    player:GossipMenuAddItem(0, "|cff888888关闭|r", SENDER_CLOSE, 0)
    player:GossipSendMenu(100, player, MENU_ID)
end

local function encode(cat, value)
    return cat * PAGE_FACTOR + value
end

local function decode(value)
    value = math.floor(tonumber(value) or -1)
    local cat, sub = math.floor(value / PAGE_FACTOR), value % PAGE_FACTOR
    if not CATEGORIES[cat] or sub < 0 then return nil, nil end
    return cat, sub
end

local function showCategoryPage(player, cat, page)
    cat, page = math.floor(tonumber(cat) or -1), math.floor(tonumber(page) or 0)
    local list = CATEGORIES[cat] and categoryEntries(cat) or nil
    if not list or page < 0 then say(player, "|cffff0000无效分类。|r"); return end
    local maxPage = math.max(0, math.floor((#list - 1) / MENU_PAGE_SIZE))
    if page > maxPage then page = maxPage end
    local first = page * MENU_PAGE_SIZE + 1
    local last = math.min(first + MENU_PAGE_SIZE - 1, #list)
    player:GossipClearMenu()
    for i = first, last do
        local item = list[i]
        player:GossipMenuAddItem(0, "|cff00ff00" .. item.entry.cn .. "|r  " .. item.entry.cmd,
            SENDER_ENTRY, encode(cat, item.index))
    end
    if page > 0 then player:GossipMenuAddItem(0, "<< 上一页", SENDER_PAGE, encode(cat, page - 1)) end
    if page < maxPage then player:GossipMenuAddItem(0, "下一页 >>", SENDER_PAGE, encode(cat, page + 1)) end
    player:GossipMenuAddItem(0, "返回分类", SENDER_BACK, 0)
    player:GossipMenuAddItem(0, "关闭", SENDER_CLOSE, 0)
    player:GossipSendMenu(100, player, MENU_ID)
end

local function runFind(player, keyword, coreOnly)
    keyword = trim(keyword)
    if keyword == "" then
        say(player, "用法：.gmhelp find <关键词>；只查核心：.gmhelp core <关键词>")
        return
    end
    local static = coreOnly and {} or staticSearch(keyword)
    local core, coreAvailable = queryCore(keyword, 30)
    say(player, "|cff00ccff===== GM帮助搜索「" .. keyword .. "」=====|r")
    for i = 1, math.min(18, #static) do
        local e = static[i].entry
        say(player, "|cff00ff00[项目]|r |cffffff00" .. e.cmd .. "|r  " .. e.cn)
    end
    for i = 1, math.min(18, #core) do
        local e = core[i]
        say(player, "|cff00ccff[核心]|r |cffffff00." .. e.name .. "|r  " .. shortHelp(e.help))
    end
    if #static == 0 and #core == 0 then say(player, "|cffff8800没有匹配指令。|r") end
    if not coreAvailable then say(player, "|cffff8800world.command当前不可读；已显示静态项目目录结果。|r") end
    if #static > 18 or #core > 18 then say(player, "结果较多，请缩小关键词。") end
end

local function verify(player)
    local seen, duplicate = {}, nil
    for _, e in ipairs(entries) do
        if seen[e.cmd] then duplicate=e.cmd; break end
        seen[e.cmd]=true
    end
    local q = G23.WorldQuery("SELECT COUNT(*) FROM command", "gmhelp:verify", false)
    say(player, "P3A项目指令条目：" .. #entries .. "；分类：" .. #CATEGORIES ..
        "；重复：" .. (duplicate or "0"))
    if q then say(player, "world.command核心帮助条目：" .. q:GetUInt32(0))
    else say(player, "world.command核心帮助条目：当前查询不可用") end
end

local function handle(player, args)
    local sub, rest = trim(args):match("^(%S*)%s*(.-)%s*$")
    sub = (sub or ""):lower(); rest = rest or ""
    if sub == "" or sub == "menu" then showCategories(player)
    elseif sub == "find" or sub == "search" then runFind(player, rest, false)
    elseif sub == "core" then runFind(player, rest, true)
    elseif sub == "cat" then
        for id = 1, #CATEGORIES do say(player, id .. ". " .. CATEGORIES[id] .. "（" .. #categoryEntries(id) .. "条）") end
    elseif sub == "list" then
        local catText, pageText = rest:match("^(%S*)%s*(%S*)$")
        local cat = findCategory(catText or "")
        if not cat then say(player, "未知分类；使用.gmhelp cat查看分类。")
        else showTextList(player, categoryEntries(cat), pageText) end
    elseif sub == "all" or sub == "custom" then showTextList(player, categoryEntries(0), rest)
    elseif sub == "verify" then verify(player)
    else runFind(player, trim(args), false) end
    return false
end

-- categoryEntries(0) means all entries for text listing.
local originalCategoryEntries = categoryEntries
categoryEntries = function(cat)
    if cat ~= 0 then return originalCategoryEntries(cat) end
    local out = {}
    for index, entry in ipairs(entries) do out[#out + 1] = {index=index, entry=entry} end
    return out
end

local function onGossip(event, player, object, sender, intid, code)
    if sender ~= SENDER_CAT and sender ~= SENDER_ENTRY and sender ~= SENDER_PAGE and
        sender ~= SENDER_BACK and sender ~= SENDER_CLOSE then return end
    player:GossipComplete()
    if G23.GetRank(player) < 1 then say(player, "|cffff0000GM帮助需要GM等级1。|r"); return end
    if sender == SENDER_CAT then showCategoryPage(player, intid, 0)
    elseif sender == SENDER_ENTRY then
        local cat, index = decode(intid)
        local entry = index and entries[index] or nil
        if entry and entry.cat == cat then detail(player, entry) else say(player, "无效指令条目。") end
    elseif sender == SENDER_PAGE then
        local cat, page = decode(intid)
        if cat then showCategoryPage(player, cat, page) end
    elseif sender == SENDER_BACK then showCategories(player) end
end

G23.RegisterCommand("gmhelp", "完整GM指令目录、无状态分类菜单及world.command实时搜索", "GM工具", 1, handle)
RegisterPlayerGossipEvent(MENU_ID, 2, onGossip)

G23.runtime.gmhelpEntryCount = #entries
G23.runtime.gmhelpCategoryCount = #CATEGORIES
print("[G23-P3A] custom_gmhelp.lua 已加载 -- 项目目录" .. #entries .. "条 + world.command按需搜索")
