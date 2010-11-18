/**
 * @file sv_rma.c
 * @brief Random map assembly code
 * More info on map-assembly can be found at:
 * http://ufoai.ninex.info/wiki/index.php/Mapping/Random_map_assembly
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../shared/threads.h"

#define ASSEMBLE_THREADS 2
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

static const mTileSet_t *SV_GetMapTileSet (const mapInfo_t *map, const char *tileSetName)
{
	int i;

	for (i = 0; i < map->numTileSets; i++)
		if (!strcmp(tileSetName, map->mTileSets[i].id))
			return &map->mTileSets[i];

	return NULL;
}

static inline const mTile_t *SV_GetMapTile (const mapInfo_t *map, const char *tileName)
{
	int i;

	for (i = 0; i < map->numTiles; i++)
		if (!strcmp(tileName, map->mTile[i].id))
			return &map->mTile[i];

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

	memset(target, 0, sizeof(*target));

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

	/* get tile name */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return qfalse;

	memset(target, 0, sizeof(*target));

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

static inline const char *SV_GetTileFromTileSet (const mapInfo_t *map, const char *filename, const char **text, const mAssembly_t *a)
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
		Com_Error(ERR_DROP, "SV_GetTileFromTileSet: Could not find tileset %s 	in %s (assembly %s)", token, filename, a->id);

	random = rand() % tileSet->numTiles;
	return tileSet->tiles[random];
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
	memset(a, 0, sizeof(*a));
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

		if (!strcmp(token, "title")) {
			/* get map title */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			Q_strncpyz(a->title, token, sizeof(a->title));
			continue;
		} else if (!strcmp(token, "multiplayer")) {
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
		} else if (!strcmp(token, "size")) {
			/* get map size */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			sscanf(token, "%i %i", &a->width, &a->height);
			a->size = a->width * a->height;
			continue;
		} else if (!strcmp(token, "grid")) {
			/* get map size */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			sscanf(token, "%i %i", &a->dx, &a->dy);
			continue;
		/* chose a tile from a tileset */
		} else if (!strcmp(token, "tileset")) {
			token = SV_GetTileFromTileSet(map, filename, text, a);
		/* fix tilename "x y" */
		} else if (!strcmp(token, "fix")) {
			const mTile_t *t;

			/* get tile */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			if (token[0] == '*') {
				token = SV_GetCvarToken(a, token + 1, filename, text, errhead);
				if (token == NULL)
					break;
			} else if (!strcmp(token, "tileset")) {
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
	unsigned long *end = &map->curMap[MAX_RANDOM_MAP_HEIGHT - 1][MAX_RANDOM_MAP_WIDTH - 1];

	memset(map->curRating, 0, sizeof(map->curRating));

	while (mp <= end)
		*(mp++) = ALL_TILES;
}

/**
 * @brief Checks if a given map-tile fits into the empty space (in a given location) of a map.
 * @param[in,out] map All we know about the map to assemble
 * @param[in] tile The tile definition that should be fitted into the map.
 * @param[in] x The x position in the map where the tile is supposed to be placed/checked.
 * @param[in] y The y position in the map where the tile is supposed to be placed/checked.
 * @return qtrue if the tile fits.
 * @return qfalse if the tile does not fit or an error was encountered.
 * @sa SV_AddMandatoryParts
 * @sa SV_AddRegion
 */
static qboolean SV_FitTile (const mapInfo_t *map, mTile_t * tile, const int x, const int y)
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

	/* test for fit */
	spec = &tile->spec[0][0];
	m = &map->curMap[y][x];
	for (ty = 0; ty < tile->h; ty++) {
		for (tx = 0; tx < tile->w; tx++, spec++, m++) {
			const unsigned long combined = (*m) & (*spec);

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
 * @return qtrue if the map is filled
 * @return qfalse if the map has still empty fields
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
 * @brief Debug fuction to dump the rating of the current map.
 */
static void SV_DumpRating (const mapInfo_t *map)
{
	int x, y;
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];

	Com_Printf("Rating:\n");
	for (y = mAsm->height; y >= 1; y--) {
		for (x = 1; x < mAsm->width + 1; x++)
			Com_Printf(" %2d", (int) map->curRating[y][x]);
		Com_Printf("\n");
	}
	Com_Printf("\n");
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

			if ((dx >= 0) && (dx < placed->tile->w) &&
					(dy >= 0) && (dy < placed->tile->h) &&
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
 * @brief Returns the rating of the given map.
 * @return A value which roughly describes the connection quality of the map
 * @sa SV_AssembleMap
 * @sa SV_AddRegion
 * @sa SV_FitTile
 */
static int SV_CalcRating (const mapInfo_t *map)
{
	int x, y, rating = 0;
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];

	for (y = 1; y <= mAsm->height; y++)
		for (x = 1; x <= mAsm->width; x++)
			rating += map->curRating[y][x];

	if (sv_dumpmapassembly->integer)
		SV_DumpRating(map);

	return rating;
}

/**
 * @brief Adds a new map-tile to an assembled map. Also adds the tile to the placed-tiles list.
 * @note The tile must fit at the given position, otherwise an assert will occure!
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
 * @brief Tries to fit a tile in the current map.
 * @return qtrue if a fitting tile was found.
 * @return qfalse if no tile fits.
 * @sa SV_FitTile
 * @sa SV_AddTile
 */
static qboolean SV_PickRandomTile (mapInfo_t *map, int* idx, int* pos)
{
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];
	const int numToPlace = map->numToPlace;
	const int mapSize = mAsm->size;
	const int mapW = mAsm->width;
	const int start_idx = *idx = rand() % numToPlace;
	const int start_pos = *pos = rand() % mapSize;
	const mToPlace_t *mToPlace = map->mToPlace;

	do {
		if (mToPlace[*idx].cnt < mToPlace[*idx].max) {
			do {
				const int x = (*pos) % mapW;
				const int y = (*pos) / mapW;

				if ((x % mAsm->dx == 0)
					&& (y % mAsm->dy == 0)
					&& SV_FitTile(map, mToPlace[*idx].tile, x, y)) {
					return qtrue;
				}

				(*pos) += 1;
				(*pos) %= mapSize;

			} while ((*pos) != start_pos);
		}

		(*idx) += 1;
		(*idx) %= numToPlace;

	} while ((*idx) != start_idx);

	return qfalse;
}

/**
 * @brief Number of test alternatives per step in SV_AddMissingTiles
 * @sa SV_AddMissingTiles
 */
#define CHECK_ALTERNATIVES_COUNT 10

/**
 * @brief Tries to fill the missing tiles of the current map.
 * @return @c false if the tiles does not fit, @c true if the map could be filled.
 * @sa SV_FitTile
 * @sa SV_AddTile
 */
static qboolean SV_AddMissingTiles (mapInfo_t *map)
{
	int i;
	int idx[CHECK_ALTERNATIVES_COUNT];
	int pos[CHECK_ALTERNATIVES_COUNT];
	int rating[CHECK_ALTERNATIVES_COUNT];
	mapInfo_t backup;
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];
	const int mapW = mAsm->width;
	const int mapH = mAsm->height;
	const mToPlace_t *mToPlace = map->mToPlace;

	memcpy(&backup, map, sizeof(*map));
	while (1) {
		int max_rating = -mapW * mapH * 4;

		/* check if the map is already filled */
		if (SV_TestFilled(map))
			return qtrue;

		/* try some random tiles at random positions */
		for (i = 0; i < CHECK_ALTERNATIVES_COUNT; i++) {
			int x, y;
			if (!SV_PickRandomTile(map, &idx[i], &pos[i])) {
				/* remove all tiles placed by this function */
				memcpy(map, &backup, sizeof(*map));
				return qfalse;
			}

			x = pos[i] % mapW;
			y = pos[i] / mapW;
			SV_AddTile(map, mToPlace[idx[i]].tile, x, y, idx[i], pos[i]);

			if (SV_TestFilled(map))
				return qtrue;

			rating[i] = SV_CalcRating(map);

			if (rating[i] > max_rating)
				max_rating = rating[i];

			SV_RemoveTile(map, NULL, NULL);
		}

		for (i = 0; i < CHECK_ALTERNATIVES_COUNT; i++) {
			if (rating[i] == max_rating) {
				const int x = pos[i] % mapW;
				const int y = pos[i] / mapW;
				SV_AddTile(map, mToPlace[idx[i]].tile, x, y, idx[i], pos[i]);
				break;
			}
		}
	}
}

/**
 * @brief Tries to build the map
 * @sa SV_FitTile
 * @sa SV_AddTile
 */
static void SV_AddMapTiles (mapInfo_t *map)
{
	int idx, pos;
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];
	const int mapW = mAsm->width;
	const int mapSize = mAsm->size;
	const int numToPlace = map->numToPlace;
	const mToPlace_t *mToPlace = map->mToPlace;
	short prList[MAX_RANDOM_MAP_HEIGHT * MAX_RANDOM_MAP_WIDTH];
	const int start = map->numPlaced;
#ifdef DEBUG
	const mPlaced_t *mPlaced = map->mPlaced;
#endif

	/* shuffle only once, the map will be build with that seed */
	RandomList(mapSize, prList);

	pos = 0;
	idx = 0;
	while (idx < numToPlace) {
		while (mToPlace[idx].cnt < mToPlace[idx].min) {
			for (; pos < mapSize; pos++) {
				const int x = prList[pos] % mapW;
				const int y = prList[pos] / mapW;
				if (sv_threads->integer) {
					if (SDL_SemValue(mapSem) != 1) {
						/* someone else beat me to it */
						return;
					}
				}

				if ((x % mAsm->dx != 0) || (y % mAsm->dy != 0))
					continue;

				if (SV_FitTile(map, mToPlace[idx].tile, x, y)) {
					/* add tile */
					SV_AddTile(map, mToPlace[idx].tile, x, y, idx, pos);
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
			SV_RemoveTile(map, &idx, &pos);
			pos++;
		}

		/* tile fits, try next tile */
		if (pos < mapSize) {
			pos = 0;
			idx++;
		} else {
			/* no more retries */
			if (start == map->numPlaced) {
				Com_Error(ERR_DROP, "SV_AddMapTiles: Impossible to assemble map '%s' with assembly '%s'\n",
						map->name, mAsm->id ? mAsm->id : "");
			}
			SV_RemoveTile(map, &idx, &pos);
			pos++;
		}

		if ((idx == numToPlace) && !SV_AddMissingTiles(map)) {
			SV_RemoveTile(map, &idx, &pos);
			pos++;
		}
	}
}

/**
 * @brief Prepare the list of tiles to place
 * @sa SV_AssembleMap
 * @sa SV_AddTile
 */
static void SV_PrepareTilesToPlace (mapInfo_t *map)
{
	int i;
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];

	map->numToPlace = 0;
	memset(&map->mToPlace[0], 0, sizeof(map->mToPlace));

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

	srand(time(NULL));

	SV_AddMapTiles(map);

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
		threads[i] = SDL_CreateThread(&SV_AssemblyThread, (void*) maps[i]);
	}
	while (threadID == 0) {
		/* if nobody is done after 5 sec, restart, double the timeout. */
		if (SDL_CondWaitTimeout(mapCond, mapLock->mutex, timeout) != 0) {
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
				threads[i] = SDL_CreateThread(&SV_AssemblyThread, (void*) maps[i]);
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
static void SV_ParseUMP (const char *name, mapInfo_t *map, qboolean inherit)
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

		if (!strcmp(token, "extends")) {
			token = Com_Parse(&text);
			SV_ParseUMP(token, map, qtrue);
		} else if (!strcmp(token, "base")) {
			token = Com_Parse(&text);
			if (inherit)
				Q_strncpyz(map->inheritBasePath, token, sizeof(map->inheritBasePath));
			else
				Q_strncpyz(map->basePath, token, sizeof(map->basePath));
		} else if (!strcmp(token, "tileset")) {
			if (map->numTileSets >= MAX_TILESETS)
				Com_Printf("SV_ParseUMP: Too many map tileset found in (%s)\n", filename);
			else if (SV_ParseMapTileSet(filename, &text, map, inherit))
				map->numTileSets++;
		} else if (!strcmp(token, "tile")) {
			if (map->numTiles >= MAX_TILETYPES)
				Com_Printf("SV_ParseUMP: Too many map tile types (%s)\n", filename);
			else if (SV_ParseMapTile(filename, &text, map, inherit))
				map->numTiles++;
		} else if (!strcmp(token, "assembly")) {
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

/**
 * @brief Assembles a "random" map
 * parses the *.ump files for assembling the "random" maps
 * @param[in] name The name of the map (ump) file to parse
 * @param[in] assembly The random map assembly that should be used from the given rma
 * @param[out] asmMap The output string of the random map assembly that contains all the
 * map tiles that should be assembled. The order is the same as in the @c asmPos string.
 * Each of the map tiles in this string has a corresponding entry in the pos string, too.
 * @param[out] asmPos The pos string for the assembly. For each tile from the @c asmMap
 * string this string contains three coordinates for shifting the given tile names.
 * @sa B_AssembleMap_f
 * @sa SV_AddTile
 * @sa SV_AddMandatoryParts
 * @sa SV_ParseAssembly
 * @sa SV_ParseMapTile
 * @note Make sure to free the returned pointer
 */
mapInfo_t* SV_AssembleMap (const char *name, const char *assembly, char *asmMap, char *asmPos)
{
	int i;
	mAssembly_t *mAsm;
	mapInfo_t *map;

	map = Mem_AllocType(mapInfo_t);
	Q_strncpyz(map->name, name, sizeof(map->name));

	SV_ParseUMP(name, map, qfalse);

	/* check for parsed tiles and assemblies */
	if (!map->numTiles)
		Com_Error(ERR_DROP, "No map tiles defined (%s)!", name);
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_SERVER, "numTiles: %i\n", map->numTiles);
#endif

	if (!map->numAssemblies)
		Com_Error(ERR_DROP, "No map assemblies defined (%s)!", name);
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_SERVER, "numAssemblies: %i\n", map->numAssemblies);
#endif

	/* use random assembly, if no valid one has been specified */
	map->mAsm = rand() % map->numAssemblies;

	/* overwrite with specified, if any */
	if (assembly && assembly[0]) {
		for (i = 0; i < map->numAssemblies; i++)
			if (!strcmp(assembly, map->mAssembly[i].id)) {
				map->mAsm = i;
				break;
			}
		if (i == map->numAssemblies) {
			Com_Printf("SV_AssembleMap: Map assembly '%s' not found\n", assembly);
		}
	}

	mAsm = &map->mAssembly[map->mAsm];
	assert(mAsm);

	Com_DPrintf(DEBUG_SERVER, "Use assembly: '%s'\n", mAsm->id);

	/* check size */
	assert(mAsm->width <= MAX_RANDOM_MAP_WIDTH);
	assert(mAsm->height <= MAX_RANDOM_MAP_HEIGHT);

	SV_PrepareTilesToPlace(map);

	/* assemble the map */
	map->numPlaced = 0;
	SV_ClearMap(map);

	/* place fixed parts - defined in ump via fix parameter */
	for (i = 0; i < mAsm->numFixed; i++)
		SV_AddTile(map, &map->mTile[mAsm->fT[i]], mAsm->fX[i], mAsm->fY[i], -1, -1);

	if (sv_threads->integer) {
		if (SV_ParallelSearch(map) < 0) {
			Mem_Free(map);
			return NULL;
		}
	} else {
		SV_AddMapTiles(map);
	}

	/* prepare map and pos strings */
	if (map->basePath[0])
		Com_sprintf(asmMap, sizeof(map->basePath) + 1, "-%s", map->basePath);

	asmPos[0] = 0;

	/* generate the strings */
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

	Com_DPrintf(DEBUG_SERVER, "tiles: %s\n", asmMap);
	Com_DPrintf(DEBUG_SERVER, "pos: %s\n", asmPos);
	Com_DPrintf(DEBUG_SERVER, "tiles: %i\n", map->numPlaced);

	assert(map);
	return map;
}
