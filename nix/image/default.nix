# The container image: the arm64 base system without a console login, the frameworks command-line
# tools link most, kido-agent, the release KC, and xnu-loader's arm64-virt EFI app on the ESP.
{ toolchain, darwin, kido, revision, nixpkgsRev, lastModifiedDate }:

let
  inherit (toolchain) pkgs;
  inherit (darwin)
    kernelSource
    certificatesBundleBuild
    coreServicesArm64Build
    curlArm64Build
    expatArm64eBuild
    iconvArm64eBuild
    kernelArm64VirtBuild
    kextsArm64Build
    libeditArm64Build
    libresslArm64eBuild
    libzDylibArm64Build
    libbz2DylibArm64eBuild
    liblzmaDylibArm64eBuild
    libutilArm64eBuild
    ncursesDylibArm64Build
    nghttp2Arm64eBuild
    securityArm64Build
    sqliteDylibArm64eBuild
    splitBaseSystemArm64VirtMinimalRelease
    systemConfigurationArm64Build
    toyboxArm64Build
    zshArm64Build
    ;
  agent = pkgs.callPackage ./agent.nix { inherit kido; };
  notices = pkgs.callPackage ./notices.nix {
    inherit kido revision nixpkgsRev;
    agent = agent.agent;
    libressl = libresslArm64eBuild;
    pinned = {
      curl = curlArm64Build;
      expat = expatArm64eBuild;
      libiconv = iconvArm64eBuild;
      nghttp2 = nghttp2Arm64eBuild;
      sqlite = sqliteDylibArm64eBuild;
    };
  };
  # every image has this root filesystem uuid: uuid5 of https://github.com/kido-sh/darwin#root
  rootUUID = "260a2575-f263-5945-9d65-ddf2d4e215b2";
  loaderPkgs = pkgs.pkgsCross.aarch64-multiplatform;
  # xnu-loader as an arm64 Linux Image, for boot loaders that take a kernel (QEMU -kernel, VZLinuxBootLoader)
  loader = loaderPkgs.callPackage ../../loader/kernel/arm64.nix { platform = "generic"; };
  # the EFI application on the ESP
  xnuLoaderArm64 = loaderPkgs.callPackage ../../loader { arch = "aarch64"; platform = "qemuvirt"; };
  kcArm64ReleaseBuild = pkgs.callPackage ../toolchain/kc-arm64.nix {
    kernel = kernelArm64VirtBuild;
    inherit kernelSource;
    kexts = kextsArm64Build;
    kcTools = pkgs.callPackage ../../kc-tools { };
  };
  # SystemConfiguration for what links it, without configd, its plugins or ipconfig: a container's
  # address comes from its boot-args, and IPConfiguration would take en0 over with DHCP
  systemConfigurationNoConfigdArm64 = pkgs.runCommand "kido-systemconfiguration-no-configd-0.1" { } ''
    cp -a ${systemConfigurationArm64Build} $out
    chmod -R u+w $out
    rm -f $out/System/Library/LaunchDaemons/com.apple.configd.plist
    rm -r $out/usr/libexec/configd $out/usr/sbin/ipconfig $out/System/Library/SystemConfiguration
  '';
  # launchd's container settings: any uid, with or without a passwd entry, uses the system
  # bootstrap namespace, as no login session ever starts a per-user launchd in a container;
  # /tmp survives a restart and a commit, as in a container's writable layer
  containerLaunchdSettings = pkgs.runCommand "kido-container-launchd-settings-0.1" { } ''
    mkdir -p $out/private/var/db
    touch $out/private/var/db/.launchd_no_per_user $out/private/var/db/.launchd_keep_tmp
  '';
  # no console login: a container is driven through its own agent, and without a console UART
  # (Virtualization.framework) the login's zsh spins in the console path and blocks shutdown
  containerBaseSystemArm64 = pkgs.runCommand "kido-basesystem-arm64-virt-container-0.1" { } ''
    cp -a ${splitBaseSystemArm64VirtMinimalRelease} $out
    chmod -R u+w $out
    rm $out/System/Library/LaunchDaemons/sh.kido.console-login.plist $out/usr/libexec/kido-console-login
    # libSystem's build-time staging for launchd and launchctl: XPC headers and static archives
    rm -r $out/pd-xpc-dev
  '';
  # the minimal release plus the frameworks command-line tools link most (adb: Security; curl:
  # SystemConfiguration; anything with gettext: CoreServices), for running macOS CLIs in a VM
  rootfs = pkgs.callPackage ./rootfs.nix {
    baseSystem = containerBaseSystemArm64;
    extraPackages = [
      zshArm64Build iconvArm64eBuild toyboxArm64Build
      securityArm64Build systemConfigurationNoConfigdArm64 coreServicesArm64Build
      libzDylibArm64Build libbz2DylibArm64eBuild liblzmaDylibArm64eBuild libutilArm64eBuild
      ncursesDylibArm64Build libeditArm64Build curlArm64Build
      nghttp2Arm64eBuild expatArm64eBuild sqliteDylibArm64eBuild
      certificatesBundleBuild containerLaunchdSettings
      agent.root
      libresslArm64eBuild
    ];
    inherit notices;
  };
  disk = pkgs.callPackage ./disk.nix {
    inherit rootfs;
    kc = kcArm64ReleaseBuild;
    xnuLoader = xnuLoaderArm64;
    efiBinary = "BOOTAA64.EFI";
    espMB = 64;
    # an 8 GiB disk: the GPT's 1 MiB at each end, the ESP and the root
    rootMB = 8126;
    bootArgs = "serial=3 kido_rtc_wait=5 kido_rtc_delay=0 kido_quiet_wait=0";
    inherit rootUUID;
    # uuid5 of https://github.com/kido-sh/darwin#disk, #esp, #esp-volume and #hash-seed
    diskGUID = "da60a1e3-7a76-5799-a44a-22428bf733df";
    espGUID = "3eacb7ee-b798-5edd-8fd9-d73e6d9772c3";
    espVolumeId = "ac6a907e";
    hashSeed = "27d20f07-54f7-5b53-a57d-35cb5beb21f3";
  };
  # the commit time, RFC 3339 in UTC
  created = with pkgs.lib; let d = lastModifiedDate; in
    "${substring 0 4 d}-${substring 4 2 d}-${substring 6 2 d}T${substring 8 2 d}:${substring 10 2 d}:${substring 12 2 d}Z";
  image = pkgs.callPackage ./bake.nix {
    kc = kcArm64ReleaseBuild;
    inherit loader disk rootUUID created;
  };
  oci = pkgs.callPackage ./oci.nix { inherit image rootUUID created revision; };

in
{
  inherit loader rootfs disk image oci;
  inherit (agent) agent;
  kc = kcArm64ReleaseBuild;
}
