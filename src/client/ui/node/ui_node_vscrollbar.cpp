/**
 * @file
 * @todo implement disabled
 * @todo robustness
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../ui_behaviour.h"
#include "../ui_parse.h"
#include "../ui_timer.h"
#include "../ui_actions.h"
#include "../ui_input.h"
#include "../ui_render.h"
#include "ui_node_abstractnode.h"
#include "ui_node_abstractscrollbar.h"
#include "ui_node_vscrollbar.h"

#include "../../input/cl_input.h"
#include "../../input/cl_keys.h"

static const int TILE_WIDTH = 32;
static const int TILE_HEIGHT = 18;
static const int ELEMENT_WIDTH = 19;
static const int ELEMENT_HEIGHT = 16;

static int oldPos;
static int oldMouseY;
static uiTimer_t* capturedTimer;
static int capturedElement;

#define EXTRADATA(node) UI_EXTRADATA(node, abstractScrollbarExtraData_t)

/**
 * @brief Return size of all elements of the scrollbar
 */
static void UI_VScrollbarNodeGetElementSize (uiNode_t* node, int description[5])
{
	const int cuttableSize = node->box.size[1] - (ELEMENT_HEIGHT * 4);
	const int low = cuttableSize * ((float)(EXTRADATA(node).pos + 0) / (float)EXTRADATA(node).fullsize);
	const int middle = cuttableSize * ((float)(EXTRADATA(node).viewsize) / (float)EXTRADATA(node).fullsize);
	const int hight = cuttableSize - low - middle;
	description[0] = ELEMENT_HEIGHT;
	description[1] = low;
	description[2] = middle + 2 * ELEMENT_HEIGHT;
	description[3] = hight;
	description[4] = ELEMENT_HEIGHT;
	assert(description[0] + description[1] + description[2] + description[3] + description[4] == node->box.size[1]);
}

/**
 * @brief Get an element of the scrollbar at a position
 * @param[in] node Absolute position y
 * @param[in] description Size of each scroll bar elements (beginarrow, mid, scroll, mid, endarrow)
 * @param[in] x,y Absolute position
 */
static int UI_VScrollbarNodeGetElement (uiNode_t* node, int description[5], int x, int y)
{
	UI_NodeAbsoluteToRelativePos(node, &x, &y);
	for (int i = 0; i < 5; i++) {
		if (y < description[i])
			return i;
		y -= description[i];
	}
	return -1;
}

/**
 * @brief Set the position of the scrollbar to a value
 */
static void UI_VScrollbarNodeSet (uiNode_t* node, int value)
{
	int pos = value;

	if (pos < 0) {
		pos = 0;
	} else if (pos > EXTRADATA(node).fullsize - EXTRADATA(node).viewsize) {
		pos = EXTRADATA(node).fullsize - EXTRADATA(node).viewsize;
	}
	if (pos < 0)
		pos = 0;

	/* nothing change */
	if (EXTRADATA(node).pos == pos)
		return;

	/* update status */
	EXTRADATA(node).pos = pos;

	/* fire change event */
	if (node->onChange) {
		UI_ExecuteEventActions(node, node->onChange);
	}
}

/**
 * @brief Translate the position to a value
 */
static inline void UI_VScrollbarNodeDiff (uiNode_t* node, int value)
{
	UI_VScrollbarNodeSet(node, EXTRADATA(node).pos + value);
}

static inline void UI_VScrollbarNodeAction(uiNode_t* node, int hoveredElement, bool allowCapture);

static void UI_VScrollbarNodeRepeat (uiNode_t* node, uiTimer_t* timer)
{
	UI_VScrollbarNodeAction(node, capturedElement, false);
	if (timer->calledTime == 1) {
		timer->delay = 50;
	}
}

/**
 * @param[in] node Our scrollbar
 * @param[in] hoveredElement beginarrow, mid, scroll, mid, endarrow
 * @param[in] allowCapture idk
 */
