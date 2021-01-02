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

#include "i_glob.h"

#include "config.h"
#include "doomtype.h"

#include "deh_str.h"
#include "i_sound.h"
#include "i_swap.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "sha1.h"
#include "w_wad.h"
#include "z_zone.h"

#define MID_HEADER_MAGIC "MThd"
#define MUS_HEADER_MAGIC "MUS\x1a"

#define FLAC_HEADER "fLaC"
#define OGG_HEADER "OggS"

// Looping Vorbis metadata tag names. These have been defined by ZDoom
// for specifying the start and end positions for looping music tracks
// in .ogg and .flac files.
// More information is here: http://zdoom.org/wiki/Audio_loop
#define LOOP_START_TAG "LOOP_START"
#define LOOP_END_TAG "LOOP_END"

// FLAC metadata headers that we care about.
#define FLAC_STREAMINFO 0
#define FLAC_VORBIS_COMMENT 4

// Ogg metadata headers that we care about.
#define OGG_ID_HEADER 1
#define OGG_COMMENT_HEADER 3

// Structure for music substitution.
// We store a mapping based on SHA1 checksum -> filename of substitute music
// file to play, so that substitution occurs based on content rather than
// lump name. This has some inherent advantages:
//  * Music for Plutonia (reused from Doom 1) works automatically.
//  * If a PWAD replaces music, the replacement music is used rather than
//    the substitute music for the IWAD.
//  * If a PWAD reuses music from an IWAD (even from a different game), we get
//    the high quality version of the music automatically (neat!)

typedef struct {
    const char *hash_prefix;
    const char *filename;
} subst_music_t;

// Structure containing parsed metadata read from a digital music track:
typedef struct {
    boolean valid;
    unsigned int samplerate_hz;
    int start_time, end_time;
} file_metadata_t;

static subst_music_t *subst_music = NULL;
static unsigned int subst_music_len = 0;

static boolean music_initialized = false;

// If this is true, this module initialized SDL sound and has the
// responsibility to shut it down

static boolean sdl_was_initialized = false;

char *music_pack_path = "";

// If true, we are playing a substitute digital track rather than in-WAD
// MIDI/MUS track, and file_metadata contains loop metadata.
static file_metadata_t file_metadata;

// Position (in samples) that we have reached in the current track.
// This is updated by the TrackPositionCallback function.
static unsigned int current_track_pos;

// Currently playing music track.
static Mix_Music *current_track_music = NULL;

// If true, the currently playing track is being played on loop.
static boolean current_track_loop;

// Table of known hashes and filenames to look up for them. This allows
// users to drop in a set of files without having to also provide a
// configuration file.
static const subst_music_t known_filenames[] = {
    // Doom 1 music files.
    {"b2e05b4e8dff8d76f8f4", "d_inter.{ext}"},
    {"0c0acce45130bab935d2", "d_intro.{ext}"},
    {"fca4086939a68ae4ed84", "d_victor.{ext}"},
    {"5971e5e20554f47ca065", "d_intro.{ext}"},
    {"99767e32769229897f77", "d_e1m1.{ext}"},
    {"b5e7dfb4efe9e688bf2a", "d_e1m2.{ext}"},
    {"fda8fa73e4d30a6b961c", "d_e1m3.{ext}"},
    {"3805f9bf3f1702f7e7f5", "d_e1m4.{ext}"},
    {"f546ed823b234fe39165", "d_e1m5.{ext}"},
    {"4450811b5a6748cfd83e", "d_e1m6.{ext}"},
    {"73edb50d96b0ac03be34", "d_e1m7.{ext}"},
    {"47d711a6fd32f5047879", "d_e1m8.{ext}"},
    {"62c631c2fdaa5ecd9a8d", "d_e1m9.{ext}"},
    {"7702a6449585428e7185", "d_e2m1.{ext}"},
    {"1cb1810989cbfae2b29b", "d_e2m2.{ext}"},
    {"7d740f3c881a22945e47", "d_e2m4.{ext}"},
    {"ae9c3dc2f9aeea002327", "d_e2m6.{ext}"},
    {"b26aad3caa420e9a2c76", "d_e2m7.{ext}"},
    {"90f06251a2a90bfaefd4", "d_e2m8.{ext}"},
    {"b2fb439f23c08c8e2577", "d_e3m1.{ext}"},
    {"b6c07bb249526b864208", "d_e3m2.{ext}"},
    {"ce3587ee503ffe707b2d", "d_e3m3.{ext}"},
    {"d746ea2aa16b3237422c", "d_e3m8.{ext}"},
    {"3da3b1335560a92912e6", "d_bunny.{ext}"},

    // Duplicates that don't have identical hashes:
    {"4a5badc4f10a7d4ed021", "d_inter.{ext}"}, // E2M3
    {"36b14bf165b3fdd3958e", "d_e1m7.{ext}"},  // E3M5
    {"e77c3d42f2ea87f04607", "d_e1m6.{ext}"},  // E3M6
    {"3d85ec9c10b5ea465568", "d_e2m7.{ext}"},  // E3M7
    {"4d42e2ce1c1ff192500e", "d_e1m9.{ext}"},  // E3M9

};

