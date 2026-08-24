#!/usr/bin/env python3
"""F44R1生命周期、互斥祝福、部署物和治疗优先的无Trinity行为模型。"""
from __future__ import annotations

from dataclasses import dataclass, field

MIGHT, WISDOM, KINGS, SANCTUARY = 25782, 25894, 25898, 20911


@dataclass
class Unit:
    guid: int
    cls: int
    hp: float = 100.0
    mana_user: bool = False
    tank: bool = False
    main_tank: bool = False
    legal: bool = True
    # spell -> remaining milliseconds; -1 means permanent
    auras: dict[int, int] = field(default_factory=dict)


@dataclass
class Runtime:
    buff_generation: int = 0
    buff_queue_until: int = 0
    auto_buff: bool = True
    combat_generation: int = 0
    was_in_combat: bool = False
    deployed: set[int] = field(default_factory=set)
    stable: dict[int, int] = field(default_factory=dict)


def preferred_blessing(target: Unit, same_class: list[Unit], known: set[int]) -> int | None:
    class_has_tank = any(u.legal and u.tank for u in same_class)
    class_uses_mana = any(u.legal and u.mana_user for u in same_class)
    if class_has_tank:
        order = (SANCTUARY, KINGS, MIGHT, WISDOM)
    elif class_uses_mana:
        order = (WISDOM, KINGS, MIGHT, SANCTUARY)
    else:
        order = (MIGHT, KINGS, SANCTUARY, WISDOM)
    return next((spell for spell in order if spell in known), None)


def needs_buff(unit: Unit, spell: int) -> bool:
    # Core acceptance rule: no early-refresh window exists.
    return unit.auras.get(spell, 0) == 0


def blessing_queue(members: list[Unit], known: set[int]) -> list[tuple[int, Unit]]:
    queue: list[tuple[int, Unit]] = []
    for spell in (MIGHT, WISDOM, KINGS, SANCTUARY):
        if spell not in known:
            continue
        for unit in members:
            same_class = [m for m in members if m.cls == unit.cls]
            if preferred_blessing(unit, same_class, known) == spell and needs_buff(unit, spell):
                queue.append((spell, unit))
    return queue


def execute_greater_blessing_queue(queue: list[tuple[int, Unit]], members: list[Unit]) -> int:
    """Model Greater Blessing affecting every same-class member and event-time recheck."""
    casts = 0
    for spell, target in queue:
        if not needs_buff(target, spell):
            continue
        casts += 1
        for unit in members:
            if unit.cls == target.cls:
                unit.auras[spell] = 1_800_000
    return casts


def collapse_families(spells: list[int], family: dict[int, int]) -> list[int]:
    out: list[int] = []
    chosen: set[int] = set()
    for spell in spells:
        f = family.get(spell, 0)
        if f and f in chosen:
            continue
        out.append(spell)
        if f:
            chosen.add(f)
    return out


def enter_combat(rt: Runtime) -> None:
    if not rt.was_in_combat:
        rt.combat_generation += 1
        rt.deployed.clear()
    rt.was_in_combat = True


def leave_combat(rt: Runtime) -> None:
    rt.was_in_combat = False


def deploy_once(rt: Runtime, spell: int, success: bool) -> bool:
    if spell in rt.deployed or not success:
        return False
    rt.deployed.add(spell)
    return True


def buff_off(rt: Runtime) -> None:
    rt.auto_buff = False
    rt.buff_generation += 1
    rt.buff_queue_until = 0


def queue_gate(rt: Runtime, now: int, in_combat: bool, healing_needed: bool) -> str:
    if now >= rt.buff_queue_until:
        return "continue"
    if in_combat or healing_needed:
        rt.buff_generation += 1
        rt.buff_queue_until = 0
        return "cancel-and-continue"
    return "yield"


def healer_action(injured: Unit | None, threshold: float, heal_ready: bool,
                  mana_low: bool, recovery_ready: bool) -> str:
    if injured is None or injured.hp > threshold:
        return "attack"
    if heal_ready:
        return "heal"
    if mana_low and recovery_ready:
        return "recover-mana"
    return "wait-for-heal"  # Never attack while the healing line is unsafe.


def choose_stable(rt: Runtime, spell: int, members: list[Unit]) -> Unit:
    saved = rt.stable.get(spell)
    for unit in members:
        if unit.guid == saved and unit.legal:
            return unit
    rt.stable.pop(spell, None)
    best = max((u for u in members if u.legal),
               key=lambda u: (3 if u.main_tank else 2 if u.tank else 0, -u.guid))
    rt.stable[spell] = best.guid
    return best


