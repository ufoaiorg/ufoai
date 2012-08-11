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
#include "../ui_render.h"
#include "ui_node_geoscape.h"

#include "../../cl_shared.h"
#include "../../cgame/cl_game.h"
#include "../../input/cl_input.h"
#include "../../input/cl_keys.h"

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
#define ROTATE_SPEED	0.5
#define GLOBE_ROTATE -90

static cvar_t *cl_3dmap;						/**< 3D geoscape or flat geoscape */
static cvar_t *cl_3dmapAmbient;
static cvar_t *cl_mapzoommax;
static cvar_t *cl_mapzoommin;
static cvar_t *cl_geoscape_overlay;

// FIXME: don't make this static
static uiNode_t *geoscapeNode;

void uiGeoscapeNode::draw (uiNode_t *node)
{
	vec2_t screenPos;

	geoscapeNode = node;
	UI_MAPEXTRADATA(node).flatgeoscape = cl_3dmap->integer == 0;
	UI_MAPEXTRADATA(node).ambientLightFactor = cl_3dmapAmbient->value;
	UI_MAPEXTRADATA(node).mapzoommin = cl_mapzoommin->value;
	UI_MAPEXTRADATA(node).mapzoommax = cl_mapzoommax->value;

	UI_GetNodeAbsPos(node, UI_MAPEXTRADATA(node).mapPos);
	Vector2Copy(node->box.size, UI_MAPEXTRADATA(node).mapSize);
	if (!UI_MAPEXTRADATACONST(node).flatgeoscape) {
		/* remove the left padding */
		UI_MAPEXTRADATA(node).mapSize[0] -= UI_MAPEXTRADATACONST(node).paddingRight;
	}

	/* Draw geoscape */
	UI_GetNodeScreenPos(node, screenPos);
	UI_PushClipRect(screenPos[0], screenPos[1], node->box.size[0], node->box.size[1]);
	GAME_DrawMap(node);
	UI_PopClipRect();
}

void uiGeoscapeNode::onCapturedMouseMove (uiNode_t *node, int x, int y)
{
	switch (mode) {
	case MODE_SHIFT2DMAP:
	{
		int i;
		const float zoom = 0.5 / UI_MAPEXTRADATACONST(node).zoom;
		/* shift the map */
		UI_MAPEXTRADATA(node).center[0] -= (float) (mousePosX - oldMousePosX) / (node->box.size[0] * UI_MAPEXTRADATACONST(node).zoom);
		UI_MAPEXTRADATA(node).center[1] -= (float) (mousePosY - oldMousePosY) / (node->box.size[1] * UI_MAPEXTRADATACONST(node).zoom);
		for (i = 0; i < 2; i++) {
			/* clamp to min/max values */
			while (UI_MAPEXTRADATACONST(node).center[i] < 0.0)
				UI_MAPEXTRADATA(node).center[i] += 1.0;
			while (UI_MAPEXTRADATACONST(node).center[i] > 1.0)
				UI_MAPEXTRADATA(node).center[i] -= 1.0;
		}
		if (UI_MAPEXTRADATACONST(node).center[1] < zoom)
			UI_MAPEXTRADATA(node).center[1] = zoom;
		if (UI_MAPEXTRADATACONST(node).center[1] > 1.0 - zoom)
			UI_MAPEXTRADATA(node).center[1] = 1.0 - zoom;
		break;
	}

	case MODE_SHIFT3DMAP:
		/* rotate a model */
		UI_MAPEXTRADATA(node).angles[PITCH] += ROTATE_SPEED * (mousePosX - oldMousePosX) / UI_MAPEXTRADATACONST(node).zoom;
		UI_MAPEXTRADATA(node).angles[YAW] -= ROTATE_SPEED * (mousePosY - oldMousePosY) / UI_MAPEXTRADATACONST(node).zoom;

		/* clamp the UI_MAPEXTRADATACONST(node).angles */
		while (UI_MAPEXTRADATACONST(node).angles[YAW] > 0.0)
			UI_MAPEXTRADATA(node).angles[YAW] = 0.0;
		while (UI_MAPEXTRADATACONST(node).angles[YAW] < -180.0)
			UI_MAPEXTRADATA(node).angles[YAW] = -180.0;

		while (UI_MAPEXTRADATACONST(node).angles[PITCH] > 180.0)
			UI_MAPEXTRADATA(node).angles[PITCH] -= 360.0;
		while (UI_MAPEXTRADATACONST(node).angles[PITCH] < -180.0)
			UI_MAPEXTRADATA(node).angles[PITCH] += 360.0;
		break;
	case MODE_ZOOMMAP:
	{
		const float zoom = 0.5 / UI_MAPEXTRADATACONST(node).zoom;
		/* zoom the map */
		UI_MAPEXTRADATA(node).zoom *= pow(0.995, mousePosY - oldMousePosY);
		if (UI_MAPEXTRADATACONST(node).zoom < UI_MAPEXTRADATACONST(node).mapzoommin)
			UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).mapzoommin;
		else if (UI_MAPEXTRADATACONST(node).zoom > UI_MAPEXTRADATACONST(node).mapzoommax)
			UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).mapzoommax;

		if (UI_MAPEXTRADATACONST(node).center[1] < zoom)
			UI_MAPEXTRADATA(node).center[1] = zoom;
		if (UI_MAPEXTRADATACONST(node).center[1] > 1.0 - zoom)
			UI_MAPEXTRADATA(node).center[1] = 1.0 - zoom;
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
	if (UI_MAPEXTRADATACONST(node).flatgeoscape)
		mode = MODE_SHIFT2DMAP;
	else
		mode = MODE_SHIFT3DMAP;
	UI_MAPEXTRADATA(node).smoothRotation = false;
	oldMousePosX = x;
	oldMousePosY = y;
}

