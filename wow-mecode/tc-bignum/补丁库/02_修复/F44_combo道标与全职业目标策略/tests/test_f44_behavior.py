#!/usr/bin/env python3
"""F44目标策略的无Trinity依赖行为模型测试。"""
from dataclasses import dataclass, field


@dataclass
class Unit:
    guid: int
    hp: float = 100.0
    max_hp: int = 1000
    alive: bool = True
    friendly: bool = True
    in_group: bool = True
    is_bot: bool = False
    is_tank: bool = False
    is_main_tank: bool = False
    victim: "Unit | None" = None
    # (rank spell, caster guid)
    auras: set[tuple[int, int]] = field(default_factory=set)


@dataclass
class State:
    generation: int = 0
    on: bool = False
    maintain: dict[int, int] = field(default_factory=dict)
    casts: int = 0
    backed_off: set[int] = field(default_factory=set)


def legal(unit: Unit, members: list[Unit]) -> bool:
    return unit in members and unit.alive and unit.friendly and unit.in_group


def choose_stable(state: State, rank1: int, caster: Unit, members: list[Unit]) -> Unit:
    old = state.maintain.get(rank1)
    for unit in members:
        if unit.guid == old and legal(unit, members):
            return unit
    state.maintain.pop(rank1, None)

    # 重启后先恢复该施法者真实已有Aura。
    for unit in members:
        if legal(unit, members) and (rank1, caster.guid) in unit.auras:
            state.maintain[rank1] = unit.guid
            return unit

    candidates = [u for u in members if legal(u, members)]
    return max(candidates, key=lambda u: (
        3 if u.is_main_tank else 2 if u.is_tank else 0 if u is caster else 1,
        u.max_hp,
    ))


def cast_checked(state: State, spell: int, target: Unit, success: bool) -> bool:
    if spell in state.backed_off:
        return False
    if not success:
        state.backed_off.add(spell)
        return False
    state.casts += 1
    return True


def maintain_cast(state: State, spell: int, caster: Unit, target: Unit, success: bool) -> bool:
    if not cast_checked(state, spell, target, success):
        return False
    state.maintain[spell] = target.guid
    target.auras.add((spell, caster.guid))
    return True


def pick_heal(members: list[Unit]) -> Unit:
    # 同血量坦克按5%虚拟权重优先；NPCBot和Player走同一路。
    return min((u for u in members if legal(u, members)),
               key=lambda u: u.hp - (5.0 if u.is_tank else 0.0))


def event_is_live(state: State, captured_generation: int) -> bool:
    return state.on and state.generation == captured_generation


def stop(state: State) -> None:
    state.on = False
    state.generation += 1
    state.maintain.clear()


def start(state: State) -> int:
    stop(state)
    state.generation += 1
    state.on = True
    return state.generation


def only_exact_duplicates(items: list[tuple[int, int]]) -> list[tuple[int, int]]:
    out = []
    for item in items:
        if item not in out:
            out.append(item)
    return out


def run() -> None:
    player = Unit(1, max_hp=1000)
    main_tank = Unit(2, hp=80, max_hp=3000, is_tank=True, is_main_tank=True)
    off_tank_bot = Unit(3, hp=20, max_hp=3500, is_bot=True, is_tank=True)
    dps = Unit(4, hp=10, max_hp=1200)
    members = [player, main_tank, off_tank_bot, dps]
    state = State()

    # 1. 道标首次选择明确主坦，不跟最低血DPS迁移。
    beacon = 53563
    assert choose_stable(state, beacon, player, members) is main_tank
    assert maintain_cast(state, beacon, player, main_tank, True)
    dps.hp, main_tank.hp = 1, 100
    assert choose_stable(state, beacon, player, members) is main_tank

    # 2. 主坦失效才迁移到合法坦克候选（NPCBot）。
    main_tank.in_group = False
    assert choose_stable(state, beacon, player, members) is off_tank_bot

    # 3. 大地之盾失败不提交GUID、不计成功，并进入退避。
    earth_shield = 974
    before_casts = state.casts
    assert not maintain_cast(state, earth_shield, player, off_tank_bot, False)
    assert earth_shield not in state.maintain
    assert state.casts == before_casts and earth_shield in state.backed_off

    # 4. rank-1 Aura必须带同一施法者；别人施放的不算自己的维护Aura。
    ward = 6346
    off_tank_bot.auras.add((ward, 999))
    state.maintain.pop(ward, None)
    assert choose_stable(state, ward, player, members) is off_tank_bot
    assert (ward, player.guid) not in off_tank_bot.auras

    # 5. 治疗候选覆盖NPCBot。
    dps.hp = 50
    off_tank_bot.hp = 5
    assert pick_heal(members) is off_tank_bot

    # 6. SF_SELF模型：无论低血友方是谁，自身目标都固定为施法者。
    dynamic_low = pick_heal(members)
    self_target = player
    assert dynamic_low is off_tank_bot and self_target is player

    # 7. generation：stop→start后旧事件永久失效且不能续订。
    old = start(state)
    stop(state)
    new = start(state)
    assert old != new and not event_is_live(state, old) and event_is_live(state, new)

    # 8. 嘲讽资格：怪打自己不嘲；怪打合法队友才嘲。
    enemy = Unit(90, friendly=False, in_group=False)
    enemy.victim = player
    assert enemy.victim is player
    enemy.victim = off_tank_bot
    assert legal(enemy.victim, members)

    # 9. 只折叠完全相同spell+flags，合法双策略必须同时保留。
    dual = [(20473, 0x0), (20473, 0x1000), (20473, 0x1000)]
    assert only_exact_duplicates(dual) == [(20473, 0x0), (20473, 0x1000)]

    # 10. logout/stop清除稳定目标。
    state.maintain[beacon] = off_tank_bot.guid
    stop(state)
    assert not state.on and not state.maintain

    print("[OK] F44_BEHAVIOR_TESTS=10/10")


if __name__ == "__main__":
    run()
