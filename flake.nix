{
  description = "procex";
  inputs = {
    nixpkgs-for-tests-and-lib.url = "github:NixOS/nixpkgs?ref=nixos-unstable";
  };
  outputs = { self, nixpkgs-for-tests-and-lib }:
    let
      nixpkgs = nixpkgs-for-tests-and-lib;
      inherit (nixpkgs.lib) genAttrs;
      systems = [ "x86_64-linux" "aarch64-linux" ];
      hsOverlay = hsPkgs: hsPkgs.override {
        overrides = final: prev: {
          procex = final.callPackage ./procex.nix {};
        };
      };
      formatters = genAttrs systems (system: with nixpkgs.legacyPackages.${system}; [
        nixpkgs-fmt
        haskellPackages.cabal-fmt
        haskellPackages.fourmolu
      ]);
      regen = genAttrs systems (system: with nixpkgs.legacyPackages.${system}; writeShellApplication {
        name = "regen";
        runtimeInputs = [ cabal2nix ] ++ formatters.${system};
        text = ''
          set -xe
          cabal2nix ./. > procex.nix
          ./bin/format
        '';
      });
    in
    {
      checks = genAttrs systems (system: {
        formatting = nixpkgs.legacyPackages.${system}.runCommandNoCC "formatting-check"
          {
            nativeBuildInputs = formatters.${system};
          } ''
          cd ${self}
          ./bin/format check
          touch $out
        '';
      });
      devShells = genAttrs systems (system: {
        default = with nixpkgs.legacyPackages.${system}; mkShell {
          nativeBuildInputs = [
            cabal-install
            ghc
            haskell-language-server
          ] ++ formatters.${system};
        };
      });
    };
}
