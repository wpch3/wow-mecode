# G17-B0 Windows真实源码Apply验收

日期：2026-08-22（Asia/Shanghai）

## 用户原件

- 用户附件：`G17B0_SOURCE_APPLY_RESULT.txt`
- 本地原样归档：`tc-bignum/规划/G17_飞行与移动/证据/G17B0_SOURCE_APPLY_RESULT_20260822.txt`
- 大小：1668字节
- SHA-256：`772f4007cd2199ec6829bbb2bdef3e11a976c072c2c1ad7ae6b31210d3bc5ca4`
- 编码/换行：ASCII / pure CRLF
- `splitlines()`：30行

离线验收器：`tc-bignum/规划/G17_飞行与移动/accept_g17b0_source_apply.py`。它锁定原件大小/哈希、行数、PRE→POST、2项改动、7项上下文、备份、wrapper和所有成功标志。

## Apply前状态

```text
SOURCE_ROOT=D:\TrinityCore
G17B0_SOURCE_AND_DB_APPROVED=True
loader state=ready
loader sha256=2a4895a32532f3c6c2c6dc3096fced4bff6d53c39dd3787bd81a76653d42f3f7
target state=ready sha256=ABSENT
locked_context_files=7
G17B0_SOURCE_STATE=READY_TO_APPLY
G17B0_CHECK_SOURCE_EDITS=0
```

这与已验收的源码预检一致，没有跳过PRE门槛。

## 真实写入与备份

```text
G17B0_APPLY_CHANGED_FILES=2
G17B0_SOURCE_APPLY_CHANGED_FILES=2
loader post sha256=5502e5b4e22535957f3db81083530b048ec33f6852f4697fbe55795628cee5cc
payload sha256=c9535dca3390ece6735e6ff6b7418ed99ff206628b5e8febd7b78b05cba999bd
loader backup=D:\TrinityCore\src\server\scripts\Commands\cs_script_loader.cpp.before_g17b0.bak
```

只改变锁定的loader和新`cs_dragonriding.cpp`目标；7个上下文文件写后仍匹配。

## 写后状态

```text
loader state=applied
target state=applied
G17B0_SOURCE_STATE=ALREADY_APPLIED
G17B0_CHECK_SOURCE_EDITS=0
G17B0_FINAL_SOURCE_STATE=ALREADY_APPLIED
G17B0_SOURCE_APPLY_PASS=True
G17B0_SOURCE_APPLY_WRAPPER_PASS=True
```

全文没有`[FAIL]`或`=False`。

## 验收结论与边界

```text
G17B0_WINDOWS_SOURCE_APPLY_ACCEPTANCE=PASS
G17B0_WINDOWS_SOURCE_APPLY=PASS
G17B0_WORLD_INSTALL=NOT_RUN
G17B0_WINDOWS_BUILD=NOT_RUN
G17B0_RUNTIME=NOT_RUN
```

不得要求用户重跑Source Apply。`ALREADY_APPLIED`是写后精确状态，不表示本轮没有写入；本轮首次真实改动明确为2项。下一阶段只允许受控world install/check，不编译、不做Runtime，除非world后像再验收通过。