// Given a time string (for LOOP_START/LOOP_END), parse it and return
// the time (in # samples since start of track) it represents.
static unsigned int ParseVorbisTime(unsigned int samplerate_hz, char *value) {
    char *num_start, *p;
    unsigned int result = 0;
    char c;

    if (strchr(value, ':') == NULL) {
        return atoi(value);
    }

    result = 0;
    num_start = value;

    for (p = value; *p != '\0'; ++p) {
        if (*p == '.' || *p == ':') {
            c = *p;
            *p = '\0';
            result = result * 60 + atoi(num_start);
            num_start = p + 1;
            *p = c;
        }

        if (*p == '.') {
            return result * samplerate_hz + (unsigned int)(atof(p) * samplerate_hz);
        }
    }

    return (result * 60 + atoi(num_start)) * samplerate_hz;
}

// Given a vorbis comment string (eg. "LOOP_START=12345"), set fields
// in the metadata structure as appropriate.
static void ParseVorbisComment(file_metadata_t *metadata, char *comment) {
    char *eq, *key, *value;

    eq = strchr(comment, '=');

    if (eq == NULL) {
        return;
    }

    key = comment;
    *eq = '\0';
    value = eq + 1;

    if (!strcmp(key, LOOP_START_TAG)) {
        metadata->start_time = ParseVorbisTime(metadata->samplerate_hz, value);
    } else if (!strcmp(key, LOOP_END_TAG)) {
        metadata->end_time = ParseVorbisTime(metadata->samplerate_hz, value);
    }
}

// Parse a vorbis comments structure, reading from the given file.
static void ParseVorbisComments(file_metadata_t *metadata, FILE *fs) {
    uint32_t buf;
    unsigned int num_comments, i, comment_len;
    char *comment;

    // We must have read the sample rate already from an earlier header.
    if (metadata->samplerate_hz == 0) {
        return;
    }

    // Skip the starting part we don't care about.
    if (fread(&buf, 4, 1, fs) < 1) {
        return;
    }
    if (fseek(fs, LONG(buf), SEEK_CUR) != 0) {
        return;
    }

    // Read count field for number of comments.
    if (fread(&buf, 4, 1, fs) < 1) {
        return;
    }
    num_comments = LONG(buf);

    // Read each individual comment.
    for (i = 0; i < num_comments; ++i) {
        // Read length of comment.
        if (fread(&buf, 4, 1, fs) < 1) {
            return;
        }

        comment_len = LONG(buf);

        // Read actual comment data into string buffer.
        comment = calloc(1, comment_len + 1);
        if (comment == NULL || fread(comment, 1, comment_len, fs) < comment_len) {
            free(comment);
            break;
        }

        // Parse comment string.
        ParseVorbisComment(metadata, comment);
        free(comment);
    }
}

