{
  description = "Lunar Gambit is a fork of freeciv21 for a master thesis project. ";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-22.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachSystem [ "x86_64-linux" "x86_64-darwin" ] (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
        lunar_gambit =
          pkgs.stdenv.mkDerivation {
            name = "lunar_gambit";
            src = self;
            nativeBuildInputs = [ pkgs.qt5.wrapQtAppsHook ];
            buildInputs = with pkgs;[
              cmake
              ninja
              gcc
              python3
              gettext
              readline
              qt5.qtbase
              # Use the libertinus in nixpkgs opposed to downloading
              libertinus
              libsForQt5.qt5.qtsvg
              lua5_3_compat
              sqlite.dev
              SDL2_mixer.dev
              # KArchive dep is automatically satisfied in x86_64-darwin
            ] ++ (if system == "x86_64-darwin" then [ ] else [ libsForQt5.karchive ]);
            buildPhase = ''
              mkdir -p $out/
              cmake . -B build -G Ninja \
                -DCMAKE_INSTALL_PREFIX=$out \
                -DFREECIV_DOWNLOAD_FONTS=0
            '';
            # Installing simply means copying all files to the output directory
            installPhase = ''# Build source files and copy them over.
            cmake --build build --target install
            cp -R ${pkgs.libertinus}/share/fonts $out/share
            ln -s $out/bin/lunar_gambit-client $out/bin/lunar_gambit
        '';
          };
      in
      {
        packages = {
          lunar_gambit = lunar_gambit;
          default = lunar_gambit;
        };
      });
}
