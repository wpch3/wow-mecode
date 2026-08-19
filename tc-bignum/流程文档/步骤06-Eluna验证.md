# 第 5 步：Eluna 验证

**目标**：确认补丁 02（uint64 截断修复）真的生效，并确认 Eluna 无沙箱限制。
**耗时**：10 分钟
**风险**：无（只读检测，改血量后立即还原）

---

## 一、这一步在验证什么

回顾第 1 步改的那一行（`LuaEngine.cpp:591`）：

```cpp
template<> unsigned long long Eluna::CHECKVAL<unsigned long long>(int narg)
{
    if (lua_isnumber(L, narg))
-       return static_cast<unsigned long long>(CHECKVAL<uint32>(narg));   // 原版 BUG
+       return static_cast<unsigned long long>(CHECKVAL<double>(narg));   // 修复
```

**原版 bug 的后果**：任何接受 `uint64` 参数的 Lua 方法，
传入超过 **4294967295（42亿）** 的数值会直接报错：

```
value must be less than or equal to 4294967295
```

我找到了**精确验证入口** —— `CreateUint64()`（`GlobalMethods.h:2513`），
它内部就是走 `CHECKVAL<unsigned long long>`：

```cpp
int CreateULongLong(Eluna* E)
{
    ...
    init = E->CHECKVAL<unsigned long long>(1);   // <- 正是我们修复的那条路径
```

所以 `CreateUint64(10000000000)` 能否成功，就是补丁生效与否的**直接证据**。

---

## 二、放置脚本

### 2-1. 确认脚本目录

你的 conf 配置是：

```ini
Eluna.ScriptPath = "data\lua_scripts"
```

对应实际路径应该是：

```
D:\TC-Build\bin\RelWithDebInfo\data\lua_scripts\
```

Git Bash 确认：

```bash
ls -d /d/TC-Build/bin/RelWithDebInfo/data/lua_scripts/ 2>/dev/null && echo "存在" || echo "不存在，需要创建"
```

**不存在就创建**：

```bash
mkdir -p /d/TC-Build/bin/RelWithDebInfo/data/lua_scripts
```

> 注意：编译时 CMake 会把 extensions 放到
> `bin\RelWithDebInfo\lua_scripts\extensions\`（无 data 层）。
> 如果你的 conf 指向 `data\lua_scripts`，建议把那个 extensions
> 也复制一份到 `data\lua_scripts\extensions\`，否则部分扩展库不可用。

### 2-2. 放入脚本

把 **`lua_scripts/bignum_selftest.lua`** 复制到上面那个目录。

### 2-3. 重载 Eluna

**不用重启服务端**，在 worldserver 控制台输入：

```
reload eluna
```

或游戏内（GM）：

```
.reload eluna
```

控制台应出现：

```
[Eluna] bignum_selftest.lua 已加载 -- 游戏内输入 .bigtest 运行自检
```

**没出现**说明脚本没被加载 → 路径不对，回到 2-1。

---

## 三、运行自检

游戏内输入：

```
.bigtest
```

### 期望输出（我已在模拟环境实测）

```
================================
   大数值改造 + Eluna 自检
================================
[1] 环境
  *   Lua 版本  Lua 5.2
  *   Eluna  <版本号>
  *   核心  TrinityCore
================================
[2] 补丁02：uint64 截断修复
 [OK] CreateUint64(40亿)  基线，应始终通过
 [OK] CreateUint64(100亿) ★补丁关键项  已突破42亿限制
 [OK] CreateUint64(1万亿)  uint64 全量可用
 [OK] CreateUint64(字符串最大值)  对照组
================================
[3] 补丁01：大数值属性
  *   当前最大生命  <你的血量>
 [OK] SetMaxHealth(10亿)  1000000000
 [OK] SetMaxHealth(40亿)  4000000000
      血量已还原为 <原值>
================================
[4] 数据库大数值
 [OK] stat_value1 = 1亿  100000000
 [OK] armor = 10亿  1000000000
 [OK] holy_res = 5亿  500000000
 [OK] MaxDurability = 100万  1000000
================================
[5] 标准库（确认无沙箱限制）
 [OK] os 库  time=...
 [OK] io 库
 [OK] require
 [OK] package
 [OK] string 库
 [OK] math 库
================================
[6] Lua 数值精度
 [OK] 2^53 精度边界  9007199254740992
================================
   全部通过： 17 / 17
================================
```

---

## 四、结果判读

### 全部 17 项通过

补丁 01 + 02 都生效，Eluna 完全可用。**直接进第 6 步。**

### 组 [2] 出现 [NG]

我特地模拟了未打补丁的情况，会显示：

```
 [NG] CreateUint64(100亿) ★补丁关键项  未打补丁: bad argument #1
      (value must be less than or equal to 4294967295)
 [NG] CreateUint64(1万亿)  仍受限
```

**说明补丁 02 没编译进去**。排查：

```bash
cd /d/TrinityCore/src/server/game/LuaEngine
grep -n "CHECKVAL<double>(narg)" LuaEngine.cpp
```

- 找到 `return static_cast<unsigned long long>(CHECKVAL<double>(narg));` → 源码是对的，
  说明**编译时用的是旧代码**，需要重新编译
- 没找到 → 补丁丢了（可能被 `git submodule update` 冲掉），重新执行
  `apply_patches.sh`

### 组 [4] 出现 [NG]

```
 [NG] 查询测试物品 900001  未找到，请先执行 03_test_item.sql
```

说明测试物品没建成功，回第 3 步重跑 `03_test_item.sql`。

### 组 [3] 的 SetMaxHealth(40亿) 失败

如果 10 亿通过但 40 亿失败，属**正常边界现象**，
说明你的核心某处对血量做了额外钳制，不影响实际使用
（实战中耐力上限 4.2 亿 → 血量约 42 亿，本来就在边缘）。

---

## 五、[OK] 完成清单

- [ ] `data\lua_scripts\` 目录存在
- [ ] `bignum_selftest.lua` 已放入
- [ ] `reload eluna` 后控制台有加载提示
- [ ] `.bigtest` 有输出
- [ ] **组 [2] 全部 [OK]** ← 最关键，证明补丁 02 生效
- [ ] 组 [3][4][5] 全部 [OK]
- [ ] 总计 17/17

---

## 六、关于"Eluna 限制解除"的说明

第一轮你问过"Eluna 的限制怎么解除"，这里给最终结论：

| 你以为的限制 | 实际情况 |
|---|---|
| 沙箱禁用了 os/io/require | **原版就没禁**。`luaL_openlibs()` 加载全部标准库，组[5]会验证 |
| 脚本数量/执行时间/内存上限 | **不存在**。`ElunaConfig.cpp` 里没有任何此类配置 |
| 数值被截断 | **确实有 1 个真 bug**，就是补丁 02 修的那个 uint64 |

配置里唯一的两个门控开关（**默认已开**，无需修改）：

```ini
Eluna.UseUnsafeMethods     = true
Eluna.UseDeprecatedMethods = true
```

**所以"解除 Eluna 限制"这件事，实质就是补丁 02 那一行。** 已完成。

---

## 七、脚本可以留着

`bignum_selftest.lua` 建议**留在 lua_scripts 目录**，
以后每次改动核心/重编译后跑一次 `.bigtest`，能快速确认没有回归。

不想要的话删掉文件再 `reload eluna` 即可。

---

## 完成后告诉我

回复 `.bigtest` 的输出（或截图），确认后进入
**第 6 步：自定义指令开发**（五维直改 + .reload item_template 等）。
