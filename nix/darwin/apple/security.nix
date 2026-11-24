{ lib
, mkPureDarwinBuild
, corefoundation
, libobjc
, foundation
, sqlite
, libressl
, libz
, src
, pname ? "kido-security"
}:

let
  # Shared with apple-sdk-pinned.nix so the SDK publishes the same headers.
  secHeaders = import ../../lib/security-headers.nix;
  secHeaderArgs = lib.concatStringsSep " " (
    map (d: "src/Libraries/Security/${d}/*.h") secHeaders.dirs
    ++ map (f: "src/Libraries/Security/${f}") (secHeaders.apiFiles ++ secHeaders.closureFiles));
in

# Security.framework, built from Apple's Security-59754.120.12 sources by
# src/Libraries/Security/CMakeLists.txt. The framework layout (Versions/A,
# Headers, the Current/ symlinks and the flat /usr/lib/libSecurity.dylib every
# other PureDarwin library also gets) is assembled here, since CMake's install
# rules only place the dylib and headers.
(mkPureDarwinBuild {
  inherit pname src;
  version = "59754.120.12";
  buildTargets = [ "Security" "trustd" "kido-tlscheck" ];
  ninjaFlags = [ "-k" "0" ];
  enableProjects = false;
  enableKernel = false;
  enableUserspace = false;
  installUserland = false;
  installKernel = false;
  extraCmakeFlags = [
    "-DPUREDARWIN_ENABLE_SECURITY=ON"
    "-DPUREDARWIN_COREFOUNDATION_PREFIX=${corefoundation}"
    "-DPUREDARWIN_LIBOBJC_PREFIX=${libobjc}"
    "-DPUREDARWIN_FOUNDATION_PREFIX=${foundation}"
    "-DPUREDARWIN_SQLITE_PREFIX=${sqlite}"
    "-DPUREDARWIN_LIBRESSL_PREFIX=${libressl.dev}"
    "-DPUREDARWIN_ZLIB_PREFIX=${libz}"
  ];
}).overrideAttrs (old: {
  installPhase = ''
    runHook preInstall

    fw="$out/System/Library/Frameworks/Security.framework"
    mkdir -p "$fw/Versions/A/Headers"
    cp build-nix/src/Libraries/Security/libSecurity.dylib \
      "$fw/Versions/A/Security"
    grep -v '^#' src/Libraries/Security/Security.exports | LC_ALL=C sort > exports.want
    "$NIX_DARWIN_TOOLCHAIN_DIR/arm64-apple-darwin20.4-nm" -gUj "$fw/Versions/A/Security" | LC_ALL=C sort > exports.have
    if ! cmp -s exports.want exports.have; then
      echo "src/Libraries/Security/Security.exports differs from what Security.framework exports:" >&2
      diff exports.want exports.have >&2
      exit 1
    fi

    # trustd serves trust evaluation, kido-tlscheck tests it
    install -Dm755 build-nix/src/Libraries/Security/trustd "$out/usr/libexec/trustd"
    install -Dm755 build-nix/src/Libraries/Security/kido-tlscheck "$out/usr/bin/kido-tlscheck"
    # Apple's own launchd job for the system trustd, the per-user agent waits for LaunchAgents
    install -Dm644 src/Libraries/Security/apple/trust/trustd/macOS/com.apple.trustd.plist \
      "$out/System/Library/LaunchDaemons/com.apple.trustd.plist"

    # Header set comes from nix/lib/security-headers.nix; see there for why
    # base/Security.h is excluded and what the second group is for.
    cp ${secHeaderArgs} "$fw/Versions/A/Headers/"

    # Apple's SecCertificatePriv.h includes <security_libDER/libDER/libDER.h>,
    # the spelling their build uses for libDER. Publish our clean-room libDER's
    # headers under that name so a consumer of the framework can resolve it;
    # the code itself is already linked into the framework.
    # Both spellings from one include root: Apple's headers use the
    # security_libDER/ prefix, libDER's own headers include each other plainly.
    mkdir -p "$out/include/security_libDER/libDER" "$out/include/libDER"
    cp src/Libraries/libDER/include/libDER/*.h "$out/include/security_libDER/libDER/"
    cp src/Libraries/libDER/include/libDER/*.h "$out/include/libDER/"

    ln -s A "$fw/Versions/Current"
    ln -s Versions/Current/Security "$fw/Security"
    ln -s Versions/Current/Headers "$fw/Headers"

    # Some consumers link -lSecurity against .../usr/lib rather than -F the
    # frameworks directory, as they do for CoreFoundation.
    mkdir -p "$out/usr/lib"
    ln -s "../../System/Library/Frameworks/Security.framework/Versions/A/Security" \
      "$out/usr/lib/libSecurity.dylib"

    runHook postInstall
  '';

  dontFixup = true;

  meta = (old.meta or { }) // {
    description = "Security.framework from Apple's Security-59754.120.12 sources";
  };
})
