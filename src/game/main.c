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
//	DOOM main program (D_DoomMain) and game loop (game_loop),
//	plus functions to determine game mode (shareware, registered),
//	parse command line parameters, configure game parameters (turbo),
//	and call the startup functions.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "main.h"

#include "../../config.h"
#include "def.h"
#include "stat.h"

#include "strings.h"
#include "../sound/sounds.h"

#include "../wad/iwad.h"

#include "../sound/sound.h"
#include "../video/diskicon.h"
#include "../video/video.h"
#include "../wad/main.h"
#include "../wad/wad.h"
#include "../mem/zone.h"

#include "finale.h"
#include "wipe.h"

#include "../lib/argv.h"
#include "../misc/config.h"
#include "controls.h"
#include "../menu/menu.h"
#include "../misc/misc.h"
#include "../player/savegame.h"

#include "../impl/endoom.h"
#include "../impl/input.h"
#include "../impl/joystick.h"
#include "../impl/system.h"
#include "../impl/timer.h"
#include "../impl/video.h"
#include "../impl/sound.h"

#include "../automap/automap.h"
#include "../hud/stuff.h"
#include "../net/client.h"
#include "../net/server.h"
#include "../net/query.h"
#include "../status/stuff.h"
#include "../window/stuff.h"

#include "../player/setup.h"
#include "../renderer/local.h"

//
// D-DoomLoop()
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//
void game_loop(void);

static char *gamedescription;

// Location where savegames are stored

char *savegamedir;

// location of IWAD and WAD files

char *iwadfile;

boolean devparm;     // started game with -devparm
boolean nomonsters;  // checkparm of -nomonsters
boolean respawnparm; // checkparm of -respawn
boolean fastparm;    // checkparm of -fast

// extern int soundVolume;
// extern  int	sfxVolume;
// extern  int	musicVolume;

extern boolean inhelpscreens;

skill_t startskill;
int startepisode;
int startmap;
boolean autostart;
int startloadgame;

boolean advancedemo;

// Store demo, do not accept any inputs
boolean storedemo;

// If true, the main game loop has started.
boolean main_loop_started = false;

char wadfile[1024]; // primary wad file
char mapdir[1024];  // directory of development maps

int show_endoom = 1;
int show_diskicon = 1;

void D_ConnectNetGame(void);
void D_CheckNetGame(void);

//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents(void) {
    event_t *ev;

    // IF STORE DEMO, DO NOT ACCEPT INPUT
    if (storedemo)
        return;

    while ((ev = D_PopEvent()) != NULL) {
        if (M_Responder(ev))
            continue; // menu ate the event
        G_Responder(ev);
    }
}

//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t wipegamestate = GS_DEMOSCREEN;
extern boolean setsizeneeded;
extern int showMessages;
void R_ExecuteSetViewSize(void);

