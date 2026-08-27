#!/usr/bin/env python3
"""G17Extract: extract Interface files from WoW 3.3.5a client MPQs.

Extracts the Blizzard FrameXML/Lua source code from the client's MPQ archives
so we can study how VehicleMenuBar, ActionBar, and other UI elements work.
This is the "crawl the client" tool the user requested.

Usage: python3 g17_extract.py --client-root "D:\\WOW" --output "D:\\G17_extracted"
"""
from __future__ import annotations
import argparse
import hashlib
import struct
import sys
from pathlib import Path


def find_mpqs(client_root: Path):
    """Find all MPQ files in the client's Data directory."""
    data_dir = client_root / "Data"
    mpqs = []
    for mpq in sorted(data_dir.glob("*.MPQ")) + sorted(data_dir.glob("*.mpq")):
        mpqs.append(mpq)
    # Also locale-specific MPQs
    for locale_dir in data_dir.iterdir():
        if locale_dir.is_dir():
            for mpq in sorted(locale_dir.glob("*.MPQ")) + sorted(locale_dir.glob("*.mpq")):
                mpqs.append(mpq)
    return mpqs


def list_mpq_contents(mpq_path: Path, mpqcli: Path):
    """List files in an MPQ using mpqcli."""
    import subprocess
    result = subprocess.run(
        [str(mpqcli), "list", str(mpq_path)],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        return []
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def extract_file(mpq_path: Path, file_path: str, output_dir: Path, mpqcli: Path):
    """Extract a single file from an MPQ."""
    import subprocess
    result = subprocess.run(
        [str(mpqcli), "extract", "--output", str(output_dir), "--keep",
         "--file", file_path, str(mpq_path)],
        capture_output=True, text=True
    )
    return result.returncode == 0


def main():
    ap = argparse.ArgumentParser(description="Extract Interface files from WoW 3.3.5a MPQs")
    ap.add_argument("--client-root", required=True, help="Path to WoW client root (e.g. D:\\WOW)")
    ap.add_argument("--output", required=True, help="Output directory for extracted files")
    ap.add_argument("--mpqcli", default="", help="Path to mpqcli.exe (optional)")
    ap.add_argument("--filter", default="Interface", help="File path filter (default: Interface)")
    ap.add_argument("--list-only", action="store_true", help="Only list files, don't extract")
    args = ap.parse_args()

    client_root = Path(args.client_root)
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    # Find mpqcli
    mpqcli = Path(args.mpqcli) if args.mpqcli else Path(__file__).parent / "mpqcli-windows-amd64.exe"
    if not mpqcli.exists():
        # Search common locations
        for candidate in [
            Path(__file__).parent / "mpqcli-windows-amd64.exe",
            Path(__file__).parent / "third_party" / "mpqcli-windows-amd64.exe",
            client_root / "mpqcli.exe",
        ]:
            if candidate.exists():
                mpqcli = candidate
                break
        else:
            print("ERROR: mpqcli not found. Place mpqcli-windows-amd64.exe next to this script.")
            return 1

    print(f"Client root: {client_root}")
    print(f"Output dir: {output_dir}")
    print(f"mpqcli: {mpqcli}")
    print()

    # Find all MPQs
    mpqs = find_mpqs(client_root)
    if not mpqs:
        print("ERROR: No MPQ files found in Data directory")
        return 1

    print(f"Found {len(mpqs)} MPQ files:")
    for mpq in mpqs:
        print(f"  {mpq.name} ({mpq.stat().st_size:,} bytes)")
    print()

    # Key files we want to extract
    interesting_patterns = [
        "Interface\\FrameXML\\VehicleMenuBar",
        "Interface\\FrameXML\\ActionBar",
        "Interface\\FrameXML\\MainMenuBar",
        "Interface\\FrameXML\\BonusActionBar",
        "Interface\\FrameXML\\PetActionBar",
        "Interface\\FrameXML\\ UIParent",
        "Interface\\FrameXML\\SecureStateDriver",
        "Interface\\FrameXML\\SecureTemplates",
    ]

    total_extracted = 0

    for mpq in mpqs:
        print(f"Scanning {mpq.name}...")
        files = list_mpq_contents(mpq, mpqcli)

        interface_files = [f for f in files if args.filter.lower() in f.lower()]

        if not interface_files:
            print(f"  No {args.filter} files found")
            continue

        print(f"  Found {len(interface_files)} {args.filter} files")

        if args.list_only:
            for f in interface_files[:50]:
                print(f"    {f}")
            if len(interface_files) > 50:
                print(f"    ... and {len(interface_files) - 50} more")
            continue

        # Extract interesting files
        for f in interface_files:
            # Check if it matches our patterns
            should_extract = False
            for pattern in interesting_patterns:
                if pattern.replace("\\\\", "\\").lower() in f.lower():
                    should_extract = True
                    break

            # Also extract all .lua and .xml files in FrameXML
            if f.lower().endswith((".lua", ".xml")) and "framexml" in f.lower():
                should_extract = True

            if should_extract:
                if extract_file(mpq, f, output_dir, mpqcli):
                    total_extracted += 1
                    print(f"    EXTRACTED: {f}")
                else:
                    print(f"    FAILED: {f}")

    print()
    print(f"Total files extracted: {total_extracted}")
    print(f"Output directory: {output_dir}")

    if total_extracted > 0:
        print()
        print("Key files to study for vehicle UI:")
        print("  Interface/FrameXML/VehicleMenuBar.lua - Vehicle action bar logic")
        print("  Interface/FrameXML/VehicleMenuBar.xml - Vehicle action bar layout")
        print("  Interface/FrameXML/ActionBarController.lua - Bar switching logic")
        print("  Interface/FrameXML/SecureTemplates.lua - Secure button templates")
        print()
        print("To understand why the player's bars hide on vehicle:")
        print("  grep -r 'VehicleMenuBar\\|HasVehicleUI\\|MainMenuBar' " + str(output_dir))

    return 0


if __name__ == "__main__":
    sys.exit(main())
