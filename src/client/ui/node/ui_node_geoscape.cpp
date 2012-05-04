/**
 * @file
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
#include "ui_node_geoscape.h"

#include "../../client.h"
#include "../../cgame/campaign/cp_campaign.h"
#include "../../cgame/campaign/cp_map.h"
#include "../../renderer/r_draw.h"

#define EXTRADATA_TYPE mapExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

/**
 * Status of the node
 */
enum mapDragMode_t {
	/**
	 * No interaction
	 */
	MODE_NULL,
	/**
	 * Mouse captured to move the 2D geoscape
	 */
	MODE_SHIFT2DMAP,
	/**
	 * Mouse captured to move the 3D geoscape
	 */
	MODE_SHIFT3DMAP,
	/**
	 * Mouse captured to zoom on the geoscape
	 */
	MODE_ZOOMMAP
};

/**
 * Old X-location of the mouse, when the node is captured.
 * It is related to a captured node (only one at a time), that why it is global.
 */
static int oldMousePosX = 0;

/**
 * Old y-location of the mouse, when the node is captured.
 * It is related to a captured node (only one at a time), that why it is global.
 */
static int oldMousePosY = 0;

/**
 * Status of the node.
 * It is related to a captured node (only one at a time), that why it is global.
 */
static mapDragMode_t mode = MODE_NULL;

void uiGeoscapeNode::draw (uiNode_t *node)
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

void uiGeoscapeNode::onCapturedMouseMove (uiNode_t *node, int x, int y)
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
		assert(false);
		break;
	}
	oldMousePosX = x;
	oldMousePosY = y;
}

void uiGeoscapeNode::startMouseShifting (uiNode_t *node, int x, int y)
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

void uiGeoscapeNode::onLeftClick(uiNode_t* node, int x, int y)
{
	if (mode != MODE_NULL)
		return;
	MAP_MapClick(node, x, y);
}

bool uiGeoscapeNode::onStartDragging(uiNode_t* node, int startX, int startY, int x, int y, int button)
{
	switch (button) {
	case K_MOUSE1:
	case K_MOUSE2:
		startMouseShifting(node, startX, startY);
		return true;
	case K_MOUSE3:
		UI_SetMouseCapture(node);
		mode = MODE_ZOOMMAP;
		oldMousePosX = startX;
		oldMousePosY = startY;
		return true;
	}
	return false;
}

void uiGeoscapeNode::onMouseUp (uiNode_t *node, int x, int y, int button)
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
void uiGeoscapeNode::onCapturedMouseLost (uiNode_t *node)
{
	mode = MODE_NULL;
}

/**
 * Zoom on the node
 * @todo it shound use an int param for smooth zoom
 */
void uiGeoscapeNode::zoom (uiNode_t *node, bool out)
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

bool uiGeoscapeNode::onScroll (uiNode_t *node, int deltaX, int deltaY)
{
	bool down = deltaY > 0;
	if (deltaY == 0)
		return false;
	zoom(node, down);
	return true;
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
void uiGeoscapeNode::onLoading (uiNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
}

static void UI_GeoscapeNodeZoomIn (uiNode_t *node, const uiCallContext_t *context)
{
	uiGeoscapeNode* m = static_cast<uiGeoscapeNode*>(node->behaviour->manager);
	m->zoom(node, false);
}

static void UI_GeoscapeNodeZoomOut (uiNode_t *node, const uiCallContext_t *context)
{
	uiGeoscapeNode* m = static_cast<uiGeoscapeNode*>(node->behaviour->manager);
	m->zoom(node, true);
}

void UI_RegisterGeoscapeNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "geoscape";
	behaviour->manager = new uiGeoscapeNode();
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Use a right padding. */
	UI_RegisterExtradataNodeProperty(behaviour, "padding-right", V_FLOAT, EXTRADATA_TYPE, paddingRight);
	/* Call it to zoom out of the map */
	UI_RegisterNodeMethod(behaviour, "zoomin", UI_GeoscapeNodeZoomIn);
	/* Call it to zoom into the map */
	UI_RegisterNodeMethod(behaviour, "zoomout", UI_GeoscapeNodeZoomOut);
}