boolean D_Display(void) {
    static boolean viewactivestate = false;
    static boolean menuactivestate = false;
    static boolean inhelpscreensstate = false;
    static boolean fullscreen = false;
    static gamestate_t oldgamestate = -1;
    static int borderdrawcount;
    boolean wipe;
    boolean redrawsbar;

    redrawsbar = false;

    // change the view size if needed
    if (setsizeneeded) {
        R_ExecuteSetViewSize();
        oldgamestate = -1; // force background redraw
        borderdrawcount = 3;
    }

    // save the current screen if about to wipe
    if (gamestate != wipegamestate) {
        wipe = true;
        wipe_StartScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);
    } else
        wipe = false;

    if (gamestate == GS_LEVEL && gametic)
        HU_Erase();

    // do buffered drawing
    switch (gamestate) {
    case GS_LEVEL:
        if (!gametic)
            break;
        if (automapactive)
            AM_Drawer();
        if (wipe || (viewheight != SCREENHEIGHT && fullscreen))
            redrawsbar = true;
        if (inhelpscreensstate && !inhelpscreens)
            redrawsbar = true; // just put away the help screen
        ST_Drawer(viewheight == SCREENHEIGHT, redrawsbar);
        fullscreen = viewheight == SCREENHEIGHT;
        break;

    case GS_INTERMISSION:
        WI_Drawer();
        break;

    case GS_FINALE:
        F_Drawer();
        break;

    case GS_DEMOSCREEN:
        D_PageDrawer();
        break;
    }

    // draw buffered stuff to screen
    I_UpdateNoBlit();

    // draw the view directly
    if (gamestate == GS_LEVEL && !automapactive && gametic)
        R_RenderPlayerView(&players[displayplayer]);

    if (gamestate == GS_LEVEL && gametic)
        HU_Drawer();

    // clean up border stuff
    if (gamestate != oldgamestate && gamestate != GS_LEVEL)
        I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));

    // see if the border needs to be initially drawn
    if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL) {
        viewactivestate = false; // view was not active
        R_FillBackScreen();      // draw the pattern into the back screen
    }

    // see if the border needs to be updated to the screen
    if (gamestate == GS_LEVEL && !automapactive && scaledviewwidth != SCREENWIDTH) {
        if (menuactive || menuactivestate || !viewactivestate)
            borderdrawcount = 3;
        if (borderdrawcount) {
            R_DrawViewBorder(); // erase old menu stuff
            borderdrawcount--;
        }
    }

    if (testcontrols) {
        // Box showing current mouse speed

        V_DrawMouseSpeedBox(testcontrols_mousespeed);
    }

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;

    // draw pause pic
    if (paused) {
        int y;
        if (automapactive)
            y = 4;
        else
            y = viewwindowy + 4;
        V_DrawPatchDirect(viewwindowx + (scaledviewwidth - 68) / 2, y, W_CacheLumpName("M_PAUSE", PU_CACHE));
    }

    // menus go directly to the screen
    M_Drawer();  // menu is drawn even on top of everything
    NetUpdate(); // send out any new accumulation

    return wipe;
}

static void EnableLoadingDisk(void) {

    if (show_diskicon) {
        const char *disk_lump_name;
        if (M_CheckParm("-cdrom") > 0) {
            disk_lump_name = "STCDROM";
        } else {
            disk_lump_name = "STDISK";
        }

        V_EnableLoadingDisk(disk_lump_name, SCREENWIDTH - LOADING_DISK_W, SCREENHEIGHT - LOADING_DISK_H);
    }
}

//
// Add configuration file variable bindings.
//

static const char *const chat_macro_defaults[10] = {
    HUSTR_CHATMACRO0, HUSTR_CHATMACRO1, HUSTR_CHATMACRO2, HUSTR_CHATMACRO3, HUSTR_CHATMACRO4,
    HUSTR_CHATMACRO5, HUSTR_CHATMACRO6, HUSTR_CHATMACRO7, HUSTR_CHATMACRO8, HUSTR_CHATMACRO9};

void D_BindVariables(void) {
    int i;

    I_BindInputVariables();
    I_BindVideoVariables();
    I_BindJoystickVariables();
    I_BindSoundVariables();

    M_BindBaseControls();
    M_BindWeaponControls();
    M_BindMapControls();
    M_BindMenuControls();
    M_BindChatControls(MAXPLAYERS);

    key_multi_msgplayer[0] = HUSTR_KEYGREEN;
    key_multi_msgplayer[1] = HUSTR_KEYINDIGO;
    key_multi_msgplayer[2] = HUSTR_KEYBROWN;
    key_multi_msgplayer[3] = HUSTR_KEYRED;

    NET_BindVariables();

    M_BindIntVariable("mouse_sensitivity", &mouseSensitivity);
    M_BindIntVariable("sfx_volume", &sfxVolume);
    M_BindIntVariable("music_volume", &musicVolume);
    M_BindIntVariable("show_messages", &showMessages);
    M_BindIntVariable("screenblocks", &screenblocks);
    M_BindIntVariable("detaillevel", &detailLevel);
    M_BindIntVariable("snd_channels", &snd_channels);
    M_BindIntVariable("vanilla_savegame_limit", &vanilla_savegame_limit);
    M_BindIntVariable("vanilla_demo_limit", &vanilla_demo_limit);
    M_BindIntVariable("show_endoom", &show_endoom);
    M_BindIntVariable("show_diskicon", &show_diskicon);

    // Multiplayer chat macros

    for (i = 0; i < 10; ++i) {
        char buf[12];

        chat_macros[i] = M_StringDuplicate(chat_macro_defaults[i]);
        M_snprintf(buf, sizeof(buf), "chatmacro%i", i);
        M_BindStringVariable(buf, &chat_macros[i]);
    }
}

