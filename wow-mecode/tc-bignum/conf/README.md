# conf 档位系统 —— 使用说明

## 一、先回答你的两个问题

### 问题 1：能做配置目录自动加载吗？

**不用做 —— 你的仓库已经内置了。** 我查了你仓库的 `Main.cpp`：

```cpp
#define _TRINITY_CORE_CONFIG_DIR "worldserver.conf.d"

sConfigMgr->LoadAdditionalDir(configDir.generic_string(), true, loadedConfigFiles, configDirErrors);
for (std::string const& loadedConfigFile : loadedConfigFiles)
    printf("Loaded additional config file %s\n", loadedConfigFile.c_str());
```

**只要在 `worldserver.exe` 同目录下建一个叫 `worldserver.conf.d` 的文件夹，
里面扩展名精确为小写 `.conf` 的普通文件都会被递归加载；附加项可覆盖主conf同名项。**

`Config.cpp::LoadAdditionalDir`只判断`configFile.extension() == ".conf"`，没有文件basename字符白名单。因此`g17_world_flight.conf`、`g11_perception.conf`中的下划线合法。为避免Windows路径编码问题，仍建议文件名只用ASCII字母、数字、`_`和`-`。

启动时会打印 `Loaded additional config file xxx`，一眼能确认加载成功。

### 问题 2：改了名字服务端还认吗？

**不认。** `Main.cpp` 里写死了：

```cpp
#define _TRINITY_CORE_CONFIG "worldserver.conf"
```

服务端只找 `worldserver.conf` 这个名字，
直接把 `difficulty_epic.conf` 放旁边它是不会读的。

不过源码里有命令行参数可以指定：

```cpp
("config,c",      value(&configFile), "use <arg> as configuration file")
("config-dir,cd", value(&configDir),  "use <arg> as directory with additional config files")
```

```
worldserver.exe -c difficulty_epic.conf      # 指定主配置文件
worldserver.exe --config-dir 某目录           # 指定附加配置目录
```

---

## 二、所以方案换了（更好）

因为发现了 `conf.d`，我把方案从「5 份完整 conf」改成「5 个小文件」：

| | 旧方案 | **新方案** |
|---|---|---|
| 每份大小 | 5160 行 / 191 KB | **212 行 / 5 KB** |
| 重复内容 | 94% | **0%** |
| 加新配置项 | 5 个文件各改一遍 | **改一个地方** |
| 主 conf | 要替换 | **完全不动** |

旧的完整版我留在 `_完整版备用/` 里，万一你想用整份替换的方式也有。

---

## 三、安装（3 步）

### 1. 复制文件夹

把 **`worldserver.conf.d`** 整个文件夹复制到：

```
D:\TC-Build\bin\RelWithDebInfo\
```

复制完应该是这样：

```
D:\TC-Build\bin\RelWithDebInfo\
├── worldserver.exe
├── worldserver.conf              ← 你原来的，不用动
├── 切换档位.bat                   ← 可选，方便切档
└── worldserver.conf.d\
    ├── casual.conf
    ├── adventure.conf
    ├── epic.conf
    ├── hardcore.conf
    └── legend.conf
```

### 2. 只留一个档位启用

**目录里所有小写`.conf`都会被加载**，所以同时放5个会互相覆盖。源码使用`recursive_directory_iterator`但没有显式排序，**不能依赖文件名字母顺序或“最后一个赢”**。要留哪个档，就把其余4个改成`.conf.off`后缀。

比如要用史诗档：

```
epic.conf              ← 保持
casual.conf.off        ← 加 .off
adventure.conf.off     ← 加 .off
hardcore.conf.off      ← 加 .off
legend.conf.off        ← 加 .off
```

嫌麻烦就用我给的 **`切换档位.bat`**，双击选编号自动改后缀。

### 3. 重启 worldserver

启动时看到这行就说明成功了：

```
Loaded additional config file .../worldserver.conf.d/epic.conf
```

---

