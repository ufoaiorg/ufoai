/**
 * @file sv_rma.c
 * @brief Random map assembly code
 * More info on map-assembly can be found at:
 * http://ufoai.org/wiki/index.php/Mapping/Random_map_assembly
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "server.h"
#include "sv_rma.h"
#include "../shared/parse.h"
#include "../shared/mutex.h"

#define ASSEMBLE_THREADS 2
/** @brief print some debugging info */
#define PRINT_RMA_PROGRESS 0
/** @brief place the biggest 'required' tiles first. Helps oriental a lot, but is bad for village. */
#define SORT_BY_SIZE 1

/** @brief max # of recursions */
#define RMA2_MAX_REC 64
/** @brief max # of valid tile/position combinations */
/* How to calculate this value:
 * Take he largest map of the project, eg. +farm
 * farm large has a size of 12x12.
 * If we had only 'fits everywhere'-tiles of size 1x1,
 * we'd need 12x12=144 x tiletypes(25) = 3600 (in theory).
 * Larger tiles eg. 2x2 would reduce the positions from 144 to 11x11=121
 * Required tiles with exactly one instance ie. "1 1" reduce both the # of tiletypes
 * and the # of positions by their size.
 * The neighbouring requirements of required tiles will further reduce the value.
 * => It's best to determine a working value empirically.
 */
#define RMA2_MAX_TILEPOS 1700
/** @brief tile code multiplier. For the various debug printfs we want a number that we can easily divide through (20, 50, 100,...) */
#define TCM 50
/** @brief the # of different tiles we can store for a gap */
#define GAPS 25
/** @brief array of working random tile positions, 50 recursions */
static short posTileList[RMA2_MAX_REC][RMA2_MAX_TILEPOS];
/** @brief for every x/y we can store the tiles that can cover that place here */
static short gapList[MAX_RANDOM_MAP_HEIGHT][MAX_RANDOM_MAP_HEIGHT][GAPS + 1];
static SDL_sem *mapSem;
static SDL_cond *mapCond;
static threads_mutex_t *mapLock;
static Uint32 threadID;

/**
 * @brief Fills a list with random values between @c 0 and @c n
 * @param[in] n Size of the list
 * @param[out] list The list to fill with random values
 */
static void RandomList (const int n, short *list)
{
	short i;

	for (i = 0; i < n; i++)
		list[i] = i;

	for (i = 0; i < n; i++) {
		const short r = rand() % (i + (n - i));
		const short t = list[r];
		list[r] = list[i];
		list[i] = t;
	}
}

#define ALL_TILES (0xfffffffeUL)
#define IS_SOLID(x) ((x)&1UL)

/**
 * @brief Convert to tile spec - normalize the characters
 * @sa SV_ParseMapTile
 * @note a tile definition looks like this:
 * @code tile +s02
 * {
 * 3 3
 *
 * 0      a      0
 * b      +b     b
 * 0      a      0
 * }
 * @endcode
 * tile +s02 defines the name of the tile which can be refered to from the assembly
 * the first two numbers defines the tile size - if you have a tile with the 'real'
 * size of 1x1 (256x256 in radiant) the definition is 3x3 because you have to
 * define the surroundings, too
 * The field marked with the + is the 'real' mapparts all the others are the
 * surroundings - the letters of the surroundings must have a tile definition with
 * a + and the letter, too - otherwise the placing of the tile may fail
 *
 * @note If you marked a tile with + the mTile_t->spec at that position will be SOLID
 * @note valid tile characters are 0-5 and a-z
 */
static unsigned long tileMask (const char chr)
{
	if (chr == '+')
		return 1UL;
	else if (chr == '0')
		return ALL_TILES;
	else if (chr >= '1' && chr <= '5')
		return 1UL << (chr - '0');
	else if (chr >= 'a' && chr <= 'z')
		return 1UL << (chr - 'a' + 6);
	else if (chr >= 'A' && chr <= 'Z')
		return 1UL << (chr - 'A' + 6);

	Com_Error(ERR_DROP, "SV_ParseMapTile: Invalid tile char '%c'", chr);
}

static void SV_TileMaskToString (unsigned long m, char *str)
{
	int i;
	int j = 0;	/* writepos */

	if (m == ALL_TILES) {
		str[0] = '0';
		str[1] = 0;
		return;
	}
	if (m & 1) {
		str[0] = '+';
		 j = 1;
	}
	for (i = 1; i < 6; i++) {
		if (m >> i & 1) {
			str[j] = '0' + i;
			j++;
		}
	}
	for (i = 6; i < 32; i++) {
		if (m >> i & 1) {
			str[j] = 'a' - 6 + i;
			j++;
		}
	}
	str[j] = 0;
}

#define ACW 6	/* ascii cell width */
#define ACH 3	/* ascii cell height */
#define MMW 13	/* map max height 13 means we support 12 */
#define MMH 13	/* map max height */
static void SV_RmaPrintMap (const mapInfo_t *map)
{
	char screen[(MMH + 1) * ACH][(MMW + 1) * ACW];
	int i, j;

	assert(map->mAssembly[map->mAsm].height < MMH);
	assert(map->mAssembly[map->mAsm].width < MMW);

	/* initialize */
	int rb = (1 + map->mAssembly[map->mAsm].width) * ACW;	/* index of right border */
	memset(screen, ' ', sizeof(screen));
	for (i = 0; i < MMH * ACH + 1; i++) {
		screen[i][rb + 1] = '|';
		screen[i][rb + 2] = 0;
	}

	/* fill in the data */
	for (i = 0; i < map->numPlaced; i++) {
		const mPlaced_t *mp = &map->mPlaced[i];
		const mTile_t *tile = mp->tile;
		int tx, ty;
		const char *tn = tile->id + 1;

		if (!strncmp(tn, "craft_", 6))
			tn += 6;
		for (ty = 0; ty < tile->h; ty++) {
			for (tx = 0; tx < tile->w; tx++) {
				if (IS_SOLID(tile->spec[ty][tx])) {
					int cbX = ACW * (mp->x + tx);
					int cbY = ACH * (mp->y + ty);
					char flags[33] = {0,};

					/* write the tilename */
					for (j = 0; j < ACW - 1; j++) {
						if (tn[j])
							screen[cbY + ACH - 1][cbX + 1 + j] = tn[j];
						else
							break;
					}

					/* get the properties of that solid */
					SV_TileMaskToString(tile->spec[ty][tx], flags);
					/* write the flags */
					for (j = 0; j < ACW - 1; j++) {
						if (flags[j])
							screen[cbY + ACH - 2][cbX + 1 + j] = flags[j];
						else
							break;
					}


					/* left border of tile */
					if (tx > 0 && !IS_SOLID(tile->spec[ty][tx - 1])) {
						for (j = 0; j < ACH; j++)
							screen[cbY + j][cbX] = '!';
					}
					if (!IS_SOLID(tile->spec[ty][tx + 1])) {
						for (j = 0; j < ACH; j++)
							screen[cbY + j][cbX + ACW] = '!';
					}
					if (ty > 0 && !IS_SOLID(tile->spec[ty - 1][tx])) {
						for (j = 1; j < ACW; j++)
							screen[cbY][cbX + j] = '-';
					}
					if (!IS_SOLID(tile->spec[ty + 1][tx])) {
						for (j = 1; j < ACW; j++)
							screen[cbY + ACH][cbX + j] = '-';
					}
				}
			}
		}
	}

	/* now add the specs of the gaps */
	int cx, cy;
	int height = map->mAssembly[map->mAsm].height;
	int width = map->mAssembly[map->mAsm].width;
	for (cy = 0; cy <= height; cy++) {
		for (cx = 0; cx <= width; cx++) {
			if (!IS_SOLID(map->curMap[cy][cx])) {
				const int cbX = ACW * (cx);
				const int cbY = ACH * (cy);
				char flags2[33] = {0,};

				/* get the requirements of that gap */
				SV_TileMaskToString(map->curMap[cy][cx], flags2);
				/* write the flags */
				for (j = 0; j < ACW - 1; j++) {
					if (flags2[j])
						screen[cbY + ACH - 2][cbX + 1 + j] = flags2[j];
					else
						break;
				}
			}
		}
	}

	/* print it */
	const char *underscores = "_________________________________________________________________________\n";
	Com_Printf("\nCurrent state of the map:\n");
	int w = ACW * (MMW - 1 - map->mAssembly[map->mAsm].width);
	Com_Printf("%s", underscores + w);
	int h = ACH * (height + 1);
	for (i = h; i >= ACH; i--)
		Com_Printf("%s\n", screen[i] + ACW);
	Com_Printf("%s", underscores + w);
}

static const mTileSet_t *SV_GetMapTileSet (const mapInfo_t *map, const char *tileSetName)
{
	int i;

	for (i = 0; i < map->numTileSets; i++) {
		const mTileSet_t *tileSet = &map->mTileSets[i];
		if (Q_streq(tileSetName, tileSet->id))
			return tileSet;
	}

	return NULL;
}

static inline const mTile_t *SV_GetMapTile (const mapInfo_t *map, const char *tileName)
{
	int i;

	for (i = 0; i < map->numTiles; i++) {
		const mTile_t *tile = &map->mTile[i];
		if (Q_streq(tileName, tile->id))
			return tile;
	}

	return NULL;
}

/**
 * @brief Parsed a tileset definition out of the ump-files
 * @sa SV_ParseAssembly
 * @sa SV_AssembleMap
 */
