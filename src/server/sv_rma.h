/**
 * @file
 */

/*
All original material Copyright (C) 2002-2014 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/server/sv_init.c

Copyright (C) 1997-2001 Id Software, Inc.

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

#pragma once

#define MAX_MAPASSEMBLIES 32
/** note: MAX_TILETYPES mostly limits the # of tiles that can be *parsed*.
 * The # *used* in an assemly is roughly limited to RMA2_MAX_REC (+fixed + required tiles). */
#define MAX_TILETYPES 128
#define MAX_TILESETS 16
#define MAX_TILESETTILES 16
#define MAX_TILESIZE 16
#define MAX_FIXEDTILES 64

#define MAX_RANDOM_MAP_WIDTH 32
#define MAX_RANDOM_MAP_HEIGHT 32

#define RMA_HIGHEST_SUPPORTED_SEED 50

int SV_AssembleMap(const char* name, const char* assembly, char* asmMap, char* asmPos, char* entityString, const unsigned int seed, bool print);
int SV_AssembleMapAndTitle(const char* mapTheme, const char* assembly, char* asmTiles, char* asmPos, char* entityString, const unsigned int seed, bool print, const char** asmTitle);

/* the next function is only exported for cunits tests */
void SV_PrintAssemblyStats(const char* mapTheme, const char* asmName);
