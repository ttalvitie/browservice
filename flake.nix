{
  description = "Browser service for legacy web browsers via server-side rendering";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    let
      # NixOS module (system-independent)
      nixosModule = { config, lib, pkgs, ... }: {
        imports = [ ./nixos-module.nix ];
        services.browservice.package = lib.mkDefault self.packages.${pkgs.system}.browservice;
      };
    in
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" ] (system:
      let
        pkgs = import nixpkgs { inherit system; };

        version = "0.9.12.1";

        cefArchMap = {
          "x86_64-linux" = "x86_64";
          "aarch64-linux" = "aarch64";
        };

        cefHashes = {
          "x86_64" = "1qdlpq98jmi3r4rg9azkrprgxs5mvmg8svgfmkyg1ld1v3api80f";
          "aarch64" = "1dxjv65rjkbnyc051hf505hd1ahw752lh59pimr50nkgkq07wqy8";
        };

        cefArch = cefArchMap.${system};

        patchedCef = pkgs.fetchurl {
          url = "https://github.com/ttalvitie/browservice/releases/download/v${version}/patched_cef_${cefArch}.tar.bz2";
          sha256 = cefHashes.${cefArch};
        };

        runtimeLibs = with pkgs; [
          # X11 and graphics
          xorg.libX11
          xorg.libXcomposite
          xorg.libXcursor
          xorg.libXdamage
          xorg.libXext
          xorg.libXfixes
          xorg.libXi
          xorg.libXrandr
          xorg.libXrender
          xorg.libXScrnSaver
          xorg.libXtst
          xorg.libxcb
          xorg.libxshmfence
          libxkbcommon

          # GTK and related
          gtk3
          glib
          pango
          cairo
          gdk-pixbuf
          atk
          at-spi2-atk
          at-spi2-core
          dbus

          # System libs
          alsa-lib
          cups
          libdrm
          mesa
          libGL
          libGLU
          expat
          nspr
          nss
          udev
          libgbm

          # Fonts
          fontconfig
          freetype

          # Other
          zlib
          bzip2
        ];

        cefDllWrapper = pkgs.stdenv.mkDerivation {
          pname = "cef-dll-wrapper";
          inherit version;

          src = patchedCef;

          nativeBuildInputs = with pkgs; [
            cmake
            autoPatchelfHook
          ];

          buildInputs = runtimeLibs;

          dontConfigure = true;

          unpackPhase = ''
            mkdir -p cef
            tar xf $src -C cef --strip-components 1
          '';

          buildPhase = ''
            mkdir -p cef/releasebuild
            cd cef/releasebuild
            cmake -DCMAKE_BUILD_TYPE=Release ..
            make -j$NIX_BUILD_CORES libcef_dll_wrapper
          '';

          installPhase = ''
            cd $NIX_BUILD_TOP
            mkdir -p $out/{lib,include,Resources,Release}

            # Install wrapper library
            cp cef/releasebuild/libcef_dll_wrapper/libcef_dll_wrapper.a $out/lib/

            # Install CEF shared libraries
            cp -r cef/Release/* $out/Release/
            cp -r cef/Resources/* $out/Resources/

            # Install headers
            cp -r cef/include $out/
          '';
        };

        browservice = pkgs.stdenv.mkDerivation {
          pname = "browservice";
          inherit version;

          src = ./.;

          nativeBuildInputs = with pkgs; [
            pkg-config
            autoPatchelfHook
            python3
          ];

          buildInputs = with pkgs; [
            pango
            xorg.libX11
            xorg.libxcb
            poco
            libjpeg
            zlib
            openssl
          ] ++ runtimeLibs;

          preBuild = ''
            # Setup CEF from pre-built wrapper
            mkdir -p cef/{Release,Resources,include,releasebuild/libcef_dll_wrapper}
            cp -r ${cefDllWrapper}/Release/* cef/Release/
            cp -r ${cefDllWrapper}/Resources/* cef/Resources/
            cp -r ${cefDllWrapper}/include/* cef/include/
            cp ${cefDllWrapper}/lib/libcef_dll_wrapper.a cef/releasebuild/libcef_dll_wrapper/

            # Generate retrojsvice HTML
            pushd viceplugins/retrojsvice
            mkdir -p gen
            python3 gen_html_cpp.py > gen/html.cpp
            popd
          '';

          buildPhase = ''
            runHook preBuild
            make -j$NIX_BUILD_CORES release
            runHook postBuild
          '';

          installPhase = ''
            mkdir -p $out/bin $out/lib $out/share/browservice

            # Install main binary
            cp release/bin/browservice $out/bin/browservice-unwrapped

            # Install CEF resources (Release dir, then Resources without duplicating locales)
            cp -rn cef/Release/* $out/lib/ || true
            # Copy Resources, handling locales directory specially
            for item in cef/Resources/*; do
              if [ -d "$item" ]; then
                cp -rn "$item" $out/lib/ || true
              else
                cp -n "$item" $out/lib/ || true
              fi
            done

            # Install retrojsvice plugin
            cp release/bin/retrojsvice.so $out/lib/

            # Create wrapper script
            cat > $out/bin/browservice << 'WRAPPER'
            #!/usr/bin/env bash
            SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
            LIB_DIR="$(dirname "$SCRIPT_DIR")/lib"
            export LD_LIBRARY_PATH="$LIB_DIR''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
            exec "$SCRIPT_DIR/browservice-unwrapped" "$@"
            WRAPPER
            chmod +x $out/bin/browservice
          '';

          # Handle the RPATH for the binary
          postFixup = ''
            patchelf --set-rpath "$out/lib:${pkgs.lib.makeLibraryPath runtimeLibs}" $out/bin/browservice-unwrapped

            # The chrome-sandbox binary needs special handling (setuid)
            if [ -f $out/lib/chrome-sandbox ]; then
              chmod 755 $out/lib/chrome-sandbox
            fi
          '';

          passthru = {
            inherit cefDllWrapper;
          };

          meta = with pkgs.lib; {
            description = "Browser as a service for legacy web browsers via server-side rendering";
            homepage = "https://github.com/ttalvitie/browservice";
            license = licenses.mit;
            platforms = [ "x86_64-linux" "aarch64-linux" ];
            mainProgram = "browservice";
          };
        };

      in {
        packages = {
          default = browservice;
          inherit browservice cefDllWrapper;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ browservice ];

          packages = with pkgs; [
            xvfb-run
            xorg.xauth
            gdb
          ];

          shellHook = ''
            echo "Browservice development shell"
            echo "Run 'nix build' to build the package"
          '';
        };
      }) // {
        # System-independent outputs
        nixosModules = {
          default = nixosModule;
          browservice = nixosModule;
        };
      };
}
