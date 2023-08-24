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
// DESCRIPTION:  Heads-up displays
//

#include <ctype.h>
#include <stdlib.h>

#include "../game/def.h"
#include "../game/keys.h"

#include "../mem/zone.h"

#include "../impl/input.h"
#include "../impl/swap.h"
#include "../impl/video.h"

#include "../game/controls.h"
#include "../misc/misc.h"
#include "../wad/wad.h"
#include "lib.h"
#include "stuff.h"

#include "../sound/sound.h"

#include "../game/stat.h"

// Data.
#include "../game/strings.h"
#include "../sound/sounds.h"

//
// Locally used constants, shortcuts.
//
#define HU_TITLE (mapnames[(gameepisode - 1) * 9 + gamemap - 1])
#define HU_TITLEHEIGHT 1
#define HU_TITLEX 0
#define HU_TITLEY (167 - SHORT(hu_font[0]->height))

#define HU_INPUTTOGGLE 't'
#define HU_INPUTX HU_MSGX
#define HU_INPUTY (HU_MSGY + HU_MSGHEIGHT * (SHORT(hu_font[0]->height) + 1))
#define HU_INPUTWIDTH 64
#define HU_INPUTHEIGHT 1

char *chat_macros[10];

const char *player_names[] = {HUSTR_PLRGREEN, HUSTR_PLRINDIGO, HUSTR_PLRBROWN, HUSTR_PLRRED};

char chat_char; // remove later.
static player_t *plr;
patch_t *hu_font[HU_FONTSIZE];
static hu_textline_t w_title;
boolean chat_on;
static hu_itext_t w_chat;
static boolean always_off = false;
static char chat_dest[MAXPLAYERS];
static hu_itext_t w_inputbuffer[MAXPLAYERS];

static boolean message_on;
boolean message_dontfuckwithme;
static boolean message_nottobefuckedwith;

static hu_stext_t w_message;
static int message_counter;

extern int showMessages;

static boolean headsupactive = false;

//
// Builtin map names.
// The actual names can be found in DStrings.h.
//

const char *mapnames[] = // DOOM shareware/registered names.
    {

        HUSTR_E1M1, HUSTR_E1M2, HUSTR_E1M3, HUSTR_E1M4, HUSTR_E1M5,
        HUSTR_E1M6, HUSTR_E1M7, HUSTR_E1M8, HUSTR_E1M9,

        HUSTR_E2M1, HUSTR_E2M2, HUSTR_E2M3, HUSTR_E2M4, HUSTR_E2M5,
        HUSTR_E2M6, HUSTR_E2M7, HUSTR_E2M8, HUSTR_E2M9,

        HUSTR_E3M1, HUSTR_E3M2, HUSTR_E3M3, HUSTR_E3M4, HUSTR_E3M5,
        HUSTR_E3M6, HUSTR_E3M7, HUSTR_E3M8, HUSTR_E3M9,

        "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL",
        "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL"};

void HU_Init(void) {

    int i;
    int j;
    char buffer[9];

    // load the heads-up font
    j = HU_FONTSTART;
    for (i = 0; i < HU_FONTSIZE; i++) {
        snprintf(buffer, 9, "STCFN%.3d", j++);
        hu_font[i] = (patch_t *)W_CacheLumpName(buffer, PU_STATIC);
    }
}

void HU_Stop(void) { headsupactive = false; }

void HU_Start(void) {

    int i;
    const char *s;

    if (headsupactive)
        HU_Stop();

    plr = &players[consoleplayer];
    message_on = false;
    message_dontfuckwithme = false;
    message_nottobefuckedwith = false;
    chat_on = false;

    // create the message widget
    HUlib_initSText(&w_message, HU_MSGX, HU_MSGY, HU_MSGHEIGHT, hu_font, HU_FONTSTART, &message_on);

    // create the map title widget
    HUlib_initTextLine(&w_title, HU_TITLEX, HU_TITLEY, hu_font, HU_FONTSTART);

    switch (logical_gamemission) {
    case doom:
        s = HU_TITLE;
        break;
    default:
        s = "Unknown level";
        break;
    }

    // dehacked substitution to get modified level name

    while (*s)
        HUlib_addCharToTextLine(&w_title, *(s++));

    // create the chat widget
    HUlib_initIText(&w_chat, HU_INPUTX, HU_INPUTY, hu_font, HU_FONTSTART, &chat_on);

    // create the inputbuffer widgets
    for (i = 0; i < MAXPLAYERS; i++)
        HUlib_initIText(&w_inputbuffer[i], 0, 0, 0, 0, &always_off);

    headsupactive = true;
}

void HU_Drawer(void) {

    HUlib_drawSText(&w_message);
    HUlib_drawIText(&w_chat);
    if (automapactive)
        HUlib_drawTextLine(&w_title, false);
}

void HU_Erase(void) {

    HUlib_eraseSText(&w_message);
    HUlib_eraseIText(&w_chat);
    HUlib_eraseTextLine(&w_title);
}