static qboolean SV_ParseMapTileSet (const char *filename, const char **text, mapInfo_t *map, qboolean inherit)
{
	const char *errhead = "SV_ParseMapTileSet: Unexpected end of file (";
	const char *token;
	mTileSet_t *target = &map->mTileSets[map->numTileSets];

	OBJZERO(*target);

	/* get tileset name */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return qfalse;

	Q_strncpyz(target->id, token, sizeof(target->id));

	/* start parsing the block */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return qfalse;
	if (*token != '{') {
		Com_Printf("SV_ParseMapTileSet: Expected '{' for tileset '%s' (%s)\n", target->id, filename);
		return qfalse;
	}

	do {
		token = Com_EParse(text, errhead, filename);
		if (!*text)
			return qfalse;
		if (token[0] != '}') {
			if (target->numTiles >= MAX_TILESETTILES)
				Com_Error(ERR_DROP, "Max tileset limit reached for tileset '%s'", target->id);
			else {	/* just to get rid of the 'mixed decl and code' warning */
				char *tileTarget = target->tiles[target->numTiles];
				const size_t size = sizeof(target->tiles[target->numTiles]);
				if (inherit) {
					if (token[0] == '+')
						token++;

					Com_sprintf(tileTarget, size, "%s%s", map->inheritBasePath, token);
				} else {
					Q_strncpyz(tileTarget, token, size);
				}

				if (SV_GetMapTile(map, tileTarget) != NULL)
					target->numTiles++;
				else
					Com_Error(ERR_DROP, "Did not find tile '%s' from tileset '%s'", tileTarget, target->id);
			}
		}
	} while (token[0] != '}');

	map->numTileSets++;
	return qfalse;
}

/**
 * @brief Parsed a tile definition out of the ump-files
 * @sa SV_ParseAssembly
 * @sa SV_AssembleMap
 */
static qboolean SV_ParseMapTile (const char *filename, const char **text, mapInfo_t *map, qboolean inherit)
{
	const char *errhead = "SV_ParseMapTile: Unexpected end of file (";
	const char *token;
	int x, y, i;
	mTile_t *target = &map->mTile[map->numTiles];
	target->area = 0;

	/* get tile name */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return qfalse;

	OBJZERO(*target);

	if (inherit) {
		if (token[0] == '+')
			token++;
		Com_sprintf(target->id, sizeof(target->id), "%s%s", map->inheritBasePath, token);
	} else {
		Q_strncpyz(target->id, token, sizeof(target->id));
	}

	/* start parsing the block */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return qfalse;
	if (*token != '{') {
		Com_Printf("SV_ParseMapTile: Expected '{' for tile '%s' (%s)\n", target->id, filename);
		return qfalse;
	}

	/* get width and height */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return qfalse;
	target->w = atoi(token);

	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return qfalse;
	target->h = atoi(token);

	if (target->w > MAX_TILESIZE || target->h > MAX_TILESIZE) {
		Com_Printf("SV_ParseMapTile: Bad tile size [%i %i] (%s) (max. [%i %i])\n", target->w, target->h, filename, MAX_TILESIZE, MAX_TILESIZE);
		*text = strchr(*text, '}');
		return qfalse;
	}

	/* get tile specs */
	for (y = target->h - 1; y >= 0; y--)
		for (x = 0; x < target->w; x++) {
			token = Com_EParse(text, errhead, filename);
			if (!*text || *token == '}') {
				Com_Printf("SV_ParseMapTile: Bad tile desc in '%s' - not enough entries for size\n", target->id);
				*text = strchr(*text, '}') + 1;
				return 0;
			}
			target->spec[y][x] = 0UL;
			for (i = 0; token[i]; i++) {
				target->spec[y][x] |= tileMask(token[i]);
			}
			if (IS_SOLID(target->spec[y][x]))
				target->area++;	/* # of solid tiles */
		}

	token = Com_EParse(text, errhead, filename);

	/* get connections */
	if (*token != '}')
		Com_Printf("SV_ParseMapTile: Bad tile desc in '%s' - too many entries for size\n", target->id);

	/* successfully parsed - this tile counts */
	return qtrue;
}

/**
 * @brief Tries to extract a tile name from a cvar - the cvar value must start with a '+'
 * @param a the assembly
 * @param token The cvar name
 * @param filename The ump filename
 * @param text The text buffer
 * @param errhead Error header
 * @return @c NULL if file has invalid format, @c the tilename of the cvar otherwise.
 */
static const char *SV_GetCvarToken (const mAssembly_t *a, const char* token, const char *filename, const char **text, const char *errhead)
{
	const cvar_t *cvar;

	Com_DPrintf(DEBUG_SERVER, "SV_GetCvarToken: cvar replacement: %s\n", token);

	cvar = Cvar_FindVar(token);

	token = Com_EParse(text, errhead, filename);
	if (!text || token[0] == '}')
		return NULL;

	if (cvar == NULL)
		return token;

	Com_DPrintf(DEBUG_SERVER, "SV_ParseAssembly: cvar replacement value: %s\n", cvar->string);
	if (cvar->string[0] != '+') {
		Com_Printf("SV_ParseAssembly: warning - cvar '%s' value doesn't seam to be a valid tile id '%s' - set to default '%s'\n",
				cvar->name, cvar->string, token);
		Cvar_Set(cvar->name, token);
		if (token[0] != '+')
			Com_Error(ERR_DROP, "SV_ParseAssembly: wrong tile id in assembly '%s'", a->id);

		return token;
	}
	return cvar->string;
}

static const char *SV_GetTileFromTileSet (const mapInfo_t *map, const char *filename, const char **text, const mAssembly_t *a)
{
	const char *errhead = "SV_GetTileFromTileSet: Unexpected end of file (";
	const mTileSet_t *tileSet;
	int random;
	const char *token;

	/* get tileset id */
	token = Com_EParse(text, errhead, filename);
	if (!text)
		Com_Error(ERR_DROP, "SV_GetTileFromTileSet: illegal tileset syntax in assembly '%s' in %s", a->id, filename);

	tileSet = SV_GetMapTileSet(map, token);
	if (tileSet == NULL)
		Com_Error(ERR_DROP, "SV_GetTileFromTileSet: Could not find tileset %s in %s (assembly %s)", token, filename, a->id);

	random = rand() % tileSet->numTiles;
	return tileSet->tiles[random];
}

/**
 * @brief Parses a list of working seeds to assemble this rma assembly
 * @param[in,out] map All we know about the map to assemble
 * @param[in] filename The name of the .UMP file, used in error messages
 * @param[out] a Pointer to the assembly to be initialized, must be allocated.
 * @param[in] text The text of the ump file to parse
 * @return @c true if it was parsed, @c false if not.
 */
static qboolean SV_ParseAssemblySeeds (mapInfo_t *map, const char *filename, const char **text, mAssembly_t *a)
{
	const char *errhead = "SV_ParseAssemblySeeds: Unexpected end of file (";
	const char *token;

	/* start parsing the block */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return qfalse;
	if (*token != '{') {
		Com_Printf("SV_ParseAssemblySeeds: Expected '{' for seed of assembly '%s' (%s)\n", a->id, filename);
		return qfalse;
	}

	for (;;) {
		token = Com_EParse(text, errhead, filename);
		if (!*text || token[0] == '}')
			break;

		if (a->numSeeds < lengthof(a->seeds)) {
			a->seeds[a->numSeeds++] = atoi(token);
		} else {
			Com_Printf("too many seeds for %s (%s) - ignore seed %s\n", a->id, filename, token);
		}
	}
	return qtrue;
}

static void SV_GetTilesFromTileSet (const mapInfo_t *map, const char *filename, const char **text, mAssembly_t *a)
{
	const char *errhead = "SV_GetTilesFromTileSet: Unexpected end of file (";
	const mTileSet_t *tileSet;
	const mTile_t *tile;
	int c, x, y, random;
	const char *token;

	/* get tileset id */
	token = Com_EParse(text, errhead, filename);
	if (!text)
		Com_Error(ERR_DROP, "SV_GetTilesFromTilesSet: illegal tileset syntax in assembly '%s' in %s", a->id, filename);
	tileSet = SV_GetMapTileSet(map, token);
	if (tileSet == NULL)
		Com_Error(ERR_DROP, "SV_GetTilesFromTilesSet: Could not find tileset %s in %s (assembly %s)", token, filename, a->id);

	/* get min and max tileset number */
	token = Com_EParse(text, errhead, filename);
	if (!text || *token == '}')
		Com_Error(ERR_DROP, "SV_GetTilesFromTilesSet: Error in assembly %s (invalid syntax for tileset %s)", filename, tileSet->id);
	if (!strstr(token, " "))
		Com_Error(ERR_DROP, "SV_GetTilesFromTilesSet: Error in assembly %s (min max value of tileset %s)", filename, tileSet->id);
	sscanf(token, "%i %i", &x, &y);
	if (x > y)
		Com_Error(ERR_DROP, "SV_GetTilesFromTilesSet: Error in assembly %s (min is bigger than max for tileset %s)", filename, tileSet->id);
	if (y <= 0)
		Com_Error(ERR_DROP, "SV_GetTilesFromTilesSet: Error in assembly %s (max is <= 0 for tileset %s)", filename, tileSet->id);
	/* set max tile numbers (increasing the max of random tiles until the required max is reached)t */
	for (c = y; c > 0; c--) {
		random = rand() % tileSet->numTiles;
		tile = SV_GetMapTile(map, tileSet->tiles[random]);
		if (tile != NULL) {
			const ptrdiff_t i = tile - map->mTile;
			a->max[i]++;
		} else {
			Com_Error(ERR_DROP, "Could not find tile: '%s' in tileset '%s' (%s)", tileSet->tiles[random], tileSet->id, filename);
		}
	}
	/* set min tile numbers (increasing the min of random tiles until the required min is reached) */
	c = x;
	while (c > 0) {
		random = rand() % tileSet->numTiles;
		tile = SV_GetMapTile(map, tileSet->tiles[random]);
		if (tile != NULL) {
			const ptrdiff_t i = tile - map->mTile;
			if (a->min[i] < a->max[i]) {
				a->min[i]++;
				c--;
			}
		} else {
			Com_Error(ERR_DROP, "Could not find tile: '%s' in tileset '%s' (%s)", tileSet->tiles[random], tileSet->id, filename);
		}
	}
}

