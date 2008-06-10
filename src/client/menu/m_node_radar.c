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

typedef struct hudRadar_s {
	/** The dimension of the icons on the radar map */
	float gridHeight, gridWidth;
	char base[MAX_QPATH];
	/** Holds the list of images for the hud radar (more than one in case of
	 * random map assembly) and the list of offsets (also only in case of rma)
	 * @note Entries: mapname (string) x coord (float) y coord (float) maxlevel (int) */
	linkedList_t *imageList;
} hudRadar_t;

static hudRadar_t radar;

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
	char name[MAX_VAR];

	LIST_Delete(&radar.imageList);
	memset(&radar, 0, sizeof(radar));

	/* load tiles */
	while (tiles) {
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
		LIST_AddString(&radar.imageList, name);

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
			LIST_Add(&radar.imageList, (byte *)&sh[0], sizeof(float)); /* x */
			LIST_Add(&radar.imageList, (byte *)&sh[1], sizeof(float)); /* y */
			LIST_Add(&radar.imageList, (const byte *)&map_maxlevel, sizeof(int));
		} else {
			/* load only a single tile, if no positions are specified */
			const float coord = 0.0f;
			LIST_Add(&radar.imageList, (const byte *)&coord, sizeof(float)); /* x */
			LIST_Add(&radar.imageList, (const byte *)&coord, sizeof(float)); /* y */
			LIST_Add(&radar.imageList, (const byte *)&map_maxlevel, sizeof(int));
			return;
		}
	}
}

static void MN_InitRadar (void)
{
	int i;

	MN_BuildRadarImageList(cl.configstrings[CS_TILES], cl.configstrings[CS_POSITIONS]);
	/* only check once per map whether all the needed images exist */
	for (i = 1; i < map_maxlevel + 1; i++) {
		/* map_mins, map_maxs */
		linkedList_t *list = radar.imageList;
		while (list) {
			const char *mapname = (const char *)list->data;
			char imageName[MAX_QPATH];

			/* skip x and y and get the map max level */
			list = list->next; /* x */
			list = list->next; /* y */
			list = list->next; /* map level */

			Com_sprintf(imageName, sizeof(imageName), "pics/radars/%s_%i.tga", mapname, i);
			if (FS_CheckFile(imageName) <= 0) {
				if (i == 1) {
					/* there should be at least one level */
					Com_Printf("No radar images for map: '%s' (%s)\n", mapname, imageName);
					cl.skipRadarNodes = qtrue;
					return;
				} else {
					int tmpLevel = i - 1;
					/* update the max map level entry */
					memcpy(list->data, &tmpLevel, sizeof(int));
				}
			}
			list = list->next;
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
	/** radar image dimensions */
	int w, h;
	int i;
	/** three vectors of the triangle, lower left (a), lower right (b), upper right (c) */
	vec3_t a, b, c;
	const vec3_t offset = {MAP_SIZE_OFFSET, MAP_SIZE_OFFSET, MAP_SIZE_OFFSET};
	char imageName[MAX_QPATH];
	vec3_t pos;
	vec2_t screenPos;
	linkedList_t *list;

	/* the cl struct is wiped with every new map */
	if (!cl.radarInited)
		MN_InitRadar();

	if (VectorNotEmpty(node->bgcolor))
		R_DrawFill(0, 0, VID_NORM_WIDTH, VID_NORM_HEIGHT, ALIGN_UL, node->bgcolor);

	w = h = 0;
	list = radar.imageList;
	while (list) {
		float x, y;
		int picWidth, picHeight;
		int maxlevel = cl_worldlevel->integer + 1;
		const char *mapname = (const char *)list->data;
		list = list->next;
		/* FIXME for RMA the coordinates are not yet done - they have to
		 * be converted (most likely) */
		x = *(float *)list->data;
		list = list->next;
		y = *(float *)list->data;
		list = list->next;

		/* check the max level value for this map tile */
		if (maxlevel >= *(int *)list->data)
			maxlevel = *(int *)list->data;

		Com_sprintf(imageName, sizeof(imageName), "radars/%s_%i", mapname, maxlevel);

		R_DrawGetPicSize(&picWidth, &picHeight, imageName);
		if (picWidth > VID_NORM_WIDTH || picHeight > VID_NORM_HEIGHT) {
			Com_Printf("Image '%s' is too big\n", imageName);
		} else {
			w = max(w, x + picWidth);
			h = max(h, y + picHeight);
			R_DrawNormPic(node->pos[0] + x, node->pos[1] + y, 0, 0, 0, 0, 0, 0, node->align, node->blend, imageName);
		}
		list = list->next;
	}

	VectorCopy(node->pos, screenPos);
	if (node->align > 0 && node->align < ALIGN_LAST) {
		switch (node->align % 3) {
		/* center */
		case 1:
			screenPos[0] -= (w / 2);
			break;
		/* right */
		case 2:
			screenPos[0] -= w;
			break;
		}
		switch (node->align / 3) {
		case 1:
			screenPos[1] -= (h / 2);
			break;
		case 2:
			screenPos[1] -= h;
			break;
		}
	}

	/* get the three points of the triangle */
	VectorSubtract(map_min, offset, a);
	VectorAdd(map_max, offset, c);
	VectorSet(b, c[0], a[1], 0);

	/* get the dimensions for one grid field on the radar map */
	radar.gridWidth = w / (Vector2Dist(a, b) / UNIT_SIZE);
	radar.gridHeight = h / (Vector2Dist(b, c) / UNIT_SIZE);

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse || le->invis)
			continue;

		VectorCopy(le->pos, pos);

		/* convert to radar area coordinates */
		pos[0] -= GRID_WIDTH + (a[0] / UNIT_SIZE);
		/* FIXME: Why do we need the -UNIT_SIZE for the y coordinates? */
		pos[1] -= GRID_WIDTH + ((a[1] - UNIT_SIZE) / UNIT_SIZE);

		switch (le->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
		{
			vec4_t color = {0, 1, 0, 1};
			const int actorLevel = le->pos[2];
			float actorDirection = 0.0f;
			int verts[4];
			/* relative to screen */
			const int x = (screenPos[0] + pos[0] * radar.gridWidth + radar.gridWidth / 2) * viddef.rx;
			const int y = (screenPos[1] + (h - pos[1] * radar.gridHeight) + radar.gridWidth / 2) * viddef.ry;

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
			const int x = (screenPos[0] + pos[0] * radar.gridWidth) * viddef.rx;
			const int y = (screenPos[1] + (h - pos[1] * radar.gridHeight)) * viddef.ry;
			R_DrawFill(x, y, radar.gridWidth, radar.gridHeight, ALIGN_UL, color);
			break;
		}
		}
	}
}
