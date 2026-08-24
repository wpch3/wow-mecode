G17-B1R5 WRAPPER MOUNT + NO-PARACHUTE BLOCKED EXIT FIX - 2026-08-23

Confirmed runtime state before this package:
- Ground and flying direct-mount conversion: PASS (user confirmed).
- B1R3 authoritative seat verification: PASS.
- B1R4 real indoor automatic exit: PASS (user confirmed).
- B1R4 indoor exit displayed a parachute: REJECTED by user.
- Love Rocket and Headless Horseman's Mount conversion: FAIL (user confirmed).

Exact root cause:
The learned outer wrapper spells contain Mounted-aura metadata in Spell.dbc, but SpellMgr intentionally changes their first two runtime effects to SPELL_EFFECT_NONE. A generic spell_gen_mount script then chooses and triggers an inner Mounted spell. B1R4 required the same spell ID to be both learned and the active Mounted aura, so the outer and inner halves were each rejected.

B1R5 generic fix:
- Recognize mount candidates from retained ApplyAuraName == SPELL_AURA_MOUNTED metadata, including direct mounts and sanitized wrapper mounts.
- Keep ownership anchored to the learned outer spell.
- After successful mounting, use the active inner Mounted aura for creature entry and the player's active mount display.
- No mount name or spell ID is hard-coded in runtime code.
- Indoor/blocked cleanup no longer casts or displays a parachute.
- Vehicle flight speed, can-fly, gravity and player movement rates are normalized during blocked cleanup.
- High/boundary exit gets a bounded, non-visual fall-damage accounting guard only; it adds no aura, model, slow-fall movement, teleport or gravity override.

This package deliberately DOES NOT modify skill 3, climb/air-walking posture, post-cast vertical control, momentum, staged speed or B2. Those recorded movement issues begin after B1R5 runtime acceptance.

ONLY ACTION:
1. Stop worldserver normally.
2. Extract this package under:
   C:\Users\Administrator\Downloads\workspace\uploads
3. Double-click:
   01_Install_Build_G17B1R5.cmd
4. Return the small result file:
   C:\Users\Administrator\Downloads\workspace\uploads\G17B1R5_WINDOWS_BUILD_RESULT.txt
5. Only after G17B1R5_WINDOWS_BUILD_RESULT=PASS, start worldserver normally.
6. Runtime check (no matrix required):
   a. Click Love Rocket: it must enter G17 control with the Love Rocket model.
   b. Click Headless Horseman's Mount: it must enter G17 control with that mount model.
   c. Enter the same real indoor location: the G17 vehicle must exit promptly with NO parachute, no retained vehicle/control, no flying/gravity state and no abnormal player speed.
7. Reply with the three results; no ZIP upload is required.

Do not run SQL. Do not modify the client or the R5 zhCN MPQ. Do not rerun B1/B1R1, R1-R5, B1R3 or B1R4. Do not run rollback unless specifically instructed after a confirmed B1R5 problem.

Strict source states:
PRE  = e9418704731a2d9cd5119cc2024079a2326802796d00bf24e88928dd17ea7059 (exact B1R4)
POST = 35af002b09b5d8112bbc1aaa1750f4a6245adec8b7c91a7852d69bdd283668b8 (exact B1R5)
Unknown source SHA aborts without overwrite.

Windows build and the three B1R5 runtime checks are PENDING_USER and are not pre-claimed as PASS.
