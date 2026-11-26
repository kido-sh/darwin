# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
import json
import os
import re
import shlex
import subprocess
import sys

build, report = sys.argv[1], sys.argv[2]
sources = ("/Extensions/VirtioFS/", "/Extensions/IOVirtIOFamily/")
extra = (
    "alpha.core.BoolAssignment",
    "alpha.core.CastSize",
    "alpha.core.StackAddressAsyncEscape",
    "alpha.core.TestAfterDivZero",
    "alpha.deadcode.UnreachableCode",
    "alpha.security.ReturnPtrRange",
    "alpha.unix.cstring.BufferOverlap",
    "alpha.unix.cstring.OutOfBounds",
    "nullability",
    "optin.performance.Padding",
    "security.ArrayBound",
    "security.FloatLoopCounter",
)
findings = []
analyzed = set()
for entry in json.load(open(os.path.join(build, "compile_commands.json"))):
    if not any(s in entry["file"] for s in sources):
        continue
    args = entry.get("arguments") or shlex.split(entry["command"])
    command = [args[0], "--analyze", "-Xclang", "-analyzer-output=text"]
    for checker in extra:
        command += ["-Xclang", "-analyzer-checker=" + checker]
    skip = False
    for arg in args[1:]:
        if skip:
            skip = False
        elif arg in ("-o", "-MF", "-MT"):
            skip = True
        elif arg not in ("-c", "-MD") and not arg.startswith("-fuse-ld="):
            command.append(arg)
    result = subprocess.run(command + ["-o", "/dev/null"], cwd=entry["directory"], capture_output=True, text=True)
    analyzed.add(os.path.realpath(os.path.join(entry["directory"], entry["file"])))
    sys.stderr.write(result.stderr)
    findings += [line for line in result.stderr.splitlines() if re.match(r"\S+:\d+:\d+: (warning|error): ", line)]
    if result.returncode:
        findings.append(f"{entry['file']}: the analyzer exited with {result.returncode}")
findings += [f"nothing under {s} was analyzed" for s in sources if not any(s in path for path in analyzed)]
for directory in sorted({os.path.dirname(path) for path in analyzed}):
    for name in sorted(os.listdir(directory)):
        path = os.path.join(directory, name)
        if name.endswith((".c", ".cpp")) and path not in analyzed:
            findings.append(f"{path}: not analyzed, the build's compile commands do not list it")
with open(report, "w") as f:
    f.write("".join(line + "\n" for line in findings))
print(f"analyzed {len(analyzed)} files, {len(findings)} findings")
sys.exit(1 if findings else 0)
