# G17-B1 全坐骑自动接管与类型会话交付（2026-08-23）

## 前置

G17-R5真实安装和最终行为均PASS，纯飞行门关闭。用户确认59961普通按钮可以召唤、上马、起飞和飞行，进入室内自动解除且无问题。

## 本批实现

- 前像R1源码SHA：`10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45`
- 后像B1源码SHA：`2c7594d0f1428a767570063ac90c5f816991bf1d883fe61e45a1a28902a68199`
- 默认自动拦截玩家自己已学会且含`SPELL_AURA_MOUNTED`的直接坐骑法术；
- 普通坐骑生效后100ms读取真实display、源法术和源creature entry；
- 替代Vehicle和可控座位准备完成后才移除普通坐骑Aura；
- Vehicle保留源坐骑display/native display；地面和飞行坐骑走同一接管链；
- 按CreatureTemplate.type语言无关识别DRAGON/BEAST/MECHANICAL/MAGIC/GENERIC，未知不拒绝；
- `.dragon auto on|off`、`.dragon mount <owned spellId>`、扩展`.dragon status`；
- 保留R1异步入座、动作条、energy、四技能、慢落和安全清理。

## 自动证据

```text
G17B1_GCC14_CXX20_SYNTAX=PASS
G17B1_GCC_WARNING_OUTPUT_LINES=0
G17B1_STATIC_TESTS=PASS_8
G17B1_INSTALLER_CHECK_APPLY_IDEMPOTENT_ROLLBACK=PASS
G17B1_POWERSHELL_AST=PASS_2_OF_2
G17B1_PACKAGE_SELFTEST=PASS
G17B1_SHA256SUMS_VERIFY=PASS
G17B1_ZIP_CRC=PASS
G17B1_ZIP_EXTRACTED_SELFTEST=PASS
```

## 最终包

```text
file=G17B1_All_Mounts_Auto_Intercept_Windows_20260823.zip
size=27946
files=13
sha256=3ea807f6e493bf7010ba6f8b5eb4e69cc5f577ff368d84d5e10cfc724eae4774
entry=01_Install_Build_G17B1.cmd
```

## 未提前声称

Windows真实构建和B1 Runtime仍待用户。B2五档/动量/1200%、B3独立战斗页、B4骑乘施法、B5自动寻路/固定、B6客户端体验和压力均未完成；本批不冒充完整御龙结束。
