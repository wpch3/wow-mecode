# G23P2 Lua稳定性与安全门

唯一操作文档：`安装说明.md`

执行顺序：

```text
G23P2_Check.cmd
正常停服
HeidiSQL执行 sql\G23P2_daily_reward_atomic.sql
G23P2_Apply.cmd
正常启动worldserver
```

无需编译，不执行`.reload eluna`。本包与F45 C++完全独立。回滚使用`G23P2_Rollback.cmd`；为防止重复奖励，SQL审计数据默认保留。
