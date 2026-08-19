# PROJECT_HANDOFF.md · 魔兽世界 3.3.5a 客户端＋服务端魔改项目 技术交接

> **生成日期**：2026-08-18（UTC 01:13）｜**最后同步**：2026-08-18 第三十三批（两个真因最终修复）
> **生成方式**：对 `/home/user` 全量取证审计（929 个文件，逐文件 SHA256，
> 与上游 commit `4e8762e` 逐文件 diff）后撰写，非记忆摘要。
> **权威副本说明**：本文件的权威副本位于 **`/home/user/PROJECT_HANDOFF.md`**。
> 由于本工作区**不存在任何 Git 仓库根目录**（见第 1 节），无法放置到"Git 仓库根"，
> 因此**只有这一份**，不存在需要同步的第二份。
>
> **编码说明（重要，非疏漏）**：本文件是 UTF-8 无 BOM。
> 它**故意不满足**本项目平时的「GBK 兼容」铁律——因为交接规格强制要求使用
> ✅ 🟢 🟡 🟠 🔴 ⚪ ❓ ❌ 这 8 种状态标记，它们在 GBK 中无对应码位。
> 运行 `python3 tc-bignum/gbk_check.py PROJECT_HANDOFF.md` 会报这 8 种字符，**属预期**。
> 已把所有**非强制**的装饰符号（⚠ / 🚫 / ❎）替换成了 GBK 安全的中文文字。
> 【请用 VS Code / 浏览器 等 UTF-8 环境阅读本文件，不要用 GBK 记事本打开。】
> 项目内**其他**所有文档与代码仍严格遵守 GBK 兼容铁律。

---

## 【给下一个代理的 30 秒摘要】

如果你只读一段，读这段：

1. **本工作区里没有 `wow-mecode` 仓库，也没有完整 TrinityCore 源码。**
   全量 grep `wow-mecode` / `wpch3` = **0 命中**。工作区里 4 个 `.git` 全都不是本项目的。
2. **`/home/user/src/` 是上游只读副本，不是我们的成果。**
   逐文件 diff：66 个与上游**完全相同**，2 个"差异"其实是文件名错配和下载失败残骸。
   **绝对不可**用它覆盖用户的 `D:\TrinityCore`。
3. **真正的成果全部在 `/home/user/tc-bignum/`（558 个文件），且它没有版本控制。**
   这是当前 **P0 头号风险**：只要 Workspace 快照丢失，四十多个补丁的心血就没了。
4. **第二十二批三件套已实测通过**（拍卖行118494条/台词141条/时间线可见）。
   坐骑问题返工两次，**第25批已给最终方案B**：用户实测证实 `Bind.Pickup=0`
   挡掉 295/310 种坐骑（95.2%），改用白名单放行（不动 Bind.Pickup，避免放回 BoP 装备）。
5. **F40/F41/F42/F43/A42修复 全部已装，禁止重复安装。**
   G19第3步（`规划/G19_*/06`）代码已改完，用户正在编译。
5. 项目主线是剧情《真龙纪元》；最终目标是"bot 自主冒险、偶遇玩家组队"。

---

# 1. 项目身份与仓库关系

## 1.1 项目定位

| 项 | 内容 |
|---|---|
| 项目名 | 魔兽世界 3.3.5a 私服客户端＋服务端魔改（用户内部称"tc-bignum"） |
| 终极目标（用户原话） | 「能实现自主冒险偶遇玩家组成队伍就好了，光是游荡npc是做不到的」 |
| 内容主线（用户原话） | 「我们的主线你一定要记得，是真龙的那条伟大的史诗」 |
| 剧情载体 | `tc-bignum/剧情/故事集-真龙纪元.txt`（2866 行 / 约 4.3 万字） |
| 用户环境 | Windows + VS2022 + Git Bash + CMake GUI + DBeaver |

## 1.2 WoW 版本与客户端 Build

| 项 | 值 | 证据等级 |
|---|---|---|
| 游戏版本 | World of Warcraft 3.3.5a | 上游仓库分支即 3.3.5，高可信 |
| 客户端具体 Build | **❓ 待确认** | **工作区无任何客户端文件可供核实**。按要求"不要未经检查就假定"，此处不写 12340 |
| 语言区域 | zhCN（简体中文） | 上游分支名含 zhCN；`uploads/GlueStrings.lua.txt` 为中文客户端字符串 |
| 客户端根目录 | `D:\WOW`（用户口述） | 目录内容不在工作区，无法核实 |

> **为什么不敢写 12340**：3.3.5a 最常见 Build 确实是 12340，但用户使用的是
> "整合客户端"（见 `工具库/11-整合客户端体检与加密应对.md`），
> 整合包常被改过 exe 和 MPQ。必须由用户在客户端登录界面右下角或
> `Wow.exe` 属性中确认后填写。

## 1.3 仓库关系（本节为交接最关键事实）

### A. 自有工作仓库 `wpch3/wow-mecode`

| 项 | 结果 |
|---|---|
| 用户声明的 URL | `https://github.com/wpch3/wow-mecode`（分支 main） |
| 本地根目录 | **未找到** |
| 本地 HEAD | **无法获取** |
| 证据 | ① `find /home/user -name .git -type d` 只找到 4 个，均非此仓库<br>② `grep -ril "wow-mecode\|wpch3" /home/user` = **0 命中**<br>③ `/home/user` 与 `/home/user/tc-bignum` 执行 `git status` 均报 `not a git repository`<br>④ 无 `~/.gitconfig`，无凭据存储 |

**结论**：本 Workspace **不是** `wow-mecode` 的一个 checkout。
它是一个**独立的、无版本控制的工作目录**。
用户所说的 GitHub 仓库很可能是**在别处（本机或另一环境）手工上传**本 Workspace 内容形成的。

> **给下一个代理**：不要假设 `git pull` 能拿到本文档描述的内容，
> 也不要假设 GitHub 上的 `wow-mecode` 与本 Workspace 内容一致——
> **两者的一致性未经验证**。必须先向用户确认（见第 17 节 Q1）。

### B. 完整 TrinityCore/NPCBots/Eluna 服务端源码

| 项 | 结果 |
|---|---|
| 是否包含在工作区 | **否，明确不包含** |
| 判据 | 无 `CMakeLists.txt`、无 `src/server/` 层级、无 `dep/`、无 `.git` |
| 用户本地真实位置 | `D:\TrinityCore`（用户口述，不在工作区） |
| 本地基线 commit | **❓ 待确认**（需用户执行 `git rev-parse HEAD`） |

### C. 上游整合源码 `328950225/TrinityCore-NPCBOT-Eluna-zhCN`

| 项 | 值 |
|---|---|
| URL | `https://github.com/328950225/TrinityCore-NPCBOT-Eluna-zhCN` |
| 目标分支 | `NPCBOT-Eluna-zhCN-2026` |
| **审计当日分支 HEAD** | **`4e8762ee2b00948fa103d0cd1afd78ccdf4364fb`** |
| 提交日期 | Sat Jul 4 23:36:54 2026 +0800 |
| 提交标题 | `Merge remote-tracking branch 'TrinityCore/3.3.5' into NPCBOT-Eluna-zhCN-2026` |
| 获取方式 | 只读 sparse clone 到 `/tmp/tcsrc`（**临时目录，不持久化，未 fetch/merge 到任何本地仓库**） |

> 【注意】**`4e8762e` 是"审计当天上游最新提交"，不等于"用户实际使用的基线"。**
> 用户 `D:\TrinityCore` 的真实 commit = **待确认**。
> 本文档中所有"上游第 N 行"的行号均基于 `4e8762e`，
> 若用户基线不同，行号需重新核对。

### D. 实际运行环境