static void ParseFlacStreaminfo(file_metadata_t *metadata, FILE *fs) {
    byte buf[34];

    // Read block data.
    if (fread(buf, sizeof(buf), 1, fs) < 1) {
        return;
    }

    // We only care about sample rate and song length.
    metadata->samplerate_hz = (buf[10] << 12) | (buf[11] << 4) | (buf[12] >> 4);
    // Song length is actually a 36 bit field, but 32 bits should be
    // enough for everybody.
    // metadata->song_length = (buf[14] << 24) | (buf[15] << 16)
    //                      | (buf[16] << 8) | buf[17];
}

static void ParseFlacFile(file_metadata_t *metadata, FILE *fs) {
    byte header[4];
    unsigned int block_type;
    size_t block_len;
    boolean last_block;

    for (;;) {
        long pos = -1;

        // Read METADATA_BLOCK_HEADER:
        if (fread(header, 4, 1, fs) < 1) {
            return;
        }

        block_type = header[0] & ~0x80;
        last_block = (header[0] & 0x80) != 0;
        block_len = (header[1] << 16) | (header[2] << 8) | header[3];

        pos = ftell(fs);
        if (pos < 0) {
            return;
        }

        if (block_type == FLAC_STREAMINFO) {
            ParseFlacStreaminfo(metadata, fs);
        } else if (block_type == FLAC_VORBIS_COMMENT) {
            ParseVorbisComments(metadata, fs);
        }

        if (last_block) {
            break;
        }

        // Seek to start of next block.
        if (fseek(fs, pos + block_len, SEEK_SET) != 0) {
            return;
        }
    }
}

static void ParseOggIdHeader(file_metadata_t *metadata, FILE *fs) {
    byte buf[21];

    if (fread(buf, sizeof(buf), 1, fs) < 1) {
        return;
    }

    metadata->samplerate_hz = (buf[8] << 24) | (buf[7] << 16) | (buf[6] << 8) | buf[5];
}

static void ParseOggFile(file_metadata_t *metadata, FILE *fs) {
    byte buf[7];
    unsigned int offset;

    // Scan through the start of the file looking for headers. They
    // begin '[byte]vorbis' where the byte value indicates header type.
    memset(buf, 0, sizeof(buf));

    for (offset = 0; offset < 100 * 1024; ++offset) {
        // buf[] is used as a sliding window. Each iteration, we
        // move the buffer one byte to the left and read an extra
        // byte onto the end.
        memmove(buf, buf + 1, sizeof(buf) - 1);

        if (fread(&buf[6], 1, 1, fs) < 1) {
            return;
        }

        if (!memcmp(buf + 1, "vorbis", 6)) {
            switch (buf[0]) {
            case OGG_ID_HEADER:
                ParseOggIdHeader(metadata, fs);
                break;
            case OGG_COMMENT_HEADER:
                ParseVorbisComments(metadata, fs);
                break;
            default:
                break;
            }
        }
    }
}

static void ReadLoopPoints(const char *filename, file_metadata_t *metadata) {
    FILE *fs;
    char header[4];

    metadata->valid = false;
    metadata->samplerate_hz = 0;
    metadata->start_time = 0;
    metadata->end_time = -1;

    fs = fopen(filename, "rb");

    if (fs == NULL) {
        return;
    }

    // Check for a recognized file format; use the first four bytes
    // of the file.

    if (fread(header, 4, 1, fs) < 1) {
        fclose(fs);
        return;
    }

    if (memcmp(header, FLAC_HEADER, 4) == 0) {
        ParseFlacFile(metadata, fs);
    } else if (memcmp(header, OGG_HEADER, 4) == 0) {
        ParseOggFile(metadata, fs);
    }

    fclose(fs);

    // Only valid if at the very least we read the sample rate.
    metadata->valid = metadata->samplerate_hz > 0;

    // If start and end time are both zero, ignore the loop tags.
    // This is consistent with other source ports.
    if (metadata->start_time == 0 && metadata->end_time == 0) {
        metadata->valid = false;
    }
}

// Given a MUS lump, look up a substitute MUS file to play instead
// (or NULL to just use normal MIDI playback).

