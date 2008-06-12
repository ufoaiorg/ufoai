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

#include "../client.h"
#include "../cl_view.h"
#include "../cl_actor.h"
#include "m_main.h"
#include "m_font.h"

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
 */
static void MN_BuildRadarImageList (const char *tiles, const char *pos)
{
	/* load tiles */
	while (tiles) {
		char name[MAX_VAR];
		hudRadarImage_t *image;
		/* get tile name */
		const char *token = COM_Parse(&tiles);
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
		image->name = Mem_PoolStrDup(name, cl_localPool, CL_TAG_NONE);

		if (pos && pos[0]) {
			int i;
			vec3_t sh;
			/* get grid position and add a tile */
			for (i = 0; i < 3; i++) {
				token = COM_Parse(&pos);
				if (!pos)
					Com_Error(ERR_DROP, "R_ModBeginLoading: invalid positions\n");
				sh[i] = atoi(token);
			}
			image->gridX = sh[0];
			image->gridY = sh[1];

			if (radar.gridMin[0] > sh[0])
				radar.gridMin[0] = sh[0];
			if (radar.gridMin[1] > sh[1])
				radar.gridMin[1] = sh[1];

			if (radar.gridMax[0] < sh[0])
				radar.gridMax[0] = sh[0];
			if (radar.gridMax[1] < sh[1])
				radar.gridMax[1] = sh[1];
		} else {
			/* load only a single tile, if no positions are specified */
			return;
		}
	}
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

	MN_FreeRadarImages();
	MN_BuildRadarImageList(cl.configstrings[CS_TILES], cl.configstrings[CS_POSITIONS]);

	radar.x = node->pos[0];
	radar.y = node->pos[1];

	/* only check once per map whether all the needed images exist */
	for (i = 0; i < PATHFINDING_HEIGHT; i++) {
		/* map_mins, map_maxs */
		for (j = 0; j < radar.numImages; j++) {
			hudRadarImage_t *image = &radar.images[j];
			char imageName[MAX_QPATH];

			Com_sprintf(imageName, sizeof(imageName), "pics/radars/%s_%i.tga", image->name, i + 1);
			if (FS_CheckFile(imageName) <= 0) {
				if (i == 0) {
					/* there should be at least one level */
					Com_Printf("No radar images for map: '%s' (%s)\n", image->name, imageName);
					cl.skipRadarNodes = qtrue;
					return;
				}
			} else {
				Com_sprintf(imageName, sizeof(imageName), "radars/%s_%i", image->name, i + 1);
				image->path[i] = Mem_PoolStrDup(imageName, cl_localPool, CL_TAG_NONE);

				image->maxlevel++;

				R_DrawGetPicSize(&image->w, &image->h, image->path[i]);
				if (image->w > VID_NORM_WIDTH || image->h > VID_NORM_HEIGHT)
					Com_Printf("Image '%s' is too big\n", image->path[i]);
			}
		}
	}

	/* get the three points of the triangle */
	VectorSubtract(map_min, offset, radar.a);
	VectorAdd(map_max, offset, radar.c);
	VectorSet(radar.b, radar.c[0], radar.a[1], 0);

	distAB = (Vector2Dist(radar.a, radar.b) / UNIT_SIZE);
	distBC = (Vector2Dist(radar.b, radar.c) / UNIT_SIZE);

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
		/* FIXME: That doesn't work for e.g. T-like map tiles */
		if (image->gridX == radar.gridMin[0])
			radar.w += image->w;
		if (image->gridY == radar.gridMin[1])
			radar.h += image->h;
	}
	/* we need this filled */
	assert(radar.w);
	assert(radar.h);

	/* get the dimensions for one grid field on the radar map */
	radar.gridWidth = radar.w / distAB;
	radar.gridHeight = radar.h / distBC;

	for (j = 0; j < radar.numImages; j++) {
		hudRadarImage_t *image = &radar.images[j];
		const float radarLength = max(1, abs(radar.gridMax[0] - radar.gridMin[0]));
		const float radarHeight = max(1, abs(radar.gridMax[1] - radar.gridMin[1]));
		/* image grid relations */
		const float gridFactorX = radar.w / radarLength;
		const float gridFactorY = radar.h / radarHeight;
		/* the grids of the map tiles that is shown in the image */
		const int imageGridWidth = image->w / gridFactorX;
		const int imageGridHeight = image->h / gridFactorY;
		/* shift the x and y values according to their grid width/height and
		 * their gridX and gridY position */
		/* FIXME: broken */
		image->x = image->gridX * imageGridWidth;
		image->y = radar.h - (image->gridY * imageGridHeight);
	}

	/* now align the screen coordinates like it's given by the menu node */
	if (node->align > 0 && node->align < ALIGN_LAST) {
		/* x position */
		switch (node->align % 3) {
		/* center */
		case 1:
			radar.x -= (radar.w / 2);
			break;
		/* right */
		case 2:
			radar.x -= radar.w;
			break;
		}
		/* y position */
		switch (node->align / 3) {
		case 1:
			radar.y -= (radar.h / 2);
			break;
		case 2:
			radar.y -= radar.h;
			break;
		}
	}

	cl.radarInited = qtrue;
}

