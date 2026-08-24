G17-B0 world受控安装v3（1267跨collation修复版，2026-08-22）

为什么替换：
- 旧v2的01已在真实MySQL 8返回1267：utf8mb4_unicode_ci与utf8mb4_0900_ai_ci的IMPLICIT“=”比较冲突。
- 旧v2禁止重跑，也不要运行旧02。
- v3的01直接写明 USE `world`;，不再依赖DBeaver当前活动数据库。
- v3把连接、文本变量和ScriptName/name比较全部显式锁为utf8mb4_unicode_ci。
- v3的02仍是单语句只读SELECT；所有表都写为`world`.`表名`，无需选择数据库。

先处理旧错误：
1. 回到刚才运行旧01的同一DBeaver连接。
2. 单独执行：ROLLBACK;
3. 不运行旧包02，不运行任何99回滚，不编译。

然后运行v3：
1. 打开 01_G17B0_world_install_v3_collation_safe.sql。
2. 使用“执行SQL脚本/执行整个文件”，不要只执行光标所在单句。
3. 01开头已经包含 USE `world`; 和 SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;。
4. 脚本最后唯一结果表必须为：G17B0_WORLD_INSTALL_PASS。
5. 如果出现任何SQL错误、BLOCKED_*或FAILED_*：立即停止；若在COMMIT前出错，在同一连接执行ROLLBACK;；不要继续02或编译。
6. 只有01 PASS后，完整执行 02_G17B0_world_postcheck_v3_readonly_collation_safe.sql。
7. 02唯一结果必须为：G17B0_WORLD_CHECK_PASS。
8. 将02结果表导出为UTF-8文本：
   C:\Users\Administrator\Downloads\workspace\uploads\G17B0_WORLD_POSTCHECK_RESULT.txt
9. 上传这个txt。

固定映射：
- source 27756 / VehicleId 70 / model 25854
- target 1000171
- 动作条 9573 / 55215 / 52197 / 53208
- movement Flight=1 / Rooted=0

本轮禁止：
- 禁止重跑v2、Source Apply、源码预检或DB探针；
- 禁止手工修改world表；
- 禁止运行99回滚；
- 禁止reload、编译或启动游戏验收。
