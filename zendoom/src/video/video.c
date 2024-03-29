//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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
//	Gamma correction LUT stuff.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../impl/system.h"

#include "../lib/type.h"

#include "../impl/input.h"
#include "../impl/swap.h"
#include "../impl/video.h"
#include "../misc/bbox.h"
#include "../misc/misc.h"
#include "video.h"
#include "../wad/wad.h"
#include "../mem/zone.h"

#include "../misc/config.h"

// TODO: There are separate RANGECHECK defines for different games, but this
// is common code. Fix this.
#define RANGECHECK

// The screen buffer that the v_video.c code draws to.

static pixel_t *dest_screen = NULL;

int dirtybox[4];

// haleyjd 08/28/10: clipping callback function for patches.
// This is needed for Chocolate Strife, which clips patches to the screen.
static vpatchclipfunc_t patchclip_callback = NULL;

//
// V_MarkRect
//
void V_MarkRect(int x, int y, int width, int height) {
    // If we are temporarily using an alternate screen, do not
    // affect the update box.

    if (dest_screen == I_VideoBuffer) {
        M_AddToBox(dirtybox, x, y);
        M_AddToBox(dirtybox, x + width - 1, y + height - 1);
    }
}

//
// V_CopyRect
//
void V_CopyRect(int srcx, int srcy, pixel_t *source, int width, int height, int destx, int desty) {
    pixel_t *src;
    pixel_t *dest;

#ifdef RANGECHECK
    if (srcx < 0 || srcx + width > SCREENWIDTH || srcy < 0 || srcy + height > SCREENHEIGHT || destx < 0 ||
        destx + width > SCREENWIDTH || desty < 0 || desty + height > SCREENHEIGHT) {
        error("Bad V_CopyRect");
    }
#endif

    V_MarkRect(destx, desty, width, height);

    src = source + SCREENWIDTH * srcy + srcx;
    dest = dest_screen + SCREENWIDTH * desty + destx;

    for (; height > 0; height--) {
        memcpy(dest, src, width * sizeof(*dest));
        src += SCREENWIDTH;
        dest += SCREENWIDTH;
    }
}

//
// V_DrawPatch
// Masks a column based masked pic to the screen.
//

