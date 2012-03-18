/**
 * @file ui_node_map.c
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

#include "../ui_nodes.h"
#include "../ui_input.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_actions.h"
#include "ui_node_abstractnode.h"
#include "ui_node_map.h"

#include "../../client.h"
#include "../../cgame/campaign/cp_campaign.h"
#include "../../cgame/campaign/cp_map.h"
#include "../../renderer/r_draw.h"

#define EXTRADATA_TYPE mapExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static void UI_MapNodeDraw (uiNode_t *node)
{
	if (CP_IsRunning()) {
		vec2_t screenPos;

		/* Draw geoscape */
		UI_GetNodeScreenPos(node, screenPos);
		R_PushClipRect(screenPos[0], screenPos[1], node->size[0], node->size[1]);
		MAP_DrawMap(node, ccs.curCampaign);
		R_PopClipRect();
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

static void UI_MapNodeCapturedMouseMove (uiNode_t *node, int x, int y)
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
		break;
	}
	oldMousePosX = x;
	oldMousePosY = y;
}

static void UI_MapNodeStartMouseShifting (uiNode_t *node, int x, int y)
{
	UI_SetMouseCapture(node);
	if (!cl_3dmap->integer)
		mode = MODE_SHIFT2DMAP;
	else
		mode = MODE_SHIFT3DMAP;
	MAP_StopSmoothMovement();
	oldMousePosX = x;
	oldMousePosY = y;
}

static void UI_MapNodeMouseDown (uiNode_t *node, int x, int y, int button)
{
	/* finish the last drag before */
	if (mode != MODE_NULL)
		return;

	switch (button) {
	case K_MOUSE1:
		if (!MAP_MapClick(node, x, y))
			UI_MapNodeStartMouseShifting(node, x, y);
		break;
	case K_MOUSE2:
		UI_MapNodeStartMouseShifting(node, x, y);
		break;
	case K_MOUSE3:
		UI_SetMouseCapture(node);
		mode = MODE_ZOOMMAP;
		oldMousePosX = x;
		oldMousePosY = y;
		break;
	}
}

static void UI_MapNodeMouseUp (uiNode_t *node, int x, int y, int button)
{
	if (mode != MODE_NULL) {
		UI_MouseRelease();
		mode = MODE_NULL;
	}
}

/**
 * @brief Called when the node have lost the captured node
 * We clean cached data
 */
static void UI_MapNodeCapturedMouseLost (uiNode_t *node)
{
	mode = MODE_NULL;
}

static void UI_MapNodeZoom (uiNode_t *node, qboolean out)
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

static qboolean UI_MapNodeMouseWheel (uiNode_t *node, int deltaX, int deltaY)
{
	qboolean down = deltaY > 0;
	if (deltaY == 0)
		return qfalse;
	UI_MapNodeZoom(node, down);
	return qtrue;
}

static void UI_MapNodeZoomIn (uiNode_t *node, const uiCallContext_t *context)
{
	UI_MapNodeZoom(node, qfalse);
}

static void UI_MapNodeZoomOut (uiNode_t *node, const uiCallContext_t *context)
{
	UI_MapNodeZoom(node, qtrue);
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void UI_MapNodeLoading (uiNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
}

void UI_RegisterMapNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "map";
	behaviour->draw = UI_MapNodeDraw;
	behaviour->mouseDown = UI_MapNodeMouseDown;
	behaviour->mouseUp = UI_MapNodeMouseUp;
	behaviour->capturedMouseMove = UI_MapNodeCapturedMouseMove;
	behaviour->capturedMouseLost = UI_MapNodeCapturedMouseLost;
	behaviour->scroll = UI_MapNodeMouseWheel;
	behaviour->loading = UI_MapNodeLoading;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Use a right padding. */
	UI_RegisterExtradataNodeProperty(behaviour, "padding-right", V_FLOAT, EXTRADATA_TYPE, paddingRight);
	/* Call it to zoom out of the map */
	UI_RegisterNodeMethod(behaviour, "zoomin", UI_MapNodeZoomIn);
	/* Call it to zoom into the map */
	UI_RegisterNodeMethod(behaviour, "zoomout", UI_MapNodeZoomOut);
}
