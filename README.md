# zendoom

An _aggresively_ minimalistic fork of Chocolate Doom that strives to have the smallest, simplest codebase possible to make exploring the core principles of the Doom engine as zen-like as possible. The only thing I might add (just for fun) is some kind of scripting support, perhaps embedding Lua or Guile so that code experiments can be made in a higher-level language.  

### Zen in the build system
- Complete dependency management and virtualized development environment with `Nix`. Install `nix`, and run `nix-shell shell.nix` in the project root. **Now you have all of the projects dependencies in an isolated space.** There's no need for your systems package manager to know about us.
- No support for GNU Autotools or CMake build systems. They been removed. Likewise, the need for Python 3 has been removed. There's a very short `meson.build` file, that builds with `ninja` and that's it. No `Makefile`, no `configure.ac`, no `CMakeLists.txt`. Plus, `meson` and `ninja` (as well as everything else) are already installed in the Nix shell.

### Zen in platform support, packaging and distribution
- `zendoom` is a Linux game. It runs on Linux. That's it. That's where my Zen is. My apologies to the other 97% of the desktop market.
- `zendoom` simply does not care about packaging and distribution. There are no `.deb` files, or `.rpm` files. It's a project built around exploration - build the code yourself. I might add an example Flatpak manifest so that if someone forks zendoom to make something else they can easily redistribute it _the zen way_, however.

### Zen in legacy support
- Remove DOOM II, Strife, Heretic, and Hexen support - we're talking about _Doom_ and _Doom_ only.
- Remove code and build scripts for the the `setup` application - a text editor for the `.cfg` file is all you need. Minimalism, baby.
- Remove support for MUS and MIDI as well as emulation support for PC speaker, OPL, PAS and GUS. Do you even know what some of those acronyms mean? I don't. We're using SDL like a sane person.
- Remove support for reading 'in-WAD' music (part of MUS support). Music playback is done only through SDL, and only via [music packs](https://www.chocolate-doom.org/wiki/index.php/Digital_music_packs). But I'm not even fussy about that. We should go a step further than the music packs and use straight `.ogg` files.
- The entire server binary has been removed - there is one unified binary for client and server (use `-dedicated`) to start a server.

The only features it _adds_ are to change the default controls to utilize the `WASD` key cluster, with `E` for interaction. Because anything else would not be Zen.

## Building

```bash
git clone https://github.com/zendoom/doom
cd doom
meson build
cd build
ninja build
```

### Dependencies

The only dependency _you_ need is `git`, and Nix. All the dependencies that _Doom_ needs are taken care of. 
