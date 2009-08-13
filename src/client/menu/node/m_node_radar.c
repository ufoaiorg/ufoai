/**
 * @file m_node_radar.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "m_node_radar.h"
#include "m_node_abstractnode.h"
#include "../m_render.h"
#include "../m_timer.h"
#include "../m_menus.h"
#include "../m_main.h"

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../../renderer/r_misc.h"
#include "../../cl_le.h"	/**< cl_actor.h needs this */
#include "../../../shared/parse.h"

/** @brief Each maptile must have an entry in the images array */
typedef struct hudRadarImage_s {
	char *name;		/**< the mapname */
	char *path[PATHFINDING_HEIGHT];		/**< the path to the image (including name) */
	int w, h;		/**< the width and height of the image */
	int x, y;		/**< screen coordinates for the image */
	int gridX, gridY;	/**< random map assembly x and y positions, @sa UNIT_SIZE */
	int maxlevel;	/**< the maxlevel for this image */
} hudRadarImage_t;

typedef struct hudRadar_s {
	/** The dimension of the icons on the radar map */
	float gridHeight, gridWidth;
	vec2_t gridMin, gridMax;	/**< from position string */
	char base[MAX_QPATH];		/**< the base path in case of a random map assembly */
	int numImages;	/**< amount of images in the images array */
	hudRadarImage_t images[MAX_MAPTILES];
	/** three vectors of the triangle, lower left (a), lower right (b), upper right (c)
	 * the triangle is something like:
	 *     - c
	 *    --
	 * a --- b
	 * and describes the three vertices of the rectangle (the radar plane)
	 * dividing triangle */
	vec3_t a, b, c;
	/** radar plane screen (pixel) coordinates */
	int x, y;
	/** radar screen (pixel) dimensions */
	int w, h;
} hudRadar_t;

static hudRadar_t radar;

static void MN_FreeRadarImages (void)
{
	int i, j;

	for (i = 0; i < radar.numImages; i++) {
		Mem_Free(radar.images[i].name);
		for (j = 0; j < radar.images[i].maxlevel; j++)
			Mem_Free(radar.images[i].path[j]);
	}
	memset(&radar, 0, sizeof(radar));
}

/**
 * @brief Reads the tiles and position config strings and convert them into a
 * linked list that holds the imagename (mapname), the x and the y position
 * (screencoordinates)
 * @param[in] tiles The configstring with the tiles (map tiles)
 * @param[in] pos The position string, only used in case of random map assembly
 * @sa MN_DrawRadar
 * @sa R_ModBeginLoading
 */
static void MN_BuildRadarImageList (const char *tiles, const char *pos)
{
	/* load tiles */
	while (tiles) {
		char name[MAX_VAR];
		hudRadarImage_t *image;
		/* get tile name */
		const char *token = Com_Parse(&tiles);
		if (!tiles) {
			/* finish */
			return;
		}

		/* get base path */
		if (token[0] == '-') {
			Q_strncpyz(radar.base, token + 1, sizeof(radar.base));
			continue;
		}

		/* get tile name */
		if (token[0] == '+')
			token++;
		Com_sprintf(name, sizeof(name), "%s%s", radar.base, token);

		image = &radar.images[radar.numImages++];
		image->name = Mem_StrDup(name);

		if (pos && pos[0]) {
			int i;
			vec3_t sh;
			/* get grid position and add a tile */
			for (i = 0; i < 3; i++) {
				token = Com_Parse(&pos);
				if (!pos)
					Com_Error(ERR_DROP, "MN_BuildRadarImageList: invalid positions\n");
				sh[i] = atoi(token);
			}
			image->gridX = sh[0];
			image->gridY = sh[1];

			if (radar.gridMin[0] > sh[0])
				radar.gridMin[0] = sh[0];
			if (radar.gridMin[1] > sh[1])
				radar.gridMin[1] = sh[1];
		} else {
			/* load only a single tile, if no positions are specified */
			return;
		}
	}
}