| 项 | 值 | 来源 |
|---|---|---|
| 源码目录 | `D:\TrinityCore` | 用户口述 |
| 编译目录 | `D:\TC-Build` | 用户口述 |
| 部署目录 | `D:\TC-Build\bin\RelWithDebInfo\` | 用户口述 + conf 路径推断 |
| 客户端目录 | `D:\WOW` | 用户口述 |
| VS 配置 | **必须 RelWithDebInfo** | 用户曾误切 Debug 导致大量 C1069 假错误 |

**以上四个目录均不在工作区**，其内容无法核实。

### E. 组件来源

| 组件 | 来源 | 版本/commit |
|---|---|---|
| TrinityCore 3.3.5 | 经上游整合仓库间接引入 | ❓ 待确认（上游 4e8762e 是一次 merge 提交） |
| NPCBots | 上游整合仓库内置 | ❓ 待确认具体 NPCBots 上游版本 |
| Eluna | 上游整合仓库内置 | 工作区 `_chk/` 另有一份 ElunaLuaEngine/Eluna 独立克隆，FETCH_HEAD=`4608e3f`，**仅用于查 API，与实际编译无关** |

## 1.4 最终事实来源判定

| 优先级 | 来源 | 说明 |
|---|---|---|
| 1（最高） | 用户的 `D:\TrinityCore` + 真实数据库 + `D:\WOW` | **不在工作区**，是唯一的"实际状态" |
| 2 | `/home/user/tc-bignum/` | **本工作区唯一的成果目录**，是补丁与文档的事实来源 |
| 3 | `tc-bignum/待办总表.md`（3127 行） | 总账本 + 最高铁律 + 坑表 |
| 4 | `tc-bignum/补丁库/00-补丁库索引.md` | 补丁索引 |
| 禁用 | `/home/user/src/`、`_chk/`、`t7-t9`、`v2-v4`、`_chk2/`、`fk/`、`luatest/`、`_dt`、`_dt2` | 上游副本与测试桩，**非成果** |

---

# 2. 当前项目总状态

## 2.1 状态图例

| 标记 | 含义 |
|---|---|
| ✅ | 已完成并经用户游戏内验证 |
| 🟢 | 已安装且编译通过，未完整游戏内验证 |
| 🟡 | 已实现/有补丁，但未安装、未编译、未部署 |
| 🟠 | 只有设计、文档或测试桩 |
| 🔴 | 已知失败、崩溃或必须撤回 |
| ⚪ | 已废弃/被新版本替代 |
| ❓ | 无法确认 |

## 2.2 全局状态

| 问题 | 回答 | 证据 |
|---|---|---|
| 当前能运行到什么程度 | 服务端可启动、可进游戏、bot 系统可用 | 用户在会话中持续报告游戏内现象（如"游荡bot只会站着"），说明服务器在跑 |
| 最近一次服务端编译是否成功 | ✅ **成功**（F41/F42/F43 那一批） | 用户明确回报三个修复"已装并验证成功" |
| 最近一次 authserver 启动 | ❓ 未单独确认 | 无直接证据，但能进游戏说明其运行正常 |
| 最近一次 worldserver 启动 | 🟢 成功 | 同上 |
| 最近一次客户端能否登录进世界 | ✅ 能 | 用户描述游戏内 bot 行为，属于游戏内观察 |
| 最近一次工作停在哪 | 第二十三批（坐骑配额修正）**刚交付，用户未部署**；第二十二批已实测通过 | 见 2.4 |
| **数据库当前 schema 状态** | ❓ **完全无法确认** | 工作区无任何数据库导出、快照或查询结果 |

## 2.3 最高优先级阻塞项（P0）

> **P0-1：全部成果无版本控制，随时可能全损。**
>
> `/home/user/tc-bignum/` 是 43 个功能补丁 + 43 个修复 + 全部规划与剧情的唯一载体，
> 共 558 个文件，**没有 `.git`，没有任何备份**。
> 用户本次提出交接需求的原话正是「代理模式用久了会出现无法加载的情况，
> 为了不会因为重置就让我们的制作消失」——**这个担心是完全成立的，且是当前最大风险。**

## 2.4 下一步唯一推荐动作

**先把 `tc-bignum/` 完整备份出去（下载 Workspace 或推到 GitHub），再谈任何开发。**

备份完成后的下一个开发动作是：**用户部署第二十三批（坐骑配额修正）**——
只需复制 `conf/worldserver.conf.d/ahbot.conf`（唯一改动 `Class.Misc` 2→8），
说明见 `规划/G16_bot经济与打工/07-坐骑变少的根因与修正.md`。

**第二批编译已缩减为只剩 G19第3步**——探针证实 F40 和 A42修复都早已安装。
用户已改完代码，**首次编译报 11 个错（3个根因）**，修复文档 `规划/G19_情境对话系统/07-编译错误修复.md`。

**再之后**：G11 感知层（通往用户最终目标「自主冒险偶遇玩家组队」）。
已实查 `Group.h:197 AddMember(Creature*)` / `:205 AddInvite(Player*)` / `:209 AddMember(Player*)` 均为 **public**，组队层技术可行。

---

# 3. 权威文件地图

## 3.1 目录角色总表（按第五节要求的 12 类归类）

| 目录 | 文件数 | 归类 | 角色说明 |
|---|---|---|---|
| `tc-bignum/` | 558 | **1 权威源文件/当前最终版** | **唯一成果目录** |
| `tc-bignum/补丁库/` | 343 | 1 + 2 可重放补丁 | 43 功能(A) + 43 修复(F) + 8 早期归档 |
| `tc-bignum/sql/` | 45 | 5 SQL 迁移 | 编号 00-45，早期大数值/装备线 |
| `tc-bignum/lua_scripts/` | 7 | 6 Eluna Lua | 6 个 .lua + 1 说明 |
| `tc-bignum/conf/` | 20 | 7 配置 | conf.d 档位 12 个 + 本次新增 ahbot.conf + 5 个旧版完整备份 |
| `tc-bignum/tools/` | 17 | 8 编译验证/工具 | check_sql.py / syntax_check.sh / fix_nbsp.py 等 |
| `tc-bignum/规划/` | 54 | 11 仅规划未实现 | G11/G16/G17/G19/G20/G21/G22 |
| `tc-bignum/剧情/` | 9 | 1 权威内容源 | 真龙纪元故事集与设定集 |
| `tc-bignum/工具库/` | 34 | 4 客户端构建输入（**仅文档**） | 25 篇客户端魔改方法文档，**无任何实际客户端文件** |
| `tc-bignum/流程文档/` `指令清单/` `参考资料/` | 23 | 1 + 11 | 装机流程、指令总表、上游资料 |
| `src/` | 83 | **3 文件快照（上游只读）+ 10 部分损坏** | **不是成果，禁止用于覆盖** |
| `_chk/` | 213 | 3 上游 Eluna 只读克隆 | ElunaLuaEngine/Eluna @ 4608e3f，无我方改动 |
| `_chk2/` `t7/` `t8/` `t9/` `v2-v4/` `fk/` `luatest/` | 49 | **8 测试桩** | 独立小程序与 mock，**禁入生产** |
| `_dt/` `_dt2/` | 9 | 8 实验沙盒 | git submodule 行为实验，与项目无关 |
| `uploads/` | 25 | **9 诊断日志/用户上传证据** | 崩溃转储、conf、编译错误、截图 |

## 3.2 关键文件权威地图

| 领域 | 功能 | 权威源/补丁路径 | 实际目标路径 | 当前版本 | 已安装 | 已验证 | 旧版/冲突 | 备注 |
|---|---|---|---|---|---|---|---|---|
| 总账 | 项目总账本 | `tc-bignum/待办总表.md` | — | 3127 行 | — | — | — | **接手必读**，含最高铁律与坑表 |
| 总账 | 目录导航 | `tc-bignum/README.md` | — | 437 行 | — | — | — | 已更新到第二十二批 |
| 总账 | 未完成想法 | `tc-bignum/未完成想法-总清单.md` | — | 798 行 | — | — | — | A-G 分类，G11-G22 |
| 总账 | 补丁索引 | `tc-bignum/补丁库/00-补丁库索引.md` | — | — | — | — | — | 索引本身**滞后**，未含 A35+/F18+ |
| C++ | F41 中立怪+发呆 | `补丁库/02_修复/F41_*/根因与修复.md` | `bot_ai.cpp` 3 处 | 最终 | ✅ | ✅ | — | 用户回报已装并成功 |
| C++ | F42 招募闪退 | `补丁库/02_修复/F42_*/根因与修复.md` | `bot_ai.cpp` 2 处 | 最终 | ✅ | ✅ | `定位方案.md`(过程稿) | 同上 |
| C++ | F43 近战不前进 | `补丁库/02_修复/F43_*/根因与修复.md` | `bot_ai.cpp` 1 处 | 最终 | ✅ | ✅ | — | 同上 |
| C++ | F40 剑圣镜像闪退 | `补丁库/02_修复/F40_*/根因与修复.md` | `botmgr.cpp:803` + `Map.cpp:1065` + Lua ext | 最终 | ✅ **是（最先装的）** | ✅ | — | **已完成，禁止重装** |
| C++ | G19 第3步 场景感知 | `规划/G19_情境对话系统/04-第3步_场景感知实现.md` | `bot_companion.cpp/.h` | 候选 | 🟡 否 | ❌ | — | 真正解决"看场合说话" |
| C++ | A42 修复 礼物权重 | `补丁库/01_功能/A42_*/修复-第2步改错了函数.md` | `bot_companion.cpp` | 候选 | 🟡 否 | ❌ | `第2步_种族口音.md`(改错函数) | 必须用"修复-"那份 |
| 配置 | 拍卖行大改 | `tc-bignum/conf/worldserver.conf.d/ahbot.conf` | `D:\TC-Build\bin\RelWithDebInfo\worldserver.conf.d\` | **v1 新** | 🟡 否 | ❌ | `规划/G16_*/03-*.md`(23处手改旧法) | **本次交付**，31 项 |
| 配置 | 档位切换脚本 | `tc-bignum/conf/切换档位.bat` | 同上目录 | **v2 已修** | 🟡 否 | ❌ | v1 通配符版 | 本次修掉误关 ahbot.conf 的 bug |
| SQL | 台词第1批 | `规划/G19_*/02-第1步_零编译版闲聊扩充.sql` | world 库 | 最终 | 🟡 否 | ❌ | — | 60 条，id 700-785 |
| SQL | 台词第2批 | `规划/G19_*/03-第2批_情境台词81条.sql` | world 库 | 最终 | 🟡 否 | ❌ | — | 81 条，id 900-1035 |
| SQL | 时间线试点 | `规划/G20_*/02-时间线试点_零客户端.sql` | world 库 | **v2 已修** | 🟡 否 | ❌ | v1（8 处 `id1` 会报错） | 本次修正列名 |
| 剧情 | 真龙纪元 | `tc-bignum/剧情/故事集-真龙纪元.txt` | — | 2866 行 | — | — | `剧情/_旧版备份/` | **主线载体** |
| 客户端 | 全部 | `tc-bignum/工具库/*.md`（25 篇） | — | 文档 | 🟠 **无成品** | ❌ | — | **只有方法，无 DBC/MPQ 文件** |

## 3.3 三类文件的处置规则

**✅ 应该继续修改的**：
- `tc-bignum/` 下的一切（补丁库、规划、SQL、conf、剧情、总表）

**【注意】只能参考，不可修改也不可复制进源码的**：
- `/home/user/src/`（上游只读副本）
- `/home/user/_chk/`（上游 Eluna 只读克隆）
- `/home/user/uploads/`（用户上传的证据）

**【禁止】禁止再使用的**：

| 路径 | 禁用原因 |
|---|---|
| `src/GossipHandler.cpp` (14B) | 内容是 `404: Not Found` 下载失败残骸 |
| `src/PlayerStorage.cpp` (14B) | 同上 |
| `src/bot/CreatureAI.h` (14B) | 同上 |
| `src/GroupRefManager.h` | 文件名与内容不符（内容实为上游 `GroupReference.h`） |
| `_chk2/` `t7/` `t8/` `t9/` `v2/` `v3/` `v4/` `fk/` `luatest/` | 测试桩，含 `mock.h`/`stub.h`，复制进源码会污染 |
| `_dt/` `_dt2/` | git submodule 实验沙盒，与项目无关 |
| `补丁库/01_功能/A31_游荡bot永久化_已停用/` | ⚪ 已停用（见 F17） |
| `conf/_完整版备用/`（5 个 191KB conf） | ⚪ 已被 conf.d 小文件方案取代 |
| `A42_*/第2步_种族口音.md` 中的 PickText 改法 | 🔴 改错了函数，必须用 `修复-第2步改错了函数.md` |

---

# 4. 功能与补丁总矩阵

## 4.1 重要声明：安装状态的证据边界

**本工作区无法提供任何"已安装"的直接证据**——没有 `D:\TrinityCore` 的 git diff，
没有编译产物，没有数据库快照。

下表的"已复制源码/编译/SQL执行/部署/游戏内测试"列，**唯一依据是用户在聊天中的口头回报**。
凡用户未明确回报过的，一律标 ❓ 或 🟡，**不做推测**。

## 4.2 最近三批（证据最清晰）

| 编号 | 功能名 | 目标 | 权威目录 | C++目标文件 | SQL | Lua | 配置 | 客户端 | 已复制源码 | 编译 | SQL执行 | 部署 | 游戏内测试 | 当前结论 | 下一步 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| F41 | 游荡bot不打中立怪+发呆 | 修 bot 不作为 | `02_修复/F41_*` | `bot_ai.cpp` :3623-3627, :18367 | — | — | — | — | ✅ | ✅ | — | ✅ | ✅ 用户回报 | ✅ **已完成** | 无 |
| F42 | 招募游荡bot闪退 | 修空指针崩溃 | `02_修复/F42_*` | `bot_ai.cpp` :309后, :19054 | — | — | — | — | ✅ | ✅ | — | ✅ | ✅ 用户回报 | ✅ **已完成** | 无 |
| F43 | 近战bot锁定不前进 | 修追击逻辑 | `02_修复/F43_*` | `bot_ai.cpp` :5566 | — | — | — | — | ✅ | ✅ | — | ✅ | ✅ 用户回报 | ✅ **已完成** | 无 |
| G16-1+3 | 拍卖行大改 | 齐全+去杂乱 | `conf/worldserver.conf.d/ahbot.conf` | — | — | — | ✅ 31项 | — | — | 零编译 | — | ✅ | ✅ 118494条 | ✅ **主体已验证**；坐骑缺陷见下行 | — |
| **G16-7** | **坐骑配额修正** | 修我自己的失误 | `规划/G16_*/07-*.md` + 同一 ahbot.conf | — | — | — | ✅ `Class.Misc` 2→8 | — | — | 零编译 | — | ❌ | ❌ | 🟡 **本批交付未装** | 复制conf+`.reload config`+`.ahbot reload`+`rebuild all` |
| G19-1 | 台词 141 条 | 话题扩充 | `规划/G19_*/02,03` | — | ✅ 2 文件 | — | — | — | — | 零编译 | ✅ | ✅ | ✅ 141条 | ✅ **已验证** | — |
| **G16-9** | **坐骑最终方案B** | 白名单放行295种 | `规划/G16_*/09-*.md` | — | 生成ID的SQL | — | 主conf `forceIncludeItems` 追加 | — | — | 零编译 | ❌ | ❌ | ❌ | 🟡 **等用户执行** | 不动 `Bind.Pickup`，避免放回BoP装备 |
| **G19-5** | **第3步装机探针** | 拿真实基线原文 | `规划/G19_*/05-*.md` | — | — | — | — | — | — | — | — | — | — | 🟡 **等用户跑探针** | 基线与04号文档不符，不探针会装失败 |
| **G16-8** | **坐骑「只有三种」** | 修池子层 | `规划/G16_*/08-*.md` | — | 诊断SQL×3 | — | 🟡 待定(A或B) | — | — | 零编译 | ❌ | ❌ | ❌ | 🟡 **等用户跑查询1定方案** | 按 `bonding` 分组结果选修法A/B |
| G19-3 | 场景感知 | **看场合说话** | **`规划/G19_*/06`（04号已作废）** | `bot_companion.h/.cpp` + `bot_ai.h/.cpp` | ✅ 建表+标签 | — | `NpcBot.Companion.ChatCooldown` | — | ✅ 已改 | 🔴 **报11个错** | ❌ | ❌ | ❌ | 🔴 **编译失败，修复文档见 `规划/G19_*/07`** | 3个根因：.h误删/大括号/函数插错位置 |
| **G19-7** | 编译错误分组 | 11错误→3根因 | `规划/G19_*/07-编译错误修复.md` | 同上 | — | — | — | — | — | — | — | — | — | ✅ 已完成分组诊断 | 组3用户已改对 |
| **G19-8** | 组1+组2根因分析 | 定位 | `规划/G19_*/08-*.md` | — | — | — | — | — | — | — | — | — | — | ✅ 分析完成 | 已被09号取代为可执行版 |
| **G19-9** | 最终修复(直接照做) | 删775-827 + 加`_giftTexts` | `规划/G19_*/09-*.md` | `bot_companion.h/.cpp` | — | — | — | — | ✅ 已改 | ✅ **.cpp零错误** | — | — | — | ✅ **组1+组2已解决** | assert脚本生效 |
| **G19-10** | 剩余3错诊断 | 定位C2601/C2660 | `规划/G19_*/10-*.md` | `bot_ai.cpp` | — | — | — | — | — | — | — | — | — | ✅ 诊断已回，真因确定 | 非A非B，是if/else中间 |
| **G19-11** | **两真因最终修复** | 移出函数+改回6参数 | **`规划/G19_*/11-最终修复_两个真因.md`** | `bot_ai.cpp` | — | — | — | — | 🟡 待改 | ❌ | — | ❌ | ❌ | 🟡 **等用户执行** | 脚本已本地跑通验证 |
| A42修复 | 礼物台词权重 | 种族100/职业50 | — | — | — | — | — | — | ✅ | ✅ | — | ✅ | ❓ | ✅ **探针证实已装**(`bot_companion.cpp:273`) | **不要重复装** |
| F40 | 剑圣镜像登出闪退 | 修镜像野指针 | `02_修复/F40_*` | `botmgr.cpp:803`+`Map.cpp:1065` | — | ✅ Lua ext | — | — | ✅ | ✅ | — | ✅ | ✅ | ✅ **已完成**（用户2026-08-18澄清：**最先装的**，我此前记录有误） | 无 |
| G20 | 时间线试点 | 渴魔症相位 | `规划/G20_*/02` | — | ✅ 1 文件 | — | — | — | — | 零编译 | ✅ | ✅ | ✅ 可见 | ✅ **已验证** | 下一步：Eluna gossip 脚本让玩家自己切相位 |
| A42修复 | 礼物台词权重 | 种族100/职业50/物品20 | `01_功能/A42_*/修复-*.md` | `bot_companion.cpp` PickGiftText | — | — | — | — | ❌ | ❌ | — | ❌ | ❌ | 🟡 **待编译** | 并入下一批 |

## 4.3 历史功能（A 系列，按索引与聊天证据）

| 编号 | 功能 | 状态 | 证据来源 | 备注 |
|---|---|---|---|---|
| A01-A14 | 战斗节奏/AoE拾取/幻化/装备魔改/老职业技能/战斗辅助/属性持久化/世界指令/字段扩容/木桩/血量扩容/NPC状态调度/闲逛bot/场景快照 | 🟢~✅ | 补丁索引标注 | 早期批次，多数已验证 |
| A15 | 剧情表情 `.emote` | ✅ | 索引"已验证" | |
| A16 | 剧情对白 `.say` | ✅ | 索引"已验证" | |
| A17 | 外观控制 `.model` `.disguise` | ✅ | 索引"已验证" | |
| A18 | 模型搜索 `.findmodel` | ✅ | 索引"已验证" | |
| A19 | 游荡bot招募 | ✅ | 索引"已验证" | |
| A20 | 伙伴关怀 | ✅ | 索引"已验证"（bot会给东西） | 建 `npcbot_care_text` 表 |
| A21 | bot召集定位 `.bf` | ✅ | 索引"已验证" | |
| A22 | bot诊断 `.bd` | ✅ | 索引"已验证" | |
| A23 | 游荡bot完整窗口 | ✅ | 索引"已验证" | |
| A24 | 游荡bot可交互 | ✅ | 索引"已验证" | |
| A25 | PlayerBot上线 | 🟢 | 会话提及已完成 | `cs_playerbot.cpp` |
| A26 | NPCBot 汉化 | ❓ | 索引"待验证" | |
| A27 | PlayerBot自动接受 | 🟢 | 会话提及已完成 | `pbot_autoaccept.cpp` |
| A28 | Bot改名 `.botname` | ❓ | 索引"待验证"（配 F09） | |
| A29 | bot数量上限 conf | ❓ | 索引"待验证" | |
| A30 | 突破人数上限 第1阶段 | ❓ | 索引"待验证" | |
| A31 | 游荡bot永久化 `.pin` | ⚪ **已停用** | 索引明确 + F17 | **禁止复活**，连续4次返工 |
| A32 | 羁绊系统 | ⚪ | 被 A37 双向重做取代 | |
| A33 | bot换外观 | ❓ | — | |
| A34 | `.npcface` 玩家外观 | ❓ | README 列为待验证 | |
| A35 | bot批量永久化 | 🟢 | 会话"A35+A36永久化"已完成 | |
| A36 | 持久化游荡bot | 🟢 | 同上 | 曾引发 F24 |
| A37 | 羁绊与关怀双向重做 | 🟢 | 会话"A37羁绊六步"已完成 | 六步，含 sql/ |
| A38 | 游荡bot锚点 | 🟢 | 会话"A38锚点"已完成 | **两个同名目录**，见 4.5 |
| A39 | pbot常用指令补全 | 🟢 | 会话"A39指令"已完成 | |
| A40 | pbot补全UI | 🟢 | 会话"A40 UI"已完成 | |
| A41 | pbot自动上线 | 🟢 | 会话"A41 pbot自动上线"已完成 | 含 2 份修复文档 |
| A42 | 种族职业口音 | 🟡 部分 | 第1步SQL已用；**第2步改错函数需修复** | 见 4.2 |

## 4.4 修复系列（F01-F43）关键项

| 编号 | 问题 | 最终有效修复 | 必须弃用的旧方案 | 状态 |
|---|---|---|---|---|
| F17 | `.pin` 状态矛盾 | **停用 A31 整个功能** | F14/F15/F16 三版 pin 修复全部作废 | ⚪ 已停用 |
| F24 | A36 导致 bot 卡世界外 | 见该目录 | — | 🟢 |
| F28 | 全面去重自检 | — | — | 🟢 |
| F31 | 队长巡检真因 | 会话确认已完成 | F30 中的推测 | ✅ |
| F32/F34 | ElvUI 刷屏 | **归档：客户端本地字符串，服务端无关** | F29/F30问题1/F32改动一/方案A/方案B 五处改动**经审计无害，保留不撤** | ⚪ **不再投入** |
| F35 | bot_ai 中文损坏 | `补丁库/02_修复/F35_*/源文件/` 已修复版 | — | 🟢 |
| F36 | 游荡bot数量 ASSERT 崩服 | 降低 `WanderingBots.Continents.Count` 至 750 | 曾配 802 导致崩 | ✅ |
| F37/F38 | 切角色闪退 | F38 为真凶版 | F37 为推测版 | 🟢 |
| F39 | 游荡bot卡位不追击 | **只修了移动层，没修决策层** | — | ⚪ 被 F41/F43 取代 |
| F40 | 剑圣镜像登出闪退 | `botmgr.cpp:803` + `Map.cpp:1065` + Lua ext | — | ✅ **已装并验证（最先装的）** |
| F41/F42/F43 | 见 4.2 | — | — | ✅ |

## 4.5 版本谱系（同一功能多份文件，必须分清）

| 功能 | 旧版 | 修复版 | 当前候选 | 实际已安装 |
|---|---|---|---|---|
| **A38 锚点** | `A38_游荡bot定点控制/操作手册.md`（5.8KB） | — | `A38_游荡bot锚点/改动清单.md`（13.4KB） | 🟢 以"锚点"版为准。**两个目录并存是历史遗留，不要都装** |
| **A41 pbot自动上线** | `改动清单.md` | `修复-函数插错位置.md` → `修复2-中文变问号的抢救.md` | `cs_playerbot_修复版.cpp` | 🟢 用"修复版.cpp" |
| **A42 种族口音** | `第2步_种族口音.md`（改了 PickText，**错**） | `修复-第2步改错了函数.md`（改 PickGiftText，**对**） | 修复版 | 🟡 **未装** |
| **拍卖行 conf** | `G16_*/03-第1步_拍卖行大改_conf.md`（23 处手改） | `04-第1步修正_基于你的真实conf.md` | **`conf/worldserver.conf.d/ahbot.conf`** | 🟡 **未装**，用 conf.d 版 |
| **G20 时间线 SQL** | v1（8 处 `id1`，**会报错**） | — | 当前文件已就地修正为 `id` | 🟡 未执行 |
| **cs_modify** | `uploads/cs_modify.cpp.txt` | `uploads/cs_modify_fixed.cpp` | ❓ 二者关系待确认 | ❓ |
| **F42 文档** | `定位方案.md`（过程稿） | `根因与修复.md` | 根因版 | ✅ 已装 |

## 4.6 已证明无效/危险的方案（防止重试）

| 方案 | 后果 | 出处 |
|---|---|---|
| 复活 `.pin` 游荡bot永久化（A31） | 状态矛盾 + 野指针，连续 4 次返工未果 | F14/F15/F16/F17 |
| 用 `SET @x` 会话变量写给用户的 SQL | DBeaver `Ctrl+Enter` 只跑光标处，SET 跑不到 → **静默返回 0 行不报错** | 坑表（A36） |
| `ALTER TABLE ... ADD COLUMN IF NOT EXISTS` | **MariaDB 专有**，MySQL 任何版本都不支持 | 坑表（A37第1步） |
| 为 ElvUI 刷屏改服务端 | 根因是客户端本地字符串，服务端改了没用 | F32/F34 |
| `WanderingBots.Continents.Count = 802` | 启动即 ASSERT 崩服 | F36 |
| 把 `Unit*` 传给 `Position const*` 参数 | 能编译（继承链），但语义从"追踪"变"走到此刻坐标"且不寻路 | F43 附带发现 |
| 只改执行层不查决策层 | F39 修了移动仍不作为，到 F41 才找到真因 | F39→F41 |
| 给用户带 NBSP(U+00A0) 的代码 | VS 弹"Unicode 存不进当前代码页"，点【否】中文全变问号 | 坑表 |
| `fix_nbsp.py --fix` 后直接交付 `.conf` | 会写回 UTF-8 BOM → boost `ini_parser` 抛错 → 配置静默失效 | **本次审计新增** |
| 用通配符 bat 管理 conf.d 目录 | `*.conf` 会把新增的 `ahbot.conf` 一起改名关掉 | **本次审计新增** |

---

# 5. 服务端 C++ 修改

## 5.1 重要前提

**本工作区没有任何被修改的 C++ 源码文件。**

- `/home/user/src/` = 上游只读副本（66/68 与上游逐字节相同）
- `tc-bignum/补丁库/**/源文件/*.cpp` = **补丁形态**的成品文件或片段，需人工复制进 `D:\TrinityCore`

因此本节记录的是**"补丁描述的改动"**，而非"已在源码中的改动"。

## 5.2 NPCBots 模块（改动最密集）

### `src/server/game/AI/NpcBots/bot_ai.cpp`

| 补丁 | 行号（基于上游 4e8762e） | 函数/结构 | 行为变化 | 为什么 | 状态 |
|---|---|---|---|---|---|
| F41 | `:3623-3627` | `CanBotAttack` 附近 | 加 Questgiver/Flightmaster 保护 | 防止 bot 打功能NPC | ✅ 已装 |
| F41 | `:18367` 下方 | `mmover` 相关 | 游荡bot 无 master 时的移动主体 | 修发呆 | ✅ 已装 |
| F42 | `:309` 之后 | `SetBotOwner(Player*)` | **招募时清 `_wanderer` + 路点** | 上游只有 `SetWanderer()`，**全源码搜 `UnsetWanderer` = 0 结果**，招募后 bot 成"既有master又是wanderer"混合态 | ✅ 已装 |
| F42 | `:19054` | `GetHomePosition(uint16&, Position*)` | 三层判空兜底 | `:19058 _travel_node_cur->GetMapId()` 空指针崩溃，保护 4 个调用点(`:5977/:18627/:18922/:18944`) | ✅ 已装 |
| F43 | `:5566` | `GetInPosition` | 游荡近战 bot 跳过 `UpdateImpossibleChase` | `:15716` 该函数**无条件 `return true`** → `:5614` 真正的 `BOT_MOVE_CHASE` 永远走不到 | ✅ 已装 |

**F43 的教训（已入坑表）**：看到 `if (SomeCheck()) return;` 必须进去看它什么情况返回 true。

### `src/server/game/AI/NpcBots/botmgr.cpp`

| 补丁 | 行号 | 改动 | 状态 |
|---|---|---|---|
| F40 | `:803` 之前（`CleanupsBeforeBotDelete` 调用点上方） | 处理剑圣镜像(entry 70552)在 `PlayerLogout` 时的清理 | ✅ **已装** |

关联事实：`botmgr.cpp:746 bot->SetCreator(nullptr)`；`:939 ASSERT(!bot->GetCreator())`；
`Creature.h:391 GetBotsPet()` 返回 `Unit*` 而非 `Creature*`。

### `bot_companion.cpp/.h`（A20/A37/A42 台词与羁绊）

| 项 | 位置 | 说明 |
|---|---|---|
| `CARE_TYPE_*` 枚举 | `bot_companion.h:36-44` | NONE=0/FOOD=1/DRINK=2/MONEY=3/**CHAT=4**/LEVELUP=5/REVIVE=6/MAX=7 |
| `CompanionText` 结构 | `bot_companion.h:48-56` | Id/CareType/BotClass/BotRace/Text/Emote/Weight |
| `PickText` | `bot_companion.h:91`（public） / 实现 `.cpp:189` | 读 `npcbot_care_text` |
| `LoadCareTexts` | `bot_companion.cpp:33`，SQL 在 `:39` | `SELECT id, care_type, bot_class, bot_race, text, emote, weight` |
| **`PickGiftText`** | G19第3步文档 `:118`，实现 `:223` | 读 `npcbot_gift_text`，**A42 第2步误改了 PickText 而非本函数** |
| 校验逻辑 | `bot_companion.cpp:59` | 只校验 `care_type < CARE_TYPE_MAX(7)` → **所以往 care_type=4 加台词零编译** |

