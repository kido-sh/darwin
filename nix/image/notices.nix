# /usr/share/doc/kido: where the image's source is, NOTICE, the texts of LICENSES/ (licenses/SPDX), and
# each component's own license file from this tree or from the third-party source it is built from.
{ runCommand
, lib
, go
, unzip
, libiconvReal
, zsh
, ncurses
, curl
, libedit
, toybox
, icu
, zlib
, bzip2
, xz
, libffiReal
, libxml2
, openssl
, libressl
, sqlite
, agent
, kido
, nixpkgsRev
, revision
}:

let
  tree = ../..;
  # component, then the license files of the tree it ships from
  fromTree = {
    xnu = [
      "darwin/src/Kernel/xnu/APPLE_LICENSE"
      "darwin/src/Kernel/xnu/osfmk/conf/copyright.cmu"
      "darwin/src/Kernel/xnu/osfmk/conf/copyright.osf"
      "darwin/src/Kernel/xnu/security/conf/copyright.nai"
    ];
    PureDarwin = [ "darwin/PUREDARWIN_LICENSE.txt" ];
    corecrypto = [ "darwin/external/corecrypto/LICENSE" ];
    xnu-loader = [ "loader/LICENSE" ];
    kc-tools = [ "kc-tools/LICENSE" ];
    iig-tools = [ "iig-tools/LICENSE" ];
    CoreFoundation = [ "darwin/src/Frameworks/CoreFoundation/LICENSE" ];
    Foundation = [ "darwin/src/Frameworks/Foundation/LICENSE" ];
    Libc = [
      "darwin/src/Libraries/libSystem/libc/APPLE_LICENSE"
      "darwin/src/Libraries/libSystem/libc/regex/FreeBSD/COPYRIGHT"
    ];
    openlibm = [ "darwin/src/Libraries/libSystem/libc/libm/openlibm/LICENSE.md" ];
    libdispatch = [ "darwin/src/Libraries/libSystem/libdispatch/LICENSE" ];
    libplatform = [ "darwin/src/Libraries/libSystem/libplatform/LICENSE" ];
    libcxx = [ "darwin/src/Libraries/libcxx/LICENSE.TXT" ];
    libcxxabi = [ "darwin/src/Libraries/libcxxabi/LICENSE.TXT" ];
    libunwind = [ "darwin/src/Libraries/libunwind/LICENSE.TXT" ];
    mDNSResponder = [ "darwin/src/Libraries/mDNSResponder/LICENSE" ];
    dyld = [ "darwin/src/Libraries/dyld/upstream/APPLE_LICENSE" ];
    objc4 = [ "darwin/src/Libraries/objc4/APPLE_LICENSE" ];
    libresolv = [ "darwin/src/Libraries/libresolv/APPLE_LICENSE" ];
    syslog = [ "darwin/src/Libraries/syslog/APPLE_LICENSE" ];
    architecture = [ "darwin/src/Libraries/architecture/APPLE_LICENSE" ];
    IOKitUser = [ "darwin/src/Libraries/IOKit/iokituser/APPLE_LICENSE" ];
    Security = [
      "darwin/src/Libraries/Security/apple/APPLE_LICENSE"
      "darwin/src/Libraries/Security/apple/OSX/libsecurity_asn1/APPLE_LICENSE"
    ];
    SystemConfiguration = [ "darwin/src/Libraries/SystemConfiguration/APPLE_LICENSE" ];
    libSystemConfiguration = [ "darwin/src/Libraries/libSystemConfiguration/APPLE_LICENSE" ];
    bootp = [ "darwin/src/Libraries/bootp/APPLE_LICENSE" ];
    eap8021x = [ "darwin/src/Libraries/eap8021x/APPLE_LICENSE" ];
    ppp = [ "darwin/src/Libraries/ppp/APPLE_LICENSE" ];
    cctools = [ "darwin/tools/cctools/APPLE_LICENSE" ];
    network_cmds-ping = [ "darwin/src/Userspace/ping/APPLE_LICENSE" ];
    IONetworkingFamily = [ "darwin/src/Kernel/Extensions/IONetworkingFamily/APPLE_LICENSE" ];
    IOStorageFamily = [ "darwin/src/Kernel/Extensions/IOStorageFamily/APPLE_LICENSE" ];
    IOCDStorageFamily = [ "darwin/src/Kernel/Extensions/IOCDStorageFamily/APPLE_LICENSE" ];
    IODVDStorageFamily = [ "darwin/src/Kernel/Extensions/IODVDStorageFamily/APPLE_LICENSE" ];
    IOSCSIArchitectureModelFamily = [ "darwin/src/Kernel/Extensions/IOSCSIArchitectureModelFamily/APPLE_LICENSE" ];
    IOSerialFamily = [ "darwin/src/Kernel/Extensions/IOSerialFamily/APPLE_LICENSE" ];
    IOUSBFamily = [ "darwin/src/Kernel/Extensions/IOUSBFamily/APPLE_LICENSE" ];
    # Mozilla's CA certificates in /etc/ssl/cert.pem are MPL-2.0, the text corecrypto ships
    ca-certificates = [ "darwin/external/corecrypto/LICENSE" ];
  };
  # component, then its source archive and the license file at the archive's top level
  fromSource = [
    [ "libiconv" libiconvReal.src "COPYING.LIB" ]
    [ "zsh" zsh.src "LICENCE" ]
    [ "ncurses" ncurses.src "COPYING" ]
    [ "curl" curl.src "COPYING" ]
    [ "libedit" libedit.src "COPYING" ]
    [ "toybox" toybox.src "LICENSE" ]
    [ "icu" icu.src "LICENSE" ]
    [ "zlib" zlib.src "LICENSE" ]
    [ "bzip2" bzip2.src "LICENSE" ]
    [ "xz" xz.src "COPYING.0BSD" ]
    [ "libffi" libffiReal.src "LICENSE" ]
    [ "libxml2" libxml2.src "Copyright" ]
    [ "openssl" openssl.src "LICENSE.txt" ]
    [ "libressl" libressl.src "COPYING" ]
    [ "sqlite" sqlite.src "LICENSE.md" ]
    [ "go" go.src "LICENSE" ]
  ];
  copyTree = component: files: lib.concatMapStrings (f: ''
    install -Dm644 ${builtins.path { path = tree + "/${f}"; name = "license"; }} "$d/${component}/${baseNameOf f}"
  '') files;
  notice = builtins.path { path = tree + "/NOTICE"; name = "notice"; };
  spdxTexts = builtins.path { path = tree + "/LICENSES"; name = "licenses"; };
