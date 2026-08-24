G17-R1 independent runtime fix - 2026-08-22

This package replaces neither the old G17-B0 world SQL nor any old build package.
Do NOT rerun G17-B0 Source Apply, world install, or Windows build v1/v2/v3.

What this fixes
---------------
1. Dragon boarding: the old command checked GetVehicleBase() immediately even
   though EnterVehicle queues VehicleJoinEvent asynchronously. R1 discovers the
   actual controllable seat and verifies vehicle/seat/charm control 250 ms later.
2. Pure flying mounts: the client Spell.dbc still blocks direct proto-drake casts
   locally. The included client-only patcher clears only that client gate on
   structural mount/flying-mount rows. The server DBC remains unchanged so G17-A
   keeps enforcing allowed and blocked areas.
3. The tracked cs_dragonriding.cpp C4018 is fixed and the Windows build gate now
   requires zero matching C4018 warnings.

Required order
--------------
A. Stop worldserver normally.
B. Run exactly once for the new source/build batch:
      Run-G17R1-Windows-Fix.cmd
C. Return this result before starting worldserver:
      C:\Users\Administrator\Downloads\workspace\uploads\G17R1_WINDOWS_FIX_RESULT.txt

The new script is rerunnable only for recovery from its own interrupted/failed
attempt. It does exact pre/post hash checks, creates a separate rollback backup,
runs CMake and MSBuild, requires a fresh dragonriding OBJ and changed EXE/PDB,
and stops without starting worldserver.

Client Spell.dbc preparation
----------------------------
The safe default only creates staging output and never edits the server DBC:
      Run-G17R1-Prepare-Client-Patch.cmd

Outputs:
  uploads\G17R1_Client_Patch_Staging\DBFilesClient\Spell.dbc
  uploads\G17R1_CLIENT_DBC_PATCH_RESULT.txt
  uploads\G17R1_CLIENT_PREPARE_RESULT.txt

For a client whose root and loose-patch support are already known, use:
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File ^
    ".\Prepare-G17R1-Client-Patch.ps1" ^
    -ClientRoot "D:\path\to\World of Warcraft" -PatchSlot Z

This creates Data\patch-Z.MPQ\DBFilesClient\Spell.dbc only when patch-Z.MPQ is
absent or is an editable directory. It refuses to overwrite a packed MPQ or a
different existing Spell.dbc. Delete the client Cache directory before testing.
If the client does not support loose directory patches, package the staging tree
as the highest-priority custom MPQ with DBFilesClient at the archive root.

Critical safety boundary
------------------------
Never copy the generated client Spell.dbc back into:
  D:\TC-Build\bin\RelWithDebInfo\dbc\Spell.dbc
The original server DBC must retain its location attribute. G17-A server code is
then the authority for outdoor, city, indoor, PvP, dungeon and no-fly policy.

Runtime acceptance after build and client patch
------------------------------------------------
1. Start the newly built worldserver.exe.
2. In an allowed outdoor location: .dragon summon
3. Wait for G17-R1 ready message; then .dragon status.
4. Required: ACTIVE, controlled=true, and a four-slot vehicle action bar.
5. Test one ability and .dragon dismiss.
6. In the same allowed outdoor location, summon Red Proto-Drake (spell 59961)
   or another pure flying proto-drake; verify summon, takeoff, movement, landing.
7. Do not repeat old SQL/install/build packages.
