G17-B0窄范围只读锁定探针

1. 双击 Run-G17B0-Narrow-Probe.cmd。
2. 看到 G17B0_PROBE=PASS。
3. 回传：C:\Users\Administrator\Downloads\workspace\uploads\G17B0_LOCK_RESULT_时间.zip

只读：不会修改D:\TrinityCore，不会编译，不会查询或记录数据库密码。
这是为了保留当前已自定义的cs_script_loader.cpp/ScriptLoader.h原件；若cs_dragonriding.cpp已存在也会只读收集其前像，禁止整文件覆盖。
