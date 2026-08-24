# 第 4 步：在新 build 目录直接启动 + 验证

**你的方案**：不替换 exe，直接在 `D:\TC-Build\bin\RelWithDebInfo\` 里跑。
data 已复制、conf 已复制。这种做法更干净，exe 本来就在原地。

**当前状态**：数据库 8/8 列已扩展，测试物品 900001 已就位。

---

## 一、⚠️ 先理解：旧 exe 已经不能用了

我实测了你当前数据库配不同 exe 的结果：

```
你的库现在有一件 stat_value1=100000000 的物品

[旧 exe] GetInt16()  读 100000000  -> ASSERT -> worldserver 启动即崩溃
[新 exe] GetInt32()  读 100000000  -> 安全，正常加载

抗性列 holy_res=500000000
[旧 exe] GetUInt8()  读 500000000  -> ASSERT -> 崩溃
[新 exe] GetUInt32() 读 500000000  -> 安全
```

你用新 build 直接跑，正好避开了这个问题。**老的备份 build 从现在起不能再启动**
（除非先删掉 900001 那件物品）。

---

## 二、启动前必须核对 3 项配置

你复制的是**备份的 conf**，里面的路径可能指向老目录。逐项确认：

### 2-1. DataDir —— 最容易出错

配置文件里默认是相对路径：

```ini
DataDir = "data"
```

**这表示 exe 同级目录下的 `data` 文件夹**，即：

```
D:\TC-Build\bin\RelWithDebInfo\data\
    ├── dbc\
    ├── maps\
    ├── vmaps\
    ├── mmaps\
    └── Cameras\