static const char *GetSubstituteMusicFile(void *data, size_t data_len) {
    sha1_context_t context;
    sha1_digest_t hash;
    const char *filename;
    char hash_str[sizeof(sha1_digest_t) * 2 + 1];
    unsigned int i;

    // Don't bother doing a hash if we're never going to find anything.
    if (subst_music_len == 0) {
        return NULL;
    }

    SHA1_Init(&context);
    SHA1_Update(&context, data, data_len);
    SHA1_Final(hash, &context);

    // Build a string representation of the hash.
    for (i = 0; i < sizeof(sha1_digest_t); ++i) {
        M_snprintf(hash_str + i * 2, sizeof(hash_str) - i * 2, "%02x", hash[i]);
    }

    // Look for a hash that matches.
    // The substitute mapping list can (intentionally) contain multiple
    // filename mappings for the same hash. This allows us to try
    // different files and fall back if our first choice isn't found.

    filename = NULL;

    for (i = 0; i < subst_music_len; ++i) {
        if (M_StringStartsWith(hash_str, subst_music[i].hash_prefix)) {
            filename = subst_music[i].filename;

            // If the file exists, then use this file in preference to
            // any fallbacks. But we always return a filename if it's
            // in the list, even if it's just so we can print an error
            // message to the user saying it doesn't exist.
            if (M_FileExists(filename)) {
                break;
            }
        }
    }

    return filename;
}

static char *GetFullPath(const char *musicdir, const char *path) {
    char *result;
    char *systemized_path;

    // Starting with directory separator means we have an absolute path,
    // so just return it.
    if (path[0] == DIR_SEPARATOR) {
        return M_StringDuplicate(path);
    }

    // Paths in the substitute filenames can contain Unix-style /
    // path separators, but we should convert this to the separator
    // for the native platform.
    systemized_path = M_StringReplace(path, "/", DIR_SEPARATOR_S);

    // Copy config filename and cut off the filename to just get the
    // parent dir.
    result = M_StringJoin(musicdir, systemized_path, NULL);
    free(systemized_path);

    return result;
}

// If filename ends with .{ext}, check if a .ogg, .flac or .mp3 exists with
// that name, returning it if found. If none exist, NULL is returned. If the
// filename doesn't end with .{ext} then it just acts as a wrapper around
// GetFullPath().
static char *ExpandFileExtension(const char *musicdir, const char *filename) {
    static const char *extns[] = {".flac", ".ogg", ".mp3"};
    char *replaced, *result;
    int i;

    if (!M_StringEndsWith(filename, ".{ext}")) {
        return GetFullPath(musicdir, filename);
    }

    for (i = 0; i < arrlen(extns); ++i) {
        replaced = M_StringReplace(filename, ".{ext}", extns[i]);
        result = GetFullPath(musicdir, replaced);
        free(replaced);
        if (M_FileExists(result)) {
            return result;
        }
        free(result);
    }

    return NULL;
}

// Add a substitute music file to the lookup list.
static void AddSubstituteMusic(const char *musicdir, const char *hash_prefix, const char *filename) {
    subst_music_t *s;
    char *path;

    path = ExpandFileExtension(musicdir, filename);
    if (path == NULL) {
        return;
    }

    ++subst_music_len;
    subst_music = I_Realloc(subst_music, sizeof(subst_music_t) * subst_music_len);
    s = &subst_music[subst_music_len - 1];
    s->hash_prefix = hash_prefix;
    s->filename = path;
}

static const char *ReadHashPrefix(char *line) {
    char *result;
    char *p;
    int i, len;

    for (p = line; *p != '\0' && !isspace(*p) && *p != '='; ++p) {
        if (!isxdigit(*p)) {
            return NULL;
        }
    }

    len = p - line;
    if (len == 0 || len > sizeof(sha1_digest_t) * 2) {
        return NULL;
    }

    result = malloc(len + 1);
    if (result == NULL) {
        return NULL;
    }

    for (i = 0; i < len; ++i) {
        result[i] = tolower(line[i]);
    }
    result[len] = '\0';

    return result;
}

// Parse a line from substitute music configuration file; returns error
// message or NULL for no error.