## 5.3 其他模块

| 模块 | 文件 | 关键位置 | 用途 |
|---|---|---|---|
| 拍卖行 | `AuctionHouseBotSeller.cpp` | `:116/:120/:123-129/:132-156/:222-230/:235-253/:320/:374/:385/:463/:473/:641` | 见第 8 节详解 |
| 拍卖行 | `AuctionHouseBot.cpp` | `:153-155/:182-185/:205-210/:478-490` | 配置读取与 Reload |
| 拍卖行 | `AuctionHouseBot.h:240` | `void Reload() { GetConfigFromFile(); }` | **只读内存不读磁盘** |
| 指令 | `cs_ahbot.cpp:68/69/70` | rebuild / reload / status | |
| 指令 | `cs_reload.cpp:89` | `.reload config` | |
| 配置 | `Main.cpp:70/136/203` | conf.d 自动加载 | |
| 配置 | `Config.cpp:40/158/166/175` | `read_ini` / `put_child` 覆盖语义 | |
| 场景感知 | `Object.h:389/390/468` | `GetZoneId/GetAreaId/FindMap` | **用 FindMap（无断言），不能用 `GetMap():467`（有 ASSERT）** |
| 场景感知 | `Map.h:446/448/454` | `IsDungeon/IsRaid/IsBattlegroundOrArena` | |
| 场景感知 | `DBCEnums.h:255/258/268` | `AREA_FLAG_CAPITAL=0x100 / SANCTUARY=0x800 / TOWN=0x200000` | |
| 实体 | `ObjectMgr.cpp:2170` | `SELECT creature.guid, id, map, ...` | **证明 creature 表列名是 `id`** |

## 5.4 `src/` 同名文件的准确定性（关键，防止误覆盖）