/**
 * @brief Parses an assembly block
 * @param[in,out] map All we know about the map to assemble
 * @param[in] filename The name of the .UMP file, used in error messages
 * @param[out] a Pointer to the assembly to be initialized, must be allocated.
 * @param[in] text The text of the ump file to parse
 * @sa SV_AssembleMap
 * @sa SV_ParseMapTile
 * @note: format of size: "size x y"
 * @note: format of fix: "fix [tilename] x y"
 * @note: format of tile: "[tilename] min max"
 * @return @c true if it was parsed, @c false if not.
 */
static qboolean SV_ParseAssembly (mapInfo_t *map, const char *filename, const char **text, mAssembly_t *a)
{
	const char *errhead = "SV_ParseAssembly: Unexpected end of file (";
	const char *token;
	int x, y;
	const mTile_t *tile;

	/* get assembly name */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return qfalse;

	/* init */
	OBJZERO(*a);
	Q_strncpyz(a->id, token, sizeof(a->id));
	a->width = 8;
	a->height = 8;
	a->dx = 1;
	a->dy = 1;

	token = Com_EParse(text, errhead, filename);
	if (!*text || *token != '{')
		Com_Error(ERR_DROP, "Invalid assembly definition '%s' - invalid token '%s' (%s)", a->id, token, filename);

	do {
		/* get tile name */
		token = Com_EParse(text, errhead, filename);
		if (!text || *token == '}')
			break;

		if (Q_streq(token, "title")) {
			/* get map title */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			Q_strncpyz(a->title, token, sizeof(a->title));
			continue;
		} else if (Q_streq(token, "multiplayer")) {
			/* get map title */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			/* a multiplayer only tile - forced to be exactly once in the map when
			 * we are playing a multiplayer match */
			if (sv_maxclients->integer >= 2) {
				const mTile_t *t = SV_GetMapTile(map, token);
				if (t != NULL) {
					const ptrdiff_t i = t - map->mTile;
					a->min[i] = 1;
					a->max[i] = 1;
				} else {
					Com_Error(ERR_DROP, "Could not find multiplayer tile: '%s' in assembly '%s' (%s)", token, a->id, filename);
				}
			}
			continue;
		} else if (Q_streq(token, "size")) {
			/* get map size */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			sscanf(token, "%i %i", &a->width, &a->height);
			a->size = a->width * a->height;
			continue;
		} else if (Q_streq(token, "seeds")) {
			if (!SV_ParseAssemblySeeds(map, filename, text, a))
				return qfalse;
			continue;
		} else if (Q_streq(token, "grid")) {
			/* get map size */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			sscanf(token, "%i %i", &a->dx, &a->dy);
			continue;
		/* chose a tile from a tileset */
		} else if (Q_streq(token, "tileset")) {
			SV_GetTilesFromTileSet(map, filename, text, a);
			continue;
		/* fix tilename "x y" */
		} else if (Q_streq(token, "fix")) {
			const mTile_t *t;

			/* get tile */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			if (token[0] == '*') {
				token = SV_GetCvarToken(a, token + 1, filename, text, errhead);
				if (token == NULL)
					break;
			} else if (Q_streq(token, "tileset")) {
				token = SV_GetTileFromTileSet(map, filename, text, a);
			}

			t = SV_GetMapTile(map, token);
			if (t != NULL) {
				const ptrdiff_t i = t - map->mTile;
				if (a->numFixed >= MAX_FIXEDTILES)
					Com_Error(ERR_DROP, "SV_ParseAssembly: Too many fixed tiles in assembly '%s' (%s)", a->id, filename);

				/* get coordinates */
				token = Com_EParse(text, errhead, filename);
				if (!text)
					Com_Error(ERR_DROP, "SV_ParseAssembly: Error in assembly %s - could not get coordinates for fixed tile", filename);

				sscanf(token, "%i %i", &x, &y);
				if (x < 0 || x >= MAX_RANDOM_MAP_WIDTH) {
					Com_Error(ERR_DROP, "SV_ParseAssembly: Error, invalid fixed coordinates given for x (%i) boundaries are: [0:%i] (%s).",
							x, MAX_RANDOM_MAP_WIDTH - 1, filename);
				} else if (y < 0 || y >= MAX_RANDOM_MAP_HEIGHT) {
					Com_Error(ERR_DROP, "SV_ParseAssembly: Error, invalid fixed coordinates given for y (%i) - boundaries are: [0:%i] (%s).",
							y, MAX_RANDOM_MAP_HEIGHT - 1, filename);
				}
				a->fX[a->numFixed] = x;
				a->fY[a->numFixed] = y;
				a->fT[a->numFixed] = i;
				a->numFixed++;
			} else
				Com_Error(ERR_DROP, "Could not find fixed tile: '%s' in assembly '%s' (%s)", token, a->id, filename);
			continue;
		/* <format>*cvarname <defaultvalue> "min max"</format> */
		} else if (token[0] == '*') {
			token = SV_GetCvarToken(a, token + 1, filename, text, errhead);
			if (token == NULL)
				break;
		}

		tile = SV_GetMapTile(map, token);
		if (tile != NULL) {
			const ptrdiff_t i = tile - map->mTile;
			/* get min and max tile number */
			token = Com_EParse(text, errhead, filename);
			if (!text || *token == '}')
				Com_Error(ERR_DROP, "SV_ParseAssembly: Error in assembly %s (invalid syntax for tile %s)", filename, tile->id);

			if (!strstr(token, " "))
				Com_Error(ERR_DROP, "SV_ParseAssembly: Error in assembly %s (min max value of tile %s)", filename, tile->id);

			sscanf(token, "%i %i", &x, &y);
			a->min[i] = x;
			a->max[i] = y;
			if (a->min[i] > a->max[i])
				Com_Error(ERR_DROP, "SV_ParseAssembly: Error in assembly %s (min is bigger than max for tile %s)", filename, tile->id);
			if (a->max[i] <= 0)
				Com_Error(ERR_DROP, "SV_ParseAssembly: Error in assembly %s (max is <= 0 for tile %s)", filename, tile->id);
		} else {
			Com_Error(ERR_DROP, "Could not find tile: '%s' in assembly '%s' (%s)", token, a->id, filename);
		}
	} while (text);

	return qtrue;
}


/**
 * @brief Combines the alternatives/connection info of a map with a tile and sets the rating
 * @param[in,out] mapAlts Pointer to the alternatives info field of the map which will be updated.
 * @param[in] tileAlts Pointer to the alternatives info field of the tile.
 * @param[in,out] mapRating Pointer to the rating field of the map.
 * @sa SV_AssembleMap
 * @sa SV_AddRegion
 * @sa SV_FitTile
 */
static void SV_CombineAlternatives (unsigned long *mapAlts, const unsigned long tileAlts, char *mapRating)
{
	/* don't touch solid fields of the map, return if tile has no connection info */
	if (IS_SOLID(*mapAlts) || (tileAlts == ALL_TILES))
		return;

	/* for an empty map tile the rating must be zero */
	assert((*mapAlts != ALL_TILES) || (*mapRating == 0));

	/* copy if tile is solid */
	if (IS_SOLID(tileAlts)) {
		*mapAlts = tileAlts;
		*mapRating = 1;
	/* combine otherways */
	} else {
		*mapAlts &= tileAlts;
		(*mapRating)--;
	}
}

/**
 * @brief Reset the map to empty state.
 */
static void SV_ClearMap (mapInfo_t *map)
{
	unsigned long *mp = &map->curMap[0][0];
	const unsigned long *end = &map->curMap[MAX_RANDOM_MAP_HEIGHT - 1][MAX_RANDOM_MAP_WIDTH - 1];

	OBJZERO(map->curRating);

	while (mp <= end)
		*(mp++) = ALL_TILES;
}

/**
 * @brief Checks if a given map-tile fits into the empty space (in a given location) of a map.
 * @param[in,out] map All we know about the map to assemble
 * @param[in] tile The tile definition that should be fitted into the map.
 * @param[in] x The x position in the map where the tile is supposed to be placed/checked.
 * @param[in] y The y position in the map where the tile is supposed to be placed/checked.
 * @return @c true if the tile fits, @c false if the tile does not fit or an error was encountered.
 * @sa SV_AddMandatoryParts
 * @sa SV_AddRegion
 */
static qboolean SV_FitTile (const mapInfo_t *map, const mTile_t * tile, const int x, const int y)
{
	int tx, ty;
	const unsigned long *spec = NULL;
	const unsigned long *m = NULL;
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];

	/* check for valid grid positions */
	assert(x % mAsm->dx == 0);
	assert(y % mAsm->dy == 0);
	assert(tile);

	if (x < 0 || y < 0)
		return qfalse;

	/* check for map border */
	if (x + tile->w > mAsm->width + 2 || y + tile->h > mAsm->height + 2)
		return qfalse;
	unsigned long combined;
