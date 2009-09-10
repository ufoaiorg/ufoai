/**
 * @file m_node_map.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "../m_nodes.h"
#include "../m_input.h"
#include "../m_parse.h"
#include "m_node_abstractnode.h"
#include "m_node_map.h"

#include "../../client.h"
#include "../../cl_game.h"
#include "../../campaign/cp_campaign.h"
#include "../../campaign/cp_map.h"
#include "../../renderer/r_draw.h"

static void MN_MapNodeDraw (menuNode_t *node)
{
	if (GAME_CP_IsRunning()) {
		vec2_t pos;
		/* don't run the campaign in console mode */
		if (cls.keyDest != key_console)
			CL_CampaignRun();	/* advance time */

		MN_GetNodeAbsPos(node, pos);

		/* Draw geoscape */
		R_BeginClipRect(pos[0], pos[1], node->size[0], node->size[1]);
		MAP_DrawMap(node);
		R_EndClipRect();
	}
}

typedef enum {
	MODE_NULL,
	MODE_SHIFT2DMAP,
	MODE_SHIFT3DMAP,
	MODE_ZOOMMAP,
} mapDragMode_t;

static int oldMousePosX = 0;
static int oldMousePosY = 0;
static mapDragMode_t mode = MODE_NULL;

static void MN_MapNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	switch (mode) {
	case MODE_SHIFT2DMAP:
	{
		int i;
		const float zoom = 0.5 / ccs.zoom;
		/* shift the map */
		ccs.center[0] -= (float) (mousePosX - oldMousePosX) / (ccs.mapSize[0] * ccs.zoom);
		ccs.center[1] -= (float) (mousePosY - oldMousePosY) / (ccs.mapSize[1] * ccs.zoom);
		for (i = 0; i < 2; i++) {
			/* clamp to min/max values */
			while (ccs.center[i] < 0.0)
				ccs.center[i] += 1.0;
			while (ccs.center[i] > 1.0)
				ccs.center[i] -= 1.0;
		}
		if (ccs.center[1] < zoom)
			ccs.center[1] = zoom;
		if (ccs.center[1] > 1.0 - zoom)
			ccs.center[1] = 1.0 - zoom;
		break;
	}

	case MODE_SHIFT3DMAP:
		/* rotate a model */
		ccs.angles[PITCH] += ROTATE_SPEED * (mousePosX - oldMousePosX) / ccs.zoom;
		ccs.angles[YAW] -= ROTATE_SPEED * (mousePosY - oldMousePosY) / ccs.zoom;

		/* clamp the angles */
		while (ccs.angles[YAW] > 0.0)
			ccs.angles[YAW] = 0.0;
		while (ccs.angles[YAW] < -180.0)
			ccs.angles[YAW] = -180.0;

		while (ccs.angles[PITCH] > 180.0)
			ccs.angles[PITCH] -= 360.0;
		while (ccs.angles[PITCH] < -180.0)
			ccs.angles[PITCH] += 360.0;
		break;
	case MODE_ZOOMMAP:
	{
		const float zoom = 0.5 / ccs.zoom;
		/* zoom the map */
		ccs.zoom *= pow(0.995, mousePosY - oldMousePosY);
		if (ccs.zoom < cl_mapzoommin->value)
			ccs.zoom = cl_mapzoommin->value;
		else if (ccs.zoom > cl_mapzoommax->value)
			ccs.zoom = cl_mapzoommax->value;

		if (ccs.center[1] < zoom)
			ccs.center[1] = zoom;
		if (ccs.center[1] > 1.0 - zoom)
			ccs.center[1] = 1.0 - zoom;
		break;
	}
	default:
		assert(qfalse);
	}
	oldMousePosX = x;
	oldMousePosY = y;
}

static void MN_MapNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	/* finish the last drag before */
	if (mode != MODE_NULL)
		return;

	switch (button) {
	case K_MOUSE2:
		MN_SetMouseCapture(node);
		if (!cl_3dmap->integer)
			mode = MODE_SHIFT2DMAP;
		else
			mode = MODE_SHIFT3DMAP;
		MAP_StopSmoothMovement();
		oldMousePosX = x;
		oldMousePosY = y;
		break;
	case K_MOUSE3:
		MN_SetMouseCapture(node);
		mode = MODE_ZOOMMAP;
		oldMousePosX = x;
		oldMousePosY = y;
		break;
	}
}

static void MN_MapNodeMouseUp (menuNode_t *node, int x, int y, int button)
{
	if (mode == MODE_NULL)
		return;

	switch (button) {
	case K_MOUSE2:
		if (mode == MODE_SHIFT3DMAP || mode == MODE_SHIFT2DMAP) {
			MN_MouseRelease();
		}
		break;
	case K_MOUSE3:
		if (mode == MODE_ZOOMMAP) {
			MN_MouseRelease();
		}
		break;
	}
}

/**
 * @brief Called when the node have lost the captured node
 * We clean cached data
 */
static void MN_MapNodeCapturedMouseLost (menuNode_t *node)
{
	mode = MODE_NULL;
}

static void MN_MapNodeZoom (menuNode_t *node, qboolean out)
{
	ccs.zoom *= pow(0.995, (out ? 10: -10));
	if (ccs.zoom < cl_mapzoommin->value)
		ccs.zoom = cl_mapzoommin->value;
	else if (ccs.zoom > cl_mapzoommax->value)
		ccs.zoom = cl_mapzoommax->value;

	if (!cl_3dmap->integer) {
		if (ccs.center[1] < 0.5 / ccs.zoom)
			ccs.center[1] = 0.5 / ccs.zoom;
		if (ccs.center[1] > 1.0 - 0.5 / ccs.zoom)
			ccs.center[1] = 1.0 - 0.5 / ccs.zoom;
	}
	MAP_StopSmoothMovement();
}

static void MN_MapNodeMouseWheel (menuNode_t *node, qboolean down, int x, int y)
{
	MN_MapNodeZoom(node, down);
}

static void MN_MapNodeZoomIn (menuNode_t *node)
{
	MN_MapNodeZoom(node, qfalse);
}

static void MN_MapNodeZoomOut (menuNode_t *node)
{
	MN_MapNodeZoom(node, qtrue);
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void MN_MapNodeLoading (menuNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
}


static const value_t properties[] = {
	/* Use a right padding. */
	{"padding-right", V_FLOAT, offsetof(menuNode_t, extraData1), MEMBER_SIZEOF(menuNode_t, extraData1)},
	/* Call it to zoom out of the map */
	{"zoomin", V_UI_NODEMETHOD, ((size_t) MN_MapNodeZoomIn), 0},
	/* Call it to zoom into the map */
	{"zoomout", V_UI_NODEMETHOD, ((size_t) MN_MapNodeZoomOut), 0},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterMapNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "map";
	behaviour->properties = properties;
	behaviour->draw = MN_MapNodeDraw;
	behaviour->leftClick = MAP_MapClick;
	behaviour->mouseDown = MN_MapNodeMouseDown;
	behaviour->mouseUp = MN_MapNodeMouseUp;
	behaviour->capturedMouseMove = MN_MapNodeCapturedMouseMove;
	behaviour->capturedMouseLost = MN_MapNodeCapturedMouseLost;
	behaviour->mouseWheel = MN_MapNodeMouseWheel;
	behaviour->loading = MN_MapNodeLoading;
}