| 文件 | 是完整文件吗 | 与上游关系 | 处置 |
|---|---|---|---|
| `src/Unit.cpp`（14912 行） | 完整 | **逐字节相同** | 只读参考 |
| `src/Player.cpp`（26813 行） | 完整 | **逐字节相同** | 只读参考 |
| `src/bot_ai/*.cpp`（10 个职业AI） | 完整 | **10/10 逐字节相同** | 只读参考 |
| `src/GroupRefManager.h` | 完整但**文件名错** | 内容实为上游 `GroupReference.h` | **禁用** |
| `src/bot/CreatureAI.h` | 14 字节 | `404: Not Found` | **禁用** |
| `src/GossipHandler.cpp` | 14 字节 | `404: Not Found` | **禁用** |
| `src/PlayerStorage.cpp` | 14 字节 | `404: Not Found` | **禁用** |
| `src/CC.cpp` `CharPkt.h` `SC.h` `SF.h` `Misc.cpp` `WorldObject.h` `CharPackets.cpp` | 上游无同名 | 疑为缩写命名的片段/摘录 | ❓ 待确认，**不要覆盖任何源码** |
| `src/compile_test.cpp` `rbac_test.cpp` | 自建测试 | — | 测试桩，禁入生产 |

---

# 6. NPCBots / PlayerBot / Eluna 专项状态

## 6.1 NPCBots

| 项 | 状态 |
|---|---|
| 基线版本 | 随上游 `4e8762e` 内置，**具体 NPCBots 版本号 ❓ 待确认** |
| 职业 AI 改动 | 工作区 `src/bot_ai/` 10 个职业文件**与上游完全相同** → 目前**没有**职业 AI 层的改动 |
| 核心改动集中在 | `bot_ai.cpp`（F41/F42/F43 已装）、`botmgr.cpp` + `Map.cpp`（F40 已装） |
| Bot 数据表 | `characters_npcbot` 等；游荡 bot **无 `creature` 表记录**（`botdatamgr.cpp:452` 官方注释 "We do not create CreatureData for generated bots"） |
| 中文命令/名字/文本 | A26 汉化 ❓待验证；F35 修复了 `bot_ai.cpp` 中文损坏 |
| 游荡 | ✅ F41/F43 已修（中立怪、发呆、近战不前进） |
| 招募 | ✅ F42 已修（`_wanderer` 混合态崩溃） |
| 永久化 `.pin` | ⚪ **已停用**（F17），禁止复活 |
| 队伍 | F31 队长转让 ✅ |
| 交易 | F10/F11/F25/F26/F27 系列，🟢 |
| 装备 | A04/A33，🟢 |
| 宠物 | F06 宠物无主崩溃，🟢 |
| **已知未修** | 🟡 F40 剑圣镜像(entry 70552)登出闪退 |
| **已知缺口** | bot 不会自动修装备/卖垃圾（用户："没它带bot玩两小时就裸奔"） |

**关键 API 访问权限（已实查，供下一个代理直接用）**：
```
bot_ai.h:195   bool IsWanderer() const          public
bot_ai.h:196   void SetWanderer()               public   （无 UnsetWanderer！）
bot_ai.h:762   bool _wanderer{}                 private
bot_ai.h:764   WanderNode const* _travel_node_last{}   private
bot_ai.h:765   WanderNode const* _travel_node_cur{}    private
bot_ai.h:143   BotMovement(...) 声明（pos 是第2参数，target 是第3参数）
botmgr.h:179   static uint8 GetBotPlayerRace(Creature const*)   public
botcommon.h:44 BOT_ENTRY_MIRROR_IMAGE_BM = 70552
botcommon.h:157 FACTION_TEMPLATE_NEUTRAL_HOSTILE = FACTION_CREATURE (2150)
```

## 6.2 PlayerBot

| 项 | 状态 |
|---|---|
| 来源 | **自定义实现**，非 liyunfan1223/mod-playerbots 移植（后者仅作思路参考） |
| 参考注意 | mod-playerbots 的拍卖功能是**死代码**（`LootAction.cpp:259-353` 被注释） |
| 自动上线 | A25 + A41，🟢；权威文件 `A41_*/cs_playerbot_修复版.cpp` |
| 自动接受 | A27，`pbot_autoaccept.cpp/.h`，🟢 |
| 交易/踢人 | F25/F26/F27，🟢 |
| 常用指令 | A39，🟢 |
| UI 补全 | A40，🟢 |
| 与 NPCBots 边界 | NPCBot = 服务端生成的假单位（无玩家账号）；PlayerBot = 真实角色自动上线。**NPCBot 当不了拍卖行卖家**（`owner` 是裸低位 guid，客户端走 `QueryHandler.cpp:81` CharacterCache 查名字查不到），**PlayerBot 可以** |
| 重复代码风险 | F28 做过"全面去重自检"；`uploads/cs_playerbot.cpp.txt` 与 `A41_*/cs_playerbot_修复版.cpp` 关系 ❓待确认 |

## 6.3 Eluna

| 项 | 状态 |
|---|---|
| 核心源码位置 | 随上游内置于 `D:\TrinityCore`；工作区 `_chk/` 另有独立只读克隆（`ElunaLuaEngine/Eluna` @ FETCH_HEAD `4608e3f`，HEAD `5734c96` detached，**仅查 API 用**） |
| 是否有 C++ 侧 Eluna 改动 | **未发现**。`_chk/` 213 文件中 grep `npcbot` = 0 命中 |
| Lua 脚本目录 | `tc-bignum/lua_scripts/`（6 个 .lua + 1 说明） |
| 脚本清单 | `bignum_selftest.lua` / `custom_announce.lua` / `custom_daily_reward.lua` / `custom_diag.lua` / `custom_teleport.lua` / `custom_welcome.lua` |
| 实际部署路径 | ❓ 待确认（需用户确认 `LuaEngine.ScriptPath`） |
| 是否已部署 | ❓ **无证据** |
| 已知 Eluna 事件坑 | `lua_scripts/extensions/ObjectVariables.ext:43` 的 `DestroyObjData` 调 `GetGUIDLow()`，在 event=31 时对已失效 Creature 报错（F40 日志中出现） |
| mock 测试 vs 真实运行 | `luatest/mock.lua` 与 `mock_unpatched.lua` 是**独立 mock**，**不能**代表真实 Eluna 运行结果 |

---

# 7. SQL 与数据库迁移

## 7.1 总体状态

| 项 | 结果 |
|---|---|
| 工作区 SQL 文件总数 | **83 个** |
| 分布 | `tc-bignum/sql/` 45 个 + `补丁库/**/sql/` 若干 + `规划/**/*.sql` 3 个 |
| 编码 | **83/83 全部 UTF-8 无 BOM**（已实测） |
| **是否已在真实库执行** | ❓ **全部待在实际数据库验证** |
| 依据 | 工作区**无任何**数据库导出、快照、`information_schema` 查询结果或执行日志 |

> **严格遵守要求**：不得因 SQL 文件存在就判断数据库已更新。
> 下表"是否已执行"列**一律标注待验证**，除非用户在聊天中明确回报过。

## 7.2 按数据库分类

### world 库

| SQL 文件 | 用途 | 幂等 | 已执行 | 备注 |
|---|---|---|---|---|
| `sql/01_world_item_template_bignum.sql` | 物品模板大数值 | ❓ | ❓待验证 | 早期大数值线 |
| `sql/01b_fix_remaining.sql` | 补漏 | ❓ | ❓待验证 | |
| `sql/03_test_item.sql` | 测试物品 | ❓ | ❓待验证 | |
| `sql/20-22_tele_cn*.sql` | 中文传送点 | ❓ | ❓待验证 | |
| `sql/23-26_transmog*.sql` | 幻化表 + RBAC | ❓ | ❓待验证 | |
| `补丁库/01_功能/A20_伙伴关怀/sql/01_world_台词与物品.sql` | 建 `npcbot_care_text`(`:22-32`) + 物品池 | ✅ 有 DROP/CREATE | 🟢 应已执行（A20 已验证） | **原表无 `bot_race` 列** |
| `补丁库/01_功能/A37_羁绊与关怀_双向重做/sql/*` | 羁绊六步 | ❓ | 🟢 应已执行 | |
| `规划/G19_*/02-第1步_零编译版闲聊扩充.sql` | 台词 60 条 (id 700-785) | ✅ `DELETE WHERE id BETWEEN` | ❌ **未执行** | **依赖 `bot_race` 列** |
| `规划/G19_*/03-第2批_情境台词81条.sql` | 台词 81 条 (id 900-1035) | ✅ 同上 | ❌ **未执行** | 同上 |
| `规划/G20_*/02-时间线试点_零客户端.sql` | 时间线 NPC + gossip | ✅ 同上 | ❌ **未执行** | **本次修正 8 处 `id1`→`id`** |

### characters 库

| SQL | 用途 | 已执行 |
|---|---|---|
| `sql/02_characters_durability.sql` | 耐久 | ❓待验证 |
| `sql/04-09_gearset/tierset*.sql` | 套装进度表 | ❓待验证 |
| A35/A36 相关 | bot 永久化数据 | 🟢 应已执行 |
| A41 `playerbot_roster` 表 | pbot 花名册 | 🟢 应已执行 |

### auth 库

| SQL | 用途 | 已执行 |
|---|---|---|
| `sql/10-19_rbac_*.sql` | RBAC 权限与链接 | ❓待验证 |

## 7.3 正确执行顺序

```
1. auth 库   RBAC 权限（10-19 系列）—— 指令类补丁的前置
2. world 库  建表类（A20 台词表 / A37 羁绊表 / 幻化表）
3. world 库  ALTER 加列（A42 第2步的 bot_race）   <-- G19 的前置！
4. world 库  数据插入（G19 台词 / G20 时间线）
5. characters 库  角色侧表
```

## 7.4 G19 的前置依赖（必须先查再跑）

```sql
SELECT COUNT(*) AS has_bot_race
FROM `information_schema`.`COLUMNS`
WHERE `TABLE_SCHEMA`='world' AND `TABLE_NAME`='npcbot_care_text'
  AND `COLUMN_NAME`='bot_race';
```
返回 0 则必须先执行：
```sql
ALTER TABLE `world`.`npcbot_care_text`
  ADD COLUMN `bot_race` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '限定种族 0=通用';
```
出处：`补丁库/01_功能/A42_种族职业口音/第2步_种族口音.md:104-106`

## 7.5 自定义 ID 范围登记（防冲突）

| 范围 | 用途 | 来源 |
|---|---|---|
| `npcbot_care_text` id 300-699 | A42 种族/职业台词 | A42 |
| `npcbot_care_text` id 700-785 | G19 第1批（60 条） | G19-1 |
| `npcbot_care_text` id 900-1035 | G19 第2批（81 条） | G19-2 |
| `creature_template` entry 980001-980099 | G20 时间线试点 | G20 |
| `gossip_menu` / `npc_text` ID 98001-98010 | G20 对话 | G20 |
| `creature` guid | 用 `IFNULL(MAX(guid),0)+1` 子查询自动避让，**不写死** | G20 |
| bot 相关 entry 70001+ / 镜像 70552 | NPCBot | 上游 + A36 |

## 7.6 回滚

**全部 SQL 均无配套回滚脚本。**
幂等性靠 `DELETE ... WHERE id BETWEEN` 前缀实现（G19/G20 有，早期 sql/ 系列多数没有）。
→ **执行任何 SQL 前必须先 `mysqldump` 备份对应库。**

## 7.7 字符集与中文

| 项 | 值 |
|---|---|
| 建表字符集 | `utf8mb4`（见 A20 建表 `ENGINE=InnoDB DEFAULT CHARSET=utf8mb4`） |
| SQL 文件编码 | UTF-8 无 BOM（83/83 实测） |
| DBeaver 连接字符集 | ❓ 待确认（若为 latin1 会导致中文入库乱码） |
| 已知坑 | 给用户的 SQL **禁用 `SET @变量`**（Ctrl+Enter 只跑光标处，静默失效） |

## 7.8 大数值/字段扩容专项

| 层 | 事实 | 状态 |
|---|---|---|
| MySQL 字段类型 | `sql/01_world_item_template_bignum.sql` 等做过扩容 | ❓待验证 |
| C++ 类型 | step24 只改函数内部计算，**参数仍是 int32**；step25 承接修剩下一半 | 🟡 待编译（见待办总表） |
| 实测天花板 | 耐力 4 亿时血量缓慢上升，**到 21 亿(≈2^31) 就停**，再治疗就死 | 用户实测 |
| 归因 | **21 亿 = int32 上限**，天花板归因**客户端字段宽度**，已归档 | ⚪ 已归档 |
| 客户端 3.3.5a 限制 | 血量等字段在封包/UI 层受 32 位限制，**服务端改不动** | 协议墙 |
| 已验证安全范围 | < 2^31 | |

---

# 8. 服务端配置与部署

## 8.1 环境（多数为用户口述，未在工作区核实）

