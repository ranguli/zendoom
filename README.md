# zendoom

An _aggresively_ minimalistic, eductional fork of Chocolate Doom that strives to have the smallest, simplest codebase possible to make exploring the core principles of the Doom engine as zen-like as possible. The only thing I might add (just for fun) is some kind of scripting support, perhaps embedding Lua or Guile so that code experiments can be made in a higher-level language.  

## Motivation

Chocolate Doom is considered among the most conserative ports of Doom, but for someone looking to study and explore the Doom engine, it still has a lot of extra stuff going on:

- DeHackEd support
- Merging of sprites and flats (DeuTex)
- OPL emulation
- MIDI support using timidity/fluidsynth
- PC speaker support 
- GUS emulation

These things are _amazing_ for recreating the original feel of Doom, but they don't serve our purpose of creating an ultra minimal educational source-port for exploring the engine itself.

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

### Zen in code quality
-A lofty but noble goal is to try and clean up as many compiler and linter warnings as possible, with the primary goal of improving code readability, and secondarily the quality of the code being read overall. This is accomplished using `clang-tidy`, `clang-format`, and `cppcheck`.


## Building

```bash
git clone https://github.com/zendoom/doom
cd doom
CFLAGS="-Wall -O2" meson setup build
ninja -C build
```

### Dependencies

The only dependency _you_ need is `git`, and Nix. All the dependencies that _Doom_ needs are taken care of. 

## Development
From within the `build` directory, extra Meson run targets are available for running `cppcheck`, `clang-tidy`, and `clang-format`. For example:

```
cd build
meson compile cppcheck    # Will run cppcheck
meson compile clangformat # Will run clang-format 
meson compile clangtidy   # Will run clang-format 
```


## Compability

Over the years a lot of configuration options have been added to various source ports of Doom as they add support for different things. Here is an (incomplete) table that overviews what options remain in zendoom:

| Configuration Value        | Removed?    |
|----------------------------|-------------|
| video_driver               |             |
| window_position            |             |
| fullscreen                 |             |
| video_display              |             |
| aspect_ratio_correct       |             |
| integer_scaling            |             |
| vga_porch_flash            |To be removed|
| window_width               |             |
| window_height              |             |
| fullscreen_width           |             |
| fullscreen_height          |             |
| force_software_renderer    |             |
| max_scaling_buffer_pixels  |             |
| startup_delay              |             |
| show_endoom                |             |
| show_diskicon              |             |
| png_screenshots            |To be removed|
| snd_samplerate             |             |
| snd_cachesize              |             |
| snd_maxslicetime_ms        |             |
| snd_pitchshift             |             |
| snd_musiccmd               |             |
| snd_dmxoption              |             |
| opl_io_port                |Yes          |
| use_libsamplerate          |             |
| libsamplerate_scale        |             |
| music_pack_path            |             |
| timidity_cfg_path          |Yes          |
| gus_patch_path             |Yes          |
| gus_ram_kb                 |Yes          |
| vanilla_savegame_limit     |             |
| vanilla_demo_limit         |             |
| vanilla_keyboard_mapping   |             |
| player_name                |             |
| grabmouse                  |             |
| novert                     |             |
| mouse_acceleration         |             |
| mouse_threshold            |             |
| mouseb_strafeleft          |             |
| mouseb_straferight         |             |
| mouseb_use                 |             |
| mouseb_backward            |             |
| mouseb_prevweapon          |             |
| mouseb_nextweapon          |             |
| dclick_use                 |             |
| joystick_guid              |             |
| joystick_index             |             |
| joystick_x_axis            |             |
| joystick_x_invert          |             |
| joystick_y_axis            |             |
| joystick_y_invert          |             |
| joystick_strafe_axis       |             |
| joystick_strafe_invert     |             |
| joystick_look_axis         |             |
| joystick_look_invert       |             |
| joystick_physical_button0  |             |
| joystick_physical_button1  |             |
| joystick_physical_button2  |             |
| joystick_physical_button3  |             |
| joystick_physical_button4  |             |
| joystick_physical_button5  |             |
| joystick_physical_button6  |             |
| joystick_physical_button7  |             |
| joystick_physical_button8  |             |
| joystick_physical_button9  |             |
| joystick_physical_button10 |             |
| joyb_strafeleft            |             |
| joyb_straferight           |             |
| joyb_menu_activate         |             |
| joyb_toggle_automap        |             |
| joyb_prevweapon            |             |
| joyb_nextweapon            |             |
| key_pause                  |             |
| key_menu_activate          |             |
| key_menu_up                |             |
| key_menu_down              |             |
| key_menu_left              |             |
| key_menu_right             |             |
| key_menu_back              |             |
| key_menu_forward           |             |
| key_menu_confirm           |             |
| key_menu_abort             |             |
| key_menu_help              |             |
| key_menu_save              |             |
| key_menu_load              |             |
| key_menu_volume            |             |
| key_menu_detail            |             |
| key_menu_qsave             |             |
| key_menu_endgame           |             |
| key_menu_messages          |             |
| key_menu_qload             |             |
| key_menu_quit              |             |
| key_menu_gamma             |             |
| key_spy                    |             |
| key_menu_incscreen         |             |
| key_menu_decscreen         |             |
| key_menu_screenshot        |             |
| key_map_toggle             |             |
| key_map_north              |             |
| key_map_south              |             |
| key_map_east               |             |
| key_map_west               |             |
| key_map_zoomin             |             |
| key_map_zoomout            |             |
| key_map_maxzoom            |             |
| key_map_follow             |             |
| key_map_grid               |             |
| key_map_mark               |             |
| key_map_clearmark          |             |
| key_weapon1                |             |
| key_weapon2                |             |
| key_weapon3                |             |
| key_weapon4                |             |
| key_weapon5                |             |
| key_weapon6                |             |
| key_weapon7                |             |
| key_weapon8                |             |
| key_prevweapon             |             |
| key_nextweapon             |             |
| key_message_refresh        |             |
| key_demo_quit              |             |
| key_multi_msg              |             |
| key_multi_msgplayer1       |             |
| key_multi_msgplayer2       |             |
| key_multi_msgplayer3       |             |
| key_multi_msgplayer4       |             |


## TODO:

- Github actions
- Embedded scripting of some kind with a well-documented API
- A simpler configuration system? Text files are fine, but surely we can clean it up someway.
- Only support PNG screenshots (no PCX)
- Remove DOS emulation of null Read Access Violation (system.c:300)
- (Gradually) replace bespoke file I/O with posix ones
- Remove gamma correction feature
- Remove DOOM II cast sequence code
