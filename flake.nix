{
  description = "VA-API compatibility shim for virgl/virtio-gpu video decode";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/331800de5053fcebacf6813adb5db9c9dca22a0c";
  };

  outputs = { self, nixpkgs }:
    let
      lib = nixpkgs.lib;
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: lib.genAttrs systems (system: f system);
      nixpkgsRev = nixpkgs.rev or nixpkgs.shortRev or "unknown";
      pkgsFor = system: import nixpkgs {
        inherit system;
        overlays = [ self.overlays.default ];
      };
    in
    {
      overlays.default = final: _prev: {
        virgl-vaapi-compat = final.callPackage ./nix/package.nix { };

        virgl-vaapi-compat-compatibility-report =
          final.callPackage ./nix/compatibility-report.nix {
            pkgs = final;
            inherit nixpkgsRev;
          };

        wrapFirefoxVirglVaapiCompat = args:
          final.callPackage ./nix/firefox-wrapper.nix ({
            virgl-vaapi-compat = final.virgl-vaapi-compat;
          } // args);

        firefox-virgl-vaapi = final.wrapFirefoxVirglVaapiCompat {
          firefox = final.firefox;
        };
      };

      packages = forAllSystems (system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.virgl-vaapi-compat;
          virgl-vaapi-compat = pkgs.virgl-vaapi-compat;
          compatibility-report = pkgs.virgl-vaapi-compat-compatibility-report;
          firefox-virgl-vaapi = pkgs.firefox-virgl-vaapi;
        });

      checks = forAllSystems (system:
        let
          pkgs = pkgsFor system;
          harness = pkgs.callPackage ./nix/harness-check.nix { };
          report = pkgs.virgl-vaapi-compat-compatibility-report;
        in
        {
          default = pkgs.runCommand "virgl-vaapi-compat-checks" { } ''
            mkdir -p "$out"
            ln -s ${pkgs.virgl-vaapi-compat} "$out/virgl-vaapi-compat"
            ln -s ${harness} "$out/fake-driver-harness"
            ln -s ${report}/compatibility-report.json "$out/compatibility-report.json"
          '';

          virgl-vaapi-compat = pkgs.virgl-vaapi-compat;
          fake-driver-harness = harness;
          compatibility-report = report;
        });

      devShells = forAllSystems (system:
        let
          pkgs = pkgsFor system;
          abi = import ./nix/libva-abi.nix {
            inherit (pkgs) lib;
            libvaVersion = pkgs.libva.version;
          };
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              binutils
              gnumake
              libva.dev
              pkg-config
            ];

            LIBVA_INIT_ABI_MAJOR = toString abi.initMajor;
            LIBVA_INIT_ABI_MINOR = toString abi.initMinor;
            VIRGL_VAAPI_COMPAT_INIT_SYMBOL = abi.initSymbol;
          };
        });

      lib = {
        libvaAbi = import ./nix/libva-abi.nix;
        firefoxWrapper = import ./nix/firefox-wrapper.nix;
      };
    };
}