//
// D_GrabMouseCallback
//
// Called to determine whether to grab the mouse pointer
//

boolean D_GrabMouseCallback(void) {
    // Drone players don't need mouse focus

    if (drone)
        return false;

    // when menu is active or game is paused, release the mouse

    if (menuactive || paused)
        return false;

    // only grab mouse when playing levels (but not demos)

    return (gamestate == GS_LEVEL) && !demoplayback && !advancedemo;
}

//
//  D_RunFrame
//
void D_RunFrame() {
    static int wipestart;
    static boolean wipe;

    if (wipe) {
        int tics;
        int nowtime;
        do {
            nowtime = I_GetTime();
            tics = nowtime - wipestart;
            I_Sleep(1);
        } while (tics <= 0);

        wipestart = nowtime;
        wipe = !wipe_ScreenWipe(wipe_Melt, SCREENWIDTH, SCREENHEIGHT, tics);
        I_UpdateNoBlit();
        M_Drawer();       // menu is drawn even on top of wipes
        I_FinishUpdate(); // page flip or blit buffer
        return;
    }

    // frame syncronous IO operations
    I_StartFrame();

    TryRunTics(); // will run at least one tic

    S_UpdateSounds(players[consoleplayer].mo); // move positional sounds

    // Update display, next frame, with current state if no profiling is on
    if (screenvisible && !nodrawers) {
        if ((wipe = D_Display())) {
            // start wipe on this frame
            wipe_EndScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);

            wipestart = I_GetTime() - 1;
        } else {
            // normal update
            I_FinishUpdate(); // page flip or blit buffer
        }
    }
}

/** This is the main game loop for Doom **/
void game_loop(void) {
    if (demorecording)
        G_BeginRecording();

    main_loop_started = true;

    I_SetWindowTitle(gamedescription);
    // Check any command-line parameter containing to graphics settings
    I_GraphicsCheckCommandLine();
    I_SetGrabMouseCallback(D_GrabMouseCallback);
    I_InitGraphics();
    EnableLoadingDisk();

    TryRunTics();

    V_RestoreBuffer();
    R_ExecuteSetViewSize();

    D_StartGameLoop();

    if (testcontrols) {
        wipegamestate = gamestate;
    }

    while (1) {
        D_RunFrame();
    }
}

//
//  DEMO LOOP
//
int demosequence;
int pagetic;
const char *pagename;

//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker(void) {
    if (--pagetic < 0)
        D_AdvanceDemo();
}

//
// D_PageDrawer
//
void D_PageDrawer(void) { V_DrawPatch(0, 0, W_CacheLumpName(pagename, PU_CACHE)); }

//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo(void) { advancedemo = true; }

//
// This cycles through the demo sequences.
// FIXME - version dependend demo numbers?
//
void D_DoAdvanceDemo(void) {
    players[consoleplayer].playerstate = PST_LIVE; // not reborn
    advancedemo = false;
    usergame = false; // no save / end game here
    paused = false;
    gameaction = ga_nothing;

    demosequence = (demosequence + 1) % 6;

    switch (demosequence) {
    case 0:
        pagetic = 170;
        gamestate = GS_DEMOSCREEN;
        pagename = "TITLEPIC";
        S_StartMusic(mus_intro);
        break;
    case 1:
        G_DeferedPlayDemo("demo1");
        break;
    case 2:
        pagetic = 200;
        gamestate = GS_DEMOSCREEN;
        pagename = "CREDIT";
        break;
    case 3:
        G_DeferedPlayDemo("demo2");
        break;
    case 4:
        gamestate = GS_DEMOSCREEN;
        pagetic = 200;
        pagename = "HELP2";
        break;
    case 5:
        G_DeferedPlayDemo("demo3");
        break;
    }
}

