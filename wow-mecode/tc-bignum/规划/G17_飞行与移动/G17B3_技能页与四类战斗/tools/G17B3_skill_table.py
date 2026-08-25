#!/usr/bin/env python3
"""G17-B3 four-archetype combat skill table (authoritative).

Every entry is a SPELL_EFFECT_DUMMY spell added to the client Spell.dbc.
Real damage/healing/control is implemented server-side by the matching
SpellScript in cs_dragonriding.cpp (B3-R1) so the client record is a safe,
visual-only carrier: no aura, no native effect, no cast gates.

Unit for base points: intended per-cast, defined by design doc §5.2.
Cooldowns/resources are server-side; client just shows tooltip.
"""
from __future__ import annotations

# Fields we set (SpellEntry 234-column layout, see DBCStructure.h):
#   0 ID, 4 Attributes, 5 AttributesEx, 28 CastingTimeIndex, 46 RangeIndex,
#   71-73 Effect1-3, 80-82 EffectBasePoints1-3, 133 SpellIconID,
#   140 Name offset, 174 Description offset.
ATTRIBUTES_CASTABLE_WHILE_MOUNTED = 0x100
ID_BASE = 990000

class Skill:
    def __init__(self, sid, name, desc, icon):
        self.id = sid
        self.name = name
        self.desc = desc
        self.icon = icon
    def __repr__(self):
        return f"<{self.id} {self.name}>"

def build_table():
    skills = []
    def add(base_name, names, descs, icon):
        for i, nm in enumerate(names):
            skills.append(Skill(ID_BASE + len(skills), nm, descs[i % len(descs)], icon))
    # DRAGON (icon 130 龙类) - 5
    add("dragon", ["龙息·烈焰","尾扫·裂地","龙鳞护体","振翼·旋风","龙威爆发"],
        ["喷吐灼热龙息，对前方敌人造成火焰伤害。","以巨尾横扫周围敌人，打断并击退。",
         "龙鳞硬化，短时间内大幅提高护甲并减免伤害。","奋力振翼，击飞身边敌人并向后位移。",
         "凝聚龙威全力爆发，对目标造成巨额伤害。"], 130)
    add("beast", ["猛兽撕咬","狂暴连爪","兽群守护","扑袭·压制","嗜血终结"],
        ["凶猛地撕咬目标，造成物理伤害。","连续的爪击，对目标造成多重伤害。",
         "呼唤兽群守护，回复自身生命并移除恐惧。","飞扑压制目标，造成伤害并使其瘫痪。",
         "进入嗜血状态，短时间内攻击大幅强化。"], 134)
    add("magic", ["奥术弹幕","相位虹吸","法力护盾","时空过载","秘法新星"],
        ["凝聚奥术能量射向目标，造成奥术伤害。","汲取目标能量转化为自身法力。",
         "展开法力护盾，吸收即将到来的伤害。","扭曲时间，短暂加速并提高闪避。",
         "释放秘法新星，对周围敌人造成奥术爆发伤害。"], 66)
    add("mechanical", ["机炮扫射","火箭齐射","烟幕掩护","战地维修","过载轰击"],
        ["旋转机炮扫射目标，造成物理伤害。","发射火箭齐射，对目标区域造成爆炸伤害。",
         "释放烟幕，降低敌人命中并提高自身闪避。","启动维修协议，恢复自身耐久与生命。",
         "引擎过载，对目标进行毁灭性轰击并产生大量热量。"], 209)
    add("generic", ["冲击波","践踏","守护之力","猛冲","全功率爆发"],
        ["释放冲击波，对前方敌人造成伤害。","重踏地面，伤害并减速周围敌人。",
         "守护之力环绕，短暂提高护甲与抗性。","猛冲向目标，造成伤害并击退。",
         "全功率爆发，对所有敌人造成毁灭性伤害。"], 136)
    return skills

if __name__ == "__main__":
    t = build_table()
    print(len(t), "skills")
    for s in t:
        print(f"  {s.id}  {s.name}  icon={s.icon}")
