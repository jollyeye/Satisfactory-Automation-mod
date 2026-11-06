"""Checks the mod's C++ source for mistakes the compiler only reports on the Windows build:
string literals broken across lines, and non-ASCII characters.

Usage (from the repo root): python Tools/check_source.py
Run it before every push. Exits with 1 if something is wrong.
"""
import glob
import re
import sys

problems = 0
for path in sorted(glob.glob("Source/**/*.h", recursive=True) + glob.glob("Source/**/*.cpp", recursive=True)):
    with open(path, encoding="utf-8", errors="replace") as source:
        lines = source.read().split("\n")
    in_block_comment = False
    for number, line in enumerate(lines, 1):
        if any(ord(c) > 127 for c in line):
            print(f"{path}:{number}: non-ASCII character")
            problems += 1
        stripped = line.strip()
        if in_block_comment:
            if "*/" in stripped:
                in_block_comment = False
            continue
        if stripped.startswith("/*") and "*/" not in stripped:
            in_block_comment = True
            continue
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        code = re.sub(r"\\.", "", line)
        code = re.sub(r"'[^']'", "", code)
        code = code.split("//")[0] if code.count('"') == 0 else code
        if code.count('"') % 2 == 1:
            print(f"{path}:{number}: string not closed on its line")
            problems += 1
        # UE checks Printf formats at compile time: the format must be a TEXT("...") literal.
        if re.search(r"Printf\(\s*(?!TEXT\()[^\s)]", line):
            print(f"{path}:{number}: Printf format is not a TEXT() literal")
            problems += 1

print("check_source: OK" if problems == 0 else f"check_source: {problems} problem(s)")
sys.exit(1 if problems else 0)
