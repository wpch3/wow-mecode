#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import importlib.util
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tempfile
import zipfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT.parents[2] / "规划/G17_飞行与移动"
PACKAGE = PLAN / "G17B0_源码受控Apply包_20260822"
ZIP = PLAN / "G17B0_Source_Apply_20260822.zip"
ZIP_SIZE = 88397
ZIP_SHA256 = "faf3fa5e4d5a419f6d09376b79cb5dbbae01b0841f073b53e7dd01b8c40cccd5"
TOP = "G17B0_Source_Apply_20260822"
ENV = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", PYTHONUTF8="1")


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_installer(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader
    spec.loader.exec_module(module)
    return module


def make_fixture(root: Path, module) -> None:
    (root / ".git").mkdir(parents=True)
    for rel in module.CONTEXT_SHA256:
        destination = root / rel
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(module.PREIMAGE_ROOT / rel, destination)
    loader = root / module.LOADER_REL
    loader.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(module.PRE_LOADER, loader)


require(ZIP.stat().st_size == ZIP_SIZE, "ZIP size")
require(sha(ZIP) == ZIP_SHA256, "ZIP hash")
require((PACKAGE / "G17B0_APPLY_APPROVED.txt").read_text(encoding="utf-8").strip() ==
        "G17B0_SOURCE_AND_DB_APPROVED", "approval")
require(not list(PACKAGE.rglob("__pycache__")), "package contains pycache")
require(not list(PACKAGE.rglob("*.pyc")), "package contains pyc")
require(not list(PACKAGE.rglob("*world_install*.sql")), "source-only package contains DB install")

sum_lines = (PACKAGE / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines()
require(len(sum_lines) == 20, "checksum entries")
seen = set()
for line in sum_lines:
    digest, rel = line.split("  ", 1)
    require(rel not in seen, f"duplicate checksum: {rel}")
    seen.add(rel)
    file = PACKAGE / PurePosixPath(rel)
    require(file.is_file() and sha(file) == digest, f"checksum mismatch: {rel}")
require("SHA256SUMS.txt" not in seen, "self checksum")

with zipfile.ZipFile(ZIP) as archive:
    require(archive.testzip() is None, "ZIP CRC")
    infos = archive.infolist()
    require(len(infos) == 21, "ZIP file count")
    for info in infos:
        pure = PurePosixPath(info.filename)
        require(not pure.is_absolute() and ".." not in pure.parts, "ZIP path")
        require(pure.parts[0] == TOP, "ZIP top directory")
        rel = PurePosixPath(*pure.parts[1:])
        require((PACKAGE / rel).read_bytes() == archive.read(info), f"ZIP byte mismatch: {rel}")

module = load_installer(PACKAGE / "install_g17b0_source.py", "g17b0_apply_delivery")
with tempfile.TemporaryDirectory(prefix="g17b0_source_apply_delivery_") as td:
    source = Path(td) / "source"
    make_fixture(source, module)
    command = [sys.executable, str(PACKAGE / "run_g17b0_source_apply.py"), str(source)]
    first = subprocess.run(command, capture_output=True, text=True, env=ENV)
    require(first.returncode == 0, first.stdout + first.stderr)
    require("G17B0_SOURCE_APPLY_CHANGED_FILES=2" in first.stdout, "first apply changes")
    require("G17B0_SOURCE_APPLY_PASS=True" in first.stdout, "first apply pass")
    require(sha(source / module.LOADER_REL) == module.POST_LOADER_SHA256, "loader post")
    require(sha(source / module.TARGET_REL) == module.PAYLOAD_SHA256, "payload post")
    backup = Path(str(source / module.LOADER_REL) + module.BACKUP_SUFFIX)
    require(sha(backup) == module.PRE_LOADER_SHA256, "backup")

    second = subprocess.run(command, capture_output=True, text=True, env=ENV)
    require(second.returncode == 0, second.stdout + second.stderr)
    require("G17B0_SOURCE_APPLY_CHANGED_FILES=0" in second.stdout, "idempotent apply")

    rollback = subprocess.run(
        [sys.executable, str(PACKAGE / "install_g17b0_source.py"), "--rollback", str(source)],
        capture_output=True, text=True, env=ENV)
    require(rollback.returncode == 0, rollback.stdout + rollback.stderr)
    require(sha(source / module.LOADER_REL) == module.PRE_LOADER_SHA256, "rollback loader")
    require(not (source / module.TARGET_REL).exists(), "rollback target")

with tempfile.TemporaryDirectory(prefix="g17b0_source_apply_negative_") as td:
    base = Path(td)
    negative_package = base / "package"
    shutil.copytree(PACKAGE, negative_package)
    (negative_package / "G17B0_APPLY_APPROVED.txt").unlink()
    negative_module = load_installer(negative_package / "install_g17b0_source.py", "g17b0_apply_negative")
    source = base / "source"
    make_fixture(source, negative_module)
    before = (sha(source / negative_module.LOADER_REL), (source / negative_module.TARGET_REL).exists())
    failed = subprocess.run(
        [sys.executable, str(negative_package / "run_g17b0_source_apply.py"), str(source)],
        capture_output=True, text=True, env=ENV)
    require(failed.returncode != 0 and "approval marker missing" in failed.stdout, "missing approval fail")
    after = (sha(source / negative_module.LOADER_REL), (source / negative_module.TARGET_REL).exists())
    require(before == after, "negative fixture changed source")

print("G17B0_SOURCE_APPLY_ZIP=PASS")
print("G17B0_SOURCE_APPLY_INTERNAL_CHECKSUMS=PASS_20")
print("G17B0_SOURCE_APPLY_ROUNDTRIP=PASS")
print("G17B0_SOURCE_APPLY_IDEMPOTENT=PASS")
print("G17B0_SOURCE_APPLY_MISSING_APPROVAL_NEGATIVE=PASS")
print("G17B0_SOURCE_APPLY_DELIVERY=FINAL_PASS")
