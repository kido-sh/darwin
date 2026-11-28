# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# Modular kl system package recipes for KidoDarwin, parameterized by target
# architecture (arm64 = strictly portable ARMv8.0-A, arm64e = Apple Silicon PAC).
{ toolchain, darwin }:

let
  inherit (toolchain)
    pkgs
    appleSdk
    nativeLd
    darwinCrossToolchain
    ;
  lib = pkgs.lib;

  mkSet = arch:
    let
      libffi = pkgs.callPackage ./libffi.nix {
        inherit darwinCrossToolchain nativeLd appleSdk arch;
        libSystem = darwin.libSystemArm64Build;
        inherit (pkgs) libffiReal;
      };
      libxml2 = pkgs.callPackage ./libxml2.nix {
        inherit darwinCrossToolchain nativeLd appleSdk arch;
        libSystem = darwin.libSystemArm64Build;
        libz = darwin.libzDylibArm64Build;
        zlibDev = pkgs.zlib.dev;
        libiconv = darwin.iconvArm64eBuild;
        inherit (pkgs) libxml2;
      };
      python3 = pkgs.callPackage ./python3.nix {
        inherit darwinCrossToolchain nativeLd appleSdk arch;
        libSystem = darwin.libSystemArm64Build;
        libz = darwin.libzDylibArm64Build;
        zlibDev = pkgs.zlib.dev;
        libbz2 = darwin.libbz2DylibArm64eBuild;
        bzip2Dev = pkgs.bzip2.dev;
        liblzma = darwin.liblzmaDylibArm64eBuild;
        xzDev = pkgs.xz.dev;
        sqlite = darwin.sqliteDylibArm64eBuild;
        klLibffi = libffi;
        inherit (pkgs) python3;
      };
      standard = pkgs.callPackage ./standard.nix {
        inherit darwinCrossToolchain arch libffi libxml2 python3;
      };
      packagesList = [ libffi libxml2 python3 standard ];
      catalogJson = builtins.toJSON {
        inherit arch;
        packages = map (p: p.kl // { dir = "packages/${p.kl.name}"; }) packagesList;
      };
      all = pkgs.runCommand "kido-kl-all-${arch}" {
        passthru = {
          inherit arch libffi libxml2 python3 standard;
        };
      } ''
        mkdir -p "$out/packages"
        ${lib.concatMapStringsSep "\n" (p: ''
          ln -s "${p}" "$out/packages/${p.kl.name}"
        '') packagesList}
        cat > "$out/manifest.json" <<'EOF'
${catalogJson}
EOF
        chmod 644 "$out/manifest.json"
      '';
    in
    {
      inherit libffi libxml2 python3 standard all;
    };
in
{
  arm64 = mkSet "arm64";
  arm64e = mkSet "arm64e";
}
