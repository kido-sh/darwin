#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
"""Print the export list of a library in the macOS SDK, as the recipes' .exports files hold it.

Reads a text-based stub (.tbd, version 4) and prints the symbols it exports for one target, sorted,
after two header lines that name the SDK, the stub, the install name and the versions.

usage: tbd-exports.py SDK STUB [TARGET]
  SDK     the macOS SDK directory, as `xcrun --show-sdk-path` prints it
  STUB    the stub's path inside the SDK, such as usr/lib/libexpat.1.tbd
  TARGET  the target whose symbols to list (default arm64e-macos)
"""
import json
import os
import re
import sys

KINDS = ("symbols", "weak-symbols", "thread-local-symbols")
KEY = re.compile(r"^( *)(- +)?([a-z-]+):\s*(.*)$")


def entries(text):
    lines = text.split("\n...")[0].splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        i += 1
        m = KEY.match(line)
        if not m:
            continue
        value = m.group(4)
        while value.count("[") > value.count("]") and i < len(lines):
            value += " " + lines[i].strip()
            i += 1
        yield len(m.group(1)), bool(m.group(2)), m.group(3), value.strip()


def items(value):
    if not value.startswith("["):
        return value.strip("'\"")
    return [v.strip().strip("'\"") for v in value.strip("[]").split(",") if v.strip()]


def parse(text):
    top = {"current-version": "1.0", "compatibility-version": "1.0"}
    blocks, section, block = [], None, None
    for indent, dash, key, value in entries(text):
        if indent == 0 and not dash:
            section = key
            if value:
                top[key] = items(value)
            continue
        if dash:
            block = {"section": section}
            blocks.append(block)
        if block is not None:
            block[key] = items(value)
    return top, blocks


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__.strip())
    sdk, stub = sys.argv[1], sys.argv[2]
    target = sys.argv[3] if len(sys.argv) == 4 else "arm64e-macos"
    with open(os.path.join(sdk, "SDKSettings.json")) as f:
        version = json.load(f)["Version"]
    with open(os.path.join(sdk, stub)) as f:
        top, blocks = parse(f.read())
    symbols = set()
    for block in blocks:
        if block["section"] == "exports" and target in block.get("targets", []):
            for kind in KINDS:
                symbols.update(block.get(kind, []))
    if not symbols:
        sys.exit(f"{stub} exports nothing for {target}")
    print(f"# The symbols the macOS {version} SDK's {os.path.basename(stub)} exports")
    print(f"# install name {top['install-name']}, current version {top['current-version']}, "
          f"compatibility version {top['compatibility-version']}")
    for symbol in sorted(symbols):
        print(symbol)


if __name__ == "__main__":
    main()
