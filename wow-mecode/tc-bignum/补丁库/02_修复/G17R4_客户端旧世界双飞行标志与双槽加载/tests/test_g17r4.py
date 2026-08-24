from __future__ import annotations

import hashlib
import struct
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STOCK = ROOT / "original/DBFilesClient/AreaTable.stock.dbc"
R3 = ROOT / "original/DBFilesClient/AreaTable.R3.dbc"
R4 = ROOT / "payload/DBFilesClient/AreaTable.dbc"

HEADER = struct.Struct("<4s4I")
U32 = struct.Struct("<I")
EXPECTED_STOCK = "b0356ff41e5777896509ec52bc68af516b67d82a659dbc47757960aef98b62dd"
EXPECTED_R3 = "214c6935d11b784f0bf5e4855fb756126d9d667d622a346c3124ae748812b6a8"
EXPECTED_R4 = "1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233"
FLYING = 0x00000400
FLIGHT_BOUNDS = 0x00004000
CLIENT_FLY_MASK = FLYING | FLIGHT_BOUNDS


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def layout(data: bytes) -> tuple[int, int, int, int]:
    magic, records, fields, record_size, strings = HEADER.unpack_from(data)
    assert magic == b"WDBC"
    assert (records, fields, record_size) == (2307, 36, 144)
    assert 20 + records * record_size + strings == len(data)
    return records, fields, record_size, strings


def value(data: bytes, row: int, field: int) -> int:
    return U32.unpack_from(data, 20 + row * 144 + field * 4)[0]


def rows(data: bytes) -> dict[int, int]:
    return {value(data, row, 0): row for row in range(2307)}


def main() -> int:
    stock = STOCK.read_bytes()
    r3 = R3.read_bytes()
    r4 = R4.read_bytes()
    assert digest(STOCK) == EXPECTED_STOCK
    assert digest(R3) == EXPECTED_R3
    assert digest(R4) == EXPECTED_R4
    assert layout(stock) == layout(r3) == layout(r4)
    assert len(stock) == len(r3) == len(r4) == 362740

    changed_stock_r3 = []
    changed_r3_r4 = []
    for row in range(2307):
        s = value(stock, row, 4)
        a = value(r3, row, 4)
        b = value(r4, row, 4)
        record_slice = slice(20 + row * 144, 20 + (row + 1) * 144)
        if s != a:
            changed_stock_r3.append(row)
            assert a == (s | FLYING)
        else:
            assert r3[record_slice] == stock[record_slice]
        if a != b:
            changed_r3_r4.append(row)
            assert b == (a | FLIGHT_BOUNDS)
        else:
            assert r4[record_slice] == r3[record_slice]

    assert len(changed_stock_r3) == 948
    assert changed_r3_r4 == changed_stock_r3
    assert stock[20 + 2307 * 144 :] == r3[20 + 2307 * 144 :] == r4[20 + 2307 * 144 :]

    by_id = rows(stock)
    wetlands = by_id[11]
    assert value(stock, wetlands, 1) == 0
    assert value(stock, wetlands, 4) == 0x00000040
    assert value(r3, wetlands, 4) == 0x00000440
    assert value(r4, wetlands, 4) == 0x00004440

    for row in changed_r3_r4:
        assert value(r4, row, 1) in (0, 1)
        assert value(r4, row, 4) & CLIENT_FLY_MASK == CLIENT_FLY_MASK

    tool = ROOT / "tools/patch_g17r4_client_areatable_dbc.py"
    with tempfile.TemporaryDirectory() as td:
        generated = Path(td) / "AreaTable.dbc"
        report = Path(td) / "report.txt"
        import subprocess
        subprocess.run(
            [sys.executable, str(tool), "patch", "--input", str(STOCK), "--output", str(generated), "--report", str(report)],
            check=True,
            stdout=subprocess.PIPE,
            text=True,
        )
        assert generated.read_bytes() == r4
        text = report.read_text(encoding="utf-8")
        assert "CLIENT_FLIGHT_FLAGS_ADDED=0x00000400,0x00004000" in text
        assert "R3_SINGLE_FLAG_0x00000400_INCOMPLETE=True" in text

    print("G17R4_DBC_TESTS=PASS")
    print("R3_TO_R4_CHANGED_ROWS=948")
    print("WETLANDS_FLAGS=0x00000040->0x00000440->0x00004440")
    print(f"R4_AREA_SHA256={EXPECTED_R4}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
