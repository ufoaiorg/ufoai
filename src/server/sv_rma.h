/**
 * @file sv_rma.h
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

#ifndef SV_RMA_H_
#define SV_RMA_H_

#define MAX_MAPASSEMBLIES 32
#define MAX_TILETYPES 64
#define MAX_TILESETS 16
#define MAX_TILESETTILES 8
#define MAX_TILESIZE 16
#define MAX_FIXEDTILES 64

#define MAX_RANDOM_MAP_WIDTH 32
#define MAX_RANDOM_MAP_HEIGHT 32

/** @brief Stores the parsed data for a map tile. (See *.ump files) */
typedef struct mTile_s {
	char id[MAX_VAR];	/**< The id (string) of the tile as defined in the ump file (next to "tile"). */
	unsigned long spec[MAX_TILESIZE][MAX_TILESIZE];	/**< connection/alternatives info for the tile  */
	int w, h;			/**< The width and height of the tile. */
	int area;			/**< Number of solid parts */
} mTile_t;

typedef struct mTileSet_s {
	char id[MAX_VAR];
	char tiles[MAX_TILESETTILES][MAX_VAR];
	int numTiles;
} mTileSet_t;

#define MAX_ASSEMBLY_SEEDS 32

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
	int seeds[MAX_ASSEMBLY_SEEDS]; /**< seeds that are used to assemble this assembly definition if the previous used
				* seed did not lead to a solution. The seeds given in the seeds block of the assembly are known to work. */
	int numSeeds;
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
	unsigned long curMap[MAX_RANDOM_MAP_HEIGHT][MAX_RANDOM_MAP_WIDTH];

	/** @brief Stores the map rating for the assembled map */
	char curRating[MAX_RANDOM_MAP_HEIGHT][MAX_RANDOM_MAP_WIDTH];

	/** @brief Stores the Tiles to Place in the map */
	mToPlace_t mToPlace[MAX_TILETYPES];
	int numToPlace;								/**< the size of the to place list */

	mAssembly_t mAssembly[MAX_MAPASSEMBLIES];	/**< A list of parsed assembly definitions. */
	int numAssemblies;							/**< The number of assemblies in mAssembly. */

	mPlaced_t mPlaced[MAX_MAPTILES];			/**< Holds all tiles that have been placed on the current map. */
	int numPlaced;								/**< The number of tiles in mPlaced. */

	mTileSet_t mTileSets[MAX_TILESETS];			/**< A list of parsed map-tiles. */
	int numTileSets;							/**< Length of the mTile list */

	mTile_t mTile[MAX_TILETYPES];				/**< A list of parsed map-tiles. */
	int numTiles;								/**< Length of the mTile list */
	unsigned long lineFlags;					/** the combined bit values of the tiles that must form a line, ie. river, street etc. */

	char basePath[MAX_QPATH];					/**< the base path of the main rma */
	char inheritBasePath[MAX_QPATH];			/**< only used during parsing, don't rely on any reasonable data after the parsing stage */

	int mAsm;									/**< the index of the selected assembly */

	int retryCnt;								/**< amount of retries in case the assembly didn't assemble and we are using the defined seeds */
} mapInfo_t;

mapInfo_t* SV_AssembleMap(const char *name, const char *assembly, char *asmMap, char *asmPos, const unsigned int seed);

/* the next two functions are only exported for cunits tests */
void SV_ParseUMP(const char *name, mapInfo_t *map, qboolean inherit);
void SV_PrepareTilesToPlace (mapInfo_t *map);

#endif /* SV_RMA_H_ */
