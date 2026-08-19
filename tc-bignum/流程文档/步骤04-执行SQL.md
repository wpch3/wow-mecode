# 第 3 步：执行 SQL 扩列（大数值真正生效的一步）

**前置**：第 2 步编译已完成（27 成功 0 失败，`lua_scripts/extensions` 已确认）。
**预计耗时**：10~20 分钟（ALTER 本身几十秒，其余是检查）。

---

## ⚠️ 这一步的顺序为什么不能反

回顾一下我在第一轮实测出的结论：

```
=== DB 列改成 int，但 C++ 读取器还是 GetInt16 ===
col=int val=100000  via GetInt16 -> assert? YES (SERVER CRASH)

=== DB 列和读取器都改好 ===
col=int val=2000000000 via GetInt32 -> assert? no
```

**你现在已经编译好了新 exe（读取器是 GetInt32），所以可以安全执行 SQL 了。**

但有一个**关键前提**：

> 🔴 **执行 SQL 后，必须用新编译的 worldserver.exe 启动。**
> 如果还用旧的 exe（在你原服务端目录里那个），一读到大数值就会
> `ASSERT` 崩溃。

---

## 3-0. 先停服 + 再备份一次

```
1. 关闭 worldserver.exe
2. 关闭 authserver.exe
```

虽然第 0 步备份过，但 ALTER 是不可逆操作，**再备份一次最保险**：

```bash
cd /c/wow_backup_20260726
"/c/Program Files/MySQL/MySQL Server 8.0/bin/mysqldump.exe" -u root -p world item_template > item_template.before_alter.sql
"/c/Program Files/MySQL/MySQL Server 8.0/bin/mysqldump.exe" -u root -p characters item_instance > item_instance.before_alter.sql
```

> 路径按你实际 MySQL 版本改。用 HeidiSQL/Navicat 导出也行。

✅ 确认两个 .sql 文件不是 0 KB。

---

## 3-1. 先跑自检（不改任何东西）

执行 **`sql/00_preflight_check.sql`**（已更新到 v3）

> ### [!] 已修复两个问题
>
> **问题 1：只显示一行结果**
> 那其实是脚本跑完了 —— 多条 SELECT 产生多个结果集，
> HeidiSQL / Navicat 默认只展示最后一个，前面的在其他结果标签页里。
>
> **问题 2：报 `SQL 错误 [1064]` 语法错误**
> 根因**不是 SQL 语法，是文件编码**。我实测确认：
>
> ```
>   ①  U+2460  OK  可编码
>   ±  U+00B1  OK  可编码
>   ✅  U+2705  XX  GBK无法编码 <- 会破坏SQL文本
>   ⬜  U+2B1C  XX  GBK无法编码
>   ❌  U+274C  XX  GBK无法编码
> ```
>
> 你的客户端以 **GBK** 读取文件时，`✅`/`⬜` 这些 emoji 变成乱码字节，
> **破坏了 SQL 字符串的引号配对**，解析器于是在下一个 `UNION ALL` 处报错。
> 所以报错指向 `④ 冲突检查` 那行，真凶却在它上面几行。
>
> **v3 已做两件事**：① 移除全部 GBK 不兼容字符（4 个 SQL 文件都清理过，
> 已复检「剩余 GBK 不兼容字符 = 无」）；② 拆成 4 条独立简单 SELECT，
> 不再用长 UNION 链（MySQL 8.0 对此更严格）。

### v3 实测输出（改造前应该长这样）

```
+------------+---------------+----------------------+
| 库         | 列名          | 当前类型             |
+------------+---------------+----------------------+
| characters | durability    | smallint(5) unsigned |
| world      | arcane_res    | tinyint(3) unsigned  |   <- 上限才 255
| world      | armor         | smallint(5) unsigned |
| world      | holy_res      | tinyint(3) unsigned  |   <- 上限才 255
| world      | ItemLevel     | smallint(5) unsigned |
| world      | MaxDurability | smallint(5) unsigned |
| world      | stat_value1   | smallint(6)          |   <- 核心限制 +/-32767
| world      | stat_value10  | smallint(6)          |
+------------+---------------+----------------------+
```

会有 **4 个结果集**，请切换结果标签页逐个看：

| 结果集 | 内容 | 重点 |
|---|---|---|
| 1 | 列类型 | 改造前全是 smallint/tinyint |
| 2 | 表规模 | 决定 ALTER 耗时 |
| 3 | 现有最大值 | 应都在旧上限内 |
| 4 | 冲突检查 | **900001=0**（你已确认）、**46017=1** |

