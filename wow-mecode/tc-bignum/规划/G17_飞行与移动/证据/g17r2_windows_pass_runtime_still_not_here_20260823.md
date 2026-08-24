# G17-R2 Windows PASS但59961仍“无法在这里使用”（2026-08-23）

## 用户真实回报

```text
G17R2_WINDOWS_BUILD_RESULT=PASS
59961湿地=仍无法召唤
当前直接提示：无法在这里使用
```

用户确认了构建结果行，但本轮未附`G17R2_WINDOWS_FIX_RESULT.txt`原始全文，因此可以记录Windows包装器结果PASS，不能补造新的EXE SHA、OBJ路径或完整日志。

## 当前精确状态

```text
G17R2_WINDOWS_BUILD_RESULT=PASS_USER_CONFIRMED
G17R2_59961_WETLANDS_RUNTIME=FAIL_NOT_HERE
G17R2_59961_SERVER_LOCATION_MARKER=UNKNOWN_NOT_REPORTED
G17R1_CLIENT_MPQ_INSTALL_RESULT=UNKNOWN_NOT_PROVIDED
```

提示文字已经变化，不能直接断言仍是同一个`SPELL_FAILED_INCORRECT_AREA`。现在存在三个必须自动分流的门：

1. 客户端是否实际加载R1 patched Spell.dbc；
2. 59961是否送达服务器并命中`G17R2 old-world pure-flight location allowed`日志；
3. 若前两项都PASS，才检查`Spell.cpp`中Aura级飞行区域门等后续服务端路径。

指定上游`Spell.cpp`审计发现，`SPELL_AURA_FLY`/`SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED`还有独立的`SPELL_FAILED_NOT_HERE`返回路径，条件是当前Area带`AREA_FLAG_NO_FLY_ZONE`或Battlefield拒飞。但湿地是否实际进入这条路径必须由R2日志和真实配置证明，不能只按中文提示猜测。

## 下一步

交付只读一键诊断：

`G17R2A_Flight_Gate_Diagnostic_Windows_20260823.zip`

它不改源码、客户端、数据库或服务器，只读取R2结果/活动EXE/源码SHA/配置/日志和客户端MPQ优先级，并直接抽取最高根Data`Spell.dbc`校验。输出：

`C:\Users\Administrator\Downloads\workspace\uploads\G17R2A_GATE_DIAGNOSTIC_RESULT.txt`

在该报告返回前不制作R3，不重复R1/R2或客户端安装包。
