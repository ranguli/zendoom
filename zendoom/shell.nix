{ pkgs ? import <nixpkgs> { } }:
pkgs.mkShell {
  nativeBuildInputs = with pkgs.buildPackages; [
    meson # Build system
    ninja
    pkg-config # For helping meson find SDL
    gcc
    cppcheck # Static analysis
    clang-tools # Formatting and linting
    SDL2
    SDL2_mixer
    SDL2_net
  ];
}