#if 0
	/* shortcut: most tiles are solid at [1][1], so check this first */
	spec = &tile->spec[1][1];
	m = &map->curMap[y+1][x+1];
	combined = (*m) & (*spec);
	if (IS_SOLID(combined) || !combined)
		return qfalse;
#endif
	/* test for fit */
	spec = &tile->spec[0][0];
	m = &map->curMap[y][x];
	for (ty = 0; ty < tile->h; ty++) {
		for (tx = 0; tx < tile->w; tx++, spec++, m++) {
			combined = (*m) & (*spec);

			/* quit if both are solid or no equal connection is found */
			if (IS_SOLID(combined) || !combined)
				return qfalse;
		}
		spec += (MAX_TILESIZE - tile->w);
		m += (MAX_RANDOM_MAP_WIDTH - tile->w);
	}

	return qtrue;
}

/**
 * @brief Checks if the map is completely filled.
 * @return @c true if the map is filled, @c false if the map has still empty fields
 * @sa SV_AssembleMap
 * @sa SV_AddRegion
 * @sa SV_FitTile
 */
static qboolean SV_TestFilled (const mapInfo_t *map)
{
	int x, y;
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];

	for (y = 1; y < mAsm->height + 1; y++)
		for (x = 1; x < mAsm->width + 1; x++)
			if (!IS_SOLID(map->curMap[y][x]))
				return qfalse;

	return qtrue;
}

/**
 * @brief Debug function to dump the map location of a placed tile.
 */
static void SV_DumpPlaced (const mapInfo_t *map, int pl)
{
	int x, y;
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];
	const int h = mAsm->height;
	const int w = mAsm->width;
	const mPlaced_t *placed = &map->mPlaced[pl];

	Com_Printf("Placed tile %s at %d %d\n", placed->tile->id, placed->x, placed->y);

	for (y = h; y >= 1; y--) {
		for (x = 1; x < w + 1; x++) {
			const int dx = x - placed->x;
			const int dy = y - placed->y;

			if (dx >= 0 && dx < placed->tile->w && dy >= 0 && dy < placed->tile->h &&
					IS_SOLID(placed->tile->spec[dy][dx]))
				Com_Printf(" X");
			else
				Com_Printf(" .");
		}
		Com_Printf("\n");
	}
	Com_Printf("\n");
}

/**
 * @brief Adds a new map-tile to an assembled map. Also adds the tile to the placed-tiles list.
 * @note The tile must fit at the given position, otherwise an assert will occur!
 * @param[in,out] map The map that will get the tile.  Modified in place.
 * @param[in] tile The tile to add to the map.
 * @param[in] x The x position in the map where the tile should be placed.
 * @param[in] y The y position in the map where the tile should be placed.
 * @param[in] idx The index of the placement algorithm.
 * @param[in] pos The position of the placement algorithm.
 * @sa SV_AssembleMap
 * @sa SV_AddRegion
 * @sa SV_FitTile
 */
static void SV_AddTile (mapInfo_t *map, const mTile_t *tile, int x, int y, int idx, int pos)
{
	int tx, ty;
#ifdef DEBUG
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];

	/* check vor valid grid positions */
	assert(x % mAsm->dx == 0);
	assert(y % mAsm->dy == 0);
#endif

	/* add the new tile */
	for (ty = 0; ty < tile->h; ty++)
		for (tx = 0; tx < tile->w; tx++) {
			assert(y + ty < MAX_RANDOM_MAP_HEIGHT);
			assert(x + tx < MAX_RANDOM_MAP_WIDTH);

			SV_CombineAlternatives(&map->curMap[y + ty][x + tx], tile->spec[ty][tx], &map->curRating[y + ty][x + tx]);
		}

	/* add the tile to the array of placed tiles*/
	if (map->numPlaced >= MAX_MAPTILES)
		Com_Error(ERR_DROP, "SV_AddTile: Too many map tiles");

	map->mPlaced[map->numPlaced].tile = tile;
	map->mPlaced[map->numPlaced].x = x;
	map->mPlaced[map->numPlaced].y = y;
	map->mPlaced[map->numPlaced].idx = idx;
	map->mPlaced[map->numPlaced].pos = pos;

	map->numPlaced++;

	if (idx >= 0) {
		map->mToPlace[idx].cnt++;
	}
}

/**
 * @brief Rebuilds a assembled map up to the previous tile.
 * @param[in,out] map All we know about the map to assemble
 * @param[out] idx Pointer to the location to store the index field of the removed tile
 * @param[out] pos Pointer to the location to store the position field of the removed tile
 * @sa SV_AssembleMap
 * @sa SV_AddTile
 * @sa SV_FitTile
 */
static void SV_RemoveTile (mapInfo_t *map, int* idx, int* pos)
{
	int tx, ty;
	int i, index;

	SV_ClearMap(map);

	if (map->numPlaced == 0)
		return;

	map->numPlaced--;
	index = map->mPlaced[map->numPlaced].idx;

	if (index >= 0) {
		map->mToPlace[index].cnt--;
	}

	for (i = map->numPlaced; i--;) {
		const mTile_t *tile = map->mPlaced[i].tile;
		const int x = map->mPlaced[i].x;
		const int y = map->mPlaced[i].y;
		assert(i >= 0);
		assert(tile);

		/* add the tile again*/
		for (ty = 0; ty < tile->h; ty++) {
			for (tx = 0; tx < tile->w; tx++) {
				assert(y + ty < MAX_RANDOM_MAP_HEIGHT);
				assert(x + tx < MAX_RANDOM_MAP_WIDTH);

				SV_CombineAlternatives(&map->curMap[y + ty][x + tx], tile->spec[ty][tx], &map->curRating[y + ty][x + tx]);
			}
		}
	}

	if (idx)
		*idx = index;

	if (pos)
		*pos = map->mPlaced[map->numPlaced].pos;
}

/**
 * @brief Prints the mapstrings as known from the ufoconsole.log
 * This can also be used to dump the progress of the RMA process.
 * @param[in,out] map All we know about the map to assemble
 * @param[out] asmMap The output string of the random map assembly that contains all the
 * map tiles that should be assembled. The order is the same as in the @c asmPos string.
 * Each of the map tiles in this string has a corresponding entry in the pos string, too.
 * @param[out] asmPos The pos string for the assembly. For each tile from the @c asmMap
 * string this string contains three coordinates for shifting the given tile names.
 */
static void SV_PrintMapStrings (mapInfo_t *map, char *asmMap, char *asmPos)
{
	int i;
	mAssembly_t *mAsm;
	mAsm = &map->mAssembly[map->mAsm];

	for (i = 0; i < map->numPlaced; i++) {
		const mPlaced_t *pl = &map->mPlaced[i];

		if (sv_dumpmapassembly->integer)
			SV_DumpPlaced(map, i);

		if (asmMap[0])
			Q_strcat(asmMap, " ", MAX_TOKEN_CHARS * MAX_TILESTRINGS);
		if (asmPos[0])
			Q_strcat(asmPos, " ", MAX_TOKEN_CHARS * MAX_TILESTRINGS);

		Q_strcat(asmMap, va("%s", pl->tile->id), MAX_TOKEN_CHARS * MAX_TILESTRINGS);
		Q_strcat(asmPos, va("%i %i %i", (pl->x - mAsm->width / 2) * 8, (pl->y - mAsm->height / 2) * 8, 0), MAX_TOKEN_CHARS * MAX_TILESTRINGS);
	}

	Com_Printf("tiles: %s\n", asmMap);
	Com_Printf("pos: %s\n", asmPos);
	Com_Printf("tiles: %i\n", map->numPlaced);
}

/**
 * @brief get the specs of a tile at map-x/y if it was placed where tileCode indicates
 * @param map All we know about the map to assemble
 * @param tileCode encoded tile and position
 * @param mapW width of the map. Needed to decode tileCode
 * @param mapX x of the gap
 * @param mapY y of the gap
 * @return the flags
 */
static unsigned long SV_GapGetFlagsAtAbsPos (mapInfo_t *map, int tileCode, int mapW, int mapX, int mapY)
{
	const int pos = tileCode / TCM;
	const int ti = tileCode % TCM;
	const int posX = pos % mapW;
	const int posY = pos / mapW;
	const mToPlace_t *mToPlace = map->mToPlace;
	const mTile_t *tile = mToPlace[ti].tile;

	return tile->spec[mapY - posY][mapX - posX];
}

/**
 * @brief Select the next tile to place and place it (recursively)
 * @param map All we know about the map to assemble
 * @param rec The number of the current recursion
 * @param posListCnt The number of remaining tile position (=options)
 * @param myPosList The previous list of remaining tile position (=options)
 * @param prevTile The tile placed by the previous recursion
 * @param prevX x where it was placed
 * @param prevY well, y
 * @return @c false if the tiles do not fit, @c true if the map could be filled.
 */
static int availableTiles[MAX_TILETYPES][2];	/* the 2nd dimension is index and count */

