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
      toolchain = import ./nix/toolchain {
        basePkgs = import nixpkgs { inherit system; };
      };
      darwin = import ./nix/darwin { inherit toolchain; };
      image = import ./nix/image {
        inherit toolchain darwin kido;
        revision = self.rev or self.dirtyRev or "unknown";
        nixpkgsRev = nixpkgs.rev;
        lastModifiedDate = self.lastModifiedDate or "19800101000000";
      };
    in
    {
      packages.${system} = {
        default = image.image;
        inherit (image) image oci disk rootfs kc loader agent;
        kernel = darwin.kernelArm64VirtBuild;
        kexts = darwin.kextsArm64Build;
      };
    };
}
