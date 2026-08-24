# TrinityCore 3.3.5 NPCBot 魔改工程

> 仓库：`328950225/TrinityCore-NPCBOT-Eluna-zhCN`
> 分支：`NPCBOT-Eluna-zhCN-2026`
> 环境：Windows + VS2022 + CMake GUI + DBeaver
> 源码 `D:\TrinityCore` / build `D:\TC-Build` / 客户端 `D:\WOW`

---

## 目录导航

```
待办总表.md              【最重要】总账本 + 最高铁律 + 坑表（每次必看）
未完成想法-总清单.md      所有承诺过但没做的事，A-G 分类

补丁库/                   所有交付物
  01_功能/  A01-A34       新功能（按主题排序）
  02_修复/  F01-F17       bug修复 / 编译错误 / 崩溃
  03_归档_早期散件/        step1-14 时期的零散文件

规划/                     设计方案（还没动工的）
指令清单/                 指令总表
流程文档/                 安装/编译/CMake 步骤
参考资料/                 技术调研笔记
剧情/                     剧情文案
tools/                    自检工具
sql/  conf/  lua_scripts/ 配套资源
```

---

## 每次交付前必跑的自检

```bash
cd /home/user/tc-bignum

# 【.cpp 必跑】真编译器语法检查（g++ -fsyntax-only，能抓变量重定义等）
bash tools/syntax_check.sh 你的文件.cpp

# 中文源码编码体检（6项：UTF8/BOM/吃行/GBK/行尾反斜杠/括号）
python3 tools/check_encoding.py 你的文件.cpp

# 【中文变问号了？】先分类诊断，再决定怎么救
#   自动分【致命-代码】【重要-字符串】【可缓-注释】【正常-三元运算符】
python3 tools/find_mojibake.py 你的文件.cpp

#   【扫全目录必须带基线】否则会误报上游自带的问号（286 vs 1）
#   先拉干净源码：cd /tmp && git clone --depth 1 -b NPCBOT-Eluna-zhCN-2026 <仓库> tcsrc
python3 tools/find_mojibake.py --dir /d/TrinityCore/src --baseline /tmp/tcsrc/src

# 【复制代码后必跑】清理隐形字符（NBSP/零宽/弯引号）
#   症状：VS 弹「某些 Unicode 字符未能保存到当前代码页」或 C2018/C2059
python3 tools/fix_nbsp.py 你的文件.cpp          # 只检查
python3 tools/fix_nbsp.py --fix 你的文件.cpp    # 修复（自动备份.bak）

# SQL 七项自检（存储过程/库名/解析/LIKE主键/GBK/【多表DELETE别名】/【被注释的SQL】）
python3 tools/check_sql.py 你的文件.sql

# 纯文档 GBK 兼容
python3 gbk_check.py 你的文件.md
```

---

## 当前状态（2026-08-24）

### 【权威检查点：与下方历史批次冲突时，以本节为准】

- **唯一整体安装入口：** `00-当前整体安装步骤_单文件入口.md`。以后每批源码、配置、SQL或客户端交付除原始文件外，都必须同步更新这一份完整安装、部署、验收与回滚步骤，用户不需要再翻多份历史文档。
- **架构来源不能忘：** 本工程是在 `328950225/TrinityCore-NPCBOT-Eluna-zhCN` 的 `NPCBOT-Eluna-zhCN-2026` 架构上持续改进和优化；`wpch3/wow-mecode` 是用户成果/规划同步仓库，不是另一套 TC 架构。
- **完美客户端最终目标已锁定：** 不能停在纯服务端绕行或3.3.5原版客户端硬限制。最终必须同步完成高清人物模型、真实ADT/WMO城市与建筑、新可玩种族、新职业、新地图/新世界、NPC专用种族，以及真龙史诗任务/场景/世界状态，并建立持续跟进官方12.x（截至2026-08-21为12.1）的功能差分账。跨领域权威总纲：`规划/G22_客户端魔改整合/02-完美客户端总纲_模型城市种族职业世界主线.md`。21/32种族槽、职业槽、Wow.exe/GlueXML/DBC/MPQ能力先由CP0对用户真实客户端实证；主城禁飞只是G17-A安全一期，最终完成城市空域后要开放真正全世界飞行。
- **G19 第3步：编译、数据库与启动加载已通过。** 数据库四字段齐全，`179`条文本中`54`条有scene标签，场景台词`14/14`，cooldown全为`900`；只剩游戏内主城/野外/副本与900秒冷却矩阵，不重跑SQL。
- **G11：第2步human最小验收通过。** 同一PID 40秒刺激得到总命中28、human 28、playerbot 0、缺字段0、`G11_HUMAN_MINIMUM_PASS=True`；不重复探针。T1–T10/性能仍待补做。
- **G17：20C-1已按用户摘要关闭。** 第20B-S的map0/map1/map530飞行烟雾已PASS；用户又确认主城内召唤拒绝、外飞入暴风城会落地、PVP战场表现相同并声明20C-1执行完成。禁止重复主城/PVP；A4明确室内及精确A6原始字段未单独提供，不补造。其余矩阵尚未完成，安全一期不能冒充最终全世界飞行。
- **G17-B1已关闭；B2R1真实Windows Apply/World迁移/MSBuild PASS，但真人Runtime三项FAIL，当前唯一动作是B2R2。** 用户反馈B2R1技能2特效太简陋、技能3不按玩家方向而固定方向转弯冲刺（增强掉头）、技能4所有坐骑都不能用。B2R2：技能2只用项目真实DBC已审计Kit(44/696/13709/13481/1066)做双层启动+550ms尾流+1100ms交替气浪+极速/结束双冲击环，不引入未验证ID；技能3删除陈旧_smoothedTravelHeading基线，7节点路径锚定骑乘者施法朝向、水平零偏航，根除掉头；技能4保留52226“飞行器着陆”（任务道具技能），新增OnCheckCast对G17龙放行（覆盖原任务/道具施法条件，即全坐骑不可用根因）+OnEffectHit抑制Dummy+AfterCast幂等兜底。源码B2R1后像ff185d99...c4fc -> 3b92e815...c7f1，安全回滚=B2R1字节像；32/32测试、未知SHA零写入、World绑定守护三状态PASS。唯一包G17B2R2_Runtime_Final_Fix_Windows_20260824.zip（96490字节，SHA=c93df481...3225，一键CMD=01_Install_Build_G17B2R2.cmd；安装器R2FIX：PS1哈希读取改为函数作用域二次匹配（$Matches不再为空）、SQL门marker与脚本PassMarker对齐（CASTABLE）、README/状态文档横幅与后像哈希与payload逐字一致。待用户Windows一键Apply+SQL+MSBuild+三行Runtime。B1/B2/B2R1旧包与客户端R1–R5禁止重跑。
- **F44R1 `.combo`阻断门槛已转后续：** 旧F44真人功能FAIL；F44R1本地回归、Windows Check/Apply、VS2022增量编译及正确哈希新二进制启动均PASS（PID 16556，exe SHA=`07a8f952...a86c1`）。用户真人定性反馈“没什么太大的问题”并授权转下一步；未回传A至D全职业结构化矩阵，不补造、不重复Check/Apply/编译/启动。
- **P2R1已修复，当前执行G23-P3A：** 用户确认`.tp`分类菜单恢复正常。P3A新增`.server`统一助手，并用153条/11类无状态目录+`world.command`按需搜索完整化`.gmhelp`；原生`.server info/restart/shutdown`安全透传。本地/ZIP PASS，当前按10号Check→停服→Apply→启动，无SQL/编译，禁止`.reload eluna`。
- **治疗血线决策：** 默认值按场景递进且不全局抬高；游戏内用`.buff scene`选择5人本/团本/高级团本。任意自奶/队友/救命阈值和其它辅助开关另立可回滚批次，不扰动当前稳定F44R1。
- **御龙术边界不缩减：** G17-A/R2是普通3.3.5旧世界安全飞行门，R1只是可控载具骨架。B1全部坐骑接管已关闭；B2旧实现已真实部署但体验FAIL；B2R1三技能体验重制的真实源码应用、World迁移、MSBuild与新鲜产物门均已PASS，当前只待客户端Runtime；B3独立类型战斗页、B4玩家骑乘施法、B5自动寻路/固定、B6客户端体验和压力加固仍未实现。
- **PBot/NPCBot完成度纠偏：** PBot目前只有基础上线/手工管理通道，其余跟随AI、战斗、职业、任务、生活、社交、经济和规模化均按未完成处理；旧A25/A27/A39/A41勾选不能代表完整PBot。NPCBot已有较成熟Creature战斗伙伴基础，但自主冒险后四层、长期记忆、身份、背包经济、真实生活、Eluna和大规模仍未完成。权威清单见 `规划/04-PBot与NPCBot真实完成度及后续路线_2026-08-21.md`。
- **G16/AHBot：共享池根因已证实，0/0/250 conf 已由用户安装。** 安装前数据库基线为唯一挂单 `39,498`、有效 `37,307`、过期 `2,191`，全部属于账号2角色“鲤鱼”，没有玩家挂单；新配置补货后的 99,250 验收尚未回传。用户已锁定每个 eligible entry 至少 `10` 条有效独立挂单（堆叠件数不替代行数），且传说品质只允许坐骑/宠物；现有随机补货和 Orange 总预算还不能保证。下一入口是 G16 文档 `12`/`14` 的完整 SQL 结果，以及 `15-每entry十条与传说过滤_源码探针和回传清单.md` / `probe_g16_min_stock.py`。

