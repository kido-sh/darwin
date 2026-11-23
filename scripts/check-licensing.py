#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
"""Check LICENSING.md and LICENSES/ against the tree.

Fails when a file is matched by no path of the mapping table (the table under "## Which license
covers which files"), or when an SPDX id has no text in LICENSES/. SPDX ids are read from the
backticked expressions in every table cell but a mapping row's first, and from every file's
SPDX-License-Identifier line.

usage: check-licensing.py [--root DIR] [--files LIST]
  --files LIST  check these paths (one per line, relative to the root) instead of every tracked file
"""
import argparse
import os
import re
import subprocess
import sys

MAPPING = "## Which license covers which files"
TICKS = re.compile(r"`([^`]+)`")
SPDX_LINE = re.compile(r"SPDX-License-Identifier:\s*([A-Za-z0-9 ().+-]+?)\s*(?:\*/|-->|$)", re.M)
OPERATORS = {"AND", "OR", "WITH"}


def glob_to_regex(glob):
    out, i = "", 0
    while i < len(glob):
        c = glob[i]
        if glob.startswith("**/", i):
            out += "(?:.*/)?"
            i += 3
            continue
        if glob.startswith("**", i):
            out += ".*"
            i += 2
            continue
        if c == "*":
            out += "[^/]*"
        elif c == "?":
            out += "[^/]"
        elif c == "{":
            j = glob.index("}", i)
            out += "(?:" + "|".join(re.escape(a) for a in glob[i + 1:j].split(",")) + ")"
            i = j
        else:
            out += re.escape(c)
        i += 1
    return re.compile(out + r"\Z")


def spdx_ids(expr):
    return {t for t in re.split(r"[\s()]+", expr) if t and t not in OPERATORS}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    ap.add_argument("--files")
    args = ap.parse_args()
    root = args.root
    texts = {f[:-4] for f in os.listdir(os.path.join(root, "LICENSES")) if f.endswith(".txt")}
    rows, named, section = [], set(), None
    with open(os.path.join(root, "LICENSING.md"), encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("## "):
                section = line.strip()
            if not line.startswith("|") or re.match(r"^\|[\s|:-]+\|?\s*$", line):
                continue
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if section == MAPPING:
                rows.extend(glob_to_regex(g) for g in TICKS.findall(cells[0]))
                cells = cells[1:]
            for cell in cells:
                for expr in TICKS.findall(cell):
                    named |= spdx_ids(expr)
    if args.files:
        with open(args.files, encoding="utf-8") as fh:
            files = [l.strip() for l in fh if l.strip()]
    else:
        files = subprocess.run(["git", "-C", root, "ls-files", "-z"], check=True,
                               capture_output=True).stdout.decode().split("\0")
        files = [f for f in files if f]
    used = set()
    for f in files:
        full = os.path.join(root, f)
        if os.path.islink(full) or not os.path.isfile(full):
            continue
        with open(full, "rb") as fh:
            head = fh.read(8192).decode("utf-8", "replace")
        for expr in SPDX_LINE.findall(head):
            used |= spdx_ids(expr)
    missing = sorted((named | used) - texts)
    uncovered = [f for f in files if not any(r.match(f) for r in rows)]
    unused = sorted(texts - named - used)
    for i in missing:
        print("no text in LICENSES/ for " + i)
    for f in uncovered:
        print("not covered by LICENSING.md: " + f)
    for i in unused:
        print("note: LICENSES/%s.txt is not named in LICENSING.md or any header" % i)
    print("%d files, %d rows, %d license texts, %d missing, %d uncovered" % (
        len(files), len(rows), len(texts), len(missing), len(uncovered)))
    return 1 if missing or uncovered else 0


if __name__ == "__main__":
    sys.exit(main())
