# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
{ lib, runCommand, cmake, llvmPackages }:

let
  dir = ../../darwin/src/Kernel/Extensions/VirtioFS;
  src = lib.fileset.toSource {
    root = dir;
    fileset = lib.fileset.unions [ (dir + "/fuse.h") (dir + "/fuse_proto.h") (dir + "/fuse_proto.c") (dir + "/tests") ];
  };
in
runCommand "virtiofs-unit" { nativeBuildInputs = [ cmake llvmPackages.clang ]; } ''
  cmake -S ${src}/tests -B build -DCMAKE_C_COMPILER=clang \
    -DCMAKE_C_FLAGS="-g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all"
  cmake --build build
  ctest --test-dir build --output-on-failure
  touch $out
''