in
runCommand "kido-notices" { nativeBuildInputs = [ go unzip ]; } ''
  d=$out/usr/share/doc/kido/licenses
  mkdir -p $d

  cat > $out/usr/share/doc/kido/SOURCE <<'EOF'
  KidoDarwin is built from https://github.com/kido-sh/darwin at revision ${revision}.
  Its third-party packages come from nixpkgs ${nixpkgsRev}, and kido-agent from
  https://github.com/kido-sh/kido at revision ${kido.rev}.
  EOF
  install -Dm644 ${notice} $out/usr/share/doc/kido/NOTICE
  install -Dm644 -t $d/SPDX ${spdxTexts}/*.txt

  ${lib.concatStrings (lib.mapAttrsToList copyTree fromTree)}
  # an archive's top-level file, or a source directory's
  fromSource() {
    local component=$1 src=$2 file=$3 name
    if [ -d "$src" ]; then
      install -Dm644 "$src/$file" "$d/$component/$file"
    elif [[ $src == *.zip ]]; then
      name=$(unzip -Z1 "$src" | grep -E "^[^/]+/$file\$")
      mkdir -p "$d/$component"
      unzip -p "$src" "$name" > "$d/$component/$file"
    else
      name=$(tar -tf "$src" | grep -E "^[^/]+/$file\$")
      mkdir -p "$d/$component"
      tar -xOf "$src" "$name" > "$d/$component/$file"
    fi
  }
  ${lib.concatMapStrings (s: ''
    fromSource ${builtins.elemAt s 0} ${builtins.elemAt s 1} ${builtins.elemAt s 2}
  '') fromSource}

  # kido-agent: kido's own license, and those of the Go modules linked into it
  install -Dm644 ${kido}/LICENSE "$d/kido-agent/LICENSE"
  go version -m ${agent}/bin/kido-agent | while read -r kind path rest; do
    [ "$kind" = dep ] || continue
    for f in ${agent.goModules}/"$path"/LICENSE* ${agent.goModules}/"$path"/COPYING*; do
      if [ -f "$f" ]; then
        install -Dm644 "$f" "$d/kido-agent/$path/$(basename "$f")"
      fi
    done
  done
  find $out -type f -exec chmod 644 {} +
''