以下保留旧批次记录用于追溯；其中状态判断可能已被上面的权威检查点取代。

### 历史记录（F22）

- **`.npcbot move` 闪退已定位** —— `botcommands.cpp:3597 ASSERT(bot)`
  与 **F21 是同一份脏数据的两个出口**：`characters_npcbot` 有行 + `creature` 表无实体
  - F21 崩在启动加载组队（`botdatamgr.cpp:1224`）-> 清数据
  - F22 崩在 move 命令（`botcommands.cpp:3597`）-> 改代码加防护
  - **两个都要做**：只清数据以后还会崩，只改代码 F21 那条仍崩
- `.npcbot spawn` **已查证不会崩**（全程没调 `FindBot`，9 处检查全是软失败），
  但失败回滚不干净，是脏数据源头之一，补丁里给了可选修复
- 见 `补丁库/02_修复/F22_move指令闪退/`

### 已验证可用

- `.pbot` PlayerBot 上线/传送/**交易**/组队/**公会加入**
- `.bf` `.bd` bot 召集与诊断
- `.emote` `.say` `.disguise` `.findmodel` 剧情与外观
- 伙伴关怀（bot 会主动给你东西）
- 游荡bot 可招募、可交互

### 待验证

- `.botname` 改名生效（`02_修复/F09`）
- party1 刷屏静默（`02_修复/F11`）

### 已停用（有替代方案）

- **`.pin`** 游荡bot永久化 —— 会崩服，原因见 `02_修复/F17_pin停用_当前`
  **替代：`补丁库/01_功能/A35_bot批量永久化`（纯SQL，支持上千个）**

### 新交付待验证

- **A35 + A36 永久化组合** —— 解决了「永久」和「游荡」互斥的根本矛盾
  - A35：写两张表 -> bot 永久存在
  - **A36（本次）**：改 `bot_ai.cpp:247`，`creature.ScriptName='wanderer'`
    的固定bot也能游荡。官方 `.npcbot spawn` 不受影响
  - 配合 A30 批量造模板 -> **成百上千个永久游荡bot**

### 下一步（用户定的顺序）

```
A37 六步计划：
  [x] 第1步 数据层        4表+49条台词，已跑通
  [x] 第2步 NPCBot赠予    已验证「给东西没问题，台词也是」
  [x] 第3步 情感反馈层    已编译通过
  [x] 第4步 羁绊计分      已编译通过
  [~] 第5步 bot主动索要   已交付，待编译
  [~] 第6步 PlayerBot版   已交付，待编译（A37收官）

  [x] 第5步 bot主动索要   已编译（索要触发待排查）
  [x] 第6步 PlayerBot版   已编译，交易完美运行

现在：
  G11 bot自主冒险+偶遇玩家自动组队【用户最终目标·进行中】
    规划/G11_bot自主冒险/01-总体设计.md  五步计划已出
    第1步 索要机制重做  已交付验证
    第2步 感知层        <- 下一步做这个
  F30 三问题（party1刷屏/whois刷屏/队长/公会签名）待验证
  PlayerBot 跟随 + 自动上线
  .npcface 指令

新增交付（2026-08-18 第三十三批）· 两个真因最终修复【都不是用户的错】：
  规划/G19_情境对话系统/11-最终修复_两个真因.md
  真因1 C2601+C2181: 函数被插进 if...else 正中间
     18796 那个 } 【缩进4空格】闭合的是if块不是函数结尾，后面还跟着 else if
     用户按我说的"找顶格的}"做了，但缩进4空格的}肉眼与第1列几乎无差别
     括号 3403/3403 差值0 -> 没多删少删，纯粹是我判据不精确
  真因2 C2660: PickGiftText 被误加4参数(声明6/调用10)
     【我的锅】06号说"搜PickText改5处"，但 PickGiftText 含子串 PickText
     搜索一并命中，我没提醒"那是另一个函数不要动"
  修复: 一条脚本同时修两处(assert校验+自动备份)
     已用同构模拟文件本地跑通: if/else恢复相邻、函数顶格移出、PickGiftText回6参数
     另附手动改法 + 4条自检
  坑表新增2条: 不要让用户肉眼判断缩进(给 grep -n "^}" 定位);
              "搜X改N处"必须列出哪些名字像但不要动

新增交付（2026-08-18 第三十二批）· 剩余3个错误诊断：
  规划/G19_情境对话系统/10-最后三个错误_诊断.md
  进展: bot_companion.cpp 已【零错误】-> 组1(_giftTexts)+组2(删775-827) 都成功
  剩余3错全在 bot_ai.cpp，是两个独立问题:
    问题1 C2601+C2181(:18800/:18822) 用户贴的那段我验过确实顶格，故只剩:
       假设A 有两份GetCurrentScene(旧的带缩进没删干净)
       假设B 上一个函数没真正闭合(那个}只闭合了内层if/for)
       A要删B要补，改法相反 -> 必须先拿数据不能猜
    问题2 C2660(:9693) 现在是【真错误】非连锁:
       .cpp已零错=头文件能正常解析 => "不接受10个参数"是字面意思
       推断 bot_companion.h:181 的PickGiftText【多行声明】(跨3行)
       在编辑_textCooldowns时被误删了第2或第3行
  诊断命令一次拿全A-F六段，输出存 D:\diag3.txt

新增交付（2026-08-18 第三十一批）· 最终修复文档【用户要求不要模糊描述】：
  规划/G19_情境对话系统/09-最终修复_直接照做.md
  用户自检确认：bot_companion.cpp 孤立块 = 第775-827行(53行)，828行起是PickItem
  用户原话：「不要用模糊描述了，直接用完整的字段替换给我说，你这样好麻烦」
  -> 全文无"搜索/在下面加"，只给精确行号+完整块+一键脚本：
     第1步 删775-827: VS的Ctrl+G操作步骤 + 带assert校验和自动备份的Python脚本
     第2步 bot_companion.h: 给【3行->4行】完整替换块 + private段最终样貌对照
     第3步 一条总自检(括号平衡/关键符号/PickText只1处/pool[0]->Text须0/顶格)
  脚本已本地实测: 833行模拟文件删除后括号差值0，防呆assert能正确拦截错误行号
  坑表新增1条: 禁止让用户自己找删除边界，必须给行号+脚本+对照块

新增交付（2026-08-18 第三十批）· 组1+组2 精确修法【用户诊断已定位】：
  规划/G19_情境对话系统/08-两个根因的精确修法.md
  组1 真因: grep -c "_giftTexts" bot_companion.h = 0
      private段里 "// A37第3步：礼物反馈台词" 注释孤零零悬着，
      成员被 _textCooldowns 顶掉了（我说"在它下面加"，用户理解成替换那一行）
      -> PickGiftText 其实没丢(:181, 返回 CompanionGiftText const*,
         不是我07号猜的 std::string，幸好标了"以诊断为准"没让用户改)
      -> bot_ai.cpp:9693 的 C2660 是 .h 解析失败的连锁，修好自动消失
      -> 修法：把 std::vector<CompanionGiftText> _giftTexts; 加回去（1行）
  组2 真因: :774 是新PickText正确收尾，:775 是【孤立的 {】
      = 旧函数体没删干净（只替换了签名那一行）
      -> 修法：删掉 775 起的整个旧函数体，文档给了大括号配对脚本算准确范围
  组3 用户已自行改对（GetCurrentScene 顶格），验证通过
  06号文档已同步修正两处表述缺陷
  坑表新增2条：禁用"在它下面加"(要给完整对照块)；
              "整函数替换"必须先给配对脚本算删除范围

新增交付（2026-08-18 第二十九批）· G19第3步编译错误修复：
  规划/G19_情境对话系统/07-编译错误修复.md
  用户编译报11个错 = 【3个根因 + 1个连锁】，不要按行号逐个改：
    组1 C2660+C2065x3+C2530+C2143x2 -> bot_companion.h 里
        PickGiftText声明 和 _giftTexts成员 被误删（替换PickText签名时选多了）
    组2 C2447(bot_companion.cpp:775) -> PickText整函数替换后大括号不配对
    组3 C2601+C2181(bot_ai.cpp:18800/18822) -> GetCurrentScene插进了别的函数体内
        【这是我的指令缺陷】06号原文"找到该函数的起始行"是废话，用户无法自检
    组4 LNK1181 -> game项目失败的连锁，修好1-3自动消失
  已同步修正 06 号文档：补上"顶格"自检判据 + grep -n "^uint8 bot_ai::" 验证命令
  括号自检: python -c "d=open('x.cpp').read(); print(d.count('{'),d.count('}'))" 两数须相等
  兜底: 用户分支 bignum-mod，可 git diff 看误删内容，或 git checkout -- 重来
  已入坑表: 让用户插入【完整函数】必须给"顶格"这类可肉眼自检的判据+grep验证命令

G11 感知层可行性实查（趁编译间隙，为最终目标铺路）：
  Group.h:197  bool AddMember(Creature*)  public  [是]
  Group.h:205  bool AddInvite(Player*)    public  [是]
  Group.h:209  bool AddMember(Player*)    public  [是]
  -> 组队层API齐备。五层: 感知->决策->行为->组队->记忆
  -> 建议 G19第3步编译通过后先做感知层（只读不改行为，风险最低）

新增交付（2026-08-18 第二十八批）· F40状态更正 + 拍卖行三个疑问答复：
  规划/G16_bot经济与打工/10-三个疑问的答复_补货慢与改动清单.md

  【重要更正】F40 用户澄清「早就装好了，你给的修复我是最先装的」
    -> 我此前在交接文档里记成"未安装"并列进P0，是【没回执就推断】的错误
    -> 已在 README/待办总表/PROJECT_HANDOFF/manifest 全部改为"已完成"
    -> 已入坑表：状态只有「明确装了」「明确没装」「未知」三种，
       没回执必须标【未知】，不能默认"未安装"（会害下一个代理去补装）

  三个疑问的答复：
  1. 除坐骑外还改了什么 -> 如实列出4项(只影响武器/护甲):
       Class.Quest/Class.Key 1->0, ItemLevel.Min 0->15,
       ItemLevel.Max 0->284, ReqLevel.Max 0->80
  2. 补货很慢 -> 【正常现象】AuctionHouseBotSeller.cpp:952-960
       缺口<=5000 走Normal只补500/周期；用户单行缺口仅202(99.5%已满)
       = 没东西可补，不是速度问题。Boost(5000)只在rebuild后触发
  3. Class.Misc 改回2 -> 支持用户判断，但连带效果：权重总和64->58,
       分母变小使其余类别配额全部上升(TradeGood 6203->6845 等)
       这正是"其他货物还没补满"的原因

新增交付（2026-08-18 第二十七批）· G19第3步精确补丁【04号作废】：
  规划/G19_情境对话系统/06-第3步_精确补丁_基于你的真实源码.md
  探针后重写。04号文档假设 PickText 在 :189，用户实际 :684（差495行），照04号装会贴错位置
  【探针让这批缩水，4项原计划改动被证明不需要】：
    A42修复 -> bot_companion.cpp:273 已有 score+=100 //种族最优先，已装(F37批次)
    DBCStores.h -> bot_ai.cpp:33 已有
    GameTime.h  -> bot_companion.cpp:17 已有
    _careChatTimer -> 运行时读 BotCfg::GetCompanionChatCooldown()
                      = conf 项 NpcBot.Companion.ChatCooldown(默认600000)，零编译
  实际改动：bot_companion.h(3) + .cpp(2) + bot_ai.h(1) + bot_ai.cpp(1定义+5调用)
  两个必踩的坑已标红：
    1) PickText 声明必须【去掉末尾 const】(要写_textCooldowns)，否则 C2678
    2) SELECT 下标从4起【全部后移3位】，错一位台词全乱
  评分：地图200 > 区域150 > 种族100 > 场景80 > 职业50（保持F37种族优先）
  验证：g++真编译 + 8项测试全PASS，含"20次发言覆盖8/8种台词"
  F40：【用户指示不装】"闪退早就修好了，不要重复安装"

新增交付（2026-08-18 第二十六批）· 第二批编译（探针先行）：
  规划/G19_情境对话系统/05-第3步装机前探针.md
  用户grep暴露：源码基线与G19第3步(04号)文档假设不符
    PickText 实现在 :684 而非文档写的 :189（差495行，文件已大改）
    CompanionText 已有 BotRace(.h:53)，且有两个struct各带一个(:53/:108)
  疑似发现：bot_companion.cpp:273 "score += 100; //种族最优先"
           = A42修复(PickGiftText权重)特征 -> 可能已装，第二批可划掉
  探针：Git Bash 一条命令，A-I 共9段，输出存 D:\probe_g19.txt
  F40【不用等探针，可直接装】：botmgr.cpp:803 / Map.cpp:1065-1073
       原文已在上游4e8762e逐行实查匹配
       Unit.h:1242 SetOwnerGUID / :1261 SetCreator 均为 public，编译无碍

新增交付（2026-08-18 第二十五批）· 坐骑最终方案B【推翻我自己的A】：
  规划/G16_bot经济与打工/09-坐骑修复_最终方案B.md
  用户实测：坐骑 bonding 0->13种 / 1->295种 / 3->2种，合计310种
            拍卖行实际仅3种(全bonding=0)，证实我挡掉了295种(95.2%)
  【收回08号推荐的修法A】: Bind.Pickup=1 会连带放行全服所有BoP装备,
     而用户刚说"其他物品都丰富了" -> 等于推翻上一批调好的"不杂乱"
  改用修法B: 295种坐骑塞进 forceIncludeItems 白名单
     依据 AuctionHouseBotSeller.cpp:123-129 白名单push_back+continue跳过绑定过滤
     -> Bind.Pickup 保持0, BoP装备继续被挡
  容量: AuctionHouseBot.h:220 是 std::string -> unordered_set, 无长度上限
  【必须在主conf原地追加】: Config.cpp:166 put_child 是覆盖语义,
     写进 ahbot.conf 会把现有359项整个冲掉

新增交付（2026-08-18 第二十四批）· 坐骑「只有三种」真因【第2次返工】：
  规划/G16_bot经济与打工/08-坐骑只有三种_真因与修法.md
  用户纠正：「物品数量不是问题，而是只有三种坐骑物品」
  -> 07号文档诊断错了：不是配额(数量)问题，是【池子】(种类)问题
  真因：我为干掉金色战刃关掉了 AuctionHouseBot.Bind.Pickup
        而坐骑绝大多数是 BIND_WHEN_PICKED_UP (ItemTemplate.h:99)
        AuctionHouseBotSeller.cpp:138-140 全部 continue 丢弃 -> 池子只剩3种
  修法A(推荐)：Bind.Pickup 改回 1 + 金色战刃entry 加进 forceExcludeItems
        依据 :120 黑名单在 :123 白名单和 :132 绑定过滤【之前】，优先级最高
  修法B(临时)：保持0，把坐骑entry批量塞进 forceIncludeItems(无视绑定过滤)
  当时曾要求保留 Class.Misc=8；2026-08-19 已按用户最终决定改回2，总量根因另由共享池0/0/250修复
  状态：等用户跑【查询1】(按bonding分组统计坐骑种类)决定用A还是B

新增交付（2026-08-18 第二十三批）· 坐骑配额修正【我的失误】：
  规划/G16_bot经济与打工/07-坐骑变少的根因与修正.md
  用户当时回报：三行各39498（后来确认是共享池重复视图，不能合计）；金色战刃已消失、其他物品丰富
            但「坐骑种类和数量还是少」
  根因=我上一版把 AuctionHouseBot.Class.Misc 从用户原值 5 改成 2
    坐骑就在这一类: ItemTemplate.h:311 ITEM_CLASS_MISCELLANEOUS=15
                    ItemTemplate.h:521 ITEM_SUBCLASS_JUNK_MOUNT=5
    配额: 单行3151->1369, 三行9452->4107 (砍57%)
    种类也少的原因: AddNewAuctions 用 SelectRandomContainerElement 随机抽样,
                    抽样次数=配额, 配额腰斩 -> 大量款式一次都没抽中
  当时修正: Class.Misc = 8（历史；2026-08-19 已按用户最终决定改回2）
        不设10是因为会挤占 TradeGood(10) 材料主力份额
  已核实安全: Mount.ReqLevel/ReqSkill 四项全0=不过滤(AuctionHouseBotSeller.cpp:279-289)
              LockBox.Enabled=0 与坐骑无关(:293-300)
  附诊断SQL: 坐骑种类数/挂单数 vs 全服坐骑总数, 一次分清"配额问题"还是"池子问题"
  状态: 零编译, 等用户复制conf + .reload config + .ahbot reload + .ahbot rebuild all

第二十二批实测结果（2026-08-18 用户回报，**后经源码核对已纠错**）：
  拍卖行 [旧口径作废] Alliance/Horde/Neutral 三行是同一共享 map 的重复视图，不能相加；唯一挂单以 2026-08-19 SQL 诊断为准
                金色战刃已消失, 其他物品丰富
  台词   [通过] 141条
  时间线 [通过] 能看到

新增交付（2026-08-17 第二十二批）· 第一批零编译三件套：
  规划/第一批_零编译_执行手册.md   <- 【从这里开始做】
  1. 拍卖行  conf/worldserver.conf.d/ahbot.conf （新增，31项）
     改法从"主conf手工替换23处"改成"丢一个文件进 conf.d"，主conf一字不动
     依据 Main.cpp:203 LoadAdditionalDir + Config.cpp:166 put_child 覆盖语义
     已实证：无BOM(会让boost ini_parser抛错)/CRLF/configparser解析31项全对
     附带修复 conf/切换档位.bat：旧第55行 `for %%f in ("%DIR%\*.conf")`
       通配符会把 ahbot.conf 一起改名成 .off 静默关掉
       -> 改成只遍历 casual/adventure/epic/hardcore/legend 五个档位名
  2. 台词 141 条（G19 第1步60条 + 第2批81条）
     已校验：ID无重复、无越界、条数与声明一致
     前置：需 npcbot_care_text 有 bot_race 列（A42第2步的ALTER），手册里给了自检SQL
  3. G20 时间线试点 SQL
     【修了会直接报错的硬伤】8处 `id1` -> `id`
     依据 ObjectMgr.cpp:2170 "SELECT creature.guid, id, map..." + 官方sql/updates/
     creature表列名是 id；creature_template 才是 entry
  实查澄清：AuctionHouseBotSeller.cpp:235-236 等级过滤只在
     case ITEM_CLASS_ARMOR/WEAPON 分支内 -> 设ItemLevel.Min=15不会误杀材料
  免重启生效：.reload config 然后 .ahbot reload（顺序不能反，
     因 AuctionHouseBot.h:240 Reload() 只读内存不读磁盘）

新增交付（2026-08-09 第二十一批）：
  F43 近战游荡bot锁定不前进【已g++验证+4项逻辑测试】
    补丁库/02_修复/F43_近战bot锁定不前进/根因与修复.md
    真凶：bot_ai.cpp:15716 UpdateImpossibleChase 无条件 return true
          -> GetInPosition 在 :5567 提前退出
          -> :5614 真正的 BOT_MOVE_CHASE 永远走不到
    为什么只有近战中招：:15706 远程判 dist>40（40码内直接过），
          近战判 !IsWithinMeleeRange（不贴脸就一直命中）
    附带发现：:15711 BotMovement(BOT_MOVE_POINT, target,...) 把 Unit*
          隐式转成 Position*（WorldObject:WorldLocation:Position），
          语义变成"走到目标此刻坐标"且不寻路 -> 对活动目标等于原地打转
    改法：游荡近战bot 跳过 UpdateImpossibleChase，走标准 MoveChase

新增交付（2026-08-09 第二十批）：
  F42 招募游荡bot闪退【日志一次定位，已g++验证】
    补丁库/02_修复/F42_招募游荡bot闪退/根因与修复.md
    真凶：bot_ai.cpp:19058  _travel_node_cur->GetMapId() 空指针
    因果：SetBotOwner全程没清 _wanderer（全源码无UnsetWanderer）
          -> bot变"既有master又是wanderer"混合态
          -> Evade -> GetHomePosition -> IsWanderer()为真 -> 读已失效路点 -> 崩
    与职业无关（日志里是战士只是恰好）
    改动1 招募时清 _wanderer + 路点（治本）
    改动2 GetHomePosition 三层判空（兜底，保护4个调用点）
    附带发现：游荡bot无creature表记录(botdatamgr.cpp:452)，
              原版else分支对它同样会空指针，一并防了

新增交付（2026-08-09 第十九批）：
  F42 招募游荡bot闪退【要日志，不再猜】
    补丁库/02_修复/F42_招募游荡bot闪退/定位方案.md
    澄清：F39改动2是【新增】代码不是原有的，所以用户搜不到
          -> 给了 F39改动2+F41改动3 的合并版（已g++验证）
    可疑点（未验证）：SetBotOwner 全程没清 _wanderer 标记，
      全源码只有 SetWanderer() 没有 UnsetWanderer()
      -> 招募后变成"既有master又是wanderer"混合态
    但我已错3次(F37/F38/F40)，这次必须先拿崩溃日志再动手

新增交付（2026-08-09 第十八批）：
  F41 游荡bot不打中立怪 + 站着发呆【用户指出F39漏了，已g++验证】
    补丁库/02_修复/F41_游荡bot不打中立怪与发呆/根因与修复.md
    真因A：bot_ai.cpp:3616 中立怪(faction 35)被直接 return false
           只有 faction 2150(敌视所有) 的bot能打，普通游荡bot用种族默认阵营
    真因B：F39 我加了 me->IsInCombat() 条件，
           但中立怪不主动打人，bot永远进不了战斗 -> 那段代码根本不执行
    改动3处：放行中立怪 / 保护功能NPC(实查7个API) / 去掉IsInCombat限制
    自查抓到：我编的 IsBankerNPC 不存在，正确是 Unit.h:1109 IsBanker()

新增交付（2026-08-09 第十七批）：
  G22 客户端魔改整合【我之前的疏漏，现在补上】
    规划/G22_客户端魔改整合/01-世界改造的客户端账.md
    关键分水岭：「往废墟里【加】东西」纯服务端能做
               「把废墟【变】完好」必须改ADT动客户端
    -> G20 路线修正：路线2「在旧址旁重建」零客户端且叙事更好
    -> 时间线用 phaseMask（3.3.5原生，零客户端）
    Noggit Red 在 GitLab 不是 GitHub，预编译版只在 Discord
  G20 时间线试点SQL【零客户端，可直接跑】
    规划/G20_世界重塑与时间线/02-时间线试点_零客户端.sql
    银月城渴魔症危机：phase1正常 / phase2渴魔症
    18条SQL全过自检；已按坑表规则改成零会话变量版（子查询取guid）

新增交付（2026-08-09 第十六批）：
  【总路线图v2】规划/总路线图-v2-含世界线.md
    v1只规划了bot线，这版补上世界线(G20)和主线落地(G21)
    盘出6个真空区，最严重的是"主线4.3万字游戏里一个字看不到"
    三个月双线并行计划 + 需要你拍板的3件事
  G21 主线落地【新建】规划/G21_主线落地/01-让故事进入游戏.md
    实查故事集有完整八卷结构+等级区间(序卷1-15 ... 终卷100+)
    -> 天生能挂进成长曲线，不用重新设计
    方案A .story指令(1天) / 方案B 8个讲述者NPC(1周,纯SQL) / 方案C 真任务链(以后)
    和G20的关系：G21让玩家【知道】，G20让玩家【参与】

新增交付（2026-08-09 第十五批，历史记录）：
  G19 第3步 场景感知【04 号后来被真实源码补丁 06、09-11 取代；现已完整编译】
    规划/G19_情境对话系统/04-第3步_场景感知实现.md（作废，勿重装）
    解决"对应场景说对应话"+"所有台词都能用上"+"不要太杂乱繁忙"
    scene/zone_id/map_id 三字段 + 冷却池轮换 + 闲聊间隔改3-6分钟
    评分：地图200 > 区域150 > 种族100 > 场景80 > 职业50
  G16 第3步 解决货物杂乱
    规划/G16_bot经济与打工/06-第3步_解决货物杂乱.md
    真因：你的 ItemLevel.Min/Max + ReqLevel.Min/Max 四个全是0=不过滤
    AuctionHouseBotSeller.cpp:238 写法 if(uint32 value=...) -> 0跳过过滤
    -> 1级破布和80级紫装混在一起。修法：15/284/0/80
    警告：ReqLevel.Min 千万别设非0，材料药水 RequiredLevel=0 会被全滤掉

新增交付（2026-08-09 第十四批）：
  F40 登出闪退真凶【日志锁定，已g++验证】
    补丁库/02_修复/F40_剑圣镜像导致登出闪退/根因与修复.md
    真凶：剑圣镜像分身(entry 70552)没跟主人一起清理
    -> creator变野指针 -> Eluna ObjectVariables.ext:43 访问失效对象 -> 崩
    Map.cpp:1052 上游自己标了 "tempfix"，:1067 else分支只打日志不清理
    F37/F38 保留（修的隐患是真的，只是不是这次病因）

新增交付（2026-08-09 第十三批）：
  F38 切角色闪退【F37修错了地方，真凶在这】
    补丁库/02_修复/F38_切角色闪退真凶/根因与修复.md
    botmgr.cpp:805 在玩家登出路径上：FindMap()判了空，GetEntry()没判
    且 FindMap() 被调3次。解释了"退出没事、进另一个角色才崩"
  F39 游荡bot卡位不追击【已g++验证】
    补丁库/02_修复/F39_游荡bot卡位不追击/根因与修复.md
    bot_ai.cpp:18367 mmover = !IAmFree() ? master : nullptr
    游荡bot是自由身 -> 恒为nullptr -> 非战场时拿不到移动目标
    顺带修 :18348 假master解引用（HasRealMaster在你的.h里不存在）
  G20 世界重塑与时间线【规划】
    规划/G20_世界重塑与时间线/01-总体设计.md
    修复失落城市 = 尘世线《携手》卷的具体载体
    时间线玩法 = 噩梦线"世界之魂受创"的自然延伸
    补充5个想法：观测台/均衡律世界事件/bot立场/遗物传承/一周试点版

新增交付（2026-08-09 第十二批）：
  F37 切角色闪退 + A42种族失效【两个真因都找到了】
    补丁库/02_修复/F37_切角色闪退与A42种族失效/根因与修复.md
    闪退：botmgr.cpp:180 混用 GetMap()(有ASSERT) 和 FindMap()
          切角色瞬间 m_currMap 失效 -> ASSERT炸 -> 无提示闪退
          影响10个调用点，改这一处全受益
    A42：我的评分权重错了。A37台词27/32是精确item_kind(40分)，
         A42种族台词全是kind=7(10分) -> 永远选不中
         修法：种族100 > 职业50 > 物品种类20（已写程序验证）

新增交付（2026-08-09 第十一批）：
  【总路线图】规划/总路线图-2026-08-09.md   <- 回答"还有什么没做完"
    三层视角：能用85% / 像人60% / 活着15%
    三阶段执行顺序 + 每阶段验收标准
    已决定不做的5项（附理由，避免反复讨论）
  G19 第2批情境台词 81条（零编译）
    规划/G19_情境对话系统/03-第2批_情境台词81条.sql
    职业互相调侃/天气时间/装备战利品/危险感知/种族文化/羁绊/幽默/战斗短句/主动提议
  G16 第2步 Buyer价格调优（零编译）
    规划/G16_bot经济与打工/05-第2步_Buyer价格调优.md
    【重要纠正】刷钱风险比我上次说的小：AuctionHouseBotBuyer.cpp:168
    参照物是物品自身SellPrice不是Baseprice -> bot只买接近商店价的东西

新增交付（2026-08-09 第十批）：
  F36 启动闪退ASSERT【和拍卖行无关】
    补丁库/02_修复/F36_游荡bot数量ASSERT崩服/根因与修复.md
    botdatamgr.cpp:1885 可用模板801 < 配置802
    修法：NpcBot.WanderingBots.Continents.Count = 802 -> 750（留余量）
    G7 早就预判过这个崩溃点，现在应验
  三个问题答复
    规划/答复-三个问题-2026-08-09.md
    拍卖行conf只是第1步，第4步(pbot真挂单)才是质变点
    问号武器：exe补丁解决不了，必须补客户端 Item.dbc（我之前说法有误）
  A42 修复：第2步改错了函数
    补丁库/01_功能/A42_种族职业口音/修复-第2步改错了函数.md
    给礼物走 PickGiftText 不是 PickText，我只改了后者

新增交付（2026-08-09 第九批）：
  G16 拍卖行修正版【基于你上传的真实conf重写】
    规划/G16_bot经济与打工/04-第1步修正_基于你的真实conf.md
    纠正我两个错判：Buyer.Enabled你早就=1；forceInclude你已配359项
    金色战刃真凶：白名单(:124)会跳过绑定过滤 + Bind.Pickup=1 两条路
    -> 只关 Bind.Pickup，白名单一个字不动（含233项WLK坐骑段）
    共23处替换，已按你的现值逐条列出
  G19 第1步【零编译版】闲聊扩充 60条
    规划/G19_情境对话系统/02-第1步_零编译版闲聊扩充.sql
    实证 CARE_TYPE_CHAT=4 已存在且只校验 <7 -> 纯SQL不用改代码
    通用20 + 职业20 + 种族20，SQL五项自检通过

新增交付（2026-08-09 第八批）：
  A42 第2步 种族口音【已g++验证+逻辑测试6项全过】
    补丁库/01_功能/A42_种族职业口音/第2步_种族口音.md
    四级评分匹配：职业+20 种族+10，自动分级不写if-else
    关键：用 BotMgr::GetBotPlayerRace(me) 不用 me->GetRace()
          （自定义职业如黑暗游侠，后者给的是模型种族）
    10族台词各3条 + 10条索要（id段500-699）
  新想法清单【5个没用过的现成钩子】
    规划/新想法-2026-08-09.md
    最推荐：OnTextEmote(你/hug它就抱回来) / 更多care_type / bot之间对话
    发现 OnChat(Group*) 可绕开"NPCBot没session"的架构墙 -> G14有救

新增交付（2026-08-09 第七批）：
  F35 bot_ai.cpp/.h 中文修复【已交付完整文件】
    补丁库/02_修复/F35_bot_ai中文修复/源文件/bot_ai.cpp + bot_ai.h
    诊断：代码0损坏，只有82处注释+1处字符串
    校验：行数不变/括号平衡/GBK兼容0问题/BOM+CRLF
  find_mojibake.py 修正误报（286 -> 2）
    加 --baseline 基线对比 + 只认连续问号

新增交付（2026-08-09 第六批）：
  A41 修复：四个函数插进了类内部（C2352+LNK1181）
    补丁库/01_功能/A41_pbot自动上线/修复-函数插错位置.md
    正确锚点是 cs_playerbot.cpp:106 g_pbots（类外），不是 :200 SpawnBot（类内）
  G19 情境对话系统【A42的正确形态】
    规划/G19_情境对话系统/01-总体设计.md
    用户纠正：不只是口音，要"到副本说副本的话，到主城说主城的话"
    核心：{zone}占位符自动填中文地名 + 评分制匹配 -> 4500条压到300条
    已实证：AreaTableEntry.AreaName[16]含中文、AREA_FLAG_CAPITAL覆盖7主城

新增交付（2026-08-09 第五批）：
  A42 种族/职业口音【第1步纯SQL，零编译】
    补丁库/01_功能/A42_种族职业口音/改动清单.md
    重要发现：A37建表时已有 bot_class 字段，
             bot_companion.cpp:198 已实现"职业专属优先、回退通用"
             -> 职业口音只需插台词，不用改代码
    第1步：10职业各3条礼物反馈 + 10条索要台词（id段300-499）
    第2步：种族需加 bot_race 字段（NpcBotExtras.race / Unit.h:893）待做

新增交付（2026-08-09 第四批）：
  A41 pbot自动上线【G18第1步，已g++验证】
    补丁库/01_功能/A41_pbot自动上线/改动清单.md
    主人上线 -> 名册查询 -> 延迟5秒队列 -> 自动拉起pbot
    新表 characters.playerbot_roster（SQL七项自检通过）
    核心：抽出 PBotSpawnCore（原SpawnBot依赖ChatHandler无法复用）

【已归档·不再投入】ElvUI刷屏
    F34探针两次为0 -> 服务端没发这句话，是客户端本地字符串
    5处改动全部无害，保留不撤（F32的IsMember还附带修好右键踢bot）

新增交付（2026-08-09 第三批）：
  F34 ElvUI刷屏【抓真凶探针】<- 当前阻塞项，等你贴日志
    补丁库/02_修复/F34_ElvUI刷屏抓真凶/探针与两条路.md
    ElvUI源码已拉取实查：github.com/ElvUI-WotLK/ElvUI
    实查结论：UninviteUnit 只在"解散队伍"弹窗触发，whois 0个调用
             -> 两条猜测都被推翻，必须插探针拿证据
    探针A WorldSession.cpp:728 SendNotification（黄字总出口）
    探针B GroupHandler.cpp:60  SendPartyResult（队伍错误出口）

新增交付（2026-08-09 第二批）：
  F32 ElvUI刷屏根治【真凶已锁定】
    补丁库/02_修复/F32_ElvUI刷屏根治/根因与修复.md
    根因：Group.cpp:2846 IsMember 只查真人容器，NPCBot 在 m_botMemberMgr
    F29改错了地方（改的是按名字版:381，ElvUI走按GUID版:295）
  F33 NPCBot套装装备失败 + 传家宝过滤
    补丁库/02_修复/F33_套装给bot失败/根因与修复.md
    三个独立原因：.gearset bot 只发背包 / 等级门槛 / 评分倒退保护
    应急办法：.gm on 再 AUTOEQUIP（现在就能用）
  G17 全世界飞行 + 御龙术
    规划/G17_飞行与移动/01-全世界飞行方案.md（历史取证，禁止施工）
    规划/G17_飞行与移动/02-重新审计_全世界飞行与御龙术分阶段实施计划.md（当前权威）
    规划/G17_飞行与移动/03-v1真实报告诊断与v2探针更正.md
    规划/G17_飞行与移动/probe_g17_source.py（schema-2真实源码只读入口）
    补丁库/02_修复/G17R1_御龙入座与纯飞行坐骑/（R1原件/后像/安装器/客户端DBC补丁器）
    规划/G17_飞行与移动/G17R1_Runtime_Fix_Windows_20260822.zip（用户Windows构建与车辆Runtime PASS，禁止重复）
    规划/G17_飞行与移动/G17R1_Client_MPQ_Install_Windows_20260822.zip（有效历史包；实际安装报告UNKNOWN，禁止无目的重复）
    补丁库/02_修复/G17R2_纯飞行坐骑服务端严格门/（当前R2前后镜像/安装器/10项测试）
    规划/G17_飞行与移动/G17R2_Pure_Flying_Server_Gate_Windows_20260823.zip（Windows PASS用户确认；禁止重复）
    规划/G17_飞行与移动/G17R2A_Flight_Gate_Diagnostic_Windows_20260823.zip（真实PASS；禁止重复）
    规划/G17_飞行与移动/G17R3_Pure_Flight_Client_AreaTable_Windows_20260823.zip（真实安装PASS但客户端单标志Runtime FAIL；禁止重复）
    补丁库/02_修复/G17R4_客户端旧世界双飞行标志与双槽加载/（原版/R3/R4 Area及逐行测试）
    规划/G17_飞行与移动/G17R4_Pure_Flight_Dual_Area_Flags_Windows_20260823.zip（历史R4；真实安装PASS、Runtime FAIL）
    规划/G17_飞行与移动/G17R5_Effective_zhCN_Locale_DBC_Mirror_Windows_20260823.zip（历史R5；真实Runtime PASS，禁止重复）
    补丁库/01_功能/G17B1_全坐骑自动接管与类型会话/（B1原件/后像/安装器/测试）
    规划/G17_飞行与移动/G17B1_All_Mounts_Auto_Intercept_Windows_20260823.zip（旧native runner卡死；永久废弃）
    规划/G17_飞行与移动/G17B1R1_Native_Runner_Fix_Windows_20260823.zip（真实构建已完成；时间戳后检假FAIL；禁止重跑）
    补丁库/02_修复/G17B1R1_PowerShell原生参数卡死修复/（原生参数修复原件/后文件/证据）
    补丁库/02_修复/G17B1R2_PDB前时间戳快照假失败修复/（规范脚本修正；用户无需执行）
    补丁库/02_修复/G17B1R3_权威座位占用验证与误清理修复/（座位接管Runtime行为PASS）
    规划/G17_飞行与移动/G17B1R3_Authoritative_Seat_Verification_Windows_20260823.zip（已执行；禁止重复）
    补丁库/02_修复/G17B1R4_室内实时安全清理修复/（真实室内Runtime PASS；已被B1R5取代）
    规划/G17_飞行与移动/G17B1R4_Continuous_Indoor_Safety_Windows_20260823.zip（已执行；禁止重复）
    补丁库/02_修复/G17B1R5_包装坐骑与无降落伞清理修复/（已验收关闭的B1R5基线）
    规划/G17_飞行与移动/G17B1R5_Wrapper_Mount_No_Parachute_Windows_20260823.zip（已验收关闭；禁止重跑）
    补丁库/02_修复/G17B2_完整御空术与特色降落/（历史B2；真实部署后体验FAIL，禁止重跑）
    规划/G17_飞行与移动/G17B2_Complete_Flight_Typed_Landing_Windows_20260823.zip（历史失败基线；禁止重跑）
    补丁库/02_修复/G17B2R1_三技能体验重制/（B2前像、B2R1后像、安全回滚、World迁移、52项测试与证据）
    规划/G17_飞行与移动/G17B2R1_Runtime_Experience_Rework_Windows_20260824.zip（已执行Windows构建PASS，禁止重跑；73526字节；SHA 94822d39...8daa）
    规划/G17_飞行与移动/G17_通用御龙与战斗坐骑总设计_20260823.md（B1-B6权威设计；B1已关闭，B2离线完成待Runtime，B3-B6未实现）
    A安全世界飞行1–3天；B0原生御龙2–5工作日；B1近似动态1–2周；B2客户端物理长期研发
  全局审视
    规划/全局审视-2026-08-09.md
    功能:修复=40:33 的信号 / 上游改了8个文件 / SQL无版本管理

新增交付（2026-08-09）：
  F31 队长巡检真因【已实证+g++验证】
    补丁库/02_修复/F31_队长巡检真因/根因与修复.md
    真因：一次性钩子 vs 每tick巡检。参考 mod-playerbots 实现
  G16 第1步 拍卖行大改【零编译，改conf】
    规划/G16_bot经济与打工/03-第1步_拍卖行大改_conf.md
    修金色战刃bug + 三行全开 + 上万挂单 + 每小时补货

新增规划（2026-08-09）：
  G16 bot经济与打工【拍卖行 + 让机器人做更多事】
    规划/G16_bot经济与打工/01-总体设计.md   五步计划已出
    规划/G16_bot经济与打工/02-第1步_拍卖行体检.sql
    第1步、第2步【零编译】，可随时插队先做
```

### G16 三个实证结论（2026-08-09）

- **你的服 AHBot 已经在跑** —— `Seller.Enabled=1` 开着，`Buyer.Enabled=0` 关着，
  `Update.Interval=1600`（默认20，等于26分钟才补一次货）
- **NPCBot 当不了拍卖行卖家** —— `owner` 是裸低位 guid，客户端走
  `QueryHandler.cpp:81` CharacterCache 查名字，NPCBot 查不到。**PlayerBot 可以**
- **上游 bug**：`AuctionHouseBotSeller.cpp:935 和 :938` 双调 `AddAuction`
  -> `OnAuctionAdd` 钩子触发两次，挂钩子前必须先处理

---

## 最高铁律（用户 2026-08-02 立）

> **不能因为难就不做 / 不能因为做不到就不做**
> **不能因为他们想让我们不做就不做 / 不能因为他们做不到我们就不做**

说"做不到"前必须回答：
1. 具体挡住哪个函数/哪行（要行号）
2. 绕过它需要什么
3. 有没有别的实现路径

---

## 血泪教训 TOP 5（完整版见 待办总表.md 坑表）

1. **中文源码一律存 UTF-8 带 BOM** —— 无BOM在中文Windows下会被编辑器转成GBK，
   导致注释吃掉下一行，报 `C2447 { 缺少函数标题`

2. **验证二进制内容用 `grep -a` 不要用 `strings`** ——
   strings 只提取 ASCII，中文永远搜不到

3. **做状态诊断前先实证"正常状态长什么样"** ——
   `.pin` 连续返工4次的根因就是把游荡bot的正常状态判成了"损坏"

4. **照抄代码必须查【调用前提】** ——
   官方 `.npcbot spawn` 的落库四步对新建bot有效，对游荡bot会崩服

5. **括号平衡 != 语法正确** —— `check_encoding.py` 只查编码和括号，
   交付 .cpp 前必须再跑 `bash tools/syntax_check.sh`（真 g++ 语法检查）

6. **看到保护性检查挡路，先问"为什么有这道检查"** ——
   `Creature.cpp:1429 //disallow saving generated bots` 是保护不是bug，
   绕过它 = 拆安全气囊
