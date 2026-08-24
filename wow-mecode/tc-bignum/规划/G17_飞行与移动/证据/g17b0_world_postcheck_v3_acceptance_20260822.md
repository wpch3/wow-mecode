# G17-B0 world v3真实写后检查验收

日期：2026-08-22（Asia/Shanghai）  
用户文件：`_G17_B0_world安装后只读检查v3_显式world表_跨collation安全_单语句_单结果表_零写入_直接完整执行_202608221520.txt`  
证据副本：`G17B0_WORLD_POSTCHECK_RESULT_20260822_1520.txt`

## 不可变身份

- 字节数：492
- SHA-256：`182a1878251ea7d4cb6af76a4721028a57750f7de47acb1eff6793a31eaef32a`
- UTF-8 pipe表：1个表头、1个分隔行、1个数据行
- 验收器：`accept_g17b0_world_postcheck_v3.py`

## 精确结果

```text
G17B0_RESULT=G17B0_WORLD_CHECK_PASS
database_name=world
source_rows=1
source_vehicle70_rows=1
target_rows=1
target_exact=1
action_rows=4
action_exact=4
movement_exact=1
script_rows=4
script_exact=4
```

没有`BLOCKED_*`、`FAILED_*`或SQL错误。

## 结论与边界

```text
G17B0_WORLD_INSTALL_V3=PASS_BY_READONLY_POSTCHECK
G17B0_WORLD_POSTCHECK_V3_ACCEPTANCE=PASS
G17B0_WINDOWS_BUILD=NOT_RUN
G17B0_RUNTIME=NOT_RUN
```

这正式证明world写后数据与固定合同一致；不证明worldserver已加载这些数据，不证明C++已编译/链接，也不证明游戏内座位、动作条、技能施放及生命周期清理已通过。下一阶段允许进入完整Windows构建，但不得提前声明Build或Runtime PASS。