static qboolean SV_AddMissingTiles_r (mapInfo_t *map, int rec, int posListCnt, short myPosList[], const mTile_t *prevTile, int prevX, int prevY)
{
	static int callCnt = 0;
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];
	const int mapW = mAsm->width;
	const mToPlace_t *mToPlace = map->mToPlace;
	int i, j = 0;
	int solids = 0;				/* the # of places that the remaining tiles can theoretically cover */
	int availableTilesCnt = 0;	/* the # of different tiles remaining in posTileList */

	assert(rec < RMA2_MAX_REC);
	callCnt++;

	/** calculate the area of the tile placed by the previous recursion */
	int prevMaxX = 0, prevMaxY = 0;
	if (rec > 0) {
		prevMaxX = prevX + prevTile->w - 1;
		prevMaxY = prevY + prevTile->h - 1;
	}

	/** circle through the old list of available tile positions and store the remaining ones in the static array */
	for (i = 0; i < posListCnt; i++) {
		const int pos = myPosList[i] / TCM;
		const int ti = myPosList[i] % TCM;
		const int x = pos % mapW;
		const int y = pos / mapW;

		if (mToPlace[ti].cnt >= mToPlace[ti].max)
			continue;

		const mTile_t *cTile = mToPlace[ti].tile;
		qboolean ok = qfalse;
		/** try to reduce the # of calls to SV_FitTile by checking only those gaps affected by the placement of the previous tile */
		if (rec > 0) {	/* the first recursion doesn't have a previous tile */
			if (x > prevMaxX || y > prevMaxY || prevX > x + cTile->w - 1 || prevY > y + cTile->h - 1)
				ok = qtrue;	/* tiles do not overlap, so tile will still fit */
		}

		if (!ok) {
			ok = SV_FitTile(map, mToPlace[ti].tile, x, y);
		}
		if (ok) {
			/* store the posTile in our new list */
			assert(j < RMA2_MAX_TILEPOS);
			posTileList[rec][j] = myPosList[i];
			j++;
			/* store the tile index in the list of remaining tile types */
			int k;
			for (k = 0; k < availableTilesCnt; k++) {
				if (availableTiles[k][0] == ti) {
					availableTiles[k][1]++;		/* inc count */
					break;
				}
			}
			if (k >= availableTilesCnt)	{					/* didn't find it in the list */
				availableTiles[availableTilesCnt][0] = ti;	/* store the tile index */
				availableTiles[availableTilesCnt][1] = 1;	/* init counter of places */
				availableTilesCnt++;
			}
		}
	}

	/** initialize the list of gaps */
	int x, y;
	int gapCount = 0;
	for (y = 1; y < mAsm->height + 1; y++) {
		for (x = 1; x < mAsm->width + 1; x++)
			if (IS_SOLID(map->curMap[y][x]))
				gapList[x][y][0] = -1;
			else {
				gapCount++;
				gapList[x][y][0] = 0;
			}
	}

	/** if the remaining tiles are simply not enough to cover the gaps, bail */
	for (i = 0; i < availableTilesCnt; i++) {
		const int ti = availableTiles[i][0];
		const int allowed = mToPlace[ti].max - mToPlace[ti].cnt;
		const int possible = availableTiles[i][1];
		const int remaining = min(allowed, possible);
		solids += remaining * mToPlace[ti].tile->area;
	}
	if (solids < gapCount) {
		if (sv_rmadisplaythemap->integer) {
			SV_RmaPrintMap(map);
			Com_Printf("out of solids\n");
		}
		return qfalse;
	}

	/** check how well the remaining tiles can cover the gaps */
	for (i = 0; i < j; i++) {
		const int pos = posTileList[rec][i] / TCM;
		const int ti = posTileList[rec][i] % TCM;
		const int x = pos % mapW;
		const int y = pos / mapW;
		const mTile_t *tile = mToPlace[ti].tile;
		int tx, ty;
		for (ty = 0; ty < tile->h; ty++) {
			for (tx = 0; tx < tile->w; tx++) {
				if (IS_SOLID(tile->spec[ty][tx])) {
					gapList[x + tx][y + ty][0] += 1;
					int cnt = gapList[x + tx][y + ty][0];	/* get the counter */
					if (cnt < GAPS + 1)
						gapList[x + tx][y + ty][cnt] = posTileList[rec][i];	/* remember the tilecode */
				}
			}
		}
	}

	/** if we find a gap that NO tile can cover, bail */
	for (y = 1; y < mAsm->height + 1; y++) {
		for (x = 1; x < mAsm->width + 1; x++) {
			if (gapList[x][y][0] == 0) {
				if (sv_rmadisplaythemap->integer) {
					SV_RmaPrintMap(map);
					Com_Printf("uncovered gap: %i/%i\n", x, y);
				}
				return qfalse;
			}
		}
	}

#if 0
	/** print the gaplist */
	for (y = 1; y < mAsm->height + 1; y++) {
		for (x = 1; x < mAsm->width + 1; x++) {
			Com_Printf("%2.i ", gapList[x][y][0]);
		}
		Com_Printf("\n");
	}
	Com_Printf("\n");
#endif

	/** if there is a gap that only ONE tile can cover, try that. If it doesn't work out, we're done. */
	/** if there is a gap that only TWO tiles can cover, try those two. If it doesn't work out, we're done. Then try with THREE and so on. */
	int g, h, line = 0;
	unsigned lineFlags = map->lineFlags;	/* lineforming tiles, eg. water tiles in forest_large */
	unsigned nonLineFlags = (~lineFlags) ^ 1L;
	if (lineFlags)
		line = 1;
	for (; line >= 0; line--) {				/* if there are lineforming tiles, process them first */
		for (g = 1; g <= GAPS; g++) {		/* process the gaps with the least alternatives first */
			for (y = 1; y < mAsm->height + 1; y++) {
				for (x = 1; x < mAsm->width + 1; x++) {
					if (gapList[x][y][0] == g) {		/* if this gap has the right amount of covering tiles */
						/* if we are looking for lines but the gap doesn't require a line-tile, skip */
						if (line && (map->curMap[y][x] & nonLineFlags))
							continue;
						for (h = 1; h <= g; h++) {		/* circle through the alternatives stored for this gap */
							const int tc = gapList[x][y][h];
							const int pos = tc / TCM;
							const int ti = tc % TCM;
							const int px = pos % mapW;
							const int py = pos / mapW;

							SV_AddTile(map, mToPlace[ti].tile, px, py, ti, pos);
#if PRINT_RMA_PROGRESS
							if (rec < 10)
								Com_Printf("GAPS: %i rec: %i chances: %i calls: %i\n", GAPS, rec, j, callCnt);
#endif
							if (SV_TestFilled(map))
								return qtrue;		/* this was the last tile we needed */
							if (SV_AddMissingTiles_r(map, rec + 1, j, posTileList[rec], mToPlace[ti].tile, px, py))
								return qtrue;		/* recursive placement succeeded */

							/* tile was a dead end, remove it */
							SV_RemoveTile(map, NULL, NULL);

							if (h >= g) {
#if 0
								if (sv_rmadisplaythemap->integer) {
									SV_RmaPrintMap(map);
									Com_Printf("no tile works for gap\n");
								}
#endif
								return qfalse;
							}
						}
					}
				}
			}
		}
	}

	/** now try each of the remaining positions, that is, those that have more than GAPS alternatives. Rarely happens. */
	for (i = 0; i < j; i++) {
		const int pos = posTileList[rec][i] / TCM;
		const int ti = posTileList[rec][i] % TCM;
		const int x = pos % mapW;
		const int y = pos / mapW;

		SV_AddTile(map, mToPlace[ti].tile, x, y, ti, pos);
		if (SV_TestFilled(map))
			return qtrue;
		if (SV_AddMissingTiles_r(map, rec + 1, j, posTileList[rec], mToPlace[ti].tile, x, y))
			return qtrue;
		else
			SV_RemoveTile(map, NULL, NULL);
	}
	return qfalse;
}

/**
 * @brief Builds a list of map positions (gaps) and the tiles that can cover them.
 * @note actually it's not a list but a 3D-array
 * @param map All we know about the map to assemble
 * @param tilePosListCnt The number of remaining possible tile positions
 * @return qfalse if we detect a gap that NO tile can cover
 */
static qboolean SV_GapListBuild (mapInfo_t *map, int tilePosListCnt)
{
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];
	const int mapW = mAsm->width;
	const mToPlace_t *mToPlace = map->mToPlace;

	/** initialize the list of gaps */
	int x, y;
	for (y = 1; y < mAsm->height + 1; y++) {
		for (x = 1; x < mAsm->width + 1; x++)
			if (IS_SOLID(map->curMap[y][x]))
				gapList[x][y][0] = -1;		/* the gap is solid already, so we don't need a counter. But we might need the info. */
			else
				gapList[x][y][0] = 0;		/* the counter for this pos */
	}

	/* check how well the tiles can cover the gaps */
	int i;
	for (i = 0; i < tilePosListCnt; i++) {
		const int pos = posTileList[0][i] / TCM;
		const int ti = posTileList[0][i] % TCM;
		x = pos % mapW;
		y = pos / mapW;
		mTile_t *tile = mToPlace[ti].tile;
		int tx, ty;
		ty = 0;	/* for a breakpoint */
		for (ty = 0; ty < tile->h; ty++) {
			for (tx = 0; tx < tile->w; tx++) {
				if (IS_SOLID(tile->spec[ty][tx])) {
					gapList[x + tx][y + ty][0] += 1;
					const int cnt = gapList[x + tx][y + ty][0];	/* get the counter */
					if (cnt < GAPS + 1)
						gapList[x + tx][y + ty][cnt] = posTileList[0][i];	/* remember the tilecode */
				}
			}
		}
	}

	/** if we find a gap that NO tile can cover, bail */
	for (y = 1; y < mAsm->height + 1; y++) {
		for (x = 1; x < mAsm->width + 1; x++) {
			if (gapList[x][y][0] == 0)
				return qfalse;
		}
	}
	return qtrue;
}