> 结果集 4 的 `46017` 很关键：若为 **0**，说明你库里没这件装备，
> 需把 `03_test_item.sql` 里的 46017 换成你库里存在的武器 entry。
> 查一个可用的：
> ```sql
> SELECT entry,name FROM world.item_template WHERE class=2 AND Quality>=4 LIMIT 5;
> ```

---

## 3-2 / 3-3. 扩列 -- [OK] 已全部完成

你的最终验证结果 8/8 全 [OK]：

```
| characters.durability | int unsigned | [OK] 已扩展 |   <- 02脚本
| world.arcane_res      | int          | [OK] 已扩展 |
| world.armor           | int          | [OK] 已扩展 |
| world.holy_res        | int          | [OK] 已扩展 |
| world.ItemLevel       | int unsigned | [OK] 已扩展 |   <- 01b脚本
| world.MaxDurability   | int unsigned | [OK] 已扩展 |   <- 01b脚本
| world.stat_value1     | int          | [OK] 已扩展 |
| world.stat_value10    | int          | [OK] 已扩展 |
```

**3-3 无需再执行**，`characters.durability` 已经是 int unsigned。

### 过程中发现的客户端特性（重要，后续都受影响）

你的 SQL 客户端有三个行为，导致前几版脚本失败：

| # | 行为 | 症状 | 规避方式 |
|---|---|---|---|
| 1 | **只执行光标所在语句** | 多语句文件只跑一条，`0.001s` | 每条语句独立执行 |
| 2 | **吞掉 \`库\`.\`表\` 的点号** | 变成 `worlditem_template`，报表不存在 | **不用反引号** |
| 3 | **不执行 USE 语句** | `ERROR 1046: No database selected` | 每条自带 `world.` 前缀 |

**最终可靠写法**：不带反引号的 `库.表`，例如
```sql
ALTER TABLE world.item_template MODIFY ...
```
这种写法同时规避了 2 和 3，已实测在「不选库 + 单条执行」下成功。

---

## 3-4. 生成测试物品

执行 **`sql/03_test_item.sql`**（已改为逐条执行版）

> 该文件有 **STEP1 ~ STEP6** 六条语句。因为你的客户端只跑光标所在语句，
> 请**把光标依次放在每条语句上，逐条执行**。
> 全部执行完后，STEP6 会显示验证结果。

### 六步做什么

| 步骤 | 语句 | 作用 |
|---|---|---|
| STEP1 | `DELETE ... WHERE entry=900001` | 清理旧数据（可重复执行） |
| STEP2 | `DROP TABLE IF EXISTS ... tmp_bignum_copy` | 清理残留临时表 |
| STEP3 | `CREATE TABLE ... AS SELECT * ... WHERE entry=46017` | 复制模板（自动匹配全部 130+ 列） |
| STEP4 | `UPDATE tmp_bignum_copy SET ...` | 改主键为 900001 + 写入大数值 |
| STEP5 | `INSERT INTO ... SELECT * FROM tmp...` + `DROP TABLE` | 写回正式表并清理 |
| STEP6 | `SELECT ...` | 验证 |

> 为什么用临时表：直接 `INSERT INTO ... SELECT *` 会主键冲突报
> `ERROR 1062 Duplicate entry '46017'`。用 `CREATE TABLE AS SELECT`
> 能自动匹配全部列，不必手写 130 多个字段名。

### STEP6 期望输出

我在模拟你环境（不选库、逐条执行）的沙箱里实测：

```
+--------+---------------------+-----------+-----------+------------+------------+--------------+---------+
| entry  | name                | 耐力      | 力量      | 攻强       | 护甲       | 神圣抗性     | 耐久    |
+--------+---------------------+-----------+-----------+------------+------------+--------------+---------+
| 900001 | 测试-十亿之刃       | 100000000 | 500000000 | 1000000000 | 1000000000 |    500000000 | 1000000 |
+--------+---------------------+-----------+-----------+------------+------------+--------------+---------+
```

判读：
- 耐力显示 **32767** -> stat_value 列没扩展（不该发生，你已确认是 int）
- 神圣抗性显示 **255** -> 抗性列没扩展（同上）
- 中文名乱码 -> 客户端连接字符集不是 utf8mb4

> **耐力为什么只给 1 亿**：`Player::UpdateMaxHealth()` 会把耐力 x10 算成血量。
> 实测 21亿耐力 -> 血量214亿 -> 塞进 uint32 回绕成 0 -> 角色一进游戏暴毙。
> 1 亿耐力约等于 10 亿血，安全。

---

## 3-5. 🔴 用新 exe 启动（关键）

**这一步最容易出错。** 你的新 exe 在：

```
D:\TC-Build\bin\RelWithDebInfo\
```

而你原来的服务端在另一个目录。**两种做法二选一：**

### 做法 A：先在原目录测试（推荐，能快速回滚）

备份旧 exe，再复制新的过去：

```bash
# 假设你的服务端在 D:\WOWServer，按实际改
cd /d/WOWServer
mv worldserver.exe worldserver.exe.old
mv authserver.exe authserver.exe.old
cp /d/TC-Build/bin/RelWithDebInfo/worldserver.exe .
cp /d/TC-Build/bin/RelWithDebInfo/authserver.exe .
```

**注意**：`.conf` 配置文件**不要覆盖**，用你原来的。

出问题就把 `.old` 改回来即可。

### 做法 B：直接在 build 目录跑

需要把 `worldserver.conf`、`authserver.conf`、`dbc/maps/vmaps/mmaps`
都放到 `D:\TC-Build\bin\RelWithDebInfo\`，比较麻烦。

**建议用做法 A。**

---

## 3-6. 启动并观察日志

先启动 `authserver.exe`，再启动 `worldserver.exe`。

### ✅ 正常：应该看到

```
Loading Item templates...
>> Loaded XXXXX item templates in XXX ms
```

### 🔴 异常：如果看到这个，说明 exe 是旧的

```
Field::GetInt16 on LONG field item_template.stat_value1 ...
caused value to be truncated. Use Field::GetInt32 instead.
```

**处理**：说明你启动的还是旧 exe。回到 3-5 确认复制对了。

---

## 3-7. 游戏内验证

用 GM 账号进游戏：

```
.additem 900001
```

### 检查清单

| 检查项 | 期望 | 不通过说明 |
|---|---|---|
| 物品能正常获得 | 背包里出现橙色武器 | — |
| 鼠标悬停看属性 | 显示 +1000000000 攻击强度 | 封包/客户端问题 |
| 显示 +100000000 耐力 | | |
| 显示 +1000000000 护甲 | | 抗性列没扩展 |
| **装备后血量正常** | 约 10 亿血，**不是 0 或负数** | 溢出了 |
| 角色面板属性正确 | 力量 5 亿 | |

### 🔧 如果显示的还是旧数值

**删客户端缓存**：关掉游戏，删除客户端目录下的 `WDB` 文件夹，重新登录。

3.3.5 客户端会把物品信息缓存在 `WDB\zhCN\ItemCache.wdb`，
不删的话看到的还是改造前的数据。

---

## ✅ 完成清单

- [ ] 停服 + 再次备份
- [ ] `00_preflight_check.sql` 确认改造前是 smallint/tinyint
- [ ] `01_world_item_template_bignum.sql` 自检全部变 int
- [ ] `02_characters_durability.sql` 自检 durability = int unsigned
- [ ] `03_test_item.sql` 读回数值正确（1亿/10亿/5亿）
- [ ] 新 exe 已复制到服务端目录（旧的改名备份）
- [ ] worldserver 启动无 truncated 报错
- [ ] `.additem 900001` 属性显示正确
- [ ] **装备后血量正常，不是 0**

---

## 🔙 回滚方法

**只回滚数据库**：

```bash
mysql -u root -p world < /c/wow_backup_20260726/item_template.before_alter.sql
mysql -u root -p characters < /c/wow_backup_20260726/item_instance.before_alter.sql
```

**回滚 exe**：

```bash
cd /d/WOWServer
mv worldserver.exe.old worldserver.exe
```

> ⚠️ 注意：如果回滚了数据库但保留新 exe，是**安全**的（新 exe 读小字段没问题）。
> 反过来（保留大数值数据库 + 旧 exe）**会崩服**。

---

## 完成后告诉我

回复执行结果，特别是：
1. `03_test_item.sql` 最后那个查询的输出
2. 游戏内 `.additem 900001` 后**装备上去的血量**是多少

下一步（第 4 步）会做更完整的边界验证，然后第 5 步 Eluna，之后就进入指令开发了。