static inline void UI_VScrollbarNodeAction (uiNode_t* node, int hoveredElement, bool allowCapture)
{
	switch (hoveredElement) {
	case 0:
		UI_VScrollbarNodeDiff(node, -1);
		if (allowCapture) {
			UI_SetMouseCapture(node);
			capturedElement = hoveredElement;
			capturedTimer = UI_AllocTimer(node, 500, UI_VScrollbarNodeRepeat);
			UI_TimerStart(capturedTimer);
		}
		break;
	case 1:
		UI_VScrollbarNodeDiff(node, -10);
		if (allowCapture) {
			UI_SetMouseCapture(node);
			capturedElement = hoveredElement;
			capturedTimer = UI_AllocTimer(node, 500, UI_VScrollbarNodeRepeat);
			UI_TimerStart(capturedTimer);
		}
		break;
	case 2:
		if (allowCapture) {
			UI_SetMouseCapture(node);
			/* save start value */
			oldMouseY = mousePosY;
			oldPos = EXTRADATA(node).pos;
			capturedElement = hoveredElement;
		}
		break;
	case 3:
		UI_VScrollbarNodeDiff(node, 10);
		if (allowCapture) {
			UI_SetMouseCapture(node);
			capturedElement = hoveredElement;
			capturedTimer = UI_AllocTimer(node, 500, UI_VScrollbarNodeRepeat);
			UI_TimerStart(capturedTimer);
		}
		break;
	case 4:
		UI_VScrollbarNodeDiff(node, 1);
		if (allowCapture) {
			UI_SetMouseCapture(node);
			capturedElement = hoveredElement;
			capturedTimer = UI_AllocTimer(node, 500, UI_VScrollbarNodeRepeat);
			UI_TimerStart(capturedTimer);
		}
		break;
	default:
		assert(false);
		break;
	}
}

void uiVScrollbarNode::onMouseDown (uiNode_t* node, int x, int y, int button)
{
	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize < EXTRADATA(node).viewsize)
		return;
	if (button != K_MOUSE1)
		return;

	int description[5];
	UI_VScrollbarNodeGetElementSize(node, description);
	const int hoveredElement = UI_VScrollbarNodeGetElement(node, description, x, y);
	UI_VScrollbarNodeAction(node, hoveredElement, true);
}

void uiVScrollbarNode::onMouseUp (uiNode_t* node, int x, int y, int button)
{
	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize < EXTRADATA(node).viewsize)
		return;
	if (button != K_MOUSE1)
		return;

	if (UI_GetMouseCapture() == node) {
		UI_MouseRelease();
	}
}

/**
 * @brief Called when the node have lost the captured node
 * We clean cached data
 */
void uiVScrollbarNode::onCapturedMouseLost (uiNode_t* node)
{
	if (capturedTimer) {
		UI_TimerRelease(capturedTimer);
		capturedTimer = nullptr;
	}
}

/**
 * @brief Called when the user wheel the mouse over the node
 */
bool uiVScrollbarNode::onScroll (uiNode_t* node, int deltaX, int deltaY)
{
	if (deltaY == 0)
		return false;
	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize < EXTRADATA(node).viewsize)
		return false;
	UI_VScrollbarNodeSet(node, EXTRADATA(node).pos + deltaY);
	return true;
}

/**
 * @brief Called when the node is captured by the mouse
 */
void uiVScrollbarNode::onCapturedMouseMove (uiNode_t* node, int x, int y)
{
	if (capturedElement != 2)
		return;

	const int posSize = EXTRADATA(node).fullsize;
	const int graphicSize = node->box.size[1] - (4 * ELEMENT_HEIGHT);

	/* compute mouse mouse */
	y -= oldMouseY;

	/* compute pos projection */
	const int pos = oldPos + (((float)y * (float)posSize) / (float)graphicSize);

	UI_VScrollbarNodeSet(node, pos);
}

/**
 * @brief Call to draw the node
 */