/**
 * @brief Find a tile that meets the requirements of tc1 at a given pos
 * @param map All we know about the map to assemble
 * @param tc1 encoded tile and position
 * @param mapW width of the map. Needed to decode tc1
 * @param nx x of the absolute map position to investigate
 * @param nx y of the absolute map position to investigate
 * @return @c true if no matching tile was found
 */
static qboolean SV_GapCheckNeighbour (mapInfo_t *map, int tc1, int mapW, int nx, int ny)
{
	if (nx < 1)
		/* map border */
		return qfalse;
	if (ny < 1)
		return qfalse;
	if (nx > mapW)
		return qfalse;
	if (ny > map->mAssembly[map->mAsm].height)
		return qfalse;

	if (gapList[nx][ny][0] < 1)
		/* no tiles cover this gap, probably map border */
		return qfalse;
	if (gapList[nx][ny][0] >= GAPS)
		/* if there are more tiles than we stored the tc's of,
		 * we can not evaluate this neighbour. */
		return qfalse;

	qboolean flags1 = SV_GapGetFlagsAtAbsPos(map, tc1, mapW, nx, ny);
	if (IS_SOLID(flags1))
		/* nx/ny is part of tc1 itself */
		return qfalse;

	/** circle through the tiles that cover this gap */
	int h;
	for (h = 1; h <= gapList[nx][ny][0]; h++) {
		const int tc2 = gapList[nx][ny][h];
		const unsigned long flags2 = SV_GapGetFlagsAtAbsPos(map, tc2, mapW, nx, ny);

		if (flags1 & flags2) {
			/* found at least one tile that would work */
			return qfalse;
		}
	}
	return qtrue;
}

/**
 * @brief Tries to find tiles that exclude all of their neighbours
 * This is called only once, before recursion starts. So we can safely (ab)use the posTileList space for recursion 1
 * @param map All we know about the map to assemble
 * @return the number of tiles that could be eliminated
 */
static int SV_GapListReduce (mapInfo_t *map)
{
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];
	const int mapW = mAsm->width;
	int x, y;
	int n = 0;	/** number of tiles marked for elinimation */

	/** if there is a tile that doesn't match ANY of the tiles available for the adjacent gap we can eliminate it. */
	for (y = 1; y < mAsm->height + 1; y++) {
		for (x = 1; x < mAsm->width + 1; x++) {
			if (gapList[x][y][0] < 1)	/* solid ? */
				continue;

			int g;
			for (g = 1; g <= gapList[x][y][0]; g++) {
				const int tc1 = gapList[x][y][g];
				if (g >= GAPS)
					break;	/* there are more tiles than we stored the tc's of. */

				/* check the neighbour to the right */
				if (SV_GapCheckNeighbour(map, tc1, mapW, x+1, y)) {
					posTileList[1][n] = tc1;
					n++;
					continue;
				}
				/* check the neighbour to the left */
				if (SV_GapCheckNeighbour(map, tc1, mapW, x-1, y)) {
					posTileList[1][n] = tc1;
					n++;
					continue;
				}
				/* check the upper neighbour */
				if (SV_GapCheckNeighbour(map, tc1, mapW, x, y+1)) {
					posTileList[1][n] = tc1;
					n++;
					continue;
				}
				/* check the neighbour below */
				if (SV_GapCheckNeighbour(map, tc1, mapW, x, y-1)) {
					posTileList[1][n] = tc1;
					n++;
					continue;
				}
			}
		}
	}

	return n;
}

#if PRINT_RMA_PROGRESS
	char mapStr[10000] = {0};
	char posStr[10000] = {0};
#endif
/**
 * @brief Tries to fill the missing tiles of the current map.
 * While the 2010 algo used a 'by chance'-algo, this algo does a full tree search.
 * Example: After placing the fixed and required tiles on a 8x8 map, we have say 32 places not yet covered by tiles.
 * If there are 10 different tiles for each gap, we can have up to have 10^32 constellations to try.
 * That's about one fantastillion. Impossible.
 * Fortunately, there are usually many solutions to this puzzle. And we only need to find one working solution.
 * But on some maps eg. 'forest large' with it's little brook, there are fewer solutions, so the search can take longer.
 * In order to complete the search in a reasonable time, this algo uses several strategies:
 * - reduce the number of available options to narrow the tree
 * - direct the search towards those options that are more likely to fail. This can cut off a whole branch of the tree.
 * Details will be explained along the call-tree
 * @param map All we know about the map to assemble
 * @return @c false if the tiles does not fit, @c true if the map could be filled.
 * @sa SV_FitTile
 * @sa SV_AddTile
 */
static qboolean SV_AddMissingTiles (mapInfo_t *map)
{
	static int attempts = 0;			/* how often this function is called in the RMA process */
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];
	const int mapSize = mAsm->size;		/* the # of grid squares in the assembly. A grid suare is usually 8x8 cells. */
	const int mapW = mAsm->width;
	const mToPlace_t *mToPlace = map->mToPlace;
	short posList[MAX_RANDOM_MAP_HEIGHT * MAX_RANDOM_MAP_WIDTH];	/* array of random positions */
	short tilenumList[MAX_TILETYPES];	/* array of tiles */

	/* shuffle only once, the map will be build with that seed */
	RandomList(mapSize, posList);
	RandomList(map->numToPlace, tilenumList);
	attempts++;

	/* check if the map is already filled */
	if (SV_TestFilled(map))
		return qtrue;

	/** build a list of all positions of the map and all the tiles that fit there */
	int i, j, k, offs, num, n = 0;
	for (i = 0; i < mapSize; i++) {
		const int x = posList[i] % mapW;
		const int y = posList[i] / mapW;

		/* only use positions that are on the grid */
		if (x % mAsm->dx != 0 || y % mAsm->dy != 0) {
			continue;
		}
		/* if we simply test the tiles in the same sequence for each pos, we get the 'boring maps' problem */
		/* So let's check them from a different starting point in the list each time */
		/* Example: if we have say 20 tiles, test eg. 13-20 first, then 1-12 */
		num = map->numToPlace;
		offs = rand() % num;
		for (k = offs; k < num + offs; k++) {
			const int ti = tilenumList[k % num];

			if (mToPlace[ti].cnt >= mToPlace[ti].max)
				continue;
			if (SV_FitTile(map, mToPlace[ti].tile, x, y)) {
				posTileList[0][n] = posList[i] * TCM + ti;
				assert(n < RMA2_MAX_TILEPOS);
				n++;
			}
		}
	}
#if PRINT_RMA_PROGRESS
	Com_Printf("\nMapsize: %i tiles: %i chances: %i\n", mapSize, map->numToPlace, n);
	mapStr[0] = 0;
	posStr[0] = 0;
	if (map->numPlaced < 8)
		SV_PrintMapStrings(map, mapStr, posStr);
#endif
	/** try to reduce the number of available options */
	qboolean eliminated = qtrue;
	while (eliminated) {		/* if we could eliminate one or more tiles, try again */
		eliminated = qfalse;
#if 0
		/* print the posTileList */
		for (i = 0; i < n; i++) {
			Com_Printf("%2.i/%2.i ", posTileList[0][i] / TCM, posTileList[0][i] % TCM);
			if (!(i % 10))
				Com_Printf("\n");
		}
		Com_Printf("\n");
#endif
		qboolean covered = SV_GapListBuild(map, n);

#if 0
		/* print the gapList */
		int x, y;
		Com_Printf("\n");
		for (x = 0; x < mAsm->width + 1; x++){
			for (y = 0; y < mAsm->height + 1; y++){
				int cnt = gapList[x][y][0];
				Com_Printf("x:%i y:%i cnt:%i ",x,y,cnt);
				for (j = 0; j <= cnt + 3; j++) {
					Com_Printf("%i ", gapList[x][y][j]);
				}
				Com_Printf("\n");
			}
		}
#endif

		if (!covered)
			return qfalse;		/* creating the gapList left one gap uncovered */

		int m = SV_GapListReduce(map);
		if (m) {				/* the number of tilepositions to discard */
			eliminated = qtrue;
			for (j = 0; j < m; j++) {
				const int tc = posTileList[1][j];	/* SV_GapListReduce abuses the space for rec=1 to return it's result */
				int offset = 0;
				for (i = 0; i < n; i++) {
					if (posTileList[0][i] == tc) {
						offset = 1;
						continue;
					}
					posTileList[0][i - offset] = posTileList[0][i];
				}
				if (offset)		/* only if we actually removed the tile */
					n--;		/* reduce the counter of posTileList */
			}
		}
	}

	return SV_AddMissingTiles_r(map, 0, n, posTileList[0], NULL, 0, 0);
}

/**
 * @brief Tries to build the map
 * There are 3 categories of tiles:
 * - fixed: the position of the tile is already given in the assembly definition
 * - required: a given number of such a tile must be placed somewhere on the map
 * - optional: up to a given number of these tiles may be used in the map
 * The algorithm works as follows:
 * The fixed tiles have already been placed in the calling function.
 * This function here now places the required tiles at random positions.
 * Then the remaining gaps are filled with the optional tiles.
 * If that fails, the last required tile is moved to a different place and the filling is tried again.
 * If no more places are left for the required tile, the previous required tile is relocated and so on.
 * That is, for the required tiles every possible combination is tried before we give up.
 * @param map All we know about the map to assemble
 * @sa SV_FitTile
 * @sa SV_AddTile
 */
