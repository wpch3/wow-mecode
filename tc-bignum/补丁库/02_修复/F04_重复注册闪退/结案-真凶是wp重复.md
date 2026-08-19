# 结案：闪退真凶 = `AddSC_wp_commandscript` 被写了两遍

> 2026-08-04
> 和 step43 / cs_botrename.cpp **完全无关**，我之前两轮的猜测都错了。

---

## 一、真凶在第 1 节，不在第 4 节

你的诊断 v2 第 1 节：

```
### 1. AddSC 注册次数（每个应正好 2 次）
  AddSC_wp_commandscript                        4 次  [!! 异常 !!]
```

**这一行就是答案。** 其它所有 AddSC 都正常（脚本只列出异常项）。

### 对照上游原文

我实查了上游 `cs_script_loader.cpp`：

```
59:void AddSC_wp_commandscript();       <- 声明，1 次
105:    AddSC_wp_commandscript();       <- 调用，1 次
总行数 106，AddCommandsScripts() 在第 63 行
```

**上游是 2 次，你的是 4 次。**

而且你的 `AddCommandsScripts()` 起点是 **86 行**（上游 63 行），
说明声明区多了 23 行 —— 陆续加东西的过程中，
**把 `wp` 那两行连带复制了一份**。

---

## 二、为什么这会崩

```cpp
// cs_wp.cpp:1067
void AddSC_wp_commandscript()
{
    new wp_commandscript();          // 调两次 = new 两个对象
}
```

```cpp
// ScriptMgr.cpp:942  AddScript()
for (auto const& entry : _scripts)
    if (entry.second.get() == script)     // <- 只比【指针】
        { ...去重... }
```

两次 `new` 得到**两个不同指针**，去重逻辑抓不到，
两个都被注册进 CommandScript 列表。

然后 `wp` 这个命令表（含 7 个子命令）被加载两遍：

```cpp
// cs_wp.cpp:61
{ "wp", rbac::RBAC_PERM_COMMAND_WP, false, nullptr, "", wpCommandTable },
// 子命令: add / event / load / modify / unload / reload / show
```

```cpp
// ChatCommand.cpp:39
ASSERT(!_invoker, "Duplicate blank sub-command.");
//     ^^^^^^^^^ 第二次注册时这里已经有 invoker 了 -> 崩
```

**崩溃时机也对得上**：你的日志最后一行是

```
Initialize commands...
```

正是加载命令表的那一步。

---

## 三、修复（2 分钟）

打开 `D:\TrinityCore\src\server\scripts\Commands\cs_script_loader.cpp`

### 3.1 先定位

Git Bash：

```bash
grep -n "AddSC_wp_commandscript" /d/TrinityCore/src/server/scripts/Commands/cs_script_loader.cpp
```

你会看到 **4 行**，类似：

```
59:void AddSC_wp_commandscript();
6X:void AddSC_wp_commandscript();        <- 多余
105:    AddSC_wp_commandscript();
1XX:    AddSC_wp_commandscript();        <- 多余
```

### 3.2 删掉多余的两行

保留：
- **声明区**（`void AddCommandsScripts()` 那行**之前**）留 **1 行** `void AddSC_wp_commandscript();`
- **函数体内**留 **1 行** `    AddSC_wp_commandscript();`（有 4 空格缩进）

**删掉重复的那两行。**

### 3.3 顺手全查一遍

改完再跑一次确认，应该输出"全部正常"：

```bash
bash 诊断脚本v2.sh /d/TrinityCore/src
```

### 3.4 重新编译

只改了 `cs_script_loader.cpp` 的内容，**不用重跑 CMake**。

---

## 四、第 4 节那一大堆重名 —— 全是正常的，不用管

这是我脚本的说明没写清楚，让你白担心了。

### 为什么正常

那些重名**全是子命令**，各自挂在不同父命令下：

```
"add"     -> .deserter instance add / .deserter bg add / .disable add / .gobject add
"info"    -> .arena info / .event info / .gobject info / .guild info
"go"      -> .go（顶层）/ .npcbot wp go / .npcbot go
"free"    -> .npcbot list spawned free / .npcbot delete free / .npcbot free
```

命令表是**树形**的，不同分支下同名完全合法：

```cpp
// ChatCommand.cpp:62-66
ChatSubCommandMap* subMap = &map;
for (size_t i = 0, n = (tokens.size() - 1); i < n; ++i)
    subMap = &((*subMap)[tokens[i]]._subCommands);   // 一层层往下钻
((*subMap)[tokens.back()]).LoadFromBuilder(builder);
```

