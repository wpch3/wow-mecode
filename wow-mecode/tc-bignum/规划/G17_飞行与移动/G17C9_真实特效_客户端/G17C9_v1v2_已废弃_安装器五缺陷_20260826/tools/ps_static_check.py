#!/usr/bin/env python3
"""Static PowerShell syntax guard for our generated PS1 installers.

Catches the exact class of bug that broke G17-C1 v7 on the user's machine:
a backslash before a double quote INSIDE a double-quoted string
(e.g.  $x = "a \\"b\\"" ) - PowerShell has no C-style escapes; only backtick
or single-quoted strings are valid.  It also reports unbalanced braces.

Usage: python ps_static_check.py <file.ps1>...
"""
from __future__ import annotations
import sys
from pathlib import Path


def check(path: Path) -> list[str]:
    errors: list[str] = []
    text = path.read_text(encoding="utf-8", errors="replace")
    in_single = False
    in_double = False
    i = 0
    n = len(text)
    line = 1
    while i < n:
        c = text[i]
        if c == "\n":
            line += 1
            i += 1
            continue
        if c == "'":
            if in_single:
                # '' is an escaped single quote inside single-quoted strings
                if i + 1 < n and text[i + 1] == "'":
                    i += 2
                    continue
                in_single = False
            elif not in_double:
                in_single = True
            i += 1
            continue
        if c == '"':
            if in_double:
                in_double = False
            elif not in_single:
                in_double = True
            i += 1
            continue
        if (in_double and c == "\\" and i + 1 < n and text[i + 1] == '"'
                and (i == 0 or text[i - 1] != "\\")):
            # A SINGLE backslash immediately before a double quote inside a
            # double-quoted string: PowerShell treats it as a literal
            # backslash, so the quote closes the string early and the rest
            # becomes unexpected tokens (exactly the v7 parser error).
            #  \\" (two backslashes then quote) is legal, so skip those.
            errors.append(f"line {line}: single-backslash-quote inside "
                          f"double-quoted string (PS has no \\\" escape)")
            i += 2
            continue
        i += 1
    if text.count("{") != text.count("}"):
        errors.append("unbalanced braces")
    return errors


def main() -> int:
    rc = 0
    for arg in sys.argv[1:]:
        errs = check(Path(arg))
        if errs:
            rc = 1
            for e in errs:
                print(f"PS_STATIC_ERROR {arg}: {e}")
        else:
            print(f"PS_STATIC_OK {arg}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
