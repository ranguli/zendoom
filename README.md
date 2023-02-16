# smol-doom

Doom source port (forked from Chocolate Doom) with the goal of creating the simplest working Doom engine in order to make studying the codebase as easy as possible, containing _zero_ fluff.

It accomplishes this by doing the following:

- Remove DOOM II, Strife, Heretic, and Hexen support
- Remove Windows/macOS/BSD support (Linux only).
  - Removes packaging support for Win32 and macOS
- Remove requiring Python 3 in the build system
- Remove documentation, icons, (generation)
- Remove the `setup` application, opting to manually edit `.cfg` files
- Remove support for MUS and MIDI as well as emulation support for PC speaker, OPL, PAS and GUS.
  - Remove support for reading 'in-WAD' music (due to removing MUS support). Music playback is done only through SDL via [music packs](https://www.chocolate-doom.org/wiki/index.php/Digital_music_packs).
- Remove GNU `autotools` build support, relying only on `cmake`.
- Remove `.desktop`, `.metainfo.xml`, `.rc`, and `.manifext.xml` files
- Remove bash completions and man pages

The only features it _adds_ are to change the default controls to utilize the `WASD` key cluster, with `E` for interaction.

## Building

```bash
cmake . && make
```

### Dependencies

```bash
sudo apt install cmake libsdl2-dev libsdl2-mixer-dev libsdl2-net-dev
```

