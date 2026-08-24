# G16：每 entry 十条与传说过滤——源码探针和回传清单

> 日期：2026-08-19
> 当前确定口径：每个 eligible `itemEntry` 至少 **10 条有效独立 AHBot 挂单**。
> `SUM(item_instance.count)` 只作堆叠件数观测，不能用一条大堆叠挂单代替10条独立挂单。
> 传说品质只允许 Misc/Pet 与 Misc/Mount，其他传说物品全部禁止。
> **日常执行请直接使用**：`tc-bignum/00-当前整体安装步骤_单文件入口.md`。本文件保留 G16 专项取证细节。

---

## 一、为什么还需要真实源码探针

上游 `NPCBOT-Eluna-zhCN-2026` 已证明当前 seller 是按品质/类别池随机抽 entry，不能保证每 entry 最低10条；但用户的 `D:\TrinityCore` 已有大量自定义修改，不能直接用上游行号安装。

本探针一次收集六个真实文件：

```text
src/server/game/AuctionHouseBot/AuctionHouseBotSeller.h
src/server/game/AuctionHouseBot/AuctionHouseBotSeller.cpp
src/server/game/AuctionHouseBot/AuctionHouseBot.h
src/server/game/AuctionHouseBot/AuctionHouseBot.cpp
src/server/game/AuctionHouse/AuctionHouseMgr.h
src/server/game/AuctionHouse/AuctionHouseMgr.cpp
```

报告包含 Git 分支/HEAD/状态、编码、换行、SHA256、锚点计数和完整带行号源码。脚本不会修改源文件，并会比较运行前后的 Git 状态。

---

## 二、运行只读源码探针

把 `probe_g16_min_stock.py` 放到任意非源码目录，在 PowerShell 执行：

```powershell
py -3 "脚本完整路径\probe_g16_min_stock.py" "D:\TrinityCore" "D:\probe_g16_min_stock.txt"
```

必须看到：

```text
[OK] Wrote complete UTF-8-BOM report: D:\probe_g16_min_stock.txt
[OK] Source files read: 6; source edits: 0
```

请完整回传 `D:\probe_g16_min_stock.txt`，不要只截最后几十行，也不要手工删减源码段。

如果提示 required file missing，完整回传报错；不要自行改脚本中的路径去猜另一套架构。

---

## 三、同时回传补货后的两份数据库结果

依次完整执行：

```text
12-共享拍卖行_只读诊断.sql
14-拍卖行每物品库存与传说过滤_只读诊断.sql
```

两份脚本都只有 `SELECT`。需要所有结果集，不要只回传总行数。

其中 `14` 号结果的当前验收重点：

1. `pct_meeting_min_10`：现状达标率；
2. 最低500种 entry：确认哪些只有1–2条；
3. 传说清单：把当前468行/3 entries 分类为允许或禁止；
4. `forbidden_entries` / `forbidden_rows`：最终都必须为0；
5. `total_stack_units`：只观察堆叠，不代替独立挂单数。

注意：SQL 只能统计已经出现的 entry。完全为0的 eligible entry 必须在 C++ seller 物品池构建完成后，通过源码指标证明100%覆盖。

---

## 四、收到完整回传后要生成什么

下一批不是继续改总量 conf，而是生成与 G11 同等级的精确 C++ 安装包：

1. 在 AHBot 配置枚举/加载链加入：

   ```ini
   AuctionHouseBot.Items.MinCopiesPerEntry = 10
   ```

2. seller 建池时先过滤所有禁止的传说物品，即使在 `forceIncludeItems` 也不得绕过；
3. 每轮读取当前共享 AuctionHouseObject 中属于 AHBot 的有效独立挂单，按 `itemEntry` 计数；
4. 先补所有 `<10` 的 eligible entries，再进入原品质/类别加权随机填充；
5. 若 `eligible_entry_count * 10` 超过配置总容量，启动日志明确报容量不足，禁止悄悄牺牲覆盖率；
6. 提供哈希校验、唯一锚点、自动备份、重复安装检测、一键回滚、编译和 SQL 验收。

在真实报告回来前，不直接给用户源码套上游行号补丁。
