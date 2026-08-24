G17-B2 COMPLETE FLIGHT + TYPED NO-PARACHUTE LANDING - 2026-08-23

PROVEN BASELINE:
- B1R5 real Windows deployment, wrapper mounts, Headless Horseman's Mount, and indoor exit without parachute: PASS (user confirmed).
- Do not rerun B1, B1R1-R5, or client R1-R5 packages.
- R5 zhCN locale Y slot is not modified by this package.

B2 CONTENT:
- Seven bounded flight stages: 250, 400, 600, 800, 1000, 1100, and 1200 percent.
- Smooth momentum from forward movement, dive, climb, turning, braking, idle drag, and a bounded boost window.
- Recoverable low-momentum gravity stall.
- Skill 3 moves forward along current facing and upward, then explicitly restores flight pose, gravity, speed, ascent, descent, turning, and forward control.
- Skill 4 suppresses its parachute Aura and uses typed motion: magic/wind, beast pounce, mechanical rocket braking, dragon folded-wing descent, or generic controlled landing.
- No G17 cleanup or active-landing path casts a parachute.
- B1R3 authoritative seats, B1R4 250 ms indoor checks, and B1R5 wrapper-mount authority remain present.

ONLY INSTALL ACTION:
1. Stop worldserver normally.
2. Extract this package under:
   C:\Users\Administrator\Downloads\workspace\uploads
3. Double-click:
   01_Install_Build_G17B2.cmd
4. Return the small result file:
   C:\Users\Administrator\Downloads\workspace\uploads\G17B2_WINDOWS_BUILD_RESULT.txt
5. Start worldserver only after G17B2_WINDOWS_BUILD_RESULT=PASS.

MINIMAL REAL RUNTIME CHECK AFTER BUILD PASS:
A. Skill 3: while facing forward in open air, press skill 3. It must move forward and upward, never backward. At completion, the mount must remain in flight pose and ascent, descent, turning, and forward movement must work immediately.
B. Momentum: fly forward and dive, then climb, turn, brake, and idle. Speed changes must be smooth. Use .dragon status to observe momentum/speed; sustained forward/dive plus skill 2 must be able to reach speed=1200 without exceeding it.
C. Stall: drain momentum while safely above ground. The mount must begin a short gravity fall and recover by forward/dive/skill 2 input.
D. Typed landing: test skill 4 on any two different available types (prefer one mechanical and one beast or magic). No parachute may appear; both must reach the ground, exit the vehicle, and leave normal player movement.

No large matrix and no ZIP return are required. Return G17B2_WINDOWS_BUILD_RESULT.txt first, then report A-D in a few lines.

DO NOT run SQL. DO NOT modify the client. DO NOT run rollback unless specifically instructed after a confirmed B2 problem.

STRICT SOURCE STATES:
PRE  = 35af002b09b5d8112bbc1aaa1750f4a6245adec8b7c91a7852d69bdd283668b8 (exact accepted B1R5)
POST = 8b47a5b507bc281198363972e10f91ab0ed3784ad920cf810bd20eacfb6ec1d5 (exact G17B2)
Unknown source SHA aborts without overwrite.

Offline GCC/C++20 strict compile and 33/33 automated tests are PASS. Windows MSBuild and real client control/visual behavior remain PENDING_USER and are not pre-claimed as PASS.
