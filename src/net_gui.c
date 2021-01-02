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
// Graphical stuff related to the networking code:
//
//  * The client waiting screen when we are waiting for the server to
//    start the game.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "doomkeys.h"

#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_misc.h"

#include "net_client.h"
#include "net_gui.h"
#include "net_query.h"
#include "net_server.h"

static int old_max_players;
static char *player_labels[NET_MAXPLAYERS];
static char *ip_labels[NET_MAXPLAYERS];
static boolean had_warning;

// Number of players we expect to be in the game. When the number is
// reached, we auto-start the game (if we're the controller). If
// zero, do not autostart.
static int expected_nodes;

static void BuildWindow(void) {
    int i;

    // Player labels

    for (i = 0; i < net_client_wait_data.max_players; ++i) {
        printf(" %i. %s %s\n", i + 1, player_labels[i], ip_labels[i]);
        player_labels[i] = "";
        ip_labels[i] = "";
    }
}

static void UpdateGUI(void) {
    unsigned int i;

    // If the value of max_players changes, we must rebuild the
    // contents of the window. This includes when the first
    // waiting data packet is received.

    if (net_client_received_wait_data) {
        if (net_client_wait_data.max_players != old_max_players) {
            BuildWindow();
        }
    } else {
        return;
    }

    for (i = 0; i < net_client_wait_data.max_players; ++i) {
        if (i < net_client_wait_data.num_players) {
            player_labels[i] = net_client_wait_data.player_names[i];
            ip_labels[i] = net_client_wait_data.player_addrs[i];
        } else {
            player_labels[i] = "";
            ip_labels[i] = "";
        }
    }

    if (net_client_wait_data.is_controller) {
        printf("You are the controller. Type \"s\" to start server when all "
               "players are connected, or any key to refresh the lobby\n");
        if (getchar() == 's') {
            NET_CL_LaunchGame();
        }
    }

    printf("\e[1;1H\e[2J"); // Clear screen
}

static void CheckMasterStatus(void) {
    boolean added;

    if (!NET_Query_CheckAddedToMaster(&added)) {
        return;
    }

    if (added) {
        printf("Your server is now registered with the global master server.\n"
               "Other players can find your server online.");
    } else {
        printf("Failed to register with the master server. Your server is not\n"
               "publicly accessible. You may need to reconfigure your Internet\n"
               "router to add a port forward for UDP port 2342. Look up\n"
               "information on port forwarding online.");
    }
}

static void PrintSHA1Digest(const char *s, const byte *digest) {
    unsigned int i;

    printf("%s: ", s);

    for (i = 0; i < sizeof(sha1_digest_t); ++i) {
        printf("%02x", digest[i]);
    }

    printf("\n");
}

static void CheckSHA1Sums(void) {
    boolean correct_wad, correct_deh;
    boolean same_freedoom;

    if (!net_client_received_wait_data || had_warning) {
        return;
    }

    correct_wad = memcmp(net_local_wad_sha1sum, net_client_wait_data.wad_sha1sum, sizeof(sha1_digest_t)) == 0;
    correct_deh = memcmp(net_local_deh_sha1sum, net_client_wait_data.deh_sha1sum, sizeof(sha1_digest_t)) == 0;
    same_freedoom = net_client_wait_data.is_freedoom == net_local_is_freedoom;

    if (correct_wad && correct_deh && same_freedoom) {
        return;
    }

    if (!correct_wad) {
        printf("Warning: WAD SHA1 does not match server:\n");
        PrintSHA1Digest("Local", net_local_wad_sha1sum);
        PrintSHA1Digest("Server", net_client_wait_data.wad_sha1sum);
    }

    if (!same_freedoom) {
        printf("Warning: Mixing Freedoom with non-Freedoom\n");
        printf("Local: %u  Server: %i\n", net_local_is_freedoom, net_client_wait_data.is_freedoom);
    }

    if (!correct_deh) {
        printf("Warning: Dehacked SHA1 does not match server:\n");
        PrintSHA1Digest("Local", net_local_deh_sha1sum);
        PrintSHA1Digest("Server", net_client_wait_data.deh_sha1sum);
    }

    if (!same_freedoom) {
        // If Freedoom and Doom IWADs are mixed, the WAD directory
        // will be wrong, but this is not neccessarily a problem.
        // Display a different message to the WAD directory message.

        if (net_local_is_freedoom) {
            printf("You are using the Freedoom IWAD to play with players\n"
                   "using an official Doom IWAD.  Make sure that you are\n"
                   "playing the same levels as other players.\n");
        } else {
            printf("You are using an official IWAD to play with players\n"
                   "using the Freedoom IWAD.  Make sure that you are\n"
                   "playing the same levels as other players.\n");
        }
    } else if (!correct_wad) {
        printf("Your WAD directory does not match other players in the game.\n"
               "Check that you have loaded the exact same WAD files as other\n"
               "players.\n");
    }

    if (!correct_deh) {
        printf("Your dehacked signature does not match other players in the\n"
               "game.  Check that you have loaded the same dehacked patches\n"
               "as other players.\n");
    }

    printf("If you continue, this may cause your game to desync.");

    had_warning = true;
}

static void ParseCommandLineArgs(void) {
    int i;

    //!
    // @arg <n>
    // @category net
    //
    // Autostart the netgame when n nodes (clients) have joined the server.
    //

    i = M_CheckParmWithArgs("-nodes", 1);
    if (i > 0) {
        expected_nodes = atoi(myargv[i + 1]);
    }
}

static void CheckAutoLaunch(void) {
    int nodes;

    if (net_client_received_wait_data && net_client_wait_data.is_controller && expected_nodes > 0) {
        nodes = net_client_wait_data.num_players + net_client_wait_data.num_drones;

        if (nodes >= expected_nodes) {
            expected_nodes = 0;
        }
    }
}

void NET_WaitForLaunch(void) {
    ParseCommandLineArgs();
    had_warning = false;

    while (net_waiting_for_launch) {
        UpdateGUI();
        CheckAutoLaunch();
        CheckSHA1Sums();
        CheckMasterStatus();

        NET_CL_Run();
        NET_SV_Run();

        if (!net_client_connected) {
            I_Error("Lost connection to server");
        }

        sleep(1);
    }
}