void V_DrawPatch(int x, int y, patch_t *patch) {
    int count;
    int col;
    column_t *column;
    pixel_t *desttop;
    pixel_t *dest;
    byte *source;
    int w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    // haleyjd 08/28/10: Strife needs silent error checking here.
    if (patchclip_callback) {
        if (!patchclip_callback(patch, x, y))
            return;
    }

#ifdef RANGECHECK
    if (x < 0 || x + SHORT(patch->width) > SCREENWIDTH || y < 0 || y + SHORT(patch->height) > SCREENHEIGHT) {
        error("Bad V_DrawPatch");
    }
#endif

    V_MarkRect(x, y, SHORT(patch->width), SHORT(patch->height));

    col = 0;
    desttop = dest_screen + y * SCREENWIDTH + x;

    w = SHORT(patch->width);

    for (; col < w; x++, col++, desttop++) {
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));

        // step through the posts in a column
        while (column->topdelta != 0xff) {
            source = (byte *)column + 3;
            dest = desttop + column->topdelta * SCREENWIDTH;
            count = column->length;

            while (count--) {
                *dest = *source++;
                dest += SCREENWIDTH;
            }
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

//
// V_DrawPatchFlipped
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//

void V_DrawPatchFlipped(int x, int y, patch_t *patch) {
    int count;
    int col;
    column_t *column;
    pixel_t *desttop;
    pixel_t *dest;
    byte *source;
    int w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    // haleyjd 08/28/10: Strife needs silent error checking here.
    if (patchclip_callback) {
        if (!patchclip_callback(patch, x, y))
            return;
    }

#ifdef RANGECHECK
    if (x < 0 || x + SHORT(patch->width) > SCREENWIDTH || y < 0 || y + SHORT(patch->height) > SCREENHEIGHT) {
        error("Bad V_DrawPatchFlipped");
    }
#endif

    V_MarkRect(x, y, SHORT(patch->width), SHORT(patch->height));

    col = 0;
    desttop = dest_screen + y * SCREENWIDTH + x;

    w = SHORT(patch->width);

    for (; col < w; x++, col++, desttop++) {
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[w - 1 - col]));

        // step through the posts in a column
        while (column->topdelta != 0xff) {
            source = (byte *)column + 3;
            dest = desttop + column->topdelta * SCREENWIDTH;
            count = column->length;

            while (count--) {
                *dest = *source++;
                dest += SCREENWIDTH;
            }
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

//
// V_DrawPatchDirect
// Draws directly to the screen on the pc.
//

void V_DrawPatchDirect(int x, int y, patch_t *patch) { V_DrawPatch(x, y, patch); }

//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//

void V_DrawBlock(int x, int y, int width, int height, pixel_t *src) {
    pixel_t *dest;

#ifdef RANGECHECK
    if (x < 0 || x + width > SCREENWIDTH || y < 0 || y + height > SCREENHEIGHT) {
        error("Bad V_DrawBlock");
    }
#endif

    V_MarkRect(x, y, width, height);

    dest = dest_screen + y * SCREENWIDTH + x;

    while (height--) {
        memcpy(dest, src, width * sizeof(*dest));
        src += width;
        dest += SCREENWIDTH;
    }
}

void V_DrawFilledBox(int x, int y, int w, int h, int c) {
    pixel_t *buf;
    int x1, y1;

    buf = I_VideoBuffer + SCREENWIDTH * y + x;

    for (y1 = 0; y1 < h; ++y1) {
        pixel_t *buf1;
        buf1 = buf;

        for (x1 = 0; x1 < w; ++x1) {
            *buf1++ = c;
        }

        buf += SCREENWIDTH;
    }
}

void V_DrawHorizLine(int x, int y, int w, int c) {
    pixel_t *buf;
    int x1;

    buf = I_VideoBuffer + SCREENWIDTH * y + x;

    for (x1 = 0; x1 < w; ++x1) {
        *buf++ = c;
    }
}

void V_DrawVertLine(int x, int y, int h, int c) {
    pixel_t *buf;
    int y1;

    buf = I_VideoBuffer + SCREENWIDTH * y + x;

    for (y1 = 0; y1 < h; ++y1) {
        *buf = c;
        buf += SCREENWIDTH;
    }
}

void V_DrawBox(int x, int y, int w, int h, int c) {
    V_DrawHorizLine(x, y, w, c);
    V_DrawHorizLine(x, y + h - 1, w, c);
    V_DrawVertLine(x, y, h, c);
    V_DrawVertLine(x + w - 1, y, h, c);
}

void V_UseBuffer(pixel_t *buffer) { dest_screen = buffer; }

// Restore screen buffer to the i_video screen buffer.

void V_RestoreBuffer(void) { dest_screen = I_VideoBuffer; }

#define MOUSE_SPEED_BOX_WIDTH 120
#define MOUSE_SPEED_BOX_HEIGHT 9
#define MOUSE_SPEED_BOX_X (SCREENWIDTH - MOUSE_SPEED_BOX_WIDTH - 10)
#define MOUSE_SPEED_BOX_Y 15

//
// V_DrawMouseSpeedBox
//

static void DrawAcceleratingBox(int speed) {
    int red, white, yellow;
    int redline_x;
    int linelen;

    red = I_GetPaletteIndex(0xff, 0x00, 0x00);
    white = I_GetPaletteIndex(0xff, 0xff, 0xff);
    yellow = I_GetPaletteIndex(0xff, 0xff, 0x00);

    // Calculate the position of the red threshold line when calibrating
    // acceleration.  This is 1/3 of the way along the box.

    redline_x = MOUSE_SPEED_BOX_WIDTH / 3;

    if (speed >= mouse_threshold) {
        int original_speed;

        // Undo acceleration and get back the original mouse speed
        original_speed = speed - mouse_threshold;
        original_speed = (int)(original_speed / mouse_acceleration);
        original_speed += mouse_threshold;

        linelen = (original_speed * redline_x) / mouse_threshold;
    } else {
        linelen = (speed * redline_x) / mouse_threshold;
    }

    // Horizontal "thermometer"
    if (linelen > MOUSE_SPEED_BOX_WIDTH - 1) {
        linelen = MOUSE_SPEED_BOX_WIDTH - 1;
    }

    if (linelen < redline_x) {
        V_DrawHorizLine(MOUSE_SPEED_BOX_X + 1, MOUSE_SPEED_BOX_Y + MOUSE_SPEED_BOX_HEIGHT / 2, linelen,
                        white);
    } else {
        V_DrawHorizLine(MOUSE_SPEED_BOX_X + 1, MOUSE_SPEED_BOX_Y + MOUSE_SPEED_BOX_HEIGHT / 2, redline_x,
                        white);
        V_DrawHorizLine(MOUSE_SPEED_BOX_X + redline_x, MOUSE_SPEED_BOX_Y + MOUSE_SPEED_BOX_HEIGHT / 2,
                        linelen - redline_x, yellow);
    }

    // Draw acceleration threshold line
    V_DrawVertLine(MOUSE_SPEED_BOX_X + redline_x, MOUSE_SPEED_BOX_Y + 1, MOUSE_SPEED_BOX_HEIGHT - 2, red);
}

// Highest seen mouse turn speed. We scale the range of the thermometer
// according to this value, so that it never exceeds the range. Initially
// this is set to a 1:1 setting where 1 pixel = 1 unit of speed.
static int max_seen_speed = MOUSE_SPEED_BOX_WIDTH - 1;

static void DrawNonAcceleratingBox(int speed) {
    int white;
    int linelen;

    white = I_GetPaletteIndex(0xff, 0xff, 0xff);

    if (speed > max_seen_speed) {
        max_seen_speed = speed;
    }

    // Draw horizontal "thermometer":
    linelen = speed * (MOUSE_SPEED_BOX_WIDTH - 1) / max_seen_speed;

    V_DrawHorizLine(MOUSE_SPEED_BOX_X + 1, MOUSE_SPEED_BOX_Y + MOUSE_SPEED_BOX_HEIGHT / 2, linelen, white);
}

void V_DrawMouseSpeedBox(int speed) {
    extern int usemouse;
    int bgcolor, bordercolor, black;

    // If the mouse is turned off, don't draw the box at all.
    if (!usemouse) {
        return;
    }

    // Get palette indices for colors for widget. These depend on the
    // palette of the game being played.

    bgcolor = I_GetPaletteIndex(0x77, 0x77, 0x77);
    bordercolor = I_GetPaletteIndex(0x55, 0x55, 0x55);
    black = I_GetPaletteIndex(0x00, 0x00, 0x00);

    // Calculate box position

    V_DrawFilledBox(MOUSE_SPEED_BOX_X, MOUSE_SPEED_BOX_Y, MOUSE_SPEED_BOX_WIDTH, MOUSE_SPEED_BOX_HEIGHT,
                    bgcolor);
    V_DrawBox(MOUSE_SPEED_BOX_X, MOUSE_SPEED_BOX_Y, MOUSE_SPEED_BOX_WIDTH, MOUSE_SPEED_BOX_HEIGHT,
              bordercolor);
    V_DrawHorizLine(MOUSE_SPEED_BOX_X + 1, MOUSE_SPEED_BOX_Y + 4, MOUSE_SPEED_BOX_WIDTH - 2, black);

    // If acceleration is used, draw a box that helps to calibrate the
    // threshold point.
    if (mouse_threshold > 0 && fabs(mouse_acceleration - 1) > 0.01) {
        DrawAcceleratingBox(speed);
    } else {
        DrawNonAcceleratingBox(speed);
    }
}
