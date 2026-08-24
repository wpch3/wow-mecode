# 修复：`.pbot spawn` 上线闪退（已用崩溃日志精确定位）

## 怎么从 30 多个 Call stack 里找到凶手

**只看第一个。**

崩溃日志会把**所有线程**的调用栈都 dump 出来（30 多个是正常的，
大部分是 boost::asio 的网络线程，长这样：

```text
NtRemoveIoCompletion+14
GetQueuedCompletionStatus+6C
boost::asio::detail::win_iocp_io_context::do_one+274
```

**这些全是无辜的**，它们只是在等网络包）。

### 判断方法

```text
1. 第一个 Call stack 通常就是崩溃线程
2. 找栈里【出现我们自己文件名】的那一行 —— 就是它
3. 忽略所有 boost / Nt / Rtl / BaseThread 开头的
```

你这次的第一个栈：

```text
TryAcceptTrade+40                        pbot_autoaccept.cpp line 389   <- 凶手
pbot_autoaccept_worldscript::OnUpdate+120  pbot_autoaccept.cpp line 689
ScriptMgr::OnWorldUpdate+BB              ScriptMgr.cpp line 1377
World::Update+BE0                        World.cpp line 2674
```

**一眼就能看出是 `pbot_autoaccept.cpp:389` 的 `TryAcceptTrade`。**

---

## 根因

```text
Exception code: C0000005 ACCESS_VIOLATION
TryAcceptTrade+40   <- 偏移量很小，说明崩在函数开头
```

`ACCESS_VIOLATION` = 访问了无效指针。
`+40` 是函数入口后 64 字节，对应的就是**第一行代码**：

```cpp
static void TryAcceptTrade(Player* bot)
{
    TradeData* trade = bot->GetTradeData();   // <- 崩在这，bot 是野指针
```

### 为什么 bot 会是野指针

`OnUpdate` 里这样拿 bot：

```cpp
Player* bot = ObjectAccessor::FindPlayer(e.CharGuid);
if (!bot) continue;
if (!bot->IsInWorld()) continue;
if (!bot->GetSession()) continue;
```

看起来有判空，但 **`.pbot spawn` 是异步登录**：

```text
1. .pbot spawn 把登录包塞进队列，g_pbots 立刻加了记录
2. 1-3 秒后 bot 才真正进世界
3. 这期间 FindPlayer 可能返回一个【正在构造中】的 Player 对象
   -> IsInWorld() 返回 true，但内部数据还没填完
   -> GetTradeData() 访问未初始化成员 -> ACCESS_VIOLATION
```

**我第6步改的 `TryAcceptTrade` 没加防护**，
而原来的版本第一句就是 `bot->GetTradeData()`，同样脆弱 ——
只是原来函数体短、没触发到。

---

## 修法：函数开头加完整防护

文件：`D:\TrinityCore\src\server\scripts\Commands\pbot_autoaccept.cpp`

### 搜索原文

```cpp
static void TryAcceptTrade(Player* bot)
{
    TradeData* trade = bot->GetTradeData();
    if (!trade)
        return;

    Player* trader = trade->GetTrader();
    if (!trader)
        return;
```

### 替换为

```cpp
static void TryAcceptTrade(Player* bot)
{
    // A37第6步崩溃修复：.pbot spawn 是异步登录，bot 可能处于半初始化状态。
    // FindPlayer 能返回对象、IsInWorld 也返回 true，但内部数据未填完，
    // 直接 GetTradeData() 会 ACCESS_VIOLATION。
    if (!bot || !bot->IsInWorld() || !bot->GetSession())
        return;
    if (bot->GetSession()->PlayerLogout() || bot->GetSession()->isLogingOut())
        return;
    if (!bot->FindMap())
        return;

    TradeData* trade = bot->GetTradeData();
    if (!trade)
        return;

    Player* trader = trade->GetTrader();
    if (!trader || !trader->IsInWorld() || !trader->GetSession())
        return;
    if (!trader->FindMap())
        return;
```

### 关键点