static qboolean SV_AddMapTiles (mapInfo_t *map)
{
	int idx;	/* index in the array of available tiles */
	int pos;	/* index in the array of random positions */
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];
	const int mapW = mAsm->width;		/* width in x-direction */
	const int mapSize = mAsm->size;		/* the # of grid squares in the assembly. A grid suare is usually 8x8 cells. */
	const int numToPlace = map->numToPlace;
	const mToPlace_t *mToPlace = map->mToPlace;		/* pointer to a tile descriptor */
	short prList[MAX_RANDOM_MAP_HEIGHT * MAX_RANDOM_MAP_WIDTH];	/* array of random positions */
	const int start = map->numPlaced;
#ifdef DEBUG
	const mPlaced_t *mPlaced = map->mPlaced;
#endif

#if PRINT_RMA_PROGRESS
	char mapStr[10000] = {0};
	char posStr[10000] = {0};
#endif

	/* shuffle only once, the map will be build with that seed */
	RandomList(mapSize, prList);

	pos = 0;
	idx = 0;
	while (idx < numToPlace) {		/* for all tile-descriptors */
		while (mToPlace[idx].cnt < mToPlace[idx].min) {
			for (; pos < mapSize; pos++) {
				const int x = prList[pos] % mapW;
				const int y = prList[pos] / mapW;
				if (sv_threads->integer && sv_rma->integer == 1) {
					if (SDL_SemValue(mapSem) != 1) {
						/* someone else beat me to it */
						return qtrue;
					}
				}

				if ((x % mAsm->dx != 0) || (y % mAsm->dy != 0))
					continue;

				if (SV_FitTile(map, mToPlace[idx].tile, x, y)) {
					/* add tile */
					SV_AddTile(map, mToPlace[idx].tile, x, y, idx, pos);
#if PRINT_RMA_PROGRESS
					mapStr[0] = 0;
					posStr[0] = 0;
					if (map->numPlaced < 6)
						SV_PrintMapStrings(map, mapStr, posStr);
#endif
					break;
				}
			}
			/* tile fits, try another tile of the same type */
			if (pos < mapSize)
				continue;

			/* tile doesn't fit and no try left with this tile */
			if (!mToPlace[idx].cnt)
				break;

			/* tile does not fit, restore last status - replace the last tile */
			assert(map->numPlaced > 0);
#ifdef DEBUG
			assert(idx == mPlaced[map->numPlaced - 1].idx);
#endif
			if (sv_rmadisplaythemap->integer) {
				SV_RmaPrintMap(map);
				Com_Printf("required tile doesn't fit\n");
			}
			SV_RemoveTile(map, &idx, &pos);
			pos++;
		}

		/* tile fits, try next tile */
		if (pos < mapSize) {
			pos = 0;	/* start at the beginning of the random positions array */
			idx++;
		} else {
			/* no more retries */
			if (start == map->numPlaced) {
				if (mAsm->numSeeds == 0 || map->retryCnt > 2) {
					Com_Error(ERR_DROP, "SV_AddMapTiles: Impossible to assemble map '%s' with assembly '%s'\n",
							map->name, mAsm->id ? mAsm->id : "");
				} else {
					Com_Printf("SV_AddMapTiles: Impossible to assemble map '%s' with assembly '%s' - retry with another seed\n",
							map->name, mAsm->id ? mAsm->id : "");
					return qfalse;
				}
			}
			SV_RemoveTile(map, &idx, &pos);
			pos++;
		}

		if (idx == numToPlace && !SV_AddMissingTiles(map)) {
			if (sv_rmadisplaythemap->integer) {
				SV_RmaPrintMap(map);
				Com_Printf("couldn't pad\n");
			}
			SV_RemoveTile(map, &idx, &pos);
			pos++;
		}
	}

	return qtrue;
}

/**
 * @brief Prepare the list of tiles to place
 * @sa SV_AssembleMap
 * @sa SV_AddTile
 */
void SV_PrepareTilesToPlace (mapInfo_t *map)
{
	int i;
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];

	map->numToPlace = 0;
	OBJZERO(map->mToPlace);

	for (i = 0; i < map->numTiles; i++) {
		if (mAsm->max[i]) {
			map->mToPlace[map->numToPlace].tile = &map->mTile[i];
			map->mToPlace[map->numToPlace].min = mAsm->min[i];
			map->mToPlace[map->numToPlace].max = mAsm->max[i];
			map->numToPlace++;
		}
	}
}

/**
 * @brief The main function for the threads that try to create random map assemblies in parallel.
 * @param data The @c mapInfo_t structure local to this thread.  Should be initialized
 *  by memcpy-ing the actual map into new memory.  Not thread-safe to read or write, this thread
 *  assumes that nobody else will access the given copy of the map before the thread ends.
 *  It is the responsibility of the caller to free the map, if needed, after the thread has died
 *  and been collected.
 * @return 0 on success, -1 if it was interrupted via the @c mapSem semaphore, signaling that
 *  someone else has finished first, or timeout occurred.
 */
static int SV_AssemblyThread (void* data)
{
	mapInfo_t *map = (mapInfo_t*) data;

	Com_SetRandomSeed(time(NULL));

	if (!SV_AddMapTiles(map)) {
		map->retryCnt++;
	}

	/* the first thread to reach this point, gets the semaphore */
	if (SDL_SemTryWait(mapSem) != 0)
		return -1;
	TH_MutexLock(mapLock);

	assert(threadID == 0);
	threadID = SDL_ThreadID();

	/* tell main we're done */
	SDL_CondSignal(mapCond);
	TH_MutexUnlock(mapLock);

	return 0;
}

/**
 * @brief Spawn ASSEMBLE_THREADS threads to try and assemble a map.  The first map complete gets returned.
 *  Allocates a new copy of the map for each thread, and frees it at the end.
 *  Uses a timeout (initially 5 seconds).  If the spawned threads have not completed by the timeout, they
 *  are restarted, with double the timeout.
 *  @note The algorithm main points:
 *  The synchronization of threads happens using a semaphore @c mapSem, a lock @c mapLock, and a condition @c mapCond.
 *  The semaphore is initially 1 (and reset to 1 every time there is a restart).
 *  The first thread that finishes, grabs the semaphore, to tell all other threads to abort.
 *  All threads test the semaphore, if it is 0, they abort.
 *  After the timeout, the main thread grabs the semaphore, to make everybody conclude, and then restarts them.
 *  The lock is used to protect writes to the threadID global variable, that holds the ID of the thread which finished,
 *  if any.  It is also used to protect the conditional @c mapCond, used by the finished thread to notify the main()
 *  thread, so it can collect all threads and copy the final map back to the caller.
 *  The lock is locked by main() at all times, unless it is waiting on the conditional (with timeout).
 *  When an assembler thread finishes, it grabs the lock (which means main() is still waiting), writes its ID to
 *  threadID, signals main() and releases the lock.  main() gets the signal after the lock is released, since the signal
 *  is protected by the lock, so there can be no race between finishing assembly and signaling main() for the
 *  assembler threads.
 *  When a timeout occurs, main() exits the conditional by grabbing the lock again.  This will prevent any thread
 *  from exiting, even if it finishes between the time that main() timed out and the time it tries to get the semaphore.
 *  So, main() checks the semaphore to see if it is taken, and if so doesn't restart, despite the timeout.
 *  @todo Maybe we also need to reduce the timeout value a bit every time it succeeds?
 */
static int SV_ParallelSearch (mapInfo_t *map)
{
	SDL_Thread *threads[ASSEMBLE_THREADS];
	mapInfo_t *maps[ASSEMBLE_THREADS];
	int i;
	static int timeout = 5000;  /* wait for 5 sec initially, double it every time it times out */
	const int threadno = min(sv_threads->integer, ASSEMBLE_THREADS);

	assert(mapLock == NULL);
	mapLock = TH_MutexCreate("rma");

	assert(mapCond == NULL);
	mapCond = SDL_CreateCond();

	threadID = 0;
	assert(mapSem == NULL);
	mapSem = SDL_CreateSemaphore(1);

	TH_MutexLock(mapLock);
	for (i = 0; i < threadno; i++) {
		maps[i] = Mem_AllocType(mapInfo_t);
		memcpy(maps[i], map, sizeof(*map));
		threads[i] = SDL_CreateThread(SV_AssemblyThread, (void*) maps[i]);
	}
	while (threadID == 0) {
		/* if nobody is done after 5 sec, restart, double the timeout. */
		if (TH_MutexCondWaitTimeout(mapLock, mapCond, timeout) != 0) {
			Com_Printf("SV_ParallelSearch: timeout at %i ms, restarting\n", timeout);
			timeout += timeout;
			/* tell them all to die */
			if (SDL_SemTryWait(mapSem) != 0) {
				/* couldn't tell everyone to die, someone must have finished since the last line... */
				continue;
			}
			/* collect the dead */
			for (i = 0; i < threadno; i++) {
				SDL_WaitThread(threads[i], NULL);
			}
			/* reset semaphore */
			SDL_SemPost(mapSem);
			/* start'em again */
			for (i = 0; i < threadno; i++) {
				memcpy(maps[i], map, sizeof(*map));
				threads[i] = SDL_CreateThread(SV_AssemblyThread, (void*) maps[i]);
			}
		} else {
			/* someone finished */
			assert(threadID != 0);
		}
	}
	TH_MutexUnlock(mapLock);
	for (i = 0; i < threadno; i++) {
		if (SDL_GetThreadID(threads[i]) == threadID) {
			memcpy(map, maps[i], sizeof(*map));
		}

		SDL_WaitThread(threads[i], NULL);
		Mem_Free(maps[i]);
	}

	/* cleanup, for possible next time */
	SDL_DestroySemaphore(mapSem);
	SDL_DestroyCond(mapCond);
	TH_MutexDestroy(mapLock);
	mapLock = NULL;
	mapSem = NULL;
	mapCond = NULL;
	threadID = 0;
	timeout = 5000;

	return 0;
}

