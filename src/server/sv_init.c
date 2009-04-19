/**
 * @file sv_init.c
 * @brief Server initialisation stuff.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

/** @brief Stores the alternatives information for the assembled map */
static uLong curMap[MAX_RANDOM_MAP_HEIGHT][MAX_RANDOM_MAP_WIDTH];

/** @brief Stores the map rating for the assembled map */
static char curRating[MAX_RANDOM_MAP_HEIGHT][MAX_RANDOM_MAP_WIDTH];

/** @brief Stores the parsed data for a map tile. (See *.ump files) */
typedef struct mTile_s {
	char id[MAX_VAR];	/**< The id (string) of the tile as defined in the ump file (next to "tile"). */
	uLong spec[MAX_TILESIZE][MAX_TILESIZE];	/**< connection/alternatives info for the tile  */
	int w, h;		/**< The width and height of the tile. */
	int area;	/**< Number of solid parts */
	struct mTile_s *duplicate;	/**< Pointer to next duplicate **/
} mTile_t;

/**
 * @brief Stores the parsed data of an assembly definition.  (See *.ump files)
 * @todo Please have a look if the comments are correct.
 */
typedef struct mAssembly_s {
	char id[MAX_VAR];		/**< The id (string) of the assembly as defined in the ump file (next to "assembly"). */
	char title[MAX_VAR];		/**< The full title fo this assembly. */
	byte min[MAX_TILETYPES];	/**< Minimum tile number for each used tile-type. */
	byte max[MAX_TILETYPES];	/**< Maximum tile number for each used tile-type. */
	byte fT[MAX_FIXEDTILES];	/**< Index of used (fix) tile in mTile[] array */
	byte fX[MAX_FIXEDTILES];	/**< x position of the used  (fix) tile in fT */
	byte fY[MAX_FIXEDTILES];	/**< y position of the used  (fix) tile in fT */
	int numFixed;	/**< Number of fixed tiles. Counts entries of fX, fY and fT */
	int w, h;	/**< The width and height of the assembly. (size "<w> <h>") */
	int dx, dy;	/**< The grid steps of the assembly. (grid "<dx> <dx>") */
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

/** @brief Stores the Tiles to Place in the map */
static mToPlace_t mToPlace[MAX_TILETYPES];

static int numToPlace;	/**< the size of the to place list */

/**
 * @brief Defines a placed tile
 * @sa mTile_t
 */
typedef struct mPlaced_s {
	const mTile_t *tile;	/**< The tile that was/is placed. */
	int x, y;	/**< The position in the map the tile was/is placed in. */
	int idx, pos;	/**< Stores the state of the placement algorithm */
} mPlaced_t;

static mTile_t mTile[MAX_TILETYPES];					 /**< A list of parsed map-tiles. */
static mAssembly_t mAssembly[MAX_MAPASSEMBLIES];		 /**< A list of parsed assembly definitions. */

static int numAssemblies;	 /**< The number of assemblies in mAssembly. */

static mPlaced_t mPlaced[MAX_MAPTILES];	 /**< @todo Holds all tiles that have been placed on the current map. */
static int numPlaced;				/**< The number of tiles in mPlaced. */

static short prList[32 * 32];			/**< used to shuffle the map positions for assembly */

static mAssembly_t *mAsm;	/**< the selected assembly */
static int mapSize;		/**< the size of the current map */
static int mapW, mapH;		/**< the width and heigth of the current map */

/**
 * @param[in] n
 * @param[out] list
 */
static void RandomList (int n, short *list)
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
 */
static void SV_ParseMapTile (const char *filename, const char **text)
{
	const char *errhead = "SV_ParseMapTile: Unexpected end of file (";
	const char *token;
	char *chr;
	mTile_t *t;
	int x, y, i;

	/* get tile name */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return;
	if (numTiles >= MAX_TILETYPES) {
		Com_Printf("SV_ParseMapTile: Too many map tile types (%s)\n", filename);
		return;
	}
	t = &mTile[numTiles];
	memset(t, 0, sizeof(*t));
	Q_strncpyz(t->id, token, sizeof(t->id));

	/* start parsing the block */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return;
	if (*token != '{') {
		Com_Printf("SV_ParseMapTile: Expected '{' for tile '%s' (%s)\n", t->id, filename);
		return;
	}

	/* get width and height */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return;
	t->w = atoi(token);

	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return;
	t->h = atoi(token);

	if (t->w > MAX_TILESIZE || t->h > MAX_TILESIZE) {
		Com_Printf("SV_ParseMapTile: Bad tile size [%i %i] (%s) (max. [%i %i])\n", t->w, t->h, filename, MAX_TILESIZE, MAX_TILESIZE);
		*text = strchr(*text, '}');
		return;
	}

	/* get tile specs */
	for (y = t->h - 1; y >= 0; y--)
		for (x = 0; x < t->w; x++) {
			token = Com_EParse(text, errhead, filename);
			if (!*text || *token == '}') {
				Com_Printf("SV_ParseMapTile: Bad tile desc in '%s' - not enough entries for size\n", t->id);
				*text = strchr(*text, '}') + 1;
				return;
			}
			t->spec[y][x] = 0L;
			for (i = 0; token[i]; i++, chr++) {
				t->spec[y][x] |= tileMask(token[i]);
			}
		}

	token = Com_EParse(text, errhead, filename);

	/* get connections */
	if (*token != '}')
		Com_Printf("SV_ParseMapTile: Bad tile desc in '%s' - too many entries for size\n", t->id);

	/* successfully parsed - this tile counts */
	numTiles++;
}


/**
 * @brief Parses an assembly block
 * @sa SV_AssembleMap
 * @sa SV_ParseMapTile
 * @note: format of size: "size x y"
 * @note: format of fix: "fix [tilename] x y"
 * @note: format of tile: "[tilename] min max"
 */
static void SV_ParseAssembly (const char *filename, const char **text)
{
	const char *errhead = "SV_ParseAssembly: Unexpected end of file (";
	const char *token, *cvarValue;
	char cvarName[MAX_VAR];
	mAssembly_t *a;
	int i, x, y;

	/* get assembly name */
	token = Com_EParse(text, errhead, filename);
	if (!*text)
		return;
	if (numAssemblies >= MAX_MAPASSEMBLIES) {
		Com_Printf("SV_ParseAssembly: Too many map assemblies (%s)\n", filename);
		return;
	}

	/* init */
	a = &mAssembly[numAssemblies++];
	memset(a, 0, sizeof(*a));
	Q_strncpyz(a->id, token, sizeof(a->id));
	a->w = 8;
	a->h = 8;
	a->dx = 1;
	a->dy = 1;

	token = Com_EParse(text, errhead, filename);
	if (!*text || *token != '{') {
		Com_Error(ERR_DROP, "Invalid assembly definition '%s' - invalid token '%s'", a->id, token);
		return;
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

			sscanf(token, "%i %i", &a->w, &a->h);
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

			for (i = 0; i < numTiles; i++)
				if (!strncmp(token, mTile[i].id, MAX_VAR)) {
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

		for (i = 0; i < numTiles; i++)
			if (!strncmp(token, mTile[i].id, MAX_VAR)) {
				/* get min and max tile number */
				token = Com_EParse(text, errhead, filename);
				if (!text || *token == '}')
					break;

				if (!strstr(token, " ")) {
					Com_Error(ERR_DROP, "SV_ParseAssembly: Error in assembly %s (min max value of tile %s)", filename, mTile[i].id);
					return;
				}
				sscanf(token, "%i %i", &x, &y);
				a->min[i] = x;
				a->max[i] = y;
				break;
			}
		if (i == numTiles)
			Com_Error(ERR_DROP, "Could not find tile: '%s' in assembly '%s' (%s)", token, a->id, filename);
	} while (text);
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
static void SV_ClearMap (void)
{
	uLong *mp;
	const uLong *me;

	memset(curMap, 0, sizeof(curMap));
	memset(curRating, 0, sizeof(curRating));

	mp = &curMap[0][0];
	me = &curMap[MAX_RANDOM_MAP_HEIGHT - 1][MAX_RANDOM_MAP_WIDTH - 1];
	while (mp <= me)
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
static qboolean SV_FitTile (mTile_t * tile, int x, int y)
{
	int tx, ty;
	const uLong *spec = NULL;
	const uLong *m = NULL;

	/* check vor valid grid positions */
	assert(x % mAsm->dx == 0);
	assert(y % mAsm->dy == 0);
	assert(tile);

	if (x < 0 || y < 0)
		return qfalse;

	/* check for map border */
	if (x + tile->w > mapW + 2 || y + tile->h > mapH + 2)
		return qfalse;

	/* test for fit */
	spec = &tile->spec[0][0];
	m = &curMap[y][x];
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
static qboolean SV_TestFilled (void)
{
	int x, y;

	for (y = 1; y < mapH + 1; y++)
		for (x = 1; x < mapW + 1; x++)
			if (!IS_SOLID(curMap[y][x]))
				return qfalse;

	return qtrue;
}

/**
 * @brief Debug fuction to dump the rating of the current map.
 */
static void SV_DumpRating (void)
{
	int x, y;

	Com_Printf("Rating:\n");
	for (y = mapH; y >= 1; y--) {
		for (x = 1; x < mapW + 1; x++)
			Com_Printf(" %2d", (int) curRating[y][x]);
		Com_Printf("\n");
	}
	Com_Printf("\n");
}

/**
 * @brief Debug fuction to dump the map location of a placed tile.
 */
static void SV_DumpPlaced (mPlaced_t * placed)
{
	int x, y;

	Com_Printf("Placed tile %s at %d %d\n", placed->tile->id, placed->x, placed->y);

	for (y = mapH; y >= 1; y--) {
		for (x = 1; x < mapW + 1; x++) {
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
 * @brief Returns the rating of the current map.
 * @return A value which roughly describes the connection quality of the map
 * @sa SV_AssembleMap
 * @sa SV_AddRegion
 * @sa SV_FitTile
 */
static int SV_CalcRating (void)
{
	int x, y, rating = 0;

	for (y = 1; y < mapH + 1; y++)
		for (x = 1; x < mapW + 1; x++)
			rating += curRating[y][x];

	if (sv_dumpmapassembly->integer)
		SV_DumpRating();

	return rating;
}

/**
 * @brief Adds a new map-tile to an assembled map. Also adds the tile to the placed-tiles list.
 * @note The tile must fit at the given position, otherwise an assert will occure!
 * @param[in] tile The tile to add to the map.
 * @param[in] x The x position in the map where the tile should be placed.
 * @param[in] y The y position in the map where the tile should be placed.
 * @param[in] idx The index of the placement algorithm.
 * @param[in] pos The position of the placement algorithm.
 * @sa SV_AssembleMap
 * @sa SV_AddRegion
 * @sa SV_FitTile
 */
static void SV_AddTile (const mTile_t *tile, int x, int y, int idx, int pos)
{
	int tx, ty;

	/* check vor valid grid positions */
	assert(x % mAsm->dx == 0);
	assert(y % mAsm->dy == 0);

	/* add the new tile */
	for (ty = 0; ty < tile->h; ty++)
		for (tx = 0; tx < tile->w; tx++) {
			assert(y + ty < MAX_RANDOM_MAP_HEIGHT);
			assert(x + tx < MAX_RANDOM_MAP_WIDTH);

			SV_CombineAlternatives(&curMap[y + ty][x + tx], tile->spec[ty][tx], &curRating[y + ty][x + tx]);
		}

	/* add the tile to the array of placed tiles*/
	if (numPlaced >= MAX_MAPTILES)
		Com_Error(ERR_DROP, "SV_AddTile: Too many map tiles");

	mPlaced[numPlaced].tile = tile;
	mPlaced[numPlaced].x = x;
	mPlaced[numPlaced].y = y;
	mPlaced[numPlaced].idx = idx;
	mPlaced[numPlaced].pos = pos;

	numPlaced++;

	if (idx >= 0) {
		mToPlace[idx].cnt++;
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
static void SV_RemoveTile (int* idx, int* pos)
{
	int tx, ty;
	int i, index;

	SV_ClearMap();

	if (numPlaced == 0)
		return;

	numPlaced--;
	index = mPlaced[numPlaced].idx;

	if (index >= 0) {
		mToPlace[index].cnt--;
	}

	for (i = numPlaced; i--;) {
		const mTile_t *tile = mPlaced[i].tile;
		const int x = mPlaced[i].x;
		const int y = mPlaced[i].y;
		assert(i >= 0);
		assert(tile);

		/* add the tile again*/
		for (ty = 0; ty < tile->h; ty++) {
			for (tx = 0; tx < tile->w; tx++) {
				assert(y + ty < MAX_RANDOM_MAP_HEIGHT);
				assert(x + tx < MAX_RANDOM_MAP_WIDTH);

				SV_CombineAlternatives(&curMap[y + ty][x + tx], tile->spec[ty][tx], &curRating[y + ty][x + tx]);
			}
		}
	}

	if (idx)
		*idx = index;

	if (pos)
		*pos = mPlaced[numPlaced].pos;
}

/**
 * @brief Tries to fit a tile in the current map.
 * @return qtrue if a fitting tile was found.
 * @return qfalse if no tile fits.
 * @sa SV_FitTile
 * @sa SV_AddTile
 */
static qboolean SV_AddRandomTile (int* idx, int* pos)
{
	const int start_idx = *idx = rand() % numToPlace;
	const int start_pos = *pos = rand() % mapSize;

	do {
		if (mToPlace[*idx].cnt < mToPlace[*idx].max) {
			do {
				const int x = (*pos) % mapW;
				const int y = (*pos) / mapW;

				if ((x % mAsm->dx == 0) && (y % mAsm->dy == 0) &&
						SV_FitTile(mToPlace[*idx].tile, x, y)) {
					SV_AddTile(mToPlace[*idx].tile, x, y, *idx, *pos);
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
static qboolean SV_AddMissingTiles (void)
{
	int i;
	int idx[CHECK_ALTERNATIVES_COUNT];
	int pos[CHECK_ALTERNATIVES_COUNT];
	int rating[CHECK_ALTERNATIVES_COUNT];
	const int startPlaced = numPlaced;

	while (1) {
		int max_rating = -mapW * mapH * 4;

		/* check if the map is already filled */
		if (SV_TestFilled())
			return qtrue;

		/* try some random tiles at random positions */
		for (i = 0; i < CHECK_ALTERNATIVES_COUNT; i++) {
			if (!SV_AddRandomTile(&idx[i], &pos[i])) {
				/* remove all tiles placed by this function */
				while (numPlaced > startPlaced)
					SV_RemoveTile(NULL, NULL);

				return qfalse;
			}

			if (SV_TestFilled())
				return qtrue;

			rating[i] = SV_CalcRating();

			if (rating[i] > max_rating)
				max_rating = rating[i];

			SV_RemoveTile(NULL, NULL);
		}

		for (i = 0; i < CHECK_ALTERNATIVES_COUNT; i++) {
			if (rating[i] == max_rating) {
				const int x = pos[i] % mapW;
				const int y = pos[i] / mapW;
				SV_AddTile(mToPlace[idx[i]].tile, x, y, idx[i], pos[i]);
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
static void SV_AddMapTiles (void)
{
	int idx, pos;
	const int start = numPlaced;

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

				if (SV_FitTile(mToPlace[idx].tile, x, y)) {
					/* add tile */
					SV_AddTile(mToPlace[idx].tile, x, y, idx, pos);
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
			assert(numPlaced > 0);
			assert(idx == mPlaced[numPlaced - 1].idx);
			SV_RemoveTile(&idx, &pos);
			pos++;
		}

		/* tile fits, try next tile */
		if (pos < mapSize) {
			pos = 0;
			idx++;
		} else {
			/* no more retries */
			if (start == numPlaced) {
				Com_Error(ERR_DROP, "SV_AddMapTiles: Impossible to assemble map\n");
			}
			SV_RemoveTile(&idx, &pos);
			pos++;
		}

		if ((idx == numToPlace) && !SV_AddMissingTiles()) {
			SV_RemoveTile(&idx, &pos);
			pos++;
		}
	}
}

/**
 * @brief Prepare the list of tiles to place
 * @sa SV_AssembleMap
 * @sa SV_AddTile
 */
static void SV_PrepareTilesToPlace (void)
{
	int i;

	numToPlace = 0;
	memset(&mToPlace[0], 0, sizeof(mToPlace));

	for (i = 0; i < numTiles; i++) {
		if (mAsm->max[i]) {
			mToPlace[numToPlace].tile = &mTile[i];
			mToPlace[numToPlace].min = mAsm->min[i];
			mToPlace[numToPlace].max = mAsm->max[i];
			numToPlace++;
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
static void SV_AssembleMap (const char *name, const char *assembly, const char **map, const char **pos)
{
	mPlaced_t *pl;
	static char asmMap[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
	static char asmPos[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
	char filename[MAX_QPATH];
	char basePath[MAX_QPATH];
	byte *buf;
	const char *text, *token;
	int i;

	/* load the map info */
	Com_sprintf(filename, sizeof(filename), "maps/%s.ump", name);
	FS_LoadFile(filename, &buf);
	if (!buf)
		Com_Error(ERR_DROP, "SV_AssembleMap: Map assembly info '%s' not found", filename);

	/* parse it */
	text = (const char*)buf;
	numTiles = 0;
	numAssemblies = 0;
	basePath[0] = 0;
	do {
		token = Com_Parse(&text);
		if (!text)
			break;

		if (!strcmp(token, "base")) {
			token = Com_Parse(&text);
			Q_strncpyz(basePath, token, sizeof(basePath));
		} else if (!strcmp(token, "tile"))
			SV_ParseMapTile(filename, &text);
		else if (!strcmp(token, "assembly"))
			SV_ParseAssembly(filename, &text);
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
	if (!numTiles)
		Com_Error(ERR_DROP, "No map tiles defined (%s)!", filename);
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_SERVER, "numTiles: %i\n", numTiles);
#endif

	if (!numAssemblies)
		Com_Error(ERR_DROP, "No map assemblies defined (%s)!", filename);
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_SERVER, "numAssemblies: %i\n", numAssemblies);
#endif

	/* get assembly */
	if (assembly && assembly[0]) {
		for (i = 0, mAsm = mAssembly; i < numAssemblies; i++, mAsm++)
			if (!strcmp(assembly, mAsm->id))
				break;
		if (i >= numAssemblies) {
			Com_Printf("SV_AssembleMap: Map assembly '%s' not found\n", assembly);
			mAsm = NULL;
		}
	} else
		mAsm = NULL;

	/* use random assembly, if no valid one has been specified */
	if (!mAsm)
		mAsm = &mAssembly[rand() % numAssemblies];
	Com_DPrintf(DEBUG_SERVER, "Use assembly: '%s'\n", mAsm->id);

	/* check size */
	assert(mAsm->w <= MAX_RANDOM_MAP_WIDTH);
	assert(mAsm->h <= MAX_RANDOM_MAP_HEIGHT);

	SV_PrepareTilesToPlace();

	/* assemble the map */
	numPlaced = 0;
	SV_ClearMap();

	/* place fixed parts - defined in ump via fix parameter */
	for (i = 0; i < mAsm->numFixed; i++)
		SV_AddTile(&mTile[mAsm->fT[i]], mAsm->fX[i], mAsm->fY[i], -1, -1);

	/* place non fixed map parts */
	mapW = mAsm->w;
	mapH = mAsm->h;
	mapSize = mapW * mapH;

	SV_AddMapTiles();

	/* prepare map and pos strings */
	if (basePath[0]) {
		asmMap[0] = '-';
		Q_strncpyz(&asmMap[1], basePath, MAX_QPATH - 1);
		*map = asmMap;
	} else {
		asmMap[0] = 0;
		*map = asmMap + 1;
	}
	asmPos[0] = 0;
	*pos = asmPos + 1;

	/* generate the strings */
	for (i = 0, pl = mPlaced; i < numPlaced; i++, pl++) {
		if (sv_dumpmapassembly->integer)
			SV_DumpPlaced(pl);

		Q_strcat(asmMap, va(" %s", pl->tile->id), MAX_TOKEN_CHARS * MAX_TILESTRINGS);
		Q_strcat(asmPos, va(" %i %i %i", (pl->x - mAsm->w / 2) * 8, (pl->y - mAsm->h / 2) * 8, 0), MAX_TOKEN_CHARS * MAX_TILESTRINGS);
	}

	Com_DPrintf(DEBUG_SERVER, "tiles: %s\n", *map);
	Com_DPrintf(DEBUG_SERVER, "pos: %s\n", *pos);
	Com_DPrintf(DEBUG_SERVER, "tiles: %i\n", numPlaced);
}

/**
 * @brief Get the map title for a given map
 * @note the title string must be translated client side
 * @return Never NULL - mapname or maptitle (if defined in assembly)
 */
static const char* SV_GetMapTitle (const char* const mapname)
{
	assert(mapname);

	if (mapname[0] == '+') {
		if (mAsm && mAsm->title[0]) {
			/* return the assembly title itself - must be translated client side */
			if (mAsm->title[0] == '_')
				return mAsm->title + 1;
			else {
				Com_Printf("The assembly title '%s' is not marked as translateable\n", mAsm->title);
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
	const char *map, *pos, *buf;

	assert(server);
	assert(server[0] != '\0');

	Com_DPrintf(DEBUG_SERVER, "SpawnServer: %s\n", server);

	svs.spawncount++;
	/* any partially connected client will be restarted */
	Com_SetServerState(ss_dead);

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
		svs.clients[i].lastframe = -1;
	}

	/* assemble and load the map */
	if (server[0] == '+') {
		SV_AssembleMap(server + 1, param, &map, &pos);
		if (!map || !pos) {
			Com_Printf("Could not load assembly for map '%s'\n", server);
			return;
		}
	} else {
		map = server;
		pos = param;
	}

	Q_strncpyz(sv.configstrings[CS_TILES], map, MAX_TOKEN_CHARS * MAX_TILESTRINGS);
	if (pos)
		Q_strncpyz(sv.configstrings[CS_POSITIONS], pos, MAX_TOKEN_CHARS * MAX_TILESTRINGS);
	else
		sv.configstrings[CS_POSITIONS][0] = 0;

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

	Com_sprintf(sv.configstrings[CS_MAPTITLE], sizeof(sv.configstrings[CS_MAPTITLE]), "%s", SV_GetMapTitle(server));
	if (!strncmp(sv.configstrings[CS_MAPTITLE], "b/", 2)) {
		/* For base attack, cl.configstrings[CS_MAPTITLE] contains too many chars */
		Com_sprintf(sv.configstrings[CS_MAPTITLE], sizeof(sv.configstrings[CS_MAPTITLE]), "Base attack");
	}

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

	/* the game is just starting */
	if (Com_ServerState() == ss_dead)
		SV_InitGame();

	if (!svs.initialized) {
		Com_Printf("Could not spawn the map\n");
		return;
	}

	SCR_BeginLoadingPlaque();
	SV_SpawnServer(day, levelstring, assembly);
	Cbuf_CopyToDefer();
}
