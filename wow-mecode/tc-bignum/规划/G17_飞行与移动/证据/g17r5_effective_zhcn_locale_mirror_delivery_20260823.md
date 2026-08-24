# G17-R5 有效 zhCN locale DBC 镜像交付证据（2026-08-23）

## 真实 R4 结果

- 用户真实结果已归档：`G17R4_CLIENT_MPQ_UPGRADE_RESULT_20260823.txt`
- 归档 SHA-256：`fd0517d74caa786d64432ea984570054faddd03e18e4b2162202862e10f6e118`
- `G17R4_CLIENT_MPQ_UPGRADE_RESULT=PASS`
- 根槽：`D:\WOW\Data\patch-Z.MPQ`
- MPQ v2、4 个文件，内部 Spell/Area 哈希正确；Cache 已删除；服务端 DBC 未改；已知根/locale DBC 碰撞为 0。
- 但真实游戏内湿地 `IsFlyableArea()=nil`，因此 `G17R4_REAL_WINDOWS_CLIENT_RUNTIME=FAIL_API_STILL_NIL`。

## 结论收紧

R3 已向湿地加入主要飞行位 `0x00000400`，R4 又加入 `0x00004000`。R4 payload 中湿地父区及全部直接子区共 26 行都已包含 `0x400`：

- 湿地 Area 11：`0x00004440`
- 米奈希尔港 Area 150：`0x40204440`
- 米奈希尔海湾 Area 299：`0x40004440`
- 米奈希尔城堡 Area 2103：`0x40004440`
- 其它直接子区同样包含 `0x00004400`

若客户端实际采用 R3/R4 AreaTable，湿地户外 API 不应在补齐两位后仍为 nil。与此同时，根 MPQ 中的 R1 Spell 窄补丁也未让普通 59961 按钮生效。这两条独立表现共同把主嫌疑从“继续缺 Flag”收紧为“根 Data 自定义 MPQ 不是 zhCN 客户端最后实际采用的 DBC 来源”。

R4 真实报告还确认 `D:\WOW\Data\zhCN\patch-zhCN-Z.MPQ` 是目录，不是封装 MPQ；目录内没有 Spell/Area。R5 不删除或覆盖它。

## R5 修复边界

R5 只做以下事情：

1. 只读验证 R4 状态、根 `patch-Z.MPQ` 整体哈希、MPQ v2/4 文件及内部 Spell/Area 哈希；
2. 要求 `Data\zhCN\patch-zhCN-Y.MPQ` 前像不存在；
3. 扫描其它 zhCN 字母槽，发现 Spell/Area 碰撞即拒绝；
4. 将已经验证的根 R4 MPQ 逐字节镜像为有效封装的 `patch-zhCN-Y.MPQ`；
5. 再次解包核验并清除 Cache；
6. 回滚只在目标哈希仍匹配时移走 R5 自有 Y 槽，保留 rescue 副本。

不改服务端、数据库、根 R4 MPQ、目录型 Z 槽；不新增 Flag；不做 Spell Effect/Aura 伪装。

## 自动验证

- PowerShell 7.6.5 AST：安装/回滚 2/2 PASS
- 生命周期 fixture：R4 根 MPQ + zhCN Z 目录 + 无 DBC 的 zhCN M 封装 MPQ
- 安装：PASS
- Y 与根 R4 MPQ 逐字节相同：PASS
- Y MPQ v2/4 文件及内部 Spell/Area 哈希：PASS
- 幂等：`ALREADY_CURRENT` PASS
- 回滚到 Y 前像不存在：PASS
- 根 R4 MPQ 和 Z 目录在安装/回滚后均保留：PASS
- 已有 X 槽包含 Spell/Area 时：退出 1、拒绝安装、未创建 Y：PASS
- 包自检：PASS
- 12 项 `SHA256SUMS.txt`：PASS
- ZIP CRC、解包后 SHA 清单与包自检：PASS

## 最终包

```text
file=G17R5_Effective_zhCN_Locale_DBC_Mirror_Windows_20260823.zip
size=450841
files=13
sha256=015a9dc27ae67d373a09813edb0121482f615fc2f91d805e2712f0cf03bde333
```

入口：`01_Install_G17R5_Locale_Mirror.cmd`

## 状态

```text
G17R4_CLIENT_MPQ_UPGRADE_RESULT=PASS
G17R4_REAL_WINDOWS_CLIENT_RUNTIME=FAIL_API_STILL_NIL
G17R4_ACTUAL_DBC_LOAD_PROVEN=False
G17R5_OFFLINE_VALIDATION=PASS
G17R5_REAL_WINDOWS_RUNTIME=PENDING_USER
G17B1_ALL_MOUNTS_GROUND_MOUNTS=BLOCKED_PENDING_R5_RUNTIME
```