/**
 * @brief Get the width of radar.
 * @param[in] node Menu node of the radar
 * @param[in] gridSize size of the radar picture, in grid units.
 * @sa MN_InitRadar
 */
static void MN_GetRadarWidth (const menuNode_t *node, vec2_t gridSize)
{
	int j;
	int tileWidth[2];		/**< Contains the width of the first and the last tile of the first line (in screen unit) */
	int tileHeight[2];		/**< Contains the height of the first and the last tile of the first column  (in screen unit)*/
	int secondTileGridX;	/**< Contains the grid X position of 2nd tiles in first line */
	int secondTileGridY;	/**< Contains the grid Y position of 2nd tiles in first column */
	float ratioConversion;	/**< ratio conversion between screen coordinates and grid coordinates */
	const int ROUNDING_PIXEL = 1;	/**< Number of pixel to remove to avoid rounding errors (and lines between tiles)
									 * We remove pixel because this is much nicer if tiles overlap a little bit rather than
									 * if they are too distant one from the other */

	/* Set radar.gridMax */
	radar.gridMax[0] = radar.gridMin[0];
	radar.gridMax[1] = radar.gridMin[1];

	/* Initialize secondTileGridX to value higher that real value */
	secondTileGridX = radar.gridMin[0] + 1000;
	secondTileGridY = radar.gridMin[1] + 1000;

	/* Initialize screen size of last tile (will be used only if there is 1 tile in a line or in a row) */
	Vector2Set(tileWidth, 0, 0);
	Vector2Set(tileHeight, 0, 0);

	for (j = 0; j < radar.numImages; j++) {
		const hudRadarImage_t *image = &radar.images[j];

		assert(image->gridX >= radar.gridMin[0]);
		assert(image->gridY >= radar.gridMin[1]);

		/* we can assume this because every random map tile has it's origin in
		 * (0, 0) and therefore there are no intersections possible on the min
		 * x and the min y axis. We just have to add the image->w and image->h
		 * values of those images that are placed on the gridMin values.
		 * This also works for static maps, because they have a gridX and gridY
		 * value of zero */

		if (image->gridX == radar.gridMin[0]) {
			/* radar.gridMax[1] is the maximum for FIRST column (maximum depends on column) */
			if (image->gridY > radar.gridMax[1]) {
				tileHeight[1] = image->h;
				radar.gridMax[1] = image->gridY;
			}
			if (image->gridY == radar.gridMin[1]) {
				/* This is the tile of the map in the lower left: */
				tileHeight[0] = image->h;
				tileWidth[0] = image->w;
			} else if (image->gridY < secondTileGridY)
				secondTileGridY = image->gridY;
		}
		if (image->gridY == radar.gridMin[1]) {
			/* radar.gridMax[1] is the maximum for FIRST line (maximum depends on line) */
			if (image->gridX > radar.gridMax[0]) {
				tileWidth[1] = image->w;
				radar.gridMax[0] = image->gridX;
			} else if (image->gridX < secondTileGridX)
				secondTileGridX = image->gridX;
		}
	}

	/* Maybe there was only one tile in a line or in a column? */
	if (!tileHeight[1])
		tileHeight[1] = tileHeight[0];
	if (!tileWidth[1])
		tileWidth[1] = tileWidth[0];

	/* Now we get the ratio conversion between screen coordinates and grid coordinates.
	 * The problem is that some tiles may have L or T shape.
	 * But we now that the tile in the lower left follows for sure the side of the map on it's whole length
	 * at least either on its height or on its width.*/
	ratioConversion = max((secondTileGridX - radar.gridMin[0]) / (tileWidth[0] - ROUNDING_PIXEL),
		(secondTileGridY - radar.gridMin[1]) / (tileHeight[0] - ROUNDING_PIXEL));

	/* And now we fill radar.w and radar.h */
	radar.w = floor((radar.gridMax[0] - radar.gridMin[0]) / ratioConversion) + tileWidth[1];
	radar.h = floor((radar.gridMax[1] - radar.gridMin[1]) / ratioConversion) + tileHeight[1];

	Vector2Set(gridSize, round(radar.w * ratioConversion), round(radar.h * ratioConversion));
}

