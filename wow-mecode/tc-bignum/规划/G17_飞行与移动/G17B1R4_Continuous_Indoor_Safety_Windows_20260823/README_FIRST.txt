G17-B1R4 CONTINUOUS INDOOR SAFETY FIX - 2026-08-23

Confirmed runtime state before this package:
- Ground and flying mount conversion: PASS.
- Takeoff and landing: PASS.
- R3 authoritative seat verification: PASS.
- Entering a real indoor location did not remove the G17 vehicle: FAIL.

Root cause:
PlayerScript::OnUpdateZone is not guaranteed to run when VMap indoor/outdoor state or only a sub-area changes inside the same zone. The old indoor policy existed but had no reliable execution point.

This package adds a bounded 250 ms safety check to the active G17 Vehicle AI. It reuses the existing server-side blocked-area policy and performs one-shot fall-safe cleanup. The old zone hook remains as defense in depth.

This package deliberately DOES NOT modify skill 3, climb, landing movement, animation state, momentum, or B2. The reported reverse jump, air-walking pose, and loss of vertical control are recorded for the later movement-system batch.

ONLY ACTION:
1. Stop worldserver normally.
2. Extract this package under:
   C:\Users\Administrator\Downloads\workspace\uploads
3. Double-click:
   01_Install_Build_G17B1R4.cmd
4. Return:
   C:\Users\Administrator\Downloads\workspace\uploads\G17B1R4_WINDOWS_BUILD_RESULT.txt
5. Only after G17B1R4_WINDOWS_BUILD_RESULT=PASS, start worldserver normally.
6. Mount either one already-proven mount, enter the SAME real indoor location, and report whether the vehicle disappears within about one second with no abnormal speed or vehicle residue.

Do not run SQL. Do not modify the client or the R5 zhCN MPQ. Do not rerun B1R1, R1-R5, or B1R3. Do not run rollback unless specifically instructed after a confirmed B1R4 problem.

Strict source states:
PRE  = 94ff80334783e8883f0811a1a7f76595d91b729cc43684f00273abb9d955628b
POST = e9418704731a2d9cd5119cc2024079a2326802796d00bf24e88928dd17ea7059
Unknown source SHA aborts without overwrite.

Windows build and indoor runtime are still PENDING_USER and are not pre-claimed as PASS.