static const char *ParseSubstituteLine(char *musicdir, char *line) {
    const char *hash_prefix;
    char *filename;
    char *p;

    // Strip out comments if present.
    p = strchr(line, '#');
    if (p != NULL) {
        while (p > line && isspace(*(p - 1))) {
            --p;
        }
        *p = '\0';
    }

    // Skip leading spaces.
    for (p = line; *p != '\0' && isspace(*p); ++p)
        ;

    // Empty line? This includes comment lines now that comments have
    // been stripped.
    if (*p == '\0') {
        return NULL;
    }

    hash_prefix = ReadHashPrefix(p);
    if (hash_prefix == NULL) {
        return "Invalid hash prefix";
    }

    p += strlen(hash_prefix);

    // Skip spaces.
    for (; *p != '\0' && isspace(*p); ++p)
        ;

    if (*p != '=') {
        return "Expected '='";
    }

    ++p;

    // Skip spaces.
    for (; *p != '\0' && isspace(*p); ++p)
        ;

    filename = p;

    // We're now at the filename. Cut off trailing space characters.
    while (strlen(p) > 0 && isspace(p[strlen(p) - 1])) {
        p[strlen(p) - 1] = '\0';
    }

    if (strlen(p) == 0) {
        return "No filename specified for music substitution";
    }

    // Expand full path and add to our database of substitutes.
    AddSubstituteMusic(musicdir, hash_prefix, filename);

    return NULL;
}

// Read a substitute music configuration file.

static boolean ReadSubstituteConfig(char *musicdir, const char *filename) {
    char *buffer;
    char *line;
    int linenum = 1;

    // This unnecessarily opens the file twice...
    if (!M_FileExists(filename)) {
        return false;
    }

    M_ReadFile(filename, (byte **)&buffer);

    line = buffer;

    while (line != NULL) {
        const char *error;
        char *next;

        // find end of line
        char *eol = strchr(line, '\n');
        if (eol != NULL) {
            // change the newline into NUL
            *eol = '\0';
            next = eol + 1;
        } else {
            // end of buffer
            next = NULL;
        }

        error = ParseSubstituteLine(musicdir, line);

        if (error != NULL) {
            fprintf(stderr, "%s:%i: Error: %s\n", filename, linenum, error);
        }

        ++linenum;
        line = next;
    }

    Z_Free(buffer);

    return true;
}

// Find substitute configs and try to load them.

static void LoadSubstituteConfigs(void) {
    glob_t *glob;
    char *musicdir;
    const char *path;
    unsigned int old_music_len;
    unsigned int i;

    // We can configure the path to music packs using the music_pack_path
    // configuration variable. Otherwise we use the current directory, or
    // $configdir/music to look for .cfg files.
    if (strcmp(music_pack_path, "") != 0) {
        musicdir = M_StringJoin(music_pack_path, DIR_SEPARATOR_S, NULL);
    } else if (!strcmp(configdir, "")) {
        musicdir = M_StringDuplicate("");
    } else {
        musicdir = M_StringJoin(configdir, "music", DIR_SEPARATOR_S, NULL);
    }

    // Load all music packs, by searching for .cfg files.
    glob = I_StartGlob(musicdir, "*.cfg", GLOB_FLAG_SORTED | GLOB_FLAG_NOCASE);
    for (;;) {
        path = I_NextGlob(glob);
        if (path == NULL) {
            break;
        }
        ReadSubstituteConfig(musicdir, path);
    }
    I_EndGlob(glob);

    if (subst_music_len > 0) {
        printf("Loaded %u music substitutions from config files.\n", subst_music_len);
    }

    old_music_len = subst_music_len;

    // Add entries from known filenames list. We add this after those from the
    // configuration files, so that the entries here can be overridden.
    for (i = 0; i < arrlen(known_filenames); ++i) {
        AddSubstituteMusic(musicdir, known_filenames[i].hash_prefix, known_filenames[i].filename);
    }

    if (subst_music_len > old_music_len) {
        printf("Configured %u music substitutions based on filename.\n", subst_music_len - old_music_len);
    }

    free(musicdir);
}

// Returns true if the given lump number is a music lump that should
// be included in substitute configs.
// Identifying music lumps by name is not feasible; some games (eg.
// Heretic, Hexen) don't have a common naming pattern for music lumps.

