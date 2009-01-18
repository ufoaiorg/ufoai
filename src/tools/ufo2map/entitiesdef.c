/**
 * @file entitiesdef.c
 * @brief parsing entities.def file, which contains info for radiant
 * and is used for checking the *.map files.
 */

/*
All original materal Copyright (C) 2002-2009 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "common/shared.h"
#include "common/cmdlib.h"
#include "entitiesdef.h"

static char *defBuffer;
static int defBuffSize;

/**
 * @brief load entities.def
 * @todo currently assumes trunk is the current directory, and has path to def hardcoded - is there a better way?
 */
void ED_Load (void)
{
	const char *defPath = "./radiant/games/entities.def";
	Verb_Printf(VERB_EXTRA, "Loading def: %s\n", defPath);
	defBuffSize = LoadFile(defPath, (void **)&defBuffer);
}

void ED_Free (void)
{
	free (defBuffer);
}

