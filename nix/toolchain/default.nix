# The toolchain: the pinned SDK, ld64 from the tree, the arm64 cross compiler, migcom, unifdef and
# compiler-rt.
{ basePkgs }:

let
  # not a path: input, which resolves into the whole tree and carries its hash into every build
  corecryptoSrc = builtins.path { path = ../../darwin/external/corecrypto; name = "corecrypto-source"; };

  # Two stages so the MIG-generated <mach/*.h> can be produced at build
  # time instead of being carried pre-generated: the base SDK has the
  # hand-written headers migcom itself needs, and the final SDK adds
  # what migcom emits from xnu's .defs.
  appleSdkBase = basePkgs.callPackage ./apple-sdk-pinned.nix { };
  sdkMigcom = basePkgs.callPackage ./migcom.nix {
    appleSdk = appleSdkBase;
  };
  appleSdk = basePkgs.callPackage ./apple-sdk-pinned.nix {
    migcom = sdkMigcom;
  };
  pkgs = basePkgs.extend (_: _: { inherit appleSdk; });
  iig = pkgs.callPackage ../../iig-tools { };
  libtapi = pkgs.callPackage ./libtapi.nix { };
  # cctools' ld64, built for the host; lld silently ignores -dylib_file/-image_base/-segaddr
  nativeLd =
    pkgs.callPackage ./native-ld.nix {
      inherit libtapi iig corecryptoSrc;
    };
  # The wrappers pin the target triple, the pinned SDK and the linker. toolchain.nix is built from
  # nixpkgs' llvmPackages_21.
  darwinCrossToolchain = pkgs.callPackage ./toolchain.nix {
    inherit nativeLd;
  };
  nativeUnifdef = pkgs.callPackage ./unifdef.nix { };
  nativeMigcom = pkgs.callPackage ./migcom.nix { };
  hostOtoolBuild = pkgs.callPackage ./host-otool.nix { };
  nativeMesonToolsDir =
    pkgs.runCommand "kido-native-meson-tools" { } (
      let toolsdir = "${hostOtoolBuild}"; in ''
      mkdir -p $out/bin
      ln -s ${toolsdir}/bin/otool $out/bin/otool
      ln -s ${toolsdir}/bin/install_name_tool $out/bin/install_name_tool
    '');
  compilerRt =
    pkgs.callPackage ./compiler-rt.nix {
      inherit darwinCrossToolchain nativeLd;
      nativeMesonTools = nativeMesonToolsDir;
      llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
      llvmVersion = pkgs.llvmPackages_21.llvm.version;
    };
in
{
  inherit
    pkgs
    corecryptoSrc
    appleSdk
    iig
    nativeLd
    darwinCrossToolchain
    nativeUnifdef
    nativeMigcom
    compilerRt
    ;
}