static boolean IsMusicLump(int lumpnum) {
    byte *data;
    boolean result;

    if (W_LumpLength(lumpnum) < 4) {
        return false;
    }

    data = W_CacheLumpNum(lumpnum, PU_STATIC);

    result = memcmp(data, MUS_HEADER_MAGIC, 4) == 0 || memcmp(data, MID_HEADER_MAGIC, 4) == 0;

    W_ReleaseLumpNum(lumpnum);

    return result;
}

// Dump an example config file containing checksums for all MIDI music
// found in the WAD directory.

static void DumpSubstituteConfig(const char *filename) {
    sha1_context_t context;
    sha1_digest_t digest;
    char name[9];
    byte *data;
    FILE *fs;
    unsigned int lumpnum;
    size_t h;

    fs = fopen(filename, "w");

    if (fs == NULL) {
        I_Error("Failed to open %s for writing", filename);
        return;
    }

    fprintf(fs, "# Example %s substitute MIDI file.\n\n", PACKAGE_NAME);
    fprintf(fs, "# SHA1 hash                              = filename\n");

    for (lumpnum = 0; lumpnum < numlumps; ++lumpnum) {
        strncpy(name, lumpinfo[lumpnum]->name, 8);
        name[8] = '\0';

        if (!IsMusicLump(lumpnum)) {
            continue;
        }

        // Calculate hash.
        data = W_CacheLumpNum(lumpnum, PU_STATIC);
        SHA1_Init(&context);
        SHA1_Update(&context, data, W_LumpLength(lumpnum));
        SHA1_Final(digest, &context);
        W_ReleaseLumpNum(lumpnum);

        // Print line.
        for (h = 0; h < sizeof(sha1_digest_t); ++h) {
            fprintf(fs, "%02x", digest[h]);
        }

        fprintf(fs, " = %s.ogg\n", name);
    }

    fprintf(fs, "\n");
    fclose(fs);

    printf("Substitute MIDI config file written to %s.\n", filename);
    I_Quit();
}

// Shutdown music

