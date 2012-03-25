/**
 * @file ui_node_radar.c
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "ui_node_radar.h"
#include "ui_node_abstractnode.h"
#include "../ui_render.h"
#include "../ui_main.h"
#include "../ui_behaviour.h"
#include "../ui_input.h"

#include "../../client.h"
#include "../../battlescape/cl_hud.h" /* cl_worldlevel cvar */
#include "../../renderer/r_draw.h"
#include "../../../shared/parse.h"

/** @brief Each maptile must have an entry in the images array */
typedef struct hudRadarImage_s {
	/* image */
	char *name;				/**< the mapname */
	char *path[PATHFINDING_HEIGHT];		/**< the path to the image (including name) */
	int x, y;				/**< screen coordinates for the image */
	int width, height;		/**< the width and height of the image */

	/* position of this piece into the map */
	float mapX;
	float mapY;
	float mapWidth;
	float mapHeight;

	qboolean isTile;
	int gridX, gridY;	/**< random map assembly x and y positions, @sa UNIT_SIZE */
	int gridHeight;
	int gridWidth;
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

static void UI_FreeRadarImages (void)
{
	int i, j;

	for (i = 0; i < radar.numImages; i++) {
		hudRadarImage_t *image = &radar.images[i];
		Mem_Free(image->name);
		for (j = 0; j < image->maxlevel; j++)
			Mem_Free(image->path[j]);
	}
	OBJZERO(radar);
}

/**
 * @brief Reads the tiles and position config strings and convert them into a
 * linked list that holds the imagename (mapname), the x and the y position
 * (screencoordinates)
 * @param[in] tiles The configstring with the tiles (map tiles)
 * @param[in] pos The position string, only used in case of random map assembly
 * @sa UI_DrawRadar
 * @sa R_ModBeginLoading
 */
static void UI_BuildRadarImageList (const char *tiles, const char *pos)
{
	const float mapMidX = (cl.mapData->mapMax[0] + cl.mapData->mapMin[0]) * 0.5;
	const float mapMidY = (cl.mapData->mapMax[1] + cl.mapData->mapMin[1]) * 0.5;

	/* load tiles */
	while (tiles) {
		int i;
		vec3_t sh;
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

		image->isTile = pos && pos[0];
		if (!image->isTile)
			/* it is a single background image*/
			return;

		/* get grid position and add a tile */
		for (i = 0; i < 3; i++) {
			token = Com_Parse(&pos);
			if (!pos)
				Com_Error(ERR_DROP, "UI_BuildRadarImageList: invalid positions\n");
			sh[i] = atoi(token);
		}
		image->gridX = sh[0];
		image->gridY = sh[1];
		image->mapX = mapMidX + sh[0] * UNIT_SIZE;
		image->mapY = mapMidY + sh[1] * UNIT_SIZE;
		Com_Printf("radar %s %dx%d\n", name, image->gridX, image->gridY);

		if (radar.gridMin[0] > sh[0])
			radar.gridMin[0] = sh[0];
		if (radar.gridMin[1] > sh[1])
			radar.gridMin[1] = sh[1];
	}
}

/**
 * @brief Get the width of radar.
 * @param[in] node Node description of the radar
 * @param[in] gridSize size of the radar picture, in grid units.
 * @sa UI_InitRadar
 */
static void UI_GetRadarWidth (const uiNode_t *node, vec2_t gridSize)
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
				tileHeight[1] = image->height;
				radar.gridMax[1] = image->gridY;
			}
			if (image->gridY == radar.gridMin[1]) {
				/* This is the tile of the map in the lower left: */
				tileHeight[0] = image->height;
				tileWidth[0] = image->width;
			} else if (image->gridY < secondTileGridY)
				secondTileGridY = image->gridY;
		}
		if (image->gridY == radar.gridMin[1]) {
			/* radar.gridMax[1] is the maximum for FIRST line (maximum depends on line) */
			if (image->gridX > radar.gridMax[0]) {
				tileWidth[1] = image->width;
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

static char const* const imageExtensions[] = {
	"tga", "jpg", "png", NULL
};

static qboolean UI_CheckRadarImage (const char *imageName, const int level)
{
	char const* const* ext = imageExtensions;

	while (*ext) {
		if (FS_CheckFile("pics/radars/%s_%i.%s", imageName, level, *ext) > 0)
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
static void UI_InitRadar (const uiNode_t *node)
{
	int i, j;
	const vec3_t offset = {MAP_SIZE_OFFSET, MAP_SIZE_OFFSET, MAP_SIZE_OFFSET};
	float distAB, distBC;
	vec2_t gridSize;		/**< Size of the whole grid (in tiles units) */
	vec2_t nodepos;
	vec2_t min;
	vec2_t max;

	UI_FreeRadarImages();
	UI_BuildRadarImageList(CL_GetConfigString(CS_TILES), CL_GetConfigString(CS_POSITIONS));

	UI_GetNodeAbsPos(node, nodepos);
	radar.x = nodepos[0] + node->size[0] / 2;
	radar.y = nodepos[1] + node->size[1] / 2;

	/* only check once per map whether all the needed images exist */
	for (j = 0; j < radar.numImages; j++) {
		hudRadarImage_t *tile = &radar.images[j];
		/* map_mins, map_maxs */
		for (i = 0; i < PATHFINDING_HEIGHT; i++) {
			char imagePath[MAX_QPATH];
			const image_t *image;
			if (!UI_CheckRadarImage(tile->name, i + 1)) {
				if (i == 0) {
					/* there should be at least one level */
					Com_Printf("No radar images for map: '%s'\n", tile->name);
					radar.numImages = 0;
					return;
				}
				continue;
			}

			Com_sprintf(imagePath, sizeof(imagePath), "radars/%s_%i", tile->name, i + 1);
			tile->path[i] = Mem_StrDup(imagePath);
			tile->maxlevel++;

			image = R_FindImage(va("pics/%s", tile->path[i]), it_pic);
			tile->width = image->width;
			tile->height = image->height;
			if (tile->isTile) {
				tile->gridWidth = round(image->width / 94.0f);
				tile->gridHeight = round(image->height / 94.0f);
				tile->mapWidth = tile->gridWidth * 8 * UNIT_SIZE;
				tile->mapHeight = tile->gridHeight * 8 * UNIT_SIZE;
			} else {
				tile->mapX = cl.mapData->mapMin[0];
				tile->mapY = cl.mapData->mapMin[1];
				tile->mapWidth = cl.mapData->mapMax[0] - cl.mapData->mapMin[0];
				tile->mapHeight = cl.mapData->mapMax[1] - cl.mapData->mapMin[1];
			}
		}
		if (tile->isTile) {
			tile->mapY = cl.mapData->mapMax[1] - tile->mapY - tile->mapHeight;
		}
	}

	/* center tiles into the minMap/maxMap */
	Vector2Copy(cl.mapData->mapMax, min);
	Vector2Copy(cl.mapData->mapMin, max);
	for (j = 0; j < radar.numImages; j++) {
		hudRadarImage_t *tile = &radar.images[j];
		if (tile->mapX < min[0])
			min[0] = tile->mapX;
		if (tile->mapY < min[1])
			min[1] = tile->mapY;
		if (tile->mapX + tile->mapWidth > max[0])
			max[0] = tile->mapX + tile->mapWidth;
		if (tile->mapY + tile->mapHeight > max[1])
			max[1] = tile->mapY + tile->mapHeight;
	}
	/* compute translation */
	min[0] = cl.mapData->mapMin[0] + (cl.mapData->mapMax[0] - cl.mapData->mapMin[0] - (max[0] - min[0])) * 0.5 - min[0];
	min[1] = cl.mapData->mapMin[1] + (cl.mapData->mapMax[1] - cl.mapData->mapMin[1] - (max[1] - min[1])) * 0.5 - min[1];
	for (j = 0; j < radar.numImages; j++) {
		hudRadarImage_t *tile = &radar.images[j];
		tile->mapX += min[0];
		tile->mapY += min[1];
	}

	/* get the three points of the triangle */
	VectorSubtract(cl.mapData->mapMin, offset, radar.a);
	VectorAdd(cl.mapData->mapMax, offset, radar.c);
	VectorSet(radar.b, radar.c[0], radar.a[1], 0);

	distAB = (Vector2Dist(radar.a, radar.b) / UNIT_SIZE);
	distBC = (Vector2Dist(radar.b, radar.c) / UNIT_SIZE);

	UI_GetRadarWidth(node, gridSize);

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
			image->y = radar.h - (image->gridY - radar.gridMin[1]) * gridFactorY - image->height;
		}
	}

	/* now align the screen coordinates like it's given by the node */
	radar.x -= (radar.w / 2);
	radar.y -= (radar.h / 2);
}

/*=========================================
 DRAW FUNCTIONS
=========================================*/

static void UI_RadarNodeGetActorColor (const le_t *le, vec4_t color)
{
	const int actorLevel = le->pos[2];
	Vector4Set(color, 0, 1, 0, 1);

	/* use different alpha values for different levels */
	if (actorLevel < cl_worldlevel->integer)
		color[3] = 0.5;
	else if (actorLevel > cl_worldlevel->integer)
		color[3] = 0.3;

	/* use different colors for different teams */
	if (LE_IsCivilian(le)) {
		color[0] = 1;
	} else if (le->team != cls.team) {
		color[1] = 0;
		color[0] = 1;
	}

	/* show dead actors in full black */
	if (LE_IsDead(le)) {
		Vector4Set(color, 0, 0, 0, 0.3);
	}
}

static void UI_RadarNodeDrawArrays (const vec4_t color, vec2_t coords[4], vec2_t vertices[4], const image_t *image)
{
	R_Color(color);
	R_DrawImageArray((const vec2_t *)coords, (const vec2_t *)vertices, image);
	R_Color(NULL);
}

static void UI_RadarNodeDrawActor (const le_t *le, const vec3_t pos)
{
	vec2_t coords[4];
	vec2_t vertices[4];
	int i;
	const float size = 10;
	const int tileSize = 28;
	int tilePos = 4;
	const image_t *image;
	vec4_t color;
	const float pov = directionAngles[le->angle] * torad + M_PI;

	image = UI_LoadImage("ui/radar");
	if (image == NULL)
		return;

	/* draw FOV */
	if (le->selected) {
		vertices[0][0] = - size * 4;
		vertices[0][1] = + 0;
		vertices[1][0] = + size * 4;
		vertices[1][1] = + 0;
		vertices[2][0] = + size * 4;
		vertices[2][1] = - size * 4;
		vertices[3][0] = - size * 4;
		vertices[3][1] = - size * 4;
		coords[0][0] = (7) / 128.0f;
		coords[0][1] = (37 + 63) / 128.0f;
		coords[1][0] = (7 + 114) / 128.0f;
		coords[1][1] = (37 + 63) / 128.0f;
		coords[2][0] = (7 + 114) / 128.0f;
		coords[2][1] = (37) / 128.0f;
		coords[3][0] = (7) / 128.0f;
		coords[3][1] = (37) / 128.0f;

		/* affine transformation */
		for (i = 0; i < 4; i++) {
			const float dx = vertices[i][0];
			const float dy = vertices[i][1];
			vertices[i][0] = pos[0] + dx * sin(pov) + dy * cos(pov);
			vertices[i][1] = pos[1] + dx * cos(pov) - dy * sin(pov);
		}

		UI_RadarNodeGetActorColor(le, color);
		Vector4Set(color, 1, 1, 1, color[3] * 0.75);
		UI_RadarNodeDrawArrays(color, coords, vertices, image);
	}

	if (LE_IsDead(le))
		tilePos = 4;
	else if (le->selected)
		tilePos = 66;
	else
		tilePos = 36;

	/* a 0,0 centered square */
	vertices[0][0] = - size;
	vertices[0][1] = + size;
	vertices[1][0] = + size;
	vertices[1][1] = + size;
	vertices[2][0] = + size;
	vertices[2][1] = - size;
	vertices[3][0] = - size;
	vertices[3][1] = - size;
	coords[0][0] = (tilePos) / 128.0f;
	coords[0][1] = (5 + tileSize) / 128.0f;
	coords[1][0] = (tilePos + tileSize) / 128.0f;
	coords[1][1] = (5 + tileSize) / 128.0f;
	coords[2][0] = (tilePos + tileSize) / 128.0f;
	coords[2][1] = (5) / 128.0f;
	coords[3][0] = (tilePos) / 128.0f;
	coords[3][1] = (5) / 128.0f;

	/* affine transformation */
	for (i = 0; i < 4; i++) {
		const float dx = vertices[i][0];
		const float dy = vertices[i][1];
		vertices[i][0] = pos[0] + dx * sin(pov) + dy * cos(pov);
		vertices[i][1] = pos[1] + dx * cos(pov) - dy * sin(pov);
	}

	UI_RadarNodeGetActorColor(le, color);
	UI_RadarNodeDrawArrays(color, coords, vertices, image);
}

/*#define RADARSIZE_DEBUG*/

/**
 * @sa CMod_GetMapSize
 * @note we only need to handle the 2d plane and can ignore the z level
 * @param[in] node Node description of the radar
 */
static void UI_RadarNodeDraw (uiNode_t *node)
{
	le_t *le;
	int i;
	vec2_t pos;
	vec2_t screenPos;
#ifdef RADARSIZE_DEBUG
	int textposy = 40;
	static const vec4_t red = {1, 0, 0, 0.5};
#endif

	static const vec4_t backgroundColor = {0.0, 0.0, 0.0, 1};
	const float mapWidth = cl.mapData->mapMax[0] - cl.mapData->mapMin[0];
	const float mapHeight = cl.mapData->mapMax[1] - cl.mapData->mapMin[1];

	/** @todo use the same coef for x and y */
	const float mapCoefX = (float) node->size[0] / (float) mapWidth;
	const float mapCoefY = (float) node->size[1] / (float) mapHeight;

	if (cls.state != ca_active)
		return;

	UI_GetNodeAbsPos(node, pos);
	UI_GetNodeScreenPos(node, screenPos);
	R_CleanupDepthBuffer(pos[0], pos[1], node->size[0], node->size[1]);
	UI_DrawFill(pos[0], pos[1], mapWidth * mapCoefX, mapHeight * mapCoefY, backgroundColor);
#ifndef RADARSIZE_DEBUG
	R_PushClipRect(screenPos[0], screenPos[1], node->size[0], node->size[1]);
#endif

	/* the cl struct is wiped with every new map */
	if (!cl.radarInited) {
		UI_InitRadar(node);
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
		UI_DrawStringInBox("f_small", 0, 50, textposy, 500, 25, va("%fx%f %fx%f map", mapMin[0], mapMin[1], mapMax[0], mapMax[1]), LONGLINES_PRETTYCHOP);
		textposy += 25;
		UI_DrawStringInBox("f_small", 0, 50, textposy, 500, 25, va("%fx%f map", mapWidth, mapHeight), LONGLINES_PRETTYCHOP);
		textposy += 25;
#endif

	/* draw background */
	for (i = 0; i < radar.numImages; i++) {
		vec2_t imagePos;
		hudRadarImage_t *tile = &radar.images[i];
		int maxlevel = cl_worldlevel->integer;

		/* check the max level value for this map tile */
		if (maxlevel >= tile->maxlevel)
			maxlevel = tile->maxlevel - 1;
		assert(tile->path[maxlevel]);
		imagePos[0] = radar.x + mapCoefX * (tile->mapX - cl.mapData->mapMin[0]);
		imagePos[1] = radar.y + mapCoefY * (tile->mapY - cl.mapData->mapMin[1]);

		UI_DrawNormImageByName(qfalse, imagePos[0], imagePos[1],
				mapCoefX * tile->mapWidth, mapCoefY * tile->mapHeight,
				0, 0, 0, 0, tile->path[maxlevel]);
#ifdef RADARSIZE_DEBUG
		UI_DrawStringInBox("f_small", 0, 50, textposy, 500, 25, va("%dx%d %dx%d %s", tile->x, tile->y, tile->width, tile->height, tile->path[maxlevel]), LONGLINES_PRETTYCHOP);
		textposy += 25;
		UI_DrawStringInBox("f_small", 0, imagePos[0], imagePos[1],
				500, 25, va("%dx%d", tile->gridX, tile->gridY), LONGLINES_PRETTYCHOP);
#endif
	}

#ifdef RADARSIZE_DEBUG
	UI_DrawFill(pos[0], pos[1], 100.0f * mapCoefX, 100.0f * mapCoefY, red);
	UI_DrawFill(pos[0], pos[1], UNIT_SIZE * mapCoefX, UNIT_SIZE * mapCoefY, red);
#endif

	le = NULL;
	while ((le = LE_GetNextInUse(le))) {
		vec3_t itempos;
		if (le->invis)
			continue;

		/* convert to radar area coordinates */
		itempos[0] = pos[0] + (le->origin[0] - cl.mapData->mapMin[0]) * mapCoefX;
		itempos[1] = pos[1] + (mapHeight - (le->origin[1] - cl.mapData->mapMin[1])) * mapCoefY;

		switch (le->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
			UI_RadarNodeDrawActor(le, itempos);
			break;
		case ET_ITEM:
			break;
		default:
			break;
		}
#ifdef RADARSIZE_DEBUG
		UI_DrawStringInBox("f_small", 0, 50, textposy, 500, 25, va("%fx%f %dx%d actor", le->origin[0], le->origin[1], le->pos[0], le->pos[1]), LONGLINES_PRETTYCHOP);
		textposy += 25;
		UI_DrawFill(itempos[0], itempos[1], UNIT_SIZE * mapCoefX, 1, red);
		UI_DrawFill(itempos[0], itempos[1], 1, UNIT_SIZE * mapCoefY, red);
#endif
	}

#ifndef RADARSIZE_DEBUG
	R_PopClipRect();
#endif
}

/**
 * @brief Called when the node is captured by the mouse
 */
static void UI_RadarNodeCapturedMouseMove (uiNode_t *node, int x, int y)
{
	const float mapWidth = cl.mapData->mapMax[0] - cl.mapData->mapMin[0];
	const float mapHeight = cl.mapData->mapMax[1] - cl.mapData->mapMin[1];
	const float mapCoefX = (float) node->size[0] / (float) mapWidth;
	const float mapCoefY = (float) node->size[1] / (float) mapHeight;
	vec3_t pos;

	/* from absolute to relative to node */
	UI_NodeAbsoluteToRelativePos(node, &x, &y);

	/* from node to map */
	pos[0] = cl.mapData->mapMin[0] + x / mapCoefX;
	pos[1] = cl.mapData->mapMax[1] - y / mapCoefY;
	pos[2] = 0;

	VectorCopy(pos, cl.cam.origin);
}

static void UI_RadarNodeMouseDown (uiNode_t *node, int x, int y, int button)
{
	if (node->disabled)
		return;

	if (button == K_MOUSE1) {
		UI_SetMouseCapture(node);
		UI_RadarNodeCapturedMouseMove(node, x, y);
	}
}

static void UI_RadarNodeMouseUp (uiNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1)
		UI_MouseRelease();
}

static void UI_RadarWindowedClosed (uiNode_t *node)
{
}

static void UI_RadarWindowedOpened (uiNode_t *node, linkedList_t *params)
{
}

void UI_RegisterRadarNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "radar";
	behaviour->draw = UI_RadarNodeDraw;
	behaviour->windowOpened = UI_RadarWindowedOpened;
	behaviour->windowClosed = UI_RadarWindowedClosed;
	behaviour->mouseDown = UI_RadarNodeMouseDown;
	behaviour->mouseUp = UI_RadarNodeMouseUp;
	behaviour->capturedMouseMove = UI_RadarNodeCapturedMouseMove;
}
