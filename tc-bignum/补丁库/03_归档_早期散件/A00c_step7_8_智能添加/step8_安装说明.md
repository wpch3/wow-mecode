# 第 8 步：Gossip 可点击菜单 + 无限分页 + 批量地基

## 一、先回答你的分页担心

我查到 Gossip 有个**会崩服的硬限制**：

```cpp
// GossipDef.cpp:42
ASSERT(_menuItems.size() <= GOSSIP_MAX_MENU_ITEMS);   // 32
```

**超过 32 项不是截断，是直接 ASSERT 崩溃。** 所以设计上必须严格卡线。

### 实测结果（137 条结果的分页测试）

```
结果总数: 137
总页数: 5   每页: 29 条结果 + 导航

所有 5 页中，单页最大项数 = 32  (硬上限 32)
结果: [通过] 永不超限
```

**首页**：29 条 + 「下一页」+「取消」= 31
**中间页**：29 条 + 「上一页」+「下一页」+「取消」= 32 ← 正好卡满
**末页**：21 条 + 「上一页」+「取消」= 23

### 分页容量

| 页数 | 可容纳结果 |
|---|---|
| 10 页 | 290 条 |
| **50 页** | **1450 条** |
| 103 页 | 2987 条 |

**代码本身支持无限页**（页数 = 总数 ÷ 29 的除法，没有上限）。
唯一的约束是搜索上限，我设为 **3000 条 ≈ 103 页**，
远超你要的 50 页。

> 为什么不设无限：遍历 38000 个物品模板本身有开销，
> 3000 条已覆盖任何合理搜索。要改的话就是一个常量：
> `MAX_SEARCH_RESULTS = 3000`

---

## 二、为套装系统预留的批量地基

这是你特别要求的部分。核心是 **`BatchSession` 通用会话结构**：

```cpp
struct BatchSession
{
    PickerType          type;      // 业务类型
    std::vector<uint32> results;   // 候选列表（已排序）
    uint32              page;      // 当前页
    uint32              amount;    // 数量
    std::string         keyword;   // 搜索词

    uint32 TotalPages() const;     // 自动算页数
    bool HasPrev() / HasNext();
};
```

**新增功能只需三步**：

```cpp
// 1. 加一个枚举
enum PickerType {
    PICKER_ITEM     = 1,
    PICKER_CREATURE = 2,
    PICKER_GEARSET  = 3,   // <- 套装系统只加这一行
};

// 2. 在 SendPickerMenu 里加一个渲染分支
else if (ss.type == PICKER_GEARSET) { /* 生成套装标签 */ }

// 3. 在 OnGossipSelect 里加一个执行分支
else if (ss.type == PICKER_GEARSET) { /* 发放整套装备 */ }
```

**分页、导航、菜单渲染、点击路由全部复用**，不用重写。

---

## 三、技术要点（我核实过的）

### 1. 无需 NPC 就能弹菜单

`MiscHandler.cpp:150` 有官方分支：

```cpp
else if (guid.IsPlayer())
{
    if (_player->GetGUID() != guid || menuId != ...GetMenuId())
        return;
}
```

所以 `SendGossipMenu(text, player->GetGUID())` 用**自己的 GUID** 是合法的。

### 2. 点击回调路径

```cpp
// MiscHandler.cpp:227
sScriptMgr->OnGossipSelect(_player, menuId, sender, action);
     ↓
// ScriptMgr.h:719
virtual void OnGossipSelect(Player*, uint32 menu_id, uint32 sender, uint32 action)
```

我用 `PlayerScript` 挂这个钩子，`action` 里存物品/生物 ID，
`sender` 区分「选中条目」还是「导航按钮」。

### 3. 用对了 AddMenuItem 重载

有两个重载，我一开始用错了：

```cpp
// ❌ 这个从 DB 读文本，自定义菜单用不了
void AddMenuItem(uint32 menuId, uint32 menuItemId, uint32 sender, uint32 action);

// ✅ 这个能直接传文字
uint32 AddMenuItem(int32 menuItemId, GossipOptionIcon icon, std::string const& message,
                   uint32 sender, uint32 action, std::string const& boxMessage,
                   uint32 boxMoney, bool coded);
```

写的时候发现并修正了。

---

## 四、菜单长这样

```
.add 剑

┌────────────────────────────────────┐
│ [荐] 埃辛诺斯战刃  [装等156 品质5]  │ ← 点击直接获得
│      霜之哀伤      [装等284 品质6]  │
│      奥金斧        [装等80  品质5]  │
│      ...（本页共29条）              │
│ >> 下一页  (第 2/5 页)              │
│ [ 取消 ]                            │
└────────────────────────────────────┘
聊天框：「剑」共 137 项 — 第 1/5 页
```

**点击条目 = 直接获得**，这就是你要的效果。

---

## 五、安装

### 1. 替换文件

`step8_cs_smartadd.cpp` → `D:\TrinityCore\src\server\scripts\Commands\cs_smartadd.cpp`

（覆盖 v2，记得改名去掉 `step8_` 前缀）

### 2. RBAC / 数据库权限

**不用动**，权限号还是 71003/71004，你已经注册过。

### 3. 编译

**不用重跑 CMake**（文件名没变）。

```
VS 2022 → RelWithDebInfo + x64 → 生成解决方案
```

---

## 六、测试清单

### 基础（应该和之前一样）
```
.add 32837                按ID直接给
.add 900001               你的十亿测试物品（名字唯一）
.add 奥金斧 3             带数量
.add 900001, 32837        批量按ID
.add last                 重复上次
```

### 新功能：可点击菜单
```
.add 剑                   ★ 应弹出菜单，点击直接获得
.add 之刃 5               ★ 菜单里点击 = 获得5个
.spawn 豺狼人             ★ 生物菜单
.spawn 豺狼人 x3          ★ 点击召唤3只
```

### 重点验证分页
```
.add 之                   搜"之"字，结果应该很多
                          翻到最后一页，确认：
                          - 每页都能正常显示
                          - 上一页/下一页正常
                          - 末页没有"下一页"按钮
                          - 首页没有"上一页"按钮
                          - 服务端不崩
```

### 强制模式仍走聊天框
```
.add! 剑                  不弹菜单，直接全给
```

---

## 七、已知设计取舍

| 项 | 说明 |
|---|---|
| Gossip 标题 | 用聊天框输出（Gossip 原生标题要 DB 文本，聊天框更灵活） |
| 会话存内存 | 重启服务端后清空，不影响使用 |
| 单页 29 条 | 硬上限 32 减 3 个导航位，不能再多 |
| 控制台不支持菜单 | 会提示用 `.add <ID>` |

---

## 八、下一步

这个批量地基打好后，套装系统就能直接复用：

```
.gearset 战士 264         → 弹出菜单选择套装方案
.gearset bot 圣骑士 264   → 给选中的bot整套
.gearset save 我的配装     → 保存当前全身装备
.gearset load             → 弹菜单选择已保存的方案
```

有编译错误发我**第一条 error**。
菜单行为不对，把操作步骤和现象一起发我。