static void I_MP_ShutdownMusic(void) {
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

// Callback function that is invoked to track current track position.
void TrackPositionCallback(int len) {
    // Position is doubled up twice: for 16-bit samples and for stereo.
    current_track_pos += len / 4;
}

// Initialize music subsystem
static boolean I_MP_InitMusic(void) {
    int i;

    //!
    // @category obscure
    // @arg <filename>
    //
    // Read all MIDI files from loaded WAD files, dump an example substitution
    // music config file to the specified filename and quit.
    //
    i = M_CheckParmWithArgs("-dumpsubstconfig", 1);

    if (i > 0) {
        DumpSubstituteConfig(myargv[i + 1]);
    }

    // If we're in GENMIDI mode, try to load sound packs.
    LoadSubstituteConfigs();

    // We can't initialize if we don't have any substitute files to work with.
    // If so, don't bother with SDL initialization etc.
    if (subst_music_len == 0) {
        return false;
    }

    // If SDL_mixer is not initialized, we have to initialize it
    // and have the responsibility to shut it down later on.
    if (SDLIsInitialized()) {
        music_initialized = true;
    } else if (SDL_Init(SDL_INIT_AUDIO) < 0) {
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

    // Initialize SDL_Mixer for digital music playback
    Mix_Init(MIX_INIT_FLAC | MIX_INIT_OGG | MIX_INIT_MP3);

    // Register an effect function to track the music position.
    Mix_RegisterEffect(MIX_CHANNEL_POST, TrackPositionCallback, NULL, NULL);

    return music_initialized;
}

// Set music volume (0 - 127)

static void I_MP_SetMusicVolume(int volume) { Mix_VolumeMusic((volume * MIX_MAX_VOLUME) / 127); }

// Start playing a mid

static void I_MP_PlaySong(void *handle, boolean looping) {
    int loops;

    if (!music_initialized) {
        return;
    }

    if (handle == NULL) {
        return;
    }

    current_track_music = (Mix_Music *)handle;
    current_track_loop = looping;

    if (looping) {
        loops = -1;
    } else {
        loops = 1;
    }

    // Don't loop when playing substitute music, as we do it
    // ourselves instead.
    if (file_metadata.valid) {
        loops = 1;
        SDL_LockAudio();
        current_track_pos = 0; // start of track
        SDL_UnlockAudio();
    }

    if (Mix_PlayMusic(current_track_music, loops) == -1) {
        fprintf(stderr, "I_MP_PlaySong: Error starting track: %s\n", Mix_GetError());
    }
}

static void I_MP_PauseSong(void) {
    if (!music_initialized) {
        return;
    }

    Mix_PauseMusic();
}

static void I_MP_ResumeSong(void) {
    if (!music_initialized) {
        return;
    }

    Mix_ResumeMusic();
}

static void I_MP_StopSong(void) {
    if (!music_initialized) {
        return;
    }

    Mix_HaltMusic();
    current_track_music = NULL;
}

static void I_MP_UnRegisterSong(void *handle) {
    Mix_Music *music = (Mix_Music *)handle;

    if (!music_initialized) {
        return;
    }

    if (handle == NULL) {
        return;
    }

    Mix_FreeMusic(music);
}

static void *I_MP_RegisterSong(void *data, int len) {
    const char *filename;
    Mix_Music *music;

    if (!music_initialized) {
        return NULL;
    }

    // See if we're substituting this MUS for a high-quality replacement.
    filename = GetSubstituteMusicFile(data, len);
    if (filename == NULL) {
        return NULL;
    }

    music = Mix_LoadMUS(filename);
    if (music == NULL) {
        // Fall through and play MIDI normally, but print an error
        // message.
        fprintf(stderr, "Failed to load substitute music file: %s: %s\n", filename, Mix_GetError());
        return NULL;
    }

    // Read loop point metadata from the file so that we know where
    // to loop the music.
    ReadLoopPoints(filename, &file_metadata);
    return music;
}

// Is the song playing?
static boolean I_MP_MusicIsPlaying(void) {
    if (!music_initialized) {
        return false;
    }

    return Mix_PlayingMusic();
}

// Get position in substitute music track, in seconds since start of track.
static double GetMusicPosition(void) {
    unsigned int music_pos;
    int freq;

    Mix_QuerySpec(&freq, NULL, NULL);

    SDL_LockAudio();
    music_pos = current_track_pos;
    SDL_UnlockAudio();

    return (double)music_pos / freq;
}

static void RestartCurrentTrack(void) {
    double start = (double)file_metadata.start_time / file_metadata.samplerate_hz;

    // If the track finished we need to restart it.
    if (current_track_music != NULL) {
        Mix_PlayMusic(current_track_music, 1);
    }

    Mix_SetMusicPosition(start);
    SDL_LockAudio();
    current_track_pos = file_metadata.start_time;
    SDL_UnlockAudio();
}

// Poll music position; if we have passed the loop point end position
// then we need to go back.
static void I_MP_PollMusic(void) {
    // When playing substitute tracks, loop tags only apply if we're playing
    // a looping track. Tracks like the title screen music have the loop
    // tags ignored.
    if (current_track_loop && file_metadata.valid) {
        double end = (double)file_metadata.end_time / file_metadata.samplerate_hz;

        // If we have reached the loop end point then we have to take action.
        if (file_metadata.end_time >= 0 && GetMusicPosition() >= end) {
            RestartCurrentTrack();
        }

        // Have we reached the actual end of track (not loop end)?
        if (!Mix_PlayingMusic()) {
            RestartCurrentTrack();
        }
    }
}

music_module_t music_pack_module = {
    NULL,
    0,
    I_MP_InitMusic,
    I_MP_ShutdownMusic,
    I_MP_SetMusicVolume,
    I_MP_PauseSong,
    I_MP_ResumeSong,
    I_MP_RegisterSong,
    I_MP_UnRegisterSong,
    I_MP_PlaySong,
    I_MP_StopSong,
    I_MP_MusicIsPlaying,
    I_MP_PollMusic,
};
