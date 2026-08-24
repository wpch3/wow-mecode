# step64  `.pin` 闪退的真正根因：**游荡bot根本不可能被固定**

> 用户实测：`.pin` 之后 worldserver.exe 直接闪退。
>
> **这次不是修 bug，是要告诉你：`.pin` 这个功能的设计方向从一开始就是错的。**

---

## 一、结论：游荡bot的"身份证"是【临时的】，写进数据库必然崩

### 1.1 游荡bot的 creature_template 不在正常的地方

实查 `botdatamgr.cpp:63`：

```cpp
static CreatureTemplateContainer _botsExtraCreatureTemplates;
//     ^^^^^^ 【文件级 static，纯内存，进程重启就没】
```

`botdatamgr.cpp:344`（生成游荡bot时）：

```cpp
CreatureTemplate& bot_template = _botsExtraCreatureTemplates[next_bot_id];
bot_template.Entry = next_bot_id;
```

**每个游荡bot的模板都是运行时凭空造的，只存在于这个内存容器里。**

### 1.2 ObjectMgr 查模板时会特殊处理它们

`ObjectMgr.cpp:10271-10280`：

```cpp
CreatureTemplate const* ObjectMgr::GetCreatureTemplate(uint32 entry) const
{
    //npcbot: try fetch custom creature template
    if (entry >= BOT_ENTRY_CREATE_BEGIN)
    {
        if (CreatureTemplate const* extra_template = BotDataMgr::GetBotExtraCreatureTemplate(entry))
        {
            //custom creature template should only exist in custom container
            ASSERT_NODEBUGINFO(_creatureTemplateStore.find(entry) == _creatureTemplateStore.end());
            //                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
            //                 【断言：这种entry【绝对不能】出现在正常模板表里】
```

**官方注释写得很清楚**：`custom creature template should only exist in custom container`。

### 1.3 `.pin` 干的事正好违反了这条铁律

```
.pin
  -> SaveToDB()  把 bot 写进 world.creature 表
  -> creature 表里出现了一条 id = 80770 的记录
  -> 重启后 ObjectMgr 加载 creature 表
  -> 发现 id=80770 但 creature_template 里没有这个 entry
     （因为那个模板是内存临时的，重启就没了）
  -> 各种断言/空指针 -> 崩服
```

**而且不用等重启** —— `.pin` 当场就可能崩，因为：

`botdatamgr.cpp:1170`（启动交叉校验，但同样逻辑在运行时也会走）：

```cpp
for (auto const& [_, cdata] : sObjectMgr->GetAllCreatureData())
    if (cdata.id >= BOT_ENTRY_BEGIN
        && sObjectMgr->GetCreatureTemplate(cdata.id)->IsNPCBot()   // <- 这里
        && std::ranges::find(entryList, cdata.id) == entryList.cend())
```

`GetCreatureTemplate(cdata.id)` 对一个刚写进 creature 表、
但模板在临时容器里的 entry，会触发 `ASSERT_NODEBUGINFO` -> **崩**。

### 1.4 还有一个致命点：despawn 时会清掉模板

`botdatamgr.cpp:787-812  CleanExtraBotData`：

```cpp
_botsData.erase(bditr);
_botsExtras.erase(beitr);
_botsExtraCreatureEquipmentTemplates.erase(bwcetitr);
_botsExtraCreatureTemplates.erase(bwctitr);      // <- 【模板被删】
_spareBotIdsPerClassMap[bot_class].insert(original_id);   // <- entry 被回收复用
```

**entry 会被回收给下一个新生成的 bot。**

所以就算 `.pin` 侥幸没崩，creature 表里那条 80770
**下次会对应一个完全不同的 bot**。

---

## 二、为什么之前几次没崩，这次崩了

| 版本 | 行为 | 结果 |
|---|---|---|
| step49-62 | `SaveToDB` 被 `IsWanderer()` 拦住，**什么都没写** | 不崩，但也没成功 |
| **step63** | 我把 `UnsetWanderer()` 提前，**SaveToDB 真的执行了** | **写进 creature 表 -> 崩** |

**上游那道 `if (IsWanderer()) return;` 不是 bug，是保护。**

`Creature.cpp:1429-1431`：