def run() -> None:
    # 1. 五个不同职业只规划每职业一种祝福，不是“三祝福 x 五人”的15次覆盖。
    members = [
        Unit(1, 2, mana_user=True),
        Unit(2, 1, tank=True, main_tank=True),
        Unit(3, 5, mana_user=True),
        Unit(4, 4),
        Unit(5, 8, mana_user=True),
    ]
    known = {MIGHT, WISDOM, KINGS}
    queue = blessing_queue(members, known)
    assert len(queue) == 5
    assert sum(1 for _, u in queue if u.guid == 2) == 1

    # 2. 初次合法施放后，32秒仍有正持续时间：0次补放、0个额外王者印记。
    assert execute_greater_blessing_queue(queue, members) == 5
    for unit in members:
        for spell in tuple(unit.auras):
            unit.auras[spell] -= 32_000
    assert blessing_queue(members, known) == []

    # 3. 只有某职业祝福真正归零，才为该职业补一次。
    rogue = members[3]
    rogue.auras[MIGHT] = 0
    expired = blessing_queue(members, known)
    assert expired == [(MIGHT, rogue)]

    # 4. 同职业两人虽然预排两项，首个强效祝福生效后事件时复检使第二项跳过。
    same_class = [Unit(20, 8, mana_user=True), Unit(21, 8, mana_user=True)]
    queued = blessing_queue(same_class, known)
    assert len(queued) == 2
    assert execute_greater_blessing_queue(queued, same_class) == 1

    # 5. 永久Aura和尚余1毫秒都不刷新；只有0才刷新。
    u = Unit(30, 1)
    u.auras[6673] = -1
    assert not needs_buff(u, 6673)
    u.auras[6673] = 1
    assert not needs_buff(u, 6673)
    u.auras[6673] = 0
    assert needs_buff(u, 6673)

    # 6. 全专精互斥族：姿态/怒吼各只留第一个首选，非互斥警戒保留。
    family = {2457: 1, 71: 1, 2458: 1, 6673: 2, 469: 2}
    assert collapse_families([71, 469, 2457, 6673, 50720], family) == [71, 469, 50720]

    # 7. 图腾每次进入战斗只成功部署一次；失败不记成功，下次tick可重试。
    rt = Runtime()
    enter_combat(rt)
    assert not deploy_once(rt, 30706, False)
    assert deploy_once(rt, 30706, True)
    assert not deploy_once(rt, 30706, True)
    leave_combat(rt)
    enter_combat(rt)
    assert deploy_once(rt, 30706, True)

    # 8. .buff off立即失效旧事件代次并清空排队截止时间。
    rt.buff_queue_until = 100_000
    old_generation = rt.buff_generation
    buff_off(rt)
    assert not rt.auto_buff and rt.buff_queue_until == 0 and rt.buff_generation == old_generation + 1

    # 9. 脱战安全时combo为队列完全让GCD；进战斗或治疗需求会取消队列。
    rt = Runtime(buff_queue_until=50_000)
    assert queue_gate(rt, 10_000, False, False) == "yield"
    assert queue_gate(rt, 10_000, False, True) == "cancel-and-continue"
    rt.buff_queue_until = 50_000
    assert queue_gate(rt, 10_000, True, False) == "cancel-and-continue"

    # 10. 治疗自己、坦克、DPS均走同一治疗优先线；没奶出来时绝不攻击。
    for injured in (Unit(40, 2, hp=20), Unit(41, 1, hp=35, tank=True), Unit(42, 4, hp=50)):
        assert healer_action(injured, 80, True, False, False) == "heal"
        assert healer_action(injured, 80, False, False, False) == "wait-for-heal"
        assert healer_action(injured, 80, False, True, True) == "recover-mana"
    assert healer_action(Unit(43, 4, hp=100), 80, False, False, False) == "attack"

    # 11. 道标等稳定友方不会随最低血迁移；原目标失效后才迁移。
    tank = Unit(50, 1, hp=90, tank=True, main_tank=True)
    dps = Unit(51, 4, hp=1)
    rt = Runtime()
    assert choose_stable(rt, 53563, [tank, dps]) is tank
    tank.hp, dps.hp = 100, 0.1
    assert choose_stable(rt, 53563, [tank, dps]) is tank
    tank.legal = False
    assert choose_stable(rt, 53563, [tank, dps]) is dps

    # 12. 修复后模型的关键消耗结论：32秒阶段只含初始5次，绝不增长到17/20次。
    assert len(queue) == 5 and blessing_queue(members, known) == [(MIGHT, rogue)]

    print("[OK] F44R1_BEHAVIOR_TESTS=12/12")


if __name__ == "__main__":
    run()
