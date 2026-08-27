# G17-C9 v3 安装器修复交付记录（2026-08-27）

## 触发证据（用户真实运行 FAIL）

用户双击 v1/v2 包 `01_Install_G17C9.cmd` 后控制台与结果文件输出：

```text
G17C9_CLIENT_VISUALS_START
C6_BUILD=v1_real_visuals_no_dbc_cd
G17C9_CLIENT_VISUALS_ERROR=OBSOLETE_PACKAGE: patcher is not v1_real_visuals_no_dbc_cd
G17C9_CLIENT_VISUALS_RESULT=FAIL
RESULT_FILE=C:\Users\Administrator\Downloads\workspace\uploads\G17C9_CLIENT_VISUALS_RESULT.txt
```

## 根因（逐行对照包内文件得出，非猜测）

`Install-G17C9-Real-Visuals.ps1`（v1/v2 版）第 98 行：

```powershell
if ($patcherText -notmatch 'G17B3R5_VISUAL_PATCHER_VERSION\s*=\s*"v1_real_visuals_no_dbc_cd"') {
    throw ("OBSOLETE_PACKAGE: patcher is not v1_real_visuals_no_dbc_cd")
}
```

而 C9 补丁器 `tools/patch_g17c9.py` 定义的变量是 `G17C9_VERSION`。该正则来自 C6 安装器模板
（C6 补丁器变量名才是 `G17B3R5_VISUAL_PATCHER_VERSION`），克隆到 C9 时未改——**该门在任何机器上永远不可能通过**。

完整审计还发现另外四个必然失败的缺陷：

| # | 缺陷 | 位置 |
|---|---|---|
| 1 | 版本门 grep 错误变量名（用户撞到的） | PS1 L98 |
| 2 | 输入门硬编码要求客户端 Spell.dbc == C3 镜像 `006a892b`；用户实际处于 C8 状态 | PS1 `$ExpectedSpellHash` + 根 DBC 检查 |
| 3 | 输出门硬编码要求补丁输出 == C6 镜像 `5db5b7a5`；C9 输出按设计必然不同 | PS1 `$ExpectedPatchedSpellHash` + `Assert-NewArchive` |
| 4 | C3 状态环境模式要求根 MPQ 哈希 == C3 时代 `NEW_MPQ_SHA256`；C6/C7/C8 安装早已重写根 MPQ | PS1 ENV_MODE=C3_STATE 分支 |
| 5 | `Rollback-G17C9-Real-Visuals.ps1` 为 0 字节空文件 | 包内 |

结论：v1/v2 包从未具备可安装性；交付前缺少"安装器门仿真"自测（B3R4c 在服务端安装器上已立过同类测试，客户端包漏配）。

## 修复（v3 包）

- 新包目录：`G17C9_真实特效_客户端/G17C9v3_Real_Visuals_Fix_Client_20260827/`
- 交付 ZIP：仓库根 `G17C9_FINAL.zip`（平铺结构，459833 字节，SHA256 `2079be3f2b9626a6421d0e7efff94ab59de07f4872c0ef83290cfd1051adf4fb`）
- DBC 负载与 v2 完全一致（25 个 Wowhead 逐条验证视觉 + Effect/Target/Range + RecoveryTime/Category=0）；补丁器仅版本指纹升为 `v3_wowhead_visuals_no_dbc_cd`
- 安装器按**用户机器上真实跑通过的 C8 安装器流程**重写：
  - 状态文件（C9→C8→C6→C3 顺序）只提供 ROOT_MPQ/LOCALE_MPQ **路径**，不再钉哈希
  - 输入状态由补丁器 `check` 自行判定（C3/C6/C7/C8 任一态可直接升级；COMPLETE 则幂等 PASS）
  - 输出验证改为**内容验证**（补丁器 check 必须 `G17C9_STATE=COMPLETE`）+ 打包后回读哈希一致
  - AreaTable 哈希/大小 + Spell.dbc 大小 + zhCN locale + mpqcli 哈希门保留
  - 真正的回滚脚本（从 `uploads\G17C9_Client_Backup_*` 恢复根+locale MPQ 并清 Cache）
- 旧目录改名 `G17C9_v1v2_已废弃_安装器五缺陷_20260826/` 并放置 DEFECT_NOTE.txt，禁止执行

## 包自检（沙箱实测）

`python tools/test_g17c9_package.py` → **28/28 PASS**：

- T1 补丁器功能测试（合成 DBC）：FRESH→patch→逐字段核对 25×7 契约（视觉按 Wowhead 表、Effect=2、Base=0、TargetA=18、Range=4、Recovery=Category=0）、布局/字符串块保持、无关记录不动、幂等 ALREADY_COMPLETE、C8 样输入可升级、PARTIAL 拒绝且零写入（13 项）
- T2 **安装器门仿真**：PS1 内所有对补丁器的 `-notmatch/-match` 正则逐条用真实补丁文本求值（本测试若存在于 v1/v2 交付时即会拦截该 bug）；指纹一致性；OBSOLETE 门通过（5 项）
- T3 期望哈希对照（mpqcli 实测一致；无 C3/C6 镜像硬编码门；无根 MPQ 哈希钉死）（4 项）
- T4 PS1 静态语法（2 个 PS1）（1 项）
- T5 陈旧 token 扫描（无 G17B3R5_VISUAL_PATCHER_VERSION/.g17c6/C6_BUILD 等）（2 项）
- T6 回滚脚本非空且含完整恢复逻辑（1 项）
- T7 SHA256SUMS 全覆盖且正确（1 项）

## 用户操作

关闭 WoW 客户端 → 双击新 `G17C9_FINAL.zip` 解压 → 双击 `01_Install_G17C9.cmd` →
期待 `G17C9_CLIENT_VISUALS_RESULT=PASS`（结果文件 `uploads\G17C9_CLIENT_VISUALS_RESULT.txt`）→ 重启客户端按 README 四点验收。
