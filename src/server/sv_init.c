/**
 * @file sv_init.c
 * @brief Server initialisation stuff.
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../shared/parse.h"

server_static_t svs;			/* persistant server info */
server_t sv;					/* local server */

static int SV_FindIndex (const char *name, int start, int max, qboolean create)
{
	int i;

	if (!name || !name[0])
		return 0;

	for (i = 1; i < max && sv.configstrings[start + i][0]; i++)
		if (!strcmp(sv.configstrings[start + i], name))
			return i;

	if (!create)
		return 0;

	if (i == max)
		Com_Error(ERR_DROP, "*Index: overflow '%s' start: %i, max: %i", name, start, max);

	Q_strncpyz(sv.configstrings[start + i], name, sizeof(sv.configstrings[i]));

	if (Com_ServerState() != ss_loading) {	/* send the update to everyone */
		struct dbuffer *msg = new_dbuffer();
		NET_WriteByte(msg, svc_configstring);
		NET_WriteShort(msg, start + i);
		NET_WriteString(msg, name);
		SV_Multicast(~0, msg);
	}

	return i;
}


int SV_ModelIndex (const char *name)
{
	return SV_FindIndex(name, CS_MODELS, MAX_MODELS, qtrue);
}

/*
 * ================
 * MAP ASSEMBLY
 * ================
 * More info on map-assembly can be found at:
 * http://ufoai.ninex.info/wiki/index.php/Mapping/Random_map_assembly
 */
#define MAX_MAPASSEMBLIES 32
#define MAX_TILETYPES 64
#define MAX_TILESIZE 16
#define MAX_FIXEDTILES 32

#define MAX_RANDOM_MAP_WIDTH 32
#define MAX_RANDOM_MAP_HEIGHT 32

/** @brief Stores the parsed data for a map tile. (See *.ump files) */
typedef struct mTile_s {
	char id[MAX_VAR];	/**< The id (string) of the tile as defined in the ump file (next to "tile"). */
	uLong spec[MAX_TILESIZE][MAX_TILESIZE];	/**< connection/alternatives info for the tile  */
	int w, h;		/**< The width and height of the tile. */
	int area;	/**< Number of solid parts */
	struct mTile_s *duplicate;	/**< Pointer to next duplicate **/
} mTile_t;

/**
 * @brief Stores the parsed data of an assembly definition.
 * See *.ump files
 */
typedef struct mAssembly_s {
	char id[MAX_VAR];		/**< The id (string) of the assembly as defined in the ump file (next to "assembly"). */
	char title[MAX_VAR];		/**< The full title for this assembly. This string should be marked as translatable (_). */
	byte min[MAX_TILETYPES];	/**< Minimum tile number for each used tile-type. */
	byte max[MAX_TILETYPES];	/**< Maximum tile number for each used tile-type. */
	byte fT[MAX_FIXEDTILES];	/**< Index of used (fix) tile in mTile[] array */
	byte fX[MAX_FIXEDTILES];	/**< x position of the used  (fix) tile in fT */
	byte fY[MAX_FIXEDTILES];	/**< y position of the used  (fix) tile in fT */
	int numFixed;	/**< Number of fixed tiles. Counts entries of fX, fY and fT */
	int width, height;	/**< The width and height of the assembly. (size "<w> <h>"). This is the maximum size of
						 * whole map that is built from all the tiles in the assembly. So if there is a single tile
						 * that is used in the assembly and that has a greater size than the one given in the assembly
						 * then this must fail. */
	int size;	/**< the result of (width * height), memoized */
	int dx, dy;	/**< The grid steps of the assembly. (grid "<dx> <dx>")
				 * This can speed up the assembly of a map if you e.g. only have tiles of the size 2x2 you
				 * can greatly improve the performance if you also set the grid parameter to "2 2" - this will
				 * only check every 2 random map units for a suitable maptile. */
} mAssembly_t;

/**
 * @brief Defines a tile to place
 * @sa mTile_t
 */
