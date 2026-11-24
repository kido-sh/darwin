# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
{ lib, runCommand, llvmPackages }:

let
  dir = ../../darwin/src/Kernel/Extensions/VirtioFS;
  src = lib.fileset.toSource {
    root = dir;
    fileset = lib.fileset.unions [ (dir + "/fuse.h") (dir + "/fuse_proto.h") (dir + "/fuse_proto.c") (dir + "/fuzz") ];
  };
in
runCommand "virtiofs-fuzz" { nativeBuildInputs = [ llvmPackages.clang ]; } ''
  clang -O1 -g -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all -DENOATTR=93 \
    -I ${src} ${src}/fuse_proto.c ${src}/fuzz/fuzz_proto.c -o fuzz_proto
  mkdir corpus
  ./fuzz_proto -max_total_time=60 -print_final_stats=1 corpus
  touch $out
''
