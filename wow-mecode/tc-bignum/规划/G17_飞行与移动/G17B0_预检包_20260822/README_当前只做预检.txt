G17-B0当前只做预检，不Apply、不导入安装SQL

1. 双击 Run-G17B0-Source-Preflight.cmd（自动完成SelfTest+源码Check）
2. 它自动生成：C:\Users\Administrator\Downloads\workspace\uploads\G17B0_SOURCE_PREFLIGHT_RESULT.txt
3. DBeaver选中真实world库，执行 sql\G17B0_world_probe_readonly.sql（单语句、单结果表）
4. 把结果表导出为UTF-8：G17B0_DB_PROBE_RESULT.txt
5. 将上述两个txt上传GitHub后告诉我文件名

期望：
G17B0_INSTALLER_SELF_TEST_PASS=True
G17B0_SOURCE_STATE=READY_TO_APPLY
G17B0_CHECK_SOURCE_EDITS=0
G17B0_DB_PREIMAGE_READY

Apply当前有硬锁；数据库结果审完前不要运行G17B0_Apply.cmd。
不要复制草案cpp，不要修改loader，不要导入G17B0_world_install.sql。
