//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	System interface for music.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"
#include "SDL_mixer.h"

#include "../misc/config.h"
#include "../lib/type.h"

#include "sound.h"
#include "swap.h"
#include "system.h"
#include "../lib/argv.h"
#include "../../config.h"
#include "../misc/misc.h"
#include "../lib/sha1.h"
#include "../wad/wad.h"
#include "../mem/zone.h"

#define MAXMIDLENGTH (96 * 1024)

static boolean music_initialized = false;

// If this is true, this module initialized SDL sound and has the
// responsibility to shut it down

static boolean sdl_was_initialized = false;

static boolean musicpaused = false;
static int current_music_volume;

// Shutdown music

static void I_SDL_ShutdownMusic(void) {
    if (music_initialized) {
        Mix_HaltMusic();
        music_initialized = false;

        if (sdl_was_initialized) {
            Mix_CloseAudio();
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            sdl_was_initialized = false;
        }
    }
}

static boolean SDLIsInitialized(void) {
    int freq, channels;
    Uint16 format;

    return Mix_QuerySpec(&freq, &format, &channels) != 0;
}

// Initialize music subsystem
static boolean I_SDL_InitMusic(void) {
    // If SDL_mixer is not initialized, we have to initialize it
    // and have the responsibility to shut it down later on.

    if (SDLIsInitialized()) {
        music_initialized = true;
    } else {
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            fprintf(stderr, "Unable to set up sound.\n");
        } else if (Mix_OpenAudioDevice(snd_samplerate, AUDIO_S16SYS, 2, 1024, NULL,
                                       SDL_AUDIO_ALLOW_FREQUENCY_CHANGE) < 0) {
            fprintf(stderr, "Error initializing SDL_mixer: %s\n", Mix_GetError());
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        } else {
            SDL_PauseAudio(0);

            sdl_was_initialized = true;
            music_initialized = true;
        }
    }

    // Initialize SDL_Mixer for MIDI music playback
    Mix_Init(MIX_INIT_MID);

    return music_initialized;
}

//
// SDL_mixer's native MIDI music playing does not pause properly.
// As a workaround, set the volume to 0 when paused.
//

static void UpdateMusicVolume(void) {
    int vol;

    if (musicpaused) {
        vol = 0;
    } else {
        vol = (current_music_volume * MIX_MAX_VOLUME) / 127;
    }

    Mix_VolumeMusic(vol);
}

// Set music volume (0 - 127)

static void I_SDL_SetMusicVolume(int volume) {
    // Internal state variable.
    current_music_volume = volume;

    UpdateMusicVolume();
}

// Start playing a mid

static void I_SDL_PlaySong(void *handle, boolean looping) {
    int loops;

    if (!music_initialized) {
        return;
    }

    if (looping) {
        loops = -1;
    } else {
        loops = 1;
    }

    Mix_PlayMusic((Mix_Music *)handle, loops);
}

static void I_SDL_PauseSong(void) {
    if (!music_initialized) {
        return;
    }

    musicpaused = true;

    UpdateMusicVolume();
}

static void I_SDL_ResumeSong(void) {
    if (!music_initialized) {
        return;
    }

    musicpaused = false;

    UpdateMusicVolume();
}

static void I_SDL_StopSong(void) {
    if (!music_initialized) {
        return;
    }

    Mix_HaltMusic();
}

static void I_SDL_UnRegisterSong(void *handle) {
    Mix_Music *music = (Mix_Music *)handle;

    if (!music_initialized) {
        return;
    }

    if (handle != NULL) {
        Mix_FreeMusic(music);
    }
}

static void *I_SDL_RegisterSong() {
    char *filename;

    if (!music_initialized) {
        return NULL;
    }

    filename = M_TempFile("doom.mid");

    remove(filename);
    free(filename);
}

// Is the song playing?
static boolean I_SDL_MusicIsPlaying(void) {
    if (!music_initialized) {
        return false;
    }

    return Mix_PlayingMusic();
}

static snddevice_t music_sdl_devices[] = {SNDDEVICE_GENMIDI};

music_module_t music_sdl_module = {
    music_sdl_devices,
    arrlen(music_sdl_devices),
    I_SDL_InitMusic,
    I_SDL_ShutdownMusic,
    I_SDL_SetMusicVolume,
    I_SDL_PauseSong,
    I_SDL_ResumeSong,
    I_SDL_RegisterSong,
    I_SDL_UnRegisterSong,
    I_SDL_PlaySong,
    I_SDL_StopSong,
    I_SDL_MusicIsPlaying,
    NULL, // Poll
};