//
// D_StartTitle
//
void D_StartTitle(void) {
    gameaction = ga_nothing;
    demosequence = -1;
    D_AdvanceDemo();
}

/** Find out what version of Doom is playing. **/

void D_IdentifyVersion(void) {
    // gamemission is set up by the D_FindIWAD function.  But if
    // we specify '-iwad', we have to identify using
    // IdentifyIWADByName.  However, if the iwad does not match
    // any known IWAD name, we may have a dilemma.  Try to
    // identify by its contents.

    if (gamemission == none) {
        unsigned int i;

        for (i = 0; i < numlumps; ++i) {
            if (!strncasecmp(lumpinfo[i]->name, "E1M1", 8)) {
                gamemission = doom;
                break;
            }
        }

        if (gamemission == none) {
            // Still no idea.  I don't think this is going to work.

            error("Unknown or invalid IWAD file.");
        }
    }

    // Make sure gamemode is set up correctly

    if (logical_gamemission == doom) {
        // Doom 1.  But which version?
        if (W_CheckNumForName("E3M1") > 0) {
            gamemode = registered;
        } else {
            gamemode = shareware;
        }
    }
}

// Set the gamedescription string

static void D_SetGameDescription(void) {
    if (logical_gamemission == doom) {
        if (gamemode == registered) {
            gamedescription = "DOOM Registered";
        } else if (gamemode == shareware) {
            gamedescription = "DOOM Shareware";
        }
    }
    if (gamedescription == NULL) {
        gamedescription = M_StringDuplicate("Unknown");
    }
}

//      print title for every printed line
char title[128];

static boolean D_AddFile(char *filename) {
    wad_file_t *handle;

    printf(" adding %s\n", filename);
    handle = W_AddFile(filename);

    return handle != NULL;
}

// Initialize the game version

static void InitGameVersion(void) {
    if (gamemode == shareware || gamemode == registered) {
        gameversion = exe_doom_1_9;
    }
}

// Function called at exit to display the ENDOOM screen

static void D_Endoom(void) {
    byte *endoom;

    // Don't show ENDOOM if we have it disabled, or we're running
    // in screensaver or control test mode. Only show it once the
    // game has actually started.

    if (!show_endoom || !main_loop_started || screensaver_mode || M_CheckParm("-testcontrols") > 0) {
        return;
    }

    endoom = W_CacheLumpName("ENDOOM", PU_STATIC);

    I_Endoom(endoom);
}

static void G_CheckDemoStatusAtExit(void) { G_CheckDemoStatus(); }

/**
 * @brief Main function that starts a game client.
 *
 * Command-line arguments are handled here. IWAD and config files are loaded
 * here. Keybindings are set here. Connecting to and querying servers
 * handled here. Important subsystems are initialized here.
 */