void uiGeoscapeNode::onLeftClick (uiNode_t* node, int x, int y)
{
	if (mode != MODE_NULL)
		return;
	GAME_MapClick(node, x, y);
}

bool uiGeoscapeNode::onStartDragging (uiNode_t* node, int startX, int startY, int x, int y, int button)
{
	switch (button) {
	case K_MOUSE1:
	case K_MOUSE3:
		startMouseShifting(node, startX, startY);
		return true;
	case K_MOUSE2:
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
 * @todo it should use an int param for smooth zoom
 */
void uiGeoscapeNode::zoom (uiNode_t *node, bool out)
{
	UI_MAPEXTRADATA(node).zoom *= pow(0.995, (out ? 10: -10));
	if (UI_MAPEXTRADATACONST(node).zoom < UI_MAPEXTRADATACONST(node).mapzoommin)
		UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).mapzoommin;
	else if (UI_MAPEXTRADATACONST(node).zoom > UI_MAPEXTRADATACONST(node).mapzoommax)
		UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).mapzoommax;

	if (UI_MAPEXTRADATACONST(node).flatgeoscape) {
		if (UI_MAPEXTRADATACONST(node).center[1] < 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 0.5 / UI_MAPEXTRADATACONST(node).zoom;
		if (UI_MAPEXTRADATACONST(node).center[1] > 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom;
	}
	UI_MAPEXTRADATA(node).smoothRotation = false;
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

	OBJZERO(EXTRADATA(node));
	EXTRADATA(node).angles[YAW] = GLOBE_ROTATE;
	EXTRADATA(node).center[0] = EXTRADATA(node).center[1] = 0.5;
	EXTRADATA(node).zoom = 1.0;
	Vector2Set(EXTRADATA(node).smoothFinal2DGeoscapeCenter, 0.5, 0.5);
	VectorSet(EXTRADATA(node).smoothFinalGlobeAngle, 0, GLOBE_ROTATE, 0);
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

/**
 * @brief Command binding for map zooming
 * @todo convert into node method
 */
static void UI_GeoscapeNodeZoom_f (void)
{
	const char *cmd;
	const float zoomAmount = 50.0f;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <in|out>\n", Cmd_Argv(0));
		return;
	}

	cmd = Cmd_Argv(1);
	uiNode_t *node = geoscapeNode;
	if (!node)
		return;

	switch (cmd[0]) {
	case 'i':
		UI_MAPEXTRADATA(node).smoothFinalZoom = UI_MAPEXTRADATACONST(node).zoom * pow(0.995, -zoomAmount);
		break;
	case 'o':
		UI_MAPEXTRADATA(node).smoothFinalZoom = UI_MAPEXTRADATACONST(node).zoom * pow(0.995, zoomAmount);
		break;
	default:
		Com_Printf("UI_GeoscapeNodeZoom_f: Invalid parameter: %s\n", cmd);
		return;
	}

	if (UI_MAPEXTRADATACONST(node).smoothFinalZoom < UI_MAPEXTRADATACONST(node).mapzoommin)
		UI_MAPEXTRADATA(node).smoothFinalZoom = UI_MAPEXTRADATACONST(node).mapzoommin;
	else if (UI_MAPEXTRADATACONST(node).smoothFinalZoom > UI_MAPEXTRADATACONST(node).mapzoommax)
		UI_MAPEXTRADATA(node).smoothFinalZoom = UI_MAPEXTRADATACONST(node).mapzoommax;

	if (UI_MAPEXTRADATACONST(node).flatgeoscape) {
		UI_MAPEXTRADATA(node).zoom = UI_MAPEXTRADATACONST(node).smoothFinalZoom;
		if (UI_MAPEXTRADATACONST(node).center[1] < 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 0.5 / UI_MAPEXTRADATACONST(node).zoom;
		if (UI_MAPEXTRADATACONST(node).center[1] > 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom;
	} else {
		VectorCopy(UI_MAPEXTRADATACONST(node).angles, UI_MAPEXTRADATA(node).smoothFinalGlobeAngle);
		UI_MAPEXTRADATA(node).smoothDeltaLength = 0;
		UI_MAPEXTRADATA(node).smoothRotation = true;
		UI_MAPEXTRADATA(node).smoothDeltaZoom = fabs(UI_MAPEXTRADATACONST(node).smoothFinalZoom - UI_MAPEXTRADATACONST(node).zoom);
	}
}

/**
 * @brief Command binding for map scrolling
 * @todo convert into node method
 */
static void UI_GeoscapeNodeScroll_f (void)
{
	const char *cmd;
	float scrollX = 0.0f, scrollY = 0.0f;
	const float scrollAmount = 80.0f;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <up|down|left|right>\n", Cmd_Argv(0));
		return;
	}

	cmd = Cmd_Argv(1);

	uiNode_t *node = geoscapeNode;
	if (!node)
		return;

	switch (cmd[0]) {
	case 'l':
		scrollX = scrollAmount;
		break;
	case 'r':
		scrollX = -scrollAmount;
		break;
	case 'u':
		scrollY = scrollAmount;
		break;
	case 'd':
		scrollY = -scrollAmount;
		break;
	default:
		Com_Printf("UI_GeoscapeNodeScroll_f: Invalid parameter\n");
		return;
	}

	if (!UI_MAPEXTRADATACONST(node).flatgeoscape) {
		/* case 3D geoscape */
		vec3_t diff;

		VectorCopy(UI_MAPEXTRADATACONST(node).angles, UI_MAPEXTRADATA(node).smoothFinalGlobeAngle);

		/* rotate a model */
		UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[PITCH] += ROTATE_SPEED * (scrollX) / UI_MAPEXTRADATACONST(node).zoom;
		UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[YAW] -= ROTATE_SPEED * (scrollY) / UI_MAPEXTRADATACONST(node).zoom;

		while (UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle[YAW] < -180.0) {
			UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[YAW] = -180.0;
		}
		while (UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle[YAW] > 0.0) {
			UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[YAW] = 0.0;
		}

		while (UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle[PITCH] > 180.0) {
			UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[PITCH] -= 360.0;
			UI_MAPEXTRADATA(node).angles[PITCH] -= 360.0;
		}
		while (UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle[PITCH] < -180.0) {
			UI_MAPEXTRADATA(node).smoothFinalGlobeAngle[PITCH] += 360.0;
			UI_MAPEXTRADATA(node).angles[PITCH] += 360.0;
		}
		VectorSubtract(UI_MAPEXTRADATACONST(node).smoothFinalGlobeAngle, UI_MAPEXTRADATACONST(node).angles, diff);
		UI_MAPEXTRADATA(node).smoothDeltaLength = VectorLength(diff);

		UI_MAPEXTRADATA(node).smoothFinalZoom = UI_MAPEXTRADATACONST(node).zoom;
		UI_MAPEXTRADATA(node).smoothDeltaZoom = 0.0f;
		UI_MAPEXTRADATA(node).smoothRotation = true;
	} else {
		int i;
		/* shift the map */
		UI_MAPEXTRADATA(node).center[0] -= (float) (scrollX) / (UI_MAPEXTRADATACONST(node).mapSize[0] * UI_MAPEXTRADATACONST(node).zoom);
		UI_MAPEXTRADATA(node).center[1] -= (float) (scrollY) / (UI_MAPEXTRADATACONST(node).mapSize[1] * UI_MAPEXTRADATACONST(node).zoom);
		for (i = 0; i < 2; i++) {
			while (UI_MAPEXTRADATACONST(node).center[i] < 0.0)
				UI_MAPEXTRADATA(node).center[i] += 1.0;
			while (UI_MAPEXTRADATACONST(node).center[i] > 1.0)
				UI_MAPEXTRADATA(node).center[i] -= 1.0;
		}
		if (UI_MAPEXTRADATACONST(node).center[1] < 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 0.5 / UI_MAPEXTRADATACONST(node).zoom;
		if (UI_MAPEXTRADATACONST(node).center[1] > 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom)
			UI_MAPEXTRADATA(node).center[1] = 1.0 - 0.5 / UI_MAPEXTRADATACONST(node).zoom;
	}
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

	Cmd_AddCommand("map_zoom", UI_GeoscapeNodeZoom_f);
	Cmd_AddCommand("map_scroll", UI_GeoscapeNodeScroll_f);

	cl_3dmap = Cvar_Get("cl_3dmap", "1", CVAR_ARCHIVE, "3D geoscape or flat geoscape");
	cl_3dmapAmbient = Cvar_Get("cl_3dmapAmbient", "0", CVAR_ARCHIVE, "3D geoscape ambient lighting factor");
	cl_mapzoommax = Cvar_Get("cl_mapzoommax", "6.0", CVAR_ARCHIVE, "Maximum geoscape zooming value");
	cl_mapzoommin = Cvar_Get("cl_mapzoommin", "1.0", CVAR_ARCHIVE, "Minimum geoscape zooming value");
	cl_geoscape_overlay = Cvar_Get("cl_geoscape_overlay", "0", 0, "Geoscape overlays - Bitmask");
}