void uiVScrollbarNode::draw (uiNode_t* node)
{
	vec2_t pos;
	int texX = 0;
	int texY = 0;

	UI_GetNodeAbsPos(node, pos);
	int y = pos[1];

	const char* texture = UI_GetReferenceString(node, node->image);
	if (!texture)
		return;

	const image_t* image = UI_LoadImage(texture);

	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize <= EXTRADATA(node).viewsize) {
		/* hide the scrollbar */
		if (EXTRADATA(node).hideWhenUnused)
			return;

		texX = TILE_WIDTH * 3;

		/* top */
		UI_DrawNormImage(false, pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			image);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* top to bottom */
		UI_DrawNormImage(false, pos[0], y, ELEMENT_WIDTH, node->box.size[1] - (ELEMENT_HEIGHT * 2),
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			image);
		texY += TILE_HEIGHT * 5;
		y += node->box.size[1] - (ELEMENT_HEIGHT * 2);
		assert(y == pos[1] + node->box.size[1] - ELEMENT_HEIGHT);

		/* bottom */
		UI_DrawNormImage(false, pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			image);

	} else {
		int hoveredElement = -1;
		int description[5];
		UI_VScrollbarNodeGetElementSize(node, description);
		if (UI_GetMouseCapture() == node)
			hoveredElement = capturedElement;
		else if (node->state)
			hoveredElement = UI_VScrollbarNodeGetElement(node, description,
					mousePosX, mousePosY);

		/* top */
		texX = (hoveredElement == 0) ? TILE_WIDTH : 0;
		UI_DrawNormImage(false, pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
				texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY, image);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* top to slider */
		if (description[1]) {
			texX = (hoveredElement == 1) ? TILE_WIDTH : 0;
			UI_DrawNormImage(false, pos[0], y, ELEMENT_WIDTH, description[1],
					texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
					image);
			y += description[1];
		}
		texY += TILE_HEIGHT;

		/* slider */
		texX = (hoveredElement == 2) ? TILE_WIDTH : 0;

		/* top slider */
		UI_DrawNormImage(false, pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
				texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY, image);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* middle slider */
		if (description[2]) {
			UI_DrawNormImage(false, pos[0], y, ELEMENT_WIDTH,
					description[2] - ELEMENT_HEIGHT - ELEMENT_HEIGHT,
					texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
					image);
			y += description[2] - ELEMENT_HEIGHT - ELEMENT_HEIGHT;
		}
		texY += TILE_HEIGHT;

		/* bottom slider */
		UI_DrawNormImage(false, pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
				texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY, image);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* slider to bottom */
		if (description[3]) {
			texX = (hoveredElement == 3) ? TILE_WIDTH : 0;
			UI_DrawNormImage(false, pos[0], y, ELEMENT_WIDTH, description[3],
					texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
					image);
			y += description[3];
		}
		texY += TILE_HEIGHT;
		assert(y == pos[1] + node->box.size[1] - ELEMENT_HEIGHT);

		/* bottom */
		texX = (hoveredElement == 4) ? TILE_WIDTH : 0;
		UI_DrawNormImage(false, pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
				texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY, image);
	}
}

void uiVScrollbarNode::onLoading (uiNode_t* node)
{
	node->box.size[0] = 19;
}

void uiVScrollbarNode::onLoaded (uiNode_t* node)
{
#ifdef DEBUG
	if (node->box.size[1] - (ELEMENT_HEIGHT * 4) < 0)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' too small. It can create graphical glitches\n", UI_GetPath(node));
#endif
}

void UI_RegisterVScrollbarNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "vscrollbar";
	behaviour->extends = "abstractscrollbar";
	behaviour->manager = UINodePtr(new uiVScrollbarNode());

	/* Image to use. Each behaviour use it like they want.
	 * @todo use V_REF_OF_STRING when its possible ('image' is never a cvar)
	 */
	UI_RegisterNodeProperty(behaviour, "image", V_CVAR_OR_STRING, uiNode_t, image);
}