static const char *imageExtensions[] = {
	"tga", "jpg", "png", NULL
};

static qboolean MN_CheckRadarImage (const char *imageName, const int level)
{
	char imagePath[MAX_QPATH];
	const char **ext = imageExtensions;

	while (*ext) {
		Com_sprintf(imagePath, sizeof(imagePath), "radars/%s_%i.%s", imageName, level, *ext);

		if (FS_CheckFile(va("pics/%s", imagePath)) > 0)
			return qtrue;
		ext++;
	}
	/* none found */
	return qfalse;
}

/**
 * @brief Calculate some radar values that won't change during a mission
 * @note Called for every new map (client_state_t is wiped with every
 * level change)
 */
static void MN_InitRadar (const menuNode_t *node)
{
	int i, j;
	const vec3_t offset = {MAP_SIZE_OFFSET, MAP_SIZE_OFFSET, MAP_SIZE_OFFSET};
	float distAB, distBC;
	vec2_t gridSize;		/**< Size of the whole grid (in tiles units) */
	vec2_t nodepos;

	MN_FreeRadarImages();
	MN_BuildRadarImageList(cl.configstrings[CS_TILES], cl.configstrings[CS_POSITIONS]);

	MN_GetNodeAbsPos(node, nodepos);
	radar.x = nodepos[0] + node->size[0] / 2;
	radar.y = nodepos[1] + node->size[1] / 2;

	/* only check once per map whether all the needed images exist */
	for (i = 0; i < PATHFINDING_HEIGHT; i++) {
		/* map_mins, map_maxs */
		for (j = 0; j < radar.numImages; j++) {
			hudRadarImage_t *hudRadarImage = &radar.images[j];
			if (!MN_CheckRadarImage(hudRadarImage->name, i + 1)) {
				if (i == 0) {
					/* there should be at least one level */
					Com_Printf("No radar images for map: '%s'\n", hudRadarImage->name);
					radar.numImages = 0;
					return;
				}
			} else {
				char imagePath[MAX_QPATH];
				const image_t *image;
				Com_sprintf(imagePath, sizeof(imagePath), "radars/%s_%i", hudRadarImage->name, i + 1);
				hudRadarImage->path[i] = Mem_StrDup(imagePath);
				hudRadarImage->maxlevel++;

				image = R_FindImage(va("pics/%s", hudRadarImage->path[i]), it_pic);
				hudRadarImage->w = image->width;
				hudRadarImage->h = image->height;
				if (hudRadarImage->w > VID_NORM_WIDTH || hudRadarImage->h > VID_NORM_HEIGHT)
					Com_Printf("Image '%s' is too big\n", hudRadarImage->path[i]);
			}
		}
	}

	/* get the three points of the triangle */
	VectorSubtract(mapMin, offset, radar.a);
	VectorAdd(mapMax, offset, radar.c);
	VectorSet(radar.b, radar.c[0], radar.a[1], 0);

	distAB = (Vector2Dist(radar.a, radar.b) / UNIT_SIZE);
	distBC = (Vector2Dist(radar.b, radar.c) / UNIT_SIZE);

	MN_GetRadarWidth(node, gridSize);

	/* get the dimensions for one grid field on the radar map */
	radar.gridWidth = radar.w / distAB;
	radar.gridHeight = radar.h / distBC;

	/* shift the x and y values according to their grid width/height and
	 * their gridX and gridY position */
	{
		const float radarLength = max(1, abs(gridSize[0]));
		const float radarHeight = max(1, abs(gridSize[1]));
		/* image grid relations */
		const float gridFactorX = radar.w / radarLength;
		const float gridFactorY = radar.h / radarHeight;
		for (j = 0; j < radar.numImages; j++) {
			hudRadarImage_t *image = &radar.images[j];

			image->x = (image->gridX - radar.gridMin[0]) * gridFactorX;
			image->y = radar.h - (image->gridY - radar.gridMin[1]) * gridFactorY - image->h;
		}
	}

	/* now align the screen coordinates like it's given by the menu node */
	radar.x -= (radar.w / 2);
	radar.y -= (radar.h / 2);
}

