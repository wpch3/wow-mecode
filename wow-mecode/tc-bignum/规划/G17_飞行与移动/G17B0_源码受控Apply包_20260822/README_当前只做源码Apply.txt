G17-B0 受控源码Apply包（2026-08-22）

前置事实：
- Windows真实源码SelfTest/只读Check已经PASS，禁止重复旧预检CMD。
- live world只读探针v2已经PASS：world / MySQL 8.0.46 / G17B0_DB_PREIMAGE_READY。
- target entry 1000171为空，source 27756是VehicleId 70，候选ScriptName绑定为0。
- 本包因此含审批文件G17B0_APPLY_APPROVED.txt；内容固定为G17B0_SOURCE_AND_DB_APPROVED。

本轮唯一操作：
1. 解压整个包，保持目录结构；
2. 双击 Run-G17B0-Source-Apply.cmd；
3. 必须看到：
   G17B0_SOURCE_APPLY_PASS=True
   G17B0_FINAL_SOURCE_STATE=ALREADY_APPLIED
   G17B0_SOURCE_APPLY_WRAPPER_PASS=True
4. 上传：
   C:\Users\Administrator\Downloads\workspace\uploads\G17B0_SOURCE_APPLY_RESULT.txt

本包只修改D:\TrinityCore中的两个源码路径：
- src\server\scripts\Commands\cs_script_loader.cpp
- src\server\scripts\Commands\cs_dragonriding.cpp

安装器会：
- 再次验证loader PRE/POST、目标cpp缺席/已应用分类和7个上下文哈希；
- 在写入前备份loader为cs_script_loader.cpp.before_g17b0.bak；
- 原子写入两个目标；
- 写后精确检查POST/payload哈希和7个上下文不变；
- 已应用时幂等返回，不重复插入loader行。

本轮不要做：
- 不运行旧Run-G17B0-Source-Preflight.cmd；
- 不手工复制或编辑cpp/loader；
- 不导入任何G17B0 world安装SQL；
- 不编译；
- 不启动游戏验收；
- 不自行运行Rollback，除非后续明确要求。

如果窗口FAIL，只上传结果txt并停止。
