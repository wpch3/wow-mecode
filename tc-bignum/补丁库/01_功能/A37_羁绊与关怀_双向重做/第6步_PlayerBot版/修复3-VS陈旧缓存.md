# C2011 是 VS 陈旧缓存，不是代码问题

## 决定性证据

### 你的文件（71-98 行原文）

```cpp
71: #ifdef _MSC_VER
72: # pragma warning(push, 4)
73: #endif
74:
75: static constexpr GossipOptionIcon BOT_ICON_ON = GOSSIP_ICON_BATTLE;
76: static constexpr GossipOptionIcon BOT_ICON_OFF = GOSSIP_ICON_CHAT;
77:
78: static constexpr uint32 MAX_AMMO_LEVEL = 13;
79: static constexpr uint8 AmmoDPSForLevel[MAX_AMMO_LEVEL][2] =
80: {
81:     { 80, 91 },
...
90:     { 15,  4 },
```

### 编译器说

```text
C2011 "BotGiftKind"   类型重定义   bot_ai.cpp 79
C2011 "BotGiftReject" 类型重定义   bot_ai.cpp 90
```

### 对不上

| 行号 | 你的文件实际内容 | 编译器认为的内容 |
|---|---|---|
| 79 | `static constexpr uint8 AmmoDPSForLevel[...]` | `enum BotGiftKind` |
| 90 | `{ 15,  4 },`（数组元素） | `enum BotGiftReject` |

**同一行号内容完全不同 = 编译器读的不是你现在这份文件。**

而且 79 和 90 这两个行号，**正好是旧版本里那两个 enum 的位置**
（枚举各占 11 行，79 + 11 = 90，完全吻合）。

**代码已经改对了，是 VS 在用缓存。**

---

## 修法：强制全量重编译

### 方法1：清理 + 重新生成（先试这个）

```text
1. VS 菜单：生成 -> 清理解决方案
2. 等它完全跑完（状态栏显示"清理 已完成"）
3. VS 菜单：生成 -> 重新生成解决方案
```

**注意是「重新生成」不是「生成」** —— 「生成」还是增量编译。

### 方法2：手工删缓存（方法1无效时）

```text
1. 完全关闭 Visual Studio
2. 删除这些（都是可再生的缓存，删了没风险）：

   D:\TC-Build\.vs                       <- 隐藏文件夹，要开显示隐藏项目
   D:\TC-Build\src\server\game\game.dir  <- game 项目的中间文件
   D:\TC-Build\src\server\scripts        下的 *.dir 文件夹

3. 重新打开 TrinityCore.sln
4. 生成 -> 重新生成解决方案
```

### 方法3：核弹级（前两个都无效）

```text
1. 关闭 VS
2. 把整个 D:\TC-Build 删掉
3. 重新跑 CMake GUI：Configure -> Generate
4. 打开新生成的 sln，完整编译一次
```

**这个要 20-40 分钟**，但保证干净。只在前两个方法都失败时用。

---

## 为什么会这样

你这次只改了 `bot_companion.h`（加枚举）和 `bot_ai.cpp`（删枚举）。

MSVC 的增量编译靠**文件时间戳**判断。可能的失败原因：

```text
1. 编辑器保存时时间戳没更新（少见但确实会发生）
2. .vs 文件夹里的 IntelliSense 数据库损坏
3. 之前编译中断过，留下了半成品的 .obj
4. 系统时间被调整过
```

第 3 种最常见 —— 你前面几次编译都报错中断了。

---

## 怎么确认修好了

重新生成后看**输出窗口**（不是错误列表），
应该能看到：

```text
1>正在编译... bot_ai.cpp
```

**如果没有这一行**，说明 VS 又跳过了这个文件，
那就用方法2或方法3。

---

## 顺便：`E0065 MapDefines.h` 也会一起消失

那个是 IntelliSense 的误报（前缀"错误(活动)"），
IntelliSense 数据库重建后就没了。

---

## 我的判断失误

我前两轮一直在找"第三处枚举定义"，
但**从一开始就该核对报错行号和实际内容是否吻合**。

你贴出 71-98 行后 5 秒就定位了 —— 79 行明明是数组，
编译器却说是 enum，这种"行号对不上"是缓存问题的典型特征。

**规则已记入坑表**：报编译错误时，第一步先核对
「报错行号处的实际代码」和「编译器描述的内容」是否一致。
不一致 = 缓存问题，别去改代码。
