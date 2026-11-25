# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# Host unit tests of the virtio-fs reply parsers, kept out of the kernel build:
#   cmake -S darwin/src/Kernel/Extensions/VirtioFS/tests -B build/virtiofs-tests
#   cmake --build build/virtiofs-tests && ctest --test-dir build/virtiofs-tests
cmake_minimum_required(VERSION 3.20)
project(virtiofs_tests C)

include(CheckSymbolExists)
check_symbol_exists(ENOATTR errno.h HAVE_ENOATTR)
enable_testing()

add_executable(test_proto test_proto.c ../fuse_proto.c)
target_include_directories(test_proto PRIVATE ..)
set_property(TARGET test_proto PROPERTY C_STANDARD 11)
if(NOT HAVE_ENOATTR)
  target_compile_definitions(test_proto PRIVATE ENOATTR=93)
endif()
add_test(NAME virtiofs_proto COMMAND test_proto)
