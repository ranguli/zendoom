//
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
//
// DESCRIPTION:
//   Functions and definitions relating to the game type and operational
//   mode.
//

#include "doomtype.h"
#include "d_mode.h"

// Valid game mode/mission combinations, with the number of
// episodes/maps for each.

static struct
{
    GameMission_t mission;
    GameMode_t mode;
    int episode;
    int map;
} valid_modes[] = {
    { pack_chex, retail,     1, 5 },
    { doom,      shareware,  1, 9 },
    { doom,      registered, 3, 9 },
    { doom,      retail,     4, 9 }
};

// Check that a gamemode+gamemission received over the network is valid.

boolean D_ValidGameMode(GameMission_t mission, GameMode_t mode)
{
    int i;

    for (i=0; i<arrlen(valid_modes); ++i)
    {
        if (valid_modes[i].mode == mode && valid_modes[i].mission == mission)
        {
            return true;
        }
    }

    return false;
}

boolean D_ValidEpisodeMap(GameMission_t mission, GameMode_t mode,
                          int episode, int map)
{
    int i;

    // Hacks for Heretic secret episodes

    if (mission == heretic)
    {
        if (mode == retail && episode == 6)
        {
            return map >= 1 && map <= 3;
        }
        else if (mode == registered && episode == 4)
        {
            return map == 1;
        }
    }

    // Find the table entry for this mission/mode combination.

    for (i=0; i<arrlen(valid_modes); ++i)
    {
        if (mission == valid_modes[i].mission
         && mode == valid_modes[i].mode)
        {
            return episode >= 1 && episode <= valid_modes[i].episode
                && map >= 1 && map <= valid_modes[i].map;
        }
    }

    // Unknown mode/mission combination

    return false;
}

// Table of valid versions

static struct {
    GameMission_t mission;
    GameVersion_t version;
} valid_versions[] = {
    { doom,     exe_doom_1_2 },
    { doom,     exe_doom_1_666 },
    { doom,     exe_doom_1_7 },
    { doom,     exe_doom_1_8 },
    { doom,     exe_doom_1_9 },
    { doom,     exe_ultimate }
};

boolean D_ValidGameVersion(GameMission_t mission, GameVersion_t version)
{
    int i;

    // All Doom variants can use the Doom versions.

    if (mission == doom2 || mission == pack_plut || mission == pack_tnt
     || mission == pack_hacx || mission == pack_chex)
    {
        mission = doom;
    }

    for (i=0; i<arrlen(valid_versions); ++i)
    {
        if (valid_versions[i].mission == mission
         && valid_versions[i].version == version)
        {
            return true;
        }
    }

    return false;
}

const char *D_GameMissionString(GameMission_t mission)
{
    switch (mission)
    {
        case none:
        default:
            return "none";
        case doom:
            return "doom";
    }
}

const char *D_GameModeString(GameMode_t mode)
{
    switch (mode)
    {
        case shareware:
            return "shareware";
        case registered:
            return "registered";
        case retail:
            return "retail";
        case indetermined:
        default:
            return "unknown";
    }
}

