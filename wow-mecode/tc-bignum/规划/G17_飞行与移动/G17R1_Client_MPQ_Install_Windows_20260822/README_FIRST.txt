G17-R1 packed client MPQ install - 2026-08-22

Current accepted state
----------------------
- The G17-R1 Windows source build already passed. Do not rebuild it.
- The client-only Spell.dbc staging file already passed exact verification.
- The staged DBC is not yet installed into D:\WOW.
- Runtime after the fix has not yet been executed.

Install now
-----------
1. Keep Wow.exe closed. The worldserver may remain stopped until Runtime.
2. Extract this ZIP to a normal folder.
3. Double-click exactly:
     Run-G17R1-Client-MPQ-Install.cmd
4. The installer uses D:\WOW, verifies the exact staged DBC and exact original
   server DBC, chooses the highest free root letter slot from patch-Z.MPQ down
   to patch-A.MPQ, creates a real packed WotLK MPQ, extracts it back, verifies
   the DBC hash, and installs it atomically.
5. Required success marker:
     G17R1_CLIENT_MPQ_INSTALL_RESULT=PASS
6. Result file:
     C:\Users\Administrator\Downloads\workspace\uploads\G17R1_CLIENT_MPQ_INSTALL_RESULT.txt

Safety properties
-----------------
- Existing packed MPQs and loose patch directories are never overwritten.
- An occupied higher-priority patch is read-only inspected. A conflicting
  DBFilesClient\Spell.dbc or an unreadable archive stops installation.
- The server DBC must remain exact SHA-256:
    df44e75ef1730e363dc06f1bc5ae064299b08d2d0047e663c0a1782ed4c8d10f
- The client-only DBC must remain exact SHA-256:
    dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea
- The installed MPQ and absent-target preimage are recorded in a state file.
- A failure after atomic install automatically removes the new target from the
  client Data directory and preserves it in the work directory for diagnosis.

Runtime immediately after install PASS
--------------------------------------
1. Start the newly built worldserver.exe and launch WoW again.
2. At a G17-A allowed ordinary outdoor location run: .dragon summon
3. Wait for the ready message, then run: .dragon status
4. Required: ACTIVE, controlled=true, and a four-slot vehicle action bar.
5. Test at least one vehicle ability and: .dragon dismiss
6. At the same allowed outdoor location summon Red Proto-Drake spell 59961 (or
   another listed proto-drake) and verify summon, takeoff, movement and landing.
7. If either test fails, preserve the first G17-R1/worldserver log lines and the
   exact game message. Do not rerun old Source Apply, SQL, or build packages.

Rollback only if needed
-----------------------
- Close WoW.
- Double-click Run-G17R1-Client-MPQ-Rollback.cmd and type ROLLBACK.
- Rollback removes only the exact state-owned MPQ after hash verification and
  preserves the archive and install state under uploads.

Third-party MPQ tool
--------------------
The package includes mpqcli v0.10.2 Windows AMD64 under its MIT license. Its
asset SHA-256 is pinned and verified before every install:
  5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f
See THIRD_PARTY_PROVENANCE.txt and third_party\mpqcli-LICENSE.txt.
