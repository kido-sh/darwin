# /usr/share/doc/kido: where the image's source is, NOTICE, the texts of LICENSES/ (licenses/SPDX), and
# each component's own license file from this tree or from the third-party source it is built from.
{ runCommand
, lib
, unzip
, python3
, rustPlatform
, zsh
, ncurses
, libedit
, toybox
, icu
, zlib
, bzip2
, xz
, libffiReal
, libxml2
, libressl
, pinned
, agent
, kl
, kido
, nixpkgsRev
, revision
, versionsJson
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
    [ "zsh" zsh.src "LICENCE" ]
    [ "ncurses" ncurses.src "COPYING" ]
    [ "libedit" libedit.src "COPYING" ]
    [ "toybox" toybox.src "LICENSE" ]
    [ "icu" icu.src "LICENSE" ]
    [ "zlib" zlib.src "LICENSE" ]
    [ "bzip2" bzip2.src "LICENSE" ]
    [ "xz" xz.src "COPYING.0BSD" ]
    [ "libffi" libffiReal.src "LICENSE" ]
    [ "libxml2" libxml2.src "Copyright" ]
    [ "libressl" libressl.src "COPYING" ]
  ] ++ lib.concatLists (lib.mapAttrsToList (name: pkg:
    let pin = lib.importJSON (../darwin/third-party + "/${name}/source.json");
    in lib.optional (pin.licenseFile != null) [ name pkg.src pin.licenseFile ]) pinned);
  copyTree = component: files: lib.concatMapStrings (f: ''
    install -Dm644 ${builtins.path { path = tree + "/${f}"; name = "license"; }} "$d/${component}/${baseNameOf f}"
  '') files;
  notice = builtins.path { path = tree + "/NOTICE"; name = "notice"; };
  spdxTexts = builtins.path { path = tree + "/LICENSES"; name = "licenses"; };
in
runCommand "kido-notices" { nativeBuildInputs = [ unzip python3 ]; } ''
  d=$out/usr/share/doc/kido/licenses
  mkdir -p $d

  cat > $out/usr/share/doc/kido/SOURCE <<'EOF'
  KidoDarwin is built from https://github.com/kido-sh/darwin at revision ${revision}.
  Its third-party packages come from nixpkgs ${nixpkgsRev}, and kido-agent and kl from
  https://github.com/kido-sh/kido at revision ${kido.rev or "unknown"}.
  EOF
  install -Dm644 ${notice} $out/usr/share/doc/kido/NOTICE
  install -Dm644 ${versionsJson} $out/usr/share/doc/kido/versions.json
  install -Dm644 -t $d/SPDX ${spdxTexts}/*.txt

  ${lib.concatStrings (lib.mapAttrsToList copyTree fromTree)}
  # an archive's top-level file, or a source directory's
  fromSource() {
    local component=$1 src=$2 file=$3 name
    if [ -d "$src" ]; then
      install -Dm644 "$src/$file" "$d/$component/$(basename "$file")"
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

  # kido-agent: kido's own license, Rust standard library licenses (MIT / Apache-2.0 /
  # compiler-builtins), and every vendored Cargo crate's license files from agent.cargoDeps
  install -Dm644 ${kido}/LICENSE "$d/kido-agent/LICENSE"
  install -Dm644 ${rustPlatform.rustLibSrc}/stdarch/LICENSE-APACHE "$d/kido-agent/rust/LICENSE-APACHE"
  install -Dm644 ${rustPlatform.rustLibSrc}/stdarch/LICENSE-MIT "$d/kido-agent/rust/LICENSE-MIT"
  install -Dm644 ${rustPlatform.rustLibSrc}/compiler-builtins/LICENSE.txt "$d/kido-agent/rust/compiler-builtins-LICENSE.txt"

  crate_count=0
  for crate_dir in ${agent.cargoDeps}/source-registry-0/*; do
    [ -d "$crate_dir" ] || continue
    crate=$(basename "$crate_dir")
    found=0
    for f in "$crate_dir"/LICENSE* "$crate_dir"/COPYING* "$crate_dir"/COPYRIGHT* "$crate_dir"/UNLICENSE*; do
      if [ -f "$f" ]; then
        install -Dm644 "$f" "$d/kido-agent/crates/$crate/$(basename "$f")"
        found=1
      fi
    done
    if [ "$found" -eq 0 ]; then
      echo "ERROR: no license file found for vendored crate $crate" >&2
      exit 1
    fi
    crate_count=$((crate_count + 1))
  done
  if [ "$crate_count" -eq 0 ]; then
    echo "ERROR: no vendored Cargo crates found under ${agent.cargoDeps}" >&2
    exit 1
  fi

  # kl: kido's own license and every external Cargo crate resolved for /usr/bin/kl
  install -Dm644 ${kido}/LICENSE "$d/kl/LICENSE"
  python3 - "${kl}/share/kl-crates.json" "${kl.cargoDeps}" "${spdxTexts}" "$d/kl/crates" <<'PY'
  import json, shutil, sys, tomllib
  from pathlib import Path

  crates = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
  vendor = Path(sys.argv[2])
  spdx_dir = Path(sys.argv[3])
  out_dir = Path(sys.argv[4])

  if not crates:
      raise SystemExit("ERROR: kl-crates.json contains no external crates")

  vendor_map = {}
  for grp in sorted(p for p in vendor.iterdir() if p.is_dir() and p.name.startswith("source-")):
      for cdir in sorted(p for p in grp.iterdir() if p.is_dir()):
          vendor_map[cdir.name] = cdir

  prefixes = ("LICENSE", "COPYING", "COPYRIGHT", "UNLICENSE")
  for entry in crates:
      cname = entry["dir"]
      cdir = vendor_map.get(cname)
      if cdir is None:
          raise SystemExit(f"ERROR: vendored crate {cname} not found under {vendor}")
      dst = out_dir / cname
      dst.mkdir(parents=True, exist_ok=True)
      found = 0
      for f in sorted(cdir.iterdir()):
          if f.is_file() and f.name.startswith(prefixes):
              shutil.copyfile(f, dst / f.name)
              found += 1
      if found == 0:
          meta = tomllib.loads((cdir / "Cargo.toml").read_text(encoding="utf-8")).get("package", {})
          lic = (meta.get("license") or "").strip()
          spdx_txt = spdx_dir / f"{lic}.txt"
          if lic and spdx_txt.is_file():
              shutil.copyfile(spdx_txt, dst / "LICENSE")
          else:
              raise SystemExit(f"ERROR: no license file or SPDX text for kl crate {cname} (license={lic!r})")
  PY
  find $out -type f -exec chmod 644 {} +
''