typedef struct mToPlace_s {
	mTile_t *tile;	/**< The tile to place. */
	int min, max;	/**< Minimum and maximum count of placements. */
	int cnt;	/**< Current count of placements */
} mToPlace_t;

/**
 * @brief Defines a placed tile
 * @sa mTile_t
 */
typedef struct mPlaced_s {
	const mTile_t *tile;	/**< The tile that was/is placed. */
	int x, y;	/**< The position in the map the tile was/is placed in. */
	int idx, pos;	/**< Stores the state of the placement algorithm */
} mPlaced_t;

typedef struct mapInfo_s {
	char name[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
	/** @brief Stores the alternatives information for the assembled map */
	uLong curMap[MAX_RANDOM_MAP_HEIGHT][MAX_RANDOM_MAP_WIDTH];

	/** @brief Stores the map rating for the assembled map */
	char curRating[MAX_RANDOM_MAP_HEIGHT][MAX_RANDOM_MAP_WIDTH];

	/** @brief Stores the Tiles to Place in the map */
	mToPlace_t mToPlace[MAX_TILETYPES];
	int numToPlace;	/**< the size of the to place list */

	mAssembly_t mAssembly[MAX_MAPASSEMBLIES];		 /**< A list of parsed assembly definitions. */
	int numAssemblies;	 /**< The number of assemblies in mAssembly. */

	mPlaced_t mPlaced[MAX_MAPTILES];	 /**< Holds all tiles that have been placed on the current map. */
	int numPlaced;				/**< The number of tiles in mPlaced. */

	mTile_t mTile[MAX_TILETYPES];			 /**< A list of parsed map-tiles. */
	int numTiles; /**< Length of the mTile list */

	int mAsm;	/**< the index of the selected assembly */
} mapInfo_t;

/*
static mapInfo_t *map = NULL;
*/
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
static uLong tileMask (const char chr)
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

/**
 * @brief Parsed a tile definition out of the ump-files
 * @sa SV_ParseAssembly
 * @sa SV_AssembleMap
 * @note Parsed data are stored into *target, which must already be allocated.
 */
static int SV_ParseMapTile (const char *filename, const char **text, mTile_t *target)
{
	const char *errhead = "SV_ParseMapTile: Unexpected end of file (";
	const char *token;
	char *chr;
	int x, y, i;

	/* get tile name */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return 0;
	memset(target, 0, sizeof(*target));
	Q_strncpyz(target->id, token, sizeof(target->id));

	/* start parsing the block */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return 0;
	if (*token != '{') {
		Com_Printf("SV_ParseMapTile: Expected '{' for tile '%s' (%s)\n", target->id, filename);
		return 0;
	}

	/* get width and height */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return 0;
	target->w = atoi(token);

	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return 0;
	target->h = atoi(token);

	if (target->w > MAX_TILESIZE || target->h > MAX_TILESIZE) {
		Com_Printf("SV_ParseMapTile: Bad tile size [%i %i] (%s) (max. [%i %i])\n", target->w, target->h, filename, MAX_TILESIZE, MAX_TILESIZE);
		*text = strchr(*text, '}');
		return 0;
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
			target->spec[y][x] = 0L;
			for (i = 0; token[i]; i++, chr++) {
				target->spec[y][x] |= tileMask(token[i]);
			}
		}

	token = Com_EParse(text, errhead, filename);

	/* get connections */
	if (*token != '}')
		Com_Printf("SV_ParseMapTile: Bad tile desc in '%s' - too many entries for size\n", target->id);

	/* successfully parsed - this tile counts */
	return 1;
}


/**
 * @brief Parses an assembly block
 * @sa SV_AssembleMap
 * @sa SV_ParseMapTile
 * @note: format of size: "size x y"
 * @note: format of fix: "fix [tilename] x y"
 * @note: format of tile: "[tilename] min max"
 * @param a Pointer to the assembly to be initialized, must be allocated.
 * @return 1 if it was parsed, 0 if not.
 */
static int SV_ParseAssembly (mapInfo_t *map, const char *filename, const char **text, mAssembly_t *a)
{
	const char *errhead = "SV_ParseAssembly: Unexpected end of file (";
	const char *token, *cvarValue;
	char cvarName[MAX_VAR];
	int i, x, y;

	/* get assembly name */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return 0;

	/* init */
	memset(a, 0, sizeof(*a));
	Q_strncpyz(a->id, token, sizeof(a->id));
	a->width = 8;
	a->height = 8;
	a->dx = 1;
	a->dy = 1;

	token = Com_EParse(text, errhead, filename);
	if (!*text || *token != '{') {
		Com_Error(ERR_DROP, "Invalid assembly definition '%s' - invalid token '%s'", a->id, token);
		return 0;
	}

	do {
		/* get tile name */
		token = Com_EParse(text, errhead, filename);
		if (!text || *token == '}')
			break;

		if (!strncmp(token, "title", 5)) {
			/* get map title */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			Q_strncpyz(a->title, token, sizeof(a->title));
			continue;
			/* fix tilename x y */
		} else if (!strncmp(token, "size", 4)) {
			/* get map size */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			sscanf(token, "%i %i", &a->width, &a->height);
			a->size = a->width * a->height;
			continue;
		} else if (!strncmp(token, "grid", 4)) {
			/* get map size */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			sscanf(token, "%i %i", &a->dx, &a->dy);
			continue;
		/* fix tilename x y */
		} else if (!strncmp(token, "fix", 3)) {
			/* get tile */
			token = Com_EParse(text, errhead, filename);
			if (!text)
				break;

			for (i = 0; i < map->numTiles; i++)
				if (!strcmp(token, map->mTile[i].id)) {
					if (a->numFixed >= MAX_FIXEDTILES) {
						Com_Printf("SV_ParseAssembly: Too many fixed tiles in assembly '%s'\n", a->id);
						break;
					}
					/* get coordinates */
					token = Com_EParse(text, errhead, filename);
					if (!text)
						break;

					sscanf(token, "%i %i", &x, &y);
					a->fX[a->numFixed] = x;
					a->fY[a->numFixed] = y;
					a->fT[a->numFixed] = i;
					a->numFixed++;
				}
			continue;
		/* <format>*cvarname <defaultvalue> "min max"</format> */
		} else if (*token == '*') {
			/* '*' is: replace by cvar value */
			token++; /* strip '*' */
			Com_DPrintf(DEBUG_SERVER, "SV_ParseAssembly: cvar replacement: %s\n", token);
			Q_strncpyz(cvarName, token, sizeof(cvarName));
			token = Com_EParse(text, errhead, filename);
			if (!text || *token == '}')
				break;
			cvarValue = Cvar_GetString(cvarName);
			Com_DPrintf(DEBUG_SERVER, "SV_ParseAssembly: cvar replacement value: %s\n", cvarValue);
			if (*cvarValue != '+') {
				Com_Printf("SV_ParseAssembly: warning - cvar '%s' value doesn't seam to be a valid tile id '%s' - set to default '%s'\n", cvarName, cvarValue, token);
				Cvar_Set(cvarName, token);
				if (*token != '+')
					Com_Error(ERR_DROP, "SV_ParseAssembly: wrong tile id in assembly '%s'", a->id);
			} else
				token = cvarValue;
		}

		for (i = 0; i < map->numTiles; i++)
			if (!strcmp(token, map->mTile[i].id)) {
				/* get min and max tile number */
				token = Com_EParse(text, errhead, filename);
				if (!text || *token == '}')
					break;

				if (!strstr(token, " ")) {
					Com_Error(ERR_DROP, "SV_ParseAssembly: Error in assembly %s (min max value of tile %s)", filename, map->mTile[i].id);
					return 0;
				}
				sscanf(token, "%i %i", &x, &y);
				a->min[i] = x;
				a->max[i] = y;
				break;
			}
		if (i == map->numTiles)
			Com_Error(ERR_DROP, "Could not find tile: '%s' in assembly '%s' (%s)", token, a->id, filename);
	} while (text);
	return 1;
}


/**
 * @brief Combines the alternatives/connection info of a map with a tile and sets the rating
 * @param[in] mapAlts Pointer to the alternatives info field of the map which will be updated.
 * @param[in] tileAlts Pointer to the alternatives info field of the tile.
 * @param[in] mapRating Pointer to the rating field of the map.
 * @sa SV_AssembleMap
 * @sa SV_AddRegion
 * @sa SV_FitTile
 */
static void SV_CombineAlternatives (uLong *mapAlts, uLong tileAlts, char *mapRating)
{
	/* don't touch solid fields of the map, return if tile has no connection info */
	if (IS_SOLID(*mapAlts) || (tileAlts == ALL_TILES))
		return;

	/* for an empty map tile must the rating be zero */
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
	uLong *mp = &map->curMap[0][0];
	uLong *end = &map->curMap[MAX_RANDOM_MAP_HEIGHT - 1][MAX_RANDOM_MAP_WIDTH - 1];

	memset(map->curRating, 0, sizeof(map->curRating));

	while (mp <= end)
		*(mp++) = ALL_TILES;
}

/**
 * @brief Checks if a given map-tile fits into the empty space (in a given location) of a map.
 * @param[in] tile The tile definition that should be fitted into the map.
 * @param[in] x The x position in the map where the tile is supposed to be placed/checked.
 * @param[in] y The y position in the map where the tile is supposed to be placed/checked.
 * @return qtrue if the tile fits.
 * @return qfalse if the tile does not fit or an error was encountered.
 * @sa SV_AddMandatoryParts
 * @sa SV_AddRegion
 */
static qboolean SV_FitTile (const mapInfo_t *map, mTile_t * tile, int x, int y)
{
	int tx, ty;
	const uLong *spec = NULL;
	const uLong *m = NULL;
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
			const uLong combined = (*m) & (*spec);

			/* quit if both are solid or no equal connection is found*/
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
	const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];

	/* check vor valid grid positions */
	assert(x % mAsm->dx == 0);
	assert(y % mAsm->dy == 0);

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
static qboolean SV_AddRandomTile (mapInfo_t *map, int* idx, int* pos)
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
					&& SV_FitTile(map, mToPlace[*idx].tile, x, y))
				{
					SV_AddTile(map, mToPlace[*idx].tile, x, y, *idx, *pos);
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
 * @return qtrue if the map could be filled.
 * @return qfalse if the the tiles does not fit.
 * @sa SV_FitTile
 * @sa SV_AddTile
 */
static qboolean SV_AddMissingTiles (mapInfo_t *map)
{
	int i;
	int idx[CHECK_ALTERNATIVES_COUNT];
	int pos[CHECK_ALTERNATIVES_COUNT];
	int rating[CHECK_ALTERNATIVES_COUNT];
	const int startPlaced = map->numPlaced;
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
			if (!SV_AddRandomTile(map, &idx[i], &pos[i])) {
				/* remove all tiles placed by this function */
				while (map->numPlaced > startPlaced)
					memcpy(map, &backup, sizeof(*map));
					//SV_RemoveTile(map, NULL, NULL);

				return qfalse;
			}

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
	const mPlaced_t *mPlaced = map->mPlaced;
	short prList[MAX_RANDOM_MAP_HEIGHT * MAX_RANDOM_MAP_WIDTH];
	const int start = map->numPlaced;

	/* shuffle only once, the map will be build with that seed */
	RandomList(mapSize, prList);

	pos = 0;
	idx = 0;
	while (idx < numToPlace) {
		while (mToPlace[idx].cnt < mToPlace[idx].min) {
			for (; pos < mapSize; pos++) {
				const int x = prList[pos] % mapW;
				const int y = prList[pos] / mapW;

				if ((x % mAsm->dx != 0) || (y % mAsm->dy != 0))
					continue;

				if (SV_FitTile(map, mToPlace[idx].tile, x, y)) {
					/* add tile */
					SV_AddTile(map, mToPlace[idx].tile, x, y, idx, pos);
					break;
				}
			}
			/* tile fits, try next tile */
			if (pos < mapSize)
				continue;

			/* tile doesn't fit and no try left with this tile */
			if (!mToPlace[idx].cnt)
				break;

			/* tile does not fit, restore last status - replace the last tile */
			assert(map->numPlaced > 0);
			assert(idx == mPlaced[map->numPlaced - 1].idx);
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
 * @brief Assembles a "random" map
 * parses the *.ump files for assembling the "random" maps
 * @sa B_AssembleMap_f
 * @sa SV_AddTile
 * @sa SV_AddMandatoryParts
 * @sa SV_ParseAssembly
 * @sa SV_ParseMapTile
 */
static mapInfo_t* SV_AssembleMap (const char *name, const char *assembly, char *asmMap, char *asmPos)
{
	char filename[MAX_QPATH];
	char basePath[MAX_QPATH];
	byte *buf;
	const char *text, *token;
	int i;
	mapInfo_t *map;
	mAssembly_t *mAsm;

	/* load the map info */
	Com_sprintf(filename, sizeof(filename), "maps/%s.ump", name);
	FS_LoadFile(filename, &buf);
	if (!buf)
		Com_Error(ERR_DROP, "SV_AssembleMap: Map assembly info '%s' not found", filename);

	map = Mem_Alloc(sizeof(*map));
	Q_strncpyz(map->name, name, sizeof(map->name));

	/* parse it */
	text = (const char*)buf;
	basePath[0] = 0;
	do {
		token = Com_Parse(&text);
		if (!text)
			break;

		if (!strcmp(token, "base")) {
			token = Com_Parse(&text);
			Q_strncpyz(basePath, token, sizeof(basePath));
		} else if (!strcmp(token, "tile")) {
			if (map->numTiles >= MAX_TILETYPES) {
				Com_Printf("SV_ParseMapTile: Too many map tile types (%s)\n", filename);
			} else {
				if (SV_ParseMapTile(filename, &text, &map->mTile[map->numTiles]))
					map->numTiles++;
			}
		}
		else if (!strcmp(token, "assembly")) {
			if (map->numAssemblies >= MAX_MAPASSEMBLIES) {
				Com_Printf("SV_ParseAssembly: Too many map assemblies (%s)\n", filename);
				Mem_Free(map);
				return NULL;
			}
			if (SV_ParseAssembly(map, filename, &text, &map->mAssembly[map->numAssemblies]))
				map->numAssemblies++;
		}
		else if (!strcmp(token, "{")) {
			Com_Printf("SV_AssembleMap: Skipping unknown block\n");
			/* ignore unknown block */
			text = strchr(text, '}') + 1;
			if (!text)
				break;
		} else
			Com_Printf("SV_AssembleMap: Unknown token '%s' (%s)\n", token, filename);
	} while (text);

	/* free the file */
	FS_FreeFile(buf);

	/* check for parsed tiles and assemblies */
	if (!map->numTiles)
		Com_Error(ERR_DROP, "No map tiles defined (%s)!", filename);
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_SERVER, "numTiles: %i\n", map->numTiles);
#endif

	if (!map->numAssemblies)
		Com_Error(ERR_DROP, "No map assemblies defined (%s)!", filename);
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

	SV_AddMapTiles(map);

	/* prepare map and pos strings */
	if (basePath[0]) {
		asmMap[0] = '-';
		Q_strncpyz(&asmMap[1], basePath, MAX_QPATH - 1);
	}
	asmPos[0] = 0;

	/* generate the strings */
	for (i = 0; i < map->numPlaced; i++) {
		const mPlaced_t *pl = &map->mPlaced[i];

		if (sv_dumpmapassembly->integer)
			SV_DumpPlaced(map, i);

		if(asmMap[0])
			Q_strcat(asmMap, " ", MAX_TOKEN_CHARS * MAX_TILESTRINGS);
		if(asmPos[0])
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

/**
 * @brief Get the map title for a given map
 * @note the title string must be translated client side
 * @return Never NULL - mapname or maptitle (if defined in assembly)
 */
static const char* SV_GetMapTitle (const mapInfo_t *map, const char* const mapname)
{
	assert(mapname);

	if (mapname[0] == '+') {
		const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];
		if (mAsm && mAsm->title[0]) {
			/* return the assembly title itself - must be translated client side */
			if (mAsm->title[0] == '_')
				return mAsm->title + 1;
			else {
				Com_Printf("The assembly title '%s' is not marked as translatable\n", mAsm->title);
				return mAsm->title;
			}
		}
	}
	return mapname;
}

/**
 * @brief Change the server to a new map, taking all connected clients along with it.
 * @sa SV_AssembleMap
 * @sa CM_LoadMap
 * @sa Com_SetServerState
 */
static void SV_SpawnServer (qboolean day, const char *server, const char *param)
{
	int i;
	unsigned checksum = 0;
	const char *buf = NULL;
	char * map = sv.configstrings[CS_TILES];
	char * pos = sv.configstrings[CS_POSITIONS];
	mapInfo_t *randomMap = NULL;

	assert(server);
	assert(server[0] != '\0');

	Com_DPrintf(DEBUG_SERVER, "SpawnServer: %s\n", server);

	svs.spawncount++;

	/* wipe the entire per-level structure */
	memset(&sv, 0, sizeof(sv));
	svs.realtime = 0;

	/* save name for levels that don't set message */
	Q_strncpyz(sv.configstrings[CS_NAME], server, MAX_TOKEN_CHARS);
	Com_sprintf(sv.configstrings[CS_LIGHTMAP], MAX_TOKEN_CHARS, "%i", day);
	sv.day = day;

	Q_strncpyz(sv.name, server, sizeof(sv.name));
	if (param)
		Q_strncpyz(sv.assembly, param, sizeof(sv.assembly));
	else
		*sv.assembly = '\0';

	/* leave slots at start for clients only */
	for (i = 0; i < sv_maxclients->integer; i++) {
		/* needs to reconnect */
		if (svs.clients[i].state > cs_connected)
			SV_SetClientState(&svs.clients[i], cs_connected);
	}

	/* assemble and load the map */
	if (server[0] == '+') {
		randomMap = SV_AssembleMap(server + 1, param, map, pos);
		if (!randomMap) {
			Com_Printf("Could not load assembly for map '%s'\n", server);
			return;
		}
	} else {
		Q_strncpyz(sv.configstrings[CS_TILES], server, MAX_TOKEN_CHARS * MAX_TILESTRINGS);
		if (param)
			Q_strncpyz(sv.configstrings[CS_POSITIONS], param, MAX_TOKEN_CHARS * MAX_TILESTRINGS);
		else
			sv.configstrings[CS_POSITIONS][0] = 0;
	}

	CM_LoadMap(map, day, pos, &checksum);

	Com_Printf("checksum for the map '%s': %u\n", server, checksum);
	Com_sprintf(sv.configstrings[CS_MAPCHECKSUM], sizeof(sv.configstrings[CS_MAPCHECKSUM]), "%i", checksum);

	checksum = 0;
	while ((buf = FS_GetFileData("ufos/*.ufo")) != NULL)
		checksum += LittleLong(Com_BlockChecksum(buf, strlen(buf)));
	FS_GetFileData(NULL);
	Com_Printf("ufo script checksum %u\n", checksum);
	Com_sprintf(sv.configstrings[CS_UFOCHECKSUM], sizeof(sv.configstrings[CS_UFOCHECKSUM]), "%i", checksum);
	Com_sprintf(sv.configstrings[CS_OBJECTAMOUNT], sizeof(sv.configstrings[CS_OBJECTAMOUNT]), "%i", csi.numODs);

	Com_sprintf(sv.configstrings[CS_VERSION], sizeof(sv.configstrings[CS_VERSION]), "%s", UFO_VERSION);

	Com_sprintf(sv.configstrings[CS_MAPTITLE], sizeof(sv.configstrings[CS_MAPTITLE]), "%s", SV_GetMapTitle(randomMap, server));
	if (!strncmp(sv.configstrings[CS_MAPTITLE], "b/", 2)) {
		/* For base attack, cl.configstrings[CS_MAPTITLE] contains too many chars */
		Com_sprintf(sv.configstrings[CS_MAPTITLE], sizeof(sv.configstrings[CS_MAPTITLE]), "Base attack");
	}

	/* clear random-map assembly data */
	Mem_Free(randomMap);
	randomMap = NULL;

	/* clear physics interaction links */
	SV_ClearWorld();

	/* fix this! */
	for (i = 1; i <= CM_NumInlineModels(); i++)
		sv.models[i] = CM_InlineModel(va("*%i", i));

	/* precache and static commands can be issued during map initialization */
	Com_SetServerState(ss_loading);

	/* load and spawn all other entities */
	ge->SpawnEntities(sv.name, CM_EntityString());

	/* all precaches are complete */
	Com_SetServerState(ss_game);

	/* set serverinfo variable */
	sv_mapname = Cvar_FullSet("sv_mapname", sv.name, CVAR_SERVERINFO | CVAR_NOSET);

	Com_Printf("-------------------------------------\n");
}

/**
 * @sa CL_ServerListDiscoveryCallback
 */
static void SV_DiscoveryCallback (struct datagram_socket *s, const char *buf, int len, struct sockaddr *from)
{
	const char match[] = "discover";
	if (len == sizeof(match) && memcmp(buf, match, len) == 0) {
		const char msg[] = "discovered";
		NET_DatagramSend(s, msg, sizeof(msg), from);
	}
}

/**
 * @brief A brand new game has been started
 */
static void SV_InitGame (void)
{
	if (svs.initialized)
		/* cause any connected clients to reconnect */
		SV_Shutdown("Server restarted.", qtrue);

	/* allow next change after map change or restart */
	sv_maxclients->flags |= CVAR_LATCH;

	/* get any latched variable changes (sv_maxclients, etc) */
	Cvar_UpdateLatchedVars();

	svs.spawncount = rand();
	svs.clients = Mem_PoolAlloc(sizeof(client_t) * sv_maxclients->integer, sv_genericPool, 0);

	/* init network stuff */
	if (sv_maxclients->integer > 1) {
		svs.initialized = SV_Start(NULL, port->string, &SV_ReadPacket);
		svs.netDatagramSocket = NET_DatagramSocketNew(NULL, Cvar_Get("port", DOUBLEQUOTE(PORT_SERVER), CVAR_NOSET, NULL)->string, &SV_DiscoveryCallback);
	} else
		svs.initialized = SV_Start(NULL, NULL, &SV_ReadPacket);

	SV_Heartbeat_f();

	/* init game */
	SV_InitGameProgs();

	if (sv_maxclients->integer != 1 && (sv_dedicated->integer || sv_public->integer))
		SV_SetMaster_f();
}


/**
 * @brief Loads the map
 * @note the full syntax is:
 * @code map [day|night] [+]<map> [<assembly>] @endcode
 * @sa SV_SpawnServer
 * @sa SV_Map_f
 * @param[in] day Use the day version of the map (only lightmap)
 */
void SV_Map (qboolean day, const char *levelstring, const char *assembly)
{
	assert(levelstring[0] != '\0');

	/* any partially connected client will be restarted */
	Com_SetServerState(ss_dead);

	/* the game is just starting */
	SV_InitGame();

	if (!svs.initialized) {
		Com_Printf("Could not spawn the map\n");
		return;
	}

	SCR_BeginLoadingPlaque();
	SV_SpawnServer(day, levelstring, assembly);
	Cbuf_CopyToDefer();
}
