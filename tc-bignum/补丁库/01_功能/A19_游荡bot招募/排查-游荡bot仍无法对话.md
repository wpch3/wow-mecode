# 排查：游荡 bot 仍然无法对话

> 用户反馈：「游荡npc还是无法对话」
> 日期：2026-08-02

---

## 【最可能的原因】我给你的 conf 默认是关的

**先检查这个，八成就是它。**

我在 `conf/npcbot_wandering.conf` 里写的是：

```ini
NpcBot.WanderingBot.AllowHire = 0     <- 关闭
```

安装说明第七章我写了「默认 false，想用再开」，但**没在验证清单第一步提醒你改成 1**，
你按第六章「第一步：开关关闭时（默认）」测的话，看到的就是原样行为。

### 修复

打开 `D:\TC-Build\bin\RelWithDebInfo\worldserver.conf.d\npcbot_wandering.conf`

```ini
NpcBot.WanderingBot.AllowHire = 1
```

**重启 worldserver**（这个配置不支持热重载）。

仓库里的 conf 我已经改成 `= 1` 了，你也可以直接覆盖过去。

---

## 如果改成 1 还是不行，按顺序查下面四项

### 检查 1：conf 到底有没有被加载

`worldserver.conf.d\` 目录的坑（都踩过）：

| 检查 | 说明 |
|---|---|
| 文件里有 `[worldserver]` 段头吗 | **没有段头整个文件不加载** |
| 注释是不是独立成行 | 行尾 `#` 注释会让那行失效 |
| 文件扩展名是 `.conf` 吗 | 不是的话不扫描 |
| 目录名对不对 | 必须是 `worldserver.conf.d`，和 exe 同级 |

**验证办法**：把值故意写成一个非法值（比如 `= abc`），
重启看日志有没有报错。有报错说明文件被读了。

### 检查 2：代码改动是不是都到位了（5 处缺一不可）

step33 要改 5 处，**漏任何一处都会导致开关无效**：

```
botconfig.h:57      加 IsWanderingBotHireEnabled() 声明
botconfig.cpp:63    加 _enableWanderingBotHire 变量
botconfig.cpp:315   加 GetBoolDefault("NpcBot.WanderingBot.AllowHire", false)
botconfig.cpp:739   加 IsWanderingBotHireEnabled() 实现
bot_ai.cpp:7707     改条件                      <- 决定能不能【开对话框】
bot_ai.cpp:7761     改条件                      <- 决定【招募按钮显不显示】
```

**最容易漏的是 `bot_ai.cpp:7761`**（安装说明第一章专门强调过）。
只改 7707 的症状是：**对话框能开，但里面没有招募选项**。

**逐条确认办法**：在 VS 里全局搜 `IsWanderingBotHireEnabled`，
应该搜到 **4 处**（h 声明 1 + cpp 实现 1 + bot_ai 里 2）。
少于 4 处就是漏了。

### 检查 3：改动有没有真的编译进去

`bot_ai.cpp` 是两万行的大文件，编译慢。
如果编译时报了错但你没注意，可能用的还是旧的 exe。

**验证办法**：看 `worldserver.exe` 的修改时间是不是刚才。

### 检查 4：你右键的是不是"游荡 bot"

有几种 bot 长得像但不是：

| 类型 | 特征 | 能不能对话 |
|---|---|---|
| 游荡 bot | 无主，在野外走动 | 改完开关后：**能** |
| 别人雇佣的 bot | 跟着别的玩家 | **不能**（设计如此）|
| 召唤物 | `me->IsSummon()` | **不能**（我没改这条）|
| 临时 bot | `IsTempBot()` | **不能** |

**用 step35 的 `.bf` 确认**：

```
.bf
```

列出来的**都是无主游荡 bot**（`.bf` 内部就是按 `owner == 0` 过滤的）。
然后：

```
.bf come
```

把最近的一个叫到面前，**对这个右键**。这样能排除"右键错对象"。

---

## 快速判定表

改成 `= 1` 重启后：

| 现象 | 结论 | 下一步 |
|---|---|---|
| 对话框打开，有招募选项 | **成功** | 测招募 |
| 对话框打开，**没有**招募选项 | 漏了 `bot_ai.cpp:7761` | 补那一处 |
| 对话框还是一闪而过 | 漏了 `bot_ai.cpp:7707`，或 conf 没生效 | 查检查 1、2 |
| 右键完全没反应 | 不是 bot，或距离太远 | 用 `.bf come` |

---

## 如果还是不行，发我这三样

1. `.bf` 的输出截图（确认有游荡 bot）
2. VS 里全局搜 `IsWanderingBotHireEnabled` 的结果数量
3. `bot_ai.cpp:7707` 和 `7761` 两行的**当前原文**

有这三样我能直接定位，不用再猜。