/*=========================================
 DRAW FUNCTIONS
=========================================*/

static void MN_DrawActor (const le_t *le, const vec3_t pos)
{
	vec4_t color = {0, 1, 0, 1};
	const int actorLevel = le->pos[2];
#if 0
	/* used to interpolate movement on the radar */
	const int interpolateX = (int)le->origin[0] % (UNIT_SIZE / 2);
	const int interpolateY = (int)le->origin[1] % (UNIT_SIZE / 2);
#endif
	/* relative to screen */
	const int x = pos[0];
	const int y = pos[1];

	/* use different alpha values for different levels */
	if (actorLevel < cl_worldlevel->integer)
		color[3] = 0.5;
	else if (actorLevel > cl_worldlevel->integer)
		color[3] = 0.3;

	/* use different colors for different teams */
	if (le->team == TEAM_CIVILIAN) {
		color[0] = 1;
	} else if (le->team != cls.team) {
		color[1] = 0;
		color[0] = 1;
	}

	/* show dead actors in full black */
	if (LE_IsDead(le)) {
		/* low alpha because we want to see items on the floor, too */
		Vector4Set(color, 0, 0, 0, 0.3);
	} else {
		/* draw direction line only for living actors */
		int verts[4];
		const float actorDirection = directionAngles[le->dir] * torad;
		verts[0] = x;
		verts[1] = y;
		verts[2] = x + (radar.gridWidth * cos(actorDirection));
		verts[3] = y - (radar.gridWidth * sin(actorDirection));
		R_DrawLine(verts, 3);

		/* 120 degree frustum view - see FrustumVis */
		verts[2] = x + (radar.gridWidth * cos((directionAngles[le->dir] + 60) * torad)) * 5;
		verts[3] = y - (radar.gridWidth * sin((directionAngles[le->dir] + 60) * torad)) * 5;
		R_DrawLine(verts, 0.1);

		verts[2] = x + (radar.gridWidth * cos((directionAngles[le->dir] - 60) * torad)) * 5;
		verts[3] = y - (radar.gridWidth * sin((directionAngles[le->dir] - 60) * torad)) * 5;
		R_DrawLine(verts, 0.1);
	}

	/* draw player circles */
	R_DrawCircle2D(x, y, radar.gridWidth / 2, qtrue, color, 1);
	if (le->selected)
		Vector4Set(color, 1.0, 1.0, 1.0, 1.0);
	else
		Vector4Set(color, 0.8, 0.8, 0.8, 1.0);
	/* outline */
	R_DrawCircle2D(x, y, radar.gridWidth / 2, qfalse, color, 2);
}

static void MN_RadarNodeDrawItem (const le_t *le, const vec3_t pos)
{
	const vec4_t color = {0, 1, 0, 1};
	/* relative to screen */
	const int x = pos[0];
	const int y = pos[1];
	MN_DrawFill(x, y, radar.gridWidth, radar.gridHeight, color);
}

#define RADARSIZE_DEBUG

/**
 * @sa CMod_GetMapSize
 * @todo Show frustum view area for actors (@sa FrustumVis)
 * @note we only need to handle the 2d plane and can ignore the z level
 * @param[in] node The radar menu node (located in the hud menu definitions)
 */
