{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    kido = {
      url = "github:kido-sh/kido";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, kido }:
    let
      system = "aarch64-linux";
      basePkgs = import nixpkgs { inherit system; };
      toolchain = import ./nix/toolchain { inherit basePkgs; };
      darwin = import ./nix/darwin { inherit toolchain; };
      kl = import ./nix/kl { inherit toolchain darwin; };
      image = import ./nix/image {
        inherit toolchain darwin kido;
        revision = self.rev or self.dirtyRev or "unknown";
        nixpkgsRev = nixpkgs.rev;
        lastModifiedDate = self.lastModifiedDate or "19800101000000";
      };
    in
    {
      checks.${system} = {
        virtiofs-fuzz = basePkgs.callPackage ./nix/checks/virtiofs-fuzz.nix { };
        virtiofs-unit = basePkgs.callPackage ./nix/checks/virtiofs-unit.nix { };
        virtiofs-analyze = darwin.virtiofsAnalyzeBuild;
        sbom = image.sbom;
      };

      packages.${system} = {
        default = image.image;
        inherit (image) image oci disk rootfs kc loader agent kl sbom;
        kernel = darwin.kernelArm64VirtBuild;
        kexts = darwin.kextsArm64Build;
        metal = darwin.metalArm64Build;
        arm64e-tests = darwin.arm64eTestsBuild;
        curl = darwin.curlArm64Build;
        libressl = darwin.libresslArm64eBuild;
        sw_vers = darwin.swVersArm64Build;
        expat = darwin.expatArm64eBuild;
        libiconv = darwin.iconvArm64eBuild;
        libutil = darwin.libutilArm64eBuild;
        xcselect = darwin.xcselectArm64eBuild;
        nghttp2 = darwin.nghttp2Arm64eBuild;
        sqlite = darwin.sqliteDylibArm64eBuild;
        kl-libffi = kl.arm64.libffi;
        kl-libxml2 = kl.arm64.libxml2;
        kl-python3 = kl.arm64.python3;
        kl-swift-runtime = kl.arm64.swiftRuntime;
        kl-standard = kl.arm64.standard;
        kl-all = kl.arm64.all;
        kl-arm64e-libffi = kl.arm64e.libffi;
        kl-arm64e-libxml2 = kl.arm64e.libxml2;
        kl-arm64e-python3 = kl.arm64e.python3;
        kl-arm64e-swift-runtime = kl.arm64e.swiftRuntime;
        kl-arm64e-standard = kl.arm64e.standard;
        kl-arm64e-all = kl.arm64e.all;
      };
    };
}