/**
 * @brief Parses an ump file that contains the random map definition
 * @param[in] name The basename of the ump file (without extension)
 * @param[out] map The data structure to store the parsed data in
 * @param[in] inherit When @c true, this is called to inherit tile definitions
 * from another ump file (no assemblies)
 */
void SV_ParseUMP (const char *name, mapInfo_t *map, qboolean inherit)
{
	char filename[MAX_QPATH];
	byte *buf;
	const char *text, *token;

	/* load the map info */
	Com_sprintf(filename, sizeof(filename), "maps/%s.ump", name);
	FS_LoadFile(filename, &buf);
	if (!buf)
		Com_Error(ERR_DROP, "SV_ParseUMP: Map assembly info '%s' not found", filename);

	/* parse it */
	text = (const char*)buf;
	do {
		token = Com_Parse(&text);
		if (!text)
			break;

		if (Q_streq(token, "extends")) {
			token = Com_Parse(&text);
			SV_ParseUMP(token, map, qtrue);
		} else if (Q_streq(token, "base")) {
			token = Com_Parse(&text);
			if (inherit)
				Q_strncpyz(map->inheritBasePath, token, sizeof(map->inheritBasePath));
			else
				Q_strncpyz(map->basePath, token, sizeof(map->basePath));
		} else if (Q_streq(token, "line")) {
			token = Com_Parse(&text);
			const char *p = token;
			map->lineFlags = 0;
			while (*p) {
				map->lineFlags |= tileMask(*p);
				p++;
			}
		} else if (Q_streq(token, "tileset")) {
			if (map->numTileSets >= MAX_TILESETS)
				Com_Printf("SV_ParseUMP: Too many map tileset found in (%s)\n", filename);
			else if (SV_ParseMapTileSet(filename, &text, map, inherit))
				map->numTileSets++;
		} else if (Q_streq(token, "tile")) {
			if (map->numTiles >= MAX_TILETYPES)
				Com_Printf("SV_ParseUMP: Too many map tile types (%s)\n", filename);
			else if (SV_ParseMapTile(filename, &text, map, inherit))
				map->numTiles++;
		} else if (Q_streq(token, "assembly")) {
			if (inherit) {
				FS_SkipBlock(&text);
			} else {
				if (map->numAssemblies >= MAX_MAPASSEMBLIES)
					Com_Printf("SV_ParseUMP: Too many map assemblies (%s)\n", filename);
				else if (SV_ParseAssembly(map, filename, &text, &map->mAssembly[map->numAssemblies]))
					map->numAssemblies++;
			}
		} else if (token[0] == '{') {
			Com_Printf("SV_ParseUMP: Skipping unknown block\n");
			/* ignore unknown block */
			text = strchr(text, '}') + 1;
			if (!text)
				break;
		} else
			Com_Printf("SV_ParseUMP: Unknown token '%s' (%s)\n", token, filename);
	} while (text);

	/* free the file */
	FS_FreeFile(buf);
}

#if SORT_BY_SIZE
static int cmpTileAreaSize (const void * a, const void * b)
{
	if (((const mToPlace_t *) a)->tile->area > ((const mToPlace_t *) b)->tile->area)
		return -1;
	else if (((const mToPlace_t *) a)->tile->area == ((const mToPlace_t *) b)->tile->area)
		return 0;
	return 1;
}
#endif

static mapInfo_t* SV_DoMapAssemble (mapInfo_t *map, const char *assembly, char *asmMap, char *asmPos, const unsigned int seed)
{
	int i;
	mAssembly_t *mAsm = &map->mAssembly[map->mAsm];

	Com_DPrintf(DEBUG_SERVER, "Use assembly: '%s'\n", mAsm->id);

	/* check size */
	assert(mAsm->width <= MAX_RANDOM_MAP_WIDTH);
	assert(mAsm->height <= MAX_RANDOM_MAP_HEIGHT);

	SV_PrepareTilesToPlace(map);

#if SORT_BY_SIZE
	/* This is the perfect time to sort them by size, which helps RMA a lot.
	 * This eliminates the need for a seedlist in oriental large completely.
	 * Unfortunately, it doesn't do that for the others. Instead, it can slow down some maps quite a bit. */
	qsort(map->mToPlace, map->numToPlace, sizeof(mToPlace_t), cmpTileAreaSize);
#endif

	/* assemble the map */
	map->numPlaced = 0;
	SV_ClearMap(map);

	/* place fixed parts - defined in ump via fix parameter */
	for (i = 0; i < mAsm->numFixed; i++)
		SV_AddTile(map, &map->mTile[mAsm->fT[i]], mAsm->fX[i], mAsm->fY[i], -1, -1);

	if (sv_threads->integer && sv_rma->integer == 1) {
		int oldCount = map->retryCnt;
		if (SV_ParallelSearch(map) < 0) {
			if (oldCount < map->retryCnt && mAsm->numSeeds > 0) {
				/* if we are allowed to restart the search with a fixed seed
				 * from the assembly definition, do so */
				Com_SetRandomSeed(mAsm->seeds[rand() % mAsm->numSeeds]);
				return SV_DoMapAssemble(map, assembly, asmMap, asmPos, seed);
			}
			Mem_Free(map);
			return NULL;
		}
	} else {
		unsigned int seedUsed;

		if (mAsm->numSeeds > 0) {
			/* if the map has a seedlist defined, use that */
			seedUsed = mAsm->seeds[rand() % mAsm->numSeeds];
			if (seed) {
				/* if a seed was passed, we are in cunit test mode */
				if (seed >= mAsm->numSeeds)
					/* if the given seed is outside the seedlist, assume that we already tested it and pretend that it's ok */
					return map;
				/* use the passed seed as an index into the seedlist */
				seedUsed = mAsm->seeds[seed % mAsm->numSeeds];
			}
			Com_Printf("Picked seed: %i for <%s>\n", seedUsed, assembly);
		} else {
			/* no seedlist */
			if (seed)
				seedUsed = seed;
			else
				seedUsed = rand() % 50;	/* limit the possible seeds to (testable) values between 0 and 49 */
		}
		Com_SetRandomSeed(seedUsed);
		Com_Printf("Using RMA%i seed: %i for <%s>\n", sv_rma->integer, seedUsed, assembly);

		if (!SV_AddMapTiles(map)) {
			map->retryCnt++;
			if (mAsm->numSeeds > 0) {
				/* if we are allowed to restart the search with a fixed seed
				 * from the assembly definition, do so */
				Com_SetRandomSeed(mAsm->seeds[seed % mAsm->numSeeds]);
				return SV_DoMapAssemble(map, assembly, asmMap, asmPos, seed);
			}
			return NULL;
		}
	}

	/* prepare map and pos strings */
	if (map->basePath[0])
		Com_sprintf(asmMap, sizeof(map->basePath) + 1, "-%s", map->basePath);

	asmPos[0] = 0;

	/* generate the strings */
	SV_PrintMapStrings(map, asmMap, asmPos);

	return map;
}

/**
 * @brief Assembles a "random" map
 * and parses the *.ump files for assembling the "random" maps and places the 'fixed' tiles.
 * For a more detailed description of the whole algorithm see SV_AddMapTiles.
 * @param[in] name The name of the map (ump) file to parse
 * @param[in] assembly The random map assembly that should be used from the given rma
 * @param[out] asmMap The output string of the random map assembly that contains all the
 * map tiles that should be assembled. The order is the same as in the @c asmPos string.
 * Each of the map tiles in this string has a corresponding entry in the pos string, too.
 * @param[out] asmPos The pos string for the assembly. For each tile from the @c asmMap
 * string this string contains three coordinates for shifting the given tile names.
 * @param[in] seed random seed to use (for cunit tests). If 0, the called functions can use their own seed setting.
 * @sa B_AssembleMap_f
 * @sa SV_AddTile
 * @sa SV_AddMandatoryParts
 * @sa SV_ParseAssembly
 * @sa SV_ParseMapTile
 * @note Make sure to free the returned pointer
 */
mapInfo_t* SV_AssembleMap (const char *name, const char *assembly, char *asmMap, char *asmPos, const unsigned int seed)
{
	mapInfo_t *map;

	map = Mem_AllocType(mapInfo_t);
	Q_strncpyz(map->name, name, sizeof(map->name));

	SV_ParseUMP(name, map, qfalse);

	/* check for parsed tiles and assemblies */
	if (!map->numTiles)
		Com_Error(ERR_DROP, "No map tiles defined (%s)!", map->name);
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_SERVER, "numTiles: %i\n", map->numTiles);
#endif

	if (!map->numAssemblies)
		Com_Error(ERR_DROP, "No map assemblies defined (%s)!", map->name);
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_SERVER, "numAssemblies: %i\n", map->numAssemblies);
#endif

	/* use random assembly, if no valid one has been specified */
	map->mAsm = rand() % map->numAssemblies;

	/* overwrite with specified, if any */
	if (assembly && assembly[0]) {
		int i;
		for (i = 0; i < map->numAssemblies; i++)
			if (Q_streq(assembly, map->mAssembly[i].id)) {
				map->mAsm = i;
				break;
			}
		if (i == map->numAssemblies) {
			Com_Printf("SV_AssembleMap: Map assembly '%s' not found\n", assembly);
		}
	}

	SV_DoMapAssemble(map, assembly, asmMap, asmPos, seed);

	return map;
}
