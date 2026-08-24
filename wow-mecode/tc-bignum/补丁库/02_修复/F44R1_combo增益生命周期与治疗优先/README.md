# F44R1：`.combo`增益生命周期与治疗优先修复

本批接续旧F44真人失败，不是重做旧探针/Apply/编译。

## 交付内容

- `源文件/`：三个安装后源码；
- `原始文件/F44_真人缺陷版/`：三个锁定安装前像；
- `工具/`：修复后的受控JSON与可复现生成器；
- `install_f44r1_combo.py`：精确哈希Check/Apply/Rollback/self-test；
- `tests/`：静态、12项行为模型、完整runtime mock与既有回归统一入口；
- `证据/`：本地回归原始输出；
- `01-真人失败根因与全职业审计.md`：根因、全职业分类和证据边界；
- `02-安装编译与真人验收.md`：Windows增量安装、编译、清楚区分的实际命令与反馈模板。

## 关键修复

1. 圣骑祝福按目标职业/职责只选一种，不再力量/智慧/王者互相覆盖；
2. 所有正常Aura必须真正消失后才补，删除5分钟/10秒提前刷新；
3. 全专精模式折叠姿态、形态、守护、护甲、圣印、领域、护盾及图腾元素家族；
4. 图腾按每次进战斗成功部署一次；毒药/武器附魔禁止错误Unit Aura重放；
5. 误导/嫁祸改为战斗工具；回蓝/防御/宠物专属错误分类已清理；
6. `.buff now`队列全局让GCD；进战斗/需治疗时自动取消；`.buff off`立即取消旧事件；
7. 治疗职责在自己、坦克或DPS低于血线时只治疗、必要回蓝或等待，不下落攻击。

## 本地结果

```text
F44R1_STATIC_TESTS=PASS specs=31 controlled_entries=493 raw_casts=1
F44R1_BEHAVIOR_TESTS=12/12
F44R1_INSTALLER_SELF_TEST=normal+optimized
F44R1_FULL_RUNTIME_MOCK=32/32
LEGACY_MOCK=10/10 suites no [FAIL]
F44R1_ALL_TESTS=PASS
```

本地PASS不等于Windows运行PASS。当前仍需一次F44R1增量Apply、VS2022真实编译、新二进制启动和限定真人验收。