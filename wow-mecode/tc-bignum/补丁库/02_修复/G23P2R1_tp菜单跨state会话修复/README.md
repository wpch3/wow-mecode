# G23-P2R1 `.tp`菜单跨state会话修复

独立热修，只替换运行目录的`lua_scripts/custom_teleport.lua`。

- 前像SHA-256：`9578875f6f9aebb3e50dee1fa9947166360799a4af56c145489d722a29c73b95`
- 后像SHA-256：`b84e7c1da66d45c781564917a06cc87b92c693e41964305bdab42068c42a0a23`
- 目标：`D:\TC-Build\bin\RelWithDebInfo`
- SQL：不需要
- 编译：不需要
- `.reload eluna`：禁止

安装顺序：`G23P2R1_Check.cmd` → 正常停服 → `G23P2R1_Apply.cmd` → 正常启动。

完整根因、验收和回滚见`安装说明.md`。