```text
!bot->GetSession()          会话没了 = 正在销毁
PlayerLogout()/isLogingOut() 正在登出，数据随时失效
!bot->FindMap()             还没进地图 = 半初始化状态
                            【必须用 FindMap 不能用 GetMap】
trader 侧同样要判           对方也可能正在登出
```

---

## 同样给 TryBotRequest 加防护

**你已经装过修复4，所以这个函数现在长这样**（不再有 `_pbotRequestTimer[bguid]`）：

```cpp
static void TryBotRequest(Player* bot, uint32 diff)
{
    if (!bot || !bot->IsInWorld() || !bot->GetSession())
        return;

    ObjectGuid bguid = bot->GetGUID();

    auto titr = _pbotRequestTimer.find(bguid);
```

### 只改第一行判断

搜索原文：

```cpp
    if (!bot || !bot->IsInWorld() || !bot->GetSession())
        return;

    ObjectGuid bguid = bot->GetGUID();
```

替换为：

```cpp
    if (!bot || !bot->IsInWorld() || !bot->GetSession() || !bot->FindMap())
        return;
    if (bot->GetSession()->PlayerLogout() || bot->GetSession()->isLogingOut())
        return;

    ObjectGuid bguid = bot->GetGUID();
```

**只加了 `|| !bot->FindMap()` 和下面那个 if，其余不动。**

> 这个函数其实不是本次崩溃的原因（崩溃日志明确指向 `TryAcceptTrade`），
> 加防护是预防性的 —— 它和 `TryAcceptTrade` 在同一个 OnUpdate 循环里，
> 面对的是同样的半初始化 bot。

---

## 需要确认的 API

```text
WorldSession.h:500  bool PlayerLogout() const { return m_playerLogout; }          public
WorldSession.h:503  bool PlayerDisconnected() const { return !m_Socket; }         public
WorldSession.h:554  bool isLogingOut() const { return _logoutTime || m_playerLogout; }  public

Object.h:467  Map* GetMap() const { ASSERT(m_currMap); return m_currMap; }   <- 【有断言，别用】
Object.h:468  Map* FindMap() const { return m_currMap; }                     <- 【用这个】
```

### 为什么必须用 `FindMap()` 不能用 `GetMap()`

```cpp
Map* GetMap() const { ASSERT(m_currMap); return m_currMap; }   // Object.h:467
```

**`GetMap()` 内部有 `ASSERT`** —— map 为空时它自己就 ABORT 了，
根本走不到我们的 `if (!...)` 判断，反而制造新的崩溃。

`FindMap()`（`Object.h:468`）直接返回指针，为空就是 nullptr，**这才是安全的判空方式**。

如果编译报 `PlayerLogout` 或 `isLogingOut` 找不到，
**去掉那一整个 if 判断**，只保留：

```cpp
    if (!bot || !bot->IsInWorld() || !bot->GetSession())
        return;
    if (!bot->FindMap())
        return;
```

这样也足够挡住绝大部分情况。

---

## 验证

```text
1. 重编译
2. .pbot spawn <账号> <角色>
3. 等它完全进世界（.pbot list 能看到）
4. 观察 1-2 分钟不崩 = 修好了
```

如果还崩，**把新的崩溃日志第一个 Call stack 贴给我**
（就是最上面那 10 行），能立刻定位新位置。

---

## 以后怎么自己看崩溃日志

```text
文件位置：D:\TC-Build\bin\RelWithDebInfo\Crashes\ 里最新的 .txt

看三样：
1. 顶部的 Exception code
   C0000005 ACCESS_VIOLATION = 空指针/野指针
   C00000FD STACK_OVERFLOW   = 无限递归
   
2. 第一个 Call stack 里【带我们文件名】的那行
   -> 文件名 + 行号 = 崩溃点
   
3. 函数名后面的 +NN 偏移
   +40 这种小数字 = 崩在函数开头（多半是参数无效）
   +2543 这种大数字 = 崩在函数中后段
```

**boost/Nt/Rtl/BaseThread 开头的全部忽略**，那些是系统线程。
