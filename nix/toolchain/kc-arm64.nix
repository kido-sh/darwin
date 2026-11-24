{ stdenv
, lib
, kcTools
, kernel
, kernelSource
, kexts
, kcBase ? "fffffe0006000000"
}:

let
  baseKexts = [
    "IOPCIFamily"
    "corecrypto"
    "pthread"
    "amfi"
    "Sandbox"
    "PDArmPlatformExpert"
    "PDArmPCI"
    "PDEcamPCI"
    "IOStorageFamily"
    "ext4"
    "AppleFileSystemDriver"
    "Ext4FileSystemDriver"
    "IOVirtIOFamily"
    "IONetworkingFamily"
    "IOVirtIOBlock"
    "IOVirtIONet"
    "VirtioFS"
    "PDWatchdog"
    "PDDiskImages2"
    "PDLifs"
    "PDKeyStore"
  ];
  darwinKernelVersion = "25.5.0";
  darwinKernelVersionShort = "25.5";
in
stdenv.mkDerivation {
  pname = "kido-kc-arm64";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    # The codeless KPI bundles live in the XNU source tree, not the kernel
    # derivation's output; without them com.apple.kpi.bsd and friends are
    # missing and any kext depending on them fails dependency resolution.
    KERNEL_EXTS=${kernelSource}/src/Kernel/xnu/config/System.kext/PlugIns
    KEXTS=${kexts}/System/Library/Extensions

    KERNEL_BIN=$(ls "${kernel}"/System/Library/Kernels/kernel* | head -n1)

    codeless=()
    CODELESS_DIR="$TMPDIR/kido-codeless"
    mkdir -p "$CODELESS_DIR"
    for p in "$KERNEL_EXTS"/*.kext; do
      name=$(basename "$p")
      mkdir -p "$CODELESS_DIR/$name"
      sed \
        -e 's/###KERNEL_VERSION_LONG###/${darwinKernelVersion}/g' \
        -e 's/###KERNEL_VERSION_SHORT###/${darwinKernelVersionShort}/g' \
        "$p/Info.plist" > "$CODELESS_DIR/$name/Info.plist"
      codeless+=( -codeless "$CODELESS_DIR/$name" )
    done

    # Must match ARM64_KC_BASE in xnu's MakeInc.def.in: XNU locates the
    # collection header at VM_KERNEL_LINK_ADDRESS, which is compiled in.
    ${kcTools}/bin/kc-builder \
      -kc-base ${kcBase} \
      -kernel "$KERNEL_BIN" \
      ${lib.concatMapStrings (k: "-kext \"$KEXTS/${k}.kext\" \\\n      ") baseKexts}\
      "''${codeless[@]}" \
      -o kernel
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp kernel $out/kernel
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin arm64-virt boot kernel collection (kernel + kexts fileset), assembled by kc-tools' kc-builder";
    platforms = platforms.unix;
  };
}