/**
 * @sa CMod_GetMapSize
 * @todo Show frustom view area for actors
 * @note we only need to handle the 2d plane and can ignore the z level
 * @param[in] node The radar menu node (located in the hud menu definitions)
 */
void MN_DrawRadar (menuNode_t *node)
{
	const le_t *le;
	int i;
	vec3_t pos;

	/* the cl struct is wiped with every new map */
	if (!cl.radarInited)
		MN_InitRadar(node);

	if (!cl.radarInited)
		return;

	if (VectorNotEmpty(node->bgcolor))
		R_DrawFill(0, 0, VID_NORM_WIDTH, VID_NORM_HEIGHT, ALIGN_UL, node->bgcolor);

	for (i = 0; i < radar.numImages; i++) {
		hudRadarImage_t *image = &radar.images[i];
		int maxlevel = cl_worldlevel->integer;
		/* check the max level value for this map tile */
		if (maxlevel >= image->maxlevel)
			maxlevel = image->maxlevel - 1;
		assert(image->path[maxlevel]);
		R_DrawNormPic(radar.x + image->x, radar.y + image->y, 0, 0, 0, 0, 0, 0, ALIGN_UL, node->blend, image->path[maxlevel]);
	}

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse || le->invis)
			continue;

		VectorCopy(le->pos, pos);

		/* convert to radar area coordinates */
		pos[0] -= GRID_WIDTH + (radar.a[0] / UNIT_SIZE);
		pos[1] -= GRID_WIDTH + ((radar.a[1] - UNIT_SIZE) / UNIT_SIZE);

		switch (le->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
		{
			vec4_t color = {0, 1, 0, 1};
			const int actorLevel = le->pos[2];
			float actorDirection = 0.0f;
			int verts[4];
			/* used to interpolate movement on the radar */
			/* FIXME: handle direction - needed */
			const int interpolateX = le->origin[0] / UNIT_SIZE;
			const int interpolateY = le->origin[1] / UNIT_SIZE;
			/* relative to screen */
			const int x = (radar.x + interpolateX + pos[0] * radar.gridWidth + radar.gridWidth / 2) * viddef.rx;
			const int y = (radar.y + (radar.h - interpolateY - pos[1] * radar.gridHeight) + radar.gridWidth / 2) * viddef.ry;

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
			if (le->state & STATE_DEAD) {
				Vector4Set(color, 0, 0, 0, 1);
			}

			/* draw direction line */
			actorDirection = dangle[le->dir] * torad;
			verts[0] = x;
			verts[1] = y;
			verts[2] = x + (radar.gridWidth * cos(actorDirection));
			verts[3] = y - (radar.gridWidth * sin(actorDirection));
			R_DrawLine(verts, 5);

			/* draw player circles */
			R_DrawCircle2D(x, y, radar.gridWidth / 2, qtrue, color, 1);
			Vector4Set(color, 0.8, 0.8, 0.8, 1.0);
			/* outline */
			R_DrawCircle2D(x, y, radar.gridWidth / 2, qfalse, color, 2);
			break;
		}
		case ET_ITEM:
		{
			const vec4_t color = {0, 1, 0, 1};
			/* relative to screen */
			const int x = (radar.x + pos[0] * radar.gridWidth) * viddef.rx;
			const int y = (radar.y + (radar.h - pos[1] * radar.gridHeight)) * viddef.ry;
			R_DrawFill(x, y, radar.gridWidth, radar.gridHeight, ALIGN_UL, color);
			break;
		}
		}
	}
}
