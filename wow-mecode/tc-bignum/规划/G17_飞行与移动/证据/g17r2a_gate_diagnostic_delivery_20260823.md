# G17-R2A客户端/服务端飞行门只读诊断交付（2026-08-23）

## 包

`tc-bignum/规划/G17_飞行与移动/G17R2A_Flight_Gate_Diagnostic_Windows_20260823.zip`

```text
SIZE=447380
SHA256=2bc732cf4d17338a0e07af7c798f7868575197940630bbdca394a53b4d8381fa
ZIP_FILES=8
ZIP_CRC=PASS
ZIP_MANIFEST_BAD=0
ZIP_UNLISTED=0
ZIP_STALE=0
EXTRACTED_STATIC_TEST=PASS
READ_ONLY_CONTRACT=PASS
MPQCLI_HASH=PASS
POWERSHELL_7_6_5_PARSE_ERRORS=0
POWERSHELL_SCRIPT_ASCII=True
```

内置官方mpqcli v0.10.2 Windows x64程序，SHA-256=`5dc56b13...a79f`，并携带MIT许可证和来源说明。

## 只读范围

- 读取`G17R2_WINDOWS_FIX_RESULT.txt`与活动worldserver.exe；
- 读取`SpellInfo.cpp`、`Spell.cpp`的SHA，不写源码；
- 读取WorldFlight配置和最近日志；
- 查找59961的R2位置门日志；
- 读取客户端安装report/state；
- 对根Data `patch-Z`到`patch-A`做直接目标抽取探针，不依赖listfile；
- 判断最高优先级根Data `Spell.dbc`是否为锁定patched SHA；
- 只在uploads建立临时目录，结束时清除。

不执行SQL、构建、启动/停止进程，不写`D:\TrinityCore`、`D:\TC-Build`或`D:\WOW`。

## 输出

`C:\Users\Administrator\Downloads\workspace\uploads\G17R2A_GATE_DIAGNOSTIC_RESULT.txt`

关键分类将区分客户端DBC未安装/被覆盖、服务器R2位置门已通过、客户端补丁存在但日志未命中、以及高优先级槽不可安全判定。报告返回前R3保持未创建。
