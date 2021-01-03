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
// Main dehacked code
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d_iwad.h"
#include "doomtype.h"
#include "i_glob.h"
#include "i_system.h"
#include "m_argv.h"
#include "w_wad.h"

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"

extern deh_section_t *deh_section_types[];
extern const char *deh_signatures[];

static boolean deh_initialized = false;

// If true, we can parse [STRINGS] sections in BEX format.

boolean deh_allow_extended_strings = false;

// If true, we can do long string replacements.

boolean deh_allow_long_strings = false;

// If true, we can do cheat replacements longer than the originals.

boolean deh_allow_long_cheats = false;

// If false, dehacked cheat replacements are ignored.

boolean deh_apply_cheats = true;

// This pattern is used a lot of times in different sections,
// an assignment is essentially just a statement of the form:
//
// Variable Name = Value
//
// The variable name can include spaces or any other characters.
// The string is split on the '=', essentially.
//
// Returns true if read correctly

// Strip whitespace from the start and end of a string

static char *CleanString(char *s) {
    char *strending;

    // Leading whitespace

    while (*s && isspace(*s))
        ++s;

    // Trailing whitespace

    strending = s + strlen(s) - 1;

    while (strlen(s) > 0 && isspace(*strending)) {
        *strending = '\0';
        --strending;
    }

    return s;
}

boolean DEH_ParseAssignment(char *line, char **variable_name, char **value) {
    char *p;

    // find the equals

    p = strchr(line, '=');

    if (p == NULL) {
        return false;
    }

    // variable name at the start
    // turn the '=' into a \0 to terminate the string here

    *p = '\0';
    *variable_name = CleanString(line);

    // value immediately follows the '='

    *value = CleanString(p + 1);



    return true;
}