int main(int argc, char **argv) {
    // save arguments

    myargc = argc;
    myargv = argv;

    int p;
    char file[256];
    char demolumpname[9];

    I_AtExit(D_Endoom, false);

    //!
    // Print the program version and exit.
    //
    if (M_ParmExists("-version") || M_ParmExists("--version")) {
        puts(PACKAGE_STRING);
        exit(0);
    }

    M_FindResponseFile();

#ifdef SDL_HINT_NO_SIGNAL_HANDLERS
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
#endif

    if (M_ParmExists("-version") || M_ParmExists("--version")) {
        puts(PACKAGE_STRING);
        exit(0);
    }

    M_FindResponseFile();

#ifdef SDL_HINT_NO_SIGNAL_HANDLERS
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
#endif

    printf("Z_Init: Init zone memory allocation daemon.");
    Z_Init();

    // Start a dedicated server, routing packets but not participating
    // in the game itself.

    if (M_CheckParm("-dedicated") > 0) {
        printf("Dedicated server mode.\n");
        net_server_init();

        // Never returns
    }

    //!
    // @category net
    //
    // Query the Internet master server for a global list of active
    // servers.
    //

    if (M_CheckParm("-search")) {
        NET_MasterQuery();
        exit(0);
    }

    //!
    // @arg <address>
    // @category net
    //
    // Query the status of the server running on the given IP
    // address.
    //

    p = M_CheckParmWithArgs("-query", 1);

    if (p) {
        net_queryAddress(myargv[p + 1]);
        exit(0);
    }

    //!
    // @category net
    //
    // Search the local LAN for running servers.
    //

    if (M_CheckParm("-localsearch")) {
        NET_LANQuery();
        exit(0);
    }

    //!
    // @category game
    // @vanilla
    //
    // Disable monsters.
    //

    nomonsters = M_CheckParm("-nomonsters");

    //!
    // @category game
    // @vanilla
    //
    // Monsters respawn after being killed.
    //

    respawnparm = M_CheckParm("-respawn");

    //!
    // @category game
    // @vanilla
    //
    // Monsters move faster.
    //

    fastparm = M_CheckParm("-fast");

    //!
    // @vanilla
    //
    // Developer mode.
    //

    devparm = M_CheckParm("-devparm");

    I_DisplayFPSDots(devparm);

    //!
    // @category net
    // @vanilla
    //
    // Start a deathmatch game.
    //

    if (M_CheckParm("-deathmatch"))
        deathmatch = 1;

    //!
    // @category net
    // @vanilla
    //
    // Start a deathmatch 2.0 game.  Weapons do not stay in place and
    // all items respawn after 30 seconds.
    //

    if (M_CheckParm("-altdeath"))
        deathmatch = 2;

    if (devparm)
        printf(D_DEVSTR);

    // find which dir to use for config files

    // Auto-detect the configuration dir.

    M_SetConfigDir(NULL);

    //!
    // @category game
    // @arg <x>
    // @vanilla
    //
    // Turbo mode.  The player's speed is multiplied by x%.  If unspecified,
    // x defaults to 200.  Values are rounded up to 10 and down to 400.
    //

    if ((p = M_CheckParm("-turbo"))) {
        int scale = 200;
        extern int forwardmove[2];
        extern int sidemove[2];

        if (p < myargc - 1)
            scale = atoi(myargv[p + 1]);
        if (scale < 10)
            scale = 10;
        if (scale > 400)
            scale = 400;
        printf("turbo scale: %i%%\n", scale);
        forwardmove[0] = forwardmove[0] * scale / 100;
        forwardmove[1] = forwardmove[1] * scale / 100;
        sidemove[0] = sidemove[0] * scale / 100;
        sidemove[1] = sidemove[1] * scale / 100;
    }

    // init subsystems
    printf("V_Init: allocate screens.\n");

    // Load configuration files before initialising other subsystems.
    printf("M_LoadDefaults: Load system defaults.\n");
    M_SetConfigFilenames("default.cfg", "zendoom.cfg");
    D_BindVariables();
    M_LoadDefaults();

    // Save configuration at exit.
    I_AtExit(M_SaveDefaults, false);

    // Find main IWAD file and load it.
    iwadfile = D_FindIWAD(IWAD_MASK_DOOM, &gamemission);

    // None found?

    if (iwadfile == NULL) {
        error("Game mode indeterminate.  No IWAD file was found.  Try\n"
              "specifying one with the '-iwad' command line parameter.\n");
    }

    modifiedgame = false;

    printf("W_Init: Init WADfiles.\n");
    D_AddFile(iwadfile);

    W_CheckCorrectIWAD(doom);

    // Now that we've loaded the IWAD, we can figure out what gamemission
    // we're playing and which version of Vanilla Doom we need to emulate.
    D_IdentifyVersion();
    InitGameVersion();

    if (!M_ParmExists("-noautoload") && gamemode != shareware) {
        char *autoload_dir;

        // common auto-loaded files for all Doom flavors

        autoload_dir = M_GetAutoloadDir("doom-all");
        W_AutoLoadWADs(autoload_dir);
        free(autoload_dir);

        // auto-loaded files per IWAD
        autoload_dir = M_GetAutoloadDir(D_SaveGameIWADName(gamemission));
        W_AutoLoadWADs(autoload_dir);
        free(autoload_dir);
    }

    // Load PWAD files.
    modifiedgame = W_ParseCommandLine();

    //!
    // @arg <demo>
    // @category demo
    // @vanilla
    //
    // Play back the demo named demo.lmp.
    //

    p = M_CheckParmWithArgs("-playdemo", 1);

    if (!p) {
        //!
        // @arg <demo>
        // @category demo
        // @vanilla
        //
        // Play back the demo named demo.lmp, determining the framerate
        // of the screen.
        //
        p = M_CheckParmWithArgs("-timedemo", 1);
    }

    if (p) {
        char *uc_filename = strdup(myargv[p + 1]);
        M_ForceUppercase(uc_filename);

        // With Vanilla you have to specify the file without extension,
        // but make that optional.
        if (M_StringEndsWith(uc_filename, ".LMP")) {
            M_StringCopy(file, myargv[p + 1], sizeof(file));
        } else {
            snprintf(file, sizeof(file), "%s.lmp", myargv[p + 1]);
        }

        free(uc_filename);

        if (D_AddFile(file)) {
            M_StringCopy(demolumpname, lumpinfo[numlumps - 1]->name, sizeof(demolumpname));
        } else {
            // If file failed to load, still continue trying to play
            // the demo in the same way as Vanilla Doom.  This makes
            // tricks like "-playdemo demo1" possible.

            M_StringCopy(demolumpname, myargv[p + 1], sizeof(demolumpname));
        }

        printf("Playing demo %s.\n", file);
    }

    I_AtExit(G_CheckDemoStatusAtExit, true);

    // Generate the WAD hash table.  Speed things up a bit.
    W_GenerateHashTable();

    // Set the gamedescription string. This is only possible now that
    // we've finished loading Dehacked patches.
    D_SetGameDescription();

    savegamedir = M_GetSaveGameDir(D_SaveGameIWADName(gamemission));

    // Check for -file in shareware
    if (modifiedgame) {
        // These are the lumps that will be checked in IWAD,
        // if any one is not present, execution will be aborted.

        if (gamemode == shareware)
            error("\nYou cannot -file with the shareware "
                  "version. Register!");

        // Check for fake IWAD with right name,
        // but w/o all the lumps of the registered version.
        if (gamemode == registered)
            for (int i = 0; i < 23; i++) {
                char name[23][8] = {"e2m1", "e2m2", "e2m3",   "e2m4",   "e2m5",   "e2m6",   "e2m7",    "e2m8",
                                    "e2m9", "e3m1", "e3m3",   "e3m3",   "e3m4",   "e3m5",   "e3m6",    "e3m7",
                                    "e3m8", "e3m9", "dphoof", "bfgga0", "heada1", "cybra1", "spida1d1"};
                if (W_CheckNumForName(name[i]) < 0)
                    error("\nThis is not the registered version.");
            }
    }

    if (W_CheckNumForName("SS_START") >= 0 || W_CheckNumForName("FF_END") >= 0) {
        I_PrintDivider();
        printf(" WARNING: The loaded WAD file contains modified sprites or\n"
               " floor textures.  You may want to use the '-merge' command\n"
               " line option instead of '-file'.\n");
    }

    I_PrintStartupBanner(gamedescription);

    printf("I_Init: Setting up machine state.\n");
    I_CheckIsScreensaver();
    I_InitTimer();
    I_InitJoystick();
    I_InitSound(true);
    I_InitMusic();

    printf("NET_Init: Init network subsystem.\n");
    NET_Init();

    // Initial netgame startup. Connect to server etc.
    D_ConnectNetGame();

    // get skill / episode / map from parms
    startskill = sk_medium;
    startepisode = 1;
    startmap = 1;
    autostart = false;

    //!
    // @category game
    // @arg <skill>
    // @vanilla
    //
    // Set the game skill, 1-5 (1: easiest, 5: hardest).  A skill of
    // 0 disables all monsters.
    //

    p = M_CheckParmWithArgs("-skill", 1);

    if (p) {
        startskill = myargv[p + 1][0] - '1';
        autostart = true;
    }

    //!
    // @category game
    // @arg <n>
    // @vanilla
    //
    // Start playing on episode n (1-4)
    //

    p = M_CheckParmWithArgs("-episode", 1);

    if (p) {
        startepisode = myargv[p + 1][0] - '0';
        startmap = 1;
        autostart = true;
    }

    timelimit = 0;

    //!
    // @arg <n>
    // @category net
    // @vanilla
    //
    // For multiplayer games: exit each level after n minutes.
    //

    p = M_CheckParmWithArgs("-timer", 1);

    if (p) {
        timelimit = atoi(myargv[p + 1]);
    }

    //!
    // @category net
    // @vanilla
    //
    // Austin Virtual Gaming: end levels after 20 minutes.
    //

    p = M_CheckParm("-avg");

    if (p) {
        timelimit = 20;
    }

    //!
    // @category game
    // @arg [<x> <y> | <xy>]
    // @vanilla
    //
    // Start a game immediately, warping to ExMy (Doom 1) or MAPxy
    // (Doom 2)
    //

    p = M_CheckParmWithArgs("-warp", 1);

    if (p) {
        startepisode = myargv[p + 1][0] - '0';

        if (p + 2 < myargc) {
            startmap = myargv[p + 2][0] - '0';
        } else {
            startmap = 1;
        }
        autostart = true;
    }

    // Undocumented:
    // Invoked by setup to test the controls.

    p = M_CheckParm("-testcontrols");

    if (p > 0) {
        startepisode = 1;
        startmap = 1;
        autostart = true;
        testcontrols = true;
    }

    // Check for load game parameter
    // We do this here and save the slot number, so that the network code
    // can override it or send the load slot to other players.

    //!
    // @category game
    // @arg <s>
    // @vanilla
    //
    // Load the game in slot s.
    //

    p = M_CheckParmWithArgs("-loadgame", 1);

    if (p) {
        startloadgame = atoi(myargv[p + 1]);
    } else {
        // Not loading a game
        startloadgame = -1;
    }

    printf("M_Init: Init miscellaneous info.");
    M_Init();

    printf("R_Init: Init DOOM refresh daemon - ");
    R_Init();

    printf("P_Init: Init Playloop state.");
    P_Init();

    printf("S_Init: Setting up sound.");
    S_Init(sfxVolume * 8, musicVolume * 8);

    printf("D_CheckNetGame: Checking network game status.");
    D_CheckNetGame();

    printf("HU_Init: Setting up heads up display.");
    HU_Init();

    printf("ST_Init: Init status bar.");
    ST_Init();

    //!
    // @arg <x>
    // @category demo
    // @vanilla
    //
    // Record a demo named x.lmp.
    //

    p = M_CheckParmWithArgs("-record", 1);

    if (p) {
        G_RecordDemo(myargv[p + 1]);
        autostart = true;
    }

    p = M_CheckParmWithArgs("-playdemo", 1);
    if (p) {
        singledemo = true; // quit after one demo
        G_DeferedPlayDemo(demolumpname);
        game_loop(); // never returns
    }

    p = M_CheckParmWithArgs("-timedemo", 1);
    if (p) {
        G_TimeDemo(demolumpname);
        game_loop(); // never returns
    }

    if (startloadgame >= 0) {
        M_StringCopy(file, P_SaveGameFile(startloadgame), sizeof(file));
        G_LoadGame(file);
    }

    if (gameaction != ga_loadgame) {
        if (autostart || netgame)
            G_InitNew(startskill, startepisode, startmap);
        else
            D_StartTitle(); // start up intro loop
    }

    game_loop(); // never returns
    return 0;
}