void HU_Ticker(void) {

    int i, rc;
    char c;

    // tick down message counter if message is up
    if (message_counter && !--message_counter) {
        message_on = false;
        message_nottobefuckedwith = false;
    }

    if (showMessages || message_dontfuckwithme) {

        // display message if necessary
        if ((plr->message && !message_nottobefuckedwith) || (plr->message && message_dontfuckwithme)) {
            HUlib_addMessageToSText(&w_message, 0, plr->message);
            plr->message = 0;
            message_on = true;
            message_counter = HU_MSGTIMEOUT;
            message_nottobefuckedwith = message_dontfuckwithme;
            message_dontfuckwithme = 0;
        }

    } // else message_on = false;

    // check for incoming chat characters
    if (netgame) {
        for (i = 0; i < MAXPLAYERS; i++) {
            if (!playeringame[i])
                continue;
            if (i != consoleplayer && (c = players[i].cmd.chatchar)) {
                if (c <= HU_BROADCAST)
                    chat_dest[i] = c;
                else {
                    rc = HUlib_keyInIText(&w_inputbuffer[i], c);
                    if (rc && c == KEY_ENTER) {
                        if (w_inputbuffer[i].l.len &&
                            (chat_dest[i] == consoleplayer + 1 || chat_dest[i] == HU_BROADCAST)) {
                            HUlib_addMessageToSText(&w_message, player_names[i],
                                                    w_inputbuffer[i].l.l);

                            message_nottobefuckedwith = true;
                            message_on = true;
                            message_counter = HU_MSGTIMEOUT;
                            S_StartSound(0, sfx_tink);
                        }
                        HUlib_resetIText(&w_inputbuffer[i]);
                    }
                }
                players[i].cmd.chatchar = 0;
            }
        }
    }
}

#define QUEUESIZE 128

static char chatchars[QUEUESIZE];
static int head = 0;
static int tail = 0;

void HU_queueChatChar(char c) {
    if (((head + 1) & (QUEUESIZE - 1)) == tail) {
        plr->message = HUSTR_MSGU;
    } else {
        chatchars[head] = c;
        head = (head + 1) & (QUEUESIZE - 1);
    }
}

char HU_dequeueChatChar(void) {
    char c;

    if (head != tail) {
        c = chatchars[tail];
        tail = (tail + 1) & (QUEUESIZE - 1);
    } else {
        c = 0;
    }

    return c;
}

static void StartChatInput() {
    chat_on = true;
    HUlib_resetIText(&w_chat);
    HU_queueChatChar(HU_BROADCAST);

    I_StartTextInput(0, 8, SCREENWIDTH, 16);
}

static void StopChatInput(void) {
    chat_on = false;
    I_StopTextInput();
}

boolean HU_Responder(event_t *ev) {

    static char lastmessage[HU_MAXLINELENGTH + 1];
    const char *macromessage;
    boolean eatkey = false;
    static boolean altdown = false;
    unsigned char c;
    int i;
    int numplayers;

    static int num_nobrainers = 0;

    numplayers = 0;
    for (i = 0; i < MAXPLAYERS; i++)
        numplayers += playeringame[i];

    if (ev->data1 == KEY_RSHIFT) {
        return false;
    } else if (ev->data1 == KEY_RALT || ev->data1 == KEY_LALT) {
        altdown = ev->type == ev_keydown;
        return false;
    }

    if (ev->type != ev_keydown)
        return false;

    if (!chat_on) {
        if (ev->data1 == key_message_refresh) {
            message_on = true;
            message_counter = HU_MSGTIMEOUT;
            eatkey = true;
        } else if (netgame && ev->data2 == key_multi_msg) {
            eatkey = true;
            StartChatInput();
        } else if (netgame && numplayers > 2) {
            for (i = 0; i < MAXPLAYERS; i++) {
                if (ev->data2 == key_multi_msgplayer[i]) {
                    if (playeringame[i] && i != consoleplayer) {
                        eatkey = true;
                        StartChatInput();
                        break;
                    } else if (i == consoleplayer) {
                        num_nobrainers++;
                        if (num_nobrainers < 3)
                            plr->message = HUSTR_TALKTOSELF1;
                        else if (num_nobrainers < 6)
                            plr->message = HUSTR_TALKTOSELF2;
                        else if (num_nobrainers < 9)
                            plr->message = HUSTR_TALKTOSELF3;
                        else if (num_nobrainers < 32)
                            plr->message = HUSTR_TALKTOSELF4;
                        else
                            plr->message = HUSTR_TALKTOSELF5;
                    }
                }
            }
        }
    } else {
        // send a macro
        if (altdown) {
            c = ev->data1 - '0';
            if (c > 9)
                return false;
            // fprintf(stderr, "got here\n");
            macromessage = chat_macros[c];

            // kill last message with a '\n'
            HU_queueChatChar(KEY_ENTER); // DEBUG!!!

            // send the macro message
            while (*macromessage)
                HU_queueChatChar(*macromessage++);
            HU_queueChatChar(KEY_ENTER);

            // leave chat mode and notify that it was sent
            StopChatInput();
            M_StringCopy(lastmessage, chat_macros[c], sizeof(lastmessage));
            plr->message = lastmessage;
            eatkey = true;
        } else {
            c = ev->data3;

            eatkey = HUlib_keyInIText(&w_chat, c);
            if (eatkey) {
                // static unsigned char buf[20]; // DEBUG
                HU_queueChatChar(c);

                // M_snprintf(buf, sizeof(buf), "KEY: %d => %d", ev->data1, c);
                //        plr->message = buf;
            }
            if (c == KEY_ENTER) {
                StopChatInput();
                if (w_chat.l.len) {
                    M_StringCopy(lastmessage, w_chat.l.l, sizeof(lastmessage));
                    plr->message = lastmessage;
                }
            } else if (c == KEY_ESCAPE) {
                StopChatInput();
            }
        }
    }

    return eatkey;
}