`.npcbot add` 和 `.gobject add` 的 `add` 落在**不同的 `_subCommands` map** 里，
互不干扰。

### 唯一真正危险的情况

**同一个完整路径**被注册两次 —— 比如 `wp add` 出现两次
（因为 `AddSC_wp_commandscript` 跑了两遍，整棵 wp 子树都翻倍了）。

**这正是你遇到的。**

### 顺带说明：你自己加的命令都没问题

我核对了第 4 节里所有涉及你自定义文件的条目：

```
"model"  cs_appearance.cpp:460   顶层，和 .npc set model / .npcbot debug model 不冲突
"say"    cs_say.cpp:311          顶层，和 .npc say 不冲突
"dummy"  cs_dummy.cpp:683        顶层，和 .debug dummy 不冲突
"item"   cs_itemforge.cpp:545    顶层，和 .list item / .lookup item 不冲突
"set"    cs_combathelper.cpp:934 顶层，和 .account set / .npc set 不冲突
"mail"   cs_worldtools.cpp:989   那是数据表不是命令，误报
"clean"  cs_smartadd.cpp:566     顶层，和 .lfg clean 不冲突
"spawn"  cs_smartadd.cpp:564     顶层，和 .npcbot spawn 不冲突
```

**全部安全，一个都不用改。**

---

## 五、第 7 节的编码报告要重新解读

```
botcommands.cpp        中文行数=0    无BOM      [!! 中文可能乱码 !!]
cs_botrename.cpp       中文行数=180  无BOM      [!! 中文可能乱码 !!]
cs_playerbot.cpp       中文行数=245  无BOM      [!! 中文可能乱码 !!]
```

### 5.1 botcommands.cpp 中文行数 = 0 —— 说明中文别名【还没加】

你说已经存成 UTF-8 BOM 了，但脚本显示"无BOM"且"中文行数=0"。

**两种可能**：

1. **你存的是 `D:\TrinityCore` 里的文件，但中文别名段还没插进去**
   （step41 的 `botcommands_中文别名.md` 那 21 行）
   -> 中文行数=0 正好印证这一点

2. 存的时候没选"带签名"

**先确认第 1 点**：

```bash
grep -n "招募\|信息\|解雇" /d/TrinityCore/src/server/game/AI/NpcBots/botcommands.cpp
```

**没有输出 = 中文别名段根本没加进去**，那当然用不了。

### 5.2 cs_botrename.cpp / cs_playerbot.cpp 无 BOM

这两个**有大量中文**（180 行 / 245 行）却没有 BOM。

但你说 `.pbot` 的中文别名**能用** —— 这说明 MSVC 把它们当
UTF-8 读对了（可能你的 VS 有相关设置，或者代码页恰好兼容）。

**既然能用就先别动**，改坏了反而麻烦。等这次崩溃修好、
确认 `.botname` 中文正常之后，再统一处理编码。

---

## 六、我这三轮的失误，都记下来

| 轮次 | 我的判断 | 实际 |
|---|---|---|
| 第1轮 | "多半是你把 AddSC_botrename 贴了两遍" | 错。它是 2 次，正常 |
| 第2轮 | v1 脚本只查 botname/pbot 两个命令名 | 漏了真凶 `wp` |
| 第3轮 | —— | v2 第 1 节抓到了，但我该在第 4 节标注"子命令重名正常" |

**教训**：
1. **诊断脚本的"正常/异常"判定要写在最显眼处**，
   v2 第 1 节其实已经打了 `[!! 异常 !!]`，但被第 4 节几百行淹没了
2. **第 4 节这种全量输出必须自带解读**，
   不能甩给用户几百行让他自己判断

下一版脚本我会加一个**「结论摘要」放在最顶部**，
把 `[!! 异常 !!]` 的项目集中列出来。

---

## 七、验证清单

```
[ ] grep 确认 AddSC_wp_commandscript 只剩 2 处
[ ] 重新编译（不用重跑 CMake）
[ ] 服务端能启动，过了 "Initialize commands..." 这一步
[ ] .wp show          上游 wp 命令正常
[ ] .botname          显示帮助
[ ] .pbot             显示帮助
[ ] grep 确认 botcommands.cpp 里到底有没有中文别名段
```