static void MN_RadarNodeDraw (menuNode_t *node)
{
	const le_t *le;
	int i;
	vec3_t pos;
#ifdef RADARSIZE_DEBUG
	int textposy = 40;
	static const vec4_t red = {1, 0, 0, 0.5};
#endif

	static const vec4_t grey = {0.7, 0.7, 0.7, 1};
	const float mapWidth = mapMax[0] - mapMin[0];
	const float mapHeight = mapMax[1] - mapMin[1];

	/** @todo use the same coef for x and y */
	const float mapCoefX = (float) node->size[0] / (float) mapWidth;
	const float mapCoefY = (float) node->size[1] / (float) mapHeight;
	/** @todo understand this coef */
	const float MAGIC_COEF = 1.0f; //(500.0f / 338.0f);
	const float imageCoefX = mapCoefX * MAGIC_COEF;
	const float imageCoefY = mapCoefY * MAGIC_COEF;

	/** @todo remove cl.skipRadarNodes from the code */
	if (cls.state != ca_active)
		return;

	MN_GetNodeAbsPos(node, pos);
	R_CleanupDepthBuffer(pos[0], pos[1], node->size[0], node->size[1]);
	MN_DrawFill(pos[0], pos[1], mapWidth * mapCoefX, mapHeight * mapCoefY, grey);
#ifndef RADARSIZE_DEBUG
	R_BeginClipRect(pos[0], pos[1], node->size[0], node->size[1]);
#endif

	/* the cl struct is wiped with every new map */
	if (!cl.radarInited) {
		MN_InitRadar(node);
		cl.radarInited = qtrue;
	}

	/* update context */
	radar.x = pos[0];
	radar.y = pos[1];
	radar.w = node->size[0];
	radar.h = node->size[1];
	if (radar.gridWidth < 6)
		radar.gridWidth = 6;
	if (radar.gridHeight < 6)
		radar.gridHeight = 6;

#ifdef RADARSIZE_DEBUG
		MN_DrawStringInBox("f_small", 0, 50, textposy, 500, 25, va("%fx%f %fx%f map", mapMin[0], mapMin[1], mapMax[0], mapMax[1]), LONGLINES_PRETTYCHOP);
		textposy += 25;
		MN_DrawStringInBox("f_small", 0, 50, textposy, 500, 25, va("%fx%f map", mapWidth, mapHeight), LONGLINES_PRETTYCHOP);
		textposy += 25;
#endif

	/* draw background */
	for (i = 0; i < radar.numImages; i++) {
		hudRadarImage_t *image = &radar.images[i];
		int maxlevel = cl_worldlevel->integer;
		/* check the max level value for this map tile */
		if (maxlevel >= image->maxlevel)
			maxlevel = image->maxlevel - 1;
		assert(image->path[maxlevel]);
		MN_DrawNormImageByName(
				radar.x + imageCoefX * image->x, radar.y + imageCoefY * image->y,
				imageCoefX * image->w, imageCoefY * image->h,
				0, 0, 0, 0, image->path[maxlevel]);
#ifdef RADARSIZE_DEBUG
		MN_DrawStringInBox("f_small", 0, 50, textposy, 500, 25, va("%dx%d %dx%d %s", image->x, image->y, image->w, image->h, image->path[maxlevel]), LONGLINES_PRETTYCHOP);
		textposy += 25;
#endif
	}

#ifdef RADARSIZE_DEBUG
	MN_DrawFill(pos[0], pos[1], 100.0f * mapCoefX, 100.0f * mapCoefY, red);
	MN_DrawFill(pos[0], pos[1], UNIT_SIZE * mapCoefX, UNIT_SIZE * mapCoefY, red);
#endif

	for (i = 0, le = LEs; i < cl.numLEs; i++, le++) {
		vec3_t itempos;
		if (!le->inuse || le->invis)
			continue;

		/* convert to radar area coordinates */
		itempos[0] = pos[0] + (le->origin[0] - mapMin[0]) * mapCoefX;
		itempos[1] = pos[1] + (mapHeight - (le->origin[1] - mapMin[1])) * mapCoefY;

		switch (le->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
			MN_DrawActor(le, itempos);
			break;
		case ET_ITEM:
			MN_RadarNodeDrawItem(le, itempos);
			break;
		}
#ifdef RADARSIZE_DEBUG
		MN_DrawStringInBox("f_small", 0, 50, textposy, 500, 25, va("%fx%f %dx%d actor", le->origin[0], le->origin[1], le->pos[0], le->pos[1]), LONGLINES_PRETTYCHOP);
		textposy += 25;
		MN_DrawFill(itempos[0], itempos[1], UNIT_SIZE * mapCoefX, 1, red);
		MN_DrawFill(itempos[0], itempos[1], 1, UNIT_SIZE * mapCoefY, red);
#endif
	}

#ifndef RADARSIZE_DEBUG
	R_EndClipRect();
#endif
}

