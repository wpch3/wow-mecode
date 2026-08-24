#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G17-B0 read-only narrow source lock probe. Never edits D:\\TrinityCore."""
from __future__ import annotations
import argparse, datetime as dt, hashlib, os, shutil, subprocess, sys, tempfile, zipfile
from pathlib import Path

DEFAULT_SOURCE=Path(r"D:\TrinityCore")
DEFAULT_UPLOADS=Path(r"C:\Users\Administrator\Downloads\workspace\uploads")
FILES=(
    "src/server/scripts/Commands/cs_script_loader.cpp",
    "src/server/scripts/ScriptLoader.h",
    "src/server/scripts/CMakeLists.txt",
    "src/server/game/AI/CoreAI/CombatAI.h",
    "src/server/game/Scripting/ScriptMgr.h",
    "src/server/game/Entities/Unit/Unit.h",
    "src/server/game/Maps/Map.h",
    "sql/base/dev/world_database.sql",
)
TARGET_NEW="src/server/scripts/Commands/cs_dragonriding.cpp"
ANCHORS={
    "loader_decl_wp":"void AddSC_wp_commandscript();",
    "loader_call_wp":"    AddSC_wp_commandscript();",
    "loader_decl_g17b0":"void AddSC_dragonriding_commandscript();",
    "loader_call_g17b0":"    AddSC_dragonriding_commandscript();",
    "vehicle_ai":"struct TC_GAME_API VehicleAI",
    "player_map_hook":"virtual void OnMapChanged(Player* player);",
    "player_zone_hook":"virtual void OnUpdateZone(Player* player, uint32 newZone, uint32 newArea);",
    "enter_vehicle":"void EnterVehicle(Unit* base, int8 seatId = -1);",
    "map_dungeon":"bool IsDungeon() const;",
    "map_bg_arena":"bool IsBattlegroundOrArena() const;",
    "db_creature_template":"CREATE TABLE `creature_template`",
    "db_creature_spell":"CREATE TABLE `creature_template_spell`",
    "db_creature_movement":"CREATE TABLE `creature_template_movement`",
    "db_spell_scripts":"CREATE TABLE `spell_script_names`",
}

def sha(data:bytes)->str: return hashlib.sha256(data).hexdigest()
def run(root:Path,*args:str)->str:
    try:
        p=subprocess.run(args,cwd=root,text=True,encoding="utf-8",errors="replace",stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
        return f"RC={p.returncode}\n{p.stdout.rstrip()}"
    except Exception as e: return f"ERROR={type(e).__name__}:{e}"

def probe(source:Path,uploads:Path)->Path:
    source=source.resolve()
    if not (source/".git").exists(): raise RuntimeError(f"not a git source root: {source}")
    stamp=dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    uploads.mkdir(parents=True,exist_ok=True)
    final_zip=uploads/f"G17B0_LOCK_RESULT_{stamp}.zip"
    with tempfile.TemporaryDirectory(prefix="g17b0_lock_") as td:
        out=Path(td)/f"G17B0_LOCK_RESULT_{stamp}"
        originals=out/"original"
        originals.mkdir(parents=True)
        report=[]
        report += ["G17B0_LOCK_SCHEMA=1",f"SOURCE_ROOT={source}",f"PROBE_TIME_LOCAL={dt.datetime.now().astimezone().isoformat()}"]
        report += ["\n--- GIT_BRANCH ---",run(source,"git","branch","--show-current")]
        report += ["\n--- GIT_HEAD ---",run(source,"git","rev-parse","HEAD")]
        report += ["\n--- GIT_STATUS_SHORT ---",run(source,"git","status","--short")]
        report += ["\n--- LOADER_DIFF_AGAINST_HEAD ---",run(source,"git","diff","--","src/server/scripts/Commands/cs_script_loader.cpp","src/server/scripts/ScriptLoader.h")]

        texts={}
        for rel in FILES:
            p=source/rel
            if not p.is_file():
                report.append(f"FILE_MISSING={rel}")
                continue
            data=p.read_bytes()
            dst=originals/rel
            dst.parent.mkdir(parents=True,exist_ok=True)
            dst.write_bytes(data)
            text=data.decode("utf-8",errors="replace")
            texts[rel]=text
            ending="CRLF" if b"\r\n" in data and b"\n" not in data.replace(b"\r\n",b"") else "LF_OR_MIXED"
            report.append(f"FILE={rel};SIZE={len(data)};SHA256={sha(data)};BOM={data.startswith(bytes((0xEF,0xBB,0xBF)))};EOL={ending}")

        target=source/TARGET_NEW
        if target.is_file():
            target_data=target.read_bytes()
            target_copy=originals/TARGET_NEW
            target_copy.parent.mkdir(parents=True,exist_ok=True)
            target_copy.write_bytes(target_data)
            report.append(f"TARGET_NEW={TARGET_NEW};STATE=PRESENT;SIZE={len(target_data)};SHA256={sha(target_data)};COPIED=True")
        else:
            report.append(f"TARGET_NEW={TARGET_NEW};STATE=ABSENT;COPIED=False")
        for label,needle in ANCHORS.items():
            count=sum(t.count(needle) for t in texts.values())
            report.append(f"ANCHOR={label};COUNT={count};NEEDLE={needle}")

        loader=texts.get(FILES[0],"")
        pre=(loader.count(ANCHORS["loader_decl_g17b0"])==0 and loader.count(ANCHORS["loader_call_g17b0"])==0)
        post=(loader.count(ANCHORS["loader_decl_g17b0"])==1 and loader.count(ANCHORS["loader_call_g17b0"])==1)
        anchors=(loader.count(ANCHORS["loader_decl_wp"])==1 and loader.count(ANCHORS["loader_call_wp"])==1)
        report.append(f"G17B0_LOADER_STATE={'PRE' if pre else ('POST' if post else 'UNKNOWN')}")
        report.append(f"G17B0_LOADER_WP_ANCHORS_READY={anchors}")
        report.append(f"G17B0_NEW_SOURCE_ABSENT={not target.exists()}")
        report.append("G17B0_LIVE_DB_NOT_QUERIED=True")
        report.append("G17B0_PROBE_WRITES_SOURCE=False")
        ready=anchors and (pre or post)
        report.append(f"G17B0_NARROW_SOURCE_LOCK_READY={ready}")
        (out/"g17b0_lock_report.txt").write_text("\n".join(report)+"\n",encoding="utf-8")
        (out/"README.txt").write_text(
            "本ZIP由G17-B0只读锁定探针生成。original/保存Windows真实原文件；报告不含数据库密码。\n"
            "探针未修改D:\\TrinityCore。将整个ZIP回传，不要手工编辑其中内容。\n",encoding="utf-8")
        with zipfile.ZipFile(final_zip,"w",zipfile.ZIP_DEFLATED,compresslevel=9) as z:
            for p in sorted(out.rglob("*")):
                if p.is_file(): z.write(p,p.relative_to(out.parent))
    return final_zip

def main()->int:
    ap=argparse.ArgumentParser(); ap.add_argument("--source",type=Path,default=DEFAULT_SOURCE); ap.add_argument("--uploads",type=Path,default=DEFAULT_UPLOADS); a=ap.parse_args()
    try:
        result=probe(a.source,a.uploads)
        print(f"G17B0_PROBE=PASS\nG17B0_RESULT_ZIP={result}\nG17B0_SOURCE_MODIFIED=False")
        return 0
    except Exception as e:
        print(f"G17B0_PROBE=FAIL\nG17B0_ERROR={e}",file=sys.stderr); return 2
if __name__=="__main__": raise SystemExit(main())
