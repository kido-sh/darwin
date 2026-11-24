# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# The install name and versions a recipe links a dylib with, read from the header of its export list.
{ lib }:

{
  idFlags = exports:
    let
      line = builtins.elemAt (lib.splitString "\n" (builtins.readFile exports)) 1;
      m = builtins.match "# install name ([^,]*), current version ([^,]*), compatibility version (.*)" line;
    in
    assert m != null;
    "-install_name ${builtins.elemAt m 0} -current_version ${builtins.elemAt m 1} -compatibility_version ${builtins.elemAt m 2}";
}