static void MN_GenPreviewRadarMap_f (void)
{
	int x, y, width, height;
	// map into screen
	R_FrameBufferMapSize(&x, &y, &width, &height);

	// from screen to virtual screen
	x /= viddef.rx;
	width /= viddef.rx;
	y /= viddef.ry;
	height /= viddef.ry;
	y = viddef.virtualHeight - y - height;

	MN_ExecuteConfunc("mn_radarhud_setmapborder %d %d %d %d", x, y, width, height);
}

/**
 * Take a screen shot of the map with the position of the radar
 *
 * We add 1 pixel into the border to easy check the result:
 * the screen shot must have a border of 1 black pixel
 */
static void MN_GenRadarMap_f (void)
{
	const int border = 1;
	const char *mapName = Cvar_GetString("sv_mapname");
	const int level = Cvar_GetInteger("cl_worldlevel");
	const char *filename = NULL;
	int x, y, width, height;

	R_FrameBufferMapSize(&x, &y, &width, &height);
	Com_Printf("Radar map size from frame buffer: %i %i %i %i\n", x, y, width, height);
	if (mapName)
		filename = va("%s_%i", mapName, level);
	R_ScreenShot(x - border, y - border, width + border * 2, height + border * 2, filename, NULL);
}

static void MN_GenAllRadarMap (menuNode_t *node, menuTimer_t *timer)
{
	int level = timer->calledTime / 2;
	int mode = timer->calledTime % 2;

	if (level >= cl.mapMaxLevel) {
		Cbuf_AddText("mn_genallradarmaprelease");
		return;
	}

	if (mode == 0)
		Cvar_SetValue("cl_worldlevel", level);
	else
		Cmd_ExecuteString("mn_genradarmap");
}

menuTimer_t* timer;

/**
 * @todo allow to call MN_TimerRelease into timer callback
 */
static void MN_GenAllRadarMapRelease_f (void) {
	MN_TimerRelease(timer);
	MN_ExecuteConfunc("mn_radarhud_reinit");
}

/**
 * Take all screenshots from lower to upper map level.
 * Use a timer to delay each capture
 */
static void MN_GenAllRadarMap_f (void)
{
	const int delay = 1000;
	timer = MN_AllocTimer(NULL, delay, MN_GenAllRadarMap);
	MN_TimerStart(timer);
}

void MN_RegisterRadarNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "radar";
	behaviour->draw = MN_RadarNodeDraw;

	Cmd_AddCommand("mn_genpreviewradarmap", MN_GenPreviewRadarMap_f, NULL);
	Cmd_AddCommand("mn_genradarmap", MN_GenRadarMap_f, NULL);
	Cmd_AddCommand("mn_genallradarmap", MN_GenAllRadarMap_f, NULL);
	Cmd_AddCommand("mn_genallradarmaprelease", MN_GenAllRadarMapRelease_f, NULL);
}
