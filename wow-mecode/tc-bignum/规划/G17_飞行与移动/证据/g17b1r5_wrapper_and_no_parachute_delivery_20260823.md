# G17-B1R5 包装坐骑与无降落伞室内清理交付记录

日期：2026-08-23

## 用户已确认前态

- B1R3地面/飞行普通坐骑接管、起飞、降落：PASS。
- B1R4真实室内自动退出：PASS；运行行为证明新二进制已部署。
- B1R4室内退出附加降落伞：用户拒绝。
- 军马、始祖龙：接管PASS；爱情火箭、无头骑士坐骑：接管FAIL。

## 修复

- 真实Spell.dbc和服务端`spell_gen_mount`/`SpellMgr`证明外层/内层包装链；精确记录见B1R5批次证据。
- Runtime不硬编码两个坐骑名称或ID。候选门读取保留的Mounted Aura metadata，所有权锚定已学习外层，真实模型/creature取自活动内层Mounted Aura。
- VehicleAI 250ms和zone防御清理都不再施放降落伞；隐式unboard也不再添加。
- 清理恢复Vehicle飞行速率、can-fly、gravity，退出Vehicle并重算玩家移动速度。
- 高空边界只使用有界、无Aura/模型/缓降/瞬移/重力修改的fall-damage accounting guard。

## 离线结果

```text
SOURCE_PRE_SHA256=e9418704731a2d9cd5119cc2024079a2326802796d00bf24e88928dd17ea7059
SOURCE_POST_SHA256=35af002b09b5d8112bbc1aaa1750f4a6245adec8b7c91a7852d69bdd283668b8
SPELL_DBC_SHA256=dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea
DBC_CHAIN_PARSE=PASS_11_OF_11
GCC14_CPP20_WARNINGS=PASS_0_DIAGNOSTICS
SOURCE_TESTS=PASS_18_OF_18
SOURCE_LIFECYCLE=PASS
POWERSHELL_AST=PASS_2_OF_2
NATIVE_RUNNER=PASS
PACKAGE_SELFTEST=PASS
ZIP_CRC_PATH_SHA_EXTRACT_REBUILD=PASS
```

## Windows交付

```text
G17B1R5_Wrapper_Mount_No_Parachute_Windows_20260823.zip
size=37888
files=15
sha256=77cded6bb996945b9ad3230d21d5455eb566c3a7a93429ca79cd26cd9afe9158
```

唯一入口：`01_Install_Build_G17B1R5.cmd`。  
结果文件：`C:\Users\Administrator\Downloads\workspace\uploads\G17B1R5_WINDOWS_BUILD_RESULT.txt`。

Windows build、爱情火箭、无头骑士和无降落伞室内退出均保持`PENDING_USER`；不预写PASS。
