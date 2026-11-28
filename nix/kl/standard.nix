# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# kl meta-package "standard": installs a small catalog manifest under
# /usr/share/kl/packages/standard.json and depends on libffi, libxml2, and python3.
{ runCommand
, lib
, bash
, darwinCrossToolchain
, libffi
, libxml2
, python3
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
}:

let
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
  version = "1.0.0";
  licenseText = builtins.path {
    path = ../../LICENSES/Apache-2.0.txt;
    name = "Apache-2.0.txt";
  };
  klMeta = {
    name = "standard";
    inherit version arch;
    license = "Apache-2.0";
    summary = "Standard KidoDarwin system library collection (libffi, libxml2, python3)";
    depends = [ "libffi" "libxml2" "python3" ];
    paths = [
      "usr/share/kl/packages/standard.json"
      "usr/share/doc/kl/standard/LICENSE"
    ];
  };
  manifestJson = builtins.toJSON {
    name = "standard";
    inherit version arch;
    packages = {
      libffi = libffi.version;
      libxml2 = libxml2.version;
      python3 = python3.version;
    };
  };
in
runCommand "kido-kl-standard-${arch}-${version}" {
  passthru = {
    kl = klMeta;
    members = { inherit libffi libxml2 python3; };
  };
  meta = with lib; {
    description = "kl standard meta-package (${arch})";
    license = licenses.asl20;
    platforms = platforms.unix;
  };
} ''
  # Reference member derivations so building kl-standard ensures every member passes its gates.
  test -d "${libffi}/usr/lib"
  test -d "${libxml2}/usr/lib"
  test -x "${python3}/usr/bin/python3"

  mkdir -p "$out/usr/share/kl/packages" "$out/usr/share/doc/kl/standard"
  cat > "$out/usr/share/kl/packages/standard.json" <<'EOF'
${manifestJson}
EOF
  chmod 644 "$out/usr/share/kl/packages/standard.json"
  install -Dm644 "${licenseText}" "$out/usr/share/doc/kl/standard/LICENSE"

  "${bash}/bin/bash" ${./check-gates.sh} \
    --root "$out" \
    --pkg-name standard \
    --arch "${arch}" \
    --license "${klMeta.license}" \
    --base-dylibs ${./base-dylibs.txt} \
    --otool "${t}-otool" \
    --objdump "${t}-objdump"
''