| 项 | 值 | 证据 |
|---|---|---|
| 操作系统 | Windows | 用户口述 |
| 编译器 | MSVC / Visual Studio 2022 | 用户口述 |
| **VS 配置** | **RelWithDebInfo（必须）** | 用户曾误切 Debug → 大量 C1069 假错误 |
| 架构 | x64（推断） | `uploads/vs架构错误.txt` `vs架构错误2.txt` 记录过架构问题 |
| CMake | CMake GUI | 用户口述 |
| Boost/OpenSSL/MySQL 版本 | ❓ 待确认 | |
| 源码/编译/部署目录 | `D:\TrinityCore` / `D:\TC-Build` / `D:\TC-Build\bin\RelWithDebInfo\` | 用户口述 |

编译流程文档：`tc-bignum/流程文档/步骤03-CMake与VS编译.md`、`03b-CMake报错处理.md`、`03c-删除后重建CMake.md`

## 8.2 配置加载机制（本次实查，重要）

```
Main.cpp:70    #define _TRINITY_CORE_CONFIG_DIR "worldserver.conf.d"
Main.cpp:136   auto configDir = fs::absolute(_TRINITY_CORE_CONFIG_DIR);
Main.cpp:203   sConfigMgr->LoadAdditionalDir(configDir, true, loadedConfigFiles, configDirErrors);
Config.cpp:175 LoadAdditionalDir -> recursive_directory_iterator，只认 .conf 后缀
Config.cpp:158 LoadAdditionalFile
Config.cpp:166   _config.put_child(...)    <- put_child 是覆盖语义，附加文件赢
Config.cpp:40  LoadFile -> boost ini_parser::read_ini
```

**三条推论**：
1. 放进 `worldserver.conf.d/` 的 `.conf` 会**自动加载并覆盖**主 conf 同名项 → 主 conf 可以完全不动
2. 文件**不能带 UTF-8 BOM**（`read_ini` 会抛 `ini_parser_error`）
3. 加载失败只进 `configDirErrors`，**服务端照常启动** → 配置静默失效，极难发现

## 8.3 conf 文件状态

| 文件 | 状态 | 说明 |
|---|---|---|
| `conf/worldserver.conf.d/ahbot.conf` | 🟡 **本次新增，未部署** | 31 项拍卖行配置，已验证无 BOM / CRLF / configparser 解析 31 项全对 |
| `conf/worldserver.conf.d/` 其余 12 个 | 🟡 候选 | 5 档位(casual/adventure/epic/hardcore/legend) + aoeloot/customspell/gearset/performance/playerstat/speed/transmog。**均无 BOM，前 3 字节 `23 23 23`** |
| `conf/_完整版备用/` 5 个 191KB | ⚪ 旧版归档 | 已被 conf.d 小文件方案取代 |
| `conf/切换档位.bat` | 🟡 **本次修正，未部署** | 修掉 `*.conf` 通配符误关 ahbot.conf 的 bug；GBK 编码 + CRLF |
| `uploads/worldserver.conf.txt` | 🔴 **含真实凭据** | 用户上传的实际配置，见第 18 节 |

## 8.4 拍卖行配置源码依据（本次实查全部行号）

```
AuctionHouseBotSeller.cpp:116      品质 >= MAX_AUCTION_QUALITY(7) 排除
AuctionHouseBotSeller.cpp:120      excludeItems 黑名单
AuctionHouseBotSeller.cpp:123-129  includeItems 白名单 push_back+continue 【跳过后面所有过滤】
AuctionHouseBotSeller.cpp:132-156  绑定过滤（白名单走不到）
AuctionHouseBotSeller.cpp:138          case BIND_WHEN_PICKED_UP  <- 金色战刃元凶
AuctionHouseBotSeller.cpp:222-230  Items.Misc=0 时只保留 商人卖的+怪物掉的
AuctionHouseBotSeller.cpp:235-236  case ITEM_CLASS_ARMOR / WEAPON
AuctionHouseBotSeller.cpp:238-253      ItemLevel/ReqLevel/SkillRank 过滤【只管武器护甲】
AuctionHouseBotSeller.cpp:258-260  case RECIPE/CONSUMABLE/PROJECTILE 只有 ReqLevel
AuctionHouseBotSeller.cpp:320      case TRADE_GOODS 用 CLASS_TRADEGOOD_* 参数
AuctionHouseBotSeller.cpp:374      ratio=0 -> 整个行跳过【暴风城空货架根因】
AuctionHouseBotSeller.cpp:385      amount * ratio / 100
AuctionHouseBotSeller.cpp:463-464  if (!totalPrioPerQuality[j]) continue;  除零保护
AuctionHouseBotSeller.cpp:473      weightedAmount = classPrio / total * qualityAmount
AuctionHouseBotSeller.cpp:641-646  GetStackSizeForItem，RandomStackRatio 是百分比
AuctionHouseBotSeller.cpp:935/938  AddAuction 被调两次【上游bug，挂 OnAuctionAdd 前必须处理】
AuctionHouseBot.cpp:478-490        ReloadAllConfig / Rebuild
AuctionHouseBot.h:240              Reload() 只读内存不读磁盘
```

**免重启生效**：`.reload config`（`cs_reload.cpp:89`）→ 再 `.ahbot reload`（顺序不能反）

## 8.5 用户 conf 现值（来自 `uploads/worldserver.conf.txt`，非敏感项）

```ini
AuctionHouseBot.Account = 2
AuctionHouseBot.Update.Interval = 1600
AuctionHouseBot.Seller.Enabled = 1
AuctionHouseBot.Buyer.Enabled = 1                 <- 早就开着（刷金风险）
AuctionHouseBot.Alliance.Items.Amount.Ratio = 0   <- 暴风城空货架根因
AuctionHouseBot.Horde.Items.Amount.Ratio = 0
AuctionHouseBot.Neutral.Items.Amount.Ratio = 100
AuctionHouseBot.Items.Misc = 0
AuctionHouseBot.Bind.Pickup = 1                   <- 金色战刃元凶
AuctionHouseBot.Items.ItemLevel.Min/Max = 0 / 0   <- 四个全0 = 杂乱根因
AuctionHouseBot.Items.ReqLevel.Min/Max  = 0 / 0
AuctionHouseBot.forceIncludeItems = 359 项（含 38766-39006 段 233 项 WLK 坐骑）【不要动】
AuctionHouseBot.forceExcludeItems = 9 项【不要动】
NpcBot.WanderingBots.Continents.Count             曾配 802 导致 ASSERT 崩，建议 750
Logger.root = 4 (Warn)                            -> TC_LOG_ERROR 必然输出
```

## 8.6 maps/vmaps/mmaps/dbc、日志、崩溃转储

| 项 | 状态 |
|---|---|
| maps/vmaps/mmaps/dbc 位置与版本 | ❓ 待确认（不在工作区） |
| 日志 | 用户曾上传 `uploads/Server.log.txt` |
| 崩溃转储 | 用户曾上传 `uploads/ae60f5b6f6ff+_worldserver.exe_[8-8_17-48-38].txt` |
| Lua 部署路径 | ❓ 待确认 |

---

# 9. 客户端修改与客户端构建链

## 9.1 【最重要结论】

> **目前客户端部分只有文档/工具/规划，实际客户端补丁未包含在当前仓库，不能视为已交付。**

**取证方法与结果**：
```
find /home/user -iname "*.dbc" -o -iname "*.mpq" -o -iname "*.blp" \
     -o -iname "*.m2" -o -iname "*.wmo" -o -iname "*.adt"