```

Git Bash 确认：

```bash
ls /d/TC-Build/bin/RelWithDebInfo/data/
```

**应该看到 dbc / maps / vmaps / mmaps 这几个文件夹。**

如果你的 conf 里 DataDir 写的是绝对路径（比如指向老服务端目录），
那也行，只要路径存在即可。检查一下：

```bash
grep -n "^DataDir" /d/TC-Build/bin/RelWithDebInfo/worldserver.conf
```

### 2-2. 数据库连接串

```bash
grep -nE "^LoginDatabaseInfo|^WorldDatabaseInfo|^CharacterDatabaseInfo" /d/TC-Build/bin/RelWithDebInfo/worldserver.conf
```

格式是 `IP;端口;用户名;密码;库名`，例如：

```ini
LoginDatabaseInfo     = "127.0.0.1;3306;trinity;trinity;auth"
WorldDatabaseInfo     = "127.0.0.1;3306;trinity;trinity;world"
CharacterDatabaseInfo = "127.0.0.1;3306;trinity;trinity;characters"
```

**重点确认库名和你刚才改的那三个库一致**
（就是你执行 SQL 时用的 `world` / `characters` / `auth`）。

> 注意默认模板里的库名可能是 `world_new`、`auth-bot`、`characters-bot`
> 这类带后缀的，取决于你这个整合包。用你备份的 conf 里的值即可。

### 2-3. Eluna 脚本路径

```bash
grep -nE "^Eluna.Enabled|^Eluna.ScriptPath" /d/TC-Build/bin/RelWithDebInfo/worldserver.conf
```

默认：

```ini
Eluna.Enabled = true
Eluna.ScriptPath = "data\lua_scripts"
```

确认这个目录存在：

```bash
ls /d/TC-Build/bin/RelWithDebInfo/data/lua_scripts/ 2>/dev/null
ls /d/TC-Build/bin/RelWithDebInfo/lua_scripts/ 2>/dev/null
```

> 编译时 CMake 会自动把 `extensions` 复制到
> `bin\RelWithDebInfo\lua_scripts\extensions\`（你已确认存在）。
> 如果 conf 里配的是 `data\lua_scripts`，而 extensions 在
> `lua_scripts\`，需要把 extensions 挪到 conf 指定的位置，或改 conf 路径一致。

### 2-4. 检查 DLL 是否齐全

```bash
ls /d/TC-Build/bin/RelWithDebInfo/*.dll 2>/dev/null || echo "没有DLL（静态链接，正常）"
```

TrinityCore 默认静态链接，通常不需要额外 DLL。
如果启动时提示缺 DLL，多半是 **MySQL 的 libmysql.dll**，
从 MySQL 安装目录的 `lib` 下复制过来即可。

---

## 三、启动

### 3-1. 先 authserver

双击 `D:\TC-Build\bin\RelWithDebInfo\authserver.exe`

正常显示：
```
Realm running as realm ID xxx
```

**常见错误**：
- `Could not connect to MySQL` -> 检查 authserver.conf 的 LoginDatabaseInfo
- 闪退 -> 用 CMD 进目录运行 `authserver.exe` 看报错

### 3-2. 再 worldserver

双击 `worldserver.exe`，重点看这几行：

**[OK] 成功标志：**

```
Loading Item templates...
>> Loaded 37801 item templates in XXX ms      <- 数字应接近 37801
...
Eluna: Loaded X Lua scripts
...
World initialized in X minutes X seconds
```

**[NG] 如果出现这个 -> 说明用了旧 exe：**

```
Field::GetInt16 on LONG field item_template.stat_value1 ...
caused value to be truncated. Use Field::GetInt32 instead.
```

你用的是新 build，理论上不该出现。真出现了说明启动的不是
`D:\TC-Build\bin\RelWithDebInfo\worldserver.exe`。

**[NG] `Could not find DataDir`：**

-> DataDir 配置或 data 目录位置不对，回到 2-1。

**[NG] 卡在 `Loading...` 不动：**

-> 多半是 maps/vmaps/mmaps 不全，检查 data 目录内容。

---

## 四、游戏内验证

### 4-1. 拿到测试物品

GM 账号登录后：

```
.additem 900001
```

### 4-2. 悬停查看属性

**期望：**

```
测试-十亿之刃  （橙色传说品质）
+100000000 耐力
+500000000 力量
+1000000000 攻击强度
护甲 1000000000
```

### 4-3. 显示旧数值/空白怎么办

**这是正常的客户端缓存，不是失败。**

1. 完全退出游戏
2. 删掉客户端根目录的 **`WDB` 文件夹**（整个删）
3. 重新登录

> `WDB\zhCN\ItemCache.wdb` 缓存了物品数据快照。

### 4-4. ★ 最关键：装备后看血量

装上武器，按 **C** 打开角色面板：

| 检查项 | 期望值 | 异常含义 |
|---|---|---|
| **生命值** | **约 10 亿（正数）** | **0 或负数 = 溢出** |
| 力量 | 5 亿左右 | |
| 攻击强度 | 10 亿左右 | |
| 护甲 | 10 亿左右 | |
| 角色状态 | 存活、能移动 | 一装备就死 = 血量溢出 |

> **为什么盯血量**：`Player::UpdateMaxHealth()` 把耐力 x10 算血量。
> 1 亿耐力 -> 10 亿血，在 uint32（42亿）内安全。
> 若以后耐力调到 5 亿以上，血量超 42 亿会回绕成小数字甚至 0。

---

## 五、[OK] 完成清单

- [ ] `data\` 下有 dbc/maps/vmaps/mmaps
- [ ] conf 里三个数据库连接串指向正确的库
- [ ] Eluna.ScriptPath 与实际 lua_scripts 位置一致
- [ ] authserver 正常启动
- [ ] worldserver 正常启动，**无 truncated 报错**
- [ ] 日志显示 `Loaded 37801 item templates`
- [ ] 日志有 `Eluna: Loaded X Lua scripts`
- [ ] `.additem 900001` 成功
- [ ] 属性显示亿级数值（必要时删 WDB）
- [ ] **装备后血量约 10 亿，角色存活**

---

## 六、🔙 回滚

你现在有两套 build，回滚很简单：

### 想用回老 build（备份的那个）

**必须先删掉大数值物品**，否则老 exe 会崩：

```sql
DELETE FROM world.item_template WHERE entry = 900001;
```

> 列类型是 int 不影响老 exe，**只要没有超过旧上限的数据就行**。
> 所以只删这一件测试物品即可，不必回滚整张表。

### 彻底回滚数据库

```bash
mysql -u root -p world < /c/wow_backup_20260726/item_template.before_alter.sql
mysql -u root -p characters < /c/wow_backup_20260726/item_instance.before_alter.sql
```

---

## 七、完成后告诉我

请回复：

1. **启动日志**里 `Loaded XXXXX item templates` 那行
2. **装备测试物品后的血量数字**
3. 有没有 `Eluna: Loaded X Lua scripts`

确认后进入 **第 5 步：Eluna 验证**，然后正式开始
**指令开发**（五维、配装、幻化等 46 个指令）。