## 四、五个档位

| 档位 | 定位 |
|---|---|
| `casual` | 休闲 — 等同你现在这套设置 |
| `adventure` | 冒险 — 保留便利，但把剧情四项还回来 |
| **`epic`** ★ | **史诗 — 为《真龙纪元》设计，推荐** |
| `hardcore` | 硬核 — 接近原版曲线 |
| `legend` | 传奇 — 原版之上再收紧 |

每档 **63 项**，五档项名完全一致（脚本验证过），切档不会残留。

---

## 五、五档已统一修复的问题

无论用哪档，这些都修好了：

| 项 | 你原本 | 改为 | 原因 |
|---|---|---|---|
| `MaxCoreStuckTime` | **0** | 60 | **崩溃看门狗关闭**，服务端卡死不自救 |
| `MaxOverspeedPings` | **0** | 2 | 加速外挂检测关闭 |
| `GameType` | **8** | 0 | 8 = FFA 全场混战，你说不是故意的，改回 PVE |
| `Stats.Limits.*` | 100.0 | 95.0 | 100% 闪避 = 物理免疫，且与 NpcBot 的 95 不一致 |
| `vmap.enableIndoorCheck` | 0 | 1 | 关闭会导致室内骑马/坐标异常 |
| `ChatFlood.MessageCount` | 0 | 10 | 刷屏防护 |
| `Random.ItemStats.Level` | 缺 | 200 | 你只填了 Enable，补上配套项 |

> `GameType` 以后想做 PVP 服改成 1 即可（0=PVE 1=PVP 4=RPPVP 8=FFAPVP）。

> **澄清**：`Eluna.Enabled = true` 我一度以为有问题，但你第 4 步日志证明
> Eluna 正常（17/17 通过），**这条没动**。

---

## 六、史诗档的取舍逻辑

**保留的「爽」**（这些是省时间，不是降难度）：
- 经验 **1 倍** —— 你原本就是，很对。剧情服要在每阶段停留够久
- 掉落高倍率 —— 刷装备是核心玩法，有套装解锁系统兜底
- 飞行点全开 + 瞬间飞行 —— 跑图是纯粹的时间消耗
- 洗点免费 —— 鼓励尝试不同流派
- 采集/制造 10 倍 —— 重复劳动没必要折磨

**收回的「无脑」**：
- **剧情四项**（过场/探索/声望/技能）—— 史诗档的灵魂
- **副本门槛** —— 你做了「刷 ICC 3 次解锁套装」，
  但 `Instance.IgnoreLevel=1` 让 1 级号就能进 ICC，门槛等于没有
- 经济收紧、死亡有代价（但 60 级以下免疫，不劝退）

**新增**：开启天气 —— 剧情里「那天下雨了」是重要场景

---

## 七、注意事项

**conf.d 文件必须有 `[worldserver]` 段头** —— 我生成的都有了，
你自己加文件时别忘了，否则加载会失败。

**不认行尾 `#` 注释**：

```conf
# 错误 —— 值会变成 "1  # 开启飞行"
AllFlightPaths = 1  # 开启飞行

# 正确
# 开启飞行
AllFlightPaths = 1
```

**加载顺序**：源码没有排序，不能假设按文件名字母序。功能conf应使用互不重复的键；五个档位必须只启用一个。

**文件格式**：`.conf`必须无UTF-8 BOM、包含`[worldserver]`段头，注释独占一行。当前`g17_world_flight.conf`首字节为`23 23 23`、无BOM，下划线文件名安全。

---

## 八、以后加配置项

比如马上要做的 GCD 战斗节奏优化，有 15 个配置项。
做法是在 `worldserver.conf.d/` 里再建一个 `speed.conf`：

```conf
[worldserver]

Speed.Enable = 1
Speed.Gcd    = 60
Speed.Cast   = 75
...
```

它和档位文件**同时生效、互不干扰**。这就是 conf.d 的好处 ——
按功能分文件，而不是按档位复制。