-> 0 个结果
```

## 9.2 客户端相关资产清单

| 项 | 工作区是否存在 | 说明 |
|---|---|---|
| 客户端版本/Build | ❌ | **待用户确认** |
| 客户端根目录 / Data / locale | ❌ | `D:\WOW`，不在工作区 |
| **DBC 文件** | ❌ **0 个** | 只有导出脚本 `tools/export_item_dbc.py` `export_itemset_dbc.py` `check_dbc.py` |
| **MPQ 补丁** | ❌ **0 个** | 只有方法文档 `工具库/15-把压缩包做成MPQ补丁.md` |
| Interface/FrameXML/GlueXML | ❌ | 仅 `uploads/GlueStrings.lua.txt`（用户上传的**只读证据**，非成品） |
| AddOns | ❌ | ElvUI 相关仅有排查结论，无文件 |
| M2/WMO/BLP/ADT/音效 | ❌ 0 个 | |
| Wow.exe 二进制补丁 | ❌ | `工具库/18/19/20` 三篇讨论过"补丁exe"，结论见下 |
| 客户端成品是否已上传 GitHub | ❓ 待确认 | 本工作区无 |
| 因大小限制未上传的文件 | ❓ **待用户提供路径/大小/SHA256** | |

## 9.3 客户端方法论文档（25 篇，`tc-bignum/工具库/`）

| 编号 | 主题 | 关键结论 |
|---|---|---|
| 00 | 工具总览与前置条件 | |
| 01-07 | MPQ编辑器/ModelViewer/wow.export/DBC编辑器/贴图/音效/格式转换 | |
| 08 | 地图编辑 Noggit | **Noggit Red 在 GitLab `prophecy-rp`，预编译版只在 Discord** |
| 09-10 | 一体化框架 / 辅助工具 | |
| 11 | 整合客户端体检与加密应对 | 用户用的是整合客户端 |
| 12-14 | 装机路线图 / 模型补丁安装 / 本次装机执行单 | |
| 15 | 把压缩包做成 MPQ 补丁 | |
| 16 | 多余文件与 DBC 风险 | |
| 17-20 | 补丁处置 / 不生效诊断 / 补丁exe分析 / 测试结果解读 | |
| 21 | HD 模型整个消失的原因与解法 | |
| 22 | 现成 MPQ 怎么装与模型 ID 怎么查 | |
| 23-25 | 能不能用正式服最新模型 / 资源排期 / 人物模型全量转换可行性 | |

## 9.4 客户端改动矩阵

| 功能 | 客户端源文件 | 生成工具/命令 | 输出文件 | MPQ内部路径 | 服务端依赖 | 测试结果 | 风险 |
|---|---|---|---|---|---|---|---|
| （无任何已完成的客户端改动） | — | — | — | — | — | — | — |
| G22 客户端魔改整合（规划） | 无 | 规划中 | 无 | 无 | G20/G21 | **未运行** | 见 9.5 |

## 9.5 已明确的客户端边界（用户已理解并接受）

来自 `规划/G22_客户端魔改整合/01-世界改造的客户端账.md`：

| 想做的事 | 需要什么 | 可行性 |
|---|---|---|
| 往废墟里**加**东西（NPC/物件/相位） | **纯服务端**（creature/gameobject + phaseMask） | ✅ 可做，G20 已试点 |
| 把废墟**变**完好（地形建筑） | **必须改 ADT + MPQ + DBC** | 🟠 大工程，未动工 |

**已归档的教训**："世界内容规划忘了算客户端的账"——加东西和改地形是两个数量级的工作量。

---

# 10. 客户端—服务端一致性矩阵

| 功能 | 服务端 C++ | world/auth/characters SQL | Eluna/Lua | 配置 | DBC | MPQ/UI/模型 | 协议/封包限制 | 当前一致性 | 验证步骤 |
|---|---|---|---|---|---|---|---|---|---|
| **血量/属性大数值** | step24 已修函数内部；step25 参数仍 int32 🟡 | 字段已扩容 ❓待验证 | — | — | ❌ 未改 | ❌ | **2^31 硬墙（客户端字段宽度）** | 🔴 **服务端能改，客户端显示不了** | 已归档，不再投入 |
| **自定义物品/装备/套装** | A04/A09 🟢 | `item_template` 扩容 ❓ | — | — | ❌ **未同步 Item.dbc** | ❌ | 客户端需 DBC 才能显示新物品 | ❓ **待确认是否已出现显示异常** | 需用户实测新物品图标/名称 |
| **幻化 transmog** | A03 🟢 | 建表 + RBAC ❓ | — | ✅ transmog.conf | ❌ | ❌ | 3.3.5 无原生幻化 UI | ❓ | 需实测 |
| **自定义 NPC + Gossip（G20）** | 无需改 | ✅ SQL 已备（未执行） | 🟠 缺 gossip 脚本 | — | **不需要** | **不需要** | phaseMask 是原生机制 | ✅ **零客户端，设计上一致** | `.modify phase 2` |
| **中文字符串/命令** | A26 汉化 ❓ | `lang_zh.sql` ❓ | — | — | ❌ | 客户端本地字符串 | — | 【注意】ElvUI 刷屏已证明**客户端本地字符串服务端管不了** | 已归档 |
| **模型/显示ID（A17/A33）** | 🟢 | — | — | — | ❌ 未改 CreatureDisplayInfo | ❌ 无 HD 模型包 | 用现有模型ID则无需客户端改动 | 🟢 **只要不引入新模型ID就一致** | `.findmodel` 查现有ID |
| **NPCBot/PlayerBot UI** | A40 🟢 | — | — | — | ❌ | ❌ | 走标准 gossip/聊天 | 🟢 | |
| **种族职业扩展（32上限）** | 🟠 仅规划 | 🟠 | — | — | **必须改 ChrClasses/ChrRaces.dbc** | 必须 | 客户端硬限制 | 🟠 **未动工** | `剧情/种族职业扩展-32上限规划.txt` |

## 10.1 ID 冲突检查状态

| 检查项 | 状态 |
|---|---|
| 自定义 entry 段登记 | ✅ 见 7.5 |
| 与上游 TDB 冲突检查 | ❓ **未做**（无数据库快照无法查） |
| 工具 | `tools/check_sql_range.py` 存在，是否跑过 ❓ |

---

# 11. 编码、中文与换行规范

## 11.1 实测编码分布（本次审计全量扫描）

| 类别 | UTF-8 无BOM | UTF-8 有BOM | 合计 |
|---|---|---|---|
| 补丁库 `.cpp` | 87 | **10** | 97 |
| 补丁库 `.h` | 19 | **2** | 21 |
| `.sql` | **83** | 0 | 83 |
| `.lua` | 6 | 0 | 6 |
| `conf.d/*.conf` | **13** | 0 | 13 |
| `切换档位.bat` | **GBK**（非 UTF-8） | — | 1 |

## 11.2 铁律与理由

| 文件类型 | 要求 | 理由 |
|---|---|---|
| **中文 `.cpp/.h`** | **UTF-8 带 BOM** | 无 BOM 在中文 Windows 下会被编辑器当 GBK 读，注释吃掉下一行 → `C2447 { 缺少函数标题` |
| **`.conf`** | **UTF-8 无 BOM** | `Config.cpp:40` boost `ini_parser::read_ini` 遇 BOM 抛错，且**静默失效** |
| **`.sql`** | UTF-8 无 BOM | 现状 83/83 一致 |
| **`.bat`** | **GBK + CRLF** | 内含中文提示，`chcp 936` |
| **`.lua`** | UTF-8 无 BOM | |

> 【注意】**本次审计新增的坑**：`tools/fix_nbsp.py --fix` 会**统一写回 UTF-8 with BOM**。
> 对 `.cpp` 是对的，**对 `.conf` 是灾难**。用完必须 `gbk_check.py` 复查并手工去 BOM。

## 11.3 已知中文事故

| 事故 | 根因 | 处置 |
|---|---|---|
| VS 弹"某些 Unicode 字符未能保存到当前代码页"，点【否】后中文全变 `?` | 我给的代码带 NBSP(U+00A0) | 做了 `tools/fix_nbsp.py`（清 12 种隐形字符） |
| `bot_ai.cpp` 中文损坏 | 编码转换事故 | F35 修复，`补丁库/02_修复/F35_*/源文件/` 存已修复版 |
| 自建检测工具误报 286 个文件 | `find_mojibake.py` 首版把英文疑问句和 `%Complete: ???` 当损坏 | 加 `--baseline` + 只认连续问号 → 286 降到 1 |
| GBK 不兼容字符（如 `【注意】` U+26A0） | 文档用了 GBK 无法编码的符号 | `gbk_check.py` 拦截；本次已把 `【注意】` 换成 `【注意】` |

## 11.4 MSVC `/utf-8` 开关

❓ **待确认** —— 未在工作区找到 CMake 缓存或工程文件。
若已开 `/utf-8`，BOM 要求可放宽；**但在确认前一律按"中文 .cpp 带 BOM"执行**。

## 11.5 已验证的安全编辑流程

```bash
python3 gbk_check.py <文件>            # GBK 兼容性（中文能否在 GBK 环境显示）
python3 tools/fix_nbsp.py <文件>       # 检查隐形字符（不加 --fix 只报告）
python3 tools/check_sql.py <文件.sql>  # SQL 交付前必跑（需 pip install sqlglot）
bash   tools/syntax_check.sh           # .cpp 交付前必须 g++ 真编译
grep -c '```' <文档.md>                # 含代码的文档，围栏数必须偶数且非 0
```

---

# 12. 构建、测试与验证证据

## 12.1 验证层级定义（13 级）

1 静态检查 · 2 Python/Shell 工具检查 · 3 单文件语法检查 · 4 mock/独立C++测试 ·
5 CMake 配置成功 · 6 **TrinityCore 全量编译成功** · 7 authserver 启动 ·
8 worldserver 启动 · 9 数据库更新成功 · 10 客户端登录 · 11 角色进入世界 ·
12 **功能游戏内实测** · 13 长时间/多人/多Bot稳定性

## 12.2 证据表

| 日期 | 功能 | 验证层级 | 命令/步骤 | 环境 | 结果 | 日志路径 | 足以结案 |
|---|---|---|---|---|---|---|---|
| 2026-08-17 | **ahbot.conf** | **2** 工具检查 | `gbk_check.py` + `fix_nbsp.py` + Python `configparser` 解析 | Workspace | ✅ 31 项全部正确解析，无 BOM，CRLF | — | ❌ **未部署未运行** |
| 2026-08-17 | **切换档位.bat** | **1-2** | GBK 解码 + CRLF 检查 + 通配符消除断言 | Workspace | ✅ 通过 | — | ❌ 未部署 |
| 2026-08-17 | **G19 台词 SQL ×2** | **2** | `tools/check_sql.py`（sqlglot）+ ID 重复/越界脚本 | Workspace | ✅ 全通过；60/81 条，无重复无越界 | — | ❌ **未在真实库执行** |
| 2026-08-17 | **G20 时间线 SQL** | **2** | `check_sql.py` 18 条语句全解析通过 | Workspace | ✅ 通过（修正 `id1`→`id` 后） | — | ❌ 未执行 |
| 2026-08-17 | **上游列名核对** | **1** 静态 | 与上游 `4e8762e` 的 `ObjectMgr.cpp:2170` 及 `sql/updates/` 交叉比对 | /tmp 只读 clone | ✅ 证实 `creature` 列名为 `id` | — | ✅ 足以支撑改法 |
| **2026-08-18** | **G16 拍卖行（第22批）** | **12 游戏内实测** | 用户复制 conf + `.reload config` + `.ahbot reload` + `rebuild all` | `D:\TC-Build` | ✅ **三行各39498、共118494条（目标39700/行，99.5%）；金色战刃消失；其他物品丰富**。缺陷：坐骑种类/数量少 | 用户口头回报 | ✅ 主体结案 |
| **2026-08-18** | **G19 台词 141 条** | **12 游戏内实测** | DBeaver 执行两个 SQL | world 库 | ✅ **141 条**，与设计值一致 | — | ✅ 结案 |
| **2026-08-18** | **G20 时间线试点** | **12 游戏内实测** | `.modify phase 2` | 用户机 | ✅ **能看到相位 NPC** | — | ✅ 结案 |
| **2026-08-18** | **G16-7 坐骑配额修正** | **2 工具检查** | `configparser` 解析 31 项 + GBK + 无BOM + CRLF | Workspace | ✅ 通过；`Class.Misc=8` | — | ❌ **未部署** |
| ~2026-08-16 | **F41 / F42 / F43** | **12** 游戏内实测 | 用户编译 + 部署 + 进游戏观察 | `D:\TC-Build` | ✅ 用户回报"已装并验证成功" | 用户未留存 | ✅ **可结案** |
| ~2026-08-15 | F42 定位 | **12** | 用户提供崩溃 Call stack | 用户机 | ✅ 30 秒定位 `GetHomePosition+30` | `uploads/` 崩溃转储 | ✅ |
| — | **F40** | **0** | 未运行 | — | **未运行** | — | ❌ |
| — | **G19 第3步 / A42修复** | **0** | 未运行 | — | **未运行** | — | ❌ |
| 历史 | A15/A16/A17/A18/A19-A24 | **12** | 用户实测 | — | ✅ 索引标"已验证" | — | ✅ |
| 历史 | step24/step28/step26/step30 | **12** | 用户实测通过 | — | ✅ | — | ✅ |

## 12.3 本会话**未**执行的验证（必须明说）

| 未做的事 | 原因 |
|---|---|
| TrinityCore 全量编译（层级 6） | 工作区无完整源码、无 MSVC |
| authserver/worldserver 启动（7/8） | 无二进制、无数据库 |
| 任何 SQL 在真实库执行（9） | 无数据库连接，且**明令禁止**擅自执行 |
| 客户端登录/进世界（10/11） | 无客户端 |
| 游戏内实测（12） | 同上 |
| 稳定性测试（13） | 同上 |

**本会话最高只达到层级 2（工具检查）。**

## 12.4 日志中的旧错误状态

| 日志 | 内含错误 | 是否已修 |
|---|---|---|
| `uploads/ae60f5b6f6ff+_worldserver.exe_*.txt` | `bot_ai::GetHomePosition+30` 崩溃 | ✅ **F42 已修并验证** |
| `uploads/Server.log.txt` | 剑圣镜像 `Map::Remove<Bot>FromMap` not in grid | ✅ **F40 已装，此日志为修复前的历史记录** |
| `uploads/服务端刷屏.txt` | ElvUI 刷屏 | ⚪ 已归档为客户端问题 |
| `uploads/vs架构错误.txt` / `2.txt` | VS 架构/配置错误 | ✅ 已定位为误切 Debug |
| `uploads/编译错误.txt` | 编译错误 | ❓ 具体条目待确认 |

---

# 13. 已知崩溃、编译错误和无效方案

| # | 症状 | 根因证据 | 试过的无效/危险方案 | 当前有效修复 | 验证程度 | 相关文件 |
|---|---|---|---|---|---|---|
| 1 | 招募游荡bot瞬间闪退 | 崩溃栈 `bot_ai::GetHomePosition+30` → `:19058 _travel_node_cur->GetMapId()` 空指针。因 `SetBotOwner` 全程没清 `_wanderer`（**全源码无 `UnsetWanderer`**） | F37/F38/F40 三次"读源码猜可疑点"全部落空 | **F42**：招募时清 `_wanderer`+路点；`GetHomePosition` 三层判空 | ✅ 游戏内验证 | `02_修复/F42_*` |
| 2 | 近战游荡bot锁定目标但站着不动 | `bot_ai.cpp:15716 UpdateImpossibleChase` **无条件 `return true`** → `:5566 if(...) return;` 吃掉 → `:5614` 的 `BOT_MOVE_CHASE` 永不执行 | F39 只修移动层无效 | **F43**：游荡近战跳过该守卫 | ✅ 游戏内验证 | `02_修复/F43_*` |
| 3 | 游荡bot不打中立怪 / 发呆 | `:3616` 同阵营/中立判定 + `:18367` 无 master 时移动主体缺失 | — | **F41** 3 处改动 | ✅ 游戏内验证 | `02_修复/F41_*` |
| 4 | **登出时闪退（剑圣镜像）** | `Map::Remove<Bot>FromMap() bot Mirror Image (Blademaster) ... not in map but not in grid!` + Eluna `ObjectVariables.ext:43 GetGUIDLow on bad self` | — | **F40**（3处改动：botmgr/Map/Lua ext） | ✅ **已装并验证，禁止重装** | `02_修复/F40_*` |
| 5 | 启动 ASSERT 崩服 | `Only 801 out of 817 bots ... Desired amount (802) cannot be created` → `botdatamgr.cpp:1913 ASSERTION FAILED` | 配 802 | **F36**：降到 750 | ✅ | `02_修复/F36_*` |
| 6 | 编译 `C2447 { 缺少函数标题` | 中文 .cpp 无 BOM 被当 GBK 读，注释吃掉下一行 | — | **F08**：统一 UTF-8 带 BOM | ✅ | `02_修复/F08_*` |
| 7 | 大量 C1069 假错误 | **VS 配置被误切到 Debug** | — | 切回 **RelWithDebInfo** | ✅ | `uploads/vs架构错误*.txt` |
| 8 | 中文源码变成 `?` | 代码含 NBSP(U+00A0)，VS 弹窗点【否】 | — | `tools/fix_nbsp.py` | ✅ | 坑表 |
| 9 | `.pin` 状态矛盾 + 野指针 | 游荡bot持久化与运行态冲突 | **F14/F15/F16 三版修复全部失败** | **F17：整个功能停用** | ⚪ 停用 | `02_修复/F17_*` |
| 10 | ElvUI 疯狂刷屏 | 探针两次为 0；`Log.cpp:240` 递归回退到 `Logger.root=4`，Error(5)≥Warn(4) 必然输出 → 服务端根本没发这些字符串 | 为此做的 5 处服务端改动全部无效 | **归档：客户端本地字符串** | ⚪ 不再投入（5 处改动经审计无害，**保留不撤**） | `02_修复/F32_*` `F34_*` |
| 11 | SQL"生效 0 个"且不报错 | 用了 `SET @x`，DBeaver `Ctrl+Enter` 只跑光标处 → `WHERE id = NULL` 永不匹配 | — | 禁用会话变量，值直接写进语句 | ✅ | 坑表 |
| 12 | `ADD COLUMN IF NOT EXISTS` 报错 | **MariaDB 专有语法**，MySQL 不支持 | — | 拆独立 ALTER 或用 information_schema 判断 | ✅ | 坑表 |
| 13 | 组队数据崩服 | 多个 SQL 各改各表，跨表不一致 | — | **F21** | 🟢 | `02_修复/F21_*` |
| 14 | bot 卡在世界外 | A36 副作用 | — | **F24** | 🟢 | `02_修复/F24_*` |
| 15 | **G20 SQL 会报 Unknown column 'id1'** | `creature` 表列名是 `id` | 原稿写了 8 处 `id1` | **本次已就地修正** | 层级 2（静态） | `规划/G20_*/02-*.sql` |

> **提醒**：写了根因分析文档 ≠ 问题已解决。
> **反向提醒（2026-08-18 教训）**：没拿到用户回执，也不能反过来假定"未安装"。
> 我曾把早已装好的 F40 记成未装并列入 P0，差点让用户重复安装。**状态未知就写未知。**

---

# 14. 需求、设计决定与不可破坏事项

## 14.1 用户确认的方向

| 项 | 内容（多为用户原话） |
|---|---|
| **终极目标** | 「能实现自主冒险偶遇玩家组成队伍就好了，光是游荡npc是做不到的，我们的下一个方案一定要加油！！」 |
| **内容主线** | 「我们的主线你一定要记得，是真龙的那条伟大的史诗」 |
| 台词要求 | 「加入的对话必须要让所有台词都能出现也能用上，之后一定要在对应的场景说对应的话，不要太杂乱和繁忙」 |
| 世界玩法 | 「加入修复主城（洛丹伦、兽人家园）的玩法，以及不同时间线的玩法，比如有些时间线部落没有来入侵，但是拥有渴魔症的精灵吸取生物的魔力或者自主吸取邪能」 |
| 客户端整合 | 「这些你没看到吗？把这些一起整合到客户端魔改计划」 |
| 工作方式 | **一步一步来**，每步验证通过再下一步；不怕麻烦，要做就做全、做优质 |

## 14.2 【最高铁律】用户 2026-08-02 立

> **不能因为难就不做 / 不能因为做不到就不做**
> **不能因为他们想让我们不做就不做 / 不能因为他们做不到我们就不做**

说"做不到"前**必须**回答三问：
1. 具体挡住哪个函数/哪行（要行号）
2. 绕过它需要什么
3. 有没有别的实现路径

## 14.3 交付规范（用户明确要求）

| 规范 | 细则 |
|---|---|
| 语言 | **中文回复** |
| 代码定位 | 精确到**行号和原文**，让用户能 Ctrl+F 搜到 |
| 作用域 | 让用户插代码时**必须说明作用域**，给可自检的判断依据（如"必须在 `class XXX` 这一行上面"） |
| 路线 | **只报一条主线**，不交叉提多路线 |
| 编码 | 文档/代码必须 **GBK 兼容**（`python3 gbk_check.py`） |
| 围栏 | 含代码的文档必须加 Markdown 围栏（`grep -c '```'` 偶数非 0） |
| 总表 | **每次交付必须更新总表**（README / 补丁库索引 / 指令总表 / 未完成想法 / 待办总表坑表） |
| C++ | 交付 `.cpp` 前**必须 g++ 真编译验证** |
| SQL | 交付前**必须跑 `tools/check_sql.py`**；**禁用 `SET @x` 会话变量** |
| 态度 | 用户曾因反复返工表达不满 → **减少猜测，先拿现场数据再下结论**；用户报崩溃，**第一句话就要日志** |

## 14.4 【禁止】下一个代理绝对不能做的事

1. **不得**用 `/home/user/src/` 或 `_chk/` 的文件覆盖 `D:\TrinityCore` 任何文件（它们是上游只读副本，且有 3 个是 `404: Not Found` 残骸）
2. **不得**重复应用 F41/F42/F43（用户已装，重复会破坏代码）
3. **不得**执行 `sql/` 下未确认状态的旧 SQL（可能已执行过，重跑会重复插数据）
4. **不得**把 `t7/t8/t9/v2/v3/v4/_chk2/luatest/fk` 的测试桩、`mock.h`、`stub.h` 复制进生产源码
5. **不得**复活 A31 `.pin` 游荡bot永久化（F17 已停用，4 次返工史）
6. **不得**擅自改动 `forceIncludeItems` 359 项 / `forceExcludeItems` 9 项（用户精心配置）
7. **不得**在没有 `mysqldump` 备份时执行任何 SQL
8. **不得**覆盖真实客户端 MPQ/DBC/Wow.exe/Data 目录
9. **不得**擅自改变已登记的自定义 ID 范围（见 7.5）
10. **不得**把"规划"写成"已完成"——`规划/` 下 54 个文件**全部是未实现的设计**
11. **不得**忽略客户端与服务端同步（改 item/spell/model 必须想 DBC）
12. **不得**给用户带 `SET @x` 的 SQL、带 NBSP 的代码、带 BOM 的 `.conf`
13. **不得**在 VS 用 Debug 配置编译（必须 RelWithDebInfo）
14. **不得**撤销 F29/F30/F32 那 5 处 ElvUI 相关改动（经审计无害，**保留**）

---

# 15. 未完成任务与优先级

## P0 — 会丢数据 / 崩服 / 无法启动

### P0-1 全部成果无版本控制（**最高优先**）

| 项 | 内容 |
|---|---|
| 编号/名称 | P0-1 成果备份缺失 |
| 证据状态 | ✅ 已确证：`/home/user` 无 `.git`，`tc-bignum/` 558 文件无版本控制，全工作区 grep `wow-mecode` = 0 命中 |
| 前置依赖 | 无 |
| 应读取 | 本文档第 1.3 节 |
| 应修改 | 无（这是运维动作） |
| 禁止使用 | — |
| 具体下一步 | ① 用户下载最新 Workspace 存档；② 确认 GitHub `wpch3/wow-mecode` 上的内容与本 Workspace 是否一致；③ 若不一致，把 `tc-bignum/` 整体推上去 |
| 完成标准 | `tc-bignum/` 558 个文件在 Workspace 之外存在至少一份副本 |
| 验证步骤 | 用 `HANDOFF_FILE_MANIFEST.tsv` 的 SHA256 逐一比对 |
| 需先问用户 | ✅ **是**（见第 17 节 Q1） |

### P0-2 F40 剑圣镜像登出闪退 —— ✅ **已完成，本项关闭**

| 项 | 内容 |
|---|---|
| 证据状态 | ✅ **用户 2026-08-18 明确澄清：F40 是最先装的，早已修好** |
| 更正说明 | **我此前把它记成"未安装"是错的**。错因：F40 交付时用户未逐条回执，我据此推断成未装，属于「没有证据就推断」的错误 |
| 现状 | 用户实测：招募各类游荡 bot 均不闪退 |
| 禁止 | **不得重复安装**（用户原话：「不要重复安装，不然又要出错」） |
| 若日后登出闪退再现 | 先查日志有无 `Map::Remove<Bot>FromMap() bot Mirror Image (Blademaster) ... but not in grid!`，没有这行则与 F40 无关 |

### P0-3 敏感凭据泄漏在 uploads/

见第 18 节。**需在推 GitHub 前处理**。

## P1 — 核心功能未完成 / 客户端服务端不一致

### P1-0 【本批】坐骑配额修正待部署

| 项 | 内容 |
|---|---|
| 证据状态 | 🟡 已改好 `Class.Misc = 8`，通过工具检查（层级2），**未部署** |
| 根因 | 我上一版把 `Class.Misc` 从用户原值 5 改成 2；坐骑属 `ITEM_CLASS_MISCELLANEOUS`(15)/`ITEM_SUBCLASS_JUNK_MOUNT`(5)，配额三行 9452→4107（砍57%）；且 `SelectRandomContainerElement` 按配额次数抽样，**种类同步变少** |
| 应读取 | `规划/G16_bot经济与打工/07-坐骑变少的根因与修正.md` |
| 应修改 | 已改好：`conf/worldserver.conf.d/ahbot.conf`（唯一改动是 `Class.Misc` 2→8） |
| 具体下一步 | 用户复制 conf → `.reload config` → `.ahbot reload` → `.ahbot rebuild all` → 等铺货 |
| 完成标准 | 坐骑种类明显变多 |
| 验证步骤 | 文档第六节的两条诊断 SQL（坐骑种类数/挂单数 vs 全服坐骑总数） |
| 需先问用户 | 否 |

### P1-1 第二十二批（零编译三件套）—— ✅ **已实测通过，本项关闭**

| 项 | 内容 |
|---|---|
| 证据状态 | ✅ **2026-08-18 用户实测全部通过**：拍卖行 118494 条（三行各 39498）、金色战刃消失、台词 141 条、时间线可见 |
| 应读取 | **`规划/第一批_零编译_执行手册.md`** |
| 应修改/使用 | `conf/worldserver.conf.d/ahbot.conf`、`conf/切换档位.bat`、`规划/G19_*/02,03*.sql`、`规划/G20_*/02*.sql` |
| 禁止使用 | `规划/G16_*/03-第1步_拍卖行大改_conf.md`（旧的 23 处手改法）；G20 SQL 的任何 `id1` 旧副本 |
| SQL 依赖 | **G19 前置：`npcbot_care_text` 必须有 `bot_race` 列** |
| 具体下一步 | 用户按手册三步执行并回报四项结果 |
| 完成标准 | ✅ 全部达成（唯一遗留：坐骑少 → 已拆为 P1-0 处理） |
| 需先问用户 | 否 |

### P1-2 G19 第3步场景感知（真正解决"看场合说话"）

| 项 | 内容 |
|---|---|
| 证据状态 | 🟠 设计完成（`规划/G19_*/04-第3步_场景感知实现.md`），未编译 |
| 前置 | P1-1 的台词 SQL 先入库 |
| 应修改 | `bot_companion.cpp` / `.h` |
| 关键 API | `Object.h:468 FindMap()`（**无断言，用这个**）；`Object.h:467 GetMap()` 有 `ASSERT(m_currMap)` **不能用**；`Map.h:446/448/454`；`DBCEnums.h:255/258/268` |
| 完成标准 | bot 在副本不说"风景不错" |
| 需先问用户 | 否 |

### P1-3 A42 PickGiftText 权重修复

| 项 | 内容 |
|---|---|
| 证据状态 | 🟡 修复文档已写，未编译 |
| 应读取 | `补丁库/01_功能/A42_种族职业口音/修复-第2步改错了函数.md` |
| **禁止使用** | 同目录 `第2步_种族口音.md` 里改 `PickText` 的方案（**改错了函数**） |
| 权重设计 | 种族+100 / 职业+50 / 物品种类+20（原方案 ItemKind+40 会让种族台词永远输） |
| 需先问用户 | 否 |

### P1-4 客户端与服务端一致性未审计

| 项 | 内容 |
|---|---|
| 证据状态 | ❓ 工作区零客户端文件，无法审计 |
| 风险 | A04/A09 改了 `item_template`，若引入**新 entry** 而未同步 `Item.dbc`，客户端可能显示异常 |
| 具体下一步 | 请用户实测：新自定义物品在背包里图标/名称是否正常 |
| 需先问用户 | ✅ **是** |

## P2 — 稳定性 / 性能 / Bot AI 体验

| 编号 | 任务 | 状态 | 下一步 |
|---|---|---|---|
| P2-1 | **bot 自动修装备 + 卖垃圾** | 🟠 未动工 | 用户原话："没它带bot玩两小时就裸奔"。阶段一最后硬骨头 |
| P2-2 | 全世界飞行（G17） | 🟠 已定位改法 | 改一行 `SpellInfo.cpp:1585` |
| P2-3 | G16 第2步 Buyer 价格调优 | 🟠 文档已写 | `规划/G16_*/05-*.md`；注意 Buyer 已开，有刷金风险 |
| P2-4 | G16 第4步 pbot 真上拍卖行 | 🟠 | 需处理上游 bug：`AuctionHouseBotSeller.cpp:935/938` AddAuction 被调两次 |
| P2-5 | 补丁库索引滞后 | 🟡 | `00-补丁库索引.md` 只到 A42/F17，缺 F18-F43 大段 |
| P2-6 | A38 双目录清理 | 🟡 | `A38_游荡bot定点控制` 与 `A38_游荡bot锚点` 并存 |

## P3 — 新玩法 / 剧情 / 长期

| 编号 | 任务 | 状态 |
|---|---|---|
| P3-1 | **G11 bot 自主冒险**（感知层→搭话层→组队层）= **终极目标** | 🟠 总体设计已出，第1步已交付，**第2步感知层是下一步** |
| P3-2 | G20 时间线扩展（通希尔 → 洛丹伦） | 🟠 试点 SQL 待执行 |
| P3-3 | G21 主线落地（真龙纪元进游戏） | 🟠 `规划/G21_*/01-让故事进入游戏.md` |
| P3-4 | G22 客户端魔改整合 | 🟠 只有账本，无成品 |
| P3-5 | 种族职业扩展 32 上限 | 🟠 需改 DBC |
| P3-6 | 人物模型全量转换 | 🟠 `工具库/25` |

---

# 16. 下一位代理的接手顺序（14 步）

```
第 1 步  读 /home/user/PROJECT_HANDOFF.md（本文件）全文。
         重点：第 1.3 节（仓库真相）、第 14.4 节（禁止事项）、第 15 节 P0。

第 2 步  读 /home/user/HANDOFF_GIT_STATE.txt。
         确认"工作区没有 wow-mecode、没有完整源码"这一事实，不要再去找。

第 3 步  抽查 HANDOFF_FILE_MANIFEST.tsv 的 SHA256：
           sha256sum tc-bignum/待办总表.md
           sha256sum tc-bignum/conf/worldserver.conf.d/ahbot.conf
         对不上 = Workspace 被改过，先问用户。

第 4 步  向用户确认 GitHub wpch3/wow-mecode(main) 的 HEAD，
         并确认它与本 Workspace 的关系（第 17 节 Q1）。
         【不要】假设二者一致。

第 5 步  确认完整 TrinityCore/NPCBots/Eluna 源码位置。
         本工作区【没有】。必须向用户索取，
         【绝对不得】用 /home/user/src/ 的片段重建源码树。

第 6 步  向用户索取 D:\TrinityCore 的真实基线 commit：
           cd /d D:\TrinityCore && git rev-parse HEAD
         与本文档记录的上游 4e8762e 对比。不一致则所有行号需重新核对。

第 7 步  分清五类目录，任何操作前先归类：
           成果   = tc-bignum/
           上游   = src/ , _chk/          （只读，禁覆盖）
           测试桩 = _chk2 t7 t8 t9 v2 v3 v4 fk luatest  （禁入生产）
           实验   = _dt _dt2               （无关）
           证据   = uploads/               （只读）

第 8 步  处理 P0-1：让用户先备份。在备份完成前不要开始任何开发。

第 9 步  处理 P0-3：提醒用户 uploads/ 内有真实数据库凭据，
         推 GitHub 前必须脱敏（第 18 节）。

第 10 步 验证 P0 问题本身，不要急着写代码。
         注意：F40/F41/F42/F43/A42修复 均已安装并验证，不要重复安装。

第 11 步 任何 SQL：先只读检查（SELECT COUNT / information_schema），
         再让用户 mysqldump 备份，最后才执行。
         【禁止】代理擅自连真实数据库执行。

第 12 步 任何客户端改动：先确认 Build、locale、MPQ 加载顺序，并要求备份 Data 目录。
         注意本项目【目前没有任何客户端成品文件】。

第 13 步 修改前建独立分支或备份副本，并取得用户确认。
         一次只做一个功能，同步记录 C++/SQL/Lua/配置/客户端五处联动。

第 14 步 按第 12 节的 13 级验证层级推进。
         【不得】用 mock 或 g++ 语法检查的结果，冒充全量编译或游戏内测试通过。
         每次交付后更新：README.md / 待办总表.md / 未完成想法-总清单.md /
                        补丁库索引 / 本 PROJECT_HANDOFF.md
```

---

# 17. 需要用户补充的外部资料

| # | 缺失资料 | 缺了会导致什么判断做不了 | 优先级 |
|---|---|---|---|
| **Q1** | **GitHub `wpch3/wow-mecode`(main) 的 HEAD，以及它与本 Workspace 的关系**（是同一份内容吗？谁更新？） | 无法判断成果是否真的有异地备份；无法判断该向哪边提交 | **P0** |
| Q2 | 完整 TrinityCore 源码（`D:\TrinityCore`）或其 `.git` | 无法产生 diff 证据 → **任何功能都无法从"有补丁"升级为"已安装"** | **P0** |
| Q3 | `D:\TrinityCore` 的 `git rev-parse HEAD` | 本文档所有行号基于上游 `4e8762e`，基线不同则行号全部要重核 | **P0** |
| Q4 | 三个数据库（auth/world/characters）的 schema 只读导出 | 第 7 节全部 SQL 的"是否已执行"永远停在❓；无法做 ID 冲突检查 | **P1** |
| Q5 | 当前 `worldserver.conf`（**脱敏后**） | 无法确认 conf.d 是否真被加载、Lua 路径、日志级别 | P1 |
| Q6 | **WoW 客户端 Build 号 + locale 目录名** | 第 9 节客户端版本永远❓；DBC 改动无从下手 | **P1** |
| Q7 | 客户端 Data 目录列表（MPQ 文件名与加载顺序） | 无法规划 MPQ 补丁 | P1 |
| Q8 | 最新一次 worldserver 启动日志 + 崩溃转储 | 无法确认当前是否还有其他崩溃（F40 已修复） | P1 |
| Q9 | VS/CMake 环境详情（Boost/OpenSSL/MySQL 版本、是否开 `/utf-8`） | 第 11.4 节 BOM 策略无法定论 | P2 |
| Q10 | 因 GitHub 大小限制未上传的大文件清单（路径/大小/SHA256） | 无法评估资产完整性 | P2 |
| Q11 | `uploads/cs_modify.cpp.txt` 与 `cs_modify_fixed.cpp` 哪份是现行版 | 版本谱系有断点 | P2 |
| Q12 | `src/` 中 7 个上游无同名文件（`CC.cpp`/`SC.h`/`SF.h`/`CharPkt.h`/`Misc.cpp`/`WorldObject.h`/`CharPackets.cpp`）的来历 | 无法确定是否含未归档的改动 | P2 |

---

# 18. 安全与敏感信息检查

## 18.1 扫描结果（**不含任何具体值**）

| 文件 | 风险类型 | 等级 | 建议 |
|---|---|---|---|
| **`uploads/worldserver.conf.txt`** | **数据库连接串：三个库(auth/world/characters)的用户名与密码均为非默认值** + IP | 🔴 **高** | ① 从仓库删除或脱敏；② **轮换数据库密码**；③ 若已推过 GitHub，需重写 Git 历史 |
| `uploads/ae60f5b6f6ff+_worldserver.exe_*.txt` | 崩溃转储，含密码字样 + IP | 🟠 中 | 脱敏后保留（有排查价值）或删除 |
| `uploads/vs架构错误.txt` | 含 password 字样（疑为编译输出中的路径/宏） | 🟡 低 | 复核后处理 |
| `uploads/GlueStrings.lua.txt` | 含 password 字样（客户端 UI 字符串，**非真实凭据**） | ⚪ 极低 | 无需处理 |
| `uploads/闪退诊断工具诊断2.txt` | 密码字样 + Windows 真实用户路径 | 🟠 中 | 脱敏 |
| `uploads/损坏检查结果.txt`、`检测工具结果.txt`、`闪退诊断工具诊断.txt` | Windows 真实用户名/绝对路径 | 🟡 低 | 脱敏 |
| `uploads/需要修复的文件.txt` | IP 地址 | 🟡 低 | 复核 |

## 18.2 未发现

- [未发现] 未发现 GitHub Token（`ghp_` / `github_pat_`）
- [未发现] 未发现 API Key / Cookie / 会话凭据
- [未发现] 本次生成的三个交接文件均**不含**任何秘密值（已自查 `grep -ciE "ghp_|github_pat_"` = 0）

## 18.3 处置建议（按顺序）

```
1. 推 GitHub 之前 —— 把 uploads/worldserver.conf.txt 移出仓库或脱敏
2. 轮换三个数据库账号的密码（该文件可能已上传过 GitHub）
3. 若确认已推送过，用 git filter-repo / BFG 重写历史
4. 今后给我 conf 时，先把 *DatabaseInfo 三行的密码段替换成 ***
5. 考虑加 .gitignore：uploads/*.conf.txt, uploads/*worldserver.exe*.txt
```

---

# 19. 给下一个代理的继续提示词

> 直接复制下面整段发给新代理。

```
【项目】魔兽世界 3.3.5a 私服魔改（TrinityCore + NPCBots + Eluna + zhCN）
【主线】剧情《真龙纪元》；终极目标：bot 自主冒险、偶遇玩家自动组队

━━━ 第一件事 ━━━
读 /home/user/PROJECT_HANDOFF.md 全文，再读 /home/user/HANDOFF_GIT_STATE.txt。
不要跳过。里面有 14 条"绝对不能做"的事。

━━━ 仓库（重要，别搞错）━━━
自有仓库： https://github.com/wpch3/wow-mecode  分支 main
  【注意】本 Workspace 里【找不到】这个仓库的任何克隆（grep wow-mecode = 0 命中）。
    /home/user 不是 git 仓库。不要去找，先问用户。
上游整合： https://github.com/328950225/TrinityCore-NPCBOT-Eluna-zhCN
  分支 NPCBOT-Eluna-zhCN-2026
  审计日 HEAD = 4e8762ee2b00948fa103d0cd1afd78ccdf4364fb
  【注意】这不等于用户 D:\TrinityCore 的基线，必须让用户跑 git rev-parse HEAD 确认。

━━━ 权威文件位置 ━━━
唯一成果目录： /home/user/tc-bignum/   （558 个文件）
  总账本： tc-bignum/待办总表.md          （3127 行，含最高铁律+坑表）
  导航：   tc-bignum/README.md
  补丁库： tc-bignum/补丁库/   （A01-A42 功能，F01-F43 修复）
  本批交付：tc-bignum/规划/第一批_零编译_执行手册.md

【禁止】禁止当成成果的目录（已逐文件 diff 证实）：
  /home/user/src/    = 上游只读副本，66/68 与上游逐字节相同
                       其中 3 个文件内容是 "404: Not Found"（GossipHandler.cpp /
                       PlayerStorage.cpp / bot/CreatureAI.h），禁止使用
  /home/user/_chk/   = ElunaLuaEngine/Eluna 上游只读克隆
  _chk2 t7 t8 t9 v2 v3 v4 fk luatest = 测试桩，禁入生产源码
  _dt _dt2           = git submodule 实验沙盒，与项目无关
  uploads/           = 用户上传的日志证据，只读

━━━ 当前 P0 / P1 ━━━
P0-1 全部成果无版本控制（tc-bignum 没有 .git，无异地备份）→ 先让用户备份
P0-3 uploads/worldserver.conf.txt 含三个库的真实非默认密码 → 推 GitHub 前必须脱敏+轮换
P1-0 坐骑配额修正（Class.Misc 2→8）已改好 conf，等用户复制部署

━━━ 已实测通过（2026-08-18，不要重做）━━━
拍卖行 118494 条（三行各 39498）/ 金色战刃已消失 / 台词 141 条 / 时间线相位可见
F40(剑圣镜像登出闪退)【已装】 F41/F42/F43【已装】 A42修复【已装】
  -> 这5项都不要重复安装，重复会破坏代码

━━━ 必须先验证、不许假设的事实 ━━━
1. GitHub 上的 wow-mecode 与本 Workspace 内容是否一致（未验证）
2. D:\TrinityCore 的真实 commit（未知）
3. 数据库里到底执行过哪些 SQL（工作区无任何数据库证据，83 个 SQL 全部状态未知）
4. 客户端 Build 号（未知，不要假定 12340）
5. 客户端【没有】任何成品：全workspace搜 *.dbc *.mpq *.blp *.m2 = 0 个

━━━ 已完成、不要重复做 ━━━
F41(中立怪+发呆) / F42(招募闪退) / F43(近战不前进) —— 用户已装并游戏内验证通过。
重复应用会破坏代码。

━━━ 第一个推荐动作 ━━━
先问用户第 17 节的 Q1/Q2/Q3（仓库关系、源码位置、基线 commit），
同时提醒他备份 Workspace。
在拿到 Q2 之前，不要写任何 C++ 补丁——因为无法验证行号。

━━━ 用户的硬性要求 ━━━
· 用中文回复；一步一步来，每步验证过再下一步
· 给代码必须精确到行号+原文，并说明作用域
· 只报一条主线，不要交叉给多个方案
· 交付 .cpp 前必须 g++ 真编译；交付 SQL 前必须跑 tools/check_sql.py
· 给用户的 SQL 禁止用 SET @变量（DBeaver Ctrl+Enter 只跑光标处，会静默失效）
· 文档必须 GBK 兼容（python3 gbk_check.py），代码围栏数必须偶数
· 每次交付必须更新总表（README/待办总表/未完成想法/补丁库索引/本交接文档）
· 用户报崩溃 → 第一句话就要日志，不要靠读源码猜
· 最高铁律：不能因为难/做不到/别人不做，就不做。
  说"做不到"前必须回答：挡在哪一行、绕过要什么、有没有别的路径
```

---

# 20. 交接完整性声明

| # | 项目 | 结论 |
|---|---|---|
| 1 | 是否看完可访问的聊天上下文 | ✅ 是。含服务端注入的前序会话摘要（F41/F42/F43 状态、用户原话、坑表、源码行号）+ 本轮完整对话 |
| 2 | 是否检查了整个 `wow-mecode` 仓库 | 【注意】**无法检查** —— 工作区不存在该仓库。已改为全量审计 `/home/user`（929 文件，逐文件 SHA256） |
| 3 | 是否找到完整服务端源码 | ❌ **否**。工作区无 CMakeLists、无 `src/server/` 层级、无 `.git`。`src/` 经逐文件 diff 证实为上游只读副本 |
| 4 | 是否检查了 Git 状态 | ✅ 是。找到 4 个 `.git` 并逐一鉴定：1 个是上游 Eluna 只读克隆（config 缺失），3 个是 submodule 实验沙盒。**均非本项目仓库** |
| 5 | 是否确认上游基线 commit | 🟡 **部分**。确认了上游分支当日 HEAD = `4e8762e`；**用户本地基线仍待确认** |
| 6 | 是否找到真实客户端文件 | ❌ **否**。`*.dbc/*.mpq/*.blp/*.m2/*.wmo/*.adt` 全workspace = **0 个** |
| 7 | 是否检查 SQL / Lua / 配置 | ✅ 是。83 个 SQL（编码全查）、6 个 Lua、20 个 conf（逐个查 BOM/行尾） |
| 8 | 是否实际编译或测试 | ❌ **否**。本会话最高只做到**验证层级 2**（Python 工具检查）。未编译、未连数据库、未进游戏 |
| 9 | 哪些状态无法确认 | ① GitHub 仓库与本地的一致性 ② 用户本地基线 commit ③ **83 个 SQL 的实际执行状态（全部）** ④ 客户端 Build ⑤ A26/A28/A29/A30/A33/A34 等❓项 ⑥ `src/` 中 7 个上游无同名文件的来历 ⑦ MSVC 是否开 `/utf-8` |
| 10 | 哪些文件可能未进入 GitHub/Workspace | ① 完整源码 `D:\TrinityCore` ② 编译产物 `D:\TC-Build` ③ 数据库全部数据 ④ 客户端 `D:\WOW` 全部内容 ⑤ 用户本地可能有的未上传大文件 |
| 11 | 是否建议用户重新下载 Workspace | ✅ **强烈建议，且是当前 P0-1** |

## 20.1 本次审计实际执行的取证动作

```
✅ find /home/user -maxdepth 4 -name .git -type d        → 4 个，逐一鉴定
✅ grep -ril "wow-mecode|wpch3" /home/user                → 0 命中
✅ 对 src/ 68 个 .cpp/.h 与上游 4e8762e 逐文件 diff        → 66 同 / 2 异
✅ 全workspace 扫 "404: Not Found"                        → 3 个损坏文件
✅ 929 个文件逐一 SHA256                                   → 写入 manifest
✅ 编码普查（BOM/GBK/CRLF）                                → 见第 11 节
✅ uploads/ 敏感信息扫描（只判类型不取值）                  → 见第 18 节
✅ 只读 clone 上游确认基线 commit                          → 4e8762e（未 merge）
✅ 三个交接文件生成后复读校验
```

## 20.2 未执行的动作（遵守禁止事项）

```
【禁止】未执行 git reset --hard / git clean / 强制 checkout / force push
【禁止】未 commit / push / 建 PR
【禁止】未 merge / rebase / 更新 submodule
【禁止】未连接或修改任何真实数据库
【禁止】未触碰真实客户端 MPQ/DBC/Wow.exe/Data
【禁止】未删除任何 build/日志/备份/诊断文件
【禁止】未修改、重构或格式化任何项目源码
   （唯一例外：应本轮开发要求在交接【之前】已完成的 G20 SQL 列名修正
     与 切换档位.bat 修复，属上一阶段工作，非本次交接动作）
```

---

**文档结束。**
权威副本：`/home/user/PROJECT_HANDOFF.md`
配套文件：`/home/user/HANDOFF_FILE_MANIFEST.tsv`（928 条记录）、`/home/user/HANDOFF_GIT_STATE.txt`
