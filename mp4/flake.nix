{
  description = "A flake for the REX project";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
        # overlays = overlays;
      };
      devPackages = with pkgs; [
        # build deps
        cmake
        ninja # rust build
        (hiPrio gcc)
        libgcc
        curl
        diffutils
        xz.dev
        llvm
        clang
        lld
        zlib.dev
        openssl.dev
        flex
        bison
        busybox
        qemu
        mold
        perl
        pkg-config
        elfutils.dev
        ncurses.dev
        rust-bindgen
        pahole
        strace
        zstd
        eza
        libclang
        git
        git-lfs

        bear # generate compile commands
        rsync # for make headers_install
        gdb

        # bmc deps
        iproute2
        memcached

        # python3 scripts
        (pkgs.python3.withPackages
          (python-pkgs: (with python-pkgs;  [
            # select Python packages here
            tqdm
          ])))

        zoxide # in case host is using zoxide
        openssh # q-script ssh support
      ];

      fhs = pkgs.buildFHSUserEnv {
        name = "dev-env";
        targetPkgs = pkgs: devPackages;
        runScript = "./scripts/start.sh";
        profile = ''
            export LIBCLANG_PATH="${pkgs.libclang.lib.outPath}/lib/libclang.so"
        '';
      };
    in
    {
      devShells."${system}" = {
        default = fhs.env;

        dev = pkgs.mkShell {
          inputsFrom = [ pkgs.linux_latest ];
          buildInputs = devPackages;
          hardeningDisable = [ "strictoverflow" "zerocallusedregs" ];

          shellHook = ''
            echo "loading rex env"
            source ./scripts/env.sh
            export LIBCLANG_PATH="${pkgs.libclang.lib.outPath}/lib/libclang.so"
          '';
        };
      };
    };
}