```cpp
//npcbot: disallow saving generated bots
if (IsNPCBot() && GetBotAI() && (GetBotAI()->IsWanderer() || IsSummon()))
    return;
```

注释是 **"disallow saving generated bots"** ——
官方**明确禁止**保存生成的bot。

**我 step63 绕过了这道保护，等于拆了安全气囊。**

---

## 三、那"游荡bot永久化"这个需求还能不能做

**能，但不能用现在这条路。**

你的原始需求（step49 记录）：

> 「给游荡bot做个是否永久存在的开关，可以让npcbot重启就消失或者永久存在」

### 3.1 正确的路：不是"固定游荡bot"，是"复制成真bot"

游荡bot的 entry 是**临时借用**的，不能持久化。
但我们可以**用它的属性，创建一个全新的、真正的 bot**：

```
1. 读取游荡bot的：种族/职业/等级/外观/装备/名字
2. 在 creature_template 表里【新建】一条真实记录（用固定的 entry，比如 71001+）
3. 在 creature_template_npcbot_extras 里写职业种族
4. 在 creature 表里 spawn 它
5. 让原来那个游荡bot消失
```

**这样得到的是一个真正的、可持久化的 NPCBot**，
和 `.npcbot spawn` 出来的一模一样。

### 3.2 这条路的代价

| 项 | 说明 |
|---|---|
| entry 从哪来 | 要维护一个"永久bot"专用的 entry 段（比如 71001-71999） |
| 要写 4 张表 | creature_template / npcbot_extras / npcbot_appearance / creature |
| 外观要复制 | 从 `_botsAppearanceData` 读，写进 appearance 表 |
| 装备要复制 | 从 `_botsExtraCreatureEquipmentTemplates` 读 |

**工作量比现在的 `.pin` 大，但这是唯一正确的做法。**

### 3.3 或者：换个思路，改配置更简单

如果你的真实目的是**"让某些bot一直在某个位置"**，
其实有更简单的办法：

**用 `.npcbot spawn` 创建真正的固定bot**（官方指令，已经能用），
然后把游荡bot数量调低。

```
固定bot（.npcbot spawn）：永久存在，你要的效果
游荡bot：当背景板，随机刷新
```

---

## 四、现在要做的三件事

### 4.1 【紧急】先把 `.pin` 停用，避免你再崩服

我会给你一个版本：**`.pin` 直接拒绝，并说明原因**，
只保留 `.pin status`（诊断用，安全）。

### 4.2 【重要】检查你的库有没有被污染

上次 `.pin` 可能已经往 `creature` 表写了记录，
**那会导致下次启动崩服**。要清掉。

### 4.3 【后续】重新设计"永久化"功能

按 3.1 的思路做"复制成真bot"，
但这个要单独排期，不是现在。

---

## 五、我的错误（第4层，也是最严重的一次）

### 5.1 我拆掉了上游的保护

`Creature.cpp:1429` 的注释白纸黑字写着
**"disallow saving generated bots"（禁止保存生成的bot）**。

我看到 `.pin` 失败，第一反应是**"怎么绕过这道拦截"**，
而不是**"官方为什么要拦"**。

### 5.2 正确的思路应该是

看到一道明确的保护性检查挡路时：

```
错误：想办法绕过去
正确：先问【为什么会有这道检查】
      -> 查它的注释、查 git blame、查相关 ASSERT
      -> 理解设计意图后，再决定是绕过还是换路
```

**如果我 step63 时点进 `Creature.cpp:1429` 看一眼那行注释，
就不会有这次崩服。**

### 5.3 更深一层：我没查"数据从哪来"

游荡bot的 `creature_template` 在 `_botsExtraCreatureTemplates`
（`botdatamgr.cpp:63`，文件级 static）。

**这个信息我在 step49 就该查** ——
"要持久化一个对象，先搞清楚它的数据存在哪、生命周期多长"。

我查了 `characters_npcbot` 和 `creature` 两张表，
**却没查最基础的 `creature_template`**。

**规则：做"持久化"功能前，必须把对象依赖的【所有】数据列出来，
逐个确认它们是否可持久化。有一个是临时的，整个方案就不成立。**

三条已记入坑表。
