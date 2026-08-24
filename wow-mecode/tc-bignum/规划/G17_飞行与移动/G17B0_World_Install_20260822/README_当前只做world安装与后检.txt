G17-B0 world受控安装与后检（2026-08-22）

前置均已真实通过：
- Windows源码Apply：2个目标写入、loader备份、POST/payload/7上下文检查PASS；禁止重跑。
- live DB v2：world / MySQL 8.0.46 / source 27756 VehicleId70 / target 1000171空 / PREIMAGE READY。

本轮顺序：
1. DBeaver保持活动数据库为world。
2. 打开 01_G17B0_world_install_v2_locked.sql。
3. 使用“执行SQL脚本/执行整个文件”，不要只执行光标所在单句。
4. 脚本最后唯一结果表必须为：G17B0_WORLD_INSTALL_PASS。
5. 如果是任何BLOCKED_*、FAILED_*或SQL错误：立即停止；若错误发生在COMMIT前，在同一连接执行 ROLLBACK;；不要继续后检/编译。
6. 只有01为PASS后，完整执行 02_G17B0_world_postcheck_v2_readonly.sql。
7. 02唯一结果必须为：G17B0_WORLD_CHECK_PASS。
8. 将02结果表导出为UTF-8文本：
   C:\Users\Administrator\Downloads\workspace\uploads\G17B0_WORLD_POSTCHECK_RESULT.txt
9. 上传这个txt。

01安全边界：
- 真实前像门：source必须唯一且VehicleId=70；外来target或外来自定义ScriptName绑定立即阻断。
- target 1000171只允许空状态或本批ScriptName拥有状态。
- 永久写入在START TRANSACTION/COMMIT内。
- 所有永久INSERT/UPDATE/DELETE均受preimage gate保护；子表还受owned gate保护。
- 精确创建4格动作条：9573 / 55215 / 52197 / 53208。
- 精确创建Flight=1、Rooted=0移动和4条ScriptName绑定。
- 最后在COMMIT后执行完整后像计数。

02是零写入、单语句、单结果表的独立后检。
99是owned-only回滚，仅在后续明确要求时使用；本轮不要执行。

本轮不要：
- 不重跑源码Apply、源码预检或DB探针；
- 不手工修改world表；
- 不运行99回滚；
- 不reload；
- 不编译；
- 不启动游戏验收。
