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
      };

      packages.${system} = {
        default = image.image;
        inherit (image) image oci disk rootfs kc loader agent;
        kernel = darwin.kernelArm64VirtBuild;
        kexts = darwin.kextsArm64Build;
        metal = darwin.metalArm64Build;
        arm64e-tests = darwin.arm64eTestsBuild;
        curl = darwin.curlArm64Build;
        expat = darwin.expatArm64eBuild;
        libiconv = darwin.iconvArm64eBuild;
        libutil = darwin.libutilArm64eBuild;
        nghttp2 = darwin.nghttp2Arm64eBuild;
        sqlite = darwin.sqliteDylibArm64eBuild;
      };
    };
}
